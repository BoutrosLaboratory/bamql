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
#include "ast_node_contains.hpp"

bamql::BitwiseContainsNode::BitwiseContainsNode(
    std::shared_ptr<AstNode> &haystack_,
    std::shared_ptr<AstNode> &needle_,
    ParseState &state)
    : DebuggableNode(state), haystack(haystack_), needle(needle_) {
  type_check(haystack, INT);
  type_check(needle, INT);
}

llvm::Value *bamql::BitwiseContainsNode::generate(GenerateState &state,
                                                  llvm::Value *read,
                                                  llvm::Value *header,
                                                  llvm::Value *error_fn,
                                                  llvm::Value *error_ctx) {
  haystack->writeDebug(state);
  auto haystack_value =
      haystack->generate(state, read, header, error_fn, error_ctx);
  needle->writeDebug(state);
  auto needle_value =
      needle->generate(state, read, header, error_fn, error_ctx);
  this->writeDebug(state);
  return state->CreateICmpEQ(state->CreateAnd(haystack_value, needle_value),
                             needle_value);
}

bamql::ExprType bamql::BitwiseContainsNode::type() { return bamql::BOOL; }
