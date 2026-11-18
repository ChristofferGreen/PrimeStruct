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

// The base name (`Reference`, `Pointer`, `map`, `vector`, ...) of the template type `def` declares as its result,
// normalized as a collection type name, or empty.
std::string declaredReturnTypeBase(const Definition &def);

// The struct `def` declares as its result: an absolute type name, or the one `resolveStructTypeName` finds for it;
// empty when there is none.
std::string declaredReturnStructPath(
    const Definition &def,
    const std::function<bool(const std::string &, const std::string &, std::string &)>
        &resolveStructTypeName);

// The storage struct of a declared builtin key/value collection result, or empty: a spawned call returns it by value
// in that struct.
std::string
declaredReturnMapStoragePath(const Definition &def,
                             const std::unordered_map<std::string, const Definition *> &defMap);

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
