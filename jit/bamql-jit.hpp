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
#include <bamql-compiler.hpp>
#include <bamql-iterator.hpp>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/LLVMContext.h>

#define BAMQL_JIT_API_VERSION 2
namespace bamql {

class CompiledPredicate;

class JIT {
public:
  /**
   * Create a JIT.
   */
  static std::shared_ptr<JIT> create();
  static std::shared_ptr<CompiledPredicate> compile(
      std::shared_ptr<JIT> &jit,
      std::shared_ptr<AstNode> &node,
      const std::string &name,
      llvm::DIScope *debug_scope = nullptr);
  ~JIT();

private:
  JIT();
  std::unique_ptr<llvm::orc::LLJIT> lljit;
  friend class CompiledPredicate;
};
/**
 * Check if a read matches a dynamically compiled predicate
 */
class CompiledPredicate {
public:
  CompiledPredicate(std::shared_ptr<JIT> &jit,
                    std::string name,
                    bamql::FilterFunction filter,
                    bamql::IndexFunction index);
  ~CompiledPredicate();
  bool wantChromosome(std::shared_ptr<bam_hdr_t> &header,
                      uint32_t tid,
                      std::function<void(const char *)> error_handler);
  bool wantRead(std::shared_ptr<bam_hdr_t> &header,
                std::shared_ptr<bam1_t> &read,
                std::function<void(const char *)> error_handler);

private:
  std::shared_ptr<JIT> jit;
  std::string name;
  bamql::FilterFunction filter;
  bamql::IndexFunction index;
};

/**
 * Iterate over the reads in a BAM file, preselecting those through a filter.
 */
class CompileIterator : public ReadIterator {
public:
  CompileIterator(std::shared_ptr<CompiledPredicate> &predicate);
  virtual bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid);
  virtual void processRead(std::shared_ptr<bam_hdr_t> &header,
                           std::shared_ptr<bam1_t> &read);
  virtual void ingestHeader(std::shared_ptr<bam_hdr_t> &header) = 0;
  virtual void handleError(const char *message) = 0;
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
  std::shared_ptr<CompiledPredicate> predicate;
};
} // namespace bamql
