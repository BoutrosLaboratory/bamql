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

#include "bamql-jit.hpp"
#include "bamql-runtime.h"
#include <iostream>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <pcre.h>
#include <sstream>

std::map<std::string, void (*)()> known = {
  { "bamql_aux_fp", (void (*)())bamql_aux_fp },
  { "bamql_aux_int", (void (*)())bamql_aux_int },
  { "bamql_aux_str", (void (*)())bamql_aux_str },
  { "bamql_check_chromosome", (void (*)())bamql_check_chromosome },
  { "bamql_check_chromosome_id", (void (*)())bamql_check_chromosome_id },
  { "bamql_check_mapping_quality", (void (*)())bamql_check_mapping_quality },
  { "bamql_check_nt", (void (*)())bamql_check_nt },
  { "bamql_check_position", (void (*)())bamql_check_position },
  { "bamql_check_split_pair", (void (*)())bamql_check_split_pair },
  { "bamql_chr", (void (*)())bamql_chr },
  { "bamql_flags", (void (*)())bamql_flags },
  { "bamql_header", (void (*)())bamql_header },
  { "bamql_insert_reversed", (void (*)())bamql_insert_reversed },
  { "bamql_insert_size", (void (*)())bamql_insert_size },
  { "bamql_mate_position_begin", (void (*)())bamql_mate_position_begin },
  { "bamql_position_begin", (void (*)())bamql_position_begin },
  { "bamql_position_end", (void (*)())bamql_position_end },
  { "bamql_randomly", (void (*)())bamql_randomly },
  { "bamql_re_bind", (void (*)())bamql_re_bind },
  { "bamql_re_match", (void (*)())bamql_re_match },
  { "bamql_strcmp", (void (*)())bamql_strcmp },
  { "bamql_re_compile", (void (*)())bamql_re_compile },
  { "bamql_re_free", (void (*)())bamql_re_free },
  { "pcre_free_substring", (void (*)())pcre_free_substring },
};

bamql::JIT::JIT() : lljit(llvm::cantFail(llvm::orc::LLJITBuilder().create())) {
  for (auto &entry : known) {
    llvm::cantFail(lljit->getMainJITDylib().define(llvm::orc::absoluteSymbols(
        { { lljit->getExecutionSession().intern(entry.first),
            llvm::JITEvaluatedSymbol::fromPointer((void *)entry.second) } })));
  }
}

bamql::JIT::~JIT() {}

std::shared_ptr<bamql::JIT> bamql::JIT::create() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();
  return std::shared_ptr<bamql::JIT>(new bamql::JIT());
}

std::shared_ptr<bamql::CompiledPredicate> bamql::JIT::compile(
    std::shared_ptr<JIT> &jit,
    std::shared_ptr<AstNode> &node,
    const std::string &name,
    llvm::DIScope *debug_scope) {
  auto context = std::make_unique<llvm::LLVMContext>();

  auto module = std::make_unique<llvm::Module>(name, *context);
  auto generator =
      std::make_shared<bamql::Generator>(module.get(), debug_scope);
  auto filter_func = node->createFilterFunction(generator, name);
  std::stringstream index_function_name;
  index_function_name << name << "_index";
  auto index_func =
      node->createIndexFunction(generator, index_function_name.str());

  generator = nullptr;
  auto &dylib = llvm::cantFail(jit->lljit->createJITDylib(name));
  dylib.addToLinkOrder(jit->lljit->getMainJITDylib());
  llvm::cantFail(jit->lljit->addIRModule(
      dylib,

      llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));

  union {
    llvm::JITTargetAddress ptr;
    FilterFunction func;
  } filter;
  filter.ptr = llvm::cantFail(jit->lljit->lookup(dylib, name)).getAddress();
  union {
    llvm::JITTargetAddress ptr;
    IndexFunction func;
  } index;
  index.ptr =
      llvm::cantFail(jit->lljit->lookup(dylib, index_function_name.str()))
          .getAddress();

  llvm::cantFail(jit->lljit->initialize(dylib));

  return std::make_shared<bamql::CompiledPredicate>(jit, name, filter.func,
                                                    index.func);
}

bamql::CompiledPredicate::CompiledPredicate(std::shared_ptr<JIT> &jit_,
                                            std::string name_,
                                            bamql::FilterFunction filter_,
                                            bamql::IndexFunction index_)
    : jit(jit_), name(name_), filter(filter_), index(index_) {}
bamql::CompiledPredicate::~CompiledPredicate() {
  auto dylib = jit->lljit->getJITDylibByName(name);
  llvm::cantFail(jit->lljit->deinitialize(*dylib));
  llvm::cantFail(jit->lljit->getExecutionSession().removeJITDylib(*dylib));
}

struct ErrorHolder {
  std::function<void(const char *)> error_handler;
};
bool bamql::CompiledPredicate::wantChromosome(
    std::shared_ptr<bam_hdr_t> &header,
    uint32_t tid,
    std::function<void(const char *)> error_handler) {
  ErrorHolder h{ error_handler };
  return index(
      header.get(), tid,
      [](const char *message, void *v) {
        ((ErrorHolder *)v)->error_handler(message);
      },
      &h);
}
bool bamql::CompiledPredicate::wantRead(
    std::shared_ptr<bam_hdr_t> &header,
    std::shared_ptr<bam1_t> &read,
    std::function<void(const char *)> error_handler) {
  ErrorHolder h{ error_handler };

  return filter(
      header.get(), read.get(),
      [](const char *message, void *v) {
        ((ErrorHolder *)v)->error_handler(message);
      },
      &h);
}
