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
#pragma once

#include "bamql-compiler.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#define type_check(EXPRESSION, TYPE)                                           \
  do {                                                                         \
    if ((EXPRESSION)->type() != (TYPE)) {                                      \
      std::cerr << __FILE__ ":" << __LINE__                                    \
                << ": Expression " #EXPRESSION " is not of type " #TYPE "\n";  \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define type_check_not(EXPRESSION, TYPE)                                       \
  do {                                                                         \
    if ((EXPRESSION)->type() == (TYPE)) {                                      \
      std::cerr << __FILE__ ":" << __LINE__                                    \
                << ": Expression " #EXPRESSION " must not be of type " #TYPE   \
                   "\n";                                                       \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define type_check_match(EXPRESSION1, EXPRESSION2)                             \
  do {                                                                         \
    if ((EXPRESSION1)->type() != (EXPRESSION2)->type()) {                      \
      std::cerr << __FILE__ ":" << __LINE__                                    \
                << ": Expression " #EXPRESSION1                                \
                   " does not have the same type as " #EXPRESSION2 "\n";       \
      abort();                                                                 \
    }                                                                          \
  } while (0)

namespace bamql {

std::shared_ptr<AstNode> parseBED(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseBinding(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseMatchBinding(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseMax(ParseState &state) throw(ParseError);
std::shared_ptr<AstNode> parseMin(ParseState &state) throw(ParseError);
}
