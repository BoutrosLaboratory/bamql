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

#include "bamql-iterator.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <uuid.h>

std::shared_ptr<bam_hdr_t> bamql::appendProgramToHeader(
    const bam_hdr_t *original,
    const std::string &name,
    const std::string &id,
    const std::string &version,
    const std::string &args) {
  auto copy = std::shared_ptr<bam_hdr_t>(bam_hdr_init(), bam_hdr_destroy);
  if (!copy) {
    return copy;
  }
  std::stringstream text;

  text << original->text << "@PG\tPN:" << name << "\tID:" << id
       << "\tVN:" << version << "\tCL:\"" << args << "\"\n";
  auto text_str = text.str();
  copy->n_targets = original->n_targets;
  copy->ignore_sam_err = original->ignore_sam_err;
  copy->l_text = text_str.length();
  copy->sdict = nullptr;
  copy->text = strdup(text_str.c_str());
  copy->target_len = (uint32_t *)calloc(original->n_targets, sizeof(uint32_t));
  copy->target_name = (char **)calloc(original->n_targets, sizeof(char *));
  for (int it = 0; it < original->n_targets; it++) {
    copy->target_len[it] = original->target_len[it];
    copy->target_name[it] = strdup(original->target_name[it]);
  }
  return copy;
}

static void hts_close0(htsFile *handle) {
  if (handle != nullptr)
    hts_close(handle);
}

std::shared_ptr<htsFile> bamql::open(const char *filename, const char *mode) {
  return std::shared_ptr<htsFile>(hts_open(filename, mode), hts_close0);
}

std::string bamql::makeUuid() {
#if defined(_UUID_UUID_H) || defined(_UL_LIBUUID_UUID_H)
  uuid_t uuid;
  char id_buf[sizeof(uuid_t) * 2 + 1];
  uuid_generate(uuid);
  uuid_unparse(uuid, id_buf);
  std::string id_str(id_buf);
  return id_str;
#elif defined(__UUID_H__)
  uuid_t *uuid = nullptr;
  char id_buf[UUID_LEN_STR + 1];
  size_t id_buf_len;
  if (uuid_create(&uuid) != UUID_RC_OK ||
      uuid_export(uuid, UUID_FMT_STR, id_buf, &id_buf_len) != UUID_RC_OK) {
    if (uuid != nullptr) {
      uuid_destroy(uuid);
    }
    return "00000000-0000-0000-0000-000000000000";
  }
  uuid_destroy(uuid);
  return std::string(id_buf, id_buf_len);
#else
#error "Cannot determine UUID library calls."
#endif
}
