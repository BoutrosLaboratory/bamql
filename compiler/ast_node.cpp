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

#include "bamql-compiler.hpp"
#include "compiler.hpp"

namespace bamql {
llvm::Value *AstNode::generateIndex(GenerateState &state,
                                    llvm::Value *read,
                                    llvm::Value *header,
                                    llvm::Value *error_fn,
                                    llvm::Value *error_ctx) {
  return llvm::ConstantInt::getTrue(state.module()->getContext());
}

bool AstNode::usesIndex() { return false; }

llvm::Function *AstNode::createFunction(std::shared_ptr<Generator> &generator,
                                        llvm::StringRef name,
                                        llvm::StringRef param_name,
                                        llvm::Type *param_type,
                                        GenerateMember member) {
  llvm::Type *func_args_ty[] = {
    llvm::PointerType::get(getBamHeaderType(generator->module()), 0),
    param_type, getErrorHandlerType(generator->module()),
    llvm::PointerType::get(
        llvm::Type::getInt8Ty(generator->module()->getContext()), 0)
  };
  auto func_ty = llvm::FunctionType::get(
      llvm::Type::getInt1Ty(generator->module()->getContext()), func_args_ty,
      false);

  auto func = llvm::cast<llvm::Function>(
      generator->module()->getOrInsertFunction(name, func_ty));
  func->addAttribute(
#if LLVM_VERSION_MAJOR <= 4
      llvm::AttributeSet::ReturnIndex
#else
      llvm::AttributeList::ReturnIndex
#endif
      ,
      llvm::Attribute::ZExt);

  auto entry = llvm::BasicBlock::Create(generator->module()->getContext(),
                                        "entry", func);
  GenerateState state(generator, entry);
  auto args = func->arg_begin();
  auto header_value =
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
      args++;
#else
      &*args;
  args++;
#endif
  header_value->setName("header");
  auto param_value =
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
      args++;
#else
      &*args;
  args++;
#endif
  param_value->setName(param_name);
  auto error_fn_value =
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
      args++;
#else
      &*args;
  args++;
#endif
  error_fn_value->setName("error_fn");
  auto error_ctx_value =
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
      args++;
#else
      &*args;
  args++;
#endif
  error_ctx_value->setName("error_ctx");
  this->writeDebug(state);
  state->CreateRet(
      member == nullptr
          ? llvm::ConstantInt::getTrue(generator->module()->getContext())
          : (this->*member)(state, param_value, header_value, error_fn_value,
                            error_ctx_value));
  return func;
}

llvm::Function *AstNode::createFilterFunction(
    std::shared_ptr<Generator> &generator, llvm::StringRef name) {
  type_check(this, BOOL);
  return createFunction(
      generator, name, "read",
      llvm::PointerType::get(getBamType(generator->module()), 0),
      &AstNode::generate);
}

llvm::Function *AstNode::createIndexFunction(
    std::shared_ptr<Generator> &generator, llvm::StringRef name) {
  type_check(this, BOOL);
  return createFunction(
      generator, name, "tid",
      llvm::Type::getInt32Ty(generator->module()->getContext()),
      this->usesIndex() ? &AstNode::generateIndex : nullptr);
}

DebuggableNode::DebuggableNode(ParseState &state)
    : line(state.currentLine()), column(state.currentColumn()) {}
void DebuggableNode::writeDebug(GenerateState &state) {
  if (state.debugScope() != nullptr)
    state->SetCurrentDebugLocation(llvm::DebugLoc::get(line, column,
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
                                                       *state.debugScope()
#else
                                                       state.debugScope()
#endif
                                                           ));
}
}
