#include "barf.hpp"

namespace barf {

	static std::shared_ptr<ast_node> parse_conditional(const std::string &input, size_t &index, predicate_map predicates);

	static std::shared_ptr<ast_node> parse_terminal(const std::string &input, size_t &index, predicate_map predicates) {

		parse_space(input, index);
		if (index >= input.length()) {
			throw parse_error(index, "Reached end of input before completing parsing.");
		}
		if (input[index] == '!') {
			index++;
			return std::make_shared<not_node>(parse_terminal(input, index, predicates));
		}
		if (input[index] == '(') {
			index++;
			size_t brace_index = index;
			auto node = parse_conditional(input, index, predicates);
			parse_space(input, index);
			if (index < input.length() && input[index] == ')') {
				index++;
				return node;
			} else {
				throw parse_error(brace_index, "Open brace has no matching closing brace.");
			}
		}
		size_t start = index;
		while(index < input.length() && (input[index] >= 'a' && input[index] <'z' || input[index] =='_' )){
			index++;
		}
		std::string predicate_name = input.substr(start, index - start);
		if (predicate_name.empty()) {
			throw parse_error(index, "Empty predicate.");
		}

		std::shared_ptr<ast_node> node;
		if (predicates.count(predicate_name)) {
			node = predicates[predicate_name](input, index);
			parse_space(input, index);
		}
		return node;
	}

	static std::shared_ptr<ast_node> parse_and(const std::string &input, size_t &index, predicate_map predicates) {
		std::vector<std::shared_ptr<ast_node>> items;

		std::shared_ptr<ast_node> node = parse_terminal(input, index, predicates);
		parse_space(input, index);
		while (index < input.length() && input[index] == '&') {
			index++;
			items.push_back(node);
			node = parse_terminal(input, index, predicates);
			parse_space(input, index);
		}
		while(items.size() > 0) {
			node = std::make_shared<and_node>(items.back(), node);
			items.pop_back();
		}
		return node;
	}

	static std::shared_ptr<ast_node> parse_or(const std::string &input, size_t &index, predicate_map predicates) {
		std::vector<std::shared_ptr<ast_node>> items;

		std::shared_ptr<ast_node> node = parse_and(input, index, predicates);
		parse_space(input, index);
		while (index < input.length() && input[index] == '|') {
			index++;
			items.push_back(node);
			node = parse_and(input, index, predicates);
			parse_space(input, index);
		}
		while(items.size() > 0) {
			node = std::make_shared<or_node>(items.back(), node);
			items.pop_back();
		}
		return node;
	}

	static std::shared_ptr<ast_node> parse_conditional(const std::string &input, size_t &index, predicate_map predicates) {
		std::shared_ptr<ast_node> cond_part, if_part, else_part;

		cond_part = parse_or(input, index, predicates);
		parse_space(input, index);
		if (index < input.length() && input[index] == '?') {
			index++;
			if_part = parse_or(input, index, predicates);
			parse_space(input, index);
		}
		else {
			return cond_part;
		}
		if (index < input.length() && input[index] == ':') {
			index++;
			else_part = parse_or(input, index, predicates);
		} else {
			throw parse_error(index, "Ternary operator has no ':'.");
		}
		return std::make_shared<conditional_node>(cond_part, if_part, else_part);
	}

	/**
	 * Parse a string into a syntax tree using the built-in logical operations and the predicates provided.
	 */
	std::shared_ptr<ast_node> ast_node::parse(const std::string &input, predicate_map predicates) throw(parse_error) {
		size_t index = 0;
		std::shared_ptr<ast_node> node = parse_conditional(input, index, predicates);

		parse_space(input, index);
		// OUTERMOST check string is fully consumed
		if (index != input.length()) {
			throw parse_error(index, "Junk at end of input.");
		}
		return node;
	}
}
