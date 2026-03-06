#include "EmitterExprControlBuiltinBlockPreludeStep.h"

namespace primec::emitter {

EmitterExprControlBuiltinBlockPreludeStepResult runEmitterExprControlBuiltinBlockPreludeStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBuiltinBlockPreludeIsBuiltinBlockFn &isBuiltinBlock,
    const EmitterExprControlBuiltinBlockPreludeHasNamedArgumentsFn &hasNamedArguments) {
  if (!isBuiltinBlock || !isBuiltinBlock(expr, nameMap) || !expr.hasBodyArguments) {
    return {};
  }

  EmitterExprControlBuiltinBlockPreludeStepResult result;
  result.matched = true;
  if (!expr.args.empty() || !expr.templateArgs.empty() ||
      (hasNamedArguments && hasNamedArguments(expr.argNames)) || expr.bodyArguments.empty()) {
    result.earlyReturnExpr = "0";
  }
  return result;
}

} // namespace primec::emitter
