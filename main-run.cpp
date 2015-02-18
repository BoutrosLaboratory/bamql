#include <unistd.h>
#include <iostream>
#include <sys/stat.h>
#include "barf.hpp"
#include "barf-runtime.hpp"

// vim: set ts=2 sw=2 tw=0 :

class data_collector {
public:
	data_collector(barf::filter_function f, bool verbose_)
			: filter(f), verbose(verbose_) {}
	void process_read(bam_hdr_t *header,
										bam1_t *read,
										htsFile *accept,
										htsFile *reject) {
		if (filter(header, read)) {
			accept_count++;
			if (accept != nullptr)
				sam_write1(accept, header, read);
		} else {
			reject_count++;
			if (reject != nullptr)
				sam_write1(reject, header, read);
		}
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
	barf::filter_function filter;
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
	std::shared_ptr<barf::ast_node> ast;
	try {
		ast = barf::ast_node::parse(std::string(query_text),
																barf::getDefaultPredicates());
	}
	catch (barf::parse_error e) {
		std::cerr << "Error: " << e.what() << std::endl << query_text << std::endl;
		for (auto i = 0; i < e.where(); i++) {
			std::cerr << " ";
		}
		std::cerr << "^" << std::endl;
		return 1;
	}

	// Create a new LLVM module and our function
	LLVMInitializeNativeTarget();
	auto module = new llvm::Module("barf", llvm::getGlobalContext());

	auto filter_func = ast->create_filter_function(module, "filter");

	// Create a JIT and convert the generated code to a callable function pointer.
	std::string error;
	std::vector<std::string> attrs;
	attrs.push_back("-avx"); // The AVX support (auto-vectoring) should be
													 // disabled since the JIT isn't smart enough to
													 // detect this, even though there is a detection
													 // routine.
	std::shared_ptr<llvm::ExecutionEngine> engine(
			llvm::EngineBuilder(module)
					.setEngineKind(llvm::EngineKind::JIT)
					.setErrorStr(&error)
					.setMAttrs(attrs)
					.create());
	if (!engine) {
		std::cout << error << std::endl;
		return 1;
	}

	auto filter =
			barf::getNativeFunction<barf::filter_function>(engine, filter_func);

	// Open the input file.
	std::shared_ptr<htsFile> input(hts_open(bam_filename, binary ? "rb" : "r"),
																 hts_close);
	if (!input) {
		perror(argv[optind]);
		return 1;
	}

	// Copy the header to the output.
	std::shared_ptr<bam_hdr_t> header(sam_hdr_read(input.get()), bam_hdr_destroy);
	if (accept)
		sam_hdr_write(accept.get(), header.get());
	if (reject)
		sam_hdr_write(reject.get(), header.get());

	data_collector stats(filter, verbose);
	// Decide if we can use an index.
	std::shared_ptr<hts_idx_t> index(
			ignore_index ? nullptr : hts_idx_load(bam_filename, HTS_FMT_BAI),
			hts_idx_destroy);
	if (index) {
		auto index_func = barf::getNativeFunction<barf::index_function>(
				engine, ast->create_index_function(module, "index"));
		// Use the index to seek chomosome of interest.
		std::shared_ptr<bam1_t> read(bam_init1(), bam_destroy1);
		for (auto tid = 0; tid < header->n_targets; tid++) {
			if (!index_func(header.get(), tid)) {
				continue;
			}
			std::shared_ptr<hts_itr_t> itr(
					bam_itr_queryi(index.get(), tid, 0, INT_MAX), hts_itr_destroy);
			while (bam_itr_next(input.get(), itr.get(), read.get()) >= 0) {
				stats.process_read(
						header.get(), read.get(), accept.get(), reject.get());
			}
		}
		stats.write_summary();
		return 0;
	}

	// Cycle through all the reads.
	std::shared_ptr<bam1_t> read(bam_init1(), bam_destroy1);
	while (sam_read1(input.get(), header.get(), read.get()) >= 0) {
		stats.process_read(header.get(), read.get(), accept.get(), reject.get());
	}
	stats.write_summary();
	return 0;
}
