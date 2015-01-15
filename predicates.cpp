#include <htslib/sam.h>
#include "barf.hpp"

// This must be included exactly once in only this file!!!
#include "runtime.inc"

// Please keep the predicates in alphabetical order.
namespace barf {

/**
 * This helper function puts a string into a global constant and then returns a pointer to it.
 *
 * One would think this is trivial, but it isn't.
 */
llvm::Value *createString(llvm::Module *module, std::string str) {
	auto array = llvm::ConstantDataArray::getString(llvm::getGlobalContext(), str);
	auto global_variable = new llvm::GlobalVariable(
		*module,
		llvm::ArrayType::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()), str.length() + 1),
		true,
		llvm::GlobalValue::PrivateLinkage,
		0,
		".str");
	global_variable->setAlignment(1);
	global_variable->setInitializer(array);
	auto zero = llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()), 0);
	std::vector<llvm::Value*> indicies;
	indicies.push_back(zero);
	indicies.push_back(zero);
	return llvm::ConstantExpr::getGetElementPtr(global_variable, indicies);
}

/**
 * A predicate that checks of the chromosome name.
 */
class check_chromosome_node : public ast_node {
public:
check_chromosome_node(std::string name_) : name(name_) {
}
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("check_chromosome");
	return builder.CreateCall3(function, read, header, createString(module, name));
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

typedef bool (*ValidChar)(char, bool not_first);

/**
 * A predicate that checks of name of a string in the BAM auxiliary data.
 */
template<char G1, char G2, ValidChar VC>
class check_aux_string_node : public ast_node {
public:
check_aux_string_node(std::string name_) : name(name_) {
}
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("check_aux_str");
	return builder.CreateCall4(
		function,
		read,
		createString(module, name),
		llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()), G1),
		llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()), G2));
}
static std::shared_ptr<ast_node> parse(const std::string& input, size_t&index) throw (parse_error) {
	parse_char_in_space(input, index, '(');

	auto name_start = index;
	while (index < input.length() && input[index] != ')' && VC(input[index], index > name_start)) {
		index++;
	}
	if (name_start == index) {
		throw parse_error(index, "Expected valid identifier.");
	}
	auto name_length = index - name_start;

	parse_char_in_space(input, index, ')');

	return std::make_shared<check_aux_string_node<G1, G2, VC>>(input.substr(name_start, name_length));
}
private:
std::string name;
};

/* This insanity is brought to you by samtools's sam_tview.c */
bool read_group_char(char input, bool not_first) {
	return input >= '!' && input <= '~' && (not_first || input != '=');
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
 * A predicate that checks of the read's flag.
 */
template<unsigned int F>
class check_flag : public ast_node {
public:
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("check_flag");
	return builder.CreateCall2(function, read, llvm::ConstantInt::get(llvm::Type::getInt16Ty(llvm::getGlobalContext()), F));
}

static std::shared_ptr<ast_node> parse(const std::string& input, size_t&index) throw (parse_error) {
	static auto result = std::make_shared<check_flag<F>>();
	return result;
}
};

/**
 * A predicate that is true if the mapping quality is sufficiently good.
 */
class mapping_quality_node : public ast_node {
public:
mapping_quality_node(double probability_) : probability(probability_) {
}
virtual llvm::Value *generate(llvm::Module *module, llvm::IRBuilder<>& builder, llvm::Value *read, llvm::Value *header) {
	auto function = module->getFunction("check_mapping_quality");
	return builder.CreateCall(function, llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()), -10 * log(probability)));
}
private:
double probability;
};

static std::shared_ptr<ast_node> parse_mapping_quality(const std::string& input, size_t&index) throw (parse_error) {
	parse_char_in_space(input, index, '(');

	auto probability = parse_double(input, index);
	if (probability <= 0 || probability >= 1) {
		throw parse_error(index, "The provided probability is not probable.");
	}

	parse_char_in_space(input, index, ')');

	return std::make_shared<mapping_quality_node>(probability);
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
		       {std::string("paired?"), check_flag<BAM_FPAIRED>::parse},
		       {std::string("proper_pair?"), check_flag<BAM_FPROPER_PAIR>::parse},
		       {std::string("unmapped?"), check_flag<BAM_FUNMAP>::parse},
		       {std::string("mate_unmapped?"), check_flag<BAM_FMUNMAP>::parse},
		       {std::string("mapped_to_reverse?"), check_flag<BAM_FREVERSE>::parse},
		       {std::string("mate_mapped_to_reverse?"), check_flag<BAM_FMREVERSE>::parse},
		       {std::string("read1?"), check_flag<BAM_FREAD1>::parse},
		       {std::string("read2?"), check_flag<BAM_FREAD2>::parse},
		       {std::string("secondary?"), check_flag<BAM_FSECONDARY>::parse},
		       {std::string("failed_qc?"), check_flag<BAM_FQCFAIL>::parse},
		       {std::string("duplicate?"), check_flag<BAM_FDUP>::parse},
		       {std::string("supplementary?"), check_flag<BAM_FSUPPLEMENTARY>::parse},
		       {std::string("mapping_quality"), parse_mapping_quality},
		       {std::string("random"), parse_randomly},
		       {std::string("read_group"), check_aux_string_node<'R', 'G', read_group_char>::parse},
		       {std::string("true"), parse_true}
	};
}

llvm::Type *getRuntimeType(llvm::Module *module, llvm::StringRef name) {
	auto struct_ty = module->getTypeByName(name);
	if (struct_ty == nullptr) {
		define_runtime(module);
		struct_ty = module->getTypeByName(name);
	}
	return struct_ty;
}

llvm::Type *getBamType(llvm::Module *module) {
	return getRuntimeType(module, "struct.bam1_t");
}

llvm::Type *getBamHeaderType(llvm::Module *module) {
	return getRuntimeType(module, "struct.bam_hdr_t");
}
}
