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

#include <stdio.h>
#include <stdlib.h>
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
 * A matching definition must be placed in `define_module` and `known` in `misc.cpp` and `jit.cpp`, respectively.
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

static bool globish_match(const char *pattern, const char *input)
{
	for (; *pattern != '\0'; pattern++) {
		const char *next_input;
		switch (*pattern) {
		case '?':
			/*
			 * Accept any character except the end of the string.
			 */
			if (*input == '\0')
				return false;
			input++;
			break;
		case '*':
			/*
			 * Eat any stars that have bunched together.
			 */
			while (pattern[1] == '*') {
				pattern++;
			}
			/*
			 * If there is no more pattern, then whatever remaining input matches.
			 */
			if (pattern[1] == '\0') {
				return true;
			}

			/*
			 * If there is input, it could be consumed by the star, or match after
			 * the star, so consume the input, character by character, each time
			 * recursively checking if the input matches the rest of the string.
			 */
			for (next_input = input; *next_input != '\0';
			     next_input++) {
				if (globish_match(pattern, next_input + 1)) {
					return true;
				}
			}
			break;
		default:
			if (*input == '\0'
			    || tolower(*pattern) != tolower(*input)) {
				return false;
			}
			input++;
			break;
		}
	}
	return *pattern == *input;
}

static bool re_match(const char *pattern, const char *input,
		     size_t input_length)
{
	return pcre_exec((const pcre *)pattern, NULL, input, input_length, 0, 0,
			 NULL, 0) >= 0;
}

bool bamql_check_aux_str(bam1_t *read, char group1, char group2,
			 const char *pattern)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	const char *str;

	if (value == NULL || (str = bam_aux2Z(value)) == NULL) {
		return false;
	}
	return globish_match(pattern, str);
}

bool bamql_check_aux_char(bam1_t *read, char group1, char group2, char pattern)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	return value != NULL && bam_aux2A(value) == pattern;
}

bool bamql_check_aux_int(bam1_t *read, char group1, char group2,
			 int32_t pattern)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	return value != NULL && bam_aux2i(value) == pattern;
}

bool bamql_check_aux_double(bam1_t *read, char group1, char group2,
			    double pattern)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	return value != NULL && bam_aux2f(value) == (float)pattern;
}

bool bamql_check_chromosome(bam_hdr_t *header, bam1_t *read,
			    const char *pattern, bool mate)
{
	return bamql_check_chromosome_id(header,
					 mate ? read->core.mtid : read->
					 core.tid, pattern);
}

bool bamql_check_chromosome_id(bam_hdr_t *header, uint32_t chr_id,
			       const char *pattern)
{
	if (chr_id >= header->n_targets) {
		return false;
	}

	const char *real_name = header->target_name[chr_id];

	if (strncasecmp("chr", real_name, 3) == 0) {
		real_name += 3;
	}
	return globish_match(pattern, real_name);
}

bool bamql_check_flag(bam1_t *read, uint16_t flag)
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
	bool match;
	if (read->core.flag & BAM_FUNMAP) {
		return false;
	}
	if (read->core.pos + 1 > position
	    || compute_mapped_end(read) < position) {
		return false;
	}

	if (read->core.n_cigar == 0) {
		mapped_position =
		    bam_is_rev(read) ? (compute_mapped_end(read) -
					position) : (position - read->core.pos -
						     1);
	} else {
		int32_t required_offset =
		    bam_is_rev(read) ? (compute_mapped_end(read) -
					position) : (position - read->core.pos -
						     1);
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

				/* keep track of matched bases */
				match = (consume == 3);
			}
		}
	}
	read_nt = bam_seqi(bam_get_seq(read), mapped_position - 1);
	return exact ? (read_nt == nt && match) : (read_nt != 0);
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

bool bamql_header_regex(bam1_t *read, const char *pattern)
{
	return re_match(pattern, bam_get_qname(read), read->core.l_qname - 1);
}

bool bamql_randomly(double probability)
{
	return probability >= drand48();
}
