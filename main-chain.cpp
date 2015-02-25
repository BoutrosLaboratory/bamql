#include <unistd.h>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include "barf.hpp"
#include "barf-jit.hpp"

/**
 * A type for a chaining behaviour. This is a bitfield where the lowest bit is
 * whether to accept the read on no-match and the next bit is whether to accept
 * the read on match.
 */
typedef unsigned int ChainPattern;

/**
 * The different chaining behaviours allowed.
 */
std::map<std::string, ChainPattern> known_chains = { { "parallel", 3 },
                                                     { "series", 2 },
                                                     { "shuttle", 1 } };

/**
 * One link of a chain. It checks the filter, writes matching reads to a file,
 * and propagates the read to the next link in the chain.
 */
class OutputWrangler : public barf::CheckIterator {
public:
  OutputWrangler(std::shared_ptr<llvm::ExecutionEngine> &engine,
                 llvm::Module *module,
                 std::shared_ptr<barf::AstNode> &node,
                 std::string name,
                 ChainPattern c,
                 std::string file_name_,
                 std::shared_ptr<htsFile> o,
                 std::shared_ptr<OutputWrangler> n)
      : barf::CheckIterator::CheckIterator(engine, module, node, name),
        chain(c), file_name(file_name_), output_file(o), next(n) {}

  /**
   * We want this chromosome if our query is interested or the next link can
   * make use of it _if_ it will see it upon failure (otherwise, its behaviour
   * is determined by ours.
   */
  bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid) {
    return CheckIterator::wantChromosome(header, tid) ||
           next && chain & 1 && next->wantChromosome(header, tid);
  }

  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
    sam_hdr_write(output_file.get(), header.get());
    if (next)
      next->ingestHeader(header);
  }

  /**
   * Write the read if it matches and then pass it down the chain if
   * appropriate.
   */
  void readMatch(bool matches,
                 std::shared_ptr<bam_hdr_t> &header,
                 std::shared_ptr<bam1_t> &read) {
    if (matches) {
      count++;
      sam_write1(output_file.get(), header.get(), read.get());
    }
    if (next && chain & (1 << matches)) {
      next->processRead(header, read);
    }
  }

  void write_summary() {
    std::cout << count << " " << file_name << std::endl;
    if (next) {
      next->write_summary();
    }
  }

private:
  ChainPattern chain;
  std::shared_ptr<htsFile> output_file;
  barf::FilterFunction filter;
  barf::IndexFunction index;
  std::shared_ptr<OutputWrangler> next;
  std::string file_name;
  size_t count = 0;
};

/**
 * Compile many queries, then arrange them into a chain and squirt reads
 * through it.
 */
int main(int argc, char *const *argv) {
  const char *input_filename = nullptr;
  bool binary = false;
  ChainPattern chain = known_chains["parallel"];
  bool help = false;
  bool ignore_index = false;
  int c;

  while ((c = getopt(argc, argv, "bc:f:hI")) != -1) {
    switch (c) {
    case 'b':
      binary = true;
      break;
    case 'c':
      if (known_chains.find(optarg) == known_chains.end()) {
        std::cerr << "Unknown chaining method: " << optarg << std::endl;
        return 1;
      } else {
        chain = known_chains[optarg];
      }
      break;
    case 'h':
      help = true;
      break;
    case 'I':
      ignore_index = true;
      break;
    case 'f':
      input_filename = optarg;
      break;
    case '?':
      fprintf(stderr, "Option -%c is not valid.\n", optopt);
      return 1;
    default:
      abort();
    }
  }
  if (help) {
    std::cout << argv[0] << " [-b] [-c] [-I] [-v] -f input.bam "
                            " query1 output1.bam ..." << std::endl;
    std::cout << "Filter a BAM/SAM file based on the provided query. For "
                 "details, see the man page." << std::endl;
    std::cout << "\t-b\tThe input file is binary (BAM) not text (SAM)."
              << std::endl;
    std::cout << "\t-c\tChain the queries, rather than use them independently."
              << std::endl;
    std::cout << "\t-I\tDo not use the index, even if it exists." << std::endl;
    std::cout << "\t-v\tPrint some information along the way." << std::endl;
    return 0;
  }

  if (optind >= argc) {
    std::cout << "Need a query and a BAM file." << std::endl;
    return 1;
  }
  if ((argc - optind) % 2 != 0) {
    std::cout << "Queries and BAM files must be paired." << std::endl;
    return 1;
  }
  if (input_filename == nullptr) {
    std::cout << "An input file is required." << std::endl;
    return 1;
  }
  // Create a new LLVM module and JIT
  LLVMInitializeNativeTarget();
  auto module = new llvm::Module("barf", llvm::getGlobalContext());
  auto engine = barf::createEngine(module);
  if (!engine) {
    return 1;
  }

  // Prepare a chain of wranglers.
  std::shared_ptr<OutputWrangler> output;
  for (auto it = argc - 2; it >= optind; it -= 2) {
    // Prepare the output file.
    auto output_file =
        std::shared_ptr<htsFile>(hts_open(argv[it + 1], "wb"), hts_close);
    if (!output_file) {
      perror(optarg);
      return 1;
    }
    // Parse the input query.
    auto ast = barf::AstNode::parseWithLogging(std::string(argv[it]),
                                               barf::getDefaultPredicates());
    if (!ast) {
      return 1;
    }

    // Add the link to the chain.
    std::stringstream function_name;
    function_name << "filter" << it;
    output = std::make_shared<OutputWrangler>(engine,
                                              module,
                                              ast,
                                              function_name.str(),
                                              chain,
                                              std::string(argv[it + 1]),
                                              output_file,
                                              output);
  }

  // Run the chain.
  if (output->processFile(input_filename, binary, ignore_index)) {
    output->write_summary();
    return 0;
  } else {
    return 1;
  }
}
