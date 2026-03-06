#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIsCountLikeCallFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, Emitter::BindingInfo> &)>;
using EmitterExprControlResolveMethodPathFn = std::function<bool(std::string &)>;

std::optional<std::string> runEmitterExprControlMethodPathStep(
    const Expr &expr,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlResolveMethodPathFn &resolveMethodPath);

} // namespace primec::emitter
