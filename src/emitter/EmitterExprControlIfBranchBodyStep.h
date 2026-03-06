#pragma once

#include <functional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

struct EmitterExprControlIfBranchBodyEmitResult {
  bool handled = false;
  std::string emittedStatement;
  bool shouldBreak = false;
};

using EmitterExprControlIfBranchBodyEmitStatementFn =
    std::function<EmitterExprControlIfBranchBodyEmitResult(const Expr &, bool isLast)>;

struct EmitterExprControlIfBranchBodyStepResult {
  bool handled = false;
  std::string emittedBody;
};

EmitterExprControlIfBranchBodyStepResult runEmitterExprControlIfBranchBodyStep(
    const Expr &candidate,
    const EmitterExprControlIfBranchBodyEmitStatementFn &emitStatement);

} // namespace primec::emitter
