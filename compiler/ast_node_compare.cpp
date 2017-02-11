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

#include "bamql-compiler.hpp"
#include "compiler.hpp"
#include "ast_node_compare.hpp"

namespace bamql {
CompareFPNode::CompareFPNode(CreateFCmp comparator_,
                             std::shared_ptr<AstNode> &left_,
                             std::shared_ptr<AstNode> &right_,
                             ParseState &state)
    : DebuggableNode(state), comparator(comparator_), left(left_),
      right(right_) {
  type_check(left, FP);
  type_check(right, FP);
}
llvm::Value *CompareFPNode::generate(GenerateState &state,
                                     llvm::Value *read,
                                     llvm::Value *header,
                                     llvm::Value *error_fn,
                                     llvm::Value *error_ctx) {
  left->writeDebug(state);
  auto left_value = left->generate(state, read, header, error_fn, error_ctx);
  right->writeDebug(state);
  auto right_value = right->generate(state, read, header, error_fn, error_ctx);
  this->writeDebug(state);
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 7
  return ((*state)->*comparator)(left_value, right_value, "");
#else
  return ((*state)->*comparator)(left_value, right_value, "", nullptr);
#endif
}
ExprType CompareFPNode::type() { return BOOL; }

CompareIntNode::CompareIntNode(CreateICmp comparator_,
                               std::shared_ptr<AstNode> &left_,
                               std::shared_ptr<AstNode> &right_,
                               ParseState &state)
    : DebuggableNode(state), comparator(comparator_), left(left_),
      right(right_) {
  type_check(left, INT);
  type_check(right, INT);
}
llvm::Value *CompareIntNode::generate(GenerateState &state,
                                      llvm::Value *read,
                                      llvm::Value *header,
                                      llvm::Value *error_fn,
                                      llvm::Value *error_ctx) {
  left->writeDebug(state);
  auto left_value = left->generate(state, read, header, error_fn, error_ctx);
  right->writeDebug(state);
  auto right_value = right->generate(state, read, header, error_fn, error_ctx);
  this->writeDebug(state);
  return ((*state)->*comparator)(left_value, right_value, "");
}
ExprType CompareIntNode::type() { return BOOL; }

CompareStrNode::CompareStrNode(CreateICmp comparator_,
                               std::shared_ptr<AstNode> &left_,
                               std::shared_ptr<AstNode> &right_,
                               ParseState &state)
    : DebuggableNode(state), comparator(comparator_), left(left_),
      right(right_) {
  type_check(left, STR);
  type_check(right, STR);
}
llvm::Value *CompareStrNode::generate(GenerateState &state,
                                      llvm::Value *read,
                                      llvm::Value *header,
                                      llvm::Value *error_fn,
                                      llvm::Value *error_ctx) {
  left->writeDebug(state);
  auto left_value = left->generate(state, read, header, error_fn, error_ctx);
  right->writeDebug(state);
  auto right_value = right->generate(state, read, header, error_fn, error_ctx);
  this->writeDebug(state);
  auto function = state.module()->getFunction("bamql_strcmp");
  llvm::Value *args[] = { left_value, right_value };
  auto result = state->CreateCall(function, args);
  return ((*state)->*comparator)(
      result,
      llvm::ConstantInt::get(
          llvm::Type::getInt32Ty(state.module()->getContext()), 0),
      "");
}
ExprType CompareStrNode::type() { return BOOL; }
}
