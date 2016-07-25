/*
 * Copyright 2015 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
 *
 * OICR makes no representations whatsoever as to the SOFTWARE contained
 * herein.  It is experimental in nature and is provided WITHOUT WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE OR ANY OTHER WARRANTY,
 * EXPRESS OR IMPLIED. OICR MAKES NO REPRESENTATION OR WARRANTY THAT THE USE OF
 * THIS SOFTWARE WILL NOT INFRINGE ANY PATENT OR OTHER PROPRIETARY RIGHT.  By
 * downloading this SOFTWARE, your Institution hereby indemnifies OICR against
 * any loss, claim, damage or liability, of whatsoever kind or nature, which
 * may arise from your Institution's respective use, handling or storage of the
 * SOFTWARE. If publications result from research using this SOFTWARE, we ask
 * that the Ontario Institute for Cancer Research be acknowledged and/or
 * credit be given to OICR scientists, as scientifically appropriate.
 */

#include <htslib/sam.h>
#include "bamql-compiler.hpp"

namespace bamql {

typedef bool (*ValidChar)(char, bool not_first);

/**
 * A predicate that checks of name of a string in the BAM auxiliary data.
 */
template <char G1, char G2, ValidChar VC>
class CheckAuxStringNode : public DebuggableNode {
public:
  CheckAuxStringNode(std::string name_, ParseState &state)
      : DebuggableNode(state), name(name_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_aux_str");
    llvm::Value *args[] = {
      read,
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), G1),
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), G2),
      state.createString(name)
    };
    return state->CreateCall(function, args);
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

class CheckAuxUserStringNode : public DebuggableNode {
public:
  CheckAuxUserStringNode(char first_,
                         char second_,
                         std::string name_,
                         ParseState &state)
      : DebuggableNode(state), first(first_), second(second_), name(name_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_aux_str");
    llvm::Value *args[] = {
      read,
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), first),
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), second),
      state.createString(name)
    };
    return state->CreateCall(function, args);
  }
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto first = *state;
    state.next();
    if (!isalnum(first)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }
    auto second = *state;
    state.next();
    if (!isalnum(second)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }

    state.parseCharInSpace(',');

    auto name_start = state.where();
    while (!state.empty() && *state != ')' && !isspace(*state)) {
      state.next();
    }
    if (name_start == state.where()) {
      throw ParseError(state.where(), "Expected valid string.");
    }
    auto match = state.strFrom(name_start);

    state.parseCharInSpace(')');

    return std::make_shared<CheckAuxUserStringNode>(
        first, second, match, state);
  }

private:
  char first;
  char second;
  std::string name;
};

class CheckAuxUserCharNode : public DebuggableNode {
public:
  CheckAuxUserCharNode(char first_,
                       char second_,
                       char value_,
                       ParseState &state)
      : DebuggableNode(state), first(first_), second(second_), value(value_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_aux_char");
    llvm::Value *args[] = {
      read,
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), first),
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), second),
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), value)
    };
    return state->CreateCall(function, args);
  }
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto first = *state;
    state.next();
    if (!isalnum(first)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }
    auto second = *state;
    state.next();
    if (!isalnum(second)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }

    state.parseCharInSpace(',');

    auto value = *state;
    state.next();

    state.parseCharInSpace(')');

    return std::make_shared<CheckAuxUserCharNode>(first, second, value, state);
  }

private:
  char first;
  char second;
  char value;
};

class CheckAuxUserIntNode : public DebuggableNode {
public:
  CheckAuxUserIntNode(char first_, char second_, int value_, ParseState &state)
      : DebuggableNode(state), first(first_), second(second_), value(value_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_aux_int");
    llvm::Value *args[] = {
      read,
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), first),
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), second),
      llvm::ConstantInt::get(
          llvm::Type::getInt32Ty(state.module()->getContext()), value)
    };
    return state->CreateCall(function, args);
  }
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto first = *state;
    state.next();
    if (!isalnum(first)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }
    auto second = *state;
    state.next();
    if (!isalnum(second)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }

    state.parseCharInSpace(',');

    bool is_negative = *state == '-';
    if (is_negative)
      state.next();
    auto value = state.parseInt();

    state.parseCharInSpace(')');

    return std::make_shared<CheckAuxUserIntNode>(
        first, second, is_negative ? -value : value, state);
  }

private:
  char first;
  char second;
  int value;
};
class CheckAuxUserFloatNode : public DebuggableNode {
public:
  CheckAuxUserFloatNode(char first_,
                        char second_,
                        double value_,
                        ParseState &state)
      : DebuggableNode(state), first(first_), second(second_), value(value_) {}
  virtual llvm::Value *generate(GenerateState &state,
                                llvm::Value *read,
                                llvm::Value *header) {
    auto function = state.module()->getFunction("bamql_check_aux_double");
    llvm::Value *args[] = {
      read,
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), first),
      llvm::ConstantInt::get(
          llvm::Type::getInt8Ty(state.module()->getContext()), second),
      llvm::ConstantFP::get(
          llvm::Type::getDoubleTy(state.module()->getContext()), value)
    };
    return state->CreateCall(function, args);
  }
  static std::shared_ptr<AstNode> parse(ParseState &state) throw(ParseError) {
    state.parseCharInSpace('(');

    auto first = *state;
    state.next();
    if (!isalnum(first)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }
    auto second = *state;
    state.next();
    if (!isalnum(second)) {
      throw ParseError(state.where(),
                       "Expected alpha numeric identifier string.");
    }

    state.parseCharInSpace(',');

    auto value = state.parseDouble();

    state.parseCharInSpace(')');

    return std::make_shared<CheckAuxUserFloatNode>(first, second, value, state);
  }

private:
  char first;
  char second;
  double value;
};
}
