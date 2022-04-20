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

#include "ast_node_regex.hpp"
#include "bamql-compiler.hpp"
#include <pcre.h>
#include <sstream>

static bamql::RegularExpression createPCRE(const std::string &input,
                                           bool caseless,
                                           size_t start,
                                           std::map<std::string, int> &names) {
  int erroroffset = 0;
  int flags = PCRE_NO_AUTO_CAPTURE | (caseless ? PCRE_CASELESS : 0);
  const char *error = nullptr;
  std::shared_ptr<pcre> regex(
      pcre_compile(input.c_str(), flags, &error, &erroroffset, nullptr),
      pcre_free);
  if (!regex) {
    throw bamql::ParseError(start + erroroffset, std::string(error));
  }
  size_t size = 0;
  int info_rc = pcre_fullinfo(regex.get(), nullptr, PCRE_INFO_SIZE, &size);
  if (info_rc != 0 || size == 0) {
    throw bamql::ParseError(start, "Internal PCRE error.");
  }

  int name_count, name_size;
  char *name_table;
  if (pcre_fullinfo(regex.get(), nullptr, PCRE_INFO_NAMECOUNT, &name_count) <
          0 ||
      pcre_fullinfo(regex.get(), nullptr, PCRE_INFO_NAMEENTRYSIZE, &name_size) <
          0 ||
      pcre_fullinfo(regex.get(), nullptr, PCRE_INFO_NAMETABLE, &name_table) <
          0) {
    throw bamql::ParseError(start, "Internal PCRE error.");
  }
  for (auto it = 0; it < name_count; it++, name_table += name_size) {
    names[name_table + 2] = (name_table[0] << 8) + name_table[1];
  }

  return [=](bamql::GenerateState &state) {
    auto base_int32 = llvm::Type::getInt32Ty(state.module()->getContext());
    auto base_str = llvm::PointerType::get(
        llvm::Type::getInt8Ty(state.module()->getContext()), 0);
    auto null_value = llvm::ConstantPointerNull::get(base_str);
    auto compile_func = state.module()->getFunction("bamql_re_compile");
    auto free_func = state.module()->getFunction("bamql_re_free");
    auto var = new llvm::GlobalVariable(*state.module(), base_str, false,
                                        llvm::GlobalVariable::PrivateLinkage, 0,
                                        ".regex");
    var->setInitializer(null_value);
    llvm::Value *construct_args[] = { state.getGenerator().createString(input),
                                      llvm::ConstantInt::get(base_int32, flags),
                                      llvm::ConstantInt::get(base_int32,
                                                             name_count) };
    state.getGenerator().constructor()->CreateStore(
        state.getGenerator().constructor()->CreateCall(compile_func,
                                                       construct_args),
        var);
    llvm::Value *free_args[] = { var };
    state.getGenerator().destructor()->CreateCall(free_func, free_args);

    return state->CreateLoad(base_str, var);
  };
}

namespace bamql {

RegularExpression ParseState::parseRegEx(std::map<std::string, int> &names) {
  auto start = index;
  index++;
  while (index < input.length() && input[index] != input[start]) {
    index++;
  }
  if (index == input.length()) {
    throw ParseError(start, "Unterminated regular expression.");
  }
  auto stop = index++;

  return createPCRE(input.substr(start + 1, stop - start - 1),
                    parseKeyword("i"), start, names);
}

RegularExpression ParseState::parseRegEx() {
  std::map<std::string, int> names;
  auto result = parseRegEx(names);
  if (names.size() > 0) {
    throw ParseError(where(), "Named subpatterns are not permitted.");
  }
  return result;
}

RegularExpression globToRegEx(const std::string &prefix,
                              const std::string &glob_str,
                              const std::string &suffix) {
  std::map<std::string, int> names;

  std::stringstream regex;
  regex << prefix;
  for (auto &glob_char : glob_str) {
    switch (glob_char) {
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
      regex << glob_char;
      break;
    }
  }
  regex << suffix;

  return createPCRE(regex.str().c_str(), true, 0, names);
}

RegularExpression setToRegEx(const std::string &prefix,
                             const std::set<std::string> &names,
                             const std::string &suffix) {
  std::map<std::string, int> captures;
  std::stringstream all;
  all << prefix << "(";
  bool first = true;
  for (auto &name : names) {
    if (first) {
      first = false;
    } else {
      all << "|";
    }
    all << name;
  }
  all << ")" << suffix;
  return createPCRE(all.str().c_str(), true, 0, captures);
}
} // namespace bamql
