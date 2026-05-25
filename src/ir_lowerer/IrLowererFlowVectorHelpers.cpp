#include "IrLowererFlowHelpers.h"

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

} // namespace primec::ir_lowerer
