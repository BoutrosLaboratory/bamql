#pragma once
#include <barf.hpp>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>

namespace barf {

/**
 * The run-time type of a filter.
 */
typedef bool (*FilterFunction)(bam_hdr_t *, bam1_t *);

/**
 * The run-time type of an index checker.
 */
typedef bool (*IndexFunction)(bam_hdr_t *, uint32_t);

/**
 * Call the JITer on a function and return it as the correct type.
 */
template <typename T>
T getNativeFunction(std::shared_ptr<llvm::ExecutionEngine> &engine,
                    llvm::Function *function) {
  union {
    T func;
    void *ptr;
  } result;
  result.ptr = engine->getPointerToFunction(function);
  return result.func;
}

/**
 * Create a JIT.
 */
std::shared_ptr<llvm::ExecutionEngine> createEngine(llvm::Module *module);

/**
 * Iterator over all the reads in a BAM file, using an index if possible.
 */
class ReadIterator {
public:
  ReadIterator();
  /**
   * Should the reads on this chromosome be examined?
   */
  virtual bool wantChromosome(std::shared_ptr<bam_hdr_t> &header,
                              uint32_t tid) = 0;
  /**
   * Examine a read.
   */
  virtual void processRead(std::shared_ptr<bam_hdr_t> &header,
                           std::shared_ptr<bam1_t> &read) = 0;
  /**
   * Examine the header of a new file.
   */
  virtual void ingestHeader(std::shared_ptr<bam_hdr_t> &header) = 0;
  /**
   * Process the reads in the supplied file.
   * @param file_name: The path to the BAM/SAM file.
   * @param binary: Is the file BAM (true) or SAM (false).
   * @param ignore_index: Do not use the index even if one is found.
   */
  bool processFile(const char *file_name, bool binary, bool ignore_index);
};
/**
 * Iterate over the reads in a BAM file, preselecting those through a filter.
 */
class CheckIterator : public ReadIterator {
public:
  CheckIterator(std::shared_ptr<llvm::ExecutionEngine> &engine,
                llvm::Module *module,
                std::shared_ptr<AstNode> &node,
                std::string name);
  virtual bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid);
  virtual void processRead(std::shared_ptr<bam_hdr_t> &header,
                           std::shared_ptr<bam1_t> &read);
  virtual void ingestHeader(std::shared_ptr<bam_hdr_t> &header) = 0;
  /**
   * After filtering, do something useful with a read based on whether it
   * matches the filter.
   * @param matches: whether the read passes the filter.
   * @param header: the BAM header.
   * @param read: the BAM read.
   */
  virtual void readMatch(bool matches,
                         std::shared_ptr<bam_hdr_t> &header,
                         std::shared_ptr<bam1_t> &read) = 0;

private:
  barf::FilterFunction filter;
  barf::IndexFunction index;
  std::shared_ptr<llvm::ExecutionEngine> engine;
};
}
