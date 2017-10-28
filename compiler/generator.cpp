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

Generator::Generator(llvm::Module *module, llvm::DIScope *debug_scope_)
    : mod(module), debug_scope(debug_scope_) {
  std::map<std::string, llvm::IRBuilder<> **> tors = { { "__ctor", &ctor },
                                                       { "__dtor", &dtor } };
  for (auto &tor : tors) {
    auto func = llvm::cast<llvm::Function>(module->getOrInsertFunction(
        tor.first,
        llvm::FunctionType::get(llvm::Type::getVoidTy(module->getContext()),
                                false)));
    func->setLinkage(llvm::GlobalValue::InternalLinkage);
    *tor.second = new llvm::IRBuilder<>(
        llvm::BasicBlock::Create(module->getContext(), "entry", func));
  }
}
Generator::~Generator() {
  std::map<std::string, llvm::IRBuilder<> *> tors = {
    { "llvm.global_ctors", ctor }, { "llvm.global_dtors", dtor }
  };
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#else
  auto base_str =
      llvm::PointerType::get(llvm::Type::getInt8Ty(mod->getContext()), 0);
#endif
  auto int32_ty = llvm::Type::getInt32Ty(mod->getContext());
  llvm::Type *struct_elements[] = {
    int32_ty,
    llvm::PointerType::get(llvm::FunctionType::get(
                               llvm::Type::getVoidTy(mod->getContext()), false),
                           0),
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#else
    base_str
#endif
  };
  auto struct_ty = llvm::StructType::create(struct_elements);
  auto array_ty = llvm::ArrayType::get(struct_ty, 1);
  for (auto &tor : tors) {
    tor.second->CreateRetVoid();
    auto link = new llvm::GlobalVariable(*mod,
                                         array_ty,
                                         false,
                                         llvm::GlobalVariable::AppendingLinkage,
                                         0,
                                         tor.first);
    llvm::Constant *constants[] = {
      llvm::ConstantStruct::get(struct_ty,
                                llvm::ConstantInt::get(int32_ty, 65535),
                                tor.second->GetInsertBlock()->getParent()
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#else
                                ,
                                llvm::ConstantPointerNull::get(base_str)
#endif
#if LLVM_VERSION_MAJOR <= 4
                                ,
                                nullptr
#endif
                                )
    };
    link->setInitializer(llvm::ConstantArray::get(array_ty, constants));
    delete tor.second;
  }
}

llvm::Module *Generator::module() const { return mod; }
llvm::DIScope *Generator::debugScope() const { return debug_scope; }
void Generator::setDebugScope(llvm::DIScope *scope) { debug_scope = scope; }
llvm::IRBuilder<> *Generator::constructor() const { return ctor; }
llvm::IRBuilder<> *Generator::destructor() const { return dtor; }

llvm::Constant *Generator::createString(const std::string &str) {
  auto iterator = constant_pool.find(str);
  if (iterator != constant_pool.end()) {
    return iterator->second;
  }

  auto array = llvm::ConstantDataArray::getString(mod->getContext(), str);
  auto global_variable = new llvm::GlobalVariable(
      *mod,
      llvm::ArrayType::get(llvm::Type::getInt8Ty(mod->getContext()),
                           str.length() + 1),
      true,
      llvm::GlobalValue::PrivateLinkage,
      0,
      ".str");
  global_variable->setAlignment(1);
  global_variable->setInitializer(array);
  auto zero =
      llvm::ConstantInt::get(llvm::Type::getInt8Ty(mod->getContext()), 0);
  std::vector<llvm::Value *> indicies;
  indicies.push_back(zero);
  indicies.push_back(zero);
  auto result = llvm::ConstantExpr::getGetElementPtr(
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
#else
      global_variable->getValueType(),
#endif
      global_variable, indicies);
  constant_pool[str] = result;
  return result;
}

GenerateState::GenerateState(std::shared_ptr<Generator> &generator_,
                             llvm::BasicBlock *entry)
    : generator(generator_), builder(entry) {}

llvm::IRBuilder<> *GenerateState::operator->() { return &builder; }
llvm::IRBuilder<> *GenerateState::operator*() { return &builder; }
Generator &GenerateState::getGenerator() const { return *generator; }
llvm::Module *GenerateState::module() const { return generator->module(); }
llvm::DIScope *GenerateState::debugScope() const {
  return generator->debugScope();
}
llvm::Constant *GenerateState::createString(const std::string &str) {
  return generator->createString(str);
}
}
