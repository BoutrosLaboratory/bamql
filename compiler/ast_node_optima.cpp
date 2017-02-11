/*
 * Copyright 2017 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
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

class ChooseBetter : public DebuggableNode {
public:
  ChooseBetter(ParseState &state,
               bool direction_,
               std::shared_ptr<AstNode> &left_,
               std::shared_ptr<AstNode> &right_)
      : DebuggableNode(state), direction(direction_), left(left_),
        right(right_) {
    type_check_match(left, right);
    type_check_not(left, BOOL);
  }

  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    left->writeDebug(state);
    auto left_value = left->generate(state, read, header, error_fn, error_ctx);
    right->writeDebug(state);
    auto right_value =
        right->generate(state, read, header, error_fn, error_ctx);
    this->writeDebug(state);
    llvm::Value *result;
    switch (left->type()) {
    case FP:
      result = state->CreateFCmpOLT(left_value, right_value);
      break;
    case INT:
      result = state->CreateICmpSLT(left_value, right_value);
      break;
    case STR:
      do {
        llvm::Value *args[] = { left_value, right_value };
        auto comparison = state->CreateCall(
            state.module()->getFunction("bamql_strcmp"), args);
        result = state->CreateICmpSLT(
            comparison,
            llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(state.module()->getContext()), 0));
      } while (0);
      break;
    default:
      abort();
    }
    return state->CreateSelect(result,
                               direction ? left_value : right_value,
                               direction ? right_value : left_value);
  }
  ExprType type() { return left->type(); }
  bool direction;
  std::shared_ptr<AstNode> left;
  std::shared_ptr<AstNode> right;

  static std::shared_ptr<AstNode> parse(ParseState &state,
                                        bool direction) throw(ParseError) {
    state.parseCharInSpace('(');
    auto node = AstNode::parse(state);
    if (node->type() != FP && node->type() != INT && node->type() != STR) {
      throw ParseError(
          state.where(),
          "Only valid for floating point numbers, integers, or strings.");
    }
    state.parseSpace();
    while (!state.empty() && *state == ',') {
      state.next();
      state.parseSpace();
      auto next = AstNode::parse(state);
      if (next->type() != node->type()) {
        throw ParseError(state.where(), "All values must be of the same type.");
      }
      auto temp = std::make_shared<ChooseBetter>(state, direction, node, next);
      node = temp;
      state.parseSpace();
    }
    state.parseCharInSpace(')');
    return node;
  }
};
std::shared_ptr<AstNode> parseMin(ParseState &state) throw(ParseError) {
  return ChooseBetter::parse(state, true);
}
std::shared_ptr<AstNode> parseMax(ParseState &state) throw(ParseError) {
  return ChooseBetter::parse(state, false);
}
}
