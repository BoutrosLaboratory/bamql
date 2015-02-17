#include <unistd.h>
#include <iostream>
#include <sstream>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <sys/stat.h>
#include "barf.hpp"

// vim: set ts=2 sw=2 tw=0 :

typedef bool (*filter_function)(bam_hdr_t *, bam1_t *);
typedef bool (*index_function)(bam_hdr_t *, uint32_t);

typedef bool (*chain_function)(bool);

bool parallel_chain(bool x) {
	return true;
}
bool series_chain(bool x) {
	return x;
}
bool shuttle_chain(bool x) {
	return !x;
}

std::map<std::string, chain_function> known_chains = {
	{ "parallel", parallel_chain },
	{ "series", series_chain },
	{ "shuttle", shuttle_chain }
};

class output_wrangler {
public:
		output_wrangler(chain_function c, std::shared_ptr<htsFile> o, filter_function f, index_function i, std::shared_ptr<output_wrangler> n) : chain(c), output_file(o), filter(f), index(i), next(n) {}

	bool index_interest(bam_hdr_t *header, uint32_t tid) {
		return index(header, tid) || next && chain(false) && next->index_interest(header, tid);
	}

	void process_read(bam_hdr_t *header,
										bam1_t *read) {
		bool matches = filter(header, read);
		if (matches) {
			accept_count++;
			sam_write1(output_file.get(), header, read);
		} else {
			reject_count++;
		}
		if (next && chain(matches)) {
			next->process_read(header, read);
		}
	}

	void write_summary() {
		std::cout << "Accepted: " << accept_count << std::endl
							<< "Rejected: " << reject_count << std::endl;
		if (next) {
			next->write_summary();
		}
	}

private:
	chain_function chain;
	std::shared_ptr<htsFile> output_file;
	filter_function filter;
	index_function index;
	std::shared_ptr<output_wrangler> next;
	size_t accept_count = 0;
	size_t reject_count = 0;
};

/**
 * Use LLVM to compile a query, JIT it, and run it over a BAM file.
 */
int main(int argc, char *const *argv) {
	const char *input_filename = nullptr;
	bool binary = false;
	chain_function chain = parallel_chain;
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
		std::cout << "\t-c\tChain the queries, rather than use them independently." << std::endl;
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
	std::shared_ptr<htsFile> input = std::shared_ptr<htsFile>(hts_open(input_filename, binary ? "rb" : "r"), hts_close);
	if (!input) {
		perror(optarg);
		return 1;
	}

	// Try to open the index.
	std::shared_ptr<hts_idx_t> index(
			ignore_index ? nullptr : hts_idx_load(input_filename, HTS_FMT_BAI),
			hts_idx_destroy);

	// Create a new LLVM module and JIT
	LLVMInitializeNativeTarget();
	auto module = new llvm::Module("barf", llvm::getGlobalContext());
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

	std::shared_ptr<output_wrangler> output;
	for(auto it = argc - 2; it >= optind; it -= 2) {
			auto output_file = std::shared_ptr<htsFile>(hts_open(argv[it + 1], "wb"), hts_close);
			if (!output_file) {
				perror(optarg);
				return 1;
			}
		// Parse the input query.
		std::shared_ptr<barf::ast_node> ast;
		try {
			ast = barf::ast_node::parse(std::string(argv[it]),
																	barf::getDefaultPredicates());
		}
		catch (barf::parse_error e) {
			std::cerr << "Error: " << e.what() << std::endl << argv[it] << std::endl;
			for (auto i = 0; i < e.where(); i++) {
				std::cerr << " ";
			}
			std::cerr << "^" << std::endl;
			return 1;
		}

		std::stringstream function_name;
		function_name << "filter" << it;
		auto filter_func = ast->create_filter_function(module, function_name.str());

		union {
			filter_function func;
			void *ptr;
		} result = { NULL };
		result.ptr = engine->getPointerToFunction(filter_func);

		union {
			index_function func;
			void *ptr;
		} index_result = { NULL };
		if (index) {
			std::stringstream index_function_name;
			index_function_name << "index" << it;
			auto index_func = ast->create_index_function(module, index_function_name.str());
			index_result.ptr = engine->getPointerToFunction(index_func);
		}

		output = std::make_shared<output_wrangler>(chain, output_file, result.func, index_result.func, output);
	}

	// Copy the header to the output.
	std::shared_ptr<bam_hdr_t> header(sam_hdr_read(input.get()), bam_hdr_destroy);

	if (index) {
		// Use the index to seek chomosome of interest.
		std::shared_ptr<bam1_t> read(bam_init1(), bam_destroy1);
		for (auto tid = 0; tid < header->n_targets; tid++) {
			if (!output->index_interest(header.get(), tid)) {
				continue;
			}
			std::shared_ptr<hts_itr_t> itr(
					bam_itr_queryi(index.get(), tid, 0, INT_MAX), hts_itr_destroy);
			while (bam_itr_next(input.get(), itr.get(), read.get()) >= 0) {
				output->process_read(header.get(), read.get());
			}
		}
		output->write_summary();
		return 0;
		
	}

	// Cycle through all the reads.
	std::shared_ptr<bam1_t> read(bam_init1(), bam_destroy1);
	while (sam_read1(input.get(), header.get(), read.get()) >= 0) {
		output->process_read(header.get(), read.get());
	}
	output->write_summary();
	return 0;
}
