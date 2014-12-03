#include <unistd.h>
#include <iostream>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include "barf.hpp"

int main(int argc, char *const *argv) {
	const char *name = "barf";
	char *output = nullptr;
	bool help = false;
	int c;

	while ((c = getopt (argc, argv, "hn:o:")) != -1) {
		switch (c)
		{
		case 'n':
			name = optarg;
			break;
		case 'o':
			output = optarg;
			break;
		case '?':
			fprintf (stderr, "Option -%c is not valid.\n", optopt);
			return 1;
		default:
			abort ();
		}
	}
	if (help) {
		std::cout << "This is where the help goes." << std::endl;
		return 0;
	}

	if (argc - optind != 1) {
		std::cout << "Need a query." << std::endl;
		return 1;
	}

	auto module = new llvm::Module(name, llvm::getGlobalContext());

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
		return 1;
	}

	std::stringstream output_filename;
	if (output == nullptr) {
		output_filename << name << ".bc";
	} else {
		output_filename << output;
	}
	std::filebuf fb;
	fb.open(output_file.str(), std::ios::out);
	std::ostream out_data(&fb);
	llvm::WriteBitcodeToFile(module, out_data);
	fb.close();
	return 0;
}
