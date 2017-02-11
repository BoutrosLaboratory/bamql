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
#include "ast_node_if.hpp"

namespace bamql {
ConditionalNode::ConditionalNode(const std::shared_ptr<AstNode> &condition_,
                                 const std::shared_ptr<AstNode> &then_part_,
                                 const std::shared_ptr<AstNode> &else_part_)
    : condition(condition_), then_part(then_part_), else_part(else_part_) {
  type_check(condition, BOOL);
  type_check_match(then_part, else_part);
}

llvm::Value *ConditionalNode::generate(GenerateState &state,
                                       llvm::Value *read,
                                       llvm::Value *header,
                                       llvm::Value *error_fn,
                                       llvm::Value *error_ctx) {
  /* Create three blocks: one for the “then”, one for the “else” and one for the
   * final. */
  auto function = state->GetInsertBlock()->getParent();
  auto then_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "then", function);
  auto else_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "else", function);
  auto merge_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "merge", function);

  /* Compute the conditional argument and then decide to which block to jump. */
  this->condition->writeDebug(state);
  auto conditional_result =
      condition->generate(state, read, header, error_fn, error_ctx);
  state->CreateCondBr(conditional_result, then_block, else_block);

  /* Generate the “then” block. */
  state->SetInsertPoint(then_block);
  this->then_part->writeDebug(state);
  auto then_result =
      then_part->generate(state, read, header, error_fn, error_ctx);
  /* Jump to the final block. */
  state->CreateBr(merge_block);
  then_block = state->GetInsertBlock();

  /* Generate the “else” block. */
  state->SetInsertPoint(else_block);
  this->else_part->writeDebug(state);
  auto else_result =
      else_part->generate(state, read, header, error_fn, error_ctx);
  /* Jump to the final block. */
  state->CreateBr(merge_block);
  else_block = state->GetInsertBlock();

  /* Get the two results and select the correct one using a PHI node. */
  state->SetInsertPoint(merge_block);
  auto phi =
      state->CreatePHI(llvm::Type::getInt1Ty(state.module()->getContext()), 2);
  phi->addIncoming(then_result, then_block);
  phi->addIncoming(else_result, else_block);
  return phi;
}

bool ConditionalNode::usesIndex() {
  return (then_part->usesIndex() && else_part->usesIndex()) ||
         (condition->usesIndex() &&
          (then_part->usesIndex() || else_part->usesIndex()));
}

ExprType ConditionalNode::type() { return then_part->type(); }

llvm::Value *ConditionalNode::generateIndex(GenerateState &state,
                                            llvm::Value *tid,
                                            llvm::Value *header,
                                            llvm::Value *error_fn,
                                            llvm::Value *error_ctx) {
  if (condition->usesIndex()) {
    /*
     * The logic in this function is twisty, so here is the explanation. Given
     * we have `C ? T : E`, consider the following cases during index building:
     *
     * 1. C is true. If C is true, we don't know if it will necessarily always
     * return true to this chromosome in the query, so we might execute T or E.
     * Therefore, whether this is true is `T | E`.
     *
     * 2. C is false. If C is false, T will never be executed. E might be
     * executed, so we should return E.
     */

    /* Create three blocks: one for the “then”, one for the “else” and one for
     * the final. */
    auto function = state->GetInsertBlock()->getParent();
    auto then_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "then", function);
    auto else_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "else", function);
    auto merge_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "merge", function);

    /* Compute the conditional argument and then decide to which block to jump.
     * If true, try to make a decision based on the “then” block, otherwise,
     * only make a decision based on the “else” block. */
    this->condition->writeDebug(state);
    auto conditional_result =
        condition->generateIndex(state, tid, header, error_fn, error_ctx);
    state->CreateCondBr(conditional_result, then_block, else_block);

    /* Generate the “then” block. */
    state->SetInsertPoint(then_block);
    this->then_part->writeDebug(state);
    auto then_result =
        then_part->generateIndex(state, tid, header, error_fn, error_ctx);
    /* If we fail, the “else” block might still be interested. */
    state->CreateCondBr(then_result, merge_block, else_block);
    then_block = state->GetInsertBlock();

    /* Generate the “else” block. */
    state->SetInsertPoint(else_block);
    this->else_part->writeDebug(state);
    auto else_result =
        else_part->generateIndex(state, tid, header, error_fn, error_ctx);
    /* Jump to the final block. */
    state->CreateBr(merge_block);
    else_block = state->GetInsertBlock();

    /* Get the two results and select the correct one using a PHI node. */
    state->SetInsertPoint(merge_block);
    auto phi = state->CreatePHI(
        llvm::Type::getInt1Ty(state.module()->getContext()), 2);
    phi->addIncoming(then_result, then_block);
    phi->addIncoming(else_result, else_block);
    return phi;
  }
  if (then_part->usesIndex() && else_part->usesIndex()) {
    auto function = state->GetInsertBlock()->getParent();
    auto else_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "else", function);
    auto merge_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "merge", function);
    auto then_result =
        then_part->generateIndex(state, tid, header, error_fn, error_ctx);
    state->CreateCondBr(then_result, merge_block, else_block);
    auto original_block = state->GetInsertBlock();
    state->SetInsertPoint(else_block);
    this->else_part->writeDebug(state);
    auto else_result =
        else_part->generateIndex(state, tid, header, error_fn, error_ctx);
    state->CreateBr(merge_block);
    else_block = state->GetInsertBlock();
    state->SetInsertPoint(merge_block);
    auto phi = state->CreatePHI(
        llvm::Type::getInt1Ty(state.module()->getContext()), 2);
    phi->addIncoming(then_result, original_block);
    phi->addIncoming(else_result, else_block);
    return phi;
  }
  return llvm::ConstantInt::getTrue(state.module()->getContext());
}
void ConditionalNode::writeDebug(GenerateState &) {}
}
