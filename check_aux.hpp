#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {

typedef bool (*ValidChar)(char, bool not_first);

/**
 * A predicate that checks of name of a string in the BAM auxiliary data.
 */
template <char G1, char G2, ValidChar VC>
class check_aux_string_node : public ast_node {
public:
  check_aux_string_node(std::string name_) : name(name_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = module->getFunction("check_aux_str");
    return builder.CreateCall4(
        function,
        read,
        createString(module, name),
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                               G1),
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(llvm::getGlobalContext()),
                               G2));
  }
  static std::shared_ptr<ast_node> parse(const std::string &input,
                                         size_t &index) throw(parse_error) {
    parse_char_in_space(input, index, '(');

    auto name_start = index;
    while (index < input.length() && input[index] != ')' &&
           VC(input[index], index > name_start)) {
      index++;
    }
    if (name_start == index) {
      throw parse_error(index, "Expected valid identifier.");
    }
    auto name_length = index - name_start;

    parse_char_in_space(input, index, ')');

    return std::make_shared<check_aux_string_node<G1, G2, VC>>(
        input.substr(name_start, name_length));
  }

private:
  std::string name;
};
}
