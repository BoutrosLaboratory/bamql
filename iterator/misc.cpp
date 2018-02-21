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

#include <cstdio>
#include <cstring>
#include <sstream>
#include "bamql-iterator.hpp"

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
  copy->cigar_tab = nullptr;
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

template <typename T> class Pool {
public:
  Pool(std::function<T *()> allocate_, std::function<void(T *)> deallocate_)
      : allocate(allocate_), deallocate(deallocate_) {}
  ~Pool() {
    while (!pool.empty()) {
      deallocate(pool.back());
      pool.pop_back();
    }
  }
  std::shared_ptr<T> take() {
    std::unique_lock<std::mutex> lock(m);
    T *item;
    if (pool.empty()) {
      item = allocate();
      items.push_back(item);
    } else {
      item = pool.back();
      pool.pop_back();
    }
    return std::shared_ptr<T>(item, [this](T *item) {
      std::unique_lock<std::mutex> lock(m);
      pool.push_back(item);
    }

                              );
  }

private:
  std::function<T *()> allocate;
  std::function<void(T *)> deallocate;
  std::vector<T *> pool;
  std::vector<T *> items;
  std::mutex m;
};

static Pool<bam1_t> bam_read_pool(bam_init1, bam_destroy1);
std::shared_ptr<bam1_t> bamql::allocateRead() { return bam_read_pool.take(); }
