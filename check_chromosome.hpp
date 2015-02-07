#include <htslib/sam.h>
#include "barf.hpp"

// vim: set ts=2 sw=2 tw=0 :

namespace barf {

/**
 * A predicate that checks of the chromosome name.
 */
template <bool mate> class check_chromosome_node : public ast_node {
public:
	check_chromosome_node(std::string name_) : name(name_) {}
	virtual llvm::Value *generate(llvm::Module *module,
																llvm::IRBuilder<> &builder,
																llvm::Value *read,
																llvm::Value *header) {
		auto function = module->getFunction("check_chromosome");
		return builder.CreateCall4(
				function,
				read,
				header,
				createString(module, name),
				mate ? llvm::ConstantInt::getTrue(llvm::getGlobalContext())
						 : llvm::ConstantInt::getFalse(llvm::getGlobalContext()));
	}
	virtual llvm::Value *generate_index(llvm::Module *module,
																			llvm::IRBuilder<> &builder,
																			llvm::Value *chromosome,
																			llvm::Value *header) {
		if (mate) {
			return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
		}
		auto function = module->getFunction("check_chromosome_id");
		return builder.CreateCall3(
				function, chromosome, header, createString(module, name));
	}

	static std::shared_ptr<ast_node> parse(const std::string &input,
																				 size_t &index) throw(parse_error) {
		parse_char_in_space(input, index, '(');

		auto str = parse_str(
				input,
				index,
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_*?.");
		if (str.compare(0, 3, "chr") == 0) {
			throw parse_error(index, "Chromosome names must not start with `chr'.");
		}

		parse_char_in_space(input, index, ')');

		// If we are dealing with a chromosome that goes by many names, match all of
		// them.
		if (str.compare("23") == 0 || str.compare("X") == 0 ||
				str.compare("x") == 0) {
			return std::make_shared<or_node>(
					std::make_shared<check_chromosome_node<mate> >("23"),
					std::make_shared<check_chromosome_node<mate> >("x"));
		}

		if (str.compare("24") == 0 || str.compare("Y") == 0 ||
				str.compare("y") == 0) {
			return std::make_shared<or_node>(
					std::make_shared<check_chromosome_node<mate> >("24"),
					std::make_shared<check_chromosome_node<mate> >("y"));
		}
		if (str.compare("25") == 0 || str.compare("M") == 0 ||
				str.compare("m") == 0) {
			return std::make_shared<or_node>(
					std::make_shared<check_chromosome_node<mate> >("25"),
					std::make_shared<check_chromosome_node<mate> >("m"));
		}
		// otherwise, just match the provided chromosome.
		return std::make_shared<check_chromosome_node<mate> >(str);
	}

private:
	std::string name;
};
}
