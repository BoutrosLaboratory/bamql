#include <pcre.h>
#include "bamql.hpp"

bamql::RegularExpression bamql::ParseState::parseRegEx() throw(ParseError) {
  auto start = index;
  index++;
  while (index < input.length() && input[index] != input[start]) {
    index++;
  }
  if (index == input.length()) {
    throw ParseError(start, "Unterminated regular expression.");
  }
  index++;

  int erroroffset;
  const char *error;
  std::shared_ptr<pcre> regex(
      pcre_compile(input.substr(start + 1, index - start - 2).c_str(),
                   PCRE_NO_AUTO_CAPTURE,
                   &error,
                   &erroroffset,
                   nullptr),
      pcre_free);
  if (!regex) {
    throw ParseError(start + erroroffset, std::string(error));
  }
  size_t size = 0;
  int info_rc = pcre_fullinfo(regex.get(), nullptr, PCRE_INFO_SIZE, &size);
  if (info_rc != 0 || size == 0) {
    throw ParseError(start, "Internal PCRE error.");
  }

  return [regex, size](GenerateState &generate) {
    int erroroffset;
    const char *error = nullptr;
    auto array = llvm::ConstantDataArray::get(
        llvm::getGlobalContext(),
        llvm::ArrayRef<uint8_t>((uint8_t *)regex.get(), size));
    auto global_variable = new llvm::GlobalVariable(
        *generate.module(),
        llvm::ArrayType::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                             size),
        true,
        llvm::GlobalValue::PrivateLinkage,
        0,
        ".regex");
    global_variable->setAlignment(alignof(long));
    global_variable->setInitializer(array);
    auto zero = llvm::ConstantInt::get(
        llvm::Type::getInt8Ty(llvm::getGlobalContext()), 0);
    std::vector<llvm::Value *> indicies;
    indicies.push_back(zero);
    indicies.push_back(zero);
    return llvm::ConstantExpr::getGetElementPtr(global_variable, indicies);
  };
}
