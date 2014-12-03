#pragma once
#include <memory>
#include <llvm/IR/IRBuilder.h>
namespace barf {
	class predicate;

	class ast_node {
		public:
			static std::shared_ptr<ast_node> parse(std::string input, std::vector<predicate> predicates);

			virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read) = 0;
	};
	
	class predicate {
		public:
			const std::string name() const;
			std::shared_ptr<ast_node> parse(std::string input, int&index);
	};

	class short_circuit_node : public ast_node {
		public:
			short_circuit_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
			virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read);
			virtual llvm::Value *branchValue() = 0;
		private:
			std::shared_ptr<ast_node>left;
			std::shared_ptr<ast_node>right;
	};
	class and_node : public short_circuit_node {
		public:
			and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
			virtual llvm::Value *branchValue();
	};
	class or_node : public short_circuit_node {
		public:
			or_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
			virtual llvm::Value *branchValue();
	};
	class not_node : public ast_node {
		public:
			not_node(std::shared_ptr<ast_node>expr);
			virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read);
		private:
			std::shared_ptr<ast_node>expr;
	};
	class conditional_node : public ast_node {
		public:
			conditional_node(std::shared_ptr<ast_node>condition, std::shared_ptr<ast_node>then_part, std::shared_ptr<ast_node> else_part);
			virtual llvm::Value *generate(llvm::IRBuilder<> builder, llvm::Value *read);
		private:
			std::shared_ptr<ast_node>condition;
			std::shared_ptr<ast_node>then_part;
			std::shared_ptr<ast_node>else_part;
	};

	int parse_int(std::string input, int& index);
	double parse_double(std::string input, int& index);
	std::string parse_str(std::string input, int& index);
	void parse_space(std::string input, int& index);
}
