#include "primec/Vm.h"

#include <vector>

namespace primec {

bool Vm::execute(const IrModule &module, int32_t &result, std::string &error) const {
  if (module.entryIndex < 0 || static_cast<size_t>(module.entryIndex) >= module.functions.size()) {
    error = "invalid IR entry index";
    return false;
  }
  const IrFunction &fn = module.functions[static_cast<size_t>(module.entryIndex)];
  std::vector<int64_t> stack;

  for (const auto &inst : fn.instructions) {
    switch (inst.op) {
      case IrOpcode::PushI32:
        stack.push_back(static_cast<int64_t>(inst.imm));
        break;
      case IrOpcode::AddI32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on add";
          return false;
        }
        int64_t rhs = stack.back();
        stack.pop_back();
        int64_t lhs = stack.back();
        stack.pop_back();
        stack.push_back(lhs + rhs);
        break;
      }
      case IrOpcode::SubI32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on sub";
          return false;
        }
        int64_t rhs = stack.back();
        stack.pop_back();
        int64_t lhs = stack.back();
        stack.pop_back();
        stack.push_back(lhs - rhs);
        break;
      }
      case IrOpcode::MulI32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on mul";
          return false;
        }
        int64_t rhs = stack.back();
        stack.pop_back();
        int64_t lhs = stack.back();
        stack.pop_back();
        stack.push_back(lhs * rhs);
        break;
      }
      case IrOpcode::DivI32: {
        if (stack.size() < 2) {
          error = "IR stack underflow on div";
          return false;
        }
        int64_t rhs = stack.back();
        stack.pop_back();
        int64_t lhs = stack.back();
        stack.pop_back();
        if (rhs == 0) {
          error = "division by zero in IR";
          return false;
        }
        stack.push_back(lhs / rhs);
        break;
      }
      case IrOpcode::NegI32: {
        if (stack.empty()) {
          error = "IR stack underflow on negate";
          return false;
        }
        int64_t value = stack.back();
        stack.pop_back();
        stack.push_back(-value);
        break;
      }
      case IrOpcode::ReturnI32: {
        if (stack.empty()) {
          error = "IR stack underflow on return";
          return false;
        }
        result = static_cast<int32_t>(stack.back());
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
