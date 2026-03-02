#include "primec/IrInliner.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <vector>

namespace primec {
namespace {

bool isReturnOpcode(IrOpcode opcode) {
  return opcode == IrOpcode::ReturnVoid || opcode == IrOpcode::ReturnI32 || opcode == IrOpcode::ReturnI64 ||
         opcode == IrOpcode::ReturnF32 || opcode == IrOpcode::ReturnF64;
}

bool hasCallOpcode(const IrFunction &function) {
  for (const auto &instruction : function.instructions) {
    if (instruction.op == IrOpcode::Call || instruction.op == IrOpcode::CallVoid) {
      return true;
    }
  }
  return false;
}

bool hasControlFlowOpcode(const IrFunction &function) {
  for (const auto &instruction : function.instructions) {
    if (instruction.op == IrOpcode::Jump || instruction.op == IrOpcode::JumpIfZero) {
      return true;
    }
  }
  return false;
}

uint64_t computeLocalCount(const IrFunction &function) {
  uint64_t count = 0;
  for (const auto &instruction : function.instructions) {
    if (instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal ||
        instruction.op == IrOpcode::AddressOfLocal) {
      count = std::max(count, instruction.imm + 1);
    }
  }
  return count;
}

bool appendInlinedCallee(const IrFunction &callee,
                         uint64_t localBase,
                         bool pushReturnValue,
                         std::vector<IrInstruction> &out,
                         std::string &error) {
  if (callee.instructions.empty()) {
    error = "IR inlining requires callee instructions";
    return false;
  }
  if (!isReturnOpcode(callee.instructions.back().op)) {
    error = "IR inlining requires callee to end with return";
    return false;
  }

  for (const auto &instruction : callee.instructions) {
    if (instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal ||
        instruction.op == IrOpcode::AddressOfLocal) {
      if (instruction.imm > (std::numeric_limits<uint64_t>::max() - localBase)) {
        error = "IR inlining local index overflow";
        return false;
      }
      out.push_back({instruction.op, instruction.imm + localBase});
      continue;
    }
    if (instruction.op == IrOpcode::ReturnVoid) {
      if (pushReturnValue) {
        out.push_back({IrOpcode::PushI32, 0});
      }
      continue;
    }
    if (instruction.op == IrOpcode::ReturnI32 || instruction.op == IrOpcode::ReturnI64 ||
        instruction.op == IrOpcode::ReturnF32 || instruction.op == IrOpcode::ReturnF64) {
      if (!pushReturnValue) {
        out.push_back({IrOpcode::Pop, 0});
      }
      continue;
    }
    out.push_back(instruction);
  }
  return true;
}

} // namespace

bool inlineIrModuleCalls(IrModule &module, std::string &error) {
  error.clear();
  if (module.functions.empty()) {
    return true;
  }

  const size_t functionCount = module.functions.size();
  std::vector<uint64_t> localCounts(functionCount, 0);
  std::vector<bool> leafInlineable(functionCount, false);
  for (size_t index = 0; index < functionCount; ++index) {
    const auto &function = module.functions[index];
    localCounts[index] = computeLocalCount(function);
    leafInlineable[index] =
        !hasCallOpcode(function) && !hasControlFlowOpcode(function) && !function.instructions.empty() &&
        isReturnOpcode(function.instructions.back().op);
  }

  const size_t maxPasses = functionCount * 2 + 1;
  for (size_t pass = 0; pass < maxPasses; ++pass) {
    bool changedInPass = false;

    for (size_t functionIndex = 0; functionIndex < functionCount; ++functionIndex) {
      IrFunction &function = module.functions[functionIndex];
      std::vector<IrInstruction> rewritten;
      rewritten.reserve(function.instructions.size());
      uint64_t nextLocal = localCounts[functionIndex];
      bool changedFunction = false;

      for (const auto &instruction : function.instructions) {
        if (instruction.op != IrOpcode::Call && instruction.op != IrOpcode::CallVoid) {
          rewritten.push_back(instruction);
          continue;
        }
        if (instruction.imm >= functionCount) {
          std::ostringstream out;
          out << "IR inlining encountered invalid call target " << instruction.imm << " in "
              << module.functions[functionIndex].name;
          error = out.str();
          return false;
        }

        const size_t calleeIndex = static_cast<size_t>(instruction.imm);
        if (calleeIndex == functionIndex || !leafInlineable[calleeIndex]) {
          rewritten.push_back(instruction);
          continue;
        }

        const bool pushReturnValue = instruction.op == IrOpcode::Call;
        if (!appendInlinedCallee(module.functions[calleeIndex], nextLocal, pushReturnValue, rewritten, error)) {
          std::ostringstream out;
          out << "IR inlining failed for " << module.functions[calleeIndex].name << ": " << error;
          error = out.str();
          return false;
        }
        nextLocal += localCounts[calleeIndex];
        changedFunction = true;
      }

      if (changedFunction) {
        function.instructions = std::move(rewritten);
        localCounts[functionIndex] = nextLocal;
        leafInlineable[functionIndex] =
            !hasCallOpcode(function) && !hasControlFlowOpcode(function) && !function.instructions.empty() &&
            isReturnOpcode(function.instructions.back().op);
        changedInPass = true;
      }
    }

    if (!changedInPass) {
      break;
    }
  }

  return true;
}

} // namespace primec
