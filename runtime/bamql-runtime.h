#pragma once
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
#ifdef __cplusplus
extern "C" {
#endif
#include <ctype.h>
#include <stdbool.h>
#include <htslib/sam.h>

#define BAMQL_RUNTIME_API_VERSION 2

	typedef void (*bamql_error_handler) (const char *str, void *context);

/*
 * This file contains the runtime library for BAMQL.
 */
	bool bamql_aux_fp(bam1_t *read, char group1, char group2, double *out);
	bool bamql_aux_int(bam1_t *read, char group1, char group2,
			   int32_t * out);
	const char *bamql_aux_str(bam1_t *read, char group1, char group2);
	bool bamql_check_chromosome(bam_hdr_t *header, bam1_t *read,
				    const char *pattern, bool mate);
	bool bamql_check_chromosome_id(bam_hdr_t *header, uint32_t chr_id,
				       const char *pattern);
	bool bamql_check_flag(bam1_t *read, uint32_t flag);
	bool bamql_check_mapping_quality(bam1_t *read, uint8_t quality);
	bool bamql_check_nt(bam1_t *read, int32_t position, unsigned char nt,
			    bool exact);
	bool bamql_check_position(bam_hdr_t *header, bam1_t *read,
				  uint32_t start, uint32_t end);
	bool bamql_check_split_pair(bam_hdr_t *header, bam1_t *read);
	const char *bamql_chr(bam_hdr_t *header, bam1_t *read, bool mate);
	const char *bamql_header(bam1_t *read);
	bool bamql_position_begin(bam_hdr_t *header, bam1_t *read,
				  uint32_t * out);
	bool bamql_position_end(bam_hdr_t *header, bam1_t *read,
				uint32_t * out);
	bool bamql_randomly(double probability);
	bool bamql_re_match(const char *pattern, const char *input);
	int bamql_strcmp(const char *left, const char *right);
#ifdef __cplusplus
}
#endif
