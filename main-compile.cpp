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

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <system_error>
#include <vector>
#include <llvm/IR/Module.h>
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#include <llvm/PassManager.h>
#else
#include <llvm/IR/LegacyPassManager.h>
#endif
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include "bamql.hpp"

class ExistingFunction : public bamql::AstNode {
public:
  ExistingFunction(llvm::Function *main_, llvm::Function *index_)
      : main(main_), index(index_) {}
  llvm::Value *generate(bamql::GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header) {
    llvm::Value *args[] = { header, read };
    return state->CreateCall(main, args);
  }

  llvm::Value *generateIndex(bamql::GenerateState &state,
                             llvm::Value *chromosome,
                             llvm::Value *header) {
    llvm::Value *args[] = { header, chromosome };
    return state->CreateCall(index, args);
  }
  bool usesIndex() { return true; }
  void writeDebug(bamql::GenerateState &state) {}

private:
  llvm::Function *main;
  llvm::Function *index;
};

std::string createFileName(const char *input_filename,
                           const char *output,
                           const char *suffix) {
  std::stringstream output_filename;
  if (output == nullptr) {
    const char *dot = strrchr(input_filename, '.');
    if (dot != nullptr && strcmp(dot, ".bamql") == 0) {
      output_filename.write(input_filename,
                            strlen(input_filename) - strlen(dot));
    } else {
      output_filename << input_filename;
    }
    output_filename << suffix;
  } else {
    output_filename << output;
  }
  return output_filename.str();
}

llvm::Function *createExternFunction(
    std::shared_ptr<bamql::Generator> &generator,
    std::string &name,
    llvm::Type *param_type) {
  auto func = generator->module()->getFunction(name);
  if (func == nullptr) {
    std::vector<llvm::Type *> args = {
      llvm::PointerType::get(bamql::getBamHeaderType(generator->module()), 0),
      param_type
    };
    auto func_type = llvm::FunctionType::get(
        llvm::IntegerType::get(generator->module()->getContext(), 1),
        args,
        false);
    func = llvm::Function::Create(func_type,
                                  llvm::GlobalValue::ExternalLinkage,
                                  name,
                                  generator->module());
    func->setCallingConv(llvm::CallingConv::C);
  }
  return func;
}

/**
 * Use LLVM to compile a query into object code.
 */
int main(int argc, char *const *argv) {
  char *output = nullptr;
  char *output_header = nullptr;
  bool help = false;
  bool dump = false;
  bool debug = false;
  int c;

  while ((c = getopt(argc, argv, "dghH:o:")) != -1) {
    switch (c) {
    case 'd':
      dump = true;
      break;
    case 'g':
      debug = true;
      break;
    case 'H':
      output_header = optarg;
      break;
    case 'o':
      output = optarg;
      break;
    case '?':
      fprintf(stderr, "Option -%c is not valid.\n", optopt);
      return 1;
    default:
      abort();
    }
  }
  if (help) {
    std::cout << argv[0] << "[-d] [-g] [-H output.h] [-o output.o] query.bamql"
              << std::endl;
    std::cout << "Compile a collection of queries to object code. For details, "
                 "see the man page." << std::endl;
    std::cout
        << "\t-d\tDump the human-readable LLVM bitcode to standard output."
        << std::endl;
    std::cout << "\t-g\tGenerate debugging symbols." << std::endl;
    std::cout
        << "\t-H\tThe C header file for functions produced. If unspecified, it "
           "will be inferred from the input file name." << std::endl;
    std::cout
        << "\t-o\tThe output file containing the object code. If unspecified, "
           "it will be the function name suffixed by `.o'." << std::endl;
    return 0;
  }

  if (argc - optind != 1) {
    std::cout << "Need a query file." << std::endl;
    return 1;
  }
  // Initialise all the LLVM magic.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  // Parse the input file.
  std::ifstream input_file(argv[optind]);
  if (!input_file) {
    std::cerr << argv[optind] << ": " << strerror(errno) << std::endl;
    return 1;
  }
  std::string queries((std::istreambuf_iterator<char>(input_file)),
                      std::istreambuf_iterator<char>());

  // Create a new LLVM module and our functions
  llvm::LLVMContext context;
  auto module = std::make_shared<llvm::Module>("bamql", context);
  std::shared_ptr<llvm::DIBuilder> debug_builder;
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
  std::shared_ptr<llvm::DIScope> scope;
#else
  llvm::DIScope *scope = nullptr;
#endif
  if (debug) {
    debug_builder = std::make_shared<llvm::DIBuilder>(*module);
    scope =
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
        std::make_shared<llvm::DIScope>(
#endif
            debug_builder->createCompileUnit(llvm::dwarf::DW_LANG_C,
                                             argv[optind],
                                             ".",
                                             "BAMQL Compiler",
                                             false,
                                             "",
                                             0)
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
            )
#endif
        ;
  }
  auto generator = std::make_shared<bamql::Generator>(module.get(),
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
                                                      scope.get()
#else
                                                      scope
#endif
                                                      );
  std::string header_filename(
      createFileName(argv[optind], output_header, ".h"));
  std::ofstream header_file(header_filename);
  if (!header_file) {
    std::cerr << header_filename << ": " << strerror(errno) << std::endl;
    return 1;
  }
  header_file << "#pragma once" << std::endl;
  header_file << "#include <stdbool.h>" << std::endl;
  header_file << "#include <htslib/sam.h>" << std::endl;
  header_file << "#ifdef __cplusplus" << std::endl;
  header_file << "extern \"C\" {" << std::endl;
  header_file << "#endif" << std::endl;

  std::set<std::string> defined_names;
  bamql::PredicateMap predicates = bamql::getDefaultPredicates();

  bamql::ParseState state(queries);
  try {
    state.parseSpace();
    while (state.parseKeyword("extern")) {
      state.parseSpace();
      auto name = state.parseStr(
          "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
      if (name[0] >= '0' && name[0] <= '9') {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Identifier \"" << name
                  << "\" must not start with digits." << std::endl;
        return 1;
      }
      state.parseCharInSpace(';');
      if (predicates.find(name) != predicates.end()) {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Redefinition of \"" << name << "\"." << std::endl;
        return 1;
      }
      auto index_name = name + "_index";
      auto node = std::make_shared<ExistingFunction>(
          createExternFunction(generator,
                               name,
                               llvm::PointerType::get(
                                   bamql::getBamType(generator->module()), 0)),
          createExternFunction(
              generator,
              index_name,
              llvm::Type::getInt32Ty(generator->module()->getContext())));
      predicates[name] = [=](bamql::ParseState &state) { return node; };
    }
    do {
      state.parseSpace();
      auto name = state.parseStr(
          "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
      if (name[0] >= '0' && name[0] <= '9') {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Identifier \"" << name
                  << "\" must not start with digits." << std::endl;
        return 1;
      }
      state.parseCharInSpace('=');
      auto ast = bamql::AstNode::parse(state, predicates);
      state.parseCharInSpace(';');
      if (name.length() >= 6 &&
          name.compare(name.length() - 6, 6, "_index") == 0) {
        std::cerr << argv[optind] << ":" << state.currentLine() << ": Name \""
                  << name << "\" must not be end in \"_index\"." << std::endl;
        return 1;
      }
      if (defined_names.find(name) == defined_names.end()) {
        defined_names.insert(name);
      } else {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Duplicate name \"" << name << "\"." << std::endl;
        return 1;
      }

      if (predicates.find(name) != predicates.end()) {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Redefinition of built-in \"" << name << "\"."
                  << std::endl;
        return 1;
      }

      std::stringstream index_name;
      index_name << name << "_index";
      header_file << "extern bool " << name << "(bam_hdr_t*, bam1_t*);"
                  << std::endl;
      header_file << "extern bool " << index_name.str()
                  << "(bam_hdr_t*, uint32_t);" << std::endl;

      auto node = std::make_shared<ExistingFunction>(
          ast->createFilterFunction(generator, name),
          ast->createIndexFunction(generator, index_name.str()));
      predicates[name] = [=](bamql::ParseState &state) { return node; };
    } while (!state.empty());
  } catch (bamql::ParseError e) {
    std::cerr << argv[optind] << ":" << state.currentLine() << ": " << e.what()
              << std::endl;
    return 1;
  }
  header_file << "#ifdef __cplusplus" << std::endl;
  header_file << "}" << std::endl;
  header_file << "#endif" << std::endl;

  if (dump) {
    module->dump();
  }

  // Create the output object file.

  auto target_triple = llvm::sys::getDefaultTargetTriple();
  std::string error;

  const llvm::Target *target =
      llvm::TargetRegistry::lookupTarget(target_triple, error);
  if (target == nullptr) {
    std::cerr << error << std::endl;
    return 1;
  }
  std::shared_ptr<llvm::TargetMachine> target_machine(
      target->createTargetMachine(target_triple,
                                  llvm::sys::getHostCPUName(),
                                  "",
                                  llvm::TargetOptions(),
                                  llvm::Reloc::PIC_,
                                  llvm::CodeModel::Default));
  if (!target_machine) {
    std::cerr << "Could not allocate target machine." << std::endl;
    return 1;
  }

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
  llvm::PassManager pass_man;
  pass_man.add(new llvm::DataLayout(*target_machine->getDataLayout()));
#elif LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
  llvm::legacy::PassManager pass_man;
  pass_man.add(new llvm::DataLayoutPass(*target_machine->getDataLayout()));
#elif LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
  llvm::legacy::PassManager pass_man;
  auto dlp = new llvm::DataLayoutPass();
  dlp->doInitialization(*module);
  pass_man.add(dlp);
#else
  llvm::legacy::PassManager pass_man;
#endif

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
  std::string error_c;
#else
  std::error_code error_c;
#endif
  llvm::raw_fd_ostream output_stream(
      createFileName(argv[optind], output, ".o").c_str(),
      error_c,
      llvm::sys::fs::F_None);
  if (error.length() > 0) {
    std::cerr << error << std::endl;
    return 1;
  }

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
  llvm::formatted_raw_ostream raw_output_stream(output_stream);
#endif

  if (target_machine->addPassesToEmitFile(pass_man,
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
                                          raw_output_stream,
#else
                                          output_stream,
#endif
                                          llvm::TargetMachine::CGFT_ObjectFile,
                                          false)) {
    std::cerr << "Cannot create object file on this architecture." << std::endl;
    return 1;
  }
  pass_man.run(*module);

  return 0;
}
