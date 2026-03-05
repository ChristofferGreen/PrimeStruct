#include "primec/Vm.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
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
  std::vector<Frame> frames;
  frames.reserve(64);
  Frame entryFrame;
  entryFrame.functionIndex = static_cast<size_t>(module.entryIndex);
  entryFrame.function = &module.functions[entryFrame.functionIndex];
  entryFrame.locals.assign(localCounts[entryFrame.functionIndex], 0);
  frames.push_back(std::move(entryFrame));
  auto writeAll = [&](int fd, const void *data, size_t size) -> int {
    const char *cursor = static_cast<const char *>(data);
    size_t remaining = size;
    while (remaining > 0) {
      ssize_t wrote = ::write(fd, cursor, remaining);
      if (wrote < 0) {
        return errno == 0 ? EIO : errno;
      }
      if (wrote == 0) {
        return EIO;
      }
      remaining -= static_cast<size_t>(wrote);
      cursor += wrote;
    }
    return 0;
  };

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
        if (address % kSlotBytes != 0) {
          error = "unaligned indirect address in IR: " + std::to_string(address);
          return false;
        }
        uint64_t index = address / kSlotBytes;
        if (index >= locals.size()) {
          error = "invalid indirect address in IR: " + std::to_string(address);
          return false;
        }
        stack.push_back(locals[static_cast<size_t>(index)]);
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
        if (address % kSlotBytes != 0) {
          error = "unaligned indirect address in IR: " + std::to_string(address);
          return false;
        }
        uint64_t index = address / kSlotBytes;
        if (index >= locals.size()) {
          error = "invalid indirect address in IR: " + std::to_string(address);
          return false;
        }
        locals[static_cast<size_t>(index)] = value;
        stack.push_back(value);
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
        if (stack.empty()) {
          error = "IR stack underflow on print";
          return false;
        }
        int32_t value = static_cast<int32_t>(stack.back());
        stack.pop_back();
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        std::string text = std::to_string(value);
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintI64: {
        if (stack.empty()) {
          error = "IR stack underflow on print";
          return false;
        }
        int64_t value = static_cast<int64_t>(stack.back());
        stack.pop_back();
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        std::string text = std::to_string(value);
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintU64: {
        if (stack.empty()) {
          error = "IR stack underflow on print";
          return false;
        }
        uint64_t value = stack.back();
        stack.pop_back();
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        std::string text = std::to_string(value);
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintString: {
        uint64_t stringIndex = decodePrintStringIndex(inst.imm);
        if (stringIndex >= module.stringTable.size()) {
          error = "invalid string index in IR";
          return false;
        }
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        const std::string &text = module.stringTable[static_cast<size_t>(stringIndex)];
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintStringDynamic: {
        if (stack.empty()) {
          error = "IR stack underflow on print";
          return false;
        }
        uint64_t stringIndex = stack.back();
        stack.pop_back();
        if (stringIndex >= module.stringTable.size()) {
          error = "invalid string index in IR";
          return false;
        }
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        const std::string &text = module.stringTable[static_cast<size_t>(stringIndex)];
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintArgv: {
        if (stack.empty()) {
          error = "IR stack underflow on print";
          return false;
        }
        if (!args) {
          error = "VM missing argv data for PrintArgv";
          return false;
        }
        int64_t index = static_cast<int64_t>(stack.back());
        stack.pop_back();
        if (index < 0 || static_cast<size_t>(index) >= args->size()) {
          error = "invalid argv index in IR";
          return false;
        }
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        std::string_view text = (*args)[static_cast<size_t>(index)];
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::PrintArgvUnsafe: {
        if (stack.empty()) {
          error = "IR stack underflow on print";
          return false;
        }
        if (!args) {
          error = "VM missing argv data for PrintArgvUnsafe";
          return false;
        }
        int64_t index = static_cast<int64_t>(stack.back());
        stack.pop_back();
        if (index < 0 || static_cast<size_t>(index) >= args->size()) {
          ip += 1;
          break;
        }
        uint64_t flags = decodePrintFlags(inst.imm);
        FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
        bool newline = (flags & PrintFlagNewline) != 0;
        std::string_view text = (*args)[static_cast<size_t>(index)];
        std::fwrite(text.data(), 1, text.size(), out);
        if (newline) {
          std::fputc('\n', out);
        }
        ip += 1;
        break;
      }
      case IrOpcode::FileOpenRead:
      case IrOpcode::FileOpenWrite:
      case IrOpcode::FileOpenAppend: {
        if (inst.imm >= module.stringTable.size()) {
          error = "invalid string index in IR";
          return false;
        }
        const std::string &path = module.stringTable[static_cast<size_t>(inst.imm)];
        int flags = O_RDONLY;
        int mode = 0644;
        if (inst.op == IrOpcode::FileOpenWrite) {
          flags = O_WRONLY | O_CREAT | O_TRUNC;
        } else if (inst.op == IrOpcode::FileOpenAppend) {
          flags = O_WRONLY | O_CREAT | O_APPEND;
        }
        int fd = ::open(path.c_str(), flags, mode);
        uint64_t packed = 0;
        if (fd < 0) {
          uint32_t err = errno == 0 ? 1u : static_cast<uint32_t>(errno);
          packed = (static_cast<uint64_t>(err) << 32);
        } else {
          packed = static_cast<uint64_t>(static_cast<uint32_t>(fd));
        }
        stack.push_back(packed);
        ip += 1;
        break;
      }
      case IrOpcode::FileClose: {
        if (stack.empty()) {
          error = "IR stack underflow on file close";
          return false;
        }
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        int rc = ::close(fd);
        uint32_t err = (rc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileFlush: {
        if (stack.empty()) {
          error = "IR stack underflow on file flush";
          return false;
        }
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        int rc = ::fsync(fd);
        uint32_t err = (rc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteI32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on file write";
          return false;
        }
        int64_t value = static_cast<int64_t>(static_cast<int32_t>(stack.back()));
        stack.pop_back();
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        std::string text = std::to_string(value);
        uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteI64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on file write";
          return false;
        }
        int64_t value = static_cast<int64_t>(stack.back());
        stack.pop_back();
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        std::string text = std::to_string(value);
        uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteU64: {
        if (stack.size() < 2) {
          error = "IR stack underflow on file write";
          return false;
        }
        uint64_t value = stack.back();
        stack.pop_back();
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        std::string text = std::to_string(static_cast<unsigned long long>(value));
        uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteString: {
        if (stack.empty()) {
          error = "IR stack underflow on file write";
          return false;
        }
        if (inst.imm >= module.stringTable.size()) {
          error = "invalid string index in IR";
          return false;
        }
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        const std::string &text = module.stringTable[static_cast<size_t>(inst.imm)];
        uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteByte: {
        if (stack.size() < 2) {
          error = "IR stack underflow on file write";
          return false;
        }
        uint8_t value = static_cast<uint8_t>(stack.back() & 0xffu);
        stack.pop_back();
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        uint32_t err = static_cast<uint32_t>(writeAll(fd, &value, 1));
        stack.push_back(static_cast<uint64_t>(err));
        ip += 1;
        break;
      }
      case IrOpcode::FileWriteNewline: {
        if (stack.empty()) {
          error = "IR stack underflow on file write";
          return false;
        }
        uint64_t handle = stack.back();
        stack.pop_back();
        int fd = static_cast<int>(handle & 0xffffffffu);
        char newline = '\n';
        uint32_t err = static_cast<uint32_t>(writeAll(fd, &newline, 1));
        stack.push_back(static_cast<uint64_t>(err));
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

bool VmDebugSession::initFromModule(const IrModule &module,
                                    uint64_t argCount,
                                    const std::vector<std::string_view> *args) {
  module_ = &module;
  argCount_ = argCount;
  args_ = args;
  localCounts_.assign(module.functions.size(), 0);
  stack_.clear();
  frames_.clear();
  result_ = 0;
  pauseRequested_ = false;
  for (size_t i = 0; i < module.functions.size(); ++i) {
    size_t localCount = 0;
    for (const auto &inst : module.functions[i].instructions) {
      if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal || inst.op == IrOpcode::AddressOfLocal) {
        localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
      }
    }
    localCounts_[i] = localCount;
  }

  Frame entryFrame;
  entryFrame.functionIndex = static_cast<size_t>(module.entryIndex);
  entryFrame.function = &module.functions[entryFrame.functionIndex];
  entryFrame.locals.assign(localCounts_[entryFrame.functionIndex], 0);
  frames_.push_back(std::move(entryFrame));

  const VmDebugTransitionResult startTransition =
      vmDebugApplyCommand(VmDebugSessionState::Idle, VmDebugSessionCommand::Start);
  if (!startTransition.valid) {
    return false;
  }
  state_ = startTransition.state;
  const VmDebugTransitionResult pauseTransition = vmDebugApplyStopReason(state_, VmDebugStopReason::Pause);
  if (!pauseTransition.valid) {
    return false;
  }
  state_ = pauseTransition.state;
  return true;
}

bool VmDebugSession::start(const IrModule &module, std::string &error, uint64_t argCount) {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  ownedArgs_.clear();
  if (!initFromModule(module, argCount, nullptr)) {
    error = "failed to initialize debug session";
    return false;
  }
  return true;
}

bool VmDebugSession::start(const IrModule &module,
                          std::string &error,
                          const std::vector<std::string_view> &args) {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  ownedArgs_ = args;
  if (!initFromModule(module, static_cast<uint64_t>(ownedArgs_.size()), &ownedArgs_)) {
    error = "failed to initialize debug session";
    return false;
  }
  return true;
}

VmDebugSession::StepOutcome VmDebugSession::stepInstruction(std::string &error) {
  constexpr uint64_t kSlotBytes = IrSlotBytes;
  constexpr size_t MaxCallDepth = 4096;
  if (!module_) {
    error = "debug session has no active module";
    return StepOutcome::Fault;
  }
  auto writeAll = [&](int fd, const void *data, size_t size) -> int {
    const char *cursor = static_cast<const char *>(data);
    size_t remaining = size;
    while (remaining > 0) {
      ssize_t wrote = ::write(fd, cursor, remaining);
      if (wrote < 0) {
        return errno == 0 ? EIO : errno;
      }
      if (wrote == 0) {
        return EIO;
      }
      remaining -= static_cast<size_t>(wrote);
      cursor += wrote;
    }
    return 0;
  };
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
    return StepOutcome::Fault;
  }
  const IrInstruction &inst = fn.instructions[ip];
  switch (inst.op) {
    case IrOpcode::PushI32:
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(inst.imm))));
      ip += 1;
      return StepOutcome::Continue;
    case IrOpcode::PushI64:
      stack_.push_back(inst.imm);
      ip += 1;
      return StepOutcome::Continue;
    case IrOpcode::PushF32:
    case IrOpcode::PushF64:
      stack_.push_back(inst.imm);
      ip += 1;
      return StepOutcome::Continue;
    case IrOpcode::PushArgc: {
      const int32_t count32 = static_cast<int32_t>(argCount_);
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(count32)));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::LoadLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return StepOutcome::Fault;
      }
      stack_.push_back(locals[static_cast<size_t>(inst.imm)]);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::StoreLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return StepOutcome::Fault;
      }
      if (stack_.empty()) {
        error = "IR stack underflow on store";
        return StepOutcome::Fault;
      }
      locals[static_cast<size_t>(inst.imm)] = stack_.back();
      stack_.pop_back();
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::AddressOfLocal: {
      if (static_cast<size_t>(inst.imm) >= locals.size()) {
        error = "invalid local index in IR";
        return StepOutcome::Fault;
      }
      stack_.push_back(static_cast<uint64_t>(inst.imm) * kSlotBytes);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::LoadIndirect: {
      if (stack_.empty()) {
        error = "IR stack underflow on load indirect";
        return StepOutcome::Fault;
      }
      const uint64_t address = stack_.back();
      stack_.pop_back();
      if (address % kSlotBytes != 0) {
        error = "unaligned indirect address in IR: " + std::to_string(address);
        return StepOutcome::Fault;
      }
      const uint64_t index = address / kSlotBytes;
      if (index >= locals.size()) {
        error = "invalid indirect address in IR: " + std::to_string(address);
        return StepOutcome::Fault;
      }
      stack_.push_back(locals[static_cast<size_t>(index)]);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::StoreIndirect: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on store indirect";
        return StepOutcome::Fault;
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      const uint64_t address = stack_.back();
      stack_.pop_back();
      if (address % kSlotBytes != 0) {
        error = "unaligned indirect address in IR: " + std::to_string(address);
        return StepOutcome::Fault;
      }
      const uint64_t index = address / kSlotBytes;
      if (index >= locals.size()) {
        error = "invalid indirect address in IR: " + std::to_string(address);
        return StepOutcome::Fault;
      }
      locals[static_cast<size_t>(index)] = value;
      stack_.push_back(value);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::Dup: {
      if (stack_.empty()) {
        error = "IR stack underflow on dup";
        return StepOutcome::Fault;
      }
      stack_.push_back(stack_.back());
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::Pop: {
      if (stack_.empty()) {
        error = "IR stack underflow on pop";
        return StepOutcome::Fault;
      }
      stack_.pop_back();
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::AddI32:
    case IrOpcode::AddI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on add";
        return StepOutcome::Fault;
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      stack_.push_back(lhs + rhs);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::SubI32:
    case IrOpcode::SubI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on sub";
        return StepOutcome::Fault;
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      stack_.push_back(lhs - rhs);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::MulI32:
    case IrOpcode::MulI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on mul";
        return StepOutcome::Fault;
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      stack_.push_back(lhs * rhs);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::DivI32:
    case IrOpcode::DivI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on div";
        return StepOutcome::Fault;
      }
      const int64_t rhs = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      const int64_t lhs = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      if (rhs == 0) {
        error = "division by zero in IR";
        return StepOutcome::Fault;
      }
      stack_.push_back(static_cast<uint64_t>(lhs / rhs));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::DivU64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on div";
        return StepOutcome::Fault;
      }
      const uint64_t rhs = stack_.back();
      stack_.pop_back();
      const uint64_t lhs = stack_.back();
      stack_.pop_back();
      if (rhs == 0) {
        error = "division by zero in IR";
        return StepOutcome::Fault;
      }
      stack_.push_back(lhs / rhs);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::NegI32:
    case IrOpcode::NegI64: {
      if (stack_.empty()) {
        error = "IR stack underflow on negate";
        return StepOutcome::Fault;
      }
      const int64_t value = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      stack_.push_back(static_cast<uint64_t>(-value));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on float op";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::NegF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on negate";
        return StepOutcome::Fault;
      }
      const float value = bitsToF32(stack_.back());
      stack_.pop_back();
      stack_.push_back(f32ToBits(-value));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on float op";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::NegF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on negate";
        return StepOutcome::Fault;
      }
      const double value = bitsToF64(stack_.back());
      stack_.pop_back();
      stack_.push_back(f64ToBits(-value));
      ip += 1;
      return StepOutcome::Continue;
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
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::CmpEqF32:
    case IrOpcode::CmpNeF32:
    case IrOpcode::CmpLtF32:
    case IrOpcode::CmpLeF32:
    case IrOpcode::CmpGtF32:
    case IrOpcode::CmpGeF32: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on compare";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::CmpEqF64:
    case IrOpcode::CmpNeF64:
    case IrOpcode::CmpLtF64:
    case IrOpcode::CmpLeF64:
    case IrOpcode::CmpGtF64:
    case IrOpcode::CmpGeF64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on compare";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertU64ToF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF32ToU64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::ConvertF64ToI32:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF64ToU64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return StepOutcome::Fault;
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
      return StepOutcome::Continue;
    }
    case IrOpcode::ConvertF32ToF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return StepOutcome::Fault;
      }
      const float value = bitsToF32(stack_.back());
      stack_.pop_back();
      const double out = static_cast<double>(value);
      stack_.push_back(f64ToBits(out));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::ConvertF64ToF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on convert";
        return StepOutcome::Fault;
      }
      const double value = bitsToF64(stack_.back());
      stack_.pop_back();
      const float out = static_cast<float>(value);
      stack_.push_back(f32ToBits(out));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::JumpIfZero: {
      if (stack_.empty()) {
        error = "IR stack underflow on jump";
        return StepOutcome::Fault;
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      if (inst.imm > fn.instructions.size()) {
        error = "invalid jump target in IR";
        return StepOutcome::Fault;
      }
      if (value == 0) {
        ip = static_cast<size_t>(inst.imm);
      } else {
        ip += 1;
      }
      return StepOutcome::Continue;
    }
    case IrOpcode::Jump: {
      if (inst.imm > fn.instructions.size()) {
        error = "invalid jump target in IR";
        return StepOutcome::Fault;
      }
      ip = static_cast<size_t>(inst.imm);
      return StepOutcome::Continue;
    }
    case IrOpcode::Call:
    case IrOpcode::CallVoid: {
      if (!module_ || inst.imm >= module_->functions.size()) {
        error = "invalid call target in IR";
        return StepOutcome::Fault;
      }
      if (frames_.size() >= MaxCallDepth) {
        error = "VM call stack overflow";
        return StepOutcome::Fault;
      }
      const size_t target = static_cast<size_t>(inst.imm);
      Frame calleeFrame;
      calleeFrame.functionIndex = target;
      calleeFrame.function = &module_->functions[target];
      calleeFrame.locals.assign(localCounts_[target], 0);
      calleeFrame.returnValueToCaller = (inst.op == IrOpcode::Call);
      ip += 1;
      frames_.push_back(std::move(calleeFrame));
      return StepOutcome::Continue;
    }
    case IrOpcode::ReturnVoid: {
      const uint64_t returnValue = 0;
      const bool returnToCaller = frame.returnValueToCaller;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        return StepOutcome::Exit;
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      return StepOutcome::Continue;
    }
    case IrOpcode::ReturnI32: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return StepOutcome::Fault;
      }
      const uint64_t returnValue = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(stack_.back())));
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        return StepOutcome::Exit;
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      return StepOutcome::Continue;
    }
    case IrOpcode::ReturnI64: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return StepOutcome::Fault;
      }
      const uint64_t returnValue = stack_.back();
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        return StepOutcome::Exit;
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      return StepOutcome::Continue;
    }
    case IrOpcode::ReturnF32: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return StepOutcome::Fault;
      }
      const uint64_t returnValue = static_cast<uint64_t>(static_cast<uint32_t>(stack_.back()));
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        return StepOutcome::Exit;
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      return StepOutcome::Continue;
    }
    case IrOpcode::ReturnF64: {
      if (stack_.empty()) {
        error = "IR stack underflow on return";
        return StepOutcome::Fault;
      }
      const uint64_t returnValue = stack_.back();
      stack_.pop_back();
      const bool returnToCaller = frame.returnValueToCaller;
      if (frames_.size() == 1) {
        result_ = returnValue;
        frames_.clear();
        return StepOutcome::Exit;
      }
      frames_.pop_back();
      if (returnToCaller) {
        stack_.push_back(returnValue);
      }
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintI32: {
      if (stack_.empty()) {
        error = "IR stack underflow on print";
        return StepOutcome::Fault;
      }
      const int32_t value = static_cast<int32_t>(stack_.back());
      stack_.pop_back();
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string text = std::to_string(value);
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintI64: {
      if (stack_.empty()) {
        error = "IR stack underflow on print";
        return StepOutcome::Fault;
      }
      const int64_t value = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string text = std::to_string(value);
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintU64: {
      if (stack_.empty()) {
        error = "IR stack underflow on print";
        return StepOutcome::Fault;
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string text = std::to_string(value);
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintString: {
      const uint64_t stringIndex = decodePrintStringIndex(inst.imm);
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return StepOutcome::Fault;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string &text = module_->stringTable[static_cast<size_t>(stringIndex)];
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintStringDynamic: {
      if (stack_.empty()) {
        error = "IR stack underflow on print";
        return StepOutcome::Fault;
      }
      const uint64_t stringIndex = stack_.back();
      stack_.pop_back();
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return StepOutcome::Fault;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string &text = module_->stringTable[static_cast<size_t>(stringIndex)];
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintArgv: {
      if (stack_.empty()) {
        error = "IR stack underflow on print";
        return StepOutcome::Fault;
      }
      if (!args_) {
        error = "VM missing argv data for PrintArgv";
        return StepOutcome::Fault;
      }
      const int64_t index = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      if (index < 0 || static_cast<size_t>(index) >= args_->size()) {
        error = "invalid argv index in IR";
        return StepOutcome::Fault;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string_view text = (*args_)[static_cast<size_t>(index)];
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::PrintArgvUnsafe: {
      if (stack_.empty()) {
        error = "IR stack underflow on print";
        return StepOutcome::Fault;
      }
      if (!args_) {
        error = "VM missing argv data for PrintArgvUnsafe";
        return StepOutcome::Fault;
      }
      const int64_t index = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      if (index < 0 || static_cast<size_t>(index) >= args_->size()) {
        ip += 1;
        return StepOutcome::Continue;
      }
      const uint64_t flags = decodePrintFlags(inst.imm);
      FILE *out = (flags & PrintFlagStderr) ? stderr : stdout;
      const bool newline = (flags & PrintFlagNewline) != 0;
      const std::string_view text = (*args_)[static_cast<size_t>(index)];
      std::fwrite(text.data(), 1, text.size(), out);
      if (newline) {
        std::fputc('\n', out);
      }
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileOpenRead:
    case IrOpcode::FileOpenWrite:
    case IrOpcode::FileOpenAppend: {
      if (inst.imm >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return StepOutcome::Fault;
      }
      const std::string &path = module_->stringTable[static_cast<size_t>(inst.imm)];
      int flags = O_RDONLY;
      const int mode = 0644;
      if (inst.op == IrOpcode::FileOpenWrite) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
      } else if (inst.op == IrOpcode::FileOpenAppend) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
      }
      const int fd = ::open(path.c_str(), flags, mode);
      uint64_t packed = 0;
      if (fd < 0) {
        const uint32_t err = errno == 0 ? 1u : static_cast<uint32_t>(errno);
        packed = static_cast<uint64_t>(err) << 32;
      } else {
        packed = static_cast<uint64_t>(static_cast<uint32_t>(fd));
      }
      stack_.push_back(packed);
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileClose: {
      if (stack_.empty()) {
        error = "IR stack underflow on file close";
        return StepOutcome::Fault;
      }
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const int rc = ::close(fd);
      const uint32_t err = (rc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileFlush: {
      if (stack_.empty()) {
        error = "IR stack underflow on file flush";
        return StepOutcome::Fault;
      }
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const int rc = ::fsync(fd);
      const uint32_t err = (rc < 0) ? (errno == 0 ? 1u : static_cast<uint32_t>(errno)) : 0u;
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileWriteI32: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on file write";
        return StepOutcome::Fault;
      }
      const int64_t value = static_cast<int64_t>(static_cast<int32_t>(stack_.back()));
      stack_.pop_back();
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string text = std::to_string(value);
      const uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileWriteI64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on file write";
        return StepOutcome::Fault;
      }
      const int64_t value = static_cast<int64_t>(stack_.back());
      stack_.pop_back();
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string text = std::to_string(value);
      const uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileWriteU64: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on file write";
        return StepOutcome::Fault;
      }
      const uint64_t value = stack_.back();
      stack_.pop_back();
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string text = std::to_string(static_cast<unsigned long long>(value));
      const uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileWriteString: {
      if (stack_.empty()) {
        error = "IR stack underflow on file write";
        return StepOutcome::Fault;
      }
      if (inst.imm >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return StepOutcome::Fault;
      }
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const std::string &text = module_->stringTable[static_cast<size_t>(inst.imm)];
      const uint32_t err = static_cast<uint32_t>(writeAll(fd, text.data(), text.size()));
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileWriteByte: {
      if (stack_.size() < 2) {
        error = "IR stack underflow on file write";
        return StepOutcome::Fault;
      }
      const uint8_t value = static_cast<uint8_t>(stack_.back() & 0xffu);
      stack_.pop_back();
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const uint32_t err = static_cast<uint32_t>(writeAll(fd, &value, 1));
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::FileWriteNewline: {
      if (stack_.empty()) {
        error = "IR stack underflow on file write";
        return StepOutcome::Fault;
      }
      const uint64_t handle = stack_.back();
      stack_.pop_back();
      const int fd = static_cast<int>(handle & 0xffffffffu);
      const char newline = '\n';
      const uint32_t err = static_cast<uint32_t>(writeAll(fd, &newline, 1));
      stack_.push_back(static_cast<uint64_t>(err));
      ip += 1;
      return StepOutcome::Continue;
    }
    case IrOpcode::LoadStringByte: {
      if (stack_.empty()) {
        error = "IR stack underflow on string index";
        return StepOutcome::Fault;
      }
      const uint64_t indexRaw = stack_.back();
      stack_.pop_back();
      const uint64_t stringIndex = inst.imm;
      if (stringIndex >= module_->stringTable.size()) {
        error = "invalid string index in IR";
        return StepOutcome::Fault;
      }
      const std::string &text = module_->stringTable[static_cast<size_t>(stringIndex)];
      const size_t index = static_cast<size_t>(indexRaw);
      if (index >= text.size()) {
        error = "string index out of bounds in IR";
        return StepOutcome::Fault;
      }
      const uint8_t byte = static_cast<uint8_t>(text[index]);
      stack_.push_back(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(byte))));
      ip += 1;
      return StepOutcome::Continue;
    }
    default:
      error = "unknown IR opcode";
      return StepOutcome::Fault;
  }
}

bool VmDebugSession::step(VmDebugStopReason &stopReason, std::string &error) {
  if (state_ == VmDebugSessionState::Idle) {
    error = "debug session is not started";
    return false;
  }
  if (state_ != VmDebugSessionState::Paused) {
    error = "debug session is not paused";
    return false;
  }
  const VmDebugTransitionResult continueTransition = vmDebugApplyCommand(state_, VmDebugSessionCommand::Continue);
  if (!continueTransition.valid) {
    error = "debug session cannot step from current state";
    return false;
  }
  state_ = continueTransition.state;
  const StepOutcome outcome = stepInstruction(error);
  if (outcome == StepOutcome::Fault) {
    stopReason = VmDebugStopReason::Fault;
    const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
    if (stopTransition.valid) {
      state_ = stopTransition.state;
    }
    return false;
  }
  if (outcome == StepOutcome::Exit) {
    stopReason = VmDebugStopReason::Exit;
    const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
    if (stopTransition.valid) {
      state_ = stopTransition.state;
    }
    return true;
  }
  stopReason = VmDebugStopReason::Step;
  const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
  if (!stopTransition.valid) {
    error = "debug session failed to enter paused state after step";
    return false;
  }
  state_ = stopTransition.state;
  return true;
}

bool VmDebugSession::continueExecution(VmDebugStopReason &stopReason, std::string &error) {
  if (state_ == VmDebugSessionState::Idle) {
    error = "debug session is not started";
    return false;
  }
  if (state_ != VmDebugSessionState::Paused) {
    error = "debug session is not paused";
    return false;
  }
  const VmDebugTransitionResult continueTransition = vmDebugApplyCommand(state_, VmDebugSessionCommand::Continue);
  if (!continueTransition.valid) {
    error = "debug session cannot continue from current state";
    return false;
  }
  state_ = continueTransition.state;
  if (pauseRequested_) {
    pauseRequested_ = false;
    stopReason = VmDebugStopReason::Pause;
    const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
    if (!stopTransition.valid) {
      error = "debug session failed to pause";
      return false;
    }
    state_ = stopTransition.state;
    return true;
  }
  while (true) {
    const StepOutcome outcome = stepInstruction(error);
    if (outcome == StepOutcome::Fault) {
      stopReason = VmDebugStopReason::Fault;
      const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
      if (stopTransition.valid) {
        state_ = stopTransition.state;
      }
      return false;
    }
    if (outcome == StepOutcome::Exit) {
      stopReason = VmDebugStopReason::Exit;
      const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
      if (!stopTransition.valid) {
        error = "debug session failed to exit";
        return false;
      }
      state_ = stopTransition.state;
      return true;
    }
    if (pauseRequested_) {
      pauseRequested_ = false;
      stopReason = VmDebugStopReason::Pause;
      const VmDebugTransitionResult stopTransition = vmDebugApplyStopReason(state_, stopReason);
      if (!stopTransition.valid) {
        error = "debug session failed to pause";
        return false;
      }
      state_ = stopTransition.state;
      return true;
    }
  }
}

bool VmDebugSession::pause(std::string &error) {
  if (state_ == VmDebugSessionState::Idle) {
    error = "debug session is not started";
    return false;
  }
  if (state_ == VmDebugSessionState::Faulted || state_ == VmDebugSessionState::Exited) {
    error = "debug session cannot pause after termination";
    return false;
  }
  pauseRequested_ = true;
  return true;
}

VmDebugSnapshot VmDebugSession::snapshot() const {
  VmDebugSnapshot out;
  out.state = state_;
  out.callDepth = frames_.size();
  out.operandStackSize = stack_.size();
  out.result = result_;
  if (!frames_.empty()) {
    out.functionIndex = frames_.back().functionIndex;
    out.instructionPointer = frames_.back().ip;
  }
  return out;
}

} // namespace primec
