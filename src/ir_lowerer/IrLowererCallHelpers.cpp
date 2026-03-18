#include "IrLowererCallHelpers.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  constexpr std::string_view vectorPrefix = "vector/";
  constexpr std::string_view arrayPrefix = "array/";
  constexpr std::string_view stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(vectorPrefix.size());
    if (helperNameOut == "count" || helperNameOut == "capacity") {
      return true;
    }
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = normalized.substr(arrayPrefix.size());
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdVectorPrefix.size());
    return true;
  }
  return false;
}

bool isArgsPackParam(const Expr &param) {
  for (const auto &transform : param.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return transform.name == "args";
  }
  return false;
}

std::string normalizeMapImportAliasPath(const std::string &path);

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isMapBuiltinName(const Expr &expr, const char *name);
bool isMapContainsHelperName(const Expr &expr);
bool isMapTryAtHelperName(const Expr &expr);

bool isExplicitMapHelperFallbackPath(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  const std::string normalizedPath = normalizeMapImportAliasPath(expr.name);
  return normalizedPath == "/map/count" || normalizedPath == "/map/at" || normalizedPath == "/map/at_unsafe" ||
         normalizedPath == "/std/collections/map/count" || normalizedPath == "/std/collections/map/at" ||
         normalizedPath == "/std/collections/map/at_unsafe";
}

std::string normalizeMapImportAliasPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  constexpr std::string_view mapPrefix = "map/";
  constexpr std::string_view stdMapPrefix = "std/collections/map/";
  if (path.rfind(mapPrefix, 0) == 0 || path.rfind(stdMapPrefix, 0) == 0) {
    return "/" + path;
  }
  return path;
}

} // namespace

bool emitMapLookupContains(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);

bool emitMapLookupTryAt(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);

BufferBuiltinDispatchResult tryEmitBufferBuiltinDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  const auto result = tryEmitBufferBuiltinCall(
      expr,
      localsIn,
      valueKindFromTypeName,
      inferExprKind,
      allocLocalRange,
      allocTempLocal,
      emitExpr,
      emitInstruction,
      error);
  if (result == BufferBuiltinCallEmitResult::Emitted) {
    return BufferBuiltinDispatchResult::Emitted;
  }
  if (result == BufferBuiltinCallEmitResult::Error) {
    return BufferBuiltinDispatchResult::Error;
  }
  return BufferBuiltinDispatchResult::NotHandled;
}

IrOpcode mapKeyCompareOpcode(LocalInfo::ValueKind mapKeyKind) {
  if (mapKeyKind == LocalInfo::ValueKind::Int64 || mapKeyKind == LocalInfo::ValueKind::UInt64) {
    return IrOpcode::CmpEqI64;
  }
  if (mapKeyKind == LocalInfo::ValueKind::Float64) {
    return IrOpcode::CmpEqF64;
  }
  if (mapKeyKind == LocalInfo::ValueKind::Float32) {
    return IrOpcode::CmpEqF32;
  }
  return IrOpcode::CmpEqI32;
}

MapLookupStringKeyResult tryResolveMapLookupStringKey(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    int32_t &stringIndexOut,
    std::string &error) {
  (void)error;
  if (mapKeyKind != LocalInfo::ValueKind::String) {
    return MapLookupStringKeyResult::NotHandled;
  }
  size_t length = 0;
  if (!resolveStringTableTarget(lookupKeyExpr, localsIn, stringIndexOut, length)) {
    return MapLookupStringKeyResult::NotHandled;
  }
  return MapLookupStringKeyResult::Resolved;
}

MapLookupKeyLocalEmitResult tryEmitMapLookupStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error) {
  int32_t stringIndex = -1;
  const auto resolveResult = tryResolveMapLookupStringKey(
      mapKeyKind, lookupKeyExpr, localsIn, resolveStringTableTarget, stringIndex, error);
  if (resolveResult == MapLookupStringKeyResult::NotHandled) {
    return MapLookupKeyLocalEmitResult::NotHandled;
  }
  if (resolveResult == MapLookupStringKeyResult::Error) {
    return MapLookupKeyLocalEmitResult::Error;
  }
  emitPushI32(stringIndex);
  emitStoreLocal(keyLocal);
  return MapLookupKeyLocalEmitResult::Emitted;
}

bool emitMapLookupNonStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error) {
  const LocalInfo::ValueKind lookupKeyKind = inferExprKind(lookupKeyExpr, localsIn);
  if (!validateMapLookupKeyKind(mapKeyKind, lookupKeyKind, error)) {
    return false;
  }
  if (!emitExpr(lookupKeyExpr, localsIn)) {
    return false;
  }
  emitStoreLocal(keyLocal);
  return true;
}

bool emitMapLookupKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &keyLocalOut,
    std::string &error) {
  keyLocalOut = allocTempLocal();
  const auto stringLookupKeyEmitResult = tryEmitMapLookupStringKeyLocal(
      mapKeyKind,
      lookupKeyExpr,
      localsIn,
      resolveStringTableTarget,
      emitPushI32,
      emitStoreLocal,
      keyLocalOut,
      error);
  if (stringLookupKeyEmitResult == MapLookupKeyLocalEmitResult::Error) {
    return false;
  }
  if (stringLookupKeyEmitResult == MapLookupKeyLocalEmitResult::Emitted) {
    return true;
  }
  return emitMapLookupNonStringKeyLocal(
      mapKeyKind, lookupKeyExpr, localsIn, inferExprKind, emitExpr, emitStoreLocal, keyLocalOut, error);
}

bool emitMapLookupTargetPointerLocal(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &ptrLocalOut) {
  ptrLocalOut = allocTempLocal();
  if (!emitExpr(targetExpr, localsIn)) {
    return false;
  }
  emitStoreLocal(ptrLocalOut);
  return true;
}

MapLookupLoopLocals emitMapLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  const auto loopLocals = emitMapLookupLoopLocals(ptrLocal, allocTempLocal, emitInstruction);
  const auto loopCondition = emitMapLookupLoopCondition(
      loopLocals.indexLocal, loopLocals.countLocal, instructionCount, emitInstruction);
  const auto loopMatch = emitMapLookupLoopMatchCheck(
      ptrLocal, loopLocals.indexLocal, keyLocal, mapKeyKind, instructionCount, emitInstruction);
  emitMapLookupLoopAdvanceAndPatch(
      loopMatch.jumpNotMatch,
      loopCondition.jumpLoopEnd,
      loopMatch.jumpFound,
      loopCondition.loopStart,
      loopLocals.indexLocal,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return loopLocals;
}

void emitMapLookupAccessEpilogue(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    emitMapLookupAtKeyNotFoundGuard(
        indexLocal, countLocal, emitMapKeyNotFound, instructionCount, emitInstruction, patchInstructionImm);
  }
  emitMapLookupValueLoad(ptrLocal, indexLocal, emitInstruction);
}

void emitMapLookupContainsResult(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
}

bool emitMapLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  int32_t ptrLocal = -1;
  if (!emitMapLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitMapLookupKeyLocal(
          mapKeyKind,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          resolveStringTableTarget,
          inferExprKind,
          emitExpr,
          [&](int32_t stringIndex) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)); },
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          keyLocal,
          error)) {
    return false;
  }

  const auto loopLocals = emitMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, mapKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitMapLookupAccessEpilogue(
      accessName,
      ptrLocal,
      loopLocals.indexLocal,
      loopLocals.countLocal,
      emitMapKeyNotFound,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return true;
}

bool emitMapLookupContains(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  int32_t ptrLocal = -1;
  if (!emitMapLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitMapLookupKeyLocal(
          mapKeyKind,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          resolveStringTableTarget,
          inferExprKind,
          emitExpr,
          [&](int32_t stringIndex) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)); },
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          keyLocal,
          error)) {
    return false;
  }

  const auto loopLocals = emitMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, mapKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitMapLookupContainsResult(loopLocals.indexLocal, loopLocals.countLocal, emitInstruction);
  return true;
}

bool emitMapLookupTryAt(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  int32_t ptrLocal = -1;
  if (!emitMapLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitMapLookupKeyLocal(
          mapKeyKind,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          resolveStringTableTarget,
          inferExprKind,
          emitExpr,
          [&](int32_t stringIndex) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)); },
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          keyLocal,
          error)) {
    return false;
  }

  const auto loopLocals = emitMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, mapKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
  emitInstruction(IrOpcode::CmpEqI32, 0);
  const size_t jumpFound = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);

  emitInstruction(IrOpcode::PushI64, 4294967296ull);
  const size_t jumpEnd = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);

  const size_t foundIndex = instructionCount();
  patchInstructionImm(jumpFound, static_cast<uint64_t>(foundIndex));
  emitMapLookupValueLoad(ptrLocal, loopLocals.indexLocal, emitInstruction);

  const size_t endIndex = instructionCount();
  patchInstructionImm(jumpEnd, static_cast<uint64_t>(endIndex));
  return true;
}

void emitStringAccessLoad(
    const std::string &accessName,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    size_t stringLength,
    int32_t stringIndex,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    if (indexKind != LocalInfo::ValueKind::UInt64) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
      emitInstruction(pushZeroForIndex(indexKind), 0);
      emitInstruction(cmpLtForIndex(indexKind), 0);
      const size_t jumpNonNegative = instructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitStringIndexOutOfBounds();
      patchInstructionImm(jumpNonNegative, static_cast<uint64_t>(instructionCount()));
    }

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    const IrOpcode lengthOp =
        (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
    emitInstruction(lengthOp, static_cast<uint64_t>(stringLength));
    emitInstruction(cmpGeForIndex(indexKind), 0);
    const size_t jumpInRange = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitStringIndexOutOfBounds();
    patchInstructionImm(jumpInRange, static_cast<uint64_t>(instructionCount()));
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadStringByte, static_cast<uint64_t>(stringIndex));
}

void emitArrayVectorAccessLoad(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    bool isVectorTarget,
    uint64_t arrayHeaderSlots,
    int32_t elementSlotCount,
    bool loadElementValue,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    const int32_t countLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal));

    if (indexKind != LocalInfo::ValueKind::UInt64) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
      emitInstruction(pushZeroForIndex(indexKind), 0);
      emitInstruction(cmpLtForIndex(indexKind), 0);
      const size_t jumpNonNegative = instructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitArrayIndexOutOfBounds();
      patchInstructionImm(jumpNonNegative, static_cast<uint64_t>(instructionCount()));
    }

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
    emitInstruction(cmpGeForIndex(indexKind), 0);
    const size_t jumpInRange = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitArrayIndexOutOfBounds();
    patchInstructionImm(jumpInRange, static_cast<uint64_t>(instructionCount()));
  }

  int32_t basePtrLocal = ptrLocal;
  if (isVectorTarget) {
    basePtrLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(2 * IrSlotBytes));
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(basePtrLocal));
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(basePtrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  if (!isVectorTarget) {
    if (elementSlotCount > 1) {
      emitInstruction(pushOneForIndex(indexKind), static_cast<uint64_t>(elementSlotCount));
      emitInstruction(mulForIndex(indexKind), 0);
    }
    emitInstruction(pushOneForIndex(indexKind), arrayHeaderSlots);
    emitInstruction(addForIndex(indexKind), 0);
  } else if (elementSlotCount > 1) {
    emitInstruction(pushOneForIndex(indexKind), static_cast<uint64_t>(elementSlotCount));
    emitInstruction(mulForIndex(indexKind), 0);
  }
  emitInstruction(pushOneForIndex(indexKind), IrSlotBytesI32);
  emitInstruction(mulForIndex(indexKind), 0);
  emitInstruction(IrOpcode::AddI64, 0);
  if (loadElementValue) {
    emitInstruction(IrOpcode::LoadIndirect, 0);
  }
}

MapLookupLoopLocals emitMapLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  MapLookupLoopLocals locals;
  locals.countLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.countLocal));

  locals.indexLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.indexLocal));
  return locals;
}

MapLookupLoopConditionAnchors emitMapLookupLoopCondition(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  MapLookupLoopConditionAnchors anchors;
  anchors.loopStart = instructionCount();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
  anchors.jumpLoopEnd = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  return anchors;
}

MapLookupLoopMatchAnchors emitMapLookupLoopMatchCheck(
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
  emitInstruction(mapKeyCompareOpcode(mapKeyKind), 0);

  MapLookupLoopMatchAnchors anchors;
  anchors.jumpNotMatch = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  anchors.jumpFound = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);
  return anchors;
}

void emitMapLookupLoopAdvanceAndPatch(
    size_t jumpNotMatch,
    size_t jumpLoopEnd,
    size_t jumpFound,
    size_t loopStart,
    int32_t indexLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  const size_t notMatchIndex = instructionCount();
  patchInstructionImm(jumpNotMatch, static_cast<uint64_t>(notMatchIndex));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(loopStart));

  const size_t loopEndIndex = instructionCount();
  patchInstructionImm(jumpLoopEnd, static_cast<uint64_t>(loopEndIndex));
  patchInstructionImm(jumpFound, static_cast<uint64_t>(loopEndIndex));
}

void emitMapLookupAtKeyNotFoundGuard(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpEqI32, 0);
  const size_t jumpKeyFound = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  emitMapKeyNotFound();
  const size_t keyFoundIndex = instructionCount();
  patchInstructionImm(jumpKeyFound, static_cast<uint64_t>(keyFoundIndex));
}

void emitMapLookupValueLoad(
    int32_t ptrLocal,
    int32_t indexLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
}

bool validateMapLookupKeyKind(LocalInfo::ValueKind mapKeyKind,
                              LocalInfo::ValueKind lookupKeyKind,
                              std::string &error) {
  if (lookupKeyKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires map lookup key type to match map key type";
    return false;
  }
  if (mapKeyKind == LocalInfo::ValueKind::String) {
    if (lookupKeyKind != LocalInfo::ValueKind::String) {
      error = "native backend requires map lookup key type to match map key type";
      return false;
    }
    return true;
  }
  if (lookupKeyKind == LocalInfo::ValueKind::String) {
    error = "native backend requires map lookup key to be numeric/bool";
    return false;
  }
  if (lookupKeyKind != mapKeyKind) {
    error = "native backend requires map lookup key type to match map key type";
    return false;
  }
  return true;
}

CountMethodFallbackResult tryEmitNonMethodCountFallback(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error,
    std::function<bool(const Expr &)> isCollectionAccessReceiverExpr) {
  if (isExplicitMapHelperFallbackPath(expr)) {
    return CountMethodFallbackResult::NotHandled;
  }
  std::string normalizedVectorHelperName;
  std::string normalizedMapHelperName;
  const bool hasVectorHelperAlias = resolveVectorHelperAliasName(expr, normalizedVectorHelperName);
  const bool hasMapHelperAlias = resolveMapHelperAliasName(expr, normalizedMapHelperName);
  const bool isCountCall = isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count");
  const bool isCapacityCall = isVectorBuiltinName(expr, "capacity");
  std::string accessName;
  const bool isCollectionAccessCall = getBuiltinArrayAccessName(expr, accessName);
  const bool isAccessCall = isCollectionAccessCall || isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref");
  const bool isVectorMutatorCall =
      isVectorBuiltinName(expr, "push") || isVectorBuiltinName(expr, "pop") || isVectorBuiltinName(expr, "reserve") ||
      isVectorBuiltinName(expr, "clear") || isVectorBuiltinName(expr, "remove_at") ||
      isVectorBuiltinName(expr, "remove_swap");
  auto expectedVectorMutatorArgCount = [&]() -> size_t {
    if (isVectorBuiltinName(expr, "pop") || isVectorBuiltinName(expr, "clear")) {
      return 1u;
    }
    return 2u;
  };
  size_t expectedArgCount = 1u;
  if (isAccessCall) {
    expectedArgCount = 2u;
  } else if (isVectorMutatorCall) {
    expectedArgCount = expectedVectorMutatorArgCount();
  }
  if (expr.isMethodCall || (!isCountCall && !isCapacityCall && !isAccessCall && !isVectorMutatorCall) ||
      expr.args.size() != expectedArgCount) {
    return CountMethodFallbackResult::NotHandled;
  }
  (void)isArrayCountCall;
  (void)isStringCountCall;

  auto hasNamedArgs = [&]() {
    for (const auto &argName : expr.argNames) {
      if (argName.has_value()) {
        return true;
      }
    }
    return false;
  };
  auto buildMethodExprForReceiverIndex = [&](size_t receiverIndex) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    if (hasVectorHelperAlias) {
      methodExpr.name = normalizedVectorHelperName;
    } else if (hasMapHelperAlias) {
      methodExpr.name = normalizedMapHelperName;
    }
    if (receiverIndex != 0 && receiverIndex < methodExpr.args.size()) {
      std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
      if (methodExpr.argNames.size() < methodExpr.args.size()) {
        methodExpr.argNames.resize(methodExpr.args.size());
      }
      std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
    }
    return methodExpr;
  };

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= expr.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };
  const bool hasNamedArgsValue = hasNamedArgs();
  if (hasNamedArgsValue) {
    bool hasValuesNamedReceiver = false;
    if (isVectorMutatorCall || isAccessCall) {
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }
  const bool probePositionalReorderedVectorMutatorReceiver =
      isVectorMutatorCall && !hasNamedArgsValue && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
       expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
       expr.args.front().kind == Expr::Kind::Name);
  if (probePositionalReorderedVectorMutatorReceiver) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool probePositionalReorderedAccessReceiver =
      isAccessCall && !hasNamedArgsValue && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
       expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
       (expr.args.front().kind == Expr::Kind::Name &&
        (!isCollectionAccessReceiverExpr ||
         !isCollectionAccessReceiverExpr(expr.args.front()))));
  if (probePositionalReorderedAccessReceiver) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool hasAlternativeCollectionReceiver =
      probePositionalReorderedAccessReceiver && isCollectionAccessReceiverExpr &&
      std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
        return index > 0 && index < expr.args.size() && isCollectionAccessReceiverExpr(expr.args[index]);
      });

  const std::string priorError = error;
  for (size_t receiverIndex : receiverIndices) {
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    const Definition *callee = resolveMethodCallDefinition(methodExpr);
    if (hasAlternativeCollectionReceiver && receiverIndex == 0 && callee != nullptr) {
      continue;
    }
    const auto emitResult = emitResolvedInlineDefinitionCall(
        methodExpr, callee, emitInlineDefinitionCall, error);
    if (emitResult == ResolvedInlineCallResult::Emitted) {
      return CountMethodFallbackResult::Emitted;
    }
    if (emitResult == ResolvedInlineCallResult::Error) {
      return CountMethodFallbackResult::Error;
    }
    error = priorError;
  }
  return CountMethodFallbackResult::NoCallee;
}

bool buildOrderedCallArguments(const Expr &callExpr,
                               const std::vector<Expr> &params,
                               std::vector<const Expr *> &ordered,
                               std::string &error) {
  const std::string callName = callExpr.name.empty() ? "<anonymous>" : callExpr.name;
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < callExpr.args.size(); ++i) {
    if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
      const std::string &name = *callExpr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &callExpr.args[i];
      continue;
    }
    while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= ordered.size()) {
      error = "argument count mismatch for " + callName;
      return false;
    }
    ordered[positionalIndex] = &callExpr.args[i];
    ++positionalIndex;
  }

  for (size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
      continue;
    }
    error = "argument count mismatch for " + callName;
    return false;
  }
  return true;
}

bool buildOrderedCallArgumentsWithPackedArgs(const Expr &callExpr,
                                            const std::vector<Expr> &params,
                                            std::vector<const Expr *> &ordered,
                                            std::vector<const Expr *> &packedArgs,
                                            size_t &packedParamIndex,
                                            std::string &error) {
  const std::string callName = callExpr.name.empty() ? "<anonymous>" : callExpr.name;
  ordered.assign(params.size(), nullptr);
  packedArgs.clear();
  packedParamIndex = params.size();
  if (!params.empty() && isArgsPackParam(params.back())) {
    packedParamIndex = params.size() - 1;
  }

  size_t positionalIndex = 0;
  for (size_t i = 0; i < callExpr.args.size(); ++i) {
    if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
      const std::string &name = *callExpr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (index == packedParamIndex) {
        error = "named arguments cannot bind variadic parameter: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &callExpr.args[i];
      continue;
    }

    if (callExpr.args[i].isSpread) {
      if (packedParamIndex >= params.size()) {
        error = "spread argument requires variadic parameter";
        return false;
      }
      packedArgs.push_back(&callExpr.args[i]);
      continue;
    }

    while (positionalIndex < params.size() && positionalIndex != packedParamIndex && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (packedParamIndex < params.size() && positionalIndex >= packedParamIndex) {
      packedArgs.push_back(&callExpr.args[i]);
      continue;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch for " + callName;
      return false;
    }
    ordered[positionalIndex] = &callExpr.args[i];
    ++positionalIndex;
  }

  for (size_t i = 0; i < params.size(); ++i) {
    if (i == packedParamIndex) {
      continue;
    }
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
      continue;
    }
    error = "argument count mismatch for " + callName;
    return false;
  }

  return true;
}

bool definitionHasTransform(const Definition &def, const std::string &transformName) {
  for (const auto &transform : def.transforms) {
    if (transform.name == transformName) {
      return true;
    }
  }
  return false;
}

bool isStructTransformName(const std::string &name) {
  return name == "struct" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding";
}

bool isStructDefinition(const Definition &def) {
  auto hasTypeLikeName = [&]() -> bool {
    if (def.fullPath.empty()) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    const std::string_view name = (slash == std::string::npos) ? std::string_view(def.fullPath)
                                                                : std::string_view(def.fullPath).substr(slash + 1);
    if (name.empty()) {
      return false;
    }
    return std::isupper(static_cast<unsigned char>(name.front())) != 0;
  };
  bool hasStruct = false;
  bool hasReturn = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturn = true;
    }
    if (isStructTransformName(transform.name)) {
      hasStruct = true;
    }
  }
  if (hasStruct) {
    return true;
  }
  if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  if (def.statements.empty()) {
    return hasTypeLikeName();
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

bool isStructHelperDefinition(const Definition &def,
                              const std::unordered_set<std::string> &structNames,
                              std::string &parentStructPathOut) {
  parentStructPathOut.clear();
  if (!def.isNested) {
    return false;
  }
  if (structNames.count(def.fullPath) > 0) {
    return false;
  }
  const size_t slash = def.fullPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }
  const std::string parent = def.fullPath.substr(0, slash);
  if (structNames.count(parent) == 0) {
    return false;
  }
  parentStructPathOut = parent;
  return true;
}

Expr makeStructHelperThisParam(const std::string &structPath, bool isMutable) {
  Expr param;
  param.kind = Expr::Kind::Name;
  param.isBinding = true;
  param.name = "this";
  Transform typeTransform;
  typeTransform.name = "Reference";
  typeTransform.templateArgs.push_back(structPath);
  param.transforms.push_back(std::move(typeTransform));
  if (isMutable) {
    Transform mutTransform;
    mutTransform.name = "mut";
    param.transforms.push_back(std::move(mutTransform));
  }
  return param;
}

bool isStaticFieldBinding(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

bool collectInstanceStructFieldParams(const Definition &structDef,
                                      std::vector<Expr> &paramsOut,
                                      std::string &error) {
  paramsOut.clear();
  paramsOut.reserve(structDef.statements.size());
  for (const auto &param : structDef.statements) {
    if (!param.isBinding) {
      error = "struct definitions may only contain field bindings: " + structDef.fullPath;
      return false;
    }
    if (isStaticFieldBinding(param)) {
      continue;
    }
    paramsOut.push_back(param);
  }
  return true;
}

bool buildInlineCallParameterList(const Definition &callee,
                                  const std::unordered_set<std::string> &structNames,
                                  std::vector<Expr> &paramsOut,
                                  std::string &error) {
  paramsOut.clear();
  if (isStructDefinition(callee)) {
    return collectInstanceStructFieldParams(callee, paramsOut, error);
  }

  std::string helperParent;
  if (!isStructHelperDefinition(callee, structNames, helperParent) ||
      definitionHasTransform(callee, "static")) {
    paramsOut = callee.parameters;
    return true;
  }

  paramsOut.reserve(callee.parameters.size() + 1);
  paramsOut.push_back(makeStructHelperThisParam(
      helperParent,
      definitionHasTransform(callee, "mut")));
  for (const auto &param : callee.parameters) {
    paramsOut.push_back(param);
  }
  return true;
}

bool buildInlineCallOrderedArguments(const Expr &callExpr,
                                     const Definition &callee,
                                     const std::unordered_set<std::string> &structNames,
                                     const LocalMap &callerLocals,
                                     std::vector<Expr> &paramsOut,
                                     std::vector<const Expr *> &orderedArgsOut,
                                     std::vector<const Expr *> &packedArgsOut,
                                     size_t &packedParamIndexOut,
                                     std::string &error) {
  if (!buildInlineCallParameterList(callee, structNames, paramsOut, error)) {
    return false;
  }
  if (callExpr.isMethodCall && !callExpr.args.empty()) {
    const Expr &receiver = callExpr.args.front();
    if (receiver.kind == Expr::Kind::Name && callerLocals.find(receiver.name) == callerLocals.end()) {
      const size_t methodSlash = callee.fullPath.find_last_of('/');
      if (methodSlash != std::string::npos && methodSlash > 0) {
        const std::string receiverPath = callee.fullPath.substr(0, methodSlash);
        const size_t receiverSlash = receiverPath.find_last_of('/');
        const std::string receiverTypeName =
            receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
        if (receiverTypeName == receiver.name) {
          Expr trimmedCallExpr = callExpr;
          trimmedCallExpr.args.erase(trimmedCallExpr.args.begin());
          if (!trimmedCallExpr.argNames.empty()) {
            trimmedCallExpr.argNames.erase(trimmedCallExpr.argNames.begin());
          }
          return buildOrderedCallArgumentsWithPackedArgs(
              trimmedCallExpr, paramsOut, orderedArgsOut, packedArgsOut, packedParamIndexOut, error);
        }
      }
    }
  }
  return buildOrderedCallArgumentsWithPackedArgs(
      callExpr, paramsOut, orderedArgsOut, packedArgsOut, packedParamIndexOut, error);
}

std::string resolveDefinitionNamespacePrefix(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath) {
  const Definition *definition = resolveDefinitionByPath(defMap, definitionPath);
  if (definition == nullptr) {
    return "";
  }
  return definition->namespacePrefix;
}

const Definition *resolveDefinitionByPath(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath) {
  auto defIt = defMap.find(definitionPath);
  if (defIt == defMap.end()) {
    return nullptr;
  }
  return defIt->second;
}

} // namespace primec::ir_lowerer
