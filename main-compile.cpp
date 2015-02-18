#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <llvm/IR/Module.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
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

  // Parse the input query.
  auto ast = barf::ast_node::parse_with_logging(std::string(argv[optind]),
                                                barf::getDefaultPredicates());
  if (!ast) {
    return 1;
  }

  // Create a new LLVM module and our functions
  auto module = new llvm::Module(name, llvm::getGlobalContext());

  auto filter_func = ast->create_filter_function(module, name);

  std::stringstream index_name;
  index_name << name << "_index";
  auto index_func = ast->create_index_function(module, index_name.str());

  if (dump) {
    module->dump();
  }

  // Create the output bitcode file.
  std::stringstream output_filename;
  if (output == nullptr) {
    output_filename << name << ".bc";
  } else {
    output_filename << output;
  }
  std::string error;
  llvm::raw_fd_ostream out_data(
      output_filename.str().c_str(), error, llvm::sys::fs::F_None);
  if (error.length() > 0) {
    std::cerr << error << std::endl;
    return 1;
  } else {
    llvm::WriteBitcodeToFile(module, out_data);
  }

  // Write the header file to stdout.
  std::cout << "#include <stdbool.h>\n#include <htslib/sam.h>\nextern bool "
            << name << "(bam_hdr_t*, bam1_t*)\nextern bool " << index_name.str()
            << "(bam_hdr_t*, uint32_t)" << std::endl;
  delete module;
  return 0;
}
