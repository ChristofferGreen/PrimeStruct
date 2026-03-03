#pragma once

#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn);
bool isStringCountCall(const Expr &expr, const LocalMap &localsIn);

} // namespace primec::ir_lowerer
