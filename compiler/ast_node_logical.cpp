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
    type_check(left, BOOL);
    type_check(right, BOOL);
  }

  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return generateGeneric(
        &AstNode::generate, state, read, header, error_fn, error_ctx);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *tid,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    if (usesIndex()) {
      return generateGeneric(
          &AstNode::generateIndex, state, tid, header, error_fn, error_ctx);
    } else {
      return llvm::ConstantInt::getTrue(state.module()->getContext());
    }
  }
  bool usesIndex() { return left->usesIndex() || right->usesIndex(); }
  ExprType type() { return BOOL; }
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
class AndNode final : public ShortCircuitNode {
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
class OrNode final : public ShortCircuitNode {
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
class XOrNode final : public AstNode {
public:
  XOrNode(const std::shared_ptr<AstNode> &left_,
          const std::shared_ptr<AstNode> &right_)
      : left(left_), right(right_) {
    type_check(left, BOOL);
    type_check(right, BOOL);
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
  ExprType type() { return BOOL; }

  void writeDebug(GenerateState &state) {}

private:
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;
};
/**
 * A syntax node for logical complement (NOT).
 */
class NotNode final : public AstNode {
public:
  NotNode(const std::shared_ptr<AstNode> &expr_) : expr(expr_) {
    type_check(expr, BOOL);
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
  ExprType type() { return BOOL; }

  void writeDebug(GenerateState &state) {}

private:
  std::shared_ptr<AstNode> expr;
};
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
