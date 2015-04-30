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

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <htslib/sam.h>

/*
 * This file contains the “runtime” library for BAMQL.
 *
 * Every function here will be available in the generated code after calling
 * `define_module`.  This allows all these functions to be part of the output
 * binary, with static linkage.
 *
 * This makes it trivial to root around in HTSlib's structures without having
 * to define them in LLVM.
 *
 * Functions here can have any signatures, but they should almost always return
 * bool. It is also important that they have no state and no side-effects.
 */

bool globish_match(const char *pattern, const char *input)
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

bool check_flag(bam1_t *read, uint16_t flag)
{
	return (flag & read->core.flag) == flag;
}

bool check_chromosome_id(uint32_t chr_id, bam_hdr_t *header,
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

bool check_chromosome(bam1_t *read, bam_hdr_t *header, const char *pattern,
		      bool mate)
{
	return check_chromosome_id(mate ? read->core.mtid : read->core.tid,
				   header, pattern);
}

bool check_mapping_quality(bam1_t *read, uint8_t quality)
{
	return read->core.qual != 255 && read->core.qual >= quality;
}

bool check_nt(bam1_t *read, int32_t position, unsigned char nt, bool exact)
{
	unsigned char read_nt;
	if (read->core.flag & BAM_FUNMAP) {
		return false;
	}
	if (read->core.pos > position ||
	    read->core.pos + read->core.l_qseq < position) {
		return false;
	}
	read_nt = bam_seqi(bam_get_seq(read), position - read->core.pos);
	return exact ? (read_nt == nt) : (read_nt != 0);
}

bool check_position(bam1_t *read, int32_t start, int32_t end)
{
	if (read->core.flag & BAM_FUNMAP) {
		return false;
	}
	return read->core.pos <= end && bam_endpos(read) >= start;
}

bool check_aux_str(bam1_t *read, const char *pattern, char group1, char group2)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	const char *str;

	if (value == NULL || (str = bam_aux2Z(value)) == NULL) {
		return false;
	}
	return globish_match(pattern, str);
}

bool check_aux_char(bam1_t *read, char pattern, char group1, char group2)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	return value != NULL && bam_aux2A(value) == pattern;
}

bool check_aux_int(bam1_t *read, int32_t pattern, char group1, char group2)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	return value != NULL && bam_aux2i(value) == pattern;
}

bool check_aux_double(bam1_t *read, double pattern, char group1, char group2)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);
	return value != NULL && bam_aux2f(value) == (float)pattern;
}

bool check_split_pair(bam_hdr_t *header, bam1_t *read)
{
	if (read->core.tid < header->n_targets
	    && read->core.mtid < header->n_targets) {
		return read->core.tid != read->core.mtid;
	}
	return false;
}

bool randomly(double probability)
{
	return probability < drand48();
}
