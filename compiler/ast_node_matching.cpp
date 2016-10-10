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

bamql::RegexNode::RegexNode(std::shared_ptr<AstNode> &operand_,
                            RegularExpression &&pattern_,
                            ParseState &state)
    : DebuggableNode(state), operand(operand_), pattern(std::move(pattern_)) {
  type_check(operand_, bamql::STR);
}
llvm::Value *bamql::RegexNode::generate(GenerateState &state,
                                        llvm::Value *read,
                                        llvm::Value *header,
                                        llvm::Value *error_fn,
                                        llvm::Value *error_ctx) {
  operand->writeDebug(state);
  auto operand_value =
      operand->generate(state, read, header, error_fn, error_ctx);
  this->writeDebug(state);
  auto function = state.module()->getFunction("bamql_re_match");
  llvm::Value *args[] = { pattern(state), operand_value };
  return state->CreateCall(function, args);
}
llvm::Value *bamql::RegexNode::generateIndex(GenerateState &state,
                                             llvm::Value *param,
                                             llvm::Value *header,
                                             llvm::Value *error_fn,
                                             llvm::Value *error_ctx) {
  return llvm::ConstantInt::getTrue(state.module()->getContext());
}
bool bamql::RegexNode::usesIndex() { return false; }
bamql::ExprType bamql::RegexNode::type() { return bamql::BOOL; }

bamql::CompareFPNode::CompareFPNode(bamql::CreateFCmp comparator_,
                                    std::shared_ptr<AstNode> &left_,
                                    std::shared_ptr<AstNode> &right_,
                                    ParseState &state)
    : DebuggableNode(state), comparator(comparator_), left(left_),
      right(right_) {
  type_check(left_, bamql::FP);
  type_check(right_, bamql::FP);
}
llvm::Value *bamql::CompareFPNode::generate(GenerateState &state,
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
bamql::ExprType bamql::CompareFPNode::type() { return bamql::BOOL; }

bamql::CompareIntNode::CompareIntNode(bamql::CreateICmp comparator_,
                                      std::shared_ptr<AstNode> &left_,
                                      std::shared_ptr<AstNode> &right_,
                                      ParseState &state)
    : DebuggableNode(state), comparator(comparator_), left(left_),
      right(right_) {
  type_check(left_, bamql::INT);
  type_check(right_, bamql::INT);
}
llvm::Value *bamql::CompareIntNode::generate(GenerateState &state,
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
bamql::ExprType bamql::CompareIntNode::type() { return bamql::BOOL; }

bamql::CompareStrNode::CompareStrNode(bamql::CreateICmp comparator_,
                                      std::shared_ptr<AstNode> &left_,
                                      std::shared_ptr<AstNode> &right_,
                                      ParseState &state)
    : DebuggableNode(state), comparator(comparator_), left(left_),
      right(right_) {
  type_check(left_, bamql::STR);
  type_check(right_, bamql::STR);
}
llvm::Value *bamql::CompareStrNode::generate(GenerateState &state,
                                             llvm::Value *read,
                                             llvm::Value *header,
                                             llvm::Value *error_fn,
                                             llvm::Value *error_ctx) {
  left->writeDebug(state);
  auto left_value = left->generate(state, read, header, error_fn, error_ctx);
  right->writeDebug(state);
  auto right_value = left->generate(state, read, header, error_fn, error_ctx);
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
bamql::ExprType bamql::CompareStrNode::type() { return bamql::BOOL; }
