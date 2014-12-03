#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <llvm/IR/Module.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/raw_ostream.h>
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

	auto entry = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", filter_func);
	llvm::IRBuilder<> builder(entry);
	auto args = filter_func->arg_begin();
	auto header_value = args++;
	header_value->setName("header");
	auto read_value = args++;
	read_value->setName("read");
	builder.CreateRet(ast->generate(module, builder, header_value, read_value));

	std::stringstream output_filename;
	if (output == nullptr) {
		output_filename << name << ".bc";
	} else {
		output_filename << output;
	}
	std::string error;
	llvm::raw_fd_ostream out_data(output_filename.str().c_str(), error);
	if (error.length() > 0) {
		std::cerr << error << std::endl;
	} else {
		llvm::WriteBitcodeToFile(module, out_data);
	}
	return 0;
}
