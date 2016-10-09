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

class bamql::UseNode : public bamql::DebuggableNode {
public:
  UseNode(ParseState &state, std::shared_ptr<AstNode> e)
      : DebuggableNode(state), expr(e) {}
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return state.definitions[this];
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    return state.definitionsIndex[this];
  }
  llvm::Value *generateAtDefinition(GenerateState &state,
                                    llvm::Value *read,
                                    llvm::Value *header,
                                    llvm::Value *error_fn,
                                    llvm::Value *error_ctx) {
    auto result = expr->generate(state, read, header, error_fn, error_ctx);
    state.definitions[this] = result;
    return result;
  }
  llvm::Value *generateIndexAtDefinition(GenerateState &state,
                                         llvm::Value *read,
                                         llvm::Value *header,
                                         llvm::Value *error_fn,
                                         llvm::Value *error_ctx) {
    auto result = expr->generateIndex(state, read, header, error_fn, error_ctx);
    state.definitionsIndex[this] = result;
    return result;
  }
  bamql::ExprType type() { return expr->type(); }

private:
  std::shared_ptr<AstNode> expr;
};

bamql::BindingNode::BindingNode(ParseState &state) : DebuggableNode(state) {}
llvm::Value *bamql::BindingNode::generate(GenerateState &state,
                                          llvm::Value *read,
                                          llvm::Value *header,
                                          llvm::Value *error_fn,
                                          llvm::Value *error_ctx) {
  for (auto it = definitions.begin(); it != definitions.end(); it++) {
    (*it)->writeDebug(state);
    (*it)->generateAtDefinition(state, read, header, error_fn, error_ctx);
  }
  body->writeDebug(state);
  return body->generate(state, read, header, error_fn, error_ctx);
}
llvm::Value *bamql::BindingNode::generateIndex(GenerateState &state,
                                               llvm::Value *read,
                                               llvm::Value *header,
                                               llvm::Value *error_fn,
                                               llvm::Value *error_ctx) {
  for (auto it = definitions.begin(); it != definitions.end(); it++) {
    (*it)->writeDebug(state);
    (*it)->generateIndexAtDefinition(state, read, header, error_fn, error_ctx);
  }
  body->writeDebug(state);
  return body->generateIndex(state, read, header, error_fn, error_ctx);
}

void bamql::BindingNode::parse(ParseState &state) throw(ParseError) {
  PredicateMap childPredicates;
  while (!state.empty() && (definitions.size() == 0 || *state == ',')) {
    if (definitions.size() > 0) {
      state.next();
    }
    state.parseSpace();
    auto name = state.parseStr(
        "ABCDEFGHIJLKLMNOPQRSTUVWXYZabcdefghijlklmnopqrstuvwxyz0123456789_");
    state.parseCharInSpace('=');
    auto use = std::make_shared<UseNode>(state, AstNode::parse(state));
    definitions.push_back(use);
    childPredicates[name] = [=](ParseState & state) throw(ParseError) {
      return std::static_pointer_cast<AstNode>(use);
    };
    state.parseSpace();
  }
  if (!state.parseKeyword("in")) {
    throw ParseError(state.where(), "Expected `in' or `,' in `let'.");
  }
  state.push(childPredicates);
  body = AstNode::parse(state);
  state.pop(childPredicates);
}

bamql::ExprType bamql::BindingNode::type() { return body->type(); }
