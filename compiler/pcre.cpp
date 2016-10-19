#include <sstream>
#include <pcre.h>
#include "bamql-compiler.hpp"

static bamql::RegularExpression createPCRE(
    const std::string &input,
    bool caseless,
    size_t start,
    std::map<std::string, int> &names) throw(bamql::ParseError) {
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
    auto free_func = state.module()->getFunction("pcre_free");
    auto var = new llvm::GlobalVariable(*state.module(),
                                        base_str,
                                        false,
                                        llvm::GlobalVariable::PrivateLinkage,
                                        0,
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
    auto dtor_loaded = state.getGenerator().destructor()->CreateLoad(var);
    auto was_initialised = state.getGenerator().destructor()->CreateICmpEQ(
        dtor_loaded, null_value);
    auto free_block = llvm::BasicBlock::Create(
        state.module()->getContext(),
        "free",
        state.getGenerator().destructor()->GetInsertBlock()->getParent());
    auto merge_block = llvm::BasicBlock::Create(
        state.module()->getContext(),
        "merge",
        state.getGenerator().destructor()->GetInsertBlock()->getParent());
    state.getGenerator().destructor()->CreateCondBr(
        was_initialised, free_block, merge_block);
    state.getGenerator().destructor()->SetInsertPoint(free_block);
    llvm::Value *free_args[] = { dtor_loaded };
    state.getGenerator().destructor()->CreateCall(free_func, free_args);
    state.getGenerator().destructor()->CreateBr(merge_block);
    state.getGenerator().destructor()->SetInsertPoint(merge_block);

    return state->CreateLoad(var);
  };
}

bamql::RegularExpression bamql::ParseState::parseRegEx(
    std::map<std::string, int> &names) throw(bamql::ParseError) {
  auto start = index;
  index++;
  while (index < input.length() && input[index] != input[start]) {
    index++;
  }
  if (index == input.length()) {
    throw bamql::ParseError(start, "Unterminated regular expression.");
  }
  auto stop = index++;

  return createPCRE(input.substr(start + 1, stop - start - 1),
                    parseKeyword("i"),
                    start,
                    names);
}

bamql::RegularExpression bamql::ParseState::parseRegEx() throw(
    bamql::ParseError) {
  std::map<std::string, int> names;
  auto result = parseRegEx(names);
  if (names.size() > 0) {
    throw ParseError(where(), "Named subpatterns are not permitted.");
  }
  return result;
}

bamql::RegularExpression bamql::globToRegEx(
    const std::string &prefix,
    const std::string &glob_str,
    const std::string &suffix) throw(bamql::ParseError) {
  std::map<std::string, int> names;

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

  return createPCRE(regex.str().c_str(), true, 0, names);
}

bamql::RegularExpression bamql::setToRegEx(
    const std::string &prefix,
    const std::set<std::string> &names,
    const std::string &suffix) throw(bamql::ParseError) {
  std::map<std::string, int> captures;
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
  return createPCRE(all.str().c_str(), true, 0, captures);
}
