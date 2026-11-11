#include "primec/ir_lowerer/IrLowererFlowHelpers.h"

namespace primec::ir_lowerer {

bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount,
                            const Int32ProviderFn &allocTempLocal) {
  if (slotCount <= 0) {
    return true;
  }
  const auto emitSlots = [&](int32_t destLocal, int32_t srcLocal, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
      const uint64_t offsetBytes = static_cast<uint64_t>(i) * IrSlotBytes;
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destLocal)});
      if (offsetBytes != 0) {
        instructions.push_back({IrOpcode::PushI64, offsetBytes});
        instructions.push_back({IrOpcode::AddI64, 0});
      }
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcLocal)});
      if (offsetBytes != 0) {
        instructions.push_back({IrOpcode::PushI64, offsetBytes});
        instructions.push_back({IrOpcode::AddI64, 0});
      }
      instructions.push_back({IrOpcode::LoadIndirect, 0});
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      instructions.push_back({IrOpcode::Pop, 0});
    }
  };
  if (allocTempLocal == nullptr || slotCount < StructCopyLoopMinSlots) {
    emitSlots(destPtrLocal, srcPtrLocal, slotCount);
    return true;
  }
  // A loop over whole chunks of slots through advancing copies of the two pointers, then the remainder.
  const int32_t chunkCount = slotCount / StructCopyLoopUnrollSlots;
  const int32_t destCursor = allocTempLocal();
  const int32_t srcCursor = allocTempLocal();
  const int32_t endCursor = allocTempLocal();
  const uint64_t chunkBytes = static_cast<uint64_t>(StructCopyLoopUnrollSlots) * IrSlotBytes;
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destCursor)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcCursor)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
  instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(chunkCount) * chunkBytes});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(endCursor)});
  const size_t loopStart = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destCursor)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(endCursor)});
  instructions.push_back({IrOpcode::CmpNeI64, 0});
  const size_t exitJump = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  emitSlots(destCursor, srcCursor, StructCopyLoopUnrollSlots);
  for (const int32_t cursor : {destCursor, srcCursor}) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(cursor)});
    instructions.push_back({IrOpcode::PushI64, chunkBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(cursor)});
  }
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});
  instructions[exitJump].imm = static_cast<uint64_t>(instructions.size());
  emitSlots(destCursor, srcCursor, slotCount - chunkCount * StructCopyLoopUnrollSlots);
  return true;
}

bool emitStructCopySlots(std::vector<IrInstruction> &instructions,
                         int32_t destBaseLocal,
                         int32_t srcPtrLocal,
                         int32_t slotCount,
                         const std::function<int32_t()> &allocTempLocal) {
  const int32_t destPtrLocal = allocTempLocal();
  instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(destBaseLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
  return emitStructCopyFromPtrs(instructions, destPtrLocal, srcPtrLocal, slotCount, allocTempLocal);
}

} // namespace primec::ir_lowerer
