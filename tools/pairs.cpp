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
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include "bamql-compiler.hpp"
#include "bamql-jit.hpp"

/**
 * Handler for output collection. Shunts reads into appropriate files and tracks
 * stats.
 */
class PairCollector : public bamql::CompileIterator {
public:
  PairCollector(std::shared_ptr<llvm::ExecutionEngine> &engine,
                std::shared_ptr<bamql::Generator> &generator,
                std::shared_ptr<bamql::AstNode> &node,
                std::set<std::string> &matched_,
                std::set<uint32_t> &matched_tids_)
      : bamql::CompileIterator::CompileIterator(
            engine, generator, node, "filter"),
        matched(matched_), matched_tids(matched_tids_) {}
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {}
  void readMatch(bool matches,
                 std::shared_ptr<bam_hdr_t> &header,
                 std::shared_ptr<bam1_t> &read) {
    if (matches) {
      matched.insert(bam_get_qname(read.get()));
      matched_tids.insert(read->core.tid);
      matched_tids.insert(read->core.mtid);
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
    for (auto it = errors.begin(); it != errors.end(); it++) {
      std::cout << it->first << " (Occurred " << it->second << " times)"
                << std::endl;
    }
  }

private:
  std::map<const char *, size_t> errors;
  std::set<std::string> &matched;
  std::set<uint32_t> &matched_tids;
};
class OutputPairs : public bamql::ReadIterator {
public:
  OutputPairs(

      std::set<std::string> &matched_,
      std::set<uint32_t> &matched_tids_,
      std::string &query_,
      std::shared_ptr<htsFile> &output_)
      : matched(matched_), matched_tids(matched_tids_), query(query_),
        output(output_) {}
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
    auto version = bamql::version();
    auto id_str = bamql::makeUuid();

    std::string name("bamql-pairs");
    auto copy = bamql::appendProgramToHeader(
        header.get(), name, id_str, version, query);
    if (sam_hdr_write(output.get(), copy.get()) == -1) {
      std::cerr << "Error writing to output BAM. Giving up on file."
                << std::endl;
    }
  }
  bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid) {
    return matched_tids.find(tid) != matched_tids.end();
  }
  void processRead(std::shared_ptr<bam_hdr_t> &header,
                   std::shared_ptr<bam1_t> &read) {
    if (matched.find(bam_get_qname(read.get())) != matched.end()) {
      if (sam_write1(output.get(), header.get(), read.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
      }
    }
  }

private:
  std::set<std::string> &matched;
  std::set<uint32_t> &matched_tids;
  std::string query;
  std::shared_ptr<htsFile> output;
};

/**
 * Use LLVM to compile a query, JIT it, and run it over a BAM file.
 */
int main(int argc, char *const *argv) {
  std::shared_ptr<htsFile> output; // The file where reads matching the query
                                   // will be placed.
  char *bam_filename = nullptr;
  char *query_filename = nullptr;
  bool binary = false;
  bool help = false;
  bool ignore_index = false;
  int c;

  while ((c = getopt(argc, argv, "bhf:Io:q:")) != -1) {
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
      output = bamql::open(optarg, "wb");
      if (!output) {
        perror(optarg);
        return 1;
      }
      break;
    case 'q':
      if (query_filename != nullptr) {
        std::cerr << "Query file already specified." << std::endl;
        return 1;
      }
      query_filename = optarg;
      break;
    case '?':
      fprintf(stderr, "Option -%c is not valid.\n", optopt);
      return 1;
    default:
      abort();
    }
  }
  if (help) {
    std::cout << argv[0] << " [-b] [-I] [-o accepted_pairs.bam] -f input.bam "
                            "{query | -q query.bamql}" << std::endl;
    std::cout << "Filter a BAM/SAM file based on the provided query and keep "
                 "read pairs if either is accepted. For "
                 "details, see the man page." << std::endl;
    std::cout << "\t-b\tThe input file is binary (BAM) not text (SAM)."
              << std::endl;
    std::cout << "\t-f\tThe input file to read." << std::endl;
    std::cout << "\t-I\tDo not use the index, even if it exists." << std::endl;
    std::cout << "\t-o\tThe output file for read pairs that pass the query."
              << std::endl;
    std::cout << "\t-q\tA file containing the query, instead of providing it "
                 "on the command line." << std::endl;
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
  if (output == nullptr) {
    std::cout << "Need an output file." << std::endl;
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
  std::set<std::string> matched;
  std::set<uint32_t> matched_tids;

  PairCollector collectNames(engine, generator, ast, matched, matched_tids);
  generator = nullptr;
  engine->finalizeObject();
  engine->runStaticConstructorsDestructors(false);
  collectNames.prepareExecution();

  if (!collectNames.processFile(bam_filename, binary, ignore_index)) {
    engine->runStaticConstructorsDestructors(true);
    return 1;
  }
  collectNames.writeSummary();
  OutputPairs matchNames(matched, matched_tids, query_content, output);
  if (!matchNames.processFile(bam_filename, binary, ignore_index)) {
    engine->runStaticConstructorsDestructors(true);
    return 1;
  }

  engine->runStaticConstructorsDestructors(true);
  return 0;
}
