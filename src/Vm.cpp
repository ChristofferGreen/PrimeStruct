#include "primec/Vm.h"

#include <cstdio>
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
  constexpr uint64_t kSlotBytes = 16;
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  const IrFunction &fn = module.functions[static_cast<size_t>(module.entryIndex)];
  std::vector<uint64_t> stack;
  size_t localCount = 0;
  for (const auto &inst : fn.instructions) {
    if (inst.op == IrOpcode::LoadLocal || inst.op == IrOpcode::StoreLocal ||
        inst.op == IrOpcode::AddressOfLocal) {
      localCount = std::max(localCount, static_cast<size_t>(inst.imm) + 1);
    }
  }
  std::vector<uint64_t> locals(localCount, 0);

  size_t ip = 0;
  while (ip < fn.instructions.size()) {
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
      case IrOpcode::ReturnVoid: {
        result = 0;
        return true;
      }
      case IrOpcode::ReturnI32: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        result = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(stack.back())));
        return true;
      }
      case IrOpcode::ReturnI64: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        result = stack.back();
        return true;
      }
      case IrOpcode::ReturnF32: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        result = static_cast<uint64_t>(static_cast<uint32_t>(stack.back()));
        return true;
      }
      case IrOpcode::ReturnF64: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        result = stack.back();
        return true;
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

} // namespace primec
