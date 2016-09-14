#include <sstream>
#include <pcre.h>
#include "bamql-compiler.hpp"

static bamql::RegularExpression createPCRE(
    const char *input,
    bool caseless,
    size_t start = 0) throw(bamql::ParseError) {
  int erroroffset = 0;
  const char *error = nullptr;
  std::shared_ptr<pcre> regex(
      pcre_compile(input,
                   PCRE_NO_AUTO_CAPTURE | (caseless ? PCRE_CASELESS : 0),
                   &error,
                   &erroroffset,
                   nullptr),
      pcre_free);
  if (!regex) {
    throw bamql::ParseError(start + erroroffset, std::string(error));
  }
  size_t size = 0;
  int info_rc = pcre_fullinfo(regex.get(), nullptr, PCRE_INFO_SIZE, &size);
  if (info_rc != 0 || size == 0) {
    throw bamql::ParseError(start, "Internal PCRE error.");
  }

  return [regex, size](bamql::GenerateState &generate) {
    auto array = llvm::ConstantDataArray::get(
        generate.module()->getContext(),
        llvm::ArrayRef<uint8_t>((uint8_t *)regex.get(), size));
    auto global_variable = new llvm::GlobalVariable(
        *generate.module(),
        llvm::ArrayType::get(
            llvm::Type::getInt8Ty(generate.module()->getContext()), size),
        true,
        llvm::GlobalValue::PrivateLinkage,
        0,
        ".regex");
    global_variable->setAlignment(alignof(long));
    global_variable->setInitializer(array);
    auto zero = llvm::ConstantInt::get(
        llvm::Type::getInt8Ty(generate.module()->getContext()), 0);
    std::vector<llvm::Value *> indicies;
    indicies.push_back(zero);
    indicies.push_back(zero);
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
    return llvm::ConstantExpr::getGetElementPtr(global_variable, indicies);
#else
    return llvm::ConstantExpr::getGetElementPtr(
        global_variable->getValueType(), global_variable, indicies);
#endif
  };
}

bamql::RegularExpression bamql::ParseState::parseRegEx() throw(
    bamql::ParseError) {
  auto start = index;
  index++;
  while (index < input.length() && input[index] != input[start]) {
    index++;
  }
  if (index == input.length()) {
    throw bamql::ParseError(start, "Unterminated regular expression.");
  }
  index++;

  return createPCRE(
      input.substr(start + 1, index - start - 2).c_str(), false, start);
}

bamql::RegularExpression bamql::globToRegEx(
    const std::string &prefix,
    const std::string &glob_str,
    const std::string &suffix) throw(bamql::ParseError) {
  std::stringstream regex;
  regex << prefix;
  for (auto glob_char = glob_str.begin(); glob_char != glob_str.end();
       glob_char++) {
    switch (*glob_char) {
    case '*':
      regex << ".*";
      break;
    case '?':
      regex << ".?";
      break;
    case '.':
      regex << "\\.";
      break;
    default:
      regex << *glob_char;
      break;
    }
  }
  regex << suffix;

  return createPCRE(regex.str().c_str(), true);
}

bamql::RegularExpression bamql::setToRegEx(
    const std::string &prefix,
    const std::set<std::string> &names,
    const std::string &suffix) throw(bamql::ParseError) {
  std::stringstream all;
  all << prefix << "(";
  bool first = true;
  for (auto name = names.begin(); name != names.end(); name++) {
    if (first) {
      first = false;
    } else {
      all << "|";
    }
    all << *name;
  }
  all << ")" << suffix;
  return createPCRE(all.str().c_str(), true);
}
