#include "IrLowererCallHelpers.h"

#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool usesVectorBackedKeyValueStorageLayout(std::string_view mapStructTypeName) {
  const std::string path(mapStructTypeName);
  const std::string experimentalRoot = mapBackingTypePath();
  return path == experimentalRoot || path.rfind(experimentalRoot + "__", 0) == 0;
}

void emitExperimentalMapVectorDataPtrLoad(
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

void emitExperimentalMapKeyLoad(
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

void emitExperimentalMapPayloadLoad(
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

KeyValueLookupLoopLocals emitExperimentalMapLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  KeyValueLookupLoopLocals locals;
  locals.countLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.countLocal));

  const int32_t keysDataPtrLocal = allocTempLocal();
  emitExperimentalMapVectorDataPtrLoad(ptrLocal, 2, keysDataPtrLocal, emitInstruction);

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

bool emitExperimentalMapLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind keyValueKeyKind,
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
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
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
  emitExperimentalMapVectorDataPtrLoad(ptrLocal, 6, payloadDataPtrLocal, emitInstruction);
  emitExperimentalMapPayloadLoad(payloadDataPtrLocal, loopLocals.indexLocal, emitInstruction);
  return true;
}

bool emitExperimentalMapLookupContains(
    LocalInfo::ValueKind keyValueKeyKind,
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
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitKeyValueLookupContainsResult(loopLocals.indexLocal, loopLocals.countLocal, emitInstruction);
  return true;
}

bool emitExperimentalMapLookupTryAt(
    LocalInfo::ValueKind keyValueKeyKind,
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
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);

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
  emitExperimentalMapVectorDataPtrLoad(ptrLocal, 6, payloadDataPtrLocal, emitInstruction);
  emitExperimentalMapPayloadLoad(payloadDataPtrLocal, loopLocals.indexLocal, emitInstruction);

  const size_t endIndex = instructionCount();
  patchInstructionImm(jumpEnd, static_cast<uint64_t>(endIndex));
  return true;
}

} // namespace

IrOpcode keyValueKeyCompareOpcode(LocalInfo::ValueKind keyValueKeyKind) {
  if (keyValueKeyKind == LocalInfo::ValueKind::Int64 || keyValueKeyKind == LocalInfo::ValueKind::UInt64) {
    return IrOpcode::CmpEqI64;
  }
  if (keyValueKeyKind == LocalInfo::ValueKind::Float64) {
    return IrOpcode::CmpEqF64;
  }
  if (keyValueKeyKind == LocalInfo::ValueKind::Float32) {
    return IrOpcode::CmpEqF32;
  }
  return IrOpcode::CmpEqI32;
}

KeyValueLookupStringKeyResult tryResolveKeyValueLookupStringKey(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    int32_t &stringIndexOut,
    std::string &error) {
  (void)error;
  if (keyValueKeyKind != LocalInfo::ValueKind::String) {
    return KeyValueLookupStringKeyResult::NotHandled;
  }
  const LocalInfo::ValueKind lookupKeyKind = inferExprKind(lookupKeyExpr, localsIn);
  if (lookupKeyKind != LocalInfo::ValueKind::Unknown &&
      lookupKeyKind != LocalInfo::ValueKind::String) {
    return KeyValueLookupStringKeyResult::NotHandled;
  }
  size_t length = 0;
  if (!resolveStringTableTarget(lookupKeyExpr, localsIn, stringIndexOut, length)) {
    return KeyValueLookupStringKeyResult::NotHandled;
  }
  return KeyValueLookupStringKeyResult::Resolved;
}

KeyValueLookupKeyLocalEmitResult tryEmitKeyValueLookupStringKeyLocal(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error) {
  int32_t stringIndex = -1;
  const auto resolveResult = tryResolveKeyValueLookupStringKey(
      keyValueKeyKind, lookupKeyExpr, localsIn, resolveStringTableTarget, inferExprKind, stringIndex, error);
  if (resolveResult == KeyValueLookupStringKeyResult::NotHandled) {
    return KeyValueLookupKeyLocalEmitResult::NotHandled;
  }
  if (resolveResult == KeyValueLookupStringKeyResult::Error) {
    return KeyValueLookupKeyLocalEmitResult::Error;
  }
  emitPushI32(stringIndex);
  emitStoreLocal(keyLocal);
  return KeyValueLookupKeyLocalEmitResult::Emitted;
}

bool emitKeyValueLookupNonStringKeyLocal(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error) {
  const LocalInfo::ValueKind lookupKeyKind = inferExprKind(lookupKeyExpr, localsIn);
  if (!validateKeyValueLookupKeyKind(keyValueKeyKind, lookupKeyKind, error)) {
    return false;
  }
  if (!emitExpr(lookupKeyExpr, localsIn)) {
    return false;
  }
  emitStoreLocal(keyLocal);
  return true;
}

bool emitKeyValueLookupKeyLocal(
    LocalInfo::ValueKind keyValueKeyKind,
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
  if (keyValueKeyKind == LocalInfo::ValueKind::String) {
    const LocalInfo::ValueKind lookupKeyKind = inferExprKind(lookupKeyExpr, localsIn);
    if (lookupKeyKind != LocalInfo::ValueKind::Unknown &&
        lookupKeyKind != LocalInfo::ValueKind::String) {
      return validateKeyValueLookupKeyKind(keyValueKeyKind, lookupKeyKind, error);
    }

    int32_t stringIndex = -1;
    size_t length = 0;
    if (resolveStringTableTarget(lookupKeyExpr, localsIn, stringIndex, length)) {
      emitPushI32(stringIndex);
      emitStoreLocal(keyLocalOut);
      return true;
    }
    if (!validateKeyValueLookupKeyKind(keyValueKeyKind, lookupKeyKind, error)) {
      return false;
    }
    if (!emitExpr(lookupKeyExpr, localsIn)) {
      return false;
    }
    emitStoreLocal(keyLocalOut);
    return true;
  }

  const auto stringLookupKeyEmitResult = tryEmitKeyValueLookupStringKeyLocal(
      keyValueKeyKind,
      lookupKeyExpr,
      localsIn,
      resolveStringTableTarget,
      inferExprKind,
      emitPushI32,
      emitStoreLocal,
      keyLocalOut,
      error);
  if (stringLookupKeyEmitResult == KeyValueLookupKeyLocalEmitResult::Error) {
    return false;
  }
  if (stringLookupKeyEmitResult == KeyValueLookupKeyLocalEmitResult::Emitted) {
    return true;
  }
  return emitKeyValueLookupNonStringKeyLocal(
      keyValueKeyKind, lookupKeyExpr, localsIn, inferExprKind, emitExpr, emitStoreLocal, keyLocalOut, error);
}

bool emitKeyValueLookupTargetPointerLocal(
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

KeyValueLookupLoopLocals emitKeyValueLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  const auto loopLocals = emitKeyValueLookupLoopLocals(ptrLocal, allocTempLocal, emitInstruction);
  const auto loopCondition = emitKeyValueLookupLoopCondition(
      loopLocals.indexLocal, loopLocals.countLocal, instructionCount, emitInstruction);
  const auto loopMatch = emitKeyValueLookupLoopMatchCheck(
      ptrLocal, loopLocals.indexLocal, keyLocal, keyValueKeyKind, instructionCount, emitInstruction);
  emitKeyValueLookupLoopAdvanceAndPatch(
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

void emitKeyValueLookupAccessEpilogue(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    emitKeyValueLookupAtKeyNotFoundGuard(
        indexLocal, countLocal, emitMapKeyNotFound, instructionCount, emitInstruction, patchInstructionImm);
  }
  emitKeyValueLookupValueLoad(ptrLocal, indexLocal, emitInstruction);
}

void emitKeyValueLookupContainsResult(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
}

bool emitKeyValueLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::string &mapStructTypeName,
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
  if (usesVectorBackedKeyValueStorageLayout(mapStructTypeName)) {
    return emitExperimentalMapLookupAccess(
        accessName,
        keyValueKeyKind,
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
        error);
  }

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

  const auto loopLocals = emitKeyValueLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitKeyValueLookupAccessEpilogue(
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

bool emitKeyValueLookupContains(
    LocalInfo::ValueKind keyValueKeyKind,
    const std::string &mapStructTypeName,
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
  if (usesVectorBackedKeyValueStorageLayout(mapStructTypeName)) {
    return emitExperimentalMapLookupContains(
        keyValueKeyKind,
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
        error);
  }

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

  const auto loopLocals = emitKeyValueLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitKeyValueLookupContainsResult(loopLocals.indexLocal, loopLocals.countLocal, emitInstruction);
  return true;
}

bool emitKeyValueLookupTryAt(
    LocalInfo::ValueKind keyValueKeyKind,
    const std::string &mapStructTypeName,
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
  if (usesVectorBackedKeyValueStorageLayout(mapStructTypeName)) {
    return emitExperimentalMapLookupTryAt(
        keyValueKeyKind,
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
        error);
  }

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

  const auto loopLocals = emitKeyValueLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);

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
  emitKeyValueLookupValueLoad(ptrLocal, loopLocals.indexLocal, emitInstruction);

  const size_t endIndex = instructionCount();
  patchInstructionImm(jumpEnd, static_cast<uint64_t>(endIndex));
  return true;
}

bool emitBuiltinCanonicalMapInsertOverwriteOrGrow(
    int32_t valuesLocal,
    int32_t valuesWrapperLocal,
    int32_t ptrLocal,
    int32_t keyLocal,
    int32_t valueLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  // Some call shapes (for example temporary value-return map receivers) can
  // surface a null map pointer on first insert. Materialize a one-entry map
  // directly in that case before the generic lookup/grow path, which assumes a
  // non-null map header pointer.
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  const size_t jumpIfNotNullPtr = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);

  const int32_t nullInsertPtrLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 3);
  emitInstruction(IrOpcode::HeapAlloc, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(nullInsertPtrLocal));

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(nullInsertPtrLocal));
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::StoreIndirect, 0);
  emitInstruction(IrOpcode::Pop, 0);

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(nullInsertPtrLocal));
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
  emitInstruction(IrOpcode::StoreIndirect, 0);
  emitInstruction(IrOpcode::Pop, 0);

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(nullInsertPtrLocal));
  emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(2 * IrSlotBytesI32));
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
  emitInstruction(IrOpcode::StoreIndirect, 0);
  emitInstruction(IrOpcode::Pop, 0);

  if (valuesLocal >= 0) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(nullInsertPtrLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
  } else if (valuesWrapperLocal >= 0) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valuesWrapperLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(nullInsertPtrLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);
  }
  const size_t jumpAfterNullInsert = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);

  patchInstructionImm(jumpIfNotNullPtr, static_cast<uint64_t>(instructionCount()));

  const auto loopLocals = emitKeyValueLookupLoopSearchScaffold(
      ptrLocal, keyLocal, keyValueKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
  emitInstruction(IrOpcode::CmpEqI32, 0);
  const size_t jumpFound = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);

  size_t jumpAfterGenericGrow = 0;
  constexpr uint64_t kHeapAddressTag = 1ull << 63;

    const int32_t genericNewCountLocal = allocTempLocal();
    const int32_t genericNewSlotCountLocal = allocTempLocal();
    const int32_t genericOldSlotCountLocal = allocTempLocal();
    const int32_t genericGrownPtrLocal = allocTempLocal();
    const int32_t genericIsHeapBackedLocal = allocTempLocal();

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::AddI32, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericNewCountLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericNewCountLocal));
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::AddI32, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericNewSlotCountLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::AddI32, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericOldSlotCountLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::PushI64, kHeapAddressTag);
    emitInstruction(IrOpcode::CmpGeU64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericIsHeapBackedLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericIsHeapBackedLocal));
    const size_t jumpGenericLocalCopy = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericNewSlotCountLocal));
    emitInstruction(IrOpcode::HeapRealloc, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    const size_t jumpGenericAfterAllocate = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);

    const size_t genericLocalCopyIndex = instructionCount();
    patchInstructionImm(jumpGenericLocalCopy, static_cast<uint64_t>(genericLocalCopyIndex));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericNewSlotCountLocal));
    emitInstruction(IrOpcode::HeapAlloc, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericGrownPtrLocal));

    const int32_t genericCopyIndexLocal = allocTempLocal();
    const int32_t genericSrcPtrLocal = allocTempLocal();
    const int32_t genericDestPtrLocal = allocTempLocal();
    const int32_t genericCopyValueLocal = allocTempLocal();

    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericCopyIndexLocal));

    const size_t genericCopyLoopStart = instructionCount();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericCopyIndexLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericOldSlotCountLocal));
    emitInstruction(IrOpcode::CmpLtI32, 0);
    const size_t jumpGenericCopyDone = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericCopyIndexLocal));
    emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericSrcPtrLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericCopyIndexLocal));
    emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericDestPtrLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericSrcPtrLocal));
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericCopyValueLocal));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericDestPtrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericCopyValueLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericCopyIndexLocal));
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::AddI32, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(genericCopyIndexLocal));
    emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(genericCopyLoopStart));

    const size_t genericCopyDoneIndex = instructionCount();
    patchInstructionImm(jumpGenericCopyDone, static_cast<uint64_t>(genericCopyDoneIndex));
    patchInstructionImm(jumpGenericAfterAllocate, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericNewCountLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::AddI32, 0);
    emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::AddI32, 0);
    emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
    emitInstruction(IrOpcode::MulI32, 0);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);

  if (valuesLocal >= 0) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
  } else if (valuesWrapperLocal >= 0) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valuesWrapperLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);
  }
  jumpAfterGenericGrow = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);

  const size_t foundIndex = instructionCount();
  patchInstructionImm(jumpFound, static_cast<uint64_t>(foundIndex));

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.indexLocal));
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
  emitInstruction(IrOpcode::StoreIndirect, 0);
  emitInstruction(IrOpcode::Pop, 0);

  patchInstructionImm(jumpAfterGenericGrow, static_cast<uint64_t>(instructionCount()));
  patchInstructionImm(jumpAfterNullInsert, static_cast<uint64_t>(instructionCount()));
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
  if (accessName == "at" || (accessName == "at_unsafe" && isVectorTarget)) {
    const int32_t countLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    // Vector<T> uses 1-indexed layout: slot 0=header, slot 1=count, slot 2=capacity, slot 3=data.
    // at: check index < count (slot 1), at_unsafe: check index < capacity (slot 2).
    if (accessName == "at_unsafe") {
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(2 * IrSlotBytes));
      emitInstruction(IrOpcode::AddI64, 0);
    } else if (isVectorTarget) {
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(IrSlotBytes));
      emitInstruction(IrOpcode::AddI64, 0);
    }
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
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(3 * IrSlotBytes));  // data at slot 3
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

KeyValueLookupLoopLocals emitKeyValueLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  KeyValueLookupLoopLocals locals;
  locals.countLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.countLocal));

  locals.indexLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.indexLocal));
  return locals;
}

KeyValueLookupLoopConditionAnchors emitKeyValueLookupLoopCondition(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  KeyValueLookupLoopConditionAnchors anchors;
  anchors.loopStart = instructionCount();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
  anchors.jumpLoopEnd = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  return anchors;
}

KeyValueLookupLoopMatchAnchors emitKeyValueLookupLoopMatchCheck(
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind keyValueKeyKind,
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
  emitInstruction(keyValueKeyCompareOpcode(keyValueKeyKind), 0);

  KeyValueLookupLoopMatchAnchors anchors;
  anchors.jumpNotMatch = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  anchors.jumpFound = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);
  return anchors;
}

void emitKeyValueLookupLoopAdvanceAndPatch(
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

void emitKeyValueLookupAtKeyNotFoundGuard(
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

void emitKeyValueLookupValueLoad(
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

bool validateKeyValueLookupKeyKind(LocalInfo::ValueKind keyValueKeyKind,
                              LocalInfo::ValueKind lookupKeyKind,
                              std::string &error) {
  if (lookupKeyKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires map lookup key type to match map key type";
    return false;
  }
  if (keyValueKeyKind == LocalInfo::ValueKind::String) {
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
  if (lookupKeyKind != keyValueKeyKind) {
    error = "native backend requires map lookup key type to match map key type";
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer
