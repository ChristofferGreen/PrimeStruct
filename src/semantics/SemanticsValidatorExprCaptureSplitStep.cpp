#include "SemanticsValidatorExprCaptureSplitStep.h"

#include "primec/testing/SemanticsControlFlowProbes.h"

#include <cctype>

namespace primec::semantics {

std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep(const std::string &text) {
  std::vector<std::string> tokens;
  std::string token;
  bool inToken = false;
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      if (inToken) {
        tokens.push_back(token);
        token.clear();
        inToken = false;
      }
      continue;
    }
    token.push_back(c);
    inToken = true;
  }
  if (inToken) {
    tokens.push_back(token);
  }
  return tokens;
}

ExprCaptureSplitProbeSnapshotForTesting probeExprCaptureSplitForTesting(const std::string &text) {
  return ExprCaptureSplitProbeSnapshotForTesting{.tokens = runSemanticsValidatorExprCaptureSplitStep(text)};
}

} // namespace primec::semantics
