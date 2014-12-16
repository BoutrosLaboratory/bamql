#include "barf.hpp"

// This must be included exactly once in only this file!!!
#include "runtime.inc"

// Please keep the predicates in alphabetical order.
namespace barf {

/**
 * A predicate that checks of the chromosome name.
 */
class check_chromosome_node : public ast_node {
public:
check_chromosome_node(std::string name_) : name(name_) {
}
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("check_chromosome");
	return builder.CreateCall3(function, read, header, llvm::ConstantDataArray::getString(llvm::getGlobalContext(), name));
}
private:
std::string name;
};

static std::shared_ptr<ast_node> parse_check_chromosome(const std::string& input, size_t&index) throw (parse_error) {
	parse_char_in_space(input, index, '(');

	auto str = parse_str(input, index, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_*?.");
	if (str.compare(0, 3, "chr") == 0) {
		throw parse_error(index, "Chromosome names must not start with `chr'.");
	}

	parse_char_in_space(input, index, ')');

	// If we are dealing with a chromosome that goes by many names, match all of them.
	if (str.compare("23") == 0 || str.compare("X")  == 0|| str.compare("x") == 0) {
		return std::make_shared<or_node>(std::make_shared<check_chromosome_node>("23"), std::make_shared<check_chromosome_node>("x") );
	}

	if (str.compare("24") == 0 || str.compare("Y") == 0 || str.compare("y") == 0) {
		return std::make_shared<or_node>(std::make_shared<check_chromosome_node>("24"), std::make_shared<check_chromosome_node>("y") );
	}
	if (str.compare("25") == 0 || str.compare("M") == 0 || str.compare("m") == 0) {
		return std::make_shared<or_node>(std::make_shared<check_chromosome_node>("25"), std::make_shared<check_chromosome_node>("m") );
	}
	// otherwise, just match the provided chromosome.
	return std::make_shared<check_chromosome_node>(str);
}

/**
 * A predicate that checks of the read group name.
 */
class check_read_group_node : public ast_node {
public:
check_read_group_node(std::string name_) : name(name_) {
}
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("check_read_group");
	return builder.CreateCall2(function, read, llvm::ConstantDataArray::getString(llvm::getGlobalContext(), name));
}
private:
std::string name;
};

static std::shared_ptr<ast_node> parse_check_read_group(const std::string& input, size_t&index) throw (parse_error) {
	parse_char_in_space(input, index, '(');

	auto name_start = index;
	/* This insanity is brought to you by samtools's sam_tview.c */
	while (index < input.length() && input[index] >= '!' && input[index] <= '~' && input[index] != ')' && (index > name_start || input[index] != '=')) {
		index++;
	}
	if (name_start == index) {
		throw parse_error(index, "Expected valid read group name.");
	}
	auto name_length = index - name_start;

	parse_char_in_space(input, index, ')');

	return std::make_shared<check_read_group_node>(input.substr(name_start, name_length));
}

/**
 * A predicate that always returns false.
 */
class false_node : public ast_node {
public:
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	return llvm::ConstantInt::getFalse(llvm::getGlobalContext());
}
};

static std::shared_ptr<ast_node> parse_false(const std::string& input, size_t&index) throw (parse_error) {
	static auto result = std::make_shared<false_node>();
	return result;
}

/**
 * A predicate that checks of the read is paired.
 */
class is_paired_node : public ast_node {
public:
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("is_paired");
	return builder.CreateCall(function, read);
}
};

static std::shared_ptr<ast_node> parse_is_paired(const std::string& input, size_t&index) throw (parse_error) {
	static auto result = std::make_shared<is_paired_node>();
	return result;
}

/**
 * A predicate that randomly is true.
 */
class randomly_node : public ast_node {
public:
randomly_node(double probability_) : probability(probability_) {
}
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("randomly");
	return builder.CreateCall(function, llvm::ConstantFP::get(llvm::Type::getDoubleTy(llvm::getGlobalContext()), probability));
}
private:
double probability;
};

static std::shared_ptr<ast_node> parse_randomly(const std::string& input, size_t&index) throw (parse_error) {
	parse_char_in_space(input, index, '(');

	auto probability = parse_double(input, index);
	if (probability < 0 || probability > 1) {
		throw parse_error(index, "The provided probability is not probable.");
	}

	parse_char_in_space(input, index, ')');

	return std::make_shared<randomly_node>(probability);
}

/**
 * A predicate that always returns true.
 */
class true_node : public ast_node {
public:
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	return llvm::ConstantInt::getTrue(llvm::getGlobalContext());
}
};

static std::shared_ptr<ast_node> parse_true(const std::string& input, size_t&index) throw (parse_error) {
	static auto result = std::make_shared<true_node>();
	return result;
}

/**
 * All the predicates known to the system.
 */
predicate_map getDefaultPredicates() {
	return {
		       {std::string("chr"), parse_check_chromosome},
		       {std::string("false"), parse_false},
		       {std::string("is_paired"), parse_is_paired},
		       {std::string("random"), parse_randomly},
		       {std::string("read_group"), parse_check_read_group},
		       {std::string("true"), parse_true}
	};
}


llvm::Type *getBamType(llvm::Module *module) {
	auto struct_bam1_t = module->getTypeByName("struct.bam1_t");
	if (struct_bam1_t == nullptr) {
		define_runtime(module);
	}
	return module->getTypeByName("struct.bam1_t");
}

llvm::Type *getBamHeaderType(llvm::Module *module) {
	auto struct_bam1_t = module->getTypeByName("struct.bam_hdr_t");
	if (struct_bam1_t == nullptr) {
		define_runtime(module);
	}
	return module->getTypeByName("struct.bam_hdr_t");
}
}
