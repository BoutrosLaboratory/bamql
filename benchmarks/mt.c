#include <stdbool.h>
#include <stdlib.h>
#include <htslib/sam.h>

int
main (int argc, char **argv)
{
  htsFile *input = NULL;
  htsFile *output = NULL;
  bam_hdr_t *header = NULL;
  bam1_t *read = NULL;
  bool *chrM_test = NULL;
  int i;

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
  chrM_test = malloc (header->n_targets);
  for (i = 0; i < header->n_targets; i++)
    {
      const char *text = header->target_name[i];
      if (strncmp ("chr", text, 3) == 0)
	{
	  text += 3;
	}
      chrM_test[i] = strcmp (text, "m") == 0 || strcmp (text, "M") == 0
	|| strcmp (text, "mt") == 0 || strcmp (text, "MT") == 0
	|| strcmp (text, "25") == 0;
    }

  read = bam_init1 ();

  while (sam_read1 (input, header, read) >= 0)
    {
      if (read->core.tid < header->n_targets && chrM_test[read->core.tid] ||
	  read->core.mtid < header->n_targets && chrM_test[read->core.mtid] ||
	  (read->core.flag & (BAM_FUNMAP | BAM_FMUNMAP)) != 0)
	{
	  sam_write1 (output, header, read);
	}
    }
  hts_close (input);
  hts_close (output);
  bam_destroy1 (read);
  bam_hdr_destroy (header);
  free (chrM_test);
  return 0;
}
