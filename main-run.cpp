#include <unistd.h>
#include <iostream>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include "barf.hpp"

#define hts_close0(x) if (x != nullptr) x = (hts_close(x), nullptr)

/**
 * Use LLVM to compile a query, JIT it, and run it over a BAM file.
 */
int main(int argc, char *const *argv) {
	htsFile *input = nullptr; // The file holding the BAM reads to be filtered.
	htsFile *accept = nullptr; // The file where reads matching the query will be placed.
	htsFile *reject = nullptr; // The file where reads not matching the query will be placed.
	bool binary = false;
	bool help = false;
	int c;

	while ((c = getopt (argc, argv, "bho:O:")) != -1) {
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
		std::cout << argv[0] << "[-b] [-o accepted_reads.bam] [-O rejected_reads.bam] query input.bam" << std::endl;
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

	// Create a new LLVM module and our function
	LLVMInitializeNativeTarget();
	auto module = new llvm::Module("barf", llvm::getGlobalContext());

	auto filter_func = llvm::cast<llvm::Function>(module->getOrInsertFunction("filter",llvm::Type::getInt1Ty(llvm::getGlobalContext()), llvm::PointerType::get(barf::getBamHeaderType(module), 0), llvm::PointerType::get(barf::getBamType(module), 0), nullptr));

	// Parse the input query.
	std::shared_ptr<barf::ast_node> ast;
	try {
		ast = barf::ast_node::parse(std::string(argv[optind]), barf::getDefaultPredicates());
	} catch (barf::parse_error e) {
		std::cerr << "Error: " << e.what() << std::endl << argv[optind] << std::endl;
		for(auto i = 0; i < e.where(); i++) {
			std::cerr << " ";
		}
		std::cerr << "^" << std::endl;
		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	// Generate the LLVM code from the query.
	auto entry = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", filter_func);
	llvm::IRBuilder<> builder(entry);
	auto args = filter_func->arg_begin();
	auto header_value = args++;
	header_value->setName("header");
	auto read_value = args++;
	read_value->setName("read");
	builder.CreateRet(ast->generate(module, builder, header_value, read_value));

	// Create a JIT and convert the generated code to a callable function pointer.
	std::string error;
	auto engine = llvm::EngineBuilder(module).setEngineKind(llvm::EngineKind::JIT).setErrorStr(&error).create();
	if (engine == NULL) {
		std::cout << error << std::endl;
		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	union {
		bool (*func)(bam_hdr_t*, bam1_t*);
		void *ptr;
	} result = { NULL };
	result.ptr = engine->getPointerToFunction(filter_func);

	// Open the input file.
	input = hts_open(argv[optind + 1], binary ? "rb" : "r");
	if (input == nullptr) {
		perror(argv[optind + 1]);
		delete engine;
		hts_close0(accept);
		hts_close0(reject);
		return 1;
	}

	size_t accept_count = 0;
	size_t reject_count = 0;

	// Copy the header to the output.
	bam_hdr_t *header = sam_hdr_read(input);
	if (accept != nullptr )
		sam_hdr_write(accept, header);
	if (reject != nullptr )
		sam_hdr_write(reject, header);

	// Cycle through all the reads.
	bam1_t *read = bam_init1();
	while(sam_read1(input, header, read) == 0) {
		if ((*result.func)(header, read)) {
			accept_count++;
			if (accept != nullptr)
				sam_write1(accept, header, read);
		} else {
			reject_count++;
			if (reject != nullptr)
				sam_write1(reject, header, read);
		}
	}
	bam_destroy1(read);

	hts_close0(input);
	hts_close0(accept);
	hts_close0(reject);

	std::cout << "Accepted: " << accept_count << std::endl << "Rejected: " << reject_count << std::endl;

	return 0;
}
