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
#include <map>
#include <iostream>
#include <sys/stat.h>
#include "bamql-iterator.hpp"

namespace bamql {

static void error_wrapper(const char *message, void *context);
/**
 * Handler for output collection. Shunts reads into appropriate files and tracks
 * stats.
 */
class DataCollector : public bamql::ReadIterator {
public:
  DataCollector(FilterFunction filter_,
                IndexFunction index_,
                bool verbose_,
                const std::string &h_,
                const std::string &v_,
                std::shared_ptr<htsFile> &a,
                std::shared_ptr<htsFile> &r)
      : accept(a), filter(filter_), header_str(h_), index(index_), reject(r),
        verbose(verbose_), version_str(v_) {}
  void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
    auto id_str = makeUuid();

    if (accept) {
      std::string name("bamql-accept");
      auto copy = bamql::appendProgramToHeader(
          header.get(), name, id_str, version_str, header_str);
      if (sam_hdr_write(accept.get(), copy.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
        accept = nullptr;
      }
    }
    if (reject) {
      std::string name("bamql-reject");
      auto copy = bamql::appendProgramToHeader(
          header.get(), name, id_str, version_str, header_str);
      if (sam_hdr_write(reject.get(), copy.get()) == -1) {
        std::cerr << "Error writing to output BAM. Giving up on file."
                  << std::endl;
        reject = nullptr;
      }
    }
  }
  void processRead(std::shared_ptr<bam_hdr_t> &header,
                   std::shared_ptr<bam1_t> &read) {
    bool matches = filter(header.get(), read.get(), error_wrapper, this);
    (matches ? accept_count : reject_count)++;
    std::shared_ptr<htsFile> &chosen = matches ? accept : reject;
    if (matches) {
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
  bool wantChromosome(std::shared_ptr<bam_hdr_t> &header, uint32_t tid) {
    return index(header.get(), tid, error_wrapper, this);
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
  FilterFunction filter;
  IndexFunction index;
  std::string header_str;
  std::shared_ptr<htsFile> reject;
  size_t reject_count = 0;
  bool verbose;
  std::string version_str;
};

static void error_wrapper(const char *message, void *context) {
  ((bamql::DataCollector *)context)->handleError(message);
}

int main(int argc,
         char *const *argv,
         FilterFunction filter,
         IndexFunction index,
         const std::string &headerName,
         const std::string &version) {
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
                            "rejected_reads.bam] [-v] -f input.bam"
              << std::endl;
    std::cout << "Filter a BAM/SAM file based on the built-in query."
              << std::endl;
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

  if (bam_filename == nullptr) {
    std::cout << "Need an input file." << std::endl;
    return 1;
  }

  // Process the input file.
  DataCollector stats(
      filter, index, verbose, headerName, version, accept, reject);
  if (stats.processFile(bam_filename, binary, ignore_index)) {
    stats.writeSummary();
    return 0;
  } else {
    return 1;
  }
}
}
