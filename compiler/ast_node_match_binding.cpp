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

#include <cassert>
#include <iostream>
#include <sstream>
#include "bamql-compiler.hpp"

static bool endsWith(const std::string &str, const std::string &tail) {
  if (str.length() > tail.length()) {
    return str.compare(str.length() - tail.length(), tail.length(), tail) == 0;
  } else {
    return false;
  }
}

namespace bamql {

class BoundMatchNode final : public DebuggableNode {
public:
  BoundMatchNode(ParseState &state,
                 int number_,
                 ExprType type_,
                 int decode_,
                 const std::string &error_)
      : DebuggableNode(state), decode(decode_), error(error_), exprType(type_),
        number(number_) {}
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    return state->CreateLoad(state.definitions[this]);
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    return llvm::ConstantInt::getTrue(state.module()->getContext());
  }
  void prepare(GenerateState &state, std::vector<llvm::Value *> &arg_values) {
    auto base_int32 = llvm::Type::getInt32Ty(state.module()->getContext());
    arg_values.push_back(llvm::ConstantInt::get(base_int32, number));
    arg_values.push_back(state.createString(error));
    arg_values.push_back(llvm::ConstantInt::get(base_int32, decode));
    llvm::Type *boxType = nullptr;
    switch (exprType) {
    case STR:
      boxType = llvm::PointerType::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), 0);
      break;
    case FP:
      boxType = llvm::Type::getDoubleTy(state.module()->getContext());
      break;
    case INT:
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

  void cleanup(GenerateState &state) {
    std::vector<llvm::Value *> arg_values;
    if (exprType == STR) {
      auto function = state.module()->getFunction("pcre_free_substring");
      arg_values.push_back(state->CreateLoad(state.definitions[this]));
      state->CreateCall(function, arg_values);
    }
  }
  ExprType type() { return exprType; }

private:
  int decode;
  std::string error;
  ExprType exprType;
  int number;
};

class MatchBindingNode final : public DebuggableNode {
public:
  MatchBindingNode(ParseState &state) throw(ParseError)
      : DebuggableNode(state) {
    input = AstNode::parse(state);
    if (input->type() != STR) {
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
    for (auto &name : names) {
      std::stringstream message;
      message << name.first << " is not matched.";
      std::string errorMessage = state.createRuntimeError(message.str());

      ExprType type = STR;
      int decode = 0;
      if (endsWith(name.first, "_d")) {
        decode = 1;
        type = FP;
      } else if (endsWith(name.first, "_i")) {
        decode = 2;
        type = INT;
      } else if (endsWith(name.first, "_c")) {
        decode = 3;
        type = INT;
      }
      auto use = std::make_shared<BoundMatchNode>(
          state, name.second, type, decode, errorMessage);
      definitions.push_back(use);
      childPredicates[name.first] = [=](ParseState & state) throw(ParseError) {
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
    if (body->type() != BOOL) {
      throw ParseError(state.where(), "Expression must be Boolean.");
    }
  }
  llvm::Value *generate(GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_ctx) {
    auto input_value =
        input->generate(state, read, header, error_fn, error_ctx);
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

    for (auto def : definitions) {
      def->prepare(state, arg_values);
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
    for (auto &def : definitions) {
      def->cleanup(state);
    }
    state->CreateBr(merge_block);

    state->SetInsertPoint(merge_block);
    auto phi = state->CreatePHI(
        llvm::Type::getInt1Ty(state.module()->getContext()), 2);
    phi->addIncoming(matched, original_block);
    phi->addIncoming(body_result, match_block);
    return phi;
  }
  llvm::Value *generateIndex(GenerateState &state,
                             llvm::Value *read,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_ctx) {
    return llvm::ConstantInt::getTrue(state.module()->getContext());
  }
  ExprType type() { return BOOL; }

private:
  std::vector<std::shared_ptr<BoundMatchNode>> definitions;
  RegularExpression regex;
  std::shared_ptr<AstNode> input;
  std::shared_ptr<AstNode> body;
};

std::shared_ptr<AstNode> parseMatchBinding(ParseState &state) throw(
    ParseError) {
  return std::make_shared<MatchBindingNode>(state);
}
}
