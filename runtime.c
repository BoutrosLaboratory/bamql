#include <stdbool.h>
#include <htslib/sam.h>

bool is_paired(bam1_t *read) {
	return BAM_FPAIRED & read->core.flag;
}

