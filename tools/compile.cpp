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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <set>
#include <sstream>
#include <system_error>
#include <unistd.h>
#include <vector>
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
#include <llvm/PassManager.h>
#else
#include <llvm/IR/LegacyPassManager.h>
#endif
#include "bamql-compiler.hpp"
#include "config.h"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

std::vector<std::string> reserved = {
  "main",    "auto",     "break",  "case",     "char",   "const",    "continue",
  "default", "do",       "double", "else",     "enum",   "extern",   "float",
  "for",     "goto",     "if",     "int",      "long",   "register", "return",
  "short",   "signed",   "sizeof", "static",   "struct", "switch",   "typedef",
  "union",   "unsigned", "void",   "volatile", "while"
};

class ExistingFunction final : public bamql::AstNode {
public:
  ExistingFunction(llvm::Function *main_, llvm::Function *index_)
      : main(main_), index(index_) {}
  llvm::Value *generate(bamql::GenerateState &state,
                        llvm::Value *read,
                        llvm::Value *header,
                        llvm::Value *error_fn,
                        llvm::Value *error_context) {
    llvm::Value *args[] = { header, read, error_fn, error_context };
    auto call = state->CreateCall(main, args);
    call->addAttribute(
#if LLVM_VERSION_MAJOR <= 4
        llvm::AttributeSet::ReturnIndex
#else
        llvm::AttributeList::ReturnIndex
#endif
        ,
        llvm::Attribute::ZExt);
    return call;
  }

  llvm::Value *generateIndex(bamql::GenerateState &state,
                             llvm::Value *chromosome,
                             llvm::Value *header,
                             llvm::Value *error_fn,
                             llvm::Value *error_context) {
    llvm::Value *args[] = { header, chromosome, error_fn, error_context };
    auto call = state->CreateCall(index, args);
    call->addAttribute(
#if LLVM_VERSION_MAJOR <= 4
        llvm::AttributeSet::ReturnIndex
#else
        llvm::AttributeList::ReturnIndex
#endif

        ,
        llvm::Attribute::ZExt);
    return call;
  }
  bool usesIndex() { return true; }
  bamql::ExprType type() { return bamql::BOOL; }
  void writeDebug(bamql::GenerateState &state) {}

private:
  llvm::Function *main;
  llvm::Function *index;
};

bool checkBadName(const bamql::PredicateMap &predicates,
                  bamql::ParseState &state,
                  const std::string &filename,
                  const std::string &name) {
  if (std::find(reserved.begin(), reserved.end(), name) != reserved.end()) {
    std::cerr << filename << ":" << state.currentLine()
              << ": Unwilling to use C keyword \"" << name << "\" as a name."
              << std::endl;
    return true;
  }
  if (predicates.find(name) != predicates.end()) {
    std::cerr << filename << ":" << state.currentLine()
              << ": Redefinition of built-in \"" << name << "\"." << std::endl;
    return true;
  }
  return false;
}
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
      param_type, bamql::getErrorHandlerType(generator->module()),
      llvm::PointerType::get(
          llvm::Type::getInt8Ty(generator->module()->getContext()), 0)
    };
    auto func_type = llvm::FunctionType::get(
        llvm::Type::getInt1Ty(generator->module()->getContext()), args, false);
    func = llvm::Function::Create(func_type, llvm::GlobalValue::ExternalLinkage,
                                  name, generator->module());
    func->setCallingConv(llvm::CallingConv::C);
    func->addAttribute(
#if LLVM_VERSION_MAJOR <= 4
        llvm::AttributeSet::ReturnIndex
#else
        llvm::AttributeList::ReturnIndex
#endif
        ,
        llvm::Attribute::ZExt);
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
    case 'h':
      help = true;
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
                 "see the man page."
              << std::endl;
    std::cout
        << "\t-d\tDump the human-readable LLVM bitcode to standard output."
        << std::endl;
    std::cout << "\t-g\tGenerate debugging symbols." << std::endl;
    std::cout
        << "\t-H\tThe C header file for functions produced. If unspecified, it "
           "will be inferred from the input file name suffixed by `.h'."
        << std::endl;
    std::cout
        << "\t-o\tThe output file containing the object code. If unspecified, "
           "it will be the input file name suffixed by `.o'."
        << std::endl;
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

  std::shared_ptr<char> path(realpath(argv[optind], nullptr), free);
  std::shared_ptr<char> filename(strdup(path.get()), free);
  auto base_name = basename(filename.get());
  auto dir_name = dirname(path.get());
  // Create a new LLVM module and our functions
  llvm::LLVMContext context;
  auto module = std::make_shared<llvm::Module>(argv[optind], context);
  std::shared_ptr<llvm::DIBuilder> debug_builder;
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
  llvm::DIFile diFile;
  llvm::DIType diInt32;
  llvm::DIType diBam1;
  llvm::DIType diBamHdr;
  llvm::DIType diErrorFunc;
  llvm::DIType diVoidP;
  llvm::DICompositeType diFilterFunc;
  llvm::DICompositeType diIndexFunc;
#elif LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 6
  llvm::DIFile diFile;
  llvm::DIType diInt32;
  llvm::DIType diBam1;
  llvm::DIType diBamHdr;
  llvm::DIType diErrorFunc;
  llvm::DIType diVoidP;
  llvm::DISubroutineType diFilterFunc;
  llvm::DISubroutineType diIndexFunc;
#else
  llvm::DIFile *diFile = nullptr;
  llvm::DIType *diInt32 = nullptr;
  llvm::DIType *diBam1 = nullptr;
  llvm::DIType *diBamHdr = nullptr;
  llvm::DIType *diErrorFunc = nullptr;
  llvm::DIType *diVoidP = nullptr;
  llvm::DISubroutineType *diFilterFunc = nullptr;
  llvm::DISubroutineType *diIndexFunc = nullptr;
#endif
  if (debug) {
    debug_builder = std::make_shared<llvm::DIBuilder>(*module);
    diFile = debug_builder->createFile(base_name, dir_name);
#if LLVM_VERSION_MAJOR < 4
    debug_builder->createCompileUnit(llvm::dwarf::DW_LANG_C, base_name,
                                     dir_name, PACKAGE_STRING, false, "", 0);
#else
    debug_builder->createCompileUnit(llvm::dwarf::DW_LANG_C, diFile,
                                     PACKAGE_STRING, false, "", 0);
#endif
    diInt32 = debug_builder->createBasicType("uint32", 32, 32
#if LLVM_VERSION_MAJOR < 4
                                             ,
                                             llvm::dwarf::DW_ATE_unsigned
#endif
                                             );
    diBamHdr = debug_builder->createUnspecifiedType("bam_hdr_t");
    diBam1 = debug_builder->createUnspecifiedType("bam1_t");
    diErrorFunc = debug_builder->createUnspecifiedType("bamql_error_handler");
    diVoidP = debug_builder->createUnspecifiedType("void");
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
    llvm::Value *diFilterArgs[] = { diBamHdr, diBam1 };
    llvm::Value *diIndexArgs[] = { diBamHdr, diInt32 };
    auto diFilterSignature = debug_builder->getOrCreateArray(diFilterArgs);
    auto diIndexSignature = debug_builder->getOrCreateArray(diIndexArgs);
#elif LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR == 6
    llvm::Metadata *diFilterArgs[] = { diBamHdr, diBam1, diErrorFunc, diVoidP };
    llvm::Metadata *diIndexArgs[] = { diBamHdr, diInt32, diErrorFunc, diVoidP };
    auto diFilterSignature = debug_builder->getOrCreateTypeArray(diFilterArgs);
    auto diIndexSignature = debug_builder->getOrCreateTypeArray(diIndexArgs);
#else
    auto diFilterSignature = debug_builder->getOrCreateTypeArray(
        { diBamHdr, diBam1, diErrorFunc, diVoidP });
    auto diIndexSignature = debug_builder->getOrCreateTypeArray(
        { diBamHdr, diInt32, diErrorFunc, diVoidP });
#endif
    diFilterFunc = debug_builder->createSubroutineType(
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
        diFile,
#endif
        diFilterSignature);
    diIndexFunc = debug_builder->createSubroutineType(
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
        diFile,
#endif
        diIndexSignature);
  }
  std::string header_filename(
      createFileName(argv[optind], output_header, ".h"));
  std::ofstream header_file(header_filename);
  if (!header_file) {
    std::cerr << header_filename << ": " << strerror(errno) << std::endl;
    return 1;
  }
  header_file << "/* AUTOMATICALLY GENERATED BY " PACKAGE_STRING " FROM "
              << base_name << " IN " << dir_name << " */" << std::endl;
  header_file << "#pragma once" << std::endl;
  header_file << "#include <bamql-runtime.h>" << std::endl;
  header_file << "#ifdef __cplusplus" << std::endl;
  header_file << "extern \"C\" {" << std::endl;
  header_file << "#endif" << std::endl;

  std::set<std::string> defined_names;
  bamql::PredicateMap predicates = bamql::getDefaultPredicates();

  bamql::ParseState state(queries);
  state.push(predicates);
  auto generator = std::make_shared<bamql::Generator>(module.get(), nullptr);
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
      if (checkBadName(predicates, state, argv[optind], name)) {
        return 1;
      }
      auto index_name = name + "_index";
      auto node = std::make_shared<ExistingFunction>(
          createExternFunction(generator, name,
                               llvm::PointerType::get(
                                   bamql::getBamType(generator->module()), 0)),
          createExternFunction(
              generator, index_name,
              llvm::Type::getInt32Ty(generator->module()->getContext())));
      predicates[name] = [=](bamql::ParseState &state) { return node; };
    }
    do {
      state.parseSpace();
      auto startLine = state.currentLine();
      auto name = state.parseStr(
          "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
      if (name[0] >= '0' && name[0] <= '9') {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Identifier \"" << name
                  << "\" must not start with digits." << std::endl;
        return 1;
      }
      state.parseCharInSpace('=');
      auto ast = bamql::AstNode::parse(state);
      state.parseCharInSpace(';');
      if (name.length() >= 6 &&
          name.compare(name.length() - 6, 6, "_index") == 0) {
        std::cerr << argv[optind] << ":" << state.currentLine() << ": Name \""
                  << name << "\" must not be end in \"_index\"." << std::endl;
        return 1;
      }
      if (ast->type() != bamql::BOOL) {
        std::cerr << argv[optind] << ":" << state.currentLine() << ": Name \""
                  << name << "\" must be Boolean." << std::endl;
        return 1;
      }
      if (defined_names.find(name) == defined_names.end()) {
        defined_names.insert(name);
      } else {
        std::cerr << argv[optind] << ":" << state.currentLine()
                  << ": Duplicate name \"" << name << "\"." << std::endl;
        return 1;
      }

      if (checkBadName(predicates, state, argv[optind], name)) {
        return 1;
      }

      std::stringstream index_name;
      index_name << name << "_index";
      header_file << "extern bool " << name
                  << "(bam_hdr_t*, bam1_t*, bamql_error_handler, void*);"
                  << std::endl;
      header_file << "extern bool " << index_name.str()
                  << "(bam_hdr_t*, uint32_t, bamql_error_handler, "
                     "void*);"
                  << std::endl;

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
      llvm::DISubprogram filter_scope;
      llvm::DISubprogram index_scope;
#else
      llvm::DISubprogram *filter_scope = nullptr;
      llvm::DISubprogram *index_scope = nullptr;
#endif
      if (debug) {
        filter_scope =
            debug_builder->createFunction(diFile, name, name, diFile, startLine,
                                          diFilterFunc, false, true, startLine);

        index_scope = debug_builder->createFunction(
            diFile, index_name.str(), index_name.str(), diFile, startLine,
            diIndexFunc, false, true, startLine);
      }
      if (debug) {
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 7
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, filter_scope, "header",
            diFile, startLine, diBamHdr);
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, filter_scope, "read", diFile,
            startLine, diBam1);
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, filter_scope, "error_func",
            diFile, startLine, diErrorFunc);
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, filter_scope,
            "error_context", diFile, startLine, diVoidP);

        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, index_scope, "header",
            diFile, startLine, diBamHdr);
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, index_scope, "tid", diFile,
            startLine, diInt32);
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, index_scope, "error_func",
            diFile, startLine, diErrorFunc);
        debug_builder->createLocalVariable(
            llvm::dwarf::DW_AT_variable_parameter, index_scope, "error_context",
            diFile, startLine, diVoidP);
#else
        debug_builder->createParameterVariable(
            filter_scope, "header", 1, diFile, startLine, diBamHdr, true);
        debug_builder->createParameterVariable(filter_scope, "read", 2, diFile,
                                               startLine, diBam1, true);
        debug_builder->createParameterVariable(filter_scope, "error_func", 3,
                                               diFile, startLine, diErrorFunc,
                                               true);
        debug_builder->createParameterVariable(
            filter_scope, "error_context", 4, diFile, startLine, diVoidP, true);

        debug_builder->createParameterVariable(index_scope, "header", 1, diFile,
                                               startLine, diBamHdr, true);
        debug_builder->createParameterVariable(index_scope, "tid", 2, diFile,
                                               startLine, diInt32, true);
        debug_builder->createParameterVariable(
            index_scope, "error_func", 3, diFile, startLine, diErrorFunc, true);
        debug_builder->createParameterVariable(
            index_scope, "error_context", 4, diFile, startLine, diVoidP, true);
#endif
      }

      generator->setDebugScope(
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6

          &filter_scope
#else
          filter_scope
#endif
          );
      auto filter_func = ast->createFilterFunction(generator, name);
      generator->setDebugScope(
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6

          &index_scope
#else
          index_scope
#endif

          );

      auto index_func = ast->createIndexFunction(generator, index_name.str());
      auto node = std::make_shared<ExistingFunction>(filter_func, index_func);
      predicates[name] = [=](bamql::ParseState &state) { return node; };
    } while (!state.empty());
  } catch (bamql::ParseError e) {
    std::cerr << argv[optind] << ":" << state.currentLine() << ": " << e.what()
              << std::endl;
    return 1;
  }
  state.pop(predicates);

  header_file << "#ifdef __cplusplus" << std::endl;
  header_file << "}" << std::endl;
  header_file << "#endif" << std::endl;

  generator = nullptr;
  if (debug) {
    debug_builder->finalize();
  }
  if (dump) {
#if LLVM_VERSION_MAJOR <= 4
    module->dump();
#else
    module->print(llvm::errs(), nullptr);
#endif
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
      target->createTargetMachine(target_triple, llvm::sys::getHostCPUName(),
                                  "", llvm::TargetOptions(), llvm::Reloc::PIC_,
#if LLVM_VERSION_MAJOR <= 4
                                  llvm::CodeModel::Default
#else
                                  llvm::CodeModel::Small
#endif

                                  ));
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
  module->setDataLayout(target_machine->createDataLayout());
  llvm::legacy::PassManager pass_man;
#endif

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5
  std::string error_c;
#else
  std::error_code error_c;
#endif
  llvm::raw_fd_ostream output_stream(
      createFileName(argv[optind], output, ".o").c_str(), error_c,
      llvm::sys::fs::F_None);
  if (error.length() > 0) {
    std::cerr << error << std::endl;
    return 1;
  }

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
  llvm::formatted_raw_ostream raw_output_stream(output_stream);
#endif

  if (
#if LLVM_VERSION_MAJOR < 7
      target_machine->addPassesToEmitFile(pass_man,
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 6
                                          raw_output_stream,
#else
                                          output_stream,
#endif
                                          llvm::TargetMachine::CGFT_ObjectFile,
                                          false)
#else
      target_machine->addPassesToEmitFile(pass_man, output_stream, nullptr,
                                          llvm::TargetMachine::CGFT_ObjectFile,
                                          false, nullptr)

#endif
          ) {
    std::cerr << "Cannot create object file on this architecture." << std::endl;
    return 1;
  }
  pass_man.run(*module);

  return 0;
}
