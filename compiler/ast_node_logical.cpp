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

namespace bamql {
/**
 * An abstract syntax node encompassing logical ANDs and ORs that can
 * short-circuit.
 */
class ShortCircuitNode : public AstNode {
public:
  ShortCircuitNode(const std::shared_ptr<AstNode> &left_,
                   const std::shared_ptr<AstNode> &right_)
      : left(left_), right(right_) {
    type_check(left, bamql::BOOL);
    type_check(right, bamql::BOOL);
  }

  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return generateGeneric(
        &bamql::AstNode::generate, state, read, header, error_fn, error_ctx);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *tid,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    if (usesIndex()) {
      return generateGeneric(&bamql::AstNode::generateIndex,
                             state,
                             tid,
                             header,
                             error_fn,
                             error_ctx);
    } else {
      return llvm::ConstantInt::getTrue(state.module()->getContext());
    }
  }
  bool usesIndex() { return left->usesIndex() || right->usesIndex(); }
  ExprType type() { return bamql::BOOL; }
  /**
   * The value that causes short circuting.
   */
  virtual llvm::Value *branchValue(llvm::LLVMContext &context) = 0;

  void writeDebug(GenerateState &state) {}

private:
  llvm::Value *generateGeneric(GenerateMember member,
                               GenerateState &state,
                               llvm::Value *param,
                               llvm::Value *header,
                               llvm::Value *error_fn,
                               llvm::Value *error_ctx) {
    /* Create two basic blocks for the possibly executed right-hand expression
     * and
     * the final block. */
    auto function = state->GetInsertBlock()->getParent();
    auto next_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "next", function);
    auto merge_block = llvm::BasicBlock::Create(
        state.module()->getContext(), "merge", function);

    /* Generate the left expression in the current block. */
    this->left->writeDebug(state);
    auto left_value =
        ((*this->left).*member)(state, param, header, error_fn, error_ctx);
    auto short_circuit_value = state->CreateICmpEQ(
        left_value, this->branchValue(state.module()->getContext()));
    /* If short circuiting, jump to the final block, otherwise, do the
     * right-hand
     * expression. */
    state->CreateCondBr(short_circuit_value, merge_block, next_block);
    auto original_block = state->GetInsertBlock();

    /* Generate the right-hand expression, then jump to the final block.*/
    state->SetInsertPoint(next_block);
    this->right->writeDebug(state);
    auto right_value =
        ((*this->right).*member)(state, param, header, error_fn, error_ctx);
    state->CreateBr(merge_block);
    next_block = state->GetInsertBlock();

    /* Merge from the above paths, selecting the correct value through a PHI
     * node.
     */
    state->SetInsertPoint(merge_block);
    auto phi = state->CreatePHI(
        llvm::Type::getInt1Ty(state.module()->getContext()), 2);
    phi->addIncoming(left_value, original_block);
    phi->addIncoming(right_value, next_block);
    return phi;
  }
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
/**
 * A syntax node for logical conjunction (AND).
 */
class AndNode : public ShortCircuitNode {
public:
  AndNode(const std::shared_ptr<AstNode> &left_,
          const std::shared_ptr<AstNode> &right_)
      : ShortCircuitNode(left_, right_) {}
  llvm::Value *branchValue(llvm::LLVMContext &context) {
    return llvm::ConstantInt::getFalse(context);
  }
};
/**
 * A syntax node for logical disjunction (OR).
 */
class OrNode : public ShortCircuitNode {
public:
  OrNode(const std::shared_ptr<AstNode> &left_,
         const std::shared_ptr<AstNode> &right_)
      : ShortCircuitNode(left_, right_) {}
  llvm::Value *branchValue(llvm::LLVMContext &context) {
    return llvm::ConstantInt::getTrue(context);
  }
};
/**
 * A syntax node for exclusive disjunction (XOR).
 */
class XOrNode : public AstNode {
public:
  XOrNode(const std::shared_ptr<AstNode> &left_,
          const std::shared_ptr<AstNode> &right_)
      : left(left_), right(right_) {
    type_check(left_, bamql::BOOL);
    type_check(right_, bamql::BOOL);
  }

  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    auto left_value = left->generate(state, read, header, error_fn, error_ctx);
    auto right_value =
        right->generate(state, read, header, error_fn, error_ctx);
    return state->CreateICmpNE(left_value, right_value);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *tid,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    if (usesIndex()) {
      auto left_value =
          left->generateIndex(state, tid, header, error_fn, error_ctx);
      auto right_value =
          right->generateIndex(state, tid, header, error_fn, error_ctx);
      return state->CreateICmpNE(left_value, right_value);
    } else {
      return llvm::ConstantInt::getTrue(state.module()->getContext());
    }
  }
  bool usesIndex() { return left->usesIndex() || right->usesIndex(); }
  ExprType type() { return bamql::BOOL; }

  void writeDebug(GenerateState &state) {}

private:
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
/**
 * A syntax node for logical complement (NOT).
 */
class NotNode : public AstNode {
public:
  NotNode(const std::shared_ptr<AstNode> &expr_) : expr(expr_) {
    type_check(expr, bamql::BOOL);
  }
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    this->expr->writeDebug(state);
    llvm::Value *result =
        this->expr->generate(state, read, header, error_fn, error_ctx);
    return state->CreateNot(result);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *tid,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    this->expr->writeDebug(state);
    llvm::Value *result =
        this->expr->generateIndex(state, tid, header, error_fn, error_ctx);
    return state->CreateNot(result);
  }
  bool usesIndex() { return expr->usesIndex(); }
  ExprType type() { return bamql::BOOL; }

  void writeDebug(GenerateState &state) {}

private:
  std::shared_ptr<AstNode> expr;
};

bamql::ConditionalNode::ConditionalNode(
    const std::shared_ptr<AstNode> &condition_,
    const std::shared_ptr<AstNode> &then_part_,
    const std::shared_ptr<AstNode> &else_part_)
    : condition(condition_), then_part(then_part_), else_part(else_part_) {
  type_check(condition_, bamql::BOOL);
  if (then_part_->type() != else_part_->type()) {
    std::cerr << "Then and Else parts of conditional do not have the same type."
              << std::endl;
    abort();
  }
}

llvm::Value *bamql::ConditionalNode::generate(GenerateState &state,
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

bool bamql::ConditionalNode::usesIndex() {
  return (then_part->usesIndex() && else_part->usesIndex()) ||
         (condition->usesIndex() &&
          (then_part->usesIndex() || else_part->usesIndex()));
}

bamql::ExprType bamql::ConditionalNode::type() { return then_part->type(); }

llvm::Value *bamql::ConditionalNode::generateIndex(GenerateState &state,
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
void bamql::ConditionalNode::writeDebug(GenerateState &) {}
}

namespace std {
std::shared_ptr<bamql::AstNode> operator&(
    std::shared_ptr<bamql::AstNode> left,
    std::shared_ptr<bamql::AstNode> right) {
  auto result = std::make_shared<bamql::AndNode>(left, right);
  return result;
}
std::shared_ptr<bamql::AstNode> operator|(
    std::shared_ptr<bamql::AstNode> left,
    std::shared_ptr<bamql::AstNode> right) {
  auto result = std::make_shared<bamql::OrNode>(left, right);
  return result;
}
std::shared_ptr<bamql::AstNode> operator^(
    std::shared_ptr<bamql::AstNode> left,
    std::shared_ptr<bamql::AstNode> right) {
  auto result = std::make_shared<bamql::XOrNode>(left, right);
  return result;
} std::shared_ptr<bamql::AstNode>
operator~(std::shared_ptr<bamql::AstNode> expr) {
  auto result = std::make_shared<bamql::NotNode>(expr);
  return result;
}
}
