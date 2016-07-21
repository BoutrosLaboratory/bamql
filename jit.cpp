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
#include <iostream>
#include <sstream>
#include <llvm/Support/DynamicLibrary.h>
#include "bamql-rt.h"
#include "bamql-jit.hpp"

std::map<std::string, void *> known = {
  { "bamql_check_aux_char", (void *)bamql_check_aux_char },
  { "bamql_check_aux_double", (void *)bamql_check_aux_double },
  { "bamql_check_aux_int", (void *)bamql_check_aux_int },
  { "bamql_check_aux_str", (void *)bamql_check_aux_str },
  { "bamql_check_chromosome", (void *)bamql_check_chromosome },
  { "bamql_check_chromosome_id", (void *)bamql_check_chromosome_id },
  { "bamql_check_flag", (void *)bamql_check_flag },
  { "bamql_check_mapping_quality", (void *)bamql_check_mapping_quality },
  { "bamql_check_nt", (void *)bamql_check_nt },
  { "bamql_check_position", (void *)bamql_check_position },
  { "bamql_check_split_pair", (void *)bamql_check_split_pair },
  { "bamql_header_regex", (void *)bamql_header_regex },
  { "bamql_randomly", (void *)bamql_randomly }
};

std::shared_ptr<llvm::ExecutionEngine> bamql::createEngine(
    std::unique_ptr<llvm::Module> module) {

  std::map<llvm::Function *, void *> required;
  for (auto func_it = known.begin(); func_it != known.end(); func_it++) {
    auto func = module->getFunction(func_it->first);
    if (func != nullptr) {
      required[func] = func_it->second;
    }
  }
  std::string error;
  std::vector<std::string> attrs;
  attrs.push_back("-avx"); // The AVX support (auto-vectoring) should be
                           // disabled since the JIT isn't smart enough to
                           // detect this, even though there is a detection
                           // routine. In particular, it makes Valgrind not
                           // work.
  std::shared_ptr<llvm::ExecutionEngine> engine(
      llvm::EngineBuilder(
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
          module.release()
#else
          std::move(module)
#endif

          )
          .setEngineKind(llvm::EngineKind::JIT)
          .setErrorStr(&error)
          .setMAttrs(attrs)
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
          .setUseMCJIT(true)
#endif

          .create());
  if (!engine) {
    std::cerr << error << std::endl;
  } else {
    for (auto func_it = required.begin(); func_it != required.end();
         func_it++) {
      engine->addGlobalMapping(func_it->first, func_it->second);
    }
  }
  return engine;
}

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
