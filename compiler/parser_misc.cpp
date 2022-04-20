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

#include "ast_node_literal.hpp"
#include "bamql-compiler.hpp"
#include <cctype>
#include <sstream>

namespace bamql {

ParseError::ParseError(size_t index, std::string what)
    : std::runtime_error(what) {
  this->index = index;
}
size_t ParseError::where() { return index; }

ParseState::ParseState(const std::string &input_)
    : input(input_), index(0), line(1), column(1) {}
unsigned int ParseState::currentLine() const { return line; }
unsigned int ParseState::currentColumn() const { return column; }

std::string ParseState::createRuntimeError(const std::string &message) {
  std::stringstream output;
  output << line << ":" << column << ": " << message;
  return output.str();
}
bool ParseState::empty() const { return index >= input.length(); }
void ParseState::next() {
  index++;
  column++;
  if (index >= input.length() && input[index] == '\n') {
    column = 1;
    line++;
  }
}

int ParseState::parseInt() {
  size_t start = index;
  int accumulator = 0;
  while (index < input.length() && input[index] >= '0' && input[index] <= '9') {
    accumulator = 10 * accumulator + (input[index] - '0');
    index++;
  }
  if (start == index) {
    throw ParseError(start, "Expected integer.");
  }
  return accumulator;
}
std::shared_ptr<AstNode> ParseState::parseLiteral() {
  if (this->empty()) {
    return nullptr;
  }

  if (input[index] == '\'') {
    next();
    if (this->empty()) {
      throw ParseError(where(), "Expected character.");
    }
    int c = input[index];
    next();
    return std::make_shared<
        LiteralNode<int, decltype(&make_int), &make_int, INT>>(c);
  }

  if (!isdigit(input[index]) && input[index] != '-') {
    return nullptr;
  }

  size_t start = index;
  char *dbl_end_ptr = nullptr;
  char *int_end_ptr = nullptr;
  auto dbl_value = strtod(input.c_str() + start, &dbl_end_ptr);
  auto int_value = strtol(input.c_str() + start, &int_end_ptr, 10);
  auto dbl_length = dbl_end_ptr - (input.c_str() + start);
  auto int_length = int_end_ptr - (input.c_str() + start);
  if (dbl_length == 0 && int_length == 0) {
    return nullptr;
  }
  if (dbl_length > int_length) {
    index += dbl_length;
    column += dbl_length;
    return std::make_shared<
        LiteralNode<double, decltype(&make_dbl), &make_dbl, FP>>(dbl_value);
  }
  index += int_length;
  column += int_length;
  return std::make_shared<
      LiteralNode<int, decltype(&make_int), &make_int, INT>>(int_value);
}
std::shared_ptr<AstNode> ParseState::parsePredicate() {
  size_t start = where();
  while (!empty() &&
         ((**this >= 'a' && **this <= 'z') ||
          ((where() - start) > 0 && (**this == '_' || **this == '?' ||
                                     (**this >= 'A' && **this <= 'Z') ||
                                     (**this >= '0' && **this <= '9'))))) {
    next();
  }
  if (start == where()) {
    throw ParseError(where(), "Missing predicate.");
  }

  std::string name = strFrom(start);
  for (auto it = predicates.rbegin(); it != predicates.rend(); it++) {
    if (it->get().count(name)) {
      return it->get().at(name)(*this);
    }
  }

  std::stringstream errmsg;
  errmsg << "Unknown predicate: " << name;
  throw ParseError(start, errmsg.str());
}

double ParseState::parseDouble() {
  size_t start = index;
  char *end_ptr = nullptr;
  auto result = strtod(input.c_str() + start, &end_ptr);
  index = end_ptr - input.c_str();
  if (index == start) {
    throw ParseError(index, "Expected floating point number.");
  }
  return result;
}
std::string ParseState::parseStr(const std::string &accept_chars, bool reject) {
  size_t start = index;
  while (index < input.length() &&
         ((accept_chars.find(input[index]) != std::string::npos) ^ reject)) {
    index++;
  }
  if (start == index) {
    throw ParseError(start, "Unexpected character.");
  }
  return input.substr(start, index - start);
}
bool ParseState::parseSpace() {
  size_t start = index;
  bool again;
  do {
    again = false;
    while (index < input.length() && isspace(input[index])) {
      index++;
    }
    if (index < input.length() && input[index] == '#') {
      while (index < input.length() &&
             (input[index] != '\n' && input[index] != '\r')) {
        index++;
      }
      again = true;
    }
  } while (again);
  return start != index;
}
void ParseState::parseCharInSpace(char c) {
  parseSpace();
  if (index >= input.length() || input[index] != c) {
    std::ostringstream err_text;
    err_text << "Expected `" << c << "'.";
    throw ParseError(index, err_text.str());
  }
  index++;
  parseSpace();
}
bool ParseState::parseKeyword(const std::string &keyword) {
  for (size_t it = 0; it < keyword.length(); it++) {
    if (index + it >= input.length() || input[index + it] != keyword[it]) {
      return false;
    }
  }
  if (index + keyword.length() == input.length() ||
      !isalnum(input[index + keyword.length()])) {
    index += keyword.length();
    return true;
  }
  return false;
}

const unsigned int degen_nt[32] = {
  /*A*/ 1,
  /*B*/ 2 | 4 | 8,
  /*C*/ 2,
  /*D*/ 1 | 4 | 8,
  /*E*/ 0,
  /*F*/ 0,
  /*G*/ 4,
  /*H*/ 1 | 2 | 8,
  /*I*/ 0,
  /*J*/ 0,
  /*K*/ 4 | 8,
  /*L*/ 0,
  /*M*/ 1 | 2,
  /*N*/ 1 | 2 | 4 | 8,
  /*O*/ 0,
  /*P*/ 0,
  /*Q*/ 0,
  /*R*/ 1 | 4,
  /*S*/ 2 | 4,
  /*T*/ 8,
  /*U*/ 8,
  /*V*/ 1 | 2 | 4,
  /*W*/ 1 | 8,
  /*X*/ 1 | 2 | 4 | 8,
  /*Y*/ 2 | 8,
  /*Z*/ 0
};
unsigned char ParseState::parseNucleotide() {
  auto c = tolower(input[index++]);
  if (c >= 'a' && c <= 'z') {
    return degen_nt[c - 'a'];
  }
  return 0;
}
std::string ParseState::strFrom(size_t start) const {
  return input.substr(start, index - start);
}

size_t ParseState::where() const { return index; }
char ParseState::operator*() const { return input[index]; }

void ParseState::push(const PredicateMap &map) { predicates.push_back(map); }
void ParseState::pop(const PredicateMap &map) {
  assert(&predicates.back().get() == &map);
  predicates.pop_back();
}
} // namespace bamql
