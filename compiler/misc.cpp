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

typedef void (*MemoryPolicy)(llvm::Function *func);
static void PureReadArg(llvm::Function *func) {
  func->setOnlyReadsMemory();
  func->setOnlyAccessesArgMemory();
}
static void MutateInaccessible(llvm::Function *func) {
  func->setOnlyAccessesInaccessibleMemory();
}
static void PureReadArgNoRecurse(llvm::Function *func) {
  PureReadArg(func);
  func->setDoesNotRecurse();
}
static void NoRecurse(llvm::Function *func) { func->setDoesNotRecurse(); }

static void createFunction(llvm::Module *module,
                           const std::string &name,
                           MemoryPolicy policy,
                           llvm::Type *ret,
                           const std::vector<llvm::Type *> &args) {
  auto func =
      llvm::Function::Create(llvm::FunctionType::get(ret, args, false),
                             llvm::GlobalValue::ExternalLinkage, name, module);
  func->setCallingConv(llvm::CallingConv::C);
  if (ret->isIntegerTy() && ret->getIntegerBitWidth() == 1) {
    func->addRetAttr(llvm::Attribute::ZExt);
  }
  policy(func);
}

llvm::Type *getRuntimeType(llvm::Module *module, llvm::StringRef name) {
  auto struct_ty = llvm::StructType::getTypeByName(module->getContext(), name);
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

    createFunction(module, "bamql_aux_fp", NoRecurse, base_bool,
                   { ptr_bam1_t, base_uint8, base_uint8, ptr_double });
    createFunction(module, "bamql_aux_int", NoRecurse, base_bool,
                   { ptr_bam1_t, base_uint8, base_uint8, ptr_uint32 });
    createFunction(module, "bamql_aux_str", PureReadArg, base_str,
                   { ptr_bam1_t, base_uint8, base_uint8 });
    createFunction(module, "bamql_check_chromosome", PureReadArg, base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, base_str, base_bool });
    createFunction(module, "bamql_check_chromosome_id", PureReadArg, base_bool,
                   { ptr_bam_hdr_t, base_uint32, base_str });
    createFunction(module, "bamql_check_mapping_quality", PureReadArgNoRecurse,
                   base_bool, { ptr_bam1_t, base_uint8 });
    createFunction(module, "bamql_check_nt", PureReadArgNoRecurse, base_bool,
                   { ptr_bam1_t, base_uint32, base_uint8, base_bool });
    createFunction(module, "bamql_check_position", PureReadArgNoRecurse,
                   base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, base_uint32, base_uint32 });
    createFunction(module, "bamql_check_split_pair", PureReadArgNoRecurse,
                   base_bool, { ptr_bam_hdr_t, ptr_bam1_t });
    createFunction(module, "bamql_chr", PureReadArg, base_str,
                   { ptr_bam_hdr_t, ptr_bam1_t, base_bool });
    createFunction(module, "bamql_flags", PureReadArgNoRecurse, base_uint32,
                   { ptr_bam1_t });
    createFunction(module, "bamql_header", PureReadArg, base_str,
                   { ptr_bam1_t });
    createFunction(module, "bamql_insert_size", PureReadArg, base_uint32,
                   { ptr_bam1_t, getErrorHandlerType(module), base_str });
    createFunction(module, "bamql_insert_reversed", PureReadArgNoRecurse,
                   base_bool, { ptr_bam1_t });
    createFunction(
        module, "bamql_mate_position_begin", PureReadArg, base_uint32,
        { ptr_bam_hdr_t, ptr_bam1_t, getErrorHandlerType(module), base_str });
    createFunction(module, "bamql_position_begin", PureReadArg, base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, ptr_uint32 });
    createFunction(module, "bamql_position_end", PureReadArg, base_bool,
                   { ptr_bam_hdr_t, ptr_bam1_t, ptr_uint32 });
    createFunction(module, "bamql_randomly", MutateInaccessible, base_bool,
                   { base_double });
    createFunction(module, "bamql_re_match", PureReadArg, base_bool,
                   { base_str, base_str });
    createFunction(module, "bamql_strcmp", PureReadArg, base_uint32,
                   { base_str, base_str });
    createFunction(module, "bamql_re_compile", PureReadArg, base_str,
                   { base_str, base_uint32, base_uint32 });

    llvm::Type *pcre_free_args[] = { base_str };
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(module->getContext()),
                                pcre_free_args, false),
        llvm::GlobalValue::ExternalLinkage, "pcre_free_substring", module);
    llvm::Type *bamql_re_free_args[] = { llvm::PointerType::get(base_str, 0) };
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(module->getContext()),
                                bamql_re_free_args, false),
        llvm::GlobalValue::ExternalLinkage, "bamql_re_free", module);
    llvm::Type *re_bind_args[] = { base_str, base_uint32,
                                   getErrorHandlerType(module), base_str,
                                   base_str };
    llvm::Function::Create(
        llvm::FunctionType::get(base_bool, re_bind_args, true),
        llvm::GlobalValue::ExternalLinkage, "bamql_re_bind", module);

    struct_ty = llvm::StructType::getTypeByName(module->getContext(), name);
    if (struct_ty == nullptr) {
      abort();
    }
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
  return llvm::PointerType::get(getErrorHandlerFunctionType(module), 0);
}

llvm::FunctionType *getErrorHandlerFunctionType(llvm::Module *module) {
  auto base_str = llvm::PointerType::get(
      llvm::IntegerType::get(module->getContext(), 8), 0);
  std::vector<llvm::Type *> args{ base_str, base_str };
  return llvm::FunctionType::get(llvm::Type::getVoidTy(module->getContext()),
                                 args, false);
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
} // namespace bamql
