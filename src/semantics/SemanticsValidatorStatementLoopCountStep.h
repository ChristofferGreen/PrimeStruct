#pragma once

#include <cstdint>
#include <optional>

#include "primec/Ast.h"

namespace primec::semantics {

std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                              bool allowBoolCount);
bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount);
bool runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(const Expr &expr);

} // namespace primec::semantics
