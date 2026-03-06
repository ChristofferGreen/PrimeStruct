#include "EmitterExprControlIfBranchBodyStep.h"

#include <sstream>

namespace primec::emitter {

EmitterExprControlIfBranchBodyStepResult runEmitterExprControlIfBranchBodyStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchBodyEmitStatementFn &emitStatement) {
  if (!emitStatement || candidate.bodyArguments.empty()) {
    return {};
  }

  EmitterExprControlIfBranchBodyStepResult result;
  result.handled = true;

  std::ostringstream out;
  for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
    const Expr &stmt = candidate.bodyArguments[i];
    const bool isLast = (i + 1 == candidate.bodyArguments.size());
    const auto emitted = emitStatement(stmt, isLast);
    if (emitted.handled) {
      out << emitted.emittedStatement;
    }
    if (emitted.shouldBreak) {
      break;
    }
  }

  result.emittedBody = out.str();
  return result;
}

} // namespace primec::emitter
