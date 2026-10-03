#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/ir_lowerer/IrLowererStructFieldBindingHelpers.h"
#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

using ResolveStructTypePathFn = std::function<std::string(const std::string &, const std::string &)>;

std::string inferStructReturnPathFromDefinition(
    const std::string &defPath,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ExprStringFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap);

std::string inferStructReturnPathFromExpr(
    const Expr &expr,
    const std::unordered_map<std::string, LayoutFieldBinding> &knownFields,
    const std::unordered_set<std::string> &structNames,
    const ResolveStructTypePathFn &resolveStructTypePath,
    const ExprStringFn &resolveStructLayoutExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap);

} // namespace primec::ir_lowerer
