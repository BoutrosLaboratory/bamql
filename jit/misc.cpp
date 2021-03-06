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
#include <llvm/Support/DynamicLibrary.h>
#include <pcre.h>

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

std::shared_ptr<llvm::ExecutionEngine> bamql::createEngine(
    std::unique_ptr<llvm::Module> module) {

  std::map<llvm::Function *, void (*)()> required;
  for (auto &known_func : known) {
    auto func = module->getFunction(known_func.first);
    if (func != nullptr) {
      required[func] = known_func.second;
    }
  }
  std::string error;
  std::vector<std::string> attrs;
  attrs.push_back("-avx"); // The AVX support (auto-vectoring) should be
                           // disabled since the JIT isn't smart enough to
                           // detect this, even though there is a detection
                           // routine. In particular, it makes Valgrind not
                           // work.
  std::shared_ptr<llvm::ExecutionEngine> engine(
      llvm::EngineBuilder(std::move(module)

                              )
          .setEngineKind(llvm::EngineKind::JIT)
          .setErrorStr(&error)
          .setMAttrs(attrs)
          .create());
  if (!engine) {
    std::cerr << error << std::endl;
  } else {
    for (auto &required_func : required) {
      union {
        void (*func)();
        void *ptr;
      } convert;
      convert.func = required_func.second;
      engine->addGlobalMapping(required_func.first, convert.ptr);
    }
  }
  return engine;
}
