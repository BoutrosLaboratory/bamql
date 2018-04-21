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

#include "ast_node_regex.hpp"
#include "bamql-compiler.hpp"
#include "compiler.hpp"

namespace bamql {
RegexNode::RegexNode(std::shared_ptr<AstNode> &operand_,
                     RegularExpression &&pattern_,
                     ParseState &state)
    : DebuggableNode(state), operand(operand_), pattern(std::move(pattern_)) {
  type_check(operand, STR);
}
llvm::Value *RegexNode::generate(GenerateState &state,
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
llvm::Value *RegexNode::generateIndex(GenerateState &state,
                                      llvm::Value *param,
                                      llvm::Value *header,
                                      llvm::Value *error_fn,
                                      llvm::Value *error_ctx) {
  return llvm::ConstantInt::getTrue(state.module()->getContext());
}
bool RegexNode::usesIndex() { return false; }
ExprType RegexNode::type() { return BOOL; }
}
