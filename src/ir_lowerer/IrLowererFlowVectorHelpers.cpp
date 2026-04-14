#include "IrLowererFlowHelpers.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererFlowVectorResolutionInternal.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"

namespace primec::ir_lowerer {

bool emitVectorDestroySlot(
    std::vector<IrInstruction> &instructions,
    int32_t dataPtrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    const std::string &structPath,
    const Definition *destroyHelper,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (destroyHelper == nullptr) {
    return true;
  }
  if (indexKind != LocalInfo::ValueKind::Int32 &&
      indexKind != LocalInfo::ValueKind::Int64 &&
      indexKind != LocalInfo::ValueKind::UInt64) {
    error = "internal error: vector removed-slot destruction requires integer index kind";
    return false;
  }

  const int32_t slotPtrLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  if (indexKind == LocalInfo::ValueKind::Int32) {
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
  } else {
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::MulI64, 0});
  }
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(slotPtrLocal)});
  return emitDestroyHelperFromPtr(
      slotPtrLocal, structPath, destroyHelper, localsIn, emitInlineDefinitionCall, error);
}

bool emitVectorMoveSlot(
    std::vector<IrInstruction> &instructions,
    int32_t dataPtrLocal,
    int32_t destIndexLocal,
    int32_t srcIndexLocal,
    LocalInfo::ValueKind indexKind,
    const std::string &structPath,
    const Definition *moveHelper,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (indexKind != LocalInfo::ValueKind::Int32 &&
      indexKind != LocalInfo::ValueKind::Int64 &&
      indexKind != LocalInfo::ValueKind::UInt64) {
    error = "internal error: vector survivor motion requires integer index kind";
    return false;
  }

  const int32_t destPtrLocal = allocTempLocal();
  const int32_t srcPtrLocal = allocTempLocal();

  auto emitStoreSlotPtr = [&](int32_t indexLocal, int32_t ptrLocal) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    if (indexKind == LocalInfo::ValueKind::Int32) {
      instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
      instructions.push_back({IrOpcode::MulI32, 0});
    } else {
      instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
      instructions.push_back({IrOpcode::MulI64, 0});
    }
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  };

  emitStoreSlotPtr(destIndexLocal, destPtrLocal);
  emitStoreSlotPtr(srcIndexLocal, srcPtrLocal);

  if (moveHelper != nullptr) {
    return emitMoveHelperFromPtrs(
        destPtrLocal, srcPtrLocal, structPath, moveHelper, localsIn, emitInlineDefinitionCall, error);
  }
  return emitStructCopyFromPtrs(instructions, destPtrLocal, srcPtrLocal, 1);
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  return tryEmitVectorStatementHelper(stmt,
                                      localsIn,
                                      instructions,
                                      allocTempLocal,
                                      inferExprKind,
                                      inferStructExprPath,
                                      emitExpr,
                                      isUserDefinedVectorHelperCall,
                                      emitVectorCapacityExceeded,
                                      emitVectorPopOnEmpty,
                                      emitVectorIndexOutOfBounds,
                                      emitVectorIndexOutOfBounds,
                                      emitVectorReserveNegative,
                                      emitVectorReserveExceeded,
                                      error);
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  return tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      allocTempLocal,
      inferExprKind,
      inferStructExprPath,
      emitExpr,
      [](const std::string &) -> const Definition * { return nullptr; },
      [](const std::string &) -> const Definition * { return nullptr; },
      [](const Expr &, const Definition &, const LocalMap &, bool) { return false; },
      isUserDefinedVectorHelperCall,
      emitVectorCapacityExceeded,
      emitVectorPopOnEmpty,
      emitVectorIndexOutOfBounds,
      emitArrayIndexOutOfBounds,
      emitVectorReserveNegative,
      emitVectorReserveExceeded,
      error);
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const std::string &)> &resolveDestroyHelperForStruct,
    const std::function<const Definition *(const std::string &)> &resolveMoveHelperForStruct,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  return tryEmitVectorStatementHelper(stmt,
                                      localsIn,
                                      instructions,
                                      allocTempLocal,
                                      inferExprKind,
                                      inferStructExprPath,
                                      emitExpr,
                                      resolveDestroyHelperForStruct,
                                      resolveMoveHelperForStruct,
                                      emitInlineDefinitionCall,
                                      isUserDefinedVectorHelperCall,
                                      emitVectorCapacityExceeded,
                                      emitVectorPopOnEmpty,
                                      emitVectorIndexOutOfBounds,
                                      emitVectorIndexOutOfBounds,
                                      emitVectorReserveNegative,
                                      emitVectorReserveExceeded,
                                      error);
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const std::string &)> &resolveDestroyHelperForStruct,
    const std::function<const Definition *(const std::string &)> &resolveMoveHelperForStruct,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  PreparedVectorStatementHelperCall preparedCall;
  const VectorStatementHelperPrepareResult prepareResult =
      prepareVectorStatementHelperCall(stmt,
                                       localsIn,
                                       inferStructExprPath,
                                       isUserDefinedVectorHelperCall,
                                       preparedCall,
                                       error);
  if (prepareResult == VectorStatementHelperPrepareResult::NotMatched) {
    return VectorStatementHelperEmitResult::NotMatched;
  }
  if (prepareResult == VectorStatementHelperPrepareResult::Error) {
    return VectorStatementHelperEmitResult::Error;
  }
  const std::string &vectorHelper = preparedCall.vectorHelper;
  const Expr &callStmt = preparedCall.callStmt;
  const Expr &target = callStmt.args.front();

  auto pushIndexConst = [&](LocalInfo::ValueKind kind, int32_t value) {
    if (kind == LocalInfo::ValueKind::Int32) {
      instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(value)});
    } else {
      instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
    }
  };

  const int32_t ptrLocal = allocTempLocal();
  constexpr uint64_t kVectorDataPtrOffsetBytes = 2 * IrSlotBytes;
  if (!emitExpr(target, localsIn)) {
    return VectorStatementHelperEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  auto emitLoadVectorDataPtr = [&](int32_t dataPtrLocal) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, kVectorDataPtrOffsetBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(dataPtrLocal)});
  };

  if (vectorHelper == "clear") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  const int32_t countLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadIndirect, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

  int32_t capacityLocal = -1;
  if (vectorHelper == "push" || vectorHelper == "reserve") {
    capacityLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});
  }

  auto emitReallocateVectorData =
      [&](int32_t desiredLocal,
          const std::function<void()> &emitAllocationFailure) {
        const int32_t oldDataPtrLocal = allocTempLocal();
        emitLoadVectorDataPtr(oldDataPtrLocal);

        const int32_t newDataPtrLocal = allocTempLocal();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
        instructions.push_back({IrOpcode::HeapAlloc, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(newDataPtrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(newDataPtrLocal)});
        instructions.push_back({IrOpcode::PushI64, 0});
        instructions.push_back({IrOpcode::CmpEqI64, 0});
        const size_t jumpAllocationSucceeded = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitAllocationFailure();
        const size_t allocationSucceededIndex = instructions.size();
        instructions[jumpAllocationSucceeded].imm =
            static_cast<int32_t>(allocationSucceededIndex);

        const int32_t copyIndexLocal = allocTempLocal();
        const int32_t srcPtrLocal = allocTempLocal();
        const int32_t destPtrLocal = allocTempLocal();
        const int32_t copyValueLocal = allocTempLocal();
        instructions.push_back({IrOpcode::PushI32, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(copyIndexLocal)});

        const size_t copyLoopStart = instructions.size();
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        instructions.push_back({IrOpcode::CmpLtI32, 0});
        const size_t jumpCopyDone = instructions.size();
        instructions.push_back({IrOpcode::JumpIfZero, 0});

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(oldDataPtrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
        instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
        instructions.push_back({IrOpcode::MulI32, 0});
        instructions.push_back({IrOpcode::AddI64, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(newDataPtrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
        instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
        instructions.push_back({IrOpcode::MulI32, 0});
        instructions.push_back({IrOpcode::AddI64, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
        instructions.push_back({IrOpcode::LoadIndirect, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(copyValueLocal)});

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyValueLocal)});
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        instructions.push_back({IrOpcode::Pop, 0});

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal)});
        instructions.push_back({IrOpcode::PushI32, 1});
        instructions.push_back({IrOpcode::AddI32, 0});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(copyIndexLocal)});
        instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(copyLoopStart)});

        const size_t copyDoneIndex = instructions.size();
        instructions[jumpCopyDone].imm = static_cast<int32_t>(copyDoneIndex);

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::PushI64, kVectorDataPtrOffsetBytes});
        instructions.push_back({IrOpcode::AddI64, 0});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(newDataPtrLocal)});
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        instructions.push_back({IrOpcode::Pop, 0});

        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
        instructions.push_back({IrOpcode::AddI64, 0});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        instructions.push_back({IrOpcode::Pop, 0});
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
        instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});
      };

  if (vectorHelper == "reserve") {
    LocalInfo::ValueKind capacityKind =
        normalizeIndexKind(inferExprKind(callStmt.args[1], localsIn));
    if (!isSupportedIndexKind(capacityKind)) {
      error = "reserve requires integer capacity";
      return VectorStatementHelperEmitResult::Error;
    }
    const Expr &desiredExpr = callStmt.args[1];
    int64_t signedDesired = 0;
    uint64_t unsignedDesired = 0;
    const SignedLiteralIntegerEvalResult signedEvalResult =
        tryEvaluateSignedLiteralIntegerExpr(desiredExpr, signedDesired);
    if (signedEvalResult == SignedLiteralIntegerEvalResult::Overflow) {
      error = "vector reserve literal expression overflow";
      return VectorStatementHelperEmitResult::Error;
    }
    if (signedEvalResult == SignedLiteralIntegerEvalResult::Value) {
      if (signedDesired < 0) {
        error = "vector reserve expects non-negative capacity";
        return VectorStatementHelperEmitResult::Error;
      }
      if (signedDesired >
          static_cast<int64_t>(kVectorLocalDynamicCapacityLimit)) {
        error = vectorReserveExceedsLocalCapacityLimitMessage();
        return VectorStatementHelperEmitResult::Error;
      }
    } else {
      const UnsignedLiteralIntegerEvalResult unsignedEvalResult =
          tryEvaluateUnsignedLiteralIntegerExpr(desiredExpr, unsignedDesired);
      if (unsignedEvalResult == UnsignedLiteralIntegerEvalResult::Overflow) {
        error = "vector reserve literal expression overflow";
        return VectorStatementHelperEmitResult::Error;
      }
      if (unsignedEvalResult == UnsignedLiteralIntegerEvalResult::Value &&
          unsignedDesired >
              static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)) {
        error = vectorReserveExceedsLocalCapacityLimitMessage();
        return VectorStatementHelperEmitResult::Error;
      }
    }

    const int32_t desiredLocal = allocTempLocal();
    if (!emitExpr(callStmt.args[1], localsIn)) {
      return VectorStatementHelperEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(desiredLocal)});

    if (capacityKind != LocalInfo::ValueKind::UInt64) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
      instructions.push_back({pushZeroForIndex(capacityKind), 0});
      instructions.push_back({cmpLtForIndex(capacityKind), 0});
      size_t jumpNonNegative = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      emitVectorReserveNegative();
      size_t nonNegativeIndex = instructions.size();
      instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
    }

    const IrOpcode cmpLeOp =
        (capacityKind == LocalInfo::ValueKind::Int32)
            ? IrOpcode::CmpLeI32
            : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLeI64
                                                           : IrOpcode::CmpLeU64);
    const IrOpcode cmpGtOp =
        (capacityKind == LocalInfo::ValueKind::Int32)
            ? IrOpcode::CmpGtI32
            : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpGtI64
                                                           : IrOpcode::CmpGtU64);
    const IrOpcode pushLimitOp =
        (capacityKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32
                                                      : IrOpcode::PushI64;

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({cmpLeOp, 0});
    const size_t jumpNeedsGrowth = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    const size_t jumpReserveEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t growthPathIndex = instructions.size();
    instructions[jumpNeedsGrowth].imm = static_cast<int32_t>(growthPathIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({pushLimitOp, static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)});
    instructions.push_back({cmpGtOp, 0});
    const size_t jumpWithinLimit = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitArrayIndexOutOfBounds();
    const size_t withinLimitIndex = instructions.size();
    instructions[jumpWithinLimit].imm = static_cast<int32_t>(withinLimitIndex);

    emitReallocateVectorData(desiredLocal, emitVectorReserveExceeded);

    const size_t reserveEndIndex = instructions.size();
    instructions[jumpReserveEnd].imm = static_cast<int32_t>(reserveEndIndex);
    return VectorStatementHelperEmitResult::Emitted;
  }

  if (vectorHelper == "push") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpNeedsGrowth = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    const size_t pushBodyIndex = instructions.size();
    const int32_t valueLocal = allocTempLocal();
    if (!emitExpr(callStmt.args[1], localsIn)) {
      return VectorStatementHelperEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});

    const int32_t dataPtrLocal = allocTempLocal();
    emitLoadVectorDataPtr(dataPtrLocal);
    const int32_t destPtrLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(dataPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    const size_t jumpEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t growthPathIndex = instructions.size();
    instructions[jumpNeedsGrowth].imm = static_cast<int32_t>(growthPathIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpOom = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    const int32_t desiredLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(desiredLocal)});

    emitReallocateVectorData(desiredLocal, emitVectorCapacityExceeded);

    instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(pushBodyIndex)});

    const size_t errorIndex = instructions.size();
    instructions[jumpOom].imm = static_cast<int32_t>(errorIndex);
    emitVectorCapacityExceeded();
    const size_t endIndex = instructions.size();
    instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
    return VectorStatementHelperEmitResult::Emitted;
  }

  if (vectorHelper == "pop") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpEqI32, 0});
    size_t jumpNonEmpty = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorPopOnEmpty();
    size_t nonEmptyIndex = instructions.size();
    instructions[jumpNonEmpty].imm = static_cast<int32_t>(nonEmptyIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  LocalInfo::ValueKind indexKind =
      normalizeIndexKind(inferExprKind(callStmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
    error = vectorHelper + " requires integer index";
    return VectorStatementHelperEmitResult::Error;
  }

  IrOpcode cmpLtOp =
      (indexKind == LocalInfo::ValueKind::Int32)
          ? IrOpcode::CmpLtI32
          : (indexKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLtI64
                                                      : IrOpcode::CmpLtU64);
  IrOpcode addOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::AddI32
                                                              : IrOpcode::AddI64;
  IrOpcode subOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::SubI32
                                                              : IrOpcode::SubI64;

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(callStmt.args[1], localsIn)) {
    return VectorStatementHelperEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

  if (indexKind != LocalInfo::ValueKind::UInt64) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    instructions.push_back({pushZeroForIndex(indexKind), 0});
    instructions.push_back({cmpLtForIndex(indexKind), 0});
    size_t jumpNonNegative = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorIndexOutOfBounds();
    size_t nonNegativeIndex = instructions.size();
    instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({cmpGeForIndex(indexKind), 0});
  size_t jumpInRange = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  emitVectorIndexOutOfBounds();
  size_t inRangeIndex = instructions.size();
  instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);

  const int32_t lastIndexLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({subOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(lastIndexLocal)});

  const auto targetInfo = resolveArrayVectorAccessTargetInfo(target, localsIn);
  const std::string removedStructPath = targetInfo.structTypeName;
  const Definition *destroyHelper = removedStructPath.empty()
                                        ? nullptr
                                        : resolveDestroyHelperForStruct(removedStructPath);
  const Definition *moveHelper = removedStructPath.empty() ? nullptr : resolveMoveHelperForStruct(removedStructPath);

  if (vectorHelper == "remove_swap") {
    const int32_t dataPtrLocal = allocTempLocal();
    emitLoadVectorDataPtr(dataPtrLocal);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
    instructions.push_back({cmpLtOp, 0});
    const size_t jumpSkipSwapCopy = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    if (!emitVectorDestroySlot(instructions,
                               dataPtrLocal,
                               indexLocal,
                               indexKind,
                               removedStructPath,
                               destroyHelper,
                               localsIn,
                               allocTempLocal,
                               emitInlineDefinitionCall,
                               error)) {
      return VectorStatementHelperEmitResult::Error;
    }

    if (!emitVectorMoveSlot(instructions,
                            dataPtrLocal,
                            indexLocal,
                            lastIndexLocal,
                            indexKind,
                            removedStructPath,
                            moveHelper,
                            localsIn,
                            allocTempLocal,
                            emitInlineDefinitionCall,
                            error)) {
      return VectorStatementHelperEmitResult::Error;
    }
    const size_t jumpAfterRemoveSwapDestroy = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t destroyOnlyIndex = instructions.size();
    instructions[jumpSkipSwapCopy].imm = static_cast<int32_t>(destroyOnlyIndex);

    if (!emitVectorDestroySlot(instructions,
                               dataPtrLocal,
                               indexLocal,
                               indexKind,
                               removedStructPath,
                               destroyHelper,
                               localsIn,
                               allocTempLocal,
                               emitInlineDefinitionCall,
                               error)) {
      return VectorStatementHelperEmitResult::Error;
    }

    const size_t afterRemoveSwapDestroyIndex = instructions.size();
    instructions[jumpAfterRemoveSwapDestroy].imm = static_cast<int32_t>(afterRemoveSwapDestroyIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  const int32_t dataPtrLocal = allocTempLocal();
  emitLoadVectorDataPtr(dataPtrLocal);

  if (!emitVectorDestroySlot(instructions,
                             dataPtrLocal,
                             indexLocal,
                             indexKind,
                             removedStructPath,
                             destroyHelper,
                             localsIn,
                             allocTempLocal,
                             emitInlineDefinitionCall,
                             error)) {
    return VectorStatementHelperEmitResult::Error;
  }

  const size_t loopStart = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
  instructions.push_back({cmpLtOp, 0});
  size_t jumpLoopEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  const int32_t nextIndexLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({addOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(nextIndexLocal)});

  if (!emitVectorMoveSlot(instructions,
                          dataPtrLocal,
                          indexLocal,
                          nextIndexLocal,
                          indexKind,
                          removedStructPath,
                          moveHelper,
                          localsIn,
                          allocTempLocal,
                          emitInlineDefinitionCall,
                          error)) {
    return VectorStatementHelperEmitResult::Error;
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({addOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

  size_t loopEndIndex = instructions.size();
  instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEndIndex);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::SubI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return VectorStatementHelperEmitResult::Emitted;
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  return tryEmitVectorStatementHelper(stmt,
                                      localsIn,
                                      instructions,
                                      allocTempLocal,
                                      inferExprKind,
                                      emitExpr,
                                      isUserDefinedVectorHelperCall,
                                      emitVectorCapacityExceeded,
                                      emitVectorPopOnEmpty,
                                      emitVectorIndexOutOfBounds,
                                      emitVectorIndexOutOfBounds,
                                      emitVectorReserveNegative,
                                      emitVectorReserveExceeded,
                                      error);
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  return tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      allocTempLocal,
      inferExprKind,
      [](const Expr &, const LocalMap &) { return std::string(); },
      emitExpr,
      [](const std::string &) -> const Definition * { return nullptr; },
      [](const std::string &) -> const Definition * { return nullptr; },
      [](const Expr &, const Definition &, const LocalMap &, bool) { return false; },
      isUserDefinedVectorHelperCall,
      emitVectorCapacityExceeded,
      emitVectorPopOnEmpty,
      emitVectorIndexOutOfBounds,
      emitArrayIndexOutOfBounds,
      emitVectorReserveNegative,
      emitVectorReserveExceeded,
      error);
}

} // namespace primec::ir_lowerer
