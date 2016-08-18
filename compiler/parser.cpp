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

#include <iostream>
#include "bamql-compiler.hpp"

namespace bamql {

typedef std::shared_ptr<AstNode>(*ParseFunc)(ParseState &state,
                                             PredicateMap &predicates);
class UseNode : public DebuggableNode {
public:
  UseNode(ParseState &state, std::shared_ptr<AstNode> e)
      : DebuggableNode(state), expr(e) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    return state.definitions[this];
  }
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *read,
                                     llvm::Value *header) {
    return state.definitionsIndex[this];
  }
  llvm::Value *generateAtDefinition(GenerateState &state,
                                    llvm::Value *read,
                                    llvm::Value *header) {
    auto result = expr->generate(state, read, header);
    state.definitions[this] = result;
    return result;
  }
  llvm::Value *generateIndexAtDefinition(GenerateState &state,
                                         llvm::Value *read,
                                         llvm::Value *header) {
    auto result = expr->generateIndex(state, read, header);
    state.definitionsIndex[this] = result;
    return result;
  }

private:
  std::shared_ptr<AstNode> expr;
};

class BindingNode : public DebuggableNode {
public:
  BindingNode(ParseState &state) : DebuggableNode(state) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    for (auto it = definitions.begin(); it != definitions.end(); it++) {
      (*it)->generateAtDefinition(state, read, header);
    }
    return body->generate(state, read, header);
  }
  virtual llvm::Value *generateIndex(GenerateState &state,
                                     llvm::Value *read,
                                     llvm::Value *header) {
    for (auto it = definitions.begin(); it != definitions.end(); it++) {
      (*it)->generateIndexAtDefinition(state, read, header);
    }
    return body->generateIndex(state, read, header);
  }

  void parse(ParseState &state, PredicateMap &predicates) throw(ParseError) {
    PredicateMap childPredicates(predicates);
    while (!state.empty() && (definitions.size() == 0 || *state == ',')) {
      if (definitions.size() > 0) {
        state.next();
      }
      state.parseSpace();
      auto name = state.parseStr(
          "ABCDEFGHIJLKLMNOPQRSTUVWXYZabcdefghijlklmnopqrstuvwxyz0123456789_");
      state.parseCharInSpace('=');
      auto use =
          std::make_shared<UseNode>(state, AstNode::parse(state, predicates));
      definitions.push_back(use);
      childPredicates[name] = [=](ParseState & state) throw(ParseError) {
        return std::static_pointer_cast<AstNode>(use);
      };
      state.parseSpace();
    }
    if (!state.parseKeyword("in")) {
      throw ParseError(state.where(), "Expected `in' or `,' in `let'.");
    }
    body = AstNode::parse(state, childPredicates);
  }

private:
  std::vector<std::shared_ptr<UseNode>> definitions;
  std::shared_ptr<AstNode> body;
};

/**
 * Handle terminal operators (final step in the recursive descent)
 */
static std::shared_ptr<AstNode> parseTerminal(
    ParseState &state, PredicateMap &predicates) throw(ParseError) {

  state.parseSpace();
  if (state.empty()) {
    throw ParseError(state.where(),
                     "Reached end of input before completing parsing.");
  }
  if (*state == '!') {
    state.next();
    return std::make_shared<NotNode>(parseTerminal(state, predicates));
  }
  if (*state == '(') {
    state.next();
    size_t brace_index = state.where();
    auto node = AstNode::parse(state, predicates);
    state.parseSpace();
    if (!state.empty() && *state == ')') {
      state.next();
      return node;
    } else {
      throw ParseError(brace_index,
                       "Open brace has no matching closing brace.");
    }
  }
  size_t start = state.where();
  while (!state.empty() && ((*state >= 'a' && *state <= 'z') ||
                            ((state.where() - start) > 0 &&
                             (*state == '_' || *state == '?' ||
                              (*state >= '0' && *state <= '9'))))) {
    state.next();
  }
  if (start == state.where()) {
    throw ParseError(state.where(), "Missing predicate.");
  }

  std::string predicate_name = state.strFrom(start);
  if (predicates.count(predicate_name)) {
    return predicates[predicate_name](state);
  } else {
    throw ParseError(start, "Unknown predicate.");
  }
}

/**
 * Parse the implication (->) operator.
 */
static std::shared_ptr<AstNode> parseImplication(
    ParseState &state, PredicateMap &predicates) throw(ParseError) {
  auto antecedent = parseTerminal(state, predicates);
  state.parseSpace();
  while (state.parseKeyword("->")) {
    auto assertion = parseTerminal(state, predicates);
    antecedent = std::make_shared<OrNode>(std::make_shared<NotNode>(antecedent),
                                          assertion);
    state.parseSpace();
  }
  return antecedent;
}

/**
 * Handle binary operators operators (the intermediate steps in recursive
 * descent)
 */
template <char S, class T, ParseFunc N>
static std::shared_ptr<AstNode> parseBinary(
    ParseState &state, PredicateMap &predicates) throw(ParseError) {
  std::vector<std::shared_ptr<AstNode>> items;

  std::shared_ptr<AstNode> node = N(state, predicates);
  state.parseSpace();
  while (!state.empty() && *state == S) {
    state.next();
    items.push_back(node);
    node = N(state, predicates);
    state.parseSpace();
  }
  while (items.size() > 0) {
    node = std::make_shared<T>(items.back(), node);
    items.pop_back();
  }
  return node;
}

static std::shared_ptr<AstNode> parseIntermediate(
    ParseState &state, PredicateMap &predicates) throw(ParseError) {
  return parseBinary<
      '|',
      OrNode,
      parseBinary<'^', XOrNode, parseBinary<'&', AndNode, parseImplication>>>(
      state, predicates);
}

/**
 * Handle conditional operators
 */
static std::shared_ptr<AstNode> parseConditional(
    ParseState &state, PredicateMap &predicates) throw(ParseError) {
  auto cond_part = parseIntermediate(state, predicates);
  state.parseSpace();
  if (!state.parseKeyword("then")) {
    return cond_part;
  }
  auto then_part = parseIntermediate(state, predicates);
  if (!state.parseKeyword("else")) {
    throw ParseError(state.where(), "Ternary operator has no else.");
  }
  auto else_part = parseIntermediate(state, predicates);
  return std::make_shared<ConditionalNode>(cond_part, then_part, else_part);
}

/**
 * Handle let operators (first step in the recursive descent)
 */
std::shared_ptr<AstNode> AstNode::parse(
    ParseState &state, PredicateMap &predicates) throw(ParseError) {
  state.parseSpace();
  if (state.parseKeyword("let")) {
    auto let = std::make_shared<BindingNode>(state);
    let->parse(state, predicates);
    return let;
  } else {
    return parseConditional(state, predicates);
  }
}

/**
 * Parse a string into a syntax tree using the built-in logical operations and
 * the predicates provided.
 */
std::shared_ptr<AstNode> AstNode::parse(
    const std::string &input, PredicateMap &predicates) throw(ParseError) {
  ParseState state(input);
  std::shared_ptr<AstNode> node = AstNode::parse(state, predicates);

  state.parseSpace();
  // check string is fully consumed
  if (!state.empty()) {
    throw ParseError(state.where(), "Junk at end of input.");
  }
  return node;
}

std::shared_ptr<AstNode> bamql::AstNode::parseWithLogging(
    const std::string &input, PredicateMap &predicates) {
  std::shared_ptr<AstNode> ast;
  try {
    ast = AstNode::parse(input, predicates);
  } catch (ParseError e) {
    std::cerr << "Error: " << e.what() << std::endl << input << std::endl;
    for (size_t i = 0; i < e.where(); i++) {
      std::cerr << " ";
    }
    std::cerr << "^" << std::endl;
  }
  return ast;
}
}
