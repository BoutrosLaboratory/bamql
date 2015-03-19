#include <htslib/sam.h>
#include "barf.hpp"

namespace barf {

typedef bool (*ValidChar)(char, bool not_first);

/**
 * A predicate that checks of name of a string in the BAM auxiliary data.
 */
template <char G1, char G2, ValidChar VC>
class CheckAuxStringNode : public DebuggableNode {
public:
  CheckAuxStringNode(std::string name_, ParseState &state)
      : DebuggableNode(state), name(name_) {}
  virtual llvm::Value *generate(llvm::Module *module,
                                llvm::IRBuilder<> &builder,
                                llvm::Value *read,
                                llvm::Value *header,
                                llvm::DIScope *debug_scope) {
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
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto name_start = state.where();
    while (!state.empty() && *state != ')' &&
           VC(*state, state.where() > name_start)) {
      state.next();
    }
    if (name_start == state.where()) {
      throw ParseError(state.where(), "Expected valid identifier.");
    }
    auto match = state.strFrom(name_start);

    state.parseCharInSpace(')');

    return std::make_shared<CheckAuxStringNode<G1, G2, VC>>(match, state);
  }

private:
  std::string name;
};
}
