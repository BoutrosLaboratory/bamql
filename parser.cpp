#include <iostream>
#include "barf.hpp"

namespace barf {

static std::shared_ptr<AstNode> parseConditional(const std::string &input,
                                                 size_t &index,
                                                 PredicateMap predicates);

/**
 * Handle terminal operators (final step in the recursive descent)
 */
static std::shared_ptr<AstNode> parseTerminal(const std::string &input,
                                              size_t &index,
                                              PredicateMap predicates) {

  parseSpace(input, index);
  if (index >= input.length()) {
    throw ParseError(index, "Reached end of input before completing parsing.");
  }
  if (input[index] == '!') {
    index++;
    return std::make_shared<NotNode>(parseTerminal(input, index, predicates));
  }
  if (input[index] == '(') {
    index++;
    size_t brace_index = index;
    auto node = parseConditional(input, index, predicates);
    parseSpace(input, index);
    if (index < input.length() && input[index] == ')') {
      index++;
      return node;
    } else {
      throw ParseError(brace_index,
                       "Open brace has no matching closing brace.");
    }
  }
  size_t start = index;
  while (
      index < input.length() &&
      (input[index] >= 'a' && input[index] <= 'z' ||
       ((index - start) > 0) && (input[index] == '_' || input[index] == '?' ||
                                 input[index] >= '0' && input[index] <= '9'))) {
    index++;
  }
  if (start == index) {
    throw ParseError(index, "Empty predicate.");
  }

  std::string predicate_name = input.substr(start, index - start);
  if (predicates.count(predicate_name)) {
    return predicates[predicate_name](input, index);
  } else {
    index = start;
    throw ParseError(index, "Unknown predicate.");
  }
}

/**
 * Handle and operators (third step in the recursive descent)
 */
static std::shared_ptr<AstNode> parseAnd(const std::string &input,
                                         size_t &index,
                                         PredicateMap predicates) {
  std::vector<std::shared_ptr<AstNode>> items;

  std::shared_ptr<AstNode> node = parseTerminal(input, index, predicates);
  parseSpace(input, index);
  while (index < input.length() && input[index] == '&') {
    index++;
    items.push_back(node);
    node = parseTerminal(input, index, predicates);
    parseSpace(input, index);
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
static std::shared_ptr<AstNode> parseOr(const std::string &input,
                                        size_t &index,
                                        PredicateMap predicates) {
  std::vector<std::shared_ptr<AstNode>> items;

  std::shared_ptr<AstNode> node = parseAnd(input, index, predicates);
  parseSpace(input, index);
  while (index < input.length() && input[index] == '|') {
    index++;
    items.push_back(node);
    node = parseAnd(input, index, predicates);
    parseSpace(input, index);
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
static std::shared_ptr<AstNode> parseConditional(const std::string &input,
                                                 size_t &index,
                                                 PredicateMap predicates) {
  auto cond_part = parseOr(input, index, predicates);
  parseSpace(input, index);
  if (!parseKeyword(input, index, "then")) {
    return cond_part;
  }
  auto then_part = parseOr(input, index, predicates);
  if (!parseKeyword(input, index, "else")) {
    throw ParseError(index, "Ternary operator has no else.");
  }
  auto else_part = parseOr(input, index, predicates);
  return std::make_shared<ConditionalNode>(cond_part, then_part, else_part);
}

/**
 * Parse a string into a syntax tree using the built-in logical operations and
 * the predicates provided.
 */
std::shared_ptr<AstNode> AstNode::parse(
    const std::string &input, PredicateMap predicates) throw(ParseError) {
  size_t index = 0;
  std::shared_ptr<AstNode> node = parseConditional(input, index, predicates);

  parseSpace(input, index);
  // check string is fully consumed
  if (index != input.length()) {
    throw ParseError(index, "Junk at end of input.");
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
