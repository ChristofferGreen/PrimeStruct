#include "primec/Vm.h"

#include <vector>

namespace primec {

bool Vm::execute(const IrModule &module, uint64_t &result, std::string &error) const {
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
      default:
        error = "unknown IR opcode";
        return false;
    }
  }
  error = "missing return in IR";
  return false;
}

} // namespace primec
