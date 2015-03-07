#include <cctype>
#include <sstream>
#include "barf.hpp"

namespace barf {
ParseError::ParseError(size_t index, std::string what)
    : std::runtime_error(what) {
  this->index = index;
}
size_t ParseError::where() { return index; }

int parseInt(const std::string &input, size_t &index) throw(ParseError) {
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
double parseDouble(const std::string &input, size_t &index) throw(ParseError) {
  size_t start = index;
  char *end_ptr = nullptr;
  auto result = strtod(input.c_str() + start, &end_ptr);
  index = end_ptr - input.c_str();
  if (index == start) {
    throw ParseError(index, "Expected floating point number.");
  }
  return result;
}
std::string parseStr(const std::string &input,
                     size_t &index,
                     const std::string &accept_chars,
                     bool reject) throw(ParseError) {
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
bool parseSpace(const std::string &input, size_t &index) {
  size_t start = index;
  while (index < input.length() &&
         (input[index] == ' ' || input[index] == '\t' || input[index] == '\n' ||
          input[index] == '\r')) {
    index++;
  }
  return start != index;
}
void parseCharInSpace(const std::string &input,
                      size_t &index,
                      char c) throw(ParseError) {
  parseSpace(input, index);
  if (index >= input.length() || input[index] != c) {
    std::ostringstream err_text;
    err_text << "Expected `" << c << "'.";
    throw ParseError(index, err_text.str());
  }
  index++;
  parseSpace(input, index);
}
bool parseKeyword(const std::string &input,
                  size_t &index,
                  const std::string &keyword) {
  for (size_t it = 0; it < keyword.length(); it++) {
    if (index + it >= input.length() || input[index + it] != keyword[it]) {
      return false;
    }
  }
  index += keyword.length();
  return true;
}

unsigned int degen_nt[32] = {
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
unsigned char parseNucleotide(const std::string &input, size_t &index) {
  auto c = tolower(input[index++]);
  if (c >= 'a' && c <= 'z') {
    return degen_nt[c - 'a'];
  }
  return 0;
}
}
