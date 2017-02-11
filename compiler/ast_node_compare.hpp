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

#include "bamql-compiler.hpp"

namespace bamql {

typedef llvm::Value *(llvm::IRBuilder<>::*CreateICmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name);
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 7
typedef llvm::Value *(llvm::IRBuilder<>::*CreateFCmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name);
#else
typedef llvm::Value *(llvm::IRBuilder<>::*CreateFCmp)(llvm::Value *lhs,
                                                      llvm::Value *rhs,
                                                      const llvm::Twine &name,
                                                      llvm::MDNode *fpmathtag);
#endif

class CompareFPNode : public DebuggableNode {
public:
  CompareFPNode(CreateFCmp comparator,
                std::shared_ptr<AstNode> &left,
                std::shared_ptr<AstNode> &right,
                ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateFCmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class CompareIntNode : public DebuggableNode {
public:
  CompareIntNode(CreateICmp comparator,
                 std::shared_ptr<AstNode> &left,
                 std::shared_ptr<AstNode> &right,
                 ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateICmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};

class CompareStrNode : public DebuggableNode {
public:
  CompareStrNode(CreateICmp comparator,
                 std::shared_ptr<AstNode> &left,
                 std::shared_ptr<AstNode> &right,
                 ParseState &state);
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx);
  ExprType type();

private:
  CreateICmp comparator;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
}
