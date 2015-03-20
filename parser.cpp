#include <iostream>
#include "barf.hpp"

namespace barf {

/**
 * Handle terminal operators (final step in the recursive descent)
 */
static std::shared_ptr<AstNode> parseTerminal(
    ParseState &state, PredicateMap predicates) throw(ParseError) {

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
  while (!state.empty() &&
         (*state >= 'a' && *state <= 'z' ||
          ((state.where() - start) > 0) && (*state == '_' || *state == '?' ||
                                            *state >= '0' && *state <= '9'))) {
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
 * Handle and operators (third step in the recursive descent)
 */
static std::shared_ptr<AstNode> parseAnd(
    ParseState &state, PredicateMap predicates) throw(ParseError) {
  std::vector<std::shared_ptr<AstNode>> items;

  std::shared_ptr<AstNode> node = parseTerminal(state, predicates);
  state.parseSpace();
  while (!state.empty() && *state == '&') {
    state.next();
    items.push_back(node);
    node = parseTerminal(state, predicates);
    state.parseSpace();
  }
  while (items.size() > 0) {
    node = std::make_shared<AndNode>(items.back(), node);
    items.pop_back();
  }
  return node;
}

/**
 * Handle or operators (second step in the recursive descent)
 */
static std::shared_ptr<AstNode> parseOr(
    ParseState &state, PredicateMap predicates) throw(ParseError) {
  std::vector<std::shared_ptr<AstNode>> items;

  std::shared_ptr<AstNode> node = parseAnd(state, predicates);
  state.parseSpace();
  while (!state.empty() && *state == '|') {
    state.next();
    items.push_back(node);
    node = parseAnd(state, predicates);
    state.parseSpace();
  }
  while (items.size() > 0) {
    node = std::make_shared<OrNode>(items.back(), node);
    items.pop_back();
  }
  return node;
}

/**
 * Handle conditional operators (first step in the recursive descent)
 */
std::shared_ptr<AstNode> AstNode::parse(
    ParseState &state, PredicateMap predicates) throw(ParseError) {
  auto cond_part = parseOr(state, predicates);
  state.parseSpace();
  if (!state.parseKeyword("then")) {
    return cond_part;
  }
  auto then_part = parseOr(state, predicates);
  if (!state.parseKeyword("else")) {
    throw ParseError(state.where(), "Ternary operator has no else.");
  }
  auto else_part = parseOr(state, predicates);
  return std::make_shared<ConditionalNode>(cond_part, then_part, else_part);
}

/**
 * Parse a string into a syntax tree using the built-in logical operations and
 * the predicates provided.
 */
std::shared_ptr<AstNode> AstNode::parse(
    const std::string &input, PredicateMap predicates) throw(ParseError) {
  ParseState state(input);
  std::shared_ptr<AstNode> node = AstNode::parse(state, predicates);

  state.parseSpace();
  // check string is fully consumed
  if (!state.empty()) {
    throw ParseError(state.where(), "Junk at end of input.");
  }
  return node;
}

std::shared_ptr<AstNode> barf::AstNode::parseWithLogging(
    const std::string &input, PredicateMap predicates) {
  std::shared_ptr<AstNode> ast;
  try {
    ast = AstNode::parse(input, predicates);
  }
  catch (ParseError e) {
    std::cerr << "Error: " << e.what() << std::endl << input << std::endl;
    for (auto i = 0; i < e.where(); i++) {
      std::cerr << " ";
    }
    std::cerr << "^" << std::endl;
  }
  return ast;
}
}
