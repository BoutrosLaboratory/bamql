#include <unistd.h>
#include <iostream>
#include <sys/stat.h>
#include "barf.hpp"
#include "barf-jit.hpp"

// vim: set ts=2 sw=2 tw=0 :

class data_collector : public barf::check_iterator {
public:
	data_collector(std::shared_ptr<llvm::ExecutionEngine> &engine,
								 llvm::Module *module,
								 std::shared_ptr<barf::ast_node> &node,
								 bool verbose_,
								 std::shared_ptr<htsFile> &a,
								 std::shared_ptr<htsFile> &r)
			: barf::check_iterator::check_iterator(
						engine, module, node, std::string("filter")),
				verbose(verbose_), accept(a), reject(r) {}
	void ingestHeader(std::shared_ptr<bam_hdr_t> &header) {
		if (accept)
			sam_hdr_write(accept.get(), header.get());
		if (reject)
			sam_hdr_write(reject.get(), header.get());
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
	void write_summary() {
		std::cout << "Accepted: " << accept_count << std::endl
							<< "Rejected: " << reject_count << std::endl;
	}

private:
	std::shared_ptr<htsFile> accept;
	std::shared_ptr<htsFile> reject;
	size_t accept_count = 0;
	size_t reject_count = 0;
	bool verbose;
};
/**
 * We are given two arguments on the command line, which our user is almost
 * certainly likely to get wrong. Rather than force them to do the USB plug
 * thing of flipping it over three times, figure out which is the file and
 * which is the query.
 */
bool figure_out_arguments(char *const *args,
													char **out_file,
													char **out_query) {
	struct stat buf;
	if (stat(args[0], &buf) == 0) {
		*out_file = args[0];
		*out_query = args[1];
		return true;
	}
	if (stat(args[1], &buf) == 0) {
		*out_file = args[1];
		*out_query = args[0];
		return true;
	}
	return false;
}

/**
 * Use LLVM to compile a query, JIT it, and run it over a BAM file.
 */
int main(int argc, char *const *argv) {
	std::shared_ptr<htsFile> accept; // The file where reads matching the query
																	 // will be placed.
	std::shared_ptr<htsFile> reject; // The file where reads not matching the
																	 // query will be placed.
	bool binary = false;
	bool help = false;
	bool verbose = false;
	bool ignore_index = false;
	int c;

	while ((c = getopt(argc, argv, "bhIo:O:v")) != -1) {
		switch (c) {
		case 'b':
			binary = true;
			break;
		case 'h':
			help = true;
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
														"rejected_reads.bam] [-v] input.bam query"
							<< std::endl;
		std::cout << "Filter a BAM/SAM file based on the provided query. For "
								 "details, see the man page." << std::endl;
		std::cout << "\t-b\tThe input file is binary (BAM) not text (SAM)."
							<< std::endl;
		std::cout << "\t-I\tDo not use the index, even if it exists." << std::endl;
		std::cout << "\t-o\tThe output file for reads that pass the query."
							<< std::endl;
		std::cout << "\t-O\tThe output file for reads that fail the query."
							<< std::endl;
		std::cout << "\t-v\tPrint some information along the way." << std::endl;
		return 0;
	}

	if (argc - optind != 2) {
		std::cout << "Need a query and a BAM file." << std::endl;
		return 1;
	}

	char *query_text;
	char *bam_filename;
	if (!figure_out_arguments(argv + optind, &bam_filename, &query_text)) {
		std::cerr << "The file supplied does not exist." << std::endl;
		return 1;
	}

	// Parse the input query.
	auto ast = barf::ast_node::parse_with_logging(std::string(query_text),
																								barf::getDefaultPredicates());
	if (!ast) {
		return 1;
	}

	// Create a new LLVM module and our function
	LLVMInitializeNativeTarget();
	auto module = new llvm::Module("barf", llvm::getGlobalContext());

	auto engine = barf::createEngine(module);
	if (!engine) {
		return 1;
	}

	data_collector stats(engine, module, ast, verbose, accept, reject);
	if (stats.processFile(bam_filename, binary, ignore_index)) {
		stats.write_summary();
		return 0;
	} else {
		return 1;
	}
}
