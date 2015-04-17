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
#include <iostream>
#include <sys/stat.h>
#include "bamql.hpp"
#include "bamql-jit.hpp"

/**
 * Handler for output collection. Shunts reads into appropriate files and tracks
 * stats.
 */
class DataCollector : public bamql::CheckIterator {
public:
  DataCollector(std::shared_ptr<llvm::ExecutionEngine> &engine,
                std::shared_ptr<bamql::Generator> &generator,
                std::string &query_,
                std::shared_ptr<bamql::AstNode> &node,
                bool verbose_,
                std::shared_ptr<htsFile> &a,
                std::shared_ptr<htsFile> &r)
      : bamql::CheckIterator::CheckIterator(
            engine, generator, node, std::string("filter")),
        query(query_), verbose(verbose_), accept(a), reject(r) {}
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
    auto version = bamql::version();
    if (accept) {
      std::string name("bamql-accept");
      auto copy =
          bamql::appendProgramToHeader(header.get(), name, version, query);
      sam_hdr_write(accept.get(), copy.get());
    }
    if (reject) {
      std::string name("bamql-reject");
      auto copy =
          bamql::appendProgramToHeader(header.get(), name, version, query);
      sam_hdr_write(reject.get(), copy.get());
    }
  }
  void readMatch(bool matches,
                 std::shared_ptr<bam_hdr_t> &header,
                 std::shared_ptr<bam1_t> &read) {
    (matches ? accept_count : reject_count)++;
    std::shared_ptr<htsFile> &chosen = matches ? accept : reject;
    if (chosen)
      sam_write1(chosen.get(), header.get(), read.get());
    if (verbose && (accept_count + reject_count) % 1000000 == 0) {
      std::cout << "So far, Accepted: " << accept_count
                << " Rejected: " << reject_count << std::endl;
    }
  }
  void writeSummary() {
    std::cout << "Accepted: " << accept_count << std::endl
              << "Rejected: " << reject_count << std::endl;
  }

private:
  std::shared_ptr<htsFile> accept;
  std::shared_ptr<htsFile> reject;
  size_t accept_count = 0;
  size_t reject_count = 0;
  std::string query;
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
  bool binary = false;
  bool help = false;
  bool verbose = false;
  bool ignore_index = false;
  int c;

  while ((c = getopt(argc, argv, "bhf:Io:O:v")) != -1) {
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
      accept = std::shared_ptr<htsFile>(hts_open(optarg, "wb"), hts_close);
      if (!accept) {
        perror(optarg);
        return 1;
      }
      break;
    case 'O':
      reject = std::shared_ptr<htsFile>(hts_open(optarg, "wb"), hts_close);
      if (!reject) {
        perror(optarg);
      }
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
    std::cout << argv[0] << " [-b] [-I] [-o accepted_reads.bam] [-O "
                            "rejected_reads.bam] [-v] -f input.bam query"
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
    std::cout << "\t-v\tPrint some information along the way." << std::endl;
    return 0;
  }

  if (argc - optind != 1) {
    std::cout << "Need a query." << std::endl;
    return 1;
  }
  if (bam_filename == nullptr) {
    std::cout << "Need an input file." << std::endl;
    return 1;
  }

  // Parse the input query.
  auto ast = bamql::AstNode::parseWithLogging(std::string(argv[optind]),
                                              bamql::getDefaultPredicates());
  if (!ast) {
    return 1;
  }

  // Create a new LLVM module and our function
  LLVMInitializeNativeTarget();
  auto module = new llvm::Module("bamql", llvm::getGlobalContext());

  auto engine = bamql::createEngine(module);
  if (!engine) {
    return 1;
  }

  // Process the input file.
  std::string query(argv[optind]);
  auto generator = std::make_shared<bamql::Generator>(module, nullptr);
  DataCollector stats(engine, generator, query, ast, verbose, accept, reject);
  if (stats.processFile(bam_filename, binary, ignore_index)) {
    stats.writeSummary();
    return 0;
  } else {
    return 1;
  }
}
