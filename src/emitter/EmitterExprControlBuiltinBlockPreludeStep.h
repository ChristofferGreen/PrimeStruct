#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockPreludeIsBuiltinBlockFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, std::string> &)>;
using EmitterExprControlBuiltinBlockPreludeHasNamedArgumentsFn =
    std::function<bool(const std::vector<std::optional<std::string>> &)>;

struct EmitterExprControlBuiltinBlockPreludeStepResult {
  bool matched = false;
  std::optional<std::string> earlyReturnExpr;
};

EmitterExprControlBuiltinBlockPreludeStepResult runEmitterExprControlBuiltinBlockPreludeStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBuiltinBlockPreludeIsBuiltinBlockFn &isBuiltinBlock,
    const EmitterExprControlBuiltinBlockPreludeHasNamedArgumentsFn &hasNamedArguments);

} // namespace primec::emitter
