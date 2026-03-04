#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using IsEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsArrayCountCallFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsVectorCapacityCallFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsStringCountCallFn = std::function<bool(const Expr &, const LocalMap &)>;

struct CountAccessClassifiers {
  IsEntryArgsNameFn isEntryArgsName;
  IsArrayCountCallFn isArrayCountCall;
  IsVectorCapacityCallFn isVectorCapacityCall;
  IsStringCountCallFn isStringCountCall;
};

struct EntryCountAccessSetup {
  bool hasEntryArgs = false;
  std::string entryArgsName;
  CountAccessClassifiers classifiers;
};
enum class StringCountCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error);
bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error);
CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName);
IsEntryArgsNameFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName);
IsArrayCountCallFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName);
IsVectorCapacityCallFn makeIsVectorCapacityCall();
IsStringCountCallFn makeIsStringCountCall();
bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName);
bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn);
bool isStringCountCall(const Expr &expr, const LocalMap &localsIn);
StringCountCallEmitResult tryEmitStringCountCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    std::string &error);

} // namespace primec::ir_lowerer
