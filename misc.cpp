#include "barf.hpp"
#include "config.h"

namespace barf {
extern llvm::Module *define_runtime(llvm::Module *module);

llvm::Value *AstNode::generateIndex(llvm::Module *module,
                                    llvm::IRBuilder<> &builder,
                                    llvm::Value *read,
                                    llvm::Value *header,
                                    llvm::DIScope *debug_scope) {
  return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
}

llvm::Function *AstNode::createFunction(llvm::Module *module,
                                        llvm::StringRef name,
                                        llvm::StringRef param_name,
                                        llvm::Type *param_type,
                                        llvm::DIScope *debug_scope,
                                        GenerateMember member) {
  auto func = llvm::cast<llvm::Function>(module->getOrInsertFunction(
      name,
      llvm::Type::getInt1Ty(llvm::getGlobalContext()),
      llvm::PointerType::get(barf::getBamHeaderType(module), 0),
      param_type,
      nullptr));

  auto entry =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", func);
  llvm::IRBuilder<> builder(entry);
  auto args = func->arg_begin();
  auto header_value = args++;
  header_value->setName("header");
  auto param_value = args++;
  param_value->setName(param_name);
  this->writeDebug(module, builder, debug_scope);
  builder.CreateRet(
      (this->*member)(module, builder, param_value, header_value, debug_scope));

  return func;
}

llvm::Function *AstNode::createFilterFunction(llvm::Module *module,
                                              llvm::StringRef name,
                                              llvm::DIScope *debug_scope) {
  return createFunction(module,
                        name,
                        "read",
                        llvm::PointerType::get(barf::getBamType(module), 0),
                        debug_scope,
                        &barf::AstNode::generate);
}

llvm::Function *AstNode::createIndexFunction(llvm::Module *module,
                                             llvm::StringRef name,
                                             llvm::DIScope *debug_scope) {
  return createFunction(module,
                        name,
                        "tid",
                        llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                        debug_scope,
                        &barf::AstNode::generateIndex);
}

DebuggableNode::DebuggableNode(ParseState &state)
    : line(state.currentLine()), column(state.currentColumn()) {}
void DebuggableNode::writeDebug(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::DIScope *debug_scope) {
  if (debug_scope != nullptr)
    builder.SetCurrentDebugLocation(
        llvm::DebugLoc::get(line, column, *debug_scope));
}

llvm::Type *getRuntimeType(llvm::Module *module, llvm::StringRef name) {
  auto struct_ty = module->getTypeByName(name);
  if (struct_ty == nullptr) {
    define_runtime(module);
    for (auto func = module->begin(); func != module->end(); func++) {
      if (!func->isDeclaration()) {
        func->setLinkage(llvm::GlobalValue::ExternalWeakLinkage);
      }
    }
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

llvm::Value *createString(llvm::Module *module, std::string str) {
  auto array =
      llvm::ConstantDataArray::getString(llvm::getGlobalContext(), str);
  auto global_variable = new llvm::GlobalVariable(
      *module,
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
  return llvm::ConstantExpr::getGetElementPtr(global_variable, indicies);
}

std::string version() { return std::string(VERSION); }
}
