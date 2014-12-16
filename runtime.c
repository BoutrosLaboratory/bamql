#include <fnmatch.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <htslib/sam.h>

void __dummy__(bam_hdr_t * header, bam1_t *read)
{
}

/*
 * This file contains the “runtime” library for BARF.
 *
 * Every function here will be available in predicates.cpp as
 * define_<function>, which will import this function, with static linkage into
 * the module being build.
 *
 * This makes it trivial to root around in HTSlib's structures without having
 * to define them in LLVM.
 *
 * Functions here can have any signatures, but they should almost always return
 * bool. It is also important that they have no state and no side-effects.
 */
bool is_paired(bam1_t *read)
{
	return BAM_FPAIRED & read->core.flag;
}

bool check_chromosome(bam1_t *read, bam_hdr_t * header, const char *name)
{
	if (read->core.tid < 0 || read->core.tid >= header->n_targets)
		return false;

	const char *real_name = header->target_name[read->core.tid];

	if (strncasecmp("chr", real_name, 3) == 0) {
		real_name += 3;
	}
	return fnmatch(name, real_name, 0) == 0;
}

bool check_read_group(bam1_t *read, const char *name)
{
	uint8_t const *read_group = bam_aux_get(read, "RG");
	if (read_group == NULL) {
		return false;
	}
	return fnmatch(name, (const char *)read_group, 0) == 0;
}

bool randomly(double probability) {
	return probability < drand48();
}
