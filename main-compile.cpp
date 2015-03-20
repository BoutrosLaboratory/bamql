#include <unistd.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <system_error>
#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include "barf.hpp"

std::string createFileName(const char *input_filename,
                           const char *output,
                           const char *suffix) {
  std::stringstream output_filename;
  if (output == nullptr) {
    const char *dot = strrchr(input_filename, '.');
    if (dot != nullptr && strcmp(dot, ".barf") == 0) {
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

/**
 * Use LLVM to compile a query into object code.
 */
int main(int argc, char *const *argv) {
  char *output = nullptr;
  char *output_header = nullptr;
  bool help = false;
  bool dump = false;
  int c;

  while ((c = getopt(argc, argv, "dhH:o:")) != -1) {
    switch (c) {
    case 'd':
      dump = true;
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
    std::cout << argv[0] << "[-d] [-H output.h] [-o output.o] query.barf"
              << std::endl;
    std::cout << "Compile a collection of queries to object code. For details, "
                 "see the man page." << std::endl;
    std::cout
        << "\t-d\tDump the human-readable LLVM bitcode to standard output."
        << std::endl;
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
  auto module =
      std::make_shared<llvm::Module>("barf", llvm::getGlobalContext());
  auto debug_builder = std::make_shared<llvm::DIBuilder>(*module);
  module->addModuleFlag(llvm::Module::Warning,
                        "Debug Info Version",
                        llvm::DEBUG_METADATA_VERSION);
  llvm::DIScope scope = debug_builder->createCompileUnit(
      llvm::dwarf::DW_LANG_C, argv[optind], ".", "BARF Compiler", false, "", 0);

  std::string header_filename(
      createFileName(argv[optind], output_header, ".h"));
  std::ofstream header_file(header_filename);
  if (!header_file) {
    std::cerr << header_filename << ": " << strerror(errno) << std::endl;
    return 1;
  }
  header_file << "#include <stdbool.h>" << std::endl;
  header_file << "#include <htslib/sam.h>" << std::endl;
  header_file << "#ifdef __cplusplus" << std::endl;
  header_file << "extern \"C\" {" << std::endl;
  header_file << "#endif" << std::endl;

  std::set<std::string> defined_names;
  barf::PredicateMap default_predicates = barf::getDefaultPredicates();

  barf::ParseState state(queries);
  try {
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
      auto ast = barf::AstNode::parse(state, default_predicates);
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

      auto filter_func = ast->createFilterFunction(module.get(), name, &scope);

      std::stringstream index_name;
      index_name << name << "_index";
      auto index_func =
          ast->createIndexFunction(module.get(), index_name.str(), &scope);
      header_file << "extern bool " << name << "(bam_hdr_t*, bam1_t*);"
                  << std::endl;
      header_file << "extern bool " << index_name.str()
                  << "(bam_hdr_t*, uint32_t);" << std::endl;

    } while (!state.empty());
  }
  catch (barf::ParseError e) {
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

  llvm::PassManager pass_man;
#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4
  pass_man.add(new llvm::DataLayout(*target_machine->getDataLayout()));
#else
  pass_man.add(new llvm::DataLayoutPass(*target_machine->getDataLayout()));
#endif

  llvm::raw_fd_ostream output_stream(
      createFileName(argv[optind], output, ".o").c_str(),
      error,
      llvm::sys::fs::F_None);
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

  return 0;
}
