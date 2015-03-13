#include <cstdio>
#include <iostream>
#include <sstream>
#include "barf-jit.hpp"

std::shared_ptr<llvm::ExecutionEngine> barf::createEngine(
    llvm::Module *module) {
  std::string error;
  std::vector<std::string> attrs;
  attrs.push_back("-avx"); // The AVX support (auto-vectoring) should be
                           // disabled since the JIT isn't smart enough to
                           // detect this, even though there is a detection
                           // routine. In particular, it makes Valgrind not
                           // work.
  std::shared_ptr<llvm::ExecutionEngine> engine(
      llvm::EngineBuilder(module)
          .setEngineKind(llvm::EngineKind::JIT)
          .setErrorStr(&error)
          .setMAttrs(attrs)
          .create());
  if (!engine) {
    std::cout << error << std::endl;
  }
  return engine;
}

std::shared_ptr<bam_hdr_t> barf::appendProgramToHeader(
    const bam_hdr_t *original,
    const std::string &id,
    const std::string &version,
    const std::string &args) {
  auto copy = std::shared_ptr<bam_hdr_t>(bam_hdr_init(), bam_hdr_destroy);
  if (!copy) {
    return copy;
  }
  std::stringstream text;

  text << original->text << "@PG\tID:" << id << "\tVN:" << version << "\tCL:\""
       << args << "\"\n";
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
