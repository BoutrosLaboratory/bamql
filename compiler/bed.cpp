/*
 * Copyright 2017 Paul Boutros. For details, see COPYING. Our lawyer cats sez:
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

#include "ast_node_chromosome.hpp"
#include "ast_node_function.hpp"
#include "ast_node_literal.hpp"
#include "bamql-compiler.hpp"
#include "compiler.hpp"
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static const std::string CHR_PREFIX("chr");
static const std::vector<bamql::RawFunctionArg> RAW_ARGS{
  bamql::RawFunctionArg::HEADER, bamql::RawFunctionArg::READ,
  bamql::RawFunctionArg::USER
};
namespace bamql {
typedef std::map<std::string, std::vector<std::shared_ptr<AstNode>>> CheckMap;

std::shared_ptr<AstNode> parseBED(ParseState &state) {
  state.parseCharInSpace('(');
  std::ifstream file(state.parseStr(")", true));
  state.parseCharInSpace(')');

  CheckMap chromosomes;
  if (!file.good()) {
    throw ParseError(state.where(), "Cannot read BED file.");
  }
  file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  auto bedLine = 1;
  while (file.good()) {
    bedLine++;
    std::string chr;
    int start;
    int end;
    std::getline(file, chr, '\t');
    if (chr.length() == 0 || file.eof()) {
      break;
    }
    file >> std::skipws >> start >> end;
    if (file.fail()) {
      std::stringstream message;
      message << "Failed to parse BED file line: " << bedLine;
      throw ParseError(state.where(), message.str());
    }
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (chr.length() > CHR_PREFIX.length() &&
        std::equal(CHR_PREFIX.begin(), CHR_PREFIX.end(), chr.begin())) {
      chr.erase(0, CHR_PREFIX.length());
    }
    std::vector<std::shared_ptr<AstNode>> args{
      std::make_shared<IntConst>(start + 1), std::make_shared<IntConst>(end + 1)
    };
    std::shared_ptr<AstNode> check = std::make_shared<BoolFunctionNode>(
        "bamql_check_position", std::move(args), RAW_ARGS, state);

    CheckMap::iterator lb = chromosomes.lower_bound(chr);

    if (lb != chromosomes.end() && !(chromosomes.key_comp()(chr, lb->first))) {
      lb->second.push_back(check);
    } else {
      chromosomes.insert(lb, CheckMap::value_type(chr, { check }));
    }
  }
  file.close();
  std::vector<std::shared_ptr<AstNode>> result;
  for (auto &chromosome : chromosomes) {
    result.push_back(
        std::make_shared<CheckChromosomeNode>(chromosome.first, false, state) &
        makeOr(std::move(chromosome.second)));
  }
  return makeOr(std::move(result));
}
} // namespace bamql
