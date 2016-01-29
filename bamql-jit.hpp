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

#pragma once
#include <bamql.hpp>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace bamql {

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
std::shared_ptr<llvm::ExecutionEngine> createEngine(
    std::unique_ptr<llvm::Module> module);

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

private:
  bool wantAll(std::shared_ptr<bam_hdr_t> &header);
};
/**
 * Iterate over the reads in a BAM file, preselecting those through a filter.
 */
class CheckIterator : public ReadIterator {
public:
  CheckIterator(std::shared_ptr<llvm::ExecutionEngine> &engine,
                std::shared_ptr<Generator> &generator,
                std::shared_ptr<AstNode> &node,
                std::string name);
  virtual void prepareExecution();
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
  bamql::FilterFunction filter;
  bamql::IndexFunction index;
  llvm::Function *filter_func;
  llvm::Function *index_func;
  std::shared_ptr<llvm::ExecutionEngine> engine;
};

/**
 * Craft a new BAM header appending information about the manipulations done.
 * @param name: the name of the program doing the manipulation.
 * @param id: a unique ID.
 * @param version: the version of the program doing the manipulation.
 * @param args: the command line arguments passed to this program.
 */
std::shared_ptr<bam_hdr_t> appendProgramToHeader(const bam_hdr_t *original,
                                                 const std::string &name,
                                                 const std::string &id,
                                                 const std::string &version,
                                                 const std::string &args);

std::shared_ptr<htsFile> open(const char *filename, const char *mode);
}
