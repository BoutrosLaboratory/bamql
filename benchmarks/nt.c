#include<htslib/sam.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

uint32_t compute_mapped_end(bam1_t *read)
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

int main(int argc, char **argv) {
        htsFile *input = NULL;
        htsFile *output = NULL;
        bam_hdr_t *header = NULL;
        bam1_t *read = NULL;

        if ((input = hts_open(argv[1], "rb")) == NULL) {
                return 1;
        }
        if ((output = hts_open(argv[2], "wb")) == NULL) {
                hts_close(input);
                return 1;
        }

        header = sam_hdr_read(input);
        sam_hdr_write(output, header);
      
	read = bam_init1();

        while (sam_read1(input, header, read) >= 0) {
 
		int32_t position = 13353;
		unsigned char nt = 2;
		char read_nt;
		int32_t mapped_position;
		bool match;
        	if (read->core.flag & BAM_FUNMAP) {
        		continue;    
        	}
       		if (read->core.pos+1 > position || compute_mapped_end(read) < position) {
        	        continue; 
        	}
              	if ( read->core.n_cigar == 0) {
			mapped_position = bam_is_rev(read) ? (compute_mapped_end(read) - position) : (position - read->core.pos-1);
		} else {
			int32_t required_offset = bam_is_rev(read) ? (compute_mapped_end(read)- position) : (position - read->core.pos-1);
			mapped_position = 0;
			for(int index = 0;index < read->core.n_cigar; index++) {
				uint8_t consume = bam_cigar_type(bam_cigar_op(bam_get_cigar(read)[index]));
				uint32_t op_length = bam_cigar_oplen(bam_get_cigar(read)[index]);
				for(int incr = 0; incr < op_length && required_offset >= 0; incr++){
					if (consume & 1)
						mapped_position++;

					if (consume & 2)
						required_offset--;
				
					match = (consume & 2) && (consume & 1);
				}
			}
		}
		read_nt = bam_seqi(bam_get_seq(read), mapped_position-1); 
		if (read_nt == nt && match){
			sam_write1(output, header, read);	
		}
        }
        
        hts_close(input);
        hts_close(output);
        bam_destroy1(read);
        bam_hdr_destroy(header);
        return 0;
}

