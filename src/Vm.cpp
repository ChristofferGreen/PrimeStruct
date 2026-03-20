#include "primec/Vm.h"

#include "VmHeapHelpers.h"
#include "VmIoHelpers.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace primec {

namespace {
float bitsToF32(uint64_t raw) {
  uint32_t bits = static_cast<uint32_t>(raw);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

uint64_t f32ToBits(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return static_cast<uint64_t>(bits);
}

double bitsToF64(uint64_t raw) {
  double value = 0.0;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

uint64_t f64ToBits(double value) {
  uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

bool executeImpl(const IrModule &module,
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
        if (!vm_detail::resolveIndirectAddress(address, kSlotBytes, locals, heapSlots, heapAllocations, slot, error)) {
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
        if (!vm_detail::resolveIndirectAddress(address, kSlotBytes, locals, heapSlots, heapAllocations, slot, error)) {
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
        if (!vm_detail::allocateVmHeapSlots(slotCount, kSlotBytes, heapSlots, heapAllocations, address, error)) {
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
        if (!vm_detail::freeVmHeapSlots(address, kSlotBytes, heapSlots, heapAllocations, error)) {
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
        if (!vm_detail::reallocVmHeapSlots(address,
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
      case IrOpcode::AddI64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on add";
          return false;
        }
        uint64_t rhs = stack.back();
        stack.pop_back();
        uint64_t lhs = stack.back();
        stack.pop_back();
        stack.push_back(lhs + rhs);
        ip += 1;
        break;
      }
      case IrOpcode::SubI32:
      case IrOpcode::SubI64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on sub";
          return false;
        }
        uint64_t rhs = stack.back();
        stack.pop_back();
        uint64_t lhs = stack.back();
        stack.pop_back();
        stack.push_back(lhs - rhs);
        ip += 1;
        break;
      }
      case IrOpcode::MulI32:
      case IrOpcode::MulI64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on mul";
          return false;
        }
        uint64_t rhs = stack.back();
        stack.pop_back();
        uint64_t lhs = stack.back();
        stack.pop_back();
        stack.push_back(lhs * rhs);
        ip += 1;
        break;
      }
      case IrOpcode::DivI32:
      case IrOpcode::DivI64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on div";
          return false;
        }
        int64_t rhs = static_cast<int64_t>(stack.back());
        stack.pop_back();
        int64_t lhs = static_cast<int64_t>(stack.back());
        stack.pop_back();
        if (rhs == 0) {
          error = "division by zero in IR";
          return false;
        }
        stack.push_back(static_cast<uint64_t>(lhs / rhs));
        ip += 1;
        break;
      }
      case IrOpcode::DivU64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on div";
          return false;
        }
        uint64_t rhs = stack.back();
        stack.pop_back();
        uint64_t lhs = stack.back();
        stack.pop_back();
        if (rhs == 0) {
          error = "division by zero in IR";
          return false;
        }
        stack.push_back(lhs / rhs);
        ip += 1;
        break;
      }
      case IrOpcode::NegI32:
      case IrOpcode::NegI64: {
        if (stack.empty()) {
          error = "IR stack underflow on negate";
          return false;
        }
        int64_t value = static_cast<int64_t>(stack.back());
        stack.pop_back();
        stack.push_back(static_cast<uint64_t>(-value));
        ip += 1;
        break;
      }
      case IrOpcode::AddF32:
      case IrOpcode::SubF32:
      case IrOpcode::MulF32:
      case IrOpcode::DivF32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on float op";
          return false;
        }
        float rhs = bitsToF32(stack.back());
        stack.pop_back();
        float lhs = bitsToF32(stack.back());
        stack.pop_back();
        float result = 0.0f;
        switch (inst.op) {
          case IrOpcode::AddF32:
            result = lhs + rhs;
            break;
          case IrOpcode::SubF32:
            result = lhs - rhs;
            break;
          case IrOpcode::MulF32:
            result = lhs * rhs;
            break;
          case IrOpcode::DivF32:
            result = lhs / rhs;
            break;
          default:
            break;
        }
        stack.push_back(f32ToBits(result));
        ip += 1;
        break;
      }
      case IrOpcode::NegF32: {
        if (stack.empty()) {
          error = "IR stack underflow on negate";
          return false;
        }
        float value = bitsToF32(stack.back());
        stack.pop_back();
        stack.push_back(f32ToBits(-value));
        ip += 1;
        break;
      }
      case IrOpcode::AddF64:
      case IrOpcode::SubF64:
      case IrOpcode::MulF64:
      case IrOpcode::DivF64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on float op";
          return false;
        }
        double rhs = bitsToF64(stack.back());
        stack.pop_back();
        double lhs = bitsToF64(stack.back());
        stack.pop_back();
        double result = 0.0;
        switch (inst.op) {
          case IrOpcode::AddF64:
            result = lhs + rhs;
            break;
          case IrOpcode::SubF64:
            result = lhs - rhs;
            break;
          case IrOpcode::MulF64:
            result = lhs * rhs;
            break;
          case IrOpcode::DivF64:
            result = lhs / rhs;
            break;
          default:
            break;
        }
        stack.push_back(f64ToBits(result));
        ip += 1;
        break;
      }
      case IrOpcode::NegF64: {
        if (stack.empty()) {
          error = "IR stack underflow on negate";
          return false;
        }
        double value = bitsToF64(stack.back());
        stack.pop_back();
        stack.push_back(f64ToBits(-value));
        ip += 1;
        break;
      }
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
      case IrOpcode::CmpGeU64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on compare";
          return false;
        }
        uint64_t rhsRaw = stack.back();
        stack.pop_back();
        uint64_t lhsRaw = stack.back();
        stack.pop_back();
        bool cmp = false;
        switch (inst.op) {
          case IrOpcode::CmpEqI32:
          case IrOpcode::CmpEqI64:
            cmp = (lhsRaw == rhsRaw);
            break;
          case IrOpcode::CmpNeI32:
          case IrOpcode::CmpNeI64:
            cmp = (lhsRaw != rhsRaw);
            break;
          case IrOpcode::CmpLtI32:
          case IrOpcode::CmpLtI64:
            cmp = (static_cast<int64_t>(lhsRaw) < static_cast<int64_t>(rhsRaw));
            break;
          case IrOpcode::CmpLeI32:
          case IrOpcode::CmpLeI64:
            cmp = (static_cast<int64_t>(lhsRaw) <= static_cast<int64_t>(rhsRaw));
            break;
          case IrOpcode::CmpGtI32:
          case IrOpcode::CmpGtI64:
            cmp = (static_cast<int64_t>(lhsRaw) > static_cast<int64_t>(rhsRaw));
            break;
          case IrOpcode::CmpGeI32:
          case IrOpcode::CmpGeI64:
            cmp = (static_cast<int64_t>(lhsRaw) >= static_cast<int64_t>(rhsRaw));
            break;
          case IrOpcode::CmpLtU64:
            cmp = (lhsRaw < rhsRaw);
            break;
          case IrOpcode::CmpLeU64:
            cmp = (lhsRaw <= rhsRaw);
            break;
          case IrOpcode::CmpGtU64:
            cmp = (lhsRaw > rhsRaw);
            break;
          case IrOpcode::CmpGeU64:
            cmp = (lhsRaw >= rhsRaw);
            break;
          default:
            break;
        }
        stack.push_back(cmp ? 1 : 0);
        ip += 1;
        break;
      }
      case IrOpcode::CmpEqF32:
      case IrOpcode::CmpNeF32:
      case IrOpcode::CmpLtF32:
      case IrOpcode::CmpLeF32:
      case IrOpcode::CmpGtF32:
      case IrOpcode::CmpGeF32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on compare";
          return false;
        }
        float rhs = bitsToF32(stack.back());
        stack.pop_back();
        float lhs = bitsToF32(stack.back());
        stack.pop_back();
        bool cmp = false;
        switch (inst.op) {
          case IrOpcode::CmpEqF32:
            cmp = (lhs == rhs);
            break;
          case IrOpcode::CmpNeF32:
            cmp = (lhs != rhs);
            break;
          case IrOpcode::CmpLtF32:
            cmp = (lhs < rhs);
            break;
          case IrOpcode::CmpLeF32:
            cmp = (lhs <= rhs);
            break;
          case IrOpcode::CmpGtF32:
            cmp = (lhs > rhs);
            break;
          case IrOpcode::CmpGeF32:
            cmp = (lhs >= rhs);
            break;
          default:
            break;
        }
        stack.push_back(cmp ? 1 : 0);
        ip += 1;
        break;
      }
      case IrOpcode::CmpEqF64:
      case IrOpcode::CmpNeF64:
      case IrOpcode::CmpLtF64:
      case IrOpcode::CmpLeF64:
      case IrOpcode::CmpGtF64:
      case IrOpcode::CmpGeF64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on compare";
          return false;
        }
        double rhs = bitsToF64(stack.back());
        stack.pop_back();
        double lhs = bitsToF64(stack.back());
        stack.pop_back();
        bool cmp = false;
        switch (inst.op) {
          case IrOpcode::CmpEqF64:
            cmp = (lhs == rhs);
            break;
          case IrOpcode::CmpNeF64:
            cmp = (lhs != rhs);
            break;
          case IrOpcode::CmpLtF64:
            cmp = (lhs < rhs);
            break;
          case IrOpcode::CmpLeF64:
            cmp = (lhs <= rhs);
            break;
          case IrOpcode::CmpGtF64:
            cmp = (lhs > rhs);
            break;
          case IrOpcode::CmpGeF64:
            cmp = (lhs >= rhs);
            break;
          default:
            break;
        }
        stack.push_back(cmp ? 1 : 0);
        ip += 1;
        break;
      }
      case IrOpcode::ConvertI32ToF32:
      case IrOpcode::ConvertI64ToF32:
      case IrOpcode::ConvertU64ToF32: {
        if (stack.empty()) {
          error = "IR stack underflow on convert";
          return false;
        }
        uint64_t raw = stack.back();
        stack.pop_back();
        float value = 0.0f;
        if (inst.op == IrOpcode::ConvertU64ToF32) {
          value = static_cast<float>(raw);
        } else if (inst.op == IrOpcode::ConvertI64ToF32) {
          value = static_cast<float>(static_cast<int64_t>(raw));
        } else {
          value = static_cast<float>(static_cast<int32_t>(raw));
        }
        stack.push_back(f32ToBits(value));
        ip += 1;
        break;
      }
      case IrOpcode::ConvertI32ToF64:
      case IrOpcode::ConvertI64ToF64:
      case IrOpcode::ConvertU64ToF64: {
        if (stack.empty()) {
          error = "IR stack underflow on convert";
          return false;
        }
        uint64_t raw = stack.back();
        stack.pop_back();
        double value = 0.0;
        if (inst.op == IrOpcode::ConvertU64ToF64) {
          value = static_cast<double>(raw);
        } else if (inst.op == IrOpcode::ConvertI64ToF64) {
          value = static_cast<double>(static_cast<int64_t>(raw));
        } else {
          value = static_cast<double>(static_cast<int32_t>(raw));
        }
        stack.push_back(f64ToBits(value));
        ip += 1;
        break;
      }
      case IrOpcode::ConvertF32ToI32:
      case IrOpcode::ConvertF32ToI64:
      case IrOpcode::ConvertF32ToU64: {
        if (stack.empty()) {
          error = "IR stack underflow on convert";
          return false;
        }
        float value = bitsToF32(stack.back());
        stack.pop_back();
        if (inst.op == IrOpcode::ConvertF32ToU64) {
          uint64_t out = static_cast<uint64_t>(value);
          stack.push_back(out);
        } else if (inst.op == IrOpcode::ConvertF32ToI64) {
          int64_t out = static_cast<int64_t>(value);
          stack.push_back(static_cast<uint64_t>(out));
        } else {
          int32_t out = static_cast<int32_t>(value);
          stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(out)));
        }
        ip += 1;
        break;
      }
      case IrOpcode::ConvertF64ToI32:
      case IrOpcode::ConvertF64ToI64:
      case IrOpcode::ConvertF64ToU64: {
        if (stack.empty()) {
          error = "IR stack underflow on convert";
          return false;
        }
        double value = bitsToF64(stack.back());
        stack.pop_back();
        if (inst.op == IrOpcode::ConvertF64ToU64) {
          uint64_t out = static_cast<uint64_t>(value);
          stack.push_back(out);
        } else if (inst.op == IrOpcode::ConvertF64ToI64) {
          int64_t out = static_cast<int64_t>(value);
          stack.push_back(static_cast<uint64_t>(out));
        } else {
          int32_t out = static_cast<int32_t>(value);
          stack.push_back(static_cast<uint64_t>(static_cast<int64_t>(out)));
        }
        ip += 1;
        break;
      }
      case IrOpcode::ConvertF32ToF64: {
        if (stack.empty()) {
          error = "IR stack underflow on convert";
          return false;
        }
        float value = bitsToF32(stack.back());
        stack.pop_back();
        double out = static_cast<double>(value);
        stack.push_back(f64ToBits(out));
        ip += 1;
        break;
      }
      case IrOpcode::ConvertF64ToF32: {
        if (stack.empty()) {
          error = "IR stack underflow on convert";
          return false;
        }
        double value = bitsToF64(stack.back());
        stack.pop_back();
        float out = static_cast<float>(value);
        stack.push_back(f32ToBits(out));
        ip += 1;
        break;
      }
      case IrOpcode::JumpIfZero: {
        if (stack.empty()) {
          error = "IR stack underflow on jump";
          return false;
        }
        uint64_t value = stack.back();
        stack.pop_back();
        if (inst.imm > fn.instructions.size()) {
          error = "invalid jump target in IR";
          return false;
        }
        if (value == 0) {
          ip = static_cast<size_t>(inst.imm);
        } else {
          ip += 1;
        }
        break;
      }
      case IrOpcode::Jump: {
        if (inst.imm > fn.instructions.size()) {
          error = "invalid jump target in IR";
          return false;
        }
        ip = static_cast<size_t>(inst.imm);
        break;
      }
      case IrOpcode::Call:
      case IrOpcode::CallVoid: {
        constexpr size_t MaxCallDepth = 4096;
        if (inst.imm >= module.functions.size()) {
          error = "invalid call target in IR";
          return false;
        }
        if (frames.size() >= MaxCallDepth) {
          error = "VM call stack overflow";
          return false;
        }
        const size_t target = static_cast<size_t>(inst.imm);
        Frame calleeFrame;
        calleeFrame.functionIndex = target;
        calleeFrame.function = &module.functions[target];
        calleeFrame.locals.assign(localCounts[target], 0);
        calleeFrame.returnValueToCaller = (inst.op == IrOpcode::Call);
        ip += 1;
        frames.push_back(std::move(calleeFrame));
        break;
      }
      case IrOpcode::ReturnVoid: {
        uint64_t returnValue = 0;
        const bool returnToCaller = frame.returnValueToCaller;
        if (frames.size() == 1) {
          result = returnValue;
          return true;
        }
        frames.pop_back();
        if (returnToCaller) {
          stack.push_back(returnValue);
        }
        break;
      }
      case IrOpcode::ReturnI32: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        uint64_t returnValue = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(stack.back())));
        stack.pop_back();
        const bool returnToCaller = frame.returnValueToCaller;
        if (frames.size() == 1) {
          result = returnValue;
          return true;
        }
        frames.pop_back();
        if (returnToCaller) {
          stack.push_back(returnValue);
        }
        break;
      }
      case IrOpcode::ReturnI64: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        uint64_t returnValue = stack.back();
        stack.pop_back();
        const bool returnToCaller = frame.returnValueToCaller;
        if (frames.size() == 1) {
          result = returnValue;
          return true;
        }
        frames.pop_back();
        if (returnToCaller) {
          stack.push_back(returnValue);
        }
        break;
      }
      case IrOpcode::ReturnF32: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        uint64_t returnValue = static_cast<uint64_t>(static_cast<uint32_t>(stack.back()));
        stack.pop_back();
        const bool returnToCaller = frame.returnValueToCaller;
        if (frames.size() == 1) {
          result = returnValue;
          return true;
        }
        frames.pop_back();
        if (returnToCaller) {
          stack.push_back(returnValue);
        }
        break;
      }
      case IrOpcode::ReturnF64: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        uint64_t returnValue = stack.back();
        stack.pop_back();
        const bool returnToCaller = frame.returnValueToCaller;
        if (frames.size() == 1) {
          result = returnValue;
          return true;
        }
        frames.pop_back();
        if (returnToCaller) {
          stack.push_back(returnValue);
        }
        break;
      }
      case IrOpcode::PrintI32: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintI64: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintU64: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintString: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintStringDynamic: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintArgv: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintArgvUnsafe: {
        if (!vm_detail::handlePrintOpcode(module, inst, stack, args, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileOpenReadDynamic:
      case IrOpcode::FileOpenWriteDynamic:
      case IrOpcode::FileOpenAppendDynamic: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileClose: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileReadByte: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileFlush: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteI32: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteI64: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteU64: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteString: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteStringDynamic: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteByte: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
          return false;
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteNewline: {
        if (!vm_detail::handleFileOpcode(module, inst, stack, locals, error)) {
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
        stack.push_back(
            static_cast<uint64_t>(module.stringTable[static_cast<size_t>(stringIndex)].size()));
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
} // namespace

bool Vm::execute(const IrModule &module, uint64_t &result, std::string &error, uint64_t argCount) const {
  return executeImpl(module, result, error, argCount, nullptr);
}

bool Vm::execute(const IrModule &module,
                 uint64_t &result,
                 std::string &error,
                 const std::vector<std::string_view> &args) const {
  return executeImpl(module, result, error, static_cast<uint64_t>(args.size()), &args);
}

VmDebugSession::StepOutcome VmDebugSession::stepInstruction(std::string &error) {
  constexpr uint64_t kSlotBytes = IrSlotBytes;
  constexpr size_t MaxCallDepth = 4096;
  if (!module_) {
    error = "debug session has no active module";
    return StepOutcome::Fault;
  }
  if (frames_.empty()) {
    error = "debug session has no active frame";
    return StepOutcome::Fault;
  }
  Frame &frame = frames_.back();
  const IrFunction &fn = *frame.function;
  std::vector<uint64_t> &locals = frame.locals;
  size_t &ip = frame.ip;
  if (ip >= fn.instructions.size()) {
    if (frames_.size() == 1) {
      error = "missing return in IR";
    } else {
      error = "missing return in IR function " + fn.name;
    }
    appendMappedStackTrace(error);
    return StepOutcome::Fault;
  }
  const IrInstruction &inst = fn.instructions[ip];
  auto emitInstructionHook = [&](VmDebugInstructionHook hook) {
    if (!hook) {
      return;
    }
    VmDebugInstructionHookEvent event;
    event.sequence = nextHookSequence_++;
    event.snapshot = snapshot();
    event.opcode = inst.op;
    event.immediate = inst.imm;
    hook(event, hooks_.userData);
  };
  auto emitCallHook = [&](VmDebugCallHook hook, size_t functionIndex, bool returnsValueToCaller) {
    if (!hook) {
      return;
    }
    VmDebugCallHookEvent event;
    event.sequence = nextHookSequence_++;
    event.snapshot = snapshot();
    event.functionIndex = functionIndex;
    event.returnsValueToCaller = returnsValueToCaller;
    hook(event, hooks_.userData);
  };
  auto finishStep = [&](StepOutcome outcome) {
    emitInstructionHook(hooks_.afterInstruction);
    return outcome;
  };
  auto finishFault = [&]() {
    appendMappedStackTrace(error);
    if (hooks_.fault) {
      VmDebugFaultHookEvent event;
      event.sequence = nextHookSequence_++;
      event.snapshot = snapshot();
      event.opcode = inst.op;
      event.immediate = inst.imm;
      event.message = error;
      hooks_.fault(event, hooks_.userData);
    }
    return StepOutcome::Fault;
  };
  emitInstructionHook(hooks_.beforeInstruction);
  switch (inst.op) {
    case IrOpcode::PushI32:
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(inst.imm))));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    case IrOpcode::PushI64:
      stack_.push_back(inst.imm);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
      stack_.push_back(inst.imm);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    case IrOpcode::PushArgc: {
      const int32_t count32 = static_cast<int32_t>(argCount_);
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(count32)));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return finishFault();
      }
      stack_.push_back(locals[static_cast<size_t>(inst.imm)]);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::StoreLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return finishFault();
      }
      if (stack_.empty()) {
        error = "IR stack underflow on store";
        return finishFault();
      }
      locals[static_cast<size_t>(inst.imm)] = stack_.back();
      stack_.pop_back();
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::AddressOfLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return finishFault();
      }
      stack_.push_back(static_cast<uint64_t>(inst.imm) * kSlotBytes);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadIndirect: {
      if (stack_.empty()) {
        error = "IR stack underflow on load indirect";
        return finishFault();
      }
      const uint64_t address = stack_.back();
      stack_.pop_back();
      uint64_t *slot = nullptr;
      if (!vm_detail::resolveIndirectAddress(address, kSlotBytes, locals, heapSlots_, heapAllocations_, slot, error)) {
        return finishFault();
      }
      stack_.push_back(*slot);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::StoreIndirect: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on store indirect";
        return finishFault();
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      const uint64_t address = stack_.back();
      stack_.pop_back();
      uint64_t *slot = nullptr;
      if (!vm_detail::resolveIndirectAddress(address, kSlotBytes, locals, heapSlots_, heapAllocations_, slot, error)) {
        return finishFault();
      }
      *slot = value;
      stack_.push_back(value);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::HeapAlloc: {
      if (stack_.empty()) {
        error = "IR stack underflow on heap alloc";
        return finishFault();
      }
      const uint64_t slotCount = stack_.back();
      stack_.pop_back();
      uint64_t address = 0;
      if (!vm_detail::allocateVmHeapSlots(slotCount, kSlotBytes, heapSlots_, heapAllocations_, address, error)) {
        return finishFault();
      }
      stack_.push_back(address);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::HeapFree: {
      if (stack_.empty()) {
        error = "IR stack underflow on heap free";
        return finishFault();
      }
      const uint64_t address = stack_.back();
      stack_.pop_back();
      if (!vm_detail::freeVmHeapSlots(address, kSlotBytes, heapSlots_, heapAllocations_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::HeapRealloc: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on heap realloc";
        return finishFault();
      }
      const uint64_t slotCount = stack_.back();
      stack_.pop_back();
      const uint64_t address = stack_.back();
      stack_.pop_back();
      uint64_t newAddress = 0;
      if (!vm_detail::reallocVmHeapSlots(address,
                                         slotCount,
                                         kSlotBytes,
                                         heapSlots_,
                                         heapAllocations_,
                                         newAddress,
                                         error)) {
        return finishFault();
      }
      stack_.push_back(newAddress);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::Dup: {
      if (stack_.empty()) {
        error = "IR stack underflow on dup";
        return finishFault();
      }
      stack_.push_back(stack_.back());
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::Pop: {
      if (stack_.empty()) {
        error = "IR stack underflow on pop";
        return finishFault();
      }
      stack_.pop_back();
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::AddI32:
    case IrOpcode::AddI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on add";
        return finishFault();
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      stack_.push_back(lhs + rhs);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::SubI32:
    case IrOpcode::SubI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on sub";
        return finishFault();
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      stack_.push_back(lhs - rhs);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::MulI32:
    case IrOpcode::MulI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on mul";
        return finishFault();
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      stack_.push_back(lhs * rhs);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::DivI32:
    case IrOpcode::DivI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on div";
        return finishFault();
      }
      const int64_t rhs = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      const int64_t lhs = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      if (rhs == 0) {
        error = "division by zero in IR";
        return finishFault();
      }
      stack_.push_back(static_cast<uint64_t>(lhs / rhs));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::DivU64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on div";
        return finishFault();
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      if (rhs == 0) {
        error = "division by zero in IR";
        return finishFault();
      }
      stack_.push_back(lhs / rhs);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::NegI32:
    case IrOpcode::NegI64: {
      if (stack_.empty()) {
        error = "IR stack underflow on negate";
        return finishFault();
      }
      const int64_t value = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      stack_.push_back(static_cast<uint64_t>(-value));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on float op";
        return finishFault();
      }
      const float rhs = bitsToF32(stack_.back());
      stack_.pop_back();
      const float lhs = bitsToF32(stack_.back());
      stack_.pop_back();
      float out = 0.0f;
      switch (inst.op) {
        case IrOpcode::AddF32:
          out = lhs + rhs;
          break;
        case IrOpcode::SubF32:
          out = lhs - rhs;
          break;
        case IrOpcode::MulF32:
          out = lhs * rhs;
          break;
        case IrOpcode::DivF32:
          out = lhs / rhs;
          break;
        default:
          break;
      }
      stack_.push_back(f32ToBits(out));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::NegF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on negate";
        return finishFault();
      }
      const float value = bitsToF32(stack_.back());
      stack_.pop_back();
      stack_.push_back(f32ToBits(-value));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on float op";
        return finishFault();
      }
      const double rhs = bitsToF64(stack_.back());
      stack_.pop_back();
      const double lhs = bitsToF64(stack_.back());
      stack_.pop_back();
      double out = 0.0;
      switch (inst.op) {
        case IrOpcode::AddF64:
          out = lhs + rhs;
          break;
        case IrOpcode::SubF64:
          out = lhs - rhs;
          break;
        case IrOpcode::MulF64:
          out = lhs * rhs;
          break;
        case IrOpcode::DivF64:
          out = lhs / rhs;
          break;
        default:
          break;
      }
      stack_.push_back(f64ToBits(out));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::NegF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on negate";
        return finishFault();
      }
      const double value = bitsToF64(stack_.back());
      stack_.pop_back();
      stack_.push_back(f64ToBits(-value));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
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
    case IrOpcode::CmpGeU64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on compare";
        return finishFault();
      }
      const uint64_t rhsRaw = stack_.back();
      stack_.pop_back();
      const uint64_t lhsRaw = stack_.back();
      stack_.pop_back();
      bool cmp = false;
      switch (inst.op) {
        case IrOpcode::CmpEqI32:
        case IrOpcode::CmpEqI64:
          cmp = lhsRaw == rhsRaw;
          break;
        case IrOpcode::CmpNeI32:
        case IrOpcode::CmpNeI64:
          cmp = lhsRaw != rhsRaw;
          break;
        case IrOpcode::CmpLtI32:
        case IrOpcode::CmpLtI64:
          cmp = static_cast<int64_t>(lhsRaw) < static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpLeI32:
        case IrOpcode::CmpLeI64:
          cmp = static_cast<int64_t>(lhsRaw) <= static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpGtI32:
        case IrOpcode::CmpGtI64:
          cmp = static_cast<int64_t>(lhsRaw) > static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpGeI32:
        case IrOpcode::CmpGeI64:
          cmp = static_cast<int64_t>(lhsRaw) >= static_cast<int64_t>(rhsRaw);
          break;
        case IrOpcode::CmpLtU64:
          cmp = lhsRaw < rhsRaw;
          break;
        case IrOpcode::CmpLeU64:
          cmp = lhsRaw <= rhsRaw;
          break;
        case IrOpcode::CmpGtU64:
          cmp = lhsRaw > rhsRaw;
          break;
        case IrOpcode::CmpGeU64:
          cmp = lhsRaw >= rhsRaw;
          break;
        default:
          break;
      }
      stack_.push_back(cmp ? 1 : 0);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on compare";
        return finishFault();
      }
      const float rhs = bitsToF32(stack_.back());
      stack_.pop_back();
      const float lhs = bitsToF32(stack_.back());
      stack_.pop_back();
      bool cmp = false;
      switch (inst.op) {
        case IrOpcode::CmpEqF32:
          cmp = lhs == rhs;
          break;
        case IrOpcode::CmpNeF32:
          cmp = lhs != rhs;
          break;
        case IrOpcode::CmpLtF32:
          cmp = lhs < rhs;
          break;
        case IrOpcode::CmpLeF32:
          cmp = lhs <= rhs;
          break;
        case IrOpcode::CmpGtF32:
          cmp = lhs > rhs;
          break;
        case IrOpcode::CmpGeF32:
          cmp = lhs >= rhs;
          break;
        default:
          break;
      }
      stack_.push_back(cmp ? 1 : 0);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on compare";
        return finishFault();
      }
      const double rhs = bitsToF64(stack_.back());
      stack_.pop_back();
      const double lhs = bitsToF64(stack_.back());
      stack_.pop_back();
      bool cmp = false;
      switch (inst.op) {
        case IrOpcode::CmpEqF64:
          cmp = lhs == rhs;
          break;
        case IrOpcode::CmpNeF64:
          cmp = lhs != rhs;
          break;
        case IrOpcode::CmpLtF64:
          cmp = lhs < rhs;
          break;
        case IrOpcode::CmpLeF64:
          cmp = lhs <= rhs;
          break;
        case IrOpcode::CmpGtF64:
          cmp = lhs > rhs;
          break;
        case IrOpcode::CmpGeF64:
          cmp = lhs >= rhs;
          break;
        default:
          break;
      }
      stack_.push_back(cmp ? 1 : 0);
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertU64ToF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return finishFault();
      }
      const uint64_t raw = stack_.back();
      stack_.pop_back();
      float value = 0.0f;
      if (inst.op == IrOpcode::ConvertU64ToF32) {
        value = static_cast<float>(raw);
      } else if (inst.op == IrOpcode::ConvertI64ToF32) {
        value = static_cast<float>(static_cast<int64_t>(raw));
      } else {
        value = static_cast<float>(static_cast<int32_t>(raw));
      }
      stack_.push_back(f32ToBits(value));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return finishFault();
      }
      const uint64_t raw = stack_.back();
      stack_.pop_back();
      double value = 0.0;
      if (inst.op == IrOpcode::ConvertU64ToF64) {
        value = static_cast<double>(raw);
      } else if (inst.op == IrOpcode::ConvertI64ToF64) {
        value = static_cast<double>(static_cast<int64_t>(raw));
      } else {
        value = static_cast<double>(static_cast<int32_t>(raw));
      }
      stack_.push_back(f64ToBits(value));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return finishFault();
      }
      const float value = bitsToF32(stack_.back());
      stack_.pop_back();
      if (inst.op == IrOpcode::ConvertF32ToU64) {
        const uint64_t out = static_cast<uint64_t>(value);
        stack_.push_back(out);
      } else if (inst.op == IrOpcode::ConvertF32ToI64) {
        const int64_t out = static_cast<int64_t>(value);
        stack_.push_back(static_cast<uint64_t>(out));
      } else {
        const int32_t out = static_cast<int32_t>(value);
        stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(out)));
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return finishFault();
      }
      const double value = bitsToF64(stack_.back());
      stack_.pop_back();
      if (inst.op == IrOpcode::ConvertF64ToU64) {
        const uint64_t out = static_cast<uint64_t>(value);
        stack_.push_back(out);
      } else if (inst.op == IrOpcode::ConvertF64ToI64) {
        const int64_t out = static_cast<int64_t>(value);
        stack_.push_back(static_cast<uint64_t>(out));
      } else {
        const int32_t out = static_cast<int32_t>(value);
        stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(out)));
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ConvertF32ToF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return finishFault();
      }
      const float value = bitsToF32(stack_.back());
      stack_.pop_back();
      const double out = static_cast<double>(value);
      stack_.push_back(f64ToBits(out));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ConvertF64ToF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return finishFault();
      }
      const double value = bitsToF64(stack_.back());
      stack_.pop_back();
      const float out = static_cast<float>(value);
      stack_.push_back(f32ToBits(out));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::JumpIfZero: {
      if (stack_.empty()) {
        error = "IR stack underflow on jump";
        return finishFault();
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      if (inst.imm > fn.instructions.size()) {
        error = "invalid jump target in IR";
        return finishFault();
      }
      if (value == 0) {
        ip = static_cast<size_t>(inst.imm);
      } else {
        ip += 1;
      }
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::Jump: {
      if (inst.imm > fn.instructions.size()) {
        error = "invalid jump target in IR";
        return finishFault();
      }
      ip = static_cast<size_t>(inst.imm);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::Call:
    case IrOpcode::CallVoid: {
      if (!module_ || inst.imm >= module_->functions.size()) {
        error = "invalid call target in IR";
        return finishFault();
      }
      if (frames_.size() >= MaxCallDepth) {
        error = "VM call stack overflow";
        return finishFault();
      }
      const size_t target = static_cast<size_t>(inst.imm);
      Frame calleeFrame;
      calleeFrame.functionIndex = target;
      calleeFrame.function = &module_->functions[target];
      calleeFrame.locals.assign(localCounts_[target], 0);
      calleeFrame.returnValueToCaller = (inst.op == IrOpcode::Call);
      ip += 1;
      frames_.push_back(std::move(calleeFrame));
      emitCallHook(hooks_.callPush, target, inst.op == IrOpcode::Call);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ReturnVoid: {
      const uint64_t returnValue = 0;
      const bool returnToCaller = frame.returnValueToCaller;
      const size_t poppedFunctionIndex = frame.functionIndex;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
        return finishStep(StepOutcome::Exit);
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ReturnI32: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return finishFault();
      }
      const uint64_t returnValue = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(stack_.back())));
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      const size_t poppedFunctionIndex = frame.functionIndex;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
        return finishStep(StepOutcome::Exit);
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ReturnI64: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return finishFault();
      }
      const uint64_t returnValue = stack_.back();
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      const size_t poppedFunctionIndex = frame.functionIndex;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
        return finishStep(StepOutcome::Exit);
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ReturnF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return finishFault();
      }
      const uint64_t returnValue = static_cast<uint64_t>(static_cast<uint32_t>(stack_.back()));
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      const size_t poppedFunctionIndex = frame.functionIndex;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
        return finishStep(StepOutcome::Exit);
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::ReturnF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return finishFault();
      }
      const uint64_t returnValue = stack_.back();
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      const size_t poppedFunctionIndex = frame.functionIndex;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
        return finishStep(StepOutcome::Exit);
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      emitCallHook(hooks_.callPop, poppedFunctionIndex, returnToCaller);
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintI32: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintI64: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintU64: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintString: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintStringDynamic: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintArgv: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::PrintArgvUnsafe: {
      if (!vm_detail::handlePrintOpcode(*module_, inst, stack_, args_, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileOpenReadDynamic:
    case IrOpcode::FileOpenWriteDynamic:
    case IrOpcode::FileOpenAppendDynamic: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileClose: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileReadByte: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileFlush: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteI32: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteI64: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteU64: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteString: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteStringDynamic: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteByte: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::FileWriteNewline: {
      if (!vm_detail::handleFileOpcode(*module_, inst, stack_, locals, error)) {
        return finishFault();
      }
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadStringByte: {
      if (stack_.empty()) {
        error = "IR stack underflow on string index";
        return finishFault();
      }
      const uint64_t indexRaw = stack_.back();
      stack_.pop_back();
      const uint64_t stringIndex = inst.imm;
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return finishFault();
      }
      const std::string &text = module_->stringTable[static_cast<size_t>(stringIndex)];
      const size_t index = static_cast<size_t>(indexRaw);
      if (index >= text.size()) {
        error = "string index out of bounds in IR";
        return finishFault();
      }
      const uint8_t byte = static_cast<uint8_t>(text[index]);
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte))));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    case IrOpcode::LoadStringLength: {
      if (stack_.empty()) {
        error = "IR stack underflow on string index";
        return finishFault();
      }
      const uint64_t stringIndex = stack_.back();
      stack_.pop_back();
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return finishFault();
      }
      stack_.push_back(
          static_cast<uint64_t>(module_->stringTable[static_cast<size_t>(stringIndex)].size()));
      ip += 1;
      return finishStep(StepOutcome::Continue);
    }
    default:
      error = "unknown IR opcode";
      return finishFault();
  }
}

} // namespace primec
