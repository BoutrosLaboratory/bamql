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
                           // routine.
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

barf::read_iterator::read_iterator() {}

bool barf::read_iterator::processFile(const char *file_name,
                                      bool binary,
                                      bool ignore_index) {
  // Open the input file.
  std::shared_ptr<htsFile> input = std::shared_ptr<htsFile>(
      hts_open(file_name, binary ? "rb" : "r"), hts_close);
  if (!input) {
    perror(file_name);
    return false;
  }

  // Copy the header to the output.
  std::shared_ptr<bam_hdr_t> header(sam_hdr_read(input.get()), bam_hdr_destroy);
  ingestHeader(header);

  // Open the index, if desired.
  std::shared_ptr<hts_idx_t> index(
      ignore_index ? nullptr : hts_idx_load(file_name, HTS_FMT_BAI),
      hts_idx_destroy);

  if (index) {
    std::shared_ptr<bam1_t> read(bam_init1(), bam_destroy1);
    // Rummage through all the chromosomes in the header...
    for (auto tid = 0; tid < header->n_targets; tid++) {
      if (!wantChromosome(header, tid)) {
        continue;
      }
      // ...and use the index to seek through chomosome of interest.
      std::shared_ptr<hts_itr_t> itr(
          bam_itr_queryi(index.get(), tid, 0, INT_MAX), hts_itr_destroy);
      while (bam_itr_next(input.get(), itr.get(), read.get()) >= 0) {
        processRead(header, read);
      }
    }
    return true;
  }

  // Cycle through all the reads when an index is unavailable.
  std::shared_ptr<bam1_t> read(bam_init1(), bam_destroy1);
  while (sam_read1(input.get(), header.get(), read.get()) >= 0) {
    processRead(header, read);
  }
}

barf::check_iterator::check_iterator(std::shared_ptr<llvm::ExecutionEngine> &e,
                                     llvm::Module *module,
                                     std::shared_ptr<ast_node> &node,
                                     std::string name)
    : engine(e) {
  // Compile the query into native functions. We must hold a reference to the
  // execution engine as long as we intend for these pointers to be valid.
  filter = getNativeFunction<filter_function>(
      e, node->create_filter_function(module, name));
  std::stringstream index_function_name;
  index_function_name << name << "_index";
  index = getNativeFunction<index_function>(
      e, node->create_index_function(module, index_function_name.str()));
}

bool barf::check_iterator::wantChromosome(std::shared_ptr<bam_hdr_t> &header,
                                          uint32_t tid) {
  return index(header.get(), tid);
}

void barf::check_iterator::processRead(std::shared_ptr<bam_hdr_t> &header,
                                       std::shared_ptr<bam1_t> &read) {
  readMatch(filter(header.get(), read.get()), header, read);
}
