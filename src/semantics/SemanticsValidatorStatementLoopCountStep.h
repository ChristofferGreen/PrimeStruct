#pragma once

#include <optional>

#include "SemanticsValidator.h"

namespace primec::semantics {

std::optional<uint64_t> runSemanticsValidatorStatementKnownIterationCountStep(const Expr &countExpr,
                                                                               bool allowBoolCount);
bool runSemanticsValidatorStatementCanIterateMoreThanOnceStep(const Expr &countExpr, bool allowBoolCount);

} // namespace primec::semantics
