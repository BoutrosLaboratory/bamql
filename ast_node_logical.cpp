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

#include "barf.hpp"

barf::ShortCircuitNode::ShortCircuitNode(std::shared_ptr<AstNode> left,
                                         std::shared_ptr<AstNode> right) {
  this->left = left;
  this->right = right;
}

llvm::Value *barf::ShortCircuitNode::generateGeneric(
    GenerateMember member,
    llvm::Module *module,
    llvm::IRBuilder<> &builder,
    llvm::Value *param,
    llvm::Value *header,
    llvm::DIScope *debug_scope) {
  /* Create two basic blocks for the possibly executed right-hand expression and
   * the final block. */
  auto function = builder.GetInsertBlock()->getParent();
  auto next_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "next", function);
  auto merge_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge", function);

  /* Generate the left expression in the current block. */
  this->left->writeDebug(module, builder, debug_scope);
  auto left_value =
      ((*this->left).*member)(module, builder, param, header, debug_scope);
  auto short_circuit_value =
      builder.CreateICmpEQ(left_value, this->branchValue());
  /* If short circuiting, jump to the final block, otherwise, do the right-hand
   * expression. */
  builder.CreateCondBr(short_circuit_value, merge_block, next_block);
  auto original_block = builder.GetInsertBlock();

  /* Generate the right-hand expression, then jump to the final block.*/
  builder.SetInsertPoint(next_block);
  this->right->writeDebug(module, builder, debug_scope);
  auto right_value =
      ((*this->right).*member)(module, builder, param, header, debug_scope);
  builder.CreateBr(merge_block);
  next_block = builder.GetInsertBlock();

  /* Merge from the above paths, selecting the correct value through a PHI node.
   */
  builder.SetInsertPoint(merge_block);
  auto phi =
      builder.CreatePHI(llvm::Type::getInt1Ty(llvm::getGlobalContext()), 2);
  phi->addIncoming(left_value, original_block);
  phi->addIncoming(right_value, next_block);
  return phi;
}

llvm::Value *barf::ShortCircuitNode::generate(llvm::Module *module,
                                              llvm::IRBuilder<> &builder,
                                              llvm::Value *read,
                                              llvm::Value *header,
                                              llvm::DIScope *debug_scope) {
  return generateGeneric(
      &barf::AstNode::generate, module, builder, read, header, debug_scope);
}

llvm::Value *barf::ShortCircuitNode::generateIndex(llvm::Module *module,
                                                   llvm::IRBuilder<> &builder,
                                                   llvm::Value *tid,
                                                   llvm::Value *header,
                                                   llvm::DIScope *debug_scope) {
  return generateGeneric(
      &barf::AstNode::generateIndex, module, builder, tid, header, debug_scope);
}
void barf::ShortCircuitNode::writeDebug(llvm::Module *module,
                                        llvm::IRBuilder<> &builder,
                                        llvm::DIScope *debug_scope) {}

barf::AndNode::AndNode(std::shared_ptr<AstNode> left,
                       std::shared_ptr<AstNode> right)
    : ShortCircuitNode(left, right) {}
llvm::Value *barf::AndNode::branchValue() {
  return llvm::ConstantInt::getFalse(llvm::getGlobalContext());
}

barf::OrNode::OrNode(std::shared_ptr<AstNode> left,
                     std::shared_ptr<AstNode> right)
    : ShortCircuitNode(left, right) {}
llvm::Value *barf::OrNode::branchValue() {
  return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
}

barf::XOrNode::XOrNode(std::shared_ptr<AstNode> left_,
                       std::shared_ptr<AstNode> right_)
    : left(left_), right(right_) {}
llvm::Value *barf::XOrNode::generate(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *read,
                                     llvm::Value *header,
                                     llvm::DIScope *debug_scope) {
  auto left_value = left->generate(module, builder, read, header, debug_scope);
  auto right_value =
      right->generate(module, builder, read, header, debug_scope);
  return builder.CreateICmpNE(left_value, right_value);
}
llvm::Value *barf::XOrNode::generateIndex(llvm::Module *module,
                                          llvm::IRBuilder<> &builder,
                                          llvm::Value *tid,
                                          llvm::Value *header,
                                          llvm::DIScope *debug_scope) {
  auto left_value =
      left->generateIndex(module, builder, tid, header, debug_scope);
  auto right_value =
      right->generateIndex(module, builder, tid, header, debug_scope);
  return builder.CreateICmpNE(left_value, right_value);
}

void barf::XOrNode::writeDebug(llvm::Module *module,
                               llvm::IRBuilder<> &builder,
                               llvm::DIScope *debug_scope) {}

barf::NotNode::NotNode(std::shared_ptr<AstNode> expr) { this->expr = expr; }
llvm::Value *barf::NotNode::generate(llvm::Module *module,
                                     llvm::IRBuilder<> &builder,
                                     llvm::Value *read,
                                     llvm::Value *header,
                                     llvm::DIScope *debug_scope) {
  this->expr->writeDebug(module, builder, debug_scope);
  llvm::Value *result =
      this->expr->generate(module, builder, read, header, debug_scope);
  return builder.CreateNot(result);
}

llvm::Value *barf::NotNode::generateIndex(llvm::Module *module,
                                          llvm::IRBuilder<> &builder,
                                          llvm::Value *tid,
                                          llvm::Value *header,
                                          llvm::DIScope *debug_scope) {
  this->expr->writeDebug(module, builder, debug_scope);
  llvm::Value *result =
      this->expr->generateIndex(module, builder, tid, header, debug_scope);
  return builder.CreateNot(result);
}
void barf::NotNode::writeDebug(llvm::Module *module,
                               llvm::IRBuilder<> &builder,
                               llvm::DIScope *debug_scope) {}

barf::ConditionalNode::ConditionalNode(std::shared_ptr<AstNode> condition,
                                       std::shared_ptr<AstNode> then_part,
                                       std::shared_ptr<AstNode> else_part) {
  this->condition = condition;
  this->then_part = then_part;
  this->else_part = else_part;
}

llvm::Value *barf::ConditionalNode::generate(llvm::Module *module,
                                             llvm::IRBuilder<> &builder,
                                             llvm::Value *read,
                                             llvm::Value *header,
                                             llvm::DIScope *debug_scope) {
  /* Create three blocks: one for the “then”, one for the “else” and one for the
   * final. */
  auto function = builder.GetInsertBlock()->getParent();
  auto then_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", function);
  auto else_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "else", function);
  auto merge_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge", function);

  /* Compute the conditional argument and then decide to which block to jump. */
  this->condition->writeDebug(module, builder, debug_scope);
  auto conditional_result =
      condition->generate(module, builder, read, header, debug_scope);
  builder.CreateCondBr(conditional_result, then_block, else_block);

  /* Generate the “then” block. */
  builder.SetInsertPoint(then_block);
  this->then_part->writeDebug(module, builder, debug_scope);
  auto then_result =
      then_part->generate(module, builder, read, header, debug_scope);
  /* Jump to the final block. */
  builder.CreateBr(merge_block);
  then_block = builder.GetInsertBlock();

  /* Generate the “else” block. */
  builder.SetInsertPoint(else_block);
  this->else_part->writeDebug(module, builder, debug_scope);
  auto else_result =
      else_part->generate(module, builder, read, header, debug_scope);
  /* Jump to the final block. */
  builder.CreateBr(merge_block);
  else_block = builder.GetInsertBlock();

  /* Get the two results and select the correct one using a PHI node. */
  builder.SetInsertPoint(merge_block);
  auto phi =
      builder.CreatePHI(llvm::Type::getInt1Ty(llvm::getGlobalContext()), 2);
  phi->addIncoming(then_result, then_block);
  phi->addIncoming(else_result, else_block);
  return phi;
}

llvm::Value *barf::ConditionalNode::generateIndex(llvm::Module *module,
                                                  llvm::IRBuilder<> &builder,
                                                  llvm::Value *tid,
                                                  llvm::Value *header,
                                                  llvm::DIScope *debug_scope) {
  /*
   * The logic in this function is twisty, so here is the explanation. Given we
   * have `C ? T : E`, consider the following cases during index building:
   *
   * 1. C is true. If C is true, we don't know if it will necessarily always
   * return true to this chromosome in the query, so we might execute T or E.
   * Therefore, whether this is true is `T | E`.
   *
   * 2. C is false. If C is false, T will never be executed. E might be
   * executed, so we should return E.
   */

  /* Create three blocks: one for the “then”, one for the “else” and one for the
   * final. */
  auto function = builder.GetInsertBlock()->getParent();
  auto then_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", function);
  auto else_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "else", function);
  auto merge_block =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "merge", function);

  /* Compute the conditional argument and then decide to which block to jump.
   * If true, try to make a decision based on the “then” block, otherwise, only
   * make a decision based on the “else” block. */
  this->condition->writeDebug(module, builder, debug_scope);
  auto conditional_result =
      condition->generateIndex(module, builder, tid, header, debug_scope);
  builder.CreateCondBr(conditional_result, then_block, else_block);

  /* Generate the “then” block. */
  builder.SetInsertPoint(then_block);
  this->then_part->writeDebug(module, builder, debug_scope);
  auto then_result =
      then_part->generateIndex(module, builder, tid, header, debug_scope);
  /* If we fail, the “else” block might still be interested. */
  builder.CreateCondBr(then_result, merge_block, else_block);
  then_block = builder.GetInsertBlock();

  /* Generate the “else” block. */
  builder.SetInsertPoint(else_block);
  this->else_part->writeDebug(module, builder, debug_scope);
  auto else_result =
      else_part->generateIndex(module, builder, tid, header, debug_scope);
  /* Jump to the final block. */
  builder.CreateBr(merge_block);
  else_block = builder.GetInsertBlock();

  /* Get the two results and select the correct one using a PHI node. */
  builder.SetInsertPoint(merge_block);
  auto phi =
      builder.CreatePHI(llvm::Type::getInt1Ty(llvm::getGlobalContext()), 2);
  phi->addIncoming(then_result, then_block);
  phi->addIncoming(else_result, else_block);
  return phi;
}
void barf::ConditionalNode::writeDebug(llvm::Module *module,
                                       llvm::IRBuilder<> &builder,
                                       llvm::DIScope *debug_scope) {}
