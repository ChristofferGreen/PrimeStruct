#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {

using EmitterExprControlBodyWrapperIsBuiltinBlockFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, std::string> &)>;

std::optional<std::string> runEmitterExprControlBodyWrapperStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBodyWrapperIsBuiltinBlockFn &isBuiltinBlock,
    const ExprStringFn &emitExpr);

} // namespace primec::emitter
