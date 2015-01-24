#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <htslib/sam.h>

/*
 * This file contains the “runtime” library for BARF.
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
	for (; *pattern != '\0' && *input != '\0'; pattern++, input++) {
		switch (*pattern) {
		case '?':
			/*
			 * Accept any character except the end of the string.
			 */
			if (*input == '\0')
				return false;
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
			if (*pattern == '\0') {
				return true;
			}

			/*
			 * If there is input, it could be consumed by the star, or match after
			 * the star, so consume the input, character by character, each time
			 * recursively checking if the input matches the rest of the string.
			 */
			while (*(input++) != '\0') {
				if (globish_match(pattern, input)) {
					return true;
				}
			}
			return false;
		default:
			if (tolower(*pattern) != tolower(*input)) {
				return false;
			}
			break;
		}
	}
	return *pattern == *input;
}

bool check_flag(bam1_t *read, uint16_t flag)
{
	return flag & read->core.flag;
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

bool check_aux_str(bam1_t *read, const char *pattern, char group1, char group2)
{
	char const id[] = { group1, group2 };
	uint8_t const *value = bam_aux_get(read, id);

	if (value == NULL || value[0] != 'Z') {
		return false;
	}
	return globish_match(pattern, (const char *)value + 1);
}

bool randomly(double probability)
{
	return probability < drand48();
}
