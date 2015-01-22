#include <unistd.h>
#include <iostream>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <sys/stat.h>
#include "barf.hpp"

#define hts_close0(x) if (x != nullptr) x = (hts_close(x), nullptr)
typedef bool (*filter_function)(bam_hdr_t*, bam1_t*);

class data_collector {
public:
data_collector(filter_function f, bool verbose_) : filter(f), verbose(verbose_) {
}
void process_read(bam_hdr_t *header, bam1_t *read, htsFile *accept, htsFile *reject) {
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
		std::cout << "So far, Accepted: " << accept_count <<  " Rejected: " << reject_count << std::endl;
	}
}
void write_summary() {
	std::cout << "Accepted: " << accept_count << std::endl << "Rejected: " << reject_count << std::endl;
}
private:
filter_function filter;
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
bool figure_out_arguments(char *const *args, char **out_file, char **out_query) {
	struct stat buf;
	if (stat(args[0], &buf) == 0) {
		*out_file  = args[0];
		*out_query  = args[1];
		return true;
	}
	if (stat(args[1], &buf) == 0) {
		*out_file  = args[1];
		*out_query  = args[0];
		return true;
	}
	return false;
}

/**
 * Use LLVM to compile a query, JIT it, and run it over a BAM file.
 */
int main(int argc, char *const *argv) {
	htsFile *input = nullptr; // The file holding the BAM reads to be filtered.
	htsFile *accept = nullptr; // The file where reads matching the query will be placed.
	htsFile *reject = nullptr; // The file where reads not matching the query will be placed.
	bool binary = false;
	bool help = false;
	bool verbose = false;
	int c;

	while ((c = getopt (argc, argv, "bho:O:v")) != -1) {
		switch (c)
		{
		case 'b':
			binary = true;
			break;
		case 'h':
			help = true;
			break;
		case 'o':
			accept = hts_open(optarg, "wb");
			if (accept == nullptr) {
				perror(optarg);
				hts_close0(reject);
			}
			break;
		case 'O':
			reject = hts_open(optarg, "wb");
			if (reject == nullptr) {
				perror(optarg);
				hts_close0(reject);
			}
			break;
		case 'v':
			verbose = true;
			break;
		case '?':
			fprintf (stderr, "Option -%c is not valid.\n", optopt);
			hts_close0(accept);
			hts_close0(reject);
			return 1;
		default:
			abort ();
		}
	}
	if (help) {
		std::cout << argv[0] << " [-b] [-o accepted_reads.bam] [-O rejected_reads.bam] input.bam query" << std::endl;
		std::cout << "Filter a BAM/SAM file based on the provided query. For details, see the man page." << std::endl;
		std::cout << "\t-b\tThe input file is binary (BAM) not text (SAM)." << std::endl;
		std::cout << "\t-o\tThe output file for reads that pass the query." << std::endl;
		std::cout << "\t-O\tThe output file for reads that fail the query." << std::endl;
		hts_close0(accept);
		hts_close0(reject);
		return 0;
	}

	if (argc - optind != 2) {
		std::cout << "Need a query and a BAM file." << std::endl;

		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	char *query_text;
	char *bam_filename;
	if (!figure_out_arguments(argv + optind, &bam_filename, &query_text)) {
		std::cerr << "The file supplied does not exist." << std::endl;

		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	// Parse the input query.
	std::shared_ptr<barf::ast_node> ast;
	try {
		ast = barf::ast_node::parse(std::string(query_text), barf::getDefaultPredicates());
	} catch (barf::parse_error e) {
		std::cerr << "Error: " << e.what() << std::endl << query_text << std::endl;
		for(auto i = 0; i < e.where(); i++) {
			std::cerr << " ";
		}
		std::cerr << "^" << std::endl;
		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	// Create a new LLVM module and our function
	LLVMInitializeNativeTarget();
	auto module = new llvm::Module("barf", llvm::getGlobalContext());

	auto filter_func = ast->create_filter_function(module, "filter");

	// Create a JIT and convert the generated code to a callable function pointer.
	std::string error;
	std::vector<std::string> attrs;
	attrs.push_back("-avx"); // The AVX support (auto-vectoring) should be disabled since the JIT isn't smart enough to detect this, even though there is a detection routine.
	auto engine = llvm::EngineBuilder(module).setEngineKind(llvm::EngineKind::JIT).setErrorStr(&error).setMAttrs(attrs).create();
	if (engine == NULL) {
		std::cout << error << std::endl;
		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	union {
		filter_function func;
		void *ptr;
	} result = { NULL };
	result.ptr = engine->getPointerToFunction(filter_func);

	// Open the input file.
	input = hts_open(bam_filename, binary ? "rb" : "r");
	if (input == nullptr) {
		perror(argv[optind]);
		delete engine;
		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	// Copy the header to the output.
	bam_hdr_t *header = sam_hdr_read(input);
	if (accept != nullptr )
		sam_hdr_write(accept, header);
	if (reject != nullptr )
		sam_hdr_write(reject, header);

	// Cycle through all the reads.
	bam1_t *read = bam_init1();
	while(sam_read1(input, header, read) >= 0) {
		stats.process_read(header, read, accept, reject);
	}
	stats.write_summary();
	bam_destroy1(read);

	hts_close0(input);
	hts_close0(accept);
	hts_close0(reject);
	bam_hdr_destroy(header);

	delete engine;

	return 0;
}
