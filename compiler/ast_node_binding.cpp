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
#include <sstream>
#include "bamql-compiler.hpp"
#include "compiler.hpp"

class bamql::UseNode : public bamql::DebuggableNode {
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
  bamql::ExprType type() { return expr->type(); }

private:
  std::shared_ptr<AstNode> expr;
};

bamql::BindingNode::BindingNode(ParseState &state) : DebuggableNode(state) {}
llvm::Value *bamql::BindingNode::generate(GenerateState &state,
                                          llvm::Value *read,
                                          llvm::Value *header,
                                          llvm::Value *error_fn,
                                          llvm::Value *error_ctx) {
  for (auto it = definitions.begin(); it != definitions.end(); it++) {
    (*it)->writeDebug(state);
    (*it)->generateAtDefinition(state, read, header, error_fn, error_ctx);
  }
  body->writeDebug(state);
  return body->generate(state, read, header, error_fn, error_ctx);
}
llvm::Value *bamql::BindingNode::generateIndex(GenerateState &state,
                                               llvm::Value *read,
                                               llvm::Value *header,
                                               llvm::Value *error_fn,
                                               llvm::Value *error_ctx) {
  for (auto it = definitions.begin(); it != definitions.end(); it++) {
    (*it)->writeDebug(state);
    (*it)->generateIndexAtDefinition(state, read, header, error_fn, error_ctx);
  }
  body->writeDebug(state);
  return body->generateIndex(state, read, header, error_fn, error_ctx);
}

void bamql::BindingNode::parse(ParseState &state) throw(ParseError) {
  PredicateMap childPredicates;
  while (!state.empty() && (definitions.size() == 0 || *state == ',')) {
    if (definitions.size() > 0) {
      state.next();
    }
    if (!state.parseSpace()) {
      throw ParseError(state.where(), "Expected space.");
    }
    auto name = state.parseStr(
        "ABCDEFGHIJLKLMNOPQRSTUVWXYZabcdefghijlklmnopqrstuvwxyz0123456789_");
    state.parseCharInSpace('=');
    auto use = std::make_shared<UseNode>(state, AstNode::parse(state));
    definitions.push_back(use);
    childPredicates[name] = [=](ParseState & state) throw(ParseError) {
      return std::static_pointer_cast<AstNode>(use);
    };
    state.parseSpace();
  }
  if (!state.parseKeyword("in")) {
    throw ParseError(state.where(), "Expected `in' or `,' in `let'.");
  }
  state.push(childPredicates);
  body = AstNode::parse(state);
  state.pop(childPredicates);
}

bamql::ExprType bamql::BindingNode::type() { return body->type(); }

class bamql::BoundMatchNode : public bamql::DebuggableNode {
public:
  BoundMatchNode(bamql::ParseState &state,
                 int number_,
                 bamql::ExprType type_,
                 int decode_,
                 const std::string &error_)
      : DebuggableNode(state), decode(decode_), error(error_), exprType(type_),
        number(number_) {}
  llvm::Value *generate(bamql::GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return state->CreateLoad(state.definitions[this]);
  }
  llvm::Value *generateIndex(bamql::GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    return llvm::ConstantInt::getTrue(state.module()->getContext());
  }
  void prepare(bamql::GenerateState &state,
               std::vector<llvm::Value *> &arg_values) {
    auto base_int32 = llvm::Type::getInt32Ty(state.module()->getContext());
    arg_values.push_back(llvm::ConstantInt::get(base_int32, number));
    arg_values.push_back(state.createString(error));
    arg_values.push_back(llvm::ConstantInt::get(base_int32, decode));
    llvm::Type *boxType = nullptr;
    switch (exprType) {
    case bamql::STR:
      boxType = llvm::PointerType::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), 0);
      break;
    case bamql::FP:
      boxType = llvm::Type::getDoubleTy(state.module()->getContext());
      break;
    case bamql::INT:
      boxType = llvm::Type::getInt32Ty(state.module()->getContext());
      break;
    default:
      std::cerr << "Invalid box type: " << exprType << std::endl;
      abort();
    }

    auto box = state->CreateAlloca(boxType);
    state.definitions[this] = box;
    arg_values.push_back(box);
  }

  void cleanup(bamql::GenerateState &state) {
    std::vector<llvm::Value *> arg_values;
    if (exprType == bamql::STR) {
      auto function = state.module()->getFunction("pcre_free_substring");
      arg_values.push_back(state->CreateLoad(state.definitions[this]));
      state->CreateCall(function, arg_values);
    }
  }
  bamql::ExprType type() { return exprType; }

private:
  int decode;
  std::string error;
  bamql::ExprType exprType;
  int number;
};

static bool endsWith(const std::string &str, const std::string &tail) {
  if (str.length() > tail.length()) {
    return str.compare(str.length() - tail.length(), tail.length(), tail) == 0;
  } else {
    return false;
  }
}
bamql::MatchBindingNode::MatchBindingNode(ParseState &state) throw(ParseError)
    : DebuggableNode(state) {
  input = AstNode::parse(state);
  if (input->type() != bamql::STR) {
    throw ParseError(state.where(),
                     "Regular expression may only be used on strings.");
  }

  state.parseSpace();
  if (!state.parseKeyword("using")) {
    throw ParseError(state.where(), "Expected `using' in `bind'.");
  }
  state.parseSpace();

  std::map<std::string, int> names;
  regex = state.parseRegEx(names);

  PredicateMap childPredicates;
  for (auto it = names.begin(); it != names.end(); it++) {
    std::stringstream message;
    message << it->first << " is not matched.";
    std::string errorMessage = state.createRuntimeError(message.str());

    ExprType type = bamql::STR;
    int decode = 0;
    if (endsWith(it->first, "_d")) {
      decode = 1;
      type = bamql::FP;
    } else if (endsWith(it->first, "_i")) {
      decode = 2;
      type = bamql::INT;
    } else if (endsWith(it->first, "_c")) {
      decode = 3;
      type = bamql::INT;
    }
    auto use = std::make_shared<BoundMatchNode>(
        state, it->second, type, decode, errorMessage);
    definitions.push_back(use);
    childPredicates[it->first] = [=](ParseState & state) throw(ParseError) {
      return std::static_pointer_cast<AstNode>(use);
    };
    state.parseSpace();
  }

  if (!state.parseKeyword("in")) {
    throw ParseError(state.where(), "Expected `in' in `bind'.");
  }
  state.push(childPredicates);
  body = AstNode::parse(state);
  state.pop(childPredicates);
  if (body->type() != bamql::BOOL) {
    throw ParseError(state.where(), "Expression must be Boolean.");
  }
}

llvm::Value *bamql::MatchBindingNode::generate(bamql::GenerateState &state,
                                               llvm::Value *read,
                                               llvm::Value *header,
                                               llvm::Value *error_fn,
                                               llvm::Value *error_ctx) {
  auto input_value = input->generate(state, read, header, error_fn, error_ctx);
  auto function = state.module()->getFunction("bamql_re_bind");
  std::vector<llvm::Value *> arg_values;
  arg_values.push_back(regex(state));
  arg_values.push_back(

      llvm::ConstantInt::get(
          llvm::Type::getInt32Ty(state.module()->getContext()),
          definitions.size()));
  arg_values.push_back(error_fn);
  arg_values.push_back(error_ctx);
  arg_values.push_back(input_value);

  for (auto it = definitions.begin(); it != definitions.end(); it++) {
    (*it)->prepare(state, arg_values);
  }
  auto matched = state->CreateCall(function, arg_values);

  auto original_block = state->GetInsertBlock();
  auto target_function = state->GetInsertBlock()->getParent();
  auto match_block = llvm::BasicBlock::Create(
      state.module()->getContext(), "match", target_function);
  auto merge_block = llvm::BasicBlock::Create(
      state.module()->getContext(), "merge", target_function);

  state->CreateCondBr(matched, match_block, merge_block);

  state->SetInsertPoint(match_block);
  body->writeDebug(state);
  auto body_result = body->generate(state, read, header, error_fn, error_ctx);
  for (auto it = definitions.begin(); it != definitions.end(); it++) {
    (*it)->cleanup(state);
  }
  state->CreateBr(merge_block);

  state->SetInsertPoint(merge_block);
  auto phi =
      state->CreatePHI(llvm::Type::getInt1Ty(state.module()->getContext()), 2);
  phi->addIncoming(matched, original_block);
  phi->addIncoming(body_result, match_block);
  return phi;
}
llvm::Value *bamql::MatchBindingNode::generateIndex(bamql::GenerateState &state,
                                                    llvm::Value *read,
                                                    llvm::Value *header,
                                                    llvm::Value *error_fn,
                                                    llvm::Value *error_ctx) {
  return llvm::ConstantInt::getTrue(state.module()->getContext());
}

bamql::ExprType bamql::MatchBindingNode::type() { return bamql::BOOL; }
