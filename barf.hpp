#pragma once
#include <memory>
#include <llvm/IR/IRBuilder>
namespace barf {
	class ast_node {
		public:
			static ast_node parse(std::string input, std::vector<predicate> predicates);

			virtual llvm::Value generate(llvm::IRBuilder builder, llvm::Value *read) = 0;
	};
	
	class predicate {
		public:
			std::shared_ptr<ast_node> parse(std::string input, int&index);
	}

	class and_node : ast_node {
		public:
			and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
		private:
			std::shared_ptr<ast_node>left;
			std::shared_ptr<ast_node>;
	};
	class or_node : ast_node {
		public:
			and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
		private:
			std::shared_ptr<ast_node>left;
			std::shared_ptr<ast_node>;
	};
	class not_node : ast_node {
		public:
			and_node(std::shared_ptr<ast_node>left, std::shared_ptr<ast_node>);
		private:
			std::shared_ptr<ast_node>left;
			std::shared_ptr<ast_node>;
	};

	int parse_int(std::string input, int& index);
	int parse_double(std::string input, int& index);
	std::string parse_str(std::string input, int& index);
	void parse_space(std::string input, int& index);
}
