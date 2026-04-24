#include "VmExecution.h"

#include "VmControlFlowOpcodeShared.h"
#include "VmExecutionNumeric.h"
#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace primec::vm_detail {

bool executeVmModule(const IrModule &module,
                     uint64_t &result,
                     std::string &error,
                     uint64_t argCount,
                     const std::vector<std::string_view> *args) {
  constexpr uint64_t kSlotBytes = IrSlotBytes;
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  struct Frame {
    const IrFunction *function = nullptr;
    size_t functionIndex = 0;
    std::vector<uint64_t> locals;
    size_t ip = 0;
    bool returnValueToCaller = false;
  };
  auto computeLocalCount = [&](const IrFunction &function) {
    size_t localCount = 0;
    for (const auto &inst : function.instructions) {
      if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
          inst.op == IrOpcode::AddressOfLocal) {
        localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
      }
    }
    return localCount;
  };
  std::vector<size_t> localCounts(module.functions.size(), 0);
  for (size_t i = 0; i < module.functions.size(); ++i) {
    localCounts[i] = computeLocalCount(module.functions[i]);
  }
  std::vector<uint64_t> stack;
  std::vector<uint64_t> heapSlots;
  std::vector<VmDebugSession::HeapAllocation> heapAllocations;
  std::vector<Frame> frames;
  frames.reserve(64);
  Frame entryFrame;
  entryFrame.functionIndex = static_cast<size_t>(module.entryIndex);
  entryFrame.function = &module.functions[entryFrame.functionIndex];
  entryFrame.locals.assign(localCounts[entryFrame.functionIndex], 0);
  frames.push_back(std::move(entryFrame));
  while (!frames.empty()) {
    Frame &frame = frames.back();
    const IrFunction &fn = *frame.function;
    std::vector<uint64_t> &locals = frame.locals;
    size_t &ip = frame.ip;
    if (ip >= fn.instructions.size()) {
      if (frames.size() == 1) {
        error = "missing return in IR";
      } else {
        error = "missing return in IR function " + fn.name;
      }
      return false;
    }
    const auto &inst = fn.instructions[ip];
    const auto controlFlowOutcome = handleSharedVmControlFlowOpcode(inst,
                                                                    stack,
                                                                    fn.instructions.size(),
                                                                    module.functions.size(),
                                                                    frames.size(),
                                                                    4096,
                                                                    frame.returnValueToCaller,
                                                                    ip,
                                                                    error);
    if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Fault) {
      return false;
    }
    if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Continue) {
      continue;
    }
    if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Call) {
      Frame calleeFrame;
      calleeFrame.functionIndex = controlFlowOutcome.targetFunctionIndex;
      calleeFrame.function = &module.functions[controlFlowOutcome.targetFunctionIndex];
      calleeFrame.locals.assign(localCounts[controlFlowOutcome.targetFunctionIndex], 0);
      calleeFrame.returnValueToCaller = controlFlowOutcome.returnValueToCaller;
      frames.push_back(std::move(calleeFrame));
      continue;
    }
    if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Exit) {
      result = controlFlowOutcome.returnValue;
      return true;
    }
    if (controlFlowOutcome.result == VmControlFlowOpcodeResult::Return) {
      const bool returnToCaller = controlFlowOutcome.returnValueToCaller;
      frames.pop_back();
      if (returnToCaller) {
        stack.push_back(controlFlowOutcome.returnValue);
      }
      continue;
    }
    switch (inst.op) {
      case IrOpcode::PushI32:
        stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(inst.imm))));
        ip += 1;
        break;
      case IrOpcode::PushI64:
        stack.push_back(inst.imm);
        ip += 1;
        break;
      case IrOpcode::PushF32:
      case IrOpcode::PushF64:
        stack.push_back(inst.imm);
        ip += 1;
        break;
      case IrOpcode::PushArgc: {
        int32_t count32 = static_cast<int32_t>(argCount);
        stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(count32)));
        ip += 1;
        break;
      }
      case IrOpcode::LoadLocal: {
        if (static_cast<size_t>(inst.imm) >= locals.size()) {
          error = "invalid local index in IR";
          return false;
        }
        stack.push_back(locals[static_cast<size_t>(inst.imm)]);
        ip += 1;
        break;
      }
      case IrOpcode::StoreLocal: {
        if (static_cast<size_t>(inst.imm) >= locals.size()) {
          error = "invalid local index in IR";
          return false;
        }
        if (stack.empty()) {
          error = "IR stack underflow on store";
          return false;
        }
        locals[static_cast<size_t>(inst.imm)] = stack.back();
        stack.pop_back();
        ip += 1;
        break;
      }
      case IrOpcode::AddressOfLocal: {
        if (static_cast<size_t>(inst.imm) >= locals.size()) {
          error = "invalid local index in IR";
          return false;
        }
        stack.push_back(static_cast<uint64_t>(inst.imm) * kSlotBytes);
        ip += 1;
        break;
      }
      case IrOpcode::LoadIndirect: {
        if (stack.empty()) {
          error = "IR stack underflow on load indirect";
          return false;
        }
        uint64_t address = stack.back();
        stack.pop_back();
        uint64_t *slot = nullptr;
        if (!resolveIndirectAddress(address, kSlotBytes, locals, heapSlots, heapAllocations, slot, error)) {
          return false;
        }
        stack.push_back(*slot);
        ip += 1;
        break;
      }
      case IrOpcode::StoreIndirect: {
        if (stack.size() < 2) {
          error = "IR stack underflow on store indirect";
          return false;
        }
        uint64_t value = stack.back();
        stack.pop_back();
        uint64_t address = stack.back();
        stack.pop_back();
        uint64_t *slot = nullptr;
        if (!resolveIndirectAddress(address, kSlotBytes, locals, heapSlots, heapAllocations, slot, error)) {
          return false;
        }
        *slot = value;
        stack.push_back(value);
        ip += 1;
        break;
      }
      case IrOpcode::HeapAlloc: {
        if (stack.empty()) {
          error = "IR stack underflow on heap alloc";
          return false;
        }
        const uint64_t slotCount = stack.back();
        stack.pop_back();
        uint64_t address = 0;
        if (!allocateVmHeapSlots(slotCount, kSlotBytes, heapSlots, heapAllocations, address, error)) {
          return false;
        }
        stack.push_back(address);
        ip += 1;
        break;
      }
      case IrOpcode::HeapFree: {
        if (stack.empty()) {
          error = "IR stack underflow on heap free";
          return false;
        }
        const uint64_t address = stack.back();
        stack.pop_back();
        if (!freeVmHeapSlots(address, kSlotBytes, heapSlots, heapAllocations, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::HeapRealloc: {
        if (stack.size() < 2) {
          error = "IR stack underflow on heap realloc";
          return false;
        }
        const uint64_t slotCount = stack.back();
        stack.pop_back();
        const uint64_t address = stack.back();
        stack.pop_back();
        uint64_t newAddress = 0;
        if (!reallocVmHeapSlots(address,
                                slotCount,
                                kSlotBytes,
                                heapSlots,
                                heapAllocations,
                                newAddress,
                                error)) {
          return false;
        }
        stack.push_back(newAddress);
        ip += 1;
        break;
      }
      case IrOpcode::Dup: {
        if (stack.empty()) {
          error = "IR stack underflow on dup";
          return false;
        }
        stack.push_back(stack.back());
        ip += 1;
        break;
      }
      case IrOpcode::Pop: {
        if (stack.empty()) {
          error = "IR stack underflow on pop";
          return false;
        }
        stack.pop_back();
        ip += 1;
        break;
      }
      case IrOpcode::AddI32:
      case IrOpcode::AddI64:
      case IrOpcode::SubI32:
      case IrOpcode::SubI64:
      case IrOpcode::MulI32:
      case IrOpcode::MulI64:
      case IrOpcode::DivI32:
      case IrOpcode::DivI64:
      case IrOpcode::DivU64:
      case IrOpcode::NegI32:
      case IrOpcode::NegI64:
      case IrOpcode::AddF32:
      case IrOpcode::SubF32:
      case IrOpcode::MulF32:
      case IrOpcode::DivF32:
      case IrOpcode::NegF32:
      case IrOpcode::AddF64:
      case IrOpcode::SubF64:
      case IrOpcode::MulF64:
      case IrOpcode::DivF64:
      case IrOpcode::NegF64:
      case IrOpcode::CmpEqI32:
      case IrOpcode::CmpNeI32:
      case IrOpcode::CmpLtI32:
      case IrOpcode::CmpLeI32:
      case IrOpcode::CmpGtI32:
      case IrOpcode::CmpGeI32:
      case IrOpcode::CmpEqI64:
      case IrOpcode::CmpNeI64:
      case IrOpcode::CmpLtI64:
      case IrOpcode::CmpLeI64:
      case IrOpcode::CmpGtI64:
      case IrOpcode::CmpGeI64:
      case IrOpcode::CmpLtU64:
      case IrOpcode::CmpLeU64:
      case IrOpcode::CmpGtU64:
      case IrOpcode::CmpGeU64:
      case IrOpcode::CmpEqF32:
      case IrOpcode::CmpNeF32:
      case IrOpcode::CmpLtF32:
      case IrOpcode::CmpLeF32:
      case IrOpcode::CmpGtF32:
      case IrOpcode::CmpGeF32:
      case IrOpcode::CmpEqF64:
      case IrOpcode::CmpNeF64:
      case IrOpcode::CmpLtF64:
      case IrOpcode::CmpLeF64:
      case IrOpcode::CmpGtF64:
      case IrOpcode::CmpGeF64:
      case IrOpcode::ConvertI32ToF32:
      case IrOpcode::ConvertI64ToF32:
      case IrOpcode::ConvertU64ToF32:
      case IrOpcode::ConvertI32ToF64:
      case IrOpcode::ConvertI64ToF64:
      case IrOpcode::ConvertU64ToF64:
      case IrOpcode::ConvertF32ToI32:
      case IrOpcode::ConvertF32ToI64:
      case IrOpcode::ConvertF32ToU64:
      case IrOpcode::ConvertF64ToI32:
      case IrOpcode::ConvertF64ToI64:
      case IrOpcode::ConvertF64ToU64:
      case IrOpcode::ConvertF32ToF64:
      case IrOpcode::ConvertF64ToF32: {
        if (!handleVmNumericOpcode(inst, stack, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintI32:
      case IrOpcode::PrintI64:
      case IrOpcode::PrintU64:
      case IrOpcode::PrintString:
      case IrOpcode::PrintStringDynamic:
      case IrOpcode::PrintArgv:
      case IrOpcode::PrintArgvUnsafe: {
        if (!handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend:
      case IrOpcode::FileOpenReadDynamic:
      case IrOpcode::FileOpenWriteDynamic:
      case IrOpcode::FileOpenAppendDynamic:
      case IrOpcode::FileClose:
      case IrOpcode::FileReadByte:
      case IrOpcode::FileFlush:
      case IrOpcode::FileWriteI32:
      case IrOpcode::FileWriteI64:
      case IrOpcode::FileWriteU64:
      case IrOpcode::FileWriteString:
      case IrOpcode::FileWriteStringDynamic:
      case IrOpcode::FileWriteByte:
      case IrOpcode::FileWriteNewline: {
        if (!handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::LoadStringByte: {
        if (stack.empty()) {
          error = "IR stack underflow on string index";
          return false;
        }
        uint64_t indexRaw = stack.back();
        stack.pop_back();
        uint64_t stringIndex = inst.imm;
        if (stringIndex >= module.stringTable.size()) {
          error = "invalid string index in IR";
          return false;
        }
        const std::string &text = module.stringTable[static_cast<size_t>(stringIndex)];
        size_t index = static_cast<size_t>(indexRaw);
        if (index >= text.size()) {
          error = "string index out of bounds in IR";
          return false;
        }
        uint8_t byte = static_cast<uint8_t>(text[index]);
        stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte))));
        ip += 1;
        break;
      }
      case IrOpcode::LoadStringLength: {
        if (stack.empty()) {
          error = "IR stack underflow on string index";
          return false;
        }
        const uint64_t stringIndex = stack.back();
        stack.pop_back();
        if (stringIndex >= module.stringTable.size()) {
          error = "invalid string index in IR";
          return false;
        }
        stack.push_back(static_cast<uint64_t>(module.stringTable[static_cast<size_t>(stringIndex)].size()));
        ip += 1;
        break;
      }
      default:
        error = "unknown IR opcode";
        return false;
    }
  }
  error = "missing return in IR";
  return false;
}

} // namespace primec::vm_detail
