#include <stdbool.h>
#include <stdlib.h>
#include <htslib/sam.h>

int main(int argc, char **argv) {
	htsFile *input = NULL;
	htsFile *output1 = hts_open("chrom1-c.bam", "wb");
	htsFile *output2 = hts_open("chrom2-c.bam", "wb");
	htsFile *output3 = hts_open("chrom3-c.bam", "wb");
	bam_hdr_t *header = NULL;
	bam1_t *read = NULL;
	int i;

	if ((input = hts_open(argv[1], "rb")) == NULL) {
		return 1;
	}

	header = sam_hdr_read(input);
	sam_hdr_write(output1, header);
	sam_hdr_write(output2, header);
	sam_hdr_write(output3, header);

	read = bam_init1();

	while (sam_read1(input, header, read) >= 0) {
		if (read->core.tid < header->n_targets){
			read->core.tid==0 ? sam_write1(output1, header, read)
                      : read->core.tid==1 ? sam_write1(output2, header, read)
		      : read->core.tid==2 ? sam_write1(output3, header, read)
		      : 0;
		}
	}
	hts_close(input);
	hts_close(output1);
	hts_close(output2);
	hts_close(output3);
	bam_destroy1(read);
	bam_hdr_destroy(header);
	return 0;
}
