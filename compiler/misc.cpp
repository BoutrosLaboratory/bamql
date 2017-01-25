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
  auto func =
      llvm::cast<llvm::Function>(generator->module()->getOrInsertFunction(
          name,
          llvm::Type::getInt1Ty(generator->module()->getContext()),
          llvm::PointerType::get(bamql::getBamHeaderType(generator->module()),
                                 0),
          param_type,
          getErrorHandlerType(generator->module()),
          llvm::PointerType::get(
              llvm::Type::getInt8Ty(generator->module()->getContext()), 0),
          nullptr));

  auto entry = llvm::BasicBlock::Create(
      generator->module()->getContext(), "entry", func);
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
  state->CreateRet(member == nullptr ? llvm::ConstantInt::getTrue(
                                           generator->module()->getContext())
                                     : (this->*member)(state,
                                                       param_value,
                                                       header_value,
                                                       error_fn_value,
                                                       error_ctx_value));

  return func;
}

llvm::Function *AstNode::createFilterFunction(
    std::shared_ptr<Generator> &generator, llvm::StringRef name) {
  type_check(this, bamql::BOOL);
  return createFunction(
      generator,
      name,
      "read",
      llvm::PointerType::get(bamql::getBamType(generator->module()), 0),
      &bamql::AstNode::generate);
}

llvm::Function *AstNode::createIndexFunction(
    std::shared_ptr<Generator> &generator, llvm::StringRef name) {
  type_check(this, bamql::BOOL);
  return createFunction(
      generator,
      name,
      "tid",
      llvm::Type::getInt32Ty(generator->module()->getContext()),
      this->usesIndex() ? &bamql::AstNode::generateIndex : nullptr);
}

DebuggableNode::DebuggableNode(ParseState &state)
    : line(state.currentLine()), column(state.currentColumn()) {}
void DebuggableNode::writeDebug(GenerateState &state) {
  if (state.debugScope() != nullptr)
    state->SetCurrentDebugLocation(llvm::DebugLoc::get(line,
                                                       column,
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
                                                       *state.debugScope()
#else
                                                       state.debugScope()
#endif
                                                       ));
}

typedef void (*MemoryPolicy)(llvm::Function *func);
static void PureReadArg(llvm::Function *func) {
  func->setOnlyReadsMemory();
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 8
#else
  func->setOnlyAccessesArgMemory();
#endif
}
static void MutateInaccessible(llvm::Function *func) {
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 8
#else
  func->setOnlyAccessesInaccessibleMemory();
#endif
}
static void PureReadArgNoRecurse(llvm::Function *func) {
  PureReadArg(func);
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 8
#else
  func->setDoesNotRecurse();
#endif
}
static void NoRecurse(llvm::Function *func) {
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 8
#else
  func->setDoesNotRecurse();
#endif
}

static void createFunction(llvm::Module *module,
                           const std::string &name,
                           MemoryPolicy policy,
                           llvm::Type *ret,
                           const std::vector<llvm::Type *> &args) {
  auto func = llvm::Function::Create(llvm::FunctionType::get(ret, args, false),
                                     llvm::GlobalValue::ExternalLinkage,
                                     name,
                                     module);
  func->setCallingConv(llvm::CallingConv::C);
  policy(func);
}

llvm::Type *getRuntimeType(llvm::Module *module, llvm::StringRef name) {
  auto struct_ty = module->getTypeByName(name);
  if (struct_ty == nullptr) {
    auto struct_bam1_t =
        llvm::StructType::create(module->getContext(), "struct.bam1_t");
    auto struct_bam_hdr_t =
        llvm::StructType::create(module->getContext(), "struct.bam_hdr_t");
    auto ptr_bam1_t = llvm::PointerType::get(struct_bam1_t, 0);
    auto ptr_bam_hdr_t = llvm::PointerType::get(struct_bam_hdr_t, 0);
    auto base_bool = llvm::IntegerType::get(module->getContext(), 1);
    auto base_uint8 = llvm::IntegerType::get(module->getContext(), 8);
    auto base_uint32 = llvm::IntegerType::get(module->getContext(), 32);
    auto ptr_uint32 = llvm::PointerType::get(base_uint32, 0);
    auto base_str = llvm::PointerType::get(base_uint8, 0);
    auto base_double = llvm::Type::getDoubleTy(module->getContext());
    auto ptr_double = llvm::PointerType::get(base_double, 0);

    createFunction(module,
                   "bamql_aux_fp",
                   NoRecurse,
                   base_bool,
                   { ptr_bam1_t, base_uint8, base_uint8, ptr_double });
    createFunction(module,
                   "bamql_aux_int",
                   NoRecurse,
                   base_bool,
                   { ptr_bam1_t, base_uint8, base_uint8, ptr_uint32 });
    createFunction(module,
                   "bamql_aux_str",
                   PureReadArg,
                   base_str,
                   { ptr_bam1_t, base_uint8, base_uint8 });
    createFunction(module,
                   "bamql_check_chromosome",
                   PureReadArg,
                   base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, base_str, base_bool });
    createFunction(module,
                   "bamql_check_chromosome_id",
                   PureReadArg,
                   base_bool,
                   { ptr_bam_hdr_t, base_uint32, base_str });
    createFunction(module,
                   "bamql_check_flag",
                   PureReadArgNoRecurse,
                   base_bool,
                   { ptr_bam1_t, base_uint32 });
    createFunction(module,
                   "bamql_check_mapping_quality",
                   PureReadArgNoRecurse,
                   base_bool,
                   { ptr_bam1_t, base_uint8 });
    createFunction(module,
                   "bamql_check_nt",
                   PureReadArgNoRecurse,
                   base_bool,
                   { ptr_bam1_t, base_uint32, base_uint8, base_bool });
    createFunction(module,
                   "bamql_check_position",
                   PureReadArgNoRecurse,
                   base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, base_uint32, base_uint32 });
    createFunction(module,
                   "bamql_check_split_pair",
                   PureReadArgNoRecurse,
                   base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t });
    createFunction(module,
                   "bamql_chr",
                   PureReadArg,
                   base_str,
                   { ptr_bam_hdr_t, ptr_bam1_t, base_bool });
    createFunction(
        module, "bamql_header", PureReadArg, base_str, { ptr_bam1_t });
    createFunction(module,
                   "bamql_position_begin",
                   PureReadArg,
                   base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, ptr_uint32 });
    createFunction(module,
                   "bamql_position_end",
                   PureReadArg,
                   base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, ptr_uint32 });
    createFunction(module,
                   "bamql_randomly",
                   MutateInaccessible,
                   base_bool,
                   { base_double });
    createFunction(module,
                   "bamql_re_match",
                   PureReadArg,
                   base_bool,
                   { base_str, base_str });
    createFunction(module,
                   "bamql_strcmp",
                   PureReadArg,
                   base_uint32,
                   { base_str, base_str });
    createFunction(module,
                   "bamql_re_compile",
                   PureReadArg,
                   base_str,
                   { base_str, base_uint32, base_uint32 });

    llvm::Type *pcre_free_args[] = { base_str };
    llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(module->getContext()), pcre_free_args, false),
        llvm::GlobalValue::ExternalLinkage,
        "pcre_free_substring",
        module);
    llvm::Type *bamql_re_free_args[] = { llvm::PointerType::get(base_str, 0) };
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(module->getContext()),
                                bamql_re_free_args,
                                false),
        llvm::GlobalValue::ExternalLinkage,
        "bamql_re_free",
        module);
    llvm::Type *re_bind_args[] = {
      base_str, base_uint32, getErrorHandlerType(module), base_str, base_str
    };
    llvm::Function::Create(
        llvm::FunctionType::get(base_bool, re_bind_args, true),
        llvm::GlobalValue::ExternalLinkage,
        "bamql_re_bind",
        module);

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

llvm::Type *getErrorHandlerType(llvm::Module *module) {
  auto base_str = llvm::PointerType::get(
      llvm::IntegerType::get(module->getContext(), 8), 0);
  std::vector<llvm::Type *> args{ base_str, base_str };
  return llvm::PointerType::get(
      llvm::FunctionType::get(
          llvm::Type::getVoidTy(module->getContext()), args, false),
      0);
}

Generator::Generator(llvm::Module *module, llvm::DIScope *debug_scope_)
    : mod(module), debug_scope(debug_scope_) {
  std::map<std::string, llvm::IRBuilder<> **> tors = { { "__ctor", &ctor },
                                                       { "__dtor", &dtor } };
  for (auto it = tors.begin(); it != tors.end(); it++) {
    auto func = llvm::cast<llvm::Function>(module->getOrInsertFunction(
        it->first, llvm::Type::getVoidTy(module->getContext()), nullptr));
    func->setLinkage(llvm::GlobalValue::InternalLinkage);
    *it->second = new llvm::IRBuilder<>(
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
  for (auto it = tors.begin(); it != tors.end(); it++) {
    it->second->CreateRetVoid();
    auto link = new llvm::GlobalVariable(*mod,
                                         array_ty,
                                         false,
                                         llvm::GlobalVariable::AppendingLinkage,
                                         0,
                                         it->first);
    llvm::Constant *constants[] = {
      llvm::ConstantStruct::get(struct_ty,
                                llvm::ConstantInt::get(int32_ty, 65535),
                                it->second->GetInsertBlock()->getParent(),
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#else
                                llvm::ConstantPointerNull::get(base_str),
#endif

                                nullptr)
    };
    link->setInitializer(llvm::ConstantArray::get(array_ty, constants));
    delete it->second;
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
llvm::Type *getReifiedType(ExprType type, llvm::LLVMContext &context) {
  switch (type) {
  case BOOL:
    return llvm::Type::getInt1Ty(context);
  case FP:
    return llvm::Type::getDoubleTy(context);
  case INT:
    return llvm::IntegerType::get(context, 32);
  case STR:
    return llvm::PointerType::get(llvm::IntegerType::get(context, 8), 0);
  default:
    return nullptr;
  }
}
}
