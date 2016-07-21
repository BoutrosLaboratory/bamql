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

namespace bamql {
llvm::Value *AstNode::generateIndex(GenerateState &state,
                                    llvm::Value *read,
                                    llvm::Value *header) {
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
  this->writeDebug(state);
  state->CreateRet(
      member == nullptr
          ? llvm::ConstantInt::getTrue(generator->module()->getContext())
          : (this->*member)(state, param_value, header_value));

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

static void createFunction(llvm::Module *module,
                           const std::string &name,
                           const std::vector<llvm::Type *> &args) {
  auto func = llvm::Function::Create(
      llvm::FunctionType::get(
          llvm::IntegerType::get(module->getContext(), 1), args, false),
      llvm::GlobalValue::ExternalLinkage,
      name,
      module);
  func->setCallingConv(llvm::CallingConv::C);
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
    auto base_uint16 = llvm::IntegerType::get(module->getContext(), 16);
    auto base_uint32 = llvm::IntegerType::get(module->getContext(), 32);
    auto base_str = llvm::PointerType::get(base_uint8, 0);
    auto base_double = llvm::Type::getDoubleTy(module->getContext());

    createFunction(module,
                   "bamql_check_aux_char",
                   { ptr_bam1_t, base_uint8, base_uint8, base_uint8 });
    createFunction(module,
                   "bamql_check_aux_double",
                   { ptr_bam1_t, base_uint8, base_uint8, base_double });
    createFunction(module,
                   "bamql_check_aux_int",
                   { ptr_bam1_t, base_uint8, base_uint8, base_uint32 });
    createFunction(module,
                   "bamql_check_aux_str",
                   { ptr_bam1_t, base_uint8, base_uint8, base_str });
    createFunction(module,
                   "bamql_check_chromosome",
                   { ptr_bam_hdr_t, ptr_bam1_t, base_str });
    createFunction(module,
                   "bamql_check_chromosome_id",
                   { ptr_bam_hdr_t, base_uint32, base_str });
    createFunction(module, "bamql_check_flag", { ptr_bam1_t, base_uint16 });
    createFunction(
        module, "bamql_check_mapping_quality", { ptr_bam1_t, base_uint8 });
    createFunction(module,
                   "bamql_check_nt",
                   { ptr_bam1_t, base_uint32, base_uint8, base_bool });
    createFunction(module,
                   "bamql_check_position",
                   { ptr_bam_hdr_t, ptr_bam1_t, base_uint32, base_uint32 });
    createFunction(
        module, "bamql_check_split_pair", { ptr_bam_hdr_t, ptr_bam1_t });
    createFunction(module, "bamql_header_regex", { ptr_bam1_t, base_str });
    createFunction(module, "bamql_randomly", { base_double });
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
llvm::Module *GenerateState::module() const { return generator->module(); }
llvm::DIScope *GenerateState::debugScope() const {
  return generator->debugScope();
}
llvm::Value *GenerateState::createString(std::string &str) {
  return generator->createString(str);
}
}
