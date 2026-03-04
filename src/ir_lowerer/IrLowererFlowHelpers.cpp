#include "IrLowererFlowHelpers.h"

#include <utility>

namespace primec::ir_lowerer {

OnErrorScope::OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

OnErrorScope::~OnErrorScope() {
  target = std::move(previous);
}

ResultReturnScope::ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

ResultReturnScope::~ResultReturnScope() {
  target = std::move(previous);
}

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex) {
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
  instructions.push_back({IrOpcode::PushI64, 0});
  instructions.push_back({IrOpcode::CmpGeI64, 0});
  const size_t jumpSkip = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
  instructions.push_back({IrOpcode::FileClose, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  const size_t skipIndex = instructions.size();
  instructions[jumpSkip].imm = static_cast<int32_t>(skipIndex);
}

void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope) {
  for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
    emitFileCloseIfValid(instructions, *it);
  }
}

void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack) {
  for (auto it = fileScopeStack.rbegin(); it != fileScopeStack.rend(); ++it) {
    emitFileScopeCleanup(instructions, *it);
  }
}

bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount) {
  if (slotCount <= 0) {
    return true;
  }
  for (int32_t i = 0; i < slotCount; ++i) {
    const uint64_t offsetBytes = static_cast<uint64_t>(i * 16);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
  }
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
  return emitStructCopyFromPtrs(instructions, destPtrLocal, srcPtrLocal, slotCount);
}

} // namespace primec::ir_lowerer
