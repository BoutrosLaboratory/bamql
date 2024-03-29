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

#include "bamql-compiler.hpp"
#include "bamql-jit.hpp"
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

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

bool checkChain(ChainPattern chain, bool matches) {
  return chain & (1 << matches);
}

/**
 * One link of a chain. It checks the filter, writes matching reads to a file,
 * and propagates the read to the next link in the chain.
 */
class OutputWrangler final : public bamql::CompileIterator {
public:
  OutputWrangler(

      std::shared_ptr<bamql::CompiledPredicate> predicate,
      std::string query_,
      ChainPattern c,
      std::string file_name_,
      std::shared_ptr<htsFile> &o,
      std::shared_ptr<OutputWrangler> &n,
      std::shared_ptr<std::map<const char *, size_t>> &errors_)
      : bamql::CompileIterator::CompileIterator(predicate), chain(c),
        errors(errors_), file_name(file_name_), output_file(o), next(n),
        query(query_) {}
  /**
   * We want this chromosome if our query is interested or the next link can
   * make use of it _if_ it will see it upon failure (otherwise, its behaviour
   * is determined by ours.
   */
  bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid) {
    return CompileIterator::wantChromosome(header, tid) ||
           (next && checkChain(chain, false) &&
            next->wantChromosome(header, tid));
  }

  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
    auto version = bamql::version();
    std::stringstream name;
    name << "bamql-chain ";
    for (auto chains = known_chains.begin(); chains != known_chains.end();
         chains++) {
      if (chains->second == chain) {
        name << chains->first;
        break;
      }
    }

    auto id_str = bamql::makeUuid();
    auto copy = bamql::appendProgramToHeader(header.get(), name.str(), id_str,
                                             version, query);
    if (output_file) {
      if (sam_hdr_write(output_file.get(), copy.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
        output_file = nullptr;
      }
    }
    if (next)
      next->ingestHeader(chain == 3 ? header : copy);
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
      if (output_file) {
        if (sam_write1(output_file.get(), header.get(), read.get()) == -1) {
          std::cerr << "Error writing to output BAM. Giving up on file."
                    << std::endl;
          output_file = nullptr;
        }
      }
    }
    if (next && checkChain(chain, matches)) {
      next->processRead(header, read);
    }
  }

  void handleError(const char *message) {
    if (errors->count(message)) {
      (*errors)[message]++;
    } else {
      (*errors)[message] = 1;
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
  size_t count = 0;
  std::shared_ptr<std::map<const char *, size_t>> errors;
  std::string file_name;
  std::shared_ptr<htsFile> output_file;
  std::shared_ptr<OutputWrangler> next;
  std::string query;
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
    std::cout << argv[0]
              << " [-b] [-c] [-I] [-v] -f input.bam "
                 " query1 output1.bam ..."
              << std::endl;
    std::cout << "Filter a BAM/SAM file based on the provided query. For "
                 "details, see the man page."
              << std::endl;
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
  auto jit = bamql::JIT::create();

  // Prepare a chain of wranglers.
  std::shared_ptr<OutputWrangler> output;
  auto errors = std::make_shared<std::map<const char *, size_t>>();
  for (auto it = argc - 2; it >= optind; it -= 2) {
    // Prepare the output file.
    std::shared_ptr<htsFile> output_file;
    if (strcmp("-", argv[it + 1]) != 0) {
      output_file =
          std::shared_ptr<htsFile>(hts_open(argv[it + 1], "wb"), hts_close);
      if (!output_file) {
        perror(optarg);
        return 1;
      }
    }
    // Parse the input query.
    std::string query(argv[it]);
    bamql::PredicateMap predicates = bamql::getDefaultPredicates();
    auto ast = bamql::AstNode::parseWithLogging(query, predicates);
    if (!ast) {
      return 1;
    }

    // Add the link to the chain.
    std::stringstream function_name;
    function_name << "filter" << it;
    output = std::make_shared<OutputWrangler>(
        bamql::JIT::compile(jit, ast, function_name.str()), query, chain,
        std::string(argv[it + 1]), output_file, output, errors);
  }

  // Run the chain.
  int exitcode;
  if (output->processFile(input_filename, binary, ignore_index)) {
    output->write_summary();
    exitcode = 0;
  } else {
    exitcode = 1;
  }
  for (auto it = errors->begin(); it != errors->end(); it++) {
    std::cout << it->first << " (Occurred " << it->second << " times)"
              << std::endl;
  }

  return exitcode;
}
