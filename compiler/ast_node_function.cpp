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

#include "ast_node_function.hpp"
#include "bamql-compiler.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <llvm/ADT/DenseMap.h>

namespace bamql {
UserArg::UserArg(ExprType type_) : type(type_) {}
void UserArg::nextArg(ParseState &state,
                      size_t &pos,
                      std::vector<std::shared_ptr<AstNode>> &args) const {
  state.parseCharInSpace(pos == 0 ? '(' : ',');
  pos++;
  auto node = AstNode::parse(state);
  if (node->type() != type) {
    throw ParseError(state.where(), "Type mismatch.");
  }
  args.push_back(node);
}
void NucleotideArg::nextArg(ParseState &state,
                            size_t &pos,
                            std::vector<std::shared_ptr<AstNode>> &args) const {
  state.parseCharInSpace(pos == 0 ? '(' : ',');
  pos++;
  args.push_back(std::make_shared<CharConst>(state.parseNucleotide()));
}

void AuxArg::nextArg(ParseState &state,
                     size_t &pos,
                     std::vector<std::shared_ptr<AstNode>> &args) const {
  state.parseCharInSpace(pos == 0 ? '(' : ',');
  pos++;
  auto first = *state;
  state.next();
  if (!isalnum(first)) {
    throw ParseError(state.where(),
                     "Expected alpha numeric identifier string.");
  }
  auto second = *state;
  state.next();
  if (!isalnum(second)) {
    throw ParseError(state.where(),
                     "Expected alpha numeric identifier string.");
  }
  args.push_back(std::make_shared<CharConst>(first));
  args.push_back(std::make_shared<CharConst>(second));
}

FunctionNode::FunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state)
    : DebuggableNode(state), arguments(std::move(arguments_)),
      rawArguments(rawArguments_), name(name_) {}
llvm::Value *FunctionNode::generate(GenerateState &state,
                                    llvm::Value *read,
                                    llvm::Value *header,
                                    llvm::Value *error_fn,
                                    llvm::Value *error_ctx) {
  auto function = state.module()->getFunction(name);
  std::vector<llvm::Value *> arg_values;
  for (auto raw_arg : rawArguments) {
    switch (raw_arg) {
    case READ:
      arg_values.push_back(read);
      break;
    case HEADER:
      arg_values.push_back(header);
      break;
    case ERROR:
      arg_values.push_back(error_fn);
      arg_values.push_back(error_ctx);
      break;
    case USER:
      for (auto &arg : arguments) {
        arg_values.push_back(
            arg->generate(state, read, header, error_fn, error_ctx));
      }
    }
  }
  return generateCall(state, function, arg_values, error_fn, error_ctx);
}
BoolFunctionNode::BoolFunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state)
    : FunctionNode(name_, std::move(arguments_), rawArguments_, state) {}
llvm::Value *BoolFunctionNode::generateCall(GenerateState &state,
                                            llvm::Function *func,
                                            std::vector<llvm::Value *> &args,
                                            llvm::Value *error_fun,
                                            llvm::Value *error_ctx) {
  auto call = state->CreateCall(&*func, args);
  call->addRetAttr(llvm::Attribute::ZExt);
  return call;
}
ExprType BoolFunctionNode::type() { return BOOL; }

ConstIntFunctionNode::ConstIntFunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state)
    : FunctionNode(name_, std::move(arguments_), rawArguments_, state) {}
llvm::Value *ConstIntFunctionNode::generateCall(
    GenerateState &state,
    llvm::Function *func,
    std::vector<llvm::Value *> &args,
    llvm::Value *error_fun,
    llvm::Value *error_ctx) {
  return state->CreateCall(func, args);
}
ExprType ConstIntFunctionNode::type() { return INT; }

ErrorFunctionNode::ErrorFunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state,
    const std::string &error_message_)
    : FunctionNode(name_, std::move(arguments_), rawArguments_, state),
      error_message(state.createRuntimeError(error_message_)) {}
llvm::Value *ErrorFunctionNode::generateCall(GenerateState &state,
                                             llvm::Function *func,
                                             std::vector<llvm::Value *> &args,
                                             llvm::Value *error_fun,
                                             llvm::Value *error_ctx) {
  llvm::Value *success = nullptr;
  llvm::Value *result = nullptr;
  generateRead(state, func, args, success, result);

  auto function = state->GetInsertBlock()->getParent();
  auto error_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "error", function);
  auto merge_block =
      llvm::BasicBlock::Create(state.module()->getContext(), "merge", function);

  state->CreateCondBr(success, merge_block, error_block);

  state->SetInsertPoint(error_block);
  llvm::Value *error_args[] = { state.createString(error_message), error_ctx };
  state->CreateCall(getErrorHandlerFunctionType(state.module()), error_fun,
                    error_args);
  state->CreateBr(merge_block);

  state->SetInsertPoint(merge_block);
  return result;
}

DblFunctionNode::DblFunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state,
    const std::string &error_message)
    : ErrorFunctionNode(
          name_, std::move(arguments_), rawArguments_, state, error_message) {}
void DblFunctionNode::generateRead(GenerateState &state,
                                   llvm::Function *function,
                                   std::vector<llvm::Value *> &args,
                                   llvm::Value *&success,
                                   llvm::Value *&result) {
  auto type = llvm::Type::getDoubleTy(state.module()->getContext());
  auto box = state->CreateAlloca(type);
  state->CreateStore(
      llvm::ConstantFP::get(type, std::numeric_limits<double>::quiet_NaN()),
      box);
  args.push_back(box);
  success = state->CreateCall(function, args);
  result = state->CreateLoad(type, box);
}
ExprType DblFunctionNode::type() { return FP; }

IntFunctionNode::IntFunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state,
    const std::string &error_message)
    : ErrorFunctionNode(
          name_, std::move(arguments_), rawArguments_, state, error_message) {}
void IntFunctionNode::generateRead(GenerateState &state,
                                   llvm::Function *function,
                                   std::vector<llvm::Value *> &args,
                                   llvm::Value *&success,
                                   llvm::Value *&result) {
  auto type = llvm::Type::getInt32Ty(state.module()->getContext());
  auto box = state->CreateAlloca(type);
  state->CreateStore(llvm::ConstantInt::get(type, 0), box);
  args.push_back(box);
  success = state->CreateCall(function, args);
  result = state->CreateLoad(type, box);
}
ExprType IntFunctionNode::type() { return INT; }

StrFunctionNode::StrFunctionNode(
    const std::string &name_,
    const std::vector<std::shared_ptr<AstNode>> &&arguments_,
    const std::vector<RawFunctionArg> &rawArguments_,
    ParseState &state,
    const std::string &error_message)
    : ErrorFunctionNode(
          name_, std::move(arguments_), rawArguments_, state, error_message) {}
void StrFunctionNode::generateRead(GenerateState &state,
                                   llvm::Function *function,
                                   std::vector<llvm::Value *> &args,
                                   llvm::Value *&success,
                                   llvm::Value *&result) {
  result = state->CreateCall(function, args);
  success = state->CreateICmpNE(
      result, llvm::ConstantPointerNull::get(llvm::PointerType::get(
                  llvm::Type::getInt8Ty(state.module()->getContext()), 0)));
}
ExprType StrFunctionNode::type() { return STR; }
} // namespace bamql
