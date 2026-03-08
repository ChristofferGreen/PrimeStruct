#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlCountRewriteIsCountLikeCallFn =
    std::function<bool(const Expr &, const std::unordered_map<std::string, Emitter::BindingInfo> &)>;
using EmitterExprControlCountRewriteResolveMethodPathFn = std::function<bool(const Expr &, std::string &)>;
using EmitterExprControlCountRewriteIsCollectionAccessReceiverFn = std::function<bool(const Expr &)>;

std::optional<std::string> runEmitterExprControlCountRewriteStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlCountRewriteResolveMethodPathFn &resolveMethodPath,
    const EmitterExprControlCountRewriteIsCollectionAccessReceiverFn &isCollectionAccessReceiverExpr = {});

} // namespace primec::emitter
