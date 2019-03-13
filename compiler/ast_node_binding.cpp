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

class UseNode final : public DebuggableNode {
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
  ExprType type() { return expr->type(); }

private:
  std::shared_ptr<AstNode> expr;
};

class BindingNode final : public DebuggableNode {
public:
  BindingNode(ParseState &state) : DebuggableNode(state) {}
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    for (auto &def : definitions) {
      def->writeDebug(state);
      def->generateAtDefinition(state, read, header, error_fn, error_ctx);
    }
    body->writeDebug(state);
    return body->generate(state, read, header, error_fn, error_ctx);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    for (auto &def : definitions) {
      def->writeDebug(state);
      def->generateIndexAtDefinition(state, read, header, error_fn, error_ctx);
    }
    body->writeDebug(state);
    return body->generateIndex(state, read, header, error_fn, error_ctx);
  }

  void parse(ParseState &state);

  ExprType type() { return body->type(); }

private:
  std::vector<std::shared_ptr<UseNode>> definitions;
  std::shared_ptr<AstNode> body;
  friend std::shared_ptr<AstNode> parseBinding(ParseState &state);
};

std::shared_ptr<AstNode> parseBinding(ParseState &state) {
  auto let = std::make_shared<BindingNode>(state);
  PredicateMap childPredicates;
  while (!state.empty() && (let->definitions.size() == 0 || *state == ',')) {
    if (let->definitions.size() > 0) {
      state.next();
    }
    if (!state.parseSpace()) {
      throw ParseError(state.where(), "Expected space.");
    }
    auto name = state.parseStr(
        "ABCDEFGHIJLKLMNOPQRSTUVWXYZabcdefghijlklmnopqrstuvwxyz0123456789_");
    state.parseCharInSpace('=');
    auto use = std::make_shared<UseNode>(state, AstNode::parse(state));
    let->definitions.push_back(use);
    childPredicates[name] = [=](ParseState &state) {
      return std::static_pointer_cast<AstNode>(use);
    };
    state.parseSpace();
  }
  if (!state.parseKeyword("in")) {
    throw ParseError(state.where(), "Expected `in' or `,' in `let'.");
  }
  state.push(childPredicates);
  let->body = AstNode::parse(state);
  state.pop(childPredicates);
  return let;
}
}
