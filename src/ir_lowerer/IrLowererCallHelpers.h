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

bool buildOrderedCallArguments(const Expr &callExpr,
                               const std::vector<Expr> &params,
                               std::vector<const Expr *> &ordered,
                               std::string &error);

} // namespace primec::ir_lowerer
