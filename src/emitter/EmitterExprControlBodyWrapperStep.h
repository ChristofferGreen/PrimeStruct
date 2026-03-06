#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitterExprControlBodyWrapperIsBuiltinBlockFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, std::string> &)>;
using EmitterExprControlBodyWrapperEmitExprFn = std::function<std::string(const Expr &)>;

std::optional<std::string> runEmitterExprControlBodyWrapperStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBodyWrapperIsBuiltinBlockFn &isBuiltinBlock,
    const EmitterExprControlBodyWrapperEmitExprFn &emitExpr);

} // namespace primec::emitter
