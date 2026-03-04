#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "IrLowererStructFieldBindingHelpers.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using ResolveStructTypePathFn = std::function<std::string(const std::string &, const std::string &)>;
using ResolveStructLayoutExprPathFn = std::function<std::string(const Expr &)>;

std::string inferStructReturnPathFromDefinition(
    const std::string &defPath,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap);

std::string inferStructReturnPathFromExpr(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ResolveStructLayoutExprPathFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap);

} // namespace primec::ir_lowerer
