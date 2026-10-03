#pragma once

// Helpers shared by the IrLowererAccessLoadHelpers*.cpp units (split out of
// IrLowererAccessLoadHelpers.cpp without changes).
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"

namespace primec::ir_lowerer {

namespace ir_lowerer_access_load_helpers_file_local {

inline bool usesVectorBackedKeyValueStorageLayout(std::string_view mapStructTypeName) {
  const std::string path(mapStructTypeName);
  const std::string experimentalRoot = mapBackingTypePath();
  const std::string canonicalRoot =
      collection_paths::memberPath(collection_paths::kMapFolder, "MapValue");
  return path == experimentalRoot || path.rfind(experimentalRoot + "__", 0) == 0 ||
         path == canonicalRoot || path.rfind(canonicalRoot + "__", 0) == 0;
}

inline bool usesCanonicalMapValueStorageLayout(std::string_view mapStructTypeName) {
  const std::string path(mapStructTypeName);
  const std::string canonicalRoot =
      collection_paths::memberPath(collection_paths::kMapFolder, "MapValue");
  return path == canonicalRoot || path.rfind(canonicalRoot + "__", 0) == 0;
}

inline void emitExperimentalMapVectorDataPtrLoad(
    int32_t ptrLocal,
    int32_t vectorSlotOffset,
    int32_t dataPtrLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  if (vectorSlotOffset != 2) {
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(vectorSlotOffset * IrSlotBytes));
    emitInstruction(IrOpcode::AddI64, 0);
  } else {
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(2 * IrSlotBytes));
    emitInstruction(IrOpcode::AddI64, 0);
  }
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(dataPtrLocal));
}

inline void emitExperimentalMapKeyLoad(
    int32_t keysDataPtrLocal,
    int32_t indexLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keysDataPtrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
}

inline void emitExperimentalMapPayloadLoad(
    int32_t payloadDataPtrLocal,
    int32_t indexLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(payloadDataPtrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
}

inline KeyValueLookupLoopLocals emitExperimentalMapLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    int32_t countSlotOffset,
    int32_t keysDataSlotOffset,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  KeyValueLookupLoopLocals locals;
  locals.countLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  if (countSlotOffset != 0) {
    emitInstruction(IrOpcode::PushI64,
                    static_cast<uint64_t>(countSlotOffset * IrSlotBytes));
    emitInstruction(IrOpcode::AddI64, 0);
  }
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.countLocal));

  const int32_t keysDataPtrLocal = allocTempLocal();
  emitExperimentalMapVectorDataPtrLoad(
      ptrLocal, keysDataSlotOffset, keysDataPtrLocal, emitInstruction);

  locals.indexLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.indexLocal));

  const auto loopCondition = emitKeyValueLookupLoopCondition(
      locals.indexLocal, locals.countLocal, instructionCount, emitInstruction);
  emitExperimentalMapKeyLoad(keysDataPtrLocal, locals.indexLocal, emitInstruction);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
  emitInstruction(keyValueKeyCompareOpcode(keyValueKeyKind), 0);
  const size_t jumpNotMatch = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  const size_t jumpFound = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);
  emitKeyValueLookupLoopAdvanceAndPatch(
      jumpNotMatch,
      loopCondition.jumpLoopEnd,
      jumpFound,
      loopCondition.loopStart,
      locals.indexLocal,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return locals;
}

inline bool emitExperimentalMapLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    int32_t countSlotOffset,
    int32_t keysDataSlotOffset,
    int32_t payloadDataSlotOffset,
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
  if (!emitKeyValueLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitKeyValueLookupKeyLocal(
          keyValueKeyKind,
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

  const auto loopLocals = emitExperimentalMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, countSlotOffset,
      keysDataSlotOffset, allocTempLocal, instructionCount, emitInstruction,
      patchInstructionImm);
  if (accessName == "at") {
    emitKeyValueLookupAtKeyNotFoundGuard(
        loopLocals.indexLocal,
        loopLocals.countLocal,
        emitMapKeyNotFound,
        instructionCount,
        emitInstruction,
        patchInstructionImm);
  }

  const int32_t payloadDataPtrLocal = allocTempLocal();
  emitExperimentalMapVectorDataPtrLoad(
      ptrLocal, payloadDataSlotOffset, payloadDataPtrLocal, emitInstruction);
  emitExperimentalMapPayloadLoad(payloadDataPtrLocal, loopLocals.indexLocal, emitInstruction);
  return true;
}

inline bool emitExperimentalMapLookupContains(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    int32_t countSlotOffset,
    int32_t keysDataSlotOffset,
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
  if (!emitKeyValueLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitKeyValueLookupKeyLocal(
          keyValueKeyKind,
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

  const auto loopLocals = emitExperimentalMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, countSlotOffset,
      keysDataSlotOffset, allocTempLocal, instructionCount, emitInstruction,
      patchInstructionImm);
  emitKeyValueLookupContainsResult(loopLocals.indexLocal, loopLocals.countLocal, emitInstruction);
  return true;
}

inline bool emitExperimentalMapLookupTryAt(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    int32_t countSlotOffset,
    int32_t keysDataSlotOffset,
    int32_t payloadDataSlotOffset,
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
  if (!emitKeyValueLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitKeyValueLookupKeyLocal(
          keyValueKeyKind,
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

  const auto loopLocals = emitExperimentalMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, countSlotOffset,
      keysDataSlotOffset, allocTempLocal, instructionCount, emitInstruction,
      patchInstructionImm);

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
  const int32_t payloadDataPtrLocal = allocTempLocal();
  emitExperimentalMapVectorDataPtrLoad(
      ptrLocal, payloadDataSlotOffset, payloadDataPtrLocal, emitInstruction);
  emitExperimentalMapPayloadLoad(payloadDataPtrLocal, loopLocals.indexLocal, emitInstruction);

  const size_t endIndex = instructionCount();
  patchInstructionImm(jumpEnd, static_cast<uint64_t>(endIndex));
  return true;
}

} // namespace ir_lowerer_access_load_helpers_file_local
} // namespace primec::ir_lowerer
