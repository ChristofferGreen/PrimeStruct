#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ast.h"

namespace primec::ir_lowerer {

const Definition *resolveDefinitionCall(const Expr &callExpr,
                                        const std::unordered_map<std::string, const Definition *> &defMap,
                                        const std::function<std::string(const Expr &)> &resolveExprPath);

std::string resolveCallPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);

bool isTailCallCandidate(const Expr &expr,
                         const std::unordered_map<std::string, const Definition *> &defMap,
                         const std::function<std::string(const Expr &)> &resolveExprPath);

bool hasTailExecutionCandidate(const std::vector<Expr> &statements,
                               bool definitionReturnsVoid,
                               const std::function<bool(const Expr &)> &isTailCallCandidate);

bool buildOrderedCallArguments(const Expr &callExpr,
                               const std::vector<Expr> &params,
                               std::vector<const Expr *> &ordered,
                               std::string &error);
std::string resolveDefinitionNamespacePrefix(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);

} // namespace primec::ir_lowerer
