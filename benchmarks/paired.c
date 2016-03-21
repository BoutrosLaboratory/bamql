#include<htslib/sam.h>

int
main (int argc, char **argv)
{
  htsFile *input = NULL;
  htsFile *output = NULL;
  bam_hdr_t *header = NULL;
  bam1_t *read = NULL;

  if ((input = hts_open (argv[1], "rb")) == NULL)
    {
      return 1;
    }
  if ((output = hts_open (argv[2], "wb")) == NULL)
    {
      hts_close (input);
      return 1;
    }

  header = sam_hdr_read (input);
  sam_hdr_write (output, header);

  read = bam_init1 ();

  while (sam_read1 (input, header, read) >= 0)
    {
      if ((read->core.flag & BAM_FPAIRED) == BAM_FPAIRED)
	{
	  sam_write1 (output, header, read);
	}
    }
  hts_close (input);
  hts_close (output);
  bam_destroy1 (read);
  bam_hdr_destroy (header);
  return 0;
}
