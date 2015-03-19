#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <llvm/DIBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include "barf.hpp"

/**
 * Use LLVM to compile a query into object code.
 */
int main(int argc, char *const *argv) {
  const char *name = "filter";
  char *output = nullptr;
  bool help = false;
  bool dump = false;
  int c;

  while ((c = getopt(argc, argv, "dhn:o:")) != -1) {
    switch (c) {
    case 'd':
      dump = true;
      break;
    case 'n':
      name = optarg;
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
    std::cout << argv[0] << "[-d] [-n name] [-o output.bc] query" << std::endl;
    std::cout
        << "Compile a query to LLVM bitcode. For details, see the man page."
        << std::endl;
    std::cout
        << "\t-d\tDump the human-readable LLVM bitcode to standard output."
        << std::endl;
    std::cout << "\t-n\tThe name for function produced. If unspecified, it "
                 "will be `filter'." << std::endl;
    std::cout << "\t-o\tThe output file containing the reads. If unspecified, "
                 "it will be the function name suffixed by `.bc'." << std::endl;
    return 0;
  }

  if (argc - optind != 1) {
    std::cout << "Need a query." << std::endl;
    return 1;
  }
  // Initialise all the LLVM magic.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeNativeTarget();
  llvm::InitializeAllAsmPrinters();

  // Parse the input query.
  auto ast = barf::AstNode::parseWithLogging(std::string(argv[optind]),
                                             barf::getDefaultPredicates());
  if (!ast) {
    return 1;
  }

  // Create a new LLVM module and our functions
  auto module = std::make_shared<llvm::Module>(name, llvm::getGlobalContext());
  auto debug_builder = std::make_shared<llvm::DIBuilder>(*module);
  module->addModuleFlag(llvm::Module::Warning,
                        "Debug Info Version",
                        llvm::DEBUG_METADATA_VERSION);
  llvm::DIScope scope = debug_builder->createCompileUnit(
      llvm::dwarf::DW_LANG_C, name, ".", "BARF Compiler", false, "", 0);

  auto filter_func = ast->createFilterFunction(module.get(), name, &scope);

  std::stringstream index_name;
  index_name << name << "_index";
  auto index_func =
      ast->createIndexFunction(module.get(), index_name.str(), &scope);

  if (dump) {
    module->dump();
  }

  // Create the output bitcode file.
  std::stringstream output_filename;
  if (output == nullptr) {
    output_filename << name << ".o";
  } else {
    output_filename << output;
  }

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

  llvm::PassManager pass_man;
  pass_man.add(new llvm::DataLayout(*target_machine->getDataLayout()));

  llvm::raw_fd_ostream output_stream(
      output_filename.str().c_str(), error, llvm::sys::fs::F_None);
  if (error.length() > 0) {
    std::cerr << error << std::endl;
    return 1;
  }

  llvm::formatted_raw_ostream raw_output_stream(output_stream);

  if (target_machine->addPassesToEmitFile(pass_man,
                                          raw_output_stream,
                                          llvm::TargetMachine::CGFT_ObjectFile,
                                          false)) {
    std::cerr << "Cannot create object file on this architecture." << std::endl;
    return 1;
  }
  pass_man.run(*module);

  // Write the header file to stdout.
  std::cout << "#include <stdbool.h>" << std::endl;
  std::cout << "#include <htslib/sam.h>" << std::endl;
  std::cout << "#ifdef __cplusplus" << std::endl;
  std::cout << "extern \"C\" {" << std::endl;
  std::cout << "#endif" << std::endl;
  std::cout << "extern bool " << name << "(bam_hdr_t*, bam1_t*);" << std::endl;
  std::cout << "extern bool " << index_name.str() << "(bam_hdr_t*, uint32_t);"
            << std::endl;
  std::cout << "#ifdef __cplusplus" << std::endl;
  std::cout << "}" << std::endl;
  std::cout << "#endif" << std::endl;
  return 0;
}
