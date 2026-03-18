#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec::semantics {

std::vector<std::string> runSemanticsValidatorExprCaptureSplitStep(const std::string &text);
std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                              bool allowBoolCount);
bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount);
bool runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(const Expr &expr);

} // namespace primec::semantics
