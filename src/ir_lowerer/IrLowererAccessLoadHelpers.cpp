#include "IrLowererCallHelpers.h"

#include "IrLowererIndexKindHelpers.h"

namespace primec::ir_lowerer {

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

bool emitBuiltinCanonicalMapInsertOverwriteOrPending(
    int32_t valuesLocal,
    int32_t ptrLocal,
    int32_t keyLocal,
    int32_t valueLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitPending,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  const auto loopLocals = emitMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, mapKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
  emitInstruction(IrOpcode::CmpEqI32, 0);
  const size_t jumpFound = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);

  size_t jumpAfterEmptyGrow = 0;
  bool hasEmptyGrowJump = false;
  size_t jumpAfterSingleGrow = 0;
  bool hasSingleGrowJump = false;
  size_t jumpAfterPairGrow = 0;
  bool hasPairGrowJump = false;
  size_t jumpAfterTripleGrow = 0;
  bool hasTripleGrowJump = false;
  if (valuesLocal >= 0) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpNotEmpty = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t newBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(newBaseLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(newBaseLocal + 1));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(newBaseLocal + 2));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(newBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterEmptyGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasEmptyGrowJump = true;

    patchInstructionImm(jumpNotEmpty, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpNotSingle = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t grownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(grownBaseLocal));

    auto emitCopyExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(grownBaseLocal + destSlotIndex));
    };
    emitCopyExistingSlot(1, 1);
    emitCopyExistingSlot(2, 2);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(grownBaseLocal + 3));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(grownBaseLocal + 4));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(grownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterSingleGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasSingleGrowJump = true;

    patchInstructionImm(jumpNotSingle, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 2);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpNotPair = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t pairGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 3);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(pairGrownBaseLocal));

    auto emitCopyPairExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(pairGrownBaseLocal + destSlotIndex));
    };
    emitCopyPairExistingSlot(1, 1);
    emitCopyPairExistingSlot(2, 2);
    emitCopyPairExistingSlot(3, 3);
    emitCopyPairExistingSlot(4, 4);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(pairGrownBaseLocal + 5));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(pairGrownBaseLocal + 6));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(pairGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterPairGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasPairGrowJump = true;

    patchInstructionImm(jumpNotPair, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 3);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPending = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t tripleGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 4);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tripleGrownBaseLocal));

    auto emitCopyTripleExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tripleGrownBaseLocal + destSlotIndex));
    };
    emitCopyTripleExistingSlot(1, 1);
    emitCopyTripleExistingSlot(2, 2);
    emitCopyTripleExistingSlot(3, 3);
    emitCopyTripleExistingSlot(4, 4);
    emitCopyTripleExistingSlot(5, 5);
    emitCopyTripleExistingSlot(6, 6);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tripleGrownBaseLocal + 7));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tripleGrownBaseLocal + 8));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(tripleGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterTripleGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasTripleGrowJump = true;

    patchInstructionImm(jumpPending, static_cast<uint64_t>(instructionCount()));
  }
  emitPending();

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
  if (hasEmptyGrowJump) {
    patchInstructionImm(jumpAfterEmptyGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasSingleGrowJump) {
    patchInstructionImm(jumpAfterSingleGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasPairGrowJump) {
    patchInstructionImm(jumpAfterPairGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasTripleGrowJump) {
    patchInstructionImm(jumpAfterTripleGrow, static_cast<uint64_t>(instructionCount()));
  }
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

} // namespace primec::ir_lowerer
