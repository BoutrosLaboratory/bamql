#include <unistd.h>
#include <iostream>
#include <htslib/hts.h>
#include <htslib/sam.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "barf.hpp"

#define hts_close0(x) if (x != nullptr) x = (hts_close(x), nullptr)

int main(int argc, char *const *argv) {
	htsFile *input = nullptr;
	htsFile *accept = nullptr;
	htsFile *reject = nullptr;
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
		std::cout << "This is where the help goes." << std::endl;
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

	auto module = new llvm::Module("barf", llvm::getGlobalContext());

	auto filter_func = llvm::cast<llvm::Function>(module->getOrInsertFunction("filter",llvm::Type::getInt1Ty(llvm::getGlobalContext()), llvm::PointerType::get(barf::getBamHeaderType(module), 0), llvm::PointerType::get(barf::getBamType(module), 0), nullptr));

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

	auto engine = llvm::EngineBuilder(module).create();

	union {
		bool (*func)(bam_hdr_t*, bam1_t*);
		void *ptr;
	} result;
	result.ptr = engine->getPointerToFunction(filter_func);

	input = hts_open(argv[optind + 1], binary ? "rb" : "r");

	size_t accept_count = 0;
	size_t reject_count = 0;
	bam_hdr_t *header = sam_hdr_read(input);
	if (accept != nullptr )
		sam_hdr_write(accept, header);
	if (reject != nullptr )
		sam_hdr_write(reject, header);

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

	std::cout << "Accepted: " << accept_count << std::endl << "Rejected: " <<reject_count << std::endl;

	return 0;
}
