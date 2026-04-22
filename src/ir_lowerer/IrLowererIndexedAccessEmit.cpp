#include "IrLowererCallHelpers.h"

#include <limits>

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isExperimentalVectorStructPath(const std::string &structPath) {
  return structPath == "/std/collections/experimental_vector/Vector" ||
         structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool usesBuiltinVectorValueStorage(const ArrayVectorAccessTargetInfo &targetInfo) {
  return targetInfo.isVectorTarget &&
         (targetInfo.structTypeName.empty() || targetInfo.structTypeName == "/vector" ||
          isExperimentalVectorStructPath(targetInfo.structTypeName));
}

} // namespace

MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto mapTargetInfo = resolveMapAccessTargetInfo(
      targetExpr, localsIn, resolveCallMapAccessTargetInfo);
  if (!mapTargetInfo.isMapTarget) {
    return MapAccessLookupEmitResult::NotHandled;
  }
  if (!validateMapAccessTargetInfo(mapTargetInfo, accessName, error)) {
    return MapAccessLookupEmitResult::Error;
  }
  if (!emitMapLookupAccess(
          accessName,
          mapTargetInfo.mapKeyKind,
          mapTargetInfo.structTypeName,
          targetExpr,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          resolveStringTableTarget,
          inferExprKind,
          emitMapKeyNotFound,
          instructionCount,
          emitInstruction,
          patchInstructionImm,
          error)) {
    return MapAccessLookupEmitResult::Error;
  }
  return MapAccessLookupEmitResult::Emitted;
}

MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
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
  return tryEmitMapAccessLookup(
      accessName,
      targetExpr,
      lookupKeyExpr,
      localsIn,
      allocTempLocal,
      emitExpr,
      resolveStringTableTarget,
      {},
      inferExprKind,
      emitMapKeyNotFound,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

MapAccessLookupEmitResult tryEmitMapContainsLookup(
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto mapTargetInfo = resolveMapAccessTargetInfo(
      targetExpr, localsIn, resolveCallMapAccessTargetInfo);
  if (!mapTargetInfo.isMapTarget) {
    return MapAccessLookupEmitResult::NotHandled;
  }
  if (!validateMapAccessTargetInfo(mapTargetInfo, "contains", error)) {
    return MapAccessLookupEmitResult::Error;
  }
  if (!emitMapLookupContains(
          mapTargetInfo.mapKeyKind,
          mapTargetInfo.structTypeName,
          targetExpr,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          resolveStringTableTarget,
          inferExprKind,
          instructionCount,
          emitInstruction,
          patchInstructionImm,
          error)) {
    return MapAccessLookupEmitResult::Error;
  }
  return MapAccessLookupEmitResult::Emitted;
}

StringTableAccessEmitResult tryEmitStringTableAccessLoad(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  int32_t stringIndex = -1;
  size_t stringLength = 0;
  if (!resolveStringTableTarget(targetExpr, localsIn, stringIndex, stringLength)) {
    return StringTableAccessEmitResult::NotHandled;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(indexExpr, localsIn, accessName, inferExprKind, indexKind, error)) {
    return StringTableAccessEmitResult::Error;
  }
  if (stringLength > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    error = "native backend string too large for indexing";
    return StringTableAccessEmitResult::Error;
  }

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(indexExpr, localsIn)) {
    return StringTableAccessEmitResult::Error;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  emitStringAccessLoad(
      accessName,
      indexLocal,
      indexKind,
      stringLength,
      stringIndex,
      emitStringIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return StringTableAccessEmitResult::Emitted;
}

namespace {

enum class DynamicStringAccessEmitResult {
  NotHandled,
  Emitted,
  Error,
};

} // namespace

DynamicStringAccessEmitResult tryEmitDynamicStringAccessLoad(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    size_t stringTableCount,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  bool isRuntimeStringTarget = false;
  if (targetExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(targetExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.valueKind == LocalInfo::ValueKind::String) {
      if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
        error = "native backend only supports entry argument indexing in print calls or string bindings";
        return DynamicStringAccessEmitResult::Error;
      }
      isRuntimeStringTarget = (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex);
    }
  } else {
    if (isEntryArgsName(targetExpr, localsIn)) {
      error = "native backend only supports entry argument indexing in print calls or string bindings";
      return DynamicStringAccessEmitResult::Error;
    }
    if (targetExpr.kind == Expr::Kind::Call) {
      std::string nestedAccessName;
      if (getBuiltinArrayAccessName(targetExpr, nestedAccessName) && targetExpr.args.size() == 2 &&
          isEntryArgsName(targetExpr.args.front(), localsIn)) {
        error = "native backend only supports entry argument indexing in print calls or string bindings";
        return DynamicStringAccessEmitResult::Error;
      }
    }
    isRuntimeStringTarget = inferExprKind(targetExpr, localsIn) == LocalInfo::ValueKind::String;
  }

  if (!isRuntimeStringTarget || stringTableCount == 0) {
    return DynamicStringAccessEmitResult::NotHandled;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(indexExpr, localsIn, accessName, inferExprKind, indexKind, error)) {
    return DynamicStringAccessEmitResult::Error;
  }

  const int32_t stringLocal = allocTempLocal();
  if (!emitExpr(targetExpr, localsIn)) {
    return DynamicStringAccessEmitResult::Error;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(stringLocal));

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(indexExpr, localsIn)) {
    return DynamicStringAccessEmitResult::Error;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  std::vector<size_t> jumpToEnd;
  jumpToEnd.reserve(stringTableCount);
  for (size_t stringIndex = 0; stringIndex < stringTableCount; ++stringIndex) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(stringLocal));
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(stringIndex));
    emitInstruction(IrOpcode::CmpEqI64, 0);
    const size_t jumpNext = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emitInstruction(IrOpcode::LoadStringByte, static_cast<uint64_t>(stringIndex));
    const size_t jumpEnd = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    patchInstructionImm(jumpNext, static_cast<uint64_t>(instructionCount()));
    jumpToEnd.push_back(jumpEnd);
  }

  emitStringIndexOutOfBounds();
  const size_t endIndex = instructionCount();
  for (size_t jumpIndex : jumpToEnd) {
    patchInstructionImm(jumpIndex, static_cast<uint64_t>(endIndex));
  }
  return DynamicStringAccessEmitResult::Emitted;
}

bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo, std::string &error) {
  const bool isStructArgsPackTarget =
      targetInfo.isArgsPackTarget && !targetInfo.isVectorTarget && !targetInfo.structTypeName.empty();
  const bool isWrappedStructArgsPackTarget =
      targetInfo.isArgsPackTarget && !targetInfo.isVectorTarget && !targetInfo.structTypeName.empty() &&
      (targetInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
       targetInfo.argsPackElementKind == LocalInfo::Kind::Reference);
  const bool isMapArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.isMapTarget && !targetInfo.isWrappedMapTarget;
  const bool isWrappedMapArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.isWrappedMapTarget;
  const bool isVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Vector;
  const bool isPointerVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
      targetInfo.isVectorTarget;
  const bool isPointerSoaVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
      targetInfo.isSoaVector;
  const bool isBorrowedVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
      targetInfo.isVectorTarget;
  const bool isBorrowedSoaVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
      targetInfo.isSoaVector;
  const bool isDirectSoaVectorStructTarget =
      !targetInfo.isArgsPackTarget && targetInfo.isSoaVector && !targetInfo.structTypeName.empty();
  if (!targetInfo.isArrayOrVectorTarget ||
      (targetInfo.elemKind == LocalInfo::ValueKind::Unknown && !isStructArgsPackTarget &&
       !isWrappedStructArgsPackTarget && !isMapArgsPackTarget && !isWrappedMapArgsPackTarget &&
       !isVectorArgsPackTarget && !isPointerVectorArgsPackTarget && !isPointerSoaVectorArgsPackTarget &&
       !isBorrowedVectorArgsPackTarget && !isBorrowedSoaVectorArgsPackTarget &&
       !isDirectSoaVectorStructTarget)) {
    error =
        "native backend only supports at() on numeric/bool/string arrays or vectors, plus "
        "args<Struct>/args<map<K, V>>/args<Pointer<Struct>>/args<Reference<Struct>>/"
        "args<Pointer<map<K, V>>>/args<Reference<map<K, V>>>/args<vector<T>>/"
        "args<Pointer<vector<T>>>/args<Reference<vector<T>>>/args<Pointer<soa_vector<T>>>/"
        "args<Reference<soa_vector<T>>> packs";
    return false;
  }
  return true;
}

bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(
      targetExpr, localsIn, resolveCallArrayVectorAccessTargetInfo);
  if (!validateArrayVectorAccessTargetInfo(arrayVectorTargetInfo, error)) {
    return false;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(indexExpr, localsIn, accessName, inferExprKind, indexKind, error)) {
    return false;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(targetExpr, localsIn)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(indexExpr, localsIn)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  const bool isWrappedStructArgsPackTarget =
      arrayVectorTargetInfo.isArgsPackTarget &&
      (arrayVectorTargetInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
       arrayVectorTargetInfo.argsPackElementKind == LocalInfo::Kind::Reference);
  const bool isInlineStructArgsPackTarget =
      arrayVectorTargetInfo.isArgsPackTarget &&
      !arrayVectorTargetInfo.structTypeName.empty() &&
      arrayVectorTargetInfo.elemSlotCount > 0 &&
      !isWrappedStructArgsPackTarget;
  const bool isInlineMapArgsPackTarget =
      arrayVectorTargetInfo.isArgsPackTarget &&
      arrayVectorTargetInfo.isMapTarget &&
      !arrayVectorTargetInfo.isWrappedMapTarget &&
      arrayVectorTargetInfo.elemSlotCount > 0;
  const bool targetUsesVectorStorageLayout =
      arrayVectorTargetInfo.isVectorTarget && !arrayVectorTargetInfo.isArgsPackTarget;
  const bool loadElementValue =
      !isInlineStructArgsPackTarget &&
      !isInlineMapArgsPackTarget &&
      (arrayVectorTargetInfo.structTypeName.empty() ||
       arrayVectorTargetInfo.isMapTarget ||
       arrayVectorTargetInfo.isWrappedMapTarget ||
       usesBuiltinVectorValueStorage(arrayVectorTargetInfo) ||
       isWrappedStructArgsPackTarget);

  emitArrayVectorAccessLoad(
      accessName,
      ptrLocal,
      indexLocal,
      indexKind,
      targetUsesVectorStorageLayout,
      1,
      (arrayVectorTargetInfo.elemSlotCount > 0) ? arrayVectorTargetInfo.elemSlotCount : 1,
      loadElementValue,
      allocTempLocal,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return true;
}

bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitArrayVectorIndexedAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      {},
      inferExprKind,
      allocTempLocal,
      emitExpr,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto stringTableAccessResult = tryEmitStringTableAccessLoad(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      inferExprKind,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
  if (stringTableAccessResult == StringTableAccessEmitResult::Error) {
    return false;
  }
  if (stringTableAccessResult == StringTableAccessEmitResult::Emitted) {
    return true;
  }

  const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(
      targetExpr, localsIn, resolveCallArrayVectorAccessTargetInfo);
  const auto mapTargetInfo = resolveMapAccessTargetInfo(
      targetExpr, localsIn, resolveCallMapAccessTargetInfo);
  std::string nestedAccessName;
  const bool isMapArgsPackElementTarget =
      arrayVectorTargetInfo.isArgsPackTarget &&
      targetExpr.kind == Expr::Kind::Call &&
      getBuiltinArrayAccessName(targetExpr, nestedAccessName) &&
      targetExpr.args.size() == 2 &&
      mapTargetInfo.isMapTarget;
  if (arrayVectorTargetInfo.isArgsPackTarget && !isMapArgsPackElementTarget) {
    return emitArrayVectorIndexedAccess(
        accessName,
        targetExpr,
        indexExpr,
        localsIn,
        resolveCallArrayVectorAccessTargetInfo,
        inferExprKind,
        allocTempLocal,
        emitExpr,
        emitArrayIndexOutOfBounds,
        instructionCount,
        emitInstruction,
        patchInstructionImm,
        error);
  }

  const auto mapLookupResult = tryEmitMapAccessLookup(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      allocTempLocal,
      emitExpr,
      resolveStringTableTarget,
      resolveCallMapAccessTargetInfo,
      inferExprKind,
      emitMapKeyNotFound,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
  if (mapLookupResult == MapAccessLookupEmitResult::Error) {
    return false;
  }
  if (mapLookupResult == MapAccessLookupEmitResult::Emitted) {
    return true;
  }

  const auto dynamicStringAccessResult = tryEmitDynamicStringAccessLoad(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      stringTableCount,
      inferExprKind,
      isEntryArgsName,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
  if (dynamicStringAccessResult == DynamicStringAccessEmitResult::Error) {
    return false;
  }
  if (dynamicStringAccessResult == DynamicStringAccessEmitResult::Emitted) {
    return true;
  }

  const auto nonLiteralStringTargetResult =
      validateNonLiteralStringAccessTarget(targetExpr, localsIn, isEntryArgsName, error);
  if (nonLiteralStringTargetResult == NonLiteralStringAccessTargetResult::Stop) {
    return false;
  }
  if (nonLiteralStringTargetResult == NonLiteralStringAccessTargetResult::Error) {
    return false;
  }

  return emitArrayVectorIndexedAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveCallArrayVectorAccessTargetInfo,
      inferExprKind,
      allocTempLocal,
      emitExpr,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitBuiltinArrayAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      0,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      inferExprKind,
      isEntryArgsName,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitBuiltinArrayAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      stringTableCount,
      {},
      {},
      inferExprKind,
      isEntryArgsName,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitBuiltinArrayAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      0,
      inferExprKind,
      isEntryArgsName,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

} // namespace primec::ir_lowerer
