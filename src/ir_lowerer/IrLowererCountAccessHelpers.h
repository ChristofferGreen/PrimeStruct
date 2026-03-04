#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using IsEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsArrayCountCallFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsVectorCapacityCallFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsStringCountCallFn = std::function<bool(const Expr &, const LocalMap &)>;

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error);
IsEntryArgsNameFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName);
IsArrayCountCallFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName);
IsVectorCapacityCallFn makeIsVectorCapacityCall();
IsStringCountCallFn makeIsStringCountCall();
bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn);
bool isStringCountCall(const Expr &expr, const LocalMap &localsIn);

} // namespace primec::ir_lowerer
