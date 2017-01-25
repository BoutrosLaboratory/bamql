/*
 * Copyright 2015 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
 *
 * OICR makes no representations whatsoever as to the SOFTWARE contained
 * herein.  It is experimental in nature and is provided WITHOUT WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR ANY OTHER WARRANTY,
 * EXPRESS OR IMPLIED. OICR MAKES NO REPRESENTATION OR WARRANTY THAT THE USE OF
 * THIS SOFTWARE WILL NOT INFRINGE ANY PATENT OR OTHER PROPRIETARY RIGHT.  By
 * downloading this SOFTWARE, your Institution hereby indemnifies OICR against
 * any loss, claim, damage or liability, of whatsoever kind or nature, which
 * may arise from your Institution's respective use, handling or storage of the
 * SOFTWARE. If publications result from research using this SOFTWARE, we ask
 * that the Ontario Institute for Cancer Research be acknowledged and/or
 * credit be given to OICR scientists, as scientifically appropriate.
 */
#define _XOPEN_SOURCE

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pcre.h>
#include "bamql-runtime.h"

/*
 * This file contains the runtime library for BAMQL.
 *
 * Every function here will be available in the generated code after calling
 * `define_module`.
 *
 * This makes it trivial to root around in HTSlib's structures without having
 * to define them in LLVM.
 *
 * Functions here can have any signatures, but they must return bool. It is
 * also important that they have no state and no side-effects.
 *
 * A matching definition must be placed in `define_module` and `known` in
 * `misc.cpp` and `jit.cpp`, respectively.
 */

static uint32_t compute_mapped_end(bam1_t *read)
{
	/* HTSlib provides a function to calculate the end position based on the
	 * CIGAR string. If none is available, it just gives the start position +
	 * 1. This is derpy since one would expect the end position to be at least
	 * the start position plus the read length.
	 * As an added bonus, if the read is “officially” unmapped, even if it has
	 * CIGAR data, bam_endpos will ignore the CIGAR string. */
	if ((read->core.flag & BAM_FUNMAP) || read->core.n_cigar == 0) {
		return read->core.pos + read->core.l_qseq;
	} else {
		return bam_endpos(read);
	}
}

bool bamql_aux_fp(bam1_t *read, char group1, char group2, double *out)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	if (value == NULL) {
		return false;
	}
	if (strchr("fd", tolower(*value)) == NULL) {
		return false;
	}
	*out = bam_aux2f(value);
	return true;
}

bool bamql_aux_int(bam1_t *read, char group1, char group2, int32_t * out)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	if (value == NULL) {
		return false;
	}
	if (*value == 'A') {
		*out = bam_aux2A(value);
		return true;
	}
	if (strchr("csi", tolower(*value)) == NULL) {
		return false;
	}

	*out = bam_aux2i(value);
	return true;
}

const char *bamql_aux_str(bam1_t *read, char group1, char group2)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	const char *str;

	if (value == NULL) {
		return NULL;
	}
	return bam_aux2Z(value);
}

bool bamql_check_chromosome(bam_hdr_t *header, bam1_t *read,
			    const char *pattern, bool mate)
{
	uint32_t tid = mate ? read->core.mtid : read->core.tid;
	return bamql_check_chromosome_id(header, tid, pattern);
}

bool bamql_check_chromosome_id(bam_hdr_t *header, uint32_t chr_id,
			       const char *pattern)
{
	if (chr_id >= header->n_targets) {
		return false;
	}
	return bamql_re_match(pattern, header->target_name[chr_id]
	    );
}

bool bamql_check_flag(bam1_t *read, uint32_t flag)
{
	return (flag & read->core.flag) == flag;
}

bool bamql_check_mapping_quality(bam1_t *read, uint8_t quality)
{
	return read->core.qual != 255 && read->core.qual >= quality;
}

bool bamql_check_nt(bam1_t *read, int32_t position, unsigned char nt,
		    bool exact)
{
	unsigned char read_nt;
	int32_t mapped_position;
	if (read->core.flag & BAM_FUNMAP) {
		return false;
	}
	if (read->core.pos + 1 > position
	    || compute_mapped_end(read) < position) {
		return false;
	}

	if (read->core.n_cigar == 0) {
		mapped_position = position - read->core.pos - 1;
	} else {
		int32_t required_offset = position - read->core.pos - 1;
		mapped_position = 0;

		for (int cigar_index = 0; cigar_index < read->core.n_cigar;
		     cigar_index++) {
			uint8_t consume =
			    bam_cigar_type(bam_cigar_op
					   (bam_get_cigar(read)[cigar_index]));
			uint32_t op_length =
			    bam_cigar_oplen(bam_get_cigar(read)[cigar_index]);

			/* Iterate through the length of the cigar operation */
			for (int op_index = 0;
			     op_index < op_length && required_offset >= 0;
			     op_index++) {
				/* Consume query */
				if (consume & 1)
					mapped_position++;

				/* Consume reference */
				if (consume & 2)
					required_offset--;
			}
		}
	}
	read_nt = bam_seqi(bam_get_seq(read), mapped_position - 1);
	return exact ? (read_nt == nt) : ((read_nt & nt) != 0);
}

bool bamql_check_position(bam_hdr_t *header, bam1_t *read, uint32_t start,
			  uint32_t end)
{
	uint32_t mapped_start = read->core.pos + 1;
	uint32_t mapped_end;
	if (read->core.tid >= header->n_targets) {
		return false;
	}
	mapped_end = compute_mapped_end(read);
	return (mapped_start <= start && mapped_end >= start)
	    || (mapped_start <= end && mapped_end >= end)
	    || (mapped_start >= start && mapped_end <= end);
}

bool bamql_check_split_pair(bam_hdr_t *header, bam1_t *read)
{
	if (read->core.tid < header->n_targets
	    && read->core.mtid < header->n_targets) {
		return read->core.tid != read->core.mtid;
	}
	return false;
}

const char *bamql_chr(bam_hdr_t *header, bam1_t *read, bool mate)
{
	uint32_t chr_id = mate ? read->core.mtid : read->core.tid;
	const char *value;
	if (chr_id >= header->n_targets) {
		return NULL;
	}
	value = header->target_name[chr_id];
	if (strncmp("chr", value, 3) == 0) {
		value += 3;
	}
	return value;
}

const char *bamql_header(bam1_t *read)
{
	return bam_get_qname(read);
}

bool bamql_position_begin(bam_hdr_t *header, bam1_t *read, uint32_t * out)
{
	if (read->core.tid >= header->n_targets) {
		return false;
	}
	*out = read->core.pos + 1;
	return true;
}

bool bamql_position_end(bam_hdr_t *header, bam1_t *read, uint32_t * out)
{
	uint32_t mapped_start = read->core.pos + 1;
	if (read->core.tid >= header->n_targets) {
		*out = INT32_MAX;
		return false;
	}
	*out = compute_mapped_end(read);
	return true;
}

bool bamql_randomly(double probability)
{
	return probability >= drand48();
}

bool bamql_re_bind(const char *pattern, uint32_t count,
		   bamql_error_handler error_fn, void *error_ctx,
		   const char *input, ...)
{
	int vect[3 * (count + 1)];
	int strnum;
	va_list args;
	if (input == NULL) {
		return false;
	}

	if ((strnum = pcre_exec
	     ((const pcre *)pattern, NULL, input, strlen(input), 0, 0, vect,
	      3 * (count + 1))) < 0) {
		return false;
	}
	va_start(args, input);
	while (count-- > 0) {
		int number = va_arg(args, uint32_t);
		const char *error_text = va_arg(args, const char *);
		const char *result = NULL;
		const char **out_str;
		double *out_fp;
		int32_t *out_int;
		char *end;

		if (pcre_get_substring
		    (input, vect, strnum, number, &result) < 0
		    || result == NULL) {
			error_fn(error_text, error_ctx);
		}
		switch (va_arg(args, uint32_t)) {
		case 0:
			out_str = va_arg(args, const char **);
			*out_str = result;
			break;

		case 1:
			out_fp = va_arg(args, double *);
			if (result == NULL) {
				*out_fp = NAN;
			} else {
				*out_fp = strtod(result, &end);
				if (*end != '\0') {
					*out_fp = NAN;
					error_fn(error_text, error_ctx);
				}
				pcre_free_substring(result);
			}
			break;

		case 2:
			out_int = va_arg(args, int32_t *);
			if (result == NULL) {
				*out_int = 0;
			} else {
				*out_int = strtol(result, &end, 10);
				if (*end != '\0') {
					*out_int = 0;
					error_fn(error_text, error_ctx);
				}
				pcre_free_substring(result);
			}
			break;

		case 3:
			out_int = va_arg(args, int32_t *);
			*out_int = result == NULL ? 0 : *result;
			if (result != NULL) {
				pcre_free_substring(result);
			}
			break;
		default:
			abort();
		}
	}
	va_end(args);
	return true;
}

const char *bamql_re_compile(const char *pattern, uint32_t flags,
			     uint32_t count)
{
	const char *errptr;
	int erroffset;
	int name_count;
	pcre *result = pcre_compile(pattern, flags, &errptr, &erroffset, NULL);
	if (result == NULL) {
		fprintf(stderr, "Failed to compile regex: %s\n", pattern);
		abort();
	}
	if (errptr != NULL) {
		fprintf(stderr, "%s: %s\n", errptr, pattern);
		abort();
	}
	if (pcre_fullinfo(result, NULL, PCRE_INFO_NAMECOUNT, &name_count) <
	    0 || name_count != count) {
		fprintf(stderr,
			"There should be %d captures but there are %d: %s\n",
			count, name_count, pattern);
		abort();
	}
	return (const char *)result;
}

void bamql_re_free(char **pattern)
{
	if (*pattern != NULL) {
		pcre_free(*pattern);
		*pattern = NULL;
	}
}

bool bamql_re_match(const char *pattern, const char *input)
{
	if (input == NULL) {
		return false;
	}
	return pcre_exec((const pcre *)pattern, NULL, input, strlen(input), 0,
			 0, NULL, 0) >= 0;
}

int bamql_strcmp(const char *left, const char *right)
{
	if (left == right) {
		return 0;
	}
	if (left == NULL) {
		return -1;
	}
	if (right == NULL) {
		return 1;
	}
	return strcmp(left, right);
}
