#include <fnmatch.h>
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
bool check_flag(bam1_t *read, uint8_t flag)
{
	return flag & read->core.flag;
}

bool check_chromosome(bam1_t *read, bam_hdr_t *header, const char *pattern, bool mate)
{
	int32_t chr_id = mate ? read->core.mtid : read->core.tid;
	if (chr_id < 0 || chr_id >= header->n_targets)
		return false;

	const char *real_name = header->target_name[chr_id];

	if (strncasecmp("chr", real_name, 3) == 0) {
		real_name += 3;
	}
	return fnmatch(pattern, real_name, FNM_PATHNAME | FNM_NOESCAPE) == 0;
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
	return fnmatch(pattern, (const char *)value + 1, FNM_PATHNAME | FNM_NOESCAPE) == 0;
}

bool randomly(double probability)
{
	return probability < drand48();
}
