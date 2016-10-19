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
#include <map>
#include <iostream>
#include <sys/stat.h>
#include <uuid.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include "bamql-compiler.hpp"
#include "bamql-jit.hpp"

/**
 * Handler for output collection. Shunts reads into appropriate files and tracks
 * stats.
 */
class DataCollector : public bamql::CompileIterator {
public:
  DataCollector(std::shared_ptr<llvm::ExecutionEngine> &engine,
                std::shared_ptr<bamql::Generator> &generator,
                std::string &query_,
                std::shared_ptr<bamql::AstNode> &node,
                bool verbose_,
                std::shared_ptr<htsFile> &a,
                std::shared_ptr<htsFile> &r)
      : bamql::CompileIterator::CompileIterator(
            engine, generator, node, "filter"),
        accept(a), query(query_), reject(r), verbose(verbose_) {}
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
    auto version = bamql::version();
    uuid_t uuid;
    uuid_generate(uuid);
    char id_buf[sizeof(uuid_t) * 2 + 1];
    uuid_unparse(uuid, id_buf);
    std::string id_str(id_buf);

    if (accept) {
      std::string name("bamql-accept");
      auto copy = bamql::appendProgramToHeader(
          header.get(), name, id_str, version, query);
      if (sam_hdr_write(accept.get(), copy.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
        accept = nullptr;
      }
    }
    if (reject) {
      std::string name("bamql-reject");
      auto copy = bamql::appendProgramToHeader(
          header.get(), name, id_str, version, query);
      if (sam_hdr_write(reject.get(), copy.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
        reject = nullptr;
      }
    }
  }
  void readMatch(bool matches,
                 std::shared_ptr<bam_hdr_t> &header,
                 std::shared_ptr<bam1_t> &read) {
    (matches ? accept_count : reject_count)++;
    std::shared_ptr<htsFile> &chosen = matches ? accept : reject;
    if (chosen) {
      if (sam_write1(chosen.get(), header.get(), read.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
        chosen = nullptr;
      }
    }
    if (verbose && (accept_count + reject_count) % 1000000 == 0) {
      std::cout << "So far, Accepted: " << accept_count
                << " Rejected: " << reject_count << std::endl;
    }
  }
  void handleError(const char *message) {
    if (errors.count(message)) {
      errors[message]++;
    } else {
      errors[message] = 1;
    }
  }
  void writeSummary() {
    std::cout << "Accepted: " << accept_count << std::endl
              << "Rejected: " << reject_count << std::endl;
    for (auto it = errors.begin(); it != errors.end(); it++) {
      std::cout << it->first << " (Occurred " << it->second << " times)"
                << std::endl;
    }
  }

private:
  std::shared_ptr<htsFile> accept;
  size_t accept_count = 0;
  std::map<const char *, size_t> errors;
  std::string query;
  std::shared_ptr<htsFile> reject;
  size_t reject_count = 0;
  bool verbose;
};

/**
 * Use LLVM to compile a query, JIT it, and run it over a BAM file.
 */
int main(int argc, char *const *argv) {
  std::shared_ptr<htsFile> accept; // The file where reads matching the query
                                   // will be placed.
  std::shared_ptr<htsFile> reject; // The file where reads not matching the
                                   // query will be placed.
  char *bam_filename = nullptr;
  char *query_filename = nullptr;
  bool binary = false;
  bool help = false;
  bool verbose = false;
  bool ignore_index = false;
  int c;

  while ((c = getopt(argc, argv, "bhf:Io:O:q:v")) != -1) {
    switch (c) {
    case 'b':
      binary = true;
      break;
    case 'h':
      help = true;
      break;
    case 'f':
      bam_filename = optarg;
      break;
    case 'I':
      ignore_index = true;
      break;
    case 'o':
      accept = bamql::open(optarg, "wb");
      if (!accept) {
        perror(optarg);
        return 1;
      }
      break;
    case 'O':
      reject = bamql::open(optarg, "wb");
      if (!reject) {
        perror(optarg);
      }
      break;
    case 'q':
      if (query_filename != nullptr) {
        std::cerr << "Query file already specified." << std::endl;
        return 1;
      }
      query_filename = optarg;
      break;
    case 'v':
      verbose = true;
      break;
    case '?':
      fprintf(stderr, "Option -%c is not valid.\n", optopt);
      return 1;
    default:
      abort();
    }
  }
  if (help) {
    std::cout
        << argv[0]
        << " [-b] [-I] [-o accepted_reads.bam] [-O "
           "rejected_reads.bam] [-v] -f input.bam {query | -q query.bamql}"
        << std::endl;
    std::cout << "Filter a BAM/SAM file based on the provided query. For "
                 "details, see the man page." << std::endl;
    std::cout << "\t-b\tThe input file is binary (BAM) not text (SAM)."
              << std::endl;
    std::cout << "\t-f\tThe input file to read." << std::endl;
    std::cout << "\t-I\tDo not use the index, even if it exists." << std::endl;
    std::cout << "\t-o\tThe output file for reads that pass the query."
              << std::endl;
    std::cout << "\t-O\tThe output file for reads that fail the query."
              << std::endl;
    std::cout << "\t-q\tA file containing the query, instead of providing it "
                 "on the command line." << std::endl;
    std::cout << "\t-v\tPrint some information along the way." << std::endl;
    return 0;
  }

  if (query_filename == nullptr) {
    if (argc - optind != 1) {
      std::cout << "Need a query." << std::endl;
      return 1;
    }
  } else {
    if (argc - optind != 0) {
      std::cout << "No query can be provided if a query file is given."
                << std::endl;
      return 1;
    }
  }
  if (bam_filename == nullptr) {
    std::cout << "Need an input file." << std::endl;
    return 1;
  }

  std::string query_content;
  if (query_filename == nullptr) {
    query_content = std::string(argv[optind]);
  } else {
    std::ifstream input_file(query_filename);
    if (!input_file) {
      std::cerr << query_filename << ": " << strerror(errno) << std::endl;
      return 1;
    }
    query_content = std::string((std::istreambuf_iterator<char>(input_file)),
                                std::istreambuf_iterator<char>());
  }
  // Parse the input query.
  bamql::PredicateMap predicates = bamql::getDefaultPredicates();
  auto ast = bamql::AstNode::parseWithLogging(query_content, predicates);
  if (!ast) {
    return 1;
  }

  // Create a new LLVM module and our function
  LLVMInitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module(new llvm::Module("bamql", context));

  auto generator = std::make_shared<bamql::Generator>(module.get(), nullptr);

  auto engine = bamql::createEngine(std::move(module));
  if (!engine) {
    std::cerr << "Failed to initialise LLVM." << std::endl;
    return 1;
  }

  // Process the input file.
  DataCollector stats(
      engine, generator, query_content, ast, verbose, accept, reject);
  generator = nullptr;
  engine->finalizeObject();
  engine->runStaticConstructorsDestructors(false);
  stats.prepareExecution();

  if (stats.processFile(bam_filename, binary, ignore_index)) {
    stats.writeSummary();
    engine->runStaticConstructorsDestructors(true);
    return 0;
  } else {
    engine->runStaticConstructorsDestructors(true);
    return 1;
  }
}
