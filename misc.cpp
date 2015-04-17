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

#include "bamql.hpp"
#include "config.h"

namespace bamql {
extern llvm::Module *define_runtime(llvm::Module *module);

llvm::Value *AstNode::generateIndex(GenerateState &state,
                                    llvm::Value *read,
                                    llvm::Value *header) {
  return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
}

llvm::Function *AstNode::createFunction(std::shared_ptr<Generator> &generator,
                                        llvm::StringRef name,
                                        llvm::StringRef param_name,
                                        llvm::Type *param_type,
                                        GenerateMember member) {
  auto func =
      llvm::cast<llvm::Function>(generator->module()->getOrInsertFunction(
          name,
          llvm::Type::getInt1Ty(llvm::getGlobalContext()),
          llvm::PointerType::get(bamql::getBamHeaderType(generator->module()),
                                 0),
          param_type,
          nullptr));

  auto entry =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", func);
  GenerateState state(generator, entry);
  auto args = func->arg_begin();
  auto header_value = args++;
  header_value->setName("header");
  auto param_value = args++;
  param_value->setName(param_name);
  this->writeDebug(state);
  state->CreateRet((this->*member)(state, param_value, header_value));

  return func;
}

llvm::Function *AstNode::createFilterFunction(
    std::shared_ptr<Generator> &generator, llvm::StringRef name) {
  return createFunction(
      generator,
      name,
      "read",
      llvm::PointerType::get(bamql::getBamType(generator->module()), 0),
      &bamql::AstNode::generate);
}

llvm::Function *AstNode::createIndexFunction(
    std::shared_ptr<Generator> &generator, llvm::StringRef name) {
  return createFunction(generator,
                        name,
                        "tid",
                        llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                        &bamql::AstNode::generateIndex);
}

DebuggableNode::DebuggableNode(ParseState &state)
    : line(state.currentLine()), column(state.currentColumn()) {}
void DebuggableNode::writeDebug(GenerateState &state) {
  if (state.debugScope() != nullptr)
    state->SetCurrentDebugLocation(
        llvm::DebugLoc::get(line, column, *state.debugScope()));
}

llvm::Type *getRuntimeType(llvm::Module *module, llvm::StringRef name) {
  auto struct_ty = module->getTypeByName(name);
  if (struct_ty == nullptr) {
    define_runtime(module);
    struct_ty = module->getTypeByName(name);
  }
  return struct_ty;
}

llvm::Type *getBamType(llvm::Module *module) {
  return getRuntimeType(module, "struct.bam1_t");
}

llvm::Type *getBamHeaderType(llvm::Module *module) {
  return getRuntimeType(module, "struct.bam_hdr_t");
}

Generator::Generator(llvm::Module *module, llvm::DIScope *debug_scope_)
    : mod(module), debug_scope(debug_scope_) {}

llvm::Module *Generator::module() const { return mod; }
llvm::DIScope *Generator::debugScope() const { return debug_scope; }

llvm::Value *Generator::createString(std::string &str) {
  auto iterator = constant_pool.find(str);
  if (iterator != constant_pool.end()) {
    return iterator->second;
  }

  auto array =
      llvm::ConstantDataArray::getString(llvm::getGlobalContext(), str);
  auto global_variable = new llvm::GlobalVariable(
      *mod,
      llvm::ArrayType::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                           str.length() + 1),
      true,
      llvm::GlobalValue::PrivateLinkage,
      0,
      ".str");
  global_variable->setAlignment(1);
  global_variable->setInitializer(array);
  auto zero = llvm::ConstantInt::get(
      llvm::Type::getInt8Ty(llvm::getGlobalContext()), 0);
  std::vector<llvm::Value *> indicies;
  indicies.push_back(zero);
  indicies.push_back(zero);
  auto result = llvm::ConstantExpr::getGetElementPtr(global_variable, indicies);
  constant_pool[str] = result;
  return result;
}

GenerateState::GenerateState(std::shared_ptr<Generator> &generator_,
                             llvm::BasicBlock *entry)
    : generator(generator_), builder(entry) {}

llvm::IRBuilder<> *GenerateState::operator->() { return &builder; }
llvm::Module *GenerateState::module() const { return generator->module(); }
llvm::DIScope *GenerateState::debugScope() const {
  return generator->debugScope();
}
llvm::Value *GenerateState::createString(std::string &str) {
  return generator->createString(str);
}

std::string version() { return std::string(VERSION); }
}
