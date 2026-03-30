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

  size_t jumpAfterGenericGrow = 0;
  bool hasGenericGrowJump = false;
  if (valuesLocal >= 0) {
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

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(genericGrownPtrLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterGenericGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasGenericGrowJump = true;

    const size_t genericFoundIndex = instructionCount();
    patchInstructionImm(jumpFound, static_cast<uint64_t>(genericFoundIndex));

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
  }

  size_t jumpAfterEmptyGrow = 0;
  bool hasEmptyGrowJump = false;
  size_t jumpAfterSingleGrow = 0;
  bool hasSingleGrowJump = false;
  size_t jumpAfterPairGrow = 0;
  bool hasPairGrowJump = false;
  size_t jumpAfterTripleGrow = 0;
  bool hasTripleGrowJump = false;
  size_t jumpAfterQuadGrow = 0;
  bool hasQuadGrowJump = false;
  size_t jumpAfterQuintGrow = 0;
  bool hasQuintGrowJump = false;
  size_t jumpAfterSextGrow = 0;
  bool hasSextGrowJump = false;
  size_t jumpAfterSeptGrow = 0;
  bool hasSeptGrowJump = false;
  size_t jumpAfterOctGrow = 0;
  bool hasOctGrowJump = false;
  size_t jumpAfterNinthGrow = 0;
  bool hasNinthGrowJump = false;
  size_t jumpAfterTenthGrow = 0;
  bool hasTenthGrowJump = false;
  size_t jumpAfterEleventhGrow = 0;
  bool hasEleventhGrowJump = false;
  size_t jumpAfterTwelfthGrow = 0;
  bool hasTwelfthGrowJump = false;
  size_t jumpAfterThirteenthGrow = 0;
  bool hasThirteenthGrowJump = false;
  size_t jumpAfterFourteenthGrow = 0;
  bool hasFourteenthGrowJump = false;
  size_t jumpAfterFifteenthGrow = 0;
  bool hasFifteenthGrowJump = false;
  size_t jumpAfterSixteenthGrow = 0;
  bool hasSixteenthGrowJump = false;
  size_t jumpAfterSeventeenthGrow = 0;
  bool hasSeventeenthGrowJump = false;
  size_t jumpAfterEighteenthGrow = 0;
  bool hasEighteenthGrowJump = false;
  size_t jumpAfterNineteenthGrow = 0;
  bool hasNineteenthGrowJump = false;
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
    const size_t jumpNotTriple = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t tripleGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
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

    patchInstructionImm(jumpNotTriple, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 4);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpNotQuad = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t quadGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 5);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quadGrownBaseLocal));

    auto emitCopyQuadExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quadGrownBaseLocal + destSlotIndex));
    };
    emitCopyQuadExistingSlot(1, 1);
    emitCopyQuadExistingSlot(2, 2);
    emitCopyQuadExistingSlot(3, 3);
    emitCopyQuadExistingSlot(4, 4);
    emitCopyQuadExistingSlot(5, 5);
    emitCopyQuadExistingSlot(6, 6);
    emitCopyQuadExistingSlot(7, 7);
    emitCopyQuadExistingSlot(8, 8);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quadGrownBaseLocal + 9));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quadGrownBaseLocal + 10));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(quadGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterQuadGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasQuadGrowJump = true;

    patchInstructionImm(jumpNotQuad, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 5);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPending = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t quintGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 6);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quintGrownBaseLocal));

    auto emitCopyQuintExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quintGrownBaseLocal + destSlotIndex));
    };
    emitCopyQuintExistingSlot(1, 1);
    emitCopyQuintExistingSlot(2, 2);
    emitCopyQuintExistingSlot(3, 3);
    emitCopyQuintExistingSlot(4, 4);
    emitCopyQuintExistingSlot(5, 5);
    emitCopyQuintExistingSlot(6, 6);
    emitCopyQuintExistingSlot(7, 7);
    emitCopyQuintExistingSlot(8, 8);
    emitCopyQuintExistingSlot(9, 9);
    emitCopyQuintExistingSlot(10, 10);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quintGrownBaseLocal + 11));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(quintGrownBaseLocal + 12));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(quintGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterQuintGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasQuintGrowJump = true;

    patchInstructionImm(jumpPending, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 6);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterSext = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t sextGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 7);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sextGrownBaseLocal));

    auto emitCopySextExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sextGrownBaseLocal + destSlotIndex));
    };
    emitCopySextExistingSlot(1, 1);
    emitCopySextExistingSlot(2, 2);
    emitCopySextExistingSlot(3, 3);
    emitCopySextExistingSlot(4, 4);
    emitCopySextExistingSlot(5, 5);
    emitCopySextExistingSlot(6, 6);
    emitCopySextExistingSlot(7, 7);
    emitCopySextExistingSlot(8, 8);
    emitCopySextExistingSlot(9, 9);
    emitCopySextExistingSlot(10, 10);
    emitCopySextExistingSlot(11, 11);
    emitCopySextExistingSlot(12, 12);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sextGrownBaseLocal + 13));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sextGrownBaseLocal + 14));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(sextGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterSextGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasSextGrowJump = true;

    patchInstructionImm(jumpPendingAfterSext, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 7);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterSept = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t septGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 8);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(septGrownBaseLocal));

    auto emitCopySeptExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(septGrownBaseLocal + destSlotIndex));
    };
    emitCopySeptExistingSlot(1, 1);
    emitCopySeptExistingSlot(2, 2);
    emitCopySeptExistingSlot(3, 3);
    emitCopySeptExistingSlot(4, 4);
    emitCopySeptExistingSlot(5, 5);
    emitCopySeptExistingSlot(6, 6);
    emitCopySeptExistingSlot(7, 7);
    emitCopySeptExistingSlot(8, 8);
    emitCopySeptExistingSlot(9, 9);
    emitCopySeptExistingSlot(10, 10);
    emitCopySeptExistingSlot(11, 11);
    emitCopySeptExistingSlot(12, 12);
    emitCopySeptExistingSlot(13, 13);
    emitCopySeptExistingSlot(14, 14);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(septGrownBaseLocal + 15));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(septGrownBaseLocal + 16));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(septGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterSeptGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasSeptGrowJump = true;

    patchInstructionImm(jumpPendingAfterSept, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 8);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterOct = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t octGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 9);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(octGrownBaseLocal));

    auto emitCopyOctExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(octGrownBaseLocal + destSlotIndex));
    };
    emitCopyOctExistingSlot(1, 1);
    emitCopyOctExistingSlot(2, 2);
    emitCopyOctExistingSlot(3, 3);
    emitCopyOctExistingSlot(4, 4);
    emitCopyOctExistingSlot(5, 5);
    emitCopyOctExistingSlot(6, 6);
    emitCopyOctExistingSlot(7, 7);
    emitCopyOctExistingSlot(8, 8);
    emitCopyOctExistingSlot(9, 9);
    emitCopyOctExistingSlot(10, 10);
    emitCopyOctExistingSlot(11, 11);
    emitCopyOctExistingSlot(12, 12);
    emitCopyOctExistingSlot(13, 13);
    emitCopyOctExistingSlot(14, 14);
    emitCopyOctExistingSlot(15, 15);
    emitCopyOctExistingSlot(16, 16);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(octGrownBaseLocal + 17));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(octGrownBaseLocal + 18));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(octGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterOctGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasOctGrowJump = true;

    patchInstructionImm(jumpPendingAfterOct, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 9);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterNinth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t ninthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 10);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ninthGrownBaseLocal));

    auto emitCopyNinthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ninthGrownBaseLocal + destSlotIndex));
    };
    emitCopyNinthExistingSlot(1, 1);
    emitCopyNinthExistingSlot(2, 2);
    emitCopyNinthExistingSlot(3, 3);
    emitCopyNinthExistingSlot(4, 4);
    emitCopyNinthExistingSlot(5, 5);
    emitCopyNinthExistingSlot(6, 6);
    emitCopyNinthExistingSlot(7, 7);
    emitCopyNinthExistingSlot(8, 8);
    emitCopyNinthExistingSlot(9, 9);
    emitCopyNinthExistingSlot(10, 10);
    emitCopyNinthExistingSlot(11, 11);
    emitCopyNinthExistingSlot(12, 12);
    emitCopyNinthExistingSlot(13, 13);
    emitCopyNinthExistingSlot(14, 14);
    emitCopyNinthExistingSlot(15, 15);
    emitCopyNinthExistingSlot(16, 16);
    emitCopyNinthExistingSlot(17, 17);
    emitCopyNinthExistingSlot(18, 18);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ninthGrownBaseLocal + 19));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ninthGrownBaseLocal + 20));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(ninthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterNinthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasNinthGrowJump = true;

    patchInstructionImm(jumpPendingAfterNinth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 10);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterTenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t tenthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 11);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tenthGrownBaseLocal));

    auto emitCopyTenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tenthGrownBaseLocal + destSlotIndex));
    };
    emitCopyTenthExistingSlot(1, 1);
    emitCopyTenthExistingSlot(2, 2);
    emitCopyTenthExistingSlot(3, 3);
    emitCopyTenthExistingSlot(4, 4);
    emitCopyTenthExistingSlot(5, 5);
    emitCopyTenthExistingSlot(6, 6);
    emitCopyTenthExistingSlot(7, 7);
    emitCopyTenthExistingSlot(8, 8);
    emitCopyTenthExistingSlot(9, 9);
    emitCopyTenthExistingSlot(10, 10);
    emitCopyTenthExistingSlot(11, 11);
    emitCopyTenthExistingSlot(12, 12);
    emitCopyTenthExistingSlot(13, 13);
    emitCopyTenthExistingSlot(14, 14);
    emitCopyTenthExistingSlot(15, 15);
    emitCopyTenthExistingSlot(16, 16);
    emitCopyTenthExistingSlot(17, 17);
    emitCopyTenthExistingSlot(18, 18);
    emitCopyTenthExistingSlot(19, 19);
    emitCopyTenthExistingSlot(20, 20);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tenthGrownBaseLocal + 21));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(tenthGrownBaseLocal + 22));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(tenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterTenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasTenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterTenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 11);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterEleventh = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t eleventhGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 12);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eleventhGrownBaseLocal));

    auto emitCopyEleventhExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal,
                      static_cast<uint64_t>(eleventhGrownBaseLocal + destSlotIndex));
    };
    emitCopyEleventhExistingSlot(1, 1);
    emitCopyEleventhExistingSlot(2, 2);
    emitCopyEleventhExistingSlot(3, 3);
    emitCopyEleventhExistingSlot(4, 4);
    emitCopyEleventhExistingSlot(5, 5);
    emitCopyEleventhExistingSlot(6, 6);
    emitCopyEleventhExistingSlot(7, 7);
    emitCopyEleventhExistingSlot(8, 8);
    emitCopyEleventhExistingSlot(9, 9);
    emitCopyEleventhExistingSlot(10, 10);
    emitCopyEleventhExistingSlot(11, 11);
    emitCopyEleventhExistingSlot(12, 12);
    emitCopyEleventhExistingSlot(13, 13);
    emitCopyEleventhExistingSlot(14, 14);
    emitCopyEleventhExistingSlot(15, 15);
    emitCopyEleventhExistingSlot(16, 16);
    emitCopyEleventhExistingSlot(17, 17);
    emitCopyEleventhExistingSlot(18, 18);
    emitCopyEleventhExistingSlot(19, 19);
    emitCopyEleventhExistingSlot(20, 20);
    emitCopyEleventhExistingSlot(21, 21);
    emitCopyEleventhExistingSlot(22, 22);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eleventhGrownBaseLocal + 23));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eleventhGrownBaseLocal + 24));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(eleventhGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterEleventhGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasEleventhGrowJump = true;

    patchInstructionImm(jumpPendingAfterEleventh, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 12);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterTwelfth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t twelfthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 13);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(twelfthGrownBaseLocal));

    auto emitCopyTwelfthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(twelfthGrownBaseLocal + destSlotIndex));
    };
    emitCopyTwelfthExistingSlot(1, 1);
    emitCopyTwelfthExistingSlot(2, 2);
    emitCopyTwelfthExistingSlot(3, 3);
    emitCopyTwelfthExistingSlot(4, 4);
    emitCopyTwelfthExistingSlot(5, 5);
    emitCopyTwelfthExistingSlot(6, 6);
    emitCopyTwelfthExistingSlot(7, 7);
    emitCopyTwelfthExistingSlot(8, 8);
    emitCopyTwelfthExistingSlot(9, 9);
    emitCopyTwelfthExistingSlot(10, 10);
    emitCopyTwelfthExistingSlot(11, 11);
    emitCopyTwelfthExistingSlot(12, 12);
    emitCopyTwelfthExistingSlot(13, 13);
    emitCopyTwelfthExistingSlot(14, 14);
    emitCopyTwelfthExistingSlot(15, 15);
    emitCopyTwelfthExistingSlot(16, 16);
    emitCopyTwelfthExistingSlot(17, 17);
    emitCopyTwelfthExistingSlot(18, 18);
    emitCopyTwelfthExistingSlot(19, 19);
    emitCopyTwelfthExistingSlot(20, 20);
    emitCopyTwelfthExistingSlot(21, 21);
    emitCopyTwelfthExistingSlot(22, 22);
    emitCopyTwelfthExistingSlot(23, 23);
    emitCopyTwelfthExistingSlot(24, 24);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(twelfthGrownBaseLocal + 25));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(twelfthGrownBaseLocal + 26));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(twelfthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterTwelfthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasTwelfthGrowJump = true;

    patchInstructionImm(jumpPendingAfterTwelfth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 13);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterThirteenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t thirteenthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 14);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(thirteenthGrownBaseLocal));

    auto emitCopyThirteenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(thirteenthGrownBaseLocal + destSlotIndex));
    };
    emitCopyThirteenthExistingSlot(1, 1);
    emitCopyThirteenthExistingSlot(2, 2);
    emitCopyThirteenthExistingSlot(3, 3);
    emitCopyThirteenthExistingSlot(4, 4);
    emitCopyThirteenthExistingSlot(5, 5);
    emitCopyThirteenthExistingSlot(6, 6);
    emitCopyThirteenthExistingSlot(7, 7);
    emitCopyThirteenthExistingSlot(8, 8);
    emitCopyThirteenthExistingSlot(9, 9);
    emitCopyThirteenthExistingSlot(10, 10);
    emitCopyThirteenthExistingSlot(11, 11);
    emitCopyThirteenthExistingSlot(12, 12);
    emitCopyThirteenthExistingSlot(13, 13);
    emitCopyThirteenthExistingSlot(14, 14);
    emitCopyThirteenthExistingSlot(15, 15);
    emitCopyThirteenthExistingSlot(16, 16);
    emitCopyThirteenthExistingSlot(17, 17);
    emitCopyThirteenthExistingSlot(18, 18);
    emitCopyThirteenthExistingSlot(19, 19);
    emitCopyThirteenthExistingSlot(20, 20);
    emitCopyThirteenthExistingSlot(21, 21);
    emitCopyThirteenthExistingSlot(22, 22);
    emitCopyThirteenthExistingSlot(23, 23);
    emitCopyThirteenthExistingSlot(24, 24);
    emitCopyThirteenthExistingSlot(25, 25);
    emitCopyThirteenthExistingSlot(26, 26);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(thirteenthGrownBaseLocal + 27));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(thirteenthGrownBaseLocal + 28));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(thirteenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterThirteenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasThirteenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterThirteenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 14);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterFourteenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t fourteenthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 15);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fourteenthGrownBaseLocal));

    auto emitCopyFourteenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fourteenthGrownBaseLocal + destSlotIndex));
    };
    emitCopyFourteenthExistingSlot(1, 1);
    emitCopyFourteenthExistingSlot(2, 2);
    emitCopyFourteenthExistingSlot(3, 3);
    emitCopyFourteenthExistingSlot(4, 4);
    emitCopyFourteenthExistingSlot(5, 5);
    emitCopyFourteenthExistingSlot(6, 6);
    emitCopyFourteenthExistingSlot(7, 7);
    emitCopyFourteenthExistingSlot(8, 8);
    emitCopyFourteenthExistingSlot(9, 9);
    emitCopyFourteenthExistingSlot(10, 10);
    emitCopyFourteenthExistingSlot(11, 11);
    emitCopyFourteenthExistingSlot(12, 12);
    emitCopyFourteenthExistingSlot(13, 13);
    emitCopyFourteenthExistingSlot(14, 14);
    emitCopyFourteenthExistingSlot(15, 15);
    emitCopyFourteenthExistingSlot(16, 16);
    emitCopyFourteenthExistingSlot(17, 17);
    emitCopyFourteenthExistingSlot(18, 18);
    emitCopyFourteenthExistingSlot(19, 19);
    emitCopyFourteenthExistingSlot(20, 20);
    emitCopyFourteenthExistingSlot(21, 21);
    emitCopyFourteenthExistingSlot(22, 22);
    emitCopyFourteenthExistingSlot(23, 23);
    emitCopyFourteenthExistingSlot(24, 24);
    emitCopyFourteenthExistingSlot(25, 25);
    emitCopyFourteenthExistingSlot(26, 26);
    emitCopyFourteenthExistingSlot(27, 27);
    emitCopyFourteenthExistingSlot(28, 28);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fourteenthGrownBaseLocal + 29));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fourteenthGrownBaseLocal + 30));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(fourteenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterFourteenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasFourteenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterFourteenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 15);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterFifteenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t fifteenthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 16);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fifteenthGrownBaseLocal));

    auto emitCopyFifteenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fifteenthGrownBaseLocal + destSlotIndex));
    };
    emitCopyFifteenthExistingSlot(1, 1);
    emitCopyFifteenthExistingSlot(2, 2);
    emitCopyFifteenthExistingSlot(3, 3);
    emitCopyFifteenthExistingSlot(4, 4);
    emitCopyFifteenthExistingSlot(5, 5);
    emitCopyFifteenthExistingSlot(6, 6);
    emitCopyFifteenthExistingSlot(7, 7);
    emitCopyFifteenthExistingSlot(8, 8);
    emitCopyFifteenthExistingSlot(9, 9);
    emitCopyFifteenthExistingSlot(10, 10);
    emitCopyFifteenthExistingSlot(11, 11);
    emitCopyFifteenthExistingSlot(12, 12);
    emitCopyFifteenthExistingSlot(13, 13);
    emitCopyFifteenthExistingSlot(14, 14);
    emitCopyFifteenthExistingSlot(15, 15);
    emitCopyFifteenthExistingSlot(16, 16);
    emitCopyFifteenthExistingSlot(17, 17);
    emitCopyFifteenthExistingSlot(18, 18);
    emitCopyFifteenthExistingSlot(19, 19);
    emitCopyFifteenthExistingSlot(20, 20);
    emitCopyFifteenthExistingSlot(21, 21);
    emitCopyFifteenthExistingSlot(22, 22);
    emitCopyFifteenthExistingSlot(23, 23);
    emitCopyFifteenthExistingSlot(24, 24);
    emitCopyFifteenthExistingSlot(25, 25);
    emitCopyFifteenthExistingSlot(26, 26);
    emitCopyFifteenthExistingSlot(27, 27);
    emitCopyFifteenthExistingSlot(28, 28);
    emitCopyFifteenthExistingSlot(29, 29);
    emitCopyFifteenthExistingSlot(30, 30);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fifteenthGrownBaseLocal + 31));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(fifteenthGrownBaseLocal + 32));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(fifteenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterFifteenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasFifteenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterFifteenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 16);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterSixteenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t sixteenthGrownBaseLocal = allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    (void)allocTempLocal();
    emitInstruction(IrOpcode::PushI32, 17);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sixteenthGrownBaseLocal));

    auto emitCopySixteenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sixteenthGrownBaseLocal + destSlotIndex));
    };
    emitCopySixteenthExistingSlot(1, 1);
    emitCopySixteenthExistingSlot(2, 2);
    emitCopySixteenthExistingSlot(3, 3);
    emitCopySixteenthExistingSlot(4, 4);
    emitCopySixteenthExistingSlot(5, 5);
    emitCopySixteenthExistingSlot(6, 6);
    emitCopySixteenthExistingSlot(7, 7);
    emitCopySixteenthExistingSlot(8, 8);
    emitCopySixteenthExistingSlot(9, 9);
    emitCopySixteenthExistingSlot(10, 10);
    emitCopySixteenthExistingSlot(11, 11);
    emitCopySixteenthExistingSlot(12, 12);
    emitCopySixteenthExistingSlot(13, 13);
    emitCopySixteenthExistingSlot(14, 14);
    emitCopySixteenthExistingSlot(15, 15);
    emitCopySixteenthExistingSlot(16, 16);
    emitCopySixteenthExistingSlot(17, 17);
    emitCopySixteenthExistingSlot(18, 18);
    emitCopySixteenthExistingSlot(19, 19);
    emitCopySixteenthExistingSlot(20, 20);
    emitCopySixteenthExistingSlot(21, 21);
    emitCopySixteenthExistingSlot(22, 22);
    emitCopySixteenthExistingSlot(23, 23);
    emitCopySixteenthExistingSlot(24, 24);
    emitCopySixteenthExistingSlot(25, 25);
    emitCopySixteenthExistingSlot(26, 26);
    emitCopySixteenthExistingSlot(27, 27);
    emitCopySixteenthExistingSlot(28, 28);
    emitCopySixteenthExistingSlot(29, 29);
    emitCopySixteenthExistingSlot(30, 30);
    emitCopySixteenthExistingSlot(31, 31);
    emitCopySixteenthExistingSlot(32, 32);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sixteenthGrownBaseLocal + 33));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sixteenthGrownBaseLocal + 34));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(sixteenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterSixteenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasSixteenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterSixteenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 17);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterSeventeenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t seventeenthGrownBaseLocal = allocTempLocal();
    for (int i = 0; i < 36; ++i) {
      (void)allocTempLocal();
    }
    emitInstruction(IrOpcode::PushI32, 18);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(seventeenthGrownBaseLocal));

    auto emitCopySeventeenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(seventeenthGrownBaseLocal + destSlotIndex));
    };
    for (int32_t slotIndex = 1; slotIndex <= 34; ++slotIndex) {
      emitCopySeventeenthExistingSlot(slotIndex, slotIndex);
    }
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(seventeenthGrownBaseLocal + 35));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(seventeenthGrownBaseLocal + 36));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(seventeenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterSeventeenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasSeventeenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterSeventeenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 18);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterEighteenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t eighteenthGrownBaseLocal = allocTempLocal();
    for (int i = 0; i < 38; ++i) {
      (void)allocTempLocal();
    }
    emitInstruction(IrOpcode::PushI32, 19);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eighteenthGrownBaseLocal));

    auto emitCopyEighteenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eighteenthGrownBaseLocal + destSlotIndex));
    };
    for (int32_t slotIndex = 1; slotIndex <= 36; ++slotIndex) {
      emitCopyEighteenthExistingSlot(slotIndex, slotIndex);
    }
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eighteenthGrownBaseLocal + 37));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(eighteenthGrownBaseLocal + 38));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(eighteenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterEighteenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasEighteenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterEighteenth, static_cast<uint64_t>(instructionCount()));

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(loopLocals.countLocal));
    emitInstruction(IrOpcode::PushI32, 19);
    emitInstruction(IrOpcode::CmpEqI32, 0);
    const size_t jumpPendingAfterNineteenth = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);

    const int32_t nineteenthGrownBaseLocal = allocTempLocal();
    for (int i = 0; i < 40; ++i) {
      (void)allocTempLocal();
    }
    emitInstruction(IrOpcode::PushI32, 20);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(nineteenthGrownBaseLocal));

    auto emitCopyNineteenthExistingSlot = [&](int32_t sourceSlotIndex, int32_t destSlotIndex) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
      emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(sourceSlotIndex * IrSlotBytesI32));
      emitInstruction(IrOpcode::AddI64, 0);
      emitInstruction(IrOpcode::LoadIndirect, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(nineteenthGrownBaseLocal + destSlotIndex));
    };
    for (int32_t slotIndex = 1; slotIndex <= 38; ++slotIndex) {
      emitCopyNineteenthExistingSlot(slotIndex, slotIndex);
    }
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(nineteenthGrownBaseLocal + 39));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(nineteenthGrownBaseLocal + 40));
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(nineteenthGrownBaseLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(valuesLocal));
    jumpAfterNineteenthGrow = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    hasNineteenthGrowJump = true;

    patchInstructionImm(jumpPendingAfterNineteenth, static_cast<uint64_t>(instructionCount()));
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
  if (hasQuadGrowJump) {
    patchInstructionImm(jumpAfterQuadGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasQuintGrowJump) {
    patchInstructionImm(jumpAfterQuintGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasSextGrowJump) {
    patchInstructionImm(jumpAfterSextGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasSeptGrowJump) {
    patchInstructionImm(jumpAfterSeptGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasOctGrowJump) {
    patchInstructionImm(jumpAfterOctGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasNinthGrowJump) {
    patchInstructionImm(jumpAfterNinthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasTenthGrowJump) {
    patchInstructionImm(jumpAfterTenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasEleventhGrowJump) {
    patchInstructionImm(jumpAfterEleventhGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasTwelfthGrowJump) {
    patchInstructionImm(jumpAfterTwelfthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasThirteenthGrowJump) {
    patchInstructionImm(jumpAfterThirteenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasFourteenthGrowJump) {
    patchInstructionImm(jumpAfterFourteenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasFifteenthGrowJump) {
    patchInstructionImm(jumpAfterFifteenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasSixteenthGrowJump) {
    patchInstructionImm(jumpAfterSixteenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasSeventeenthGrowJump) {
    patchInstructionImm(jumpAfterSeventeenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasEighteenthGrowJump) {
    patchInstructionImm(jumpAfterEighteenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasNineteenthGrowJump) {
    patchInstructionImm(jumpAfterNineteenthGrow, static_cast<uint64_t>(instructionCount()));
  }
  if (hasGenericGrowJump) {
    patchInstructionImm(jumpAfterGenericGrow, static_cast<uint64_t>(instructionCount()));
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
