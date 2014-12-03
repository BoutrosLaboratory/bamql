#include <stdbool.h>
#include <htslib/sam.h>

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
