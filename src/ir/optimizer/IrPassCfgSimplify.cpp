#include "IrPassUtil.h"

#include <cstdint>
#include <vector>

namespace primec::ir_opt {
namespace {

bool isConstantPush(const IrInstruction &instruction) {
  return instruction.op == IrOpcode::PushI32 || instruction.op == IrOpcode::PushI64;
}

// Value a JumpIfZero would test, for a constant integer push.
bool constantIsZero(const IrInstruction &push) {
  if (push.op == IrOpcode::PushI32) {
    return static_cast<int32_t>(push.imm) == 0;
  }
  return push.imm == 0;
}

// `Push c; JumpIfZero L` has a known outcome: either an unconditional jump or
// no jump at all. The jump must not be a join point, or other paths would have
// pushed their own condition.
bool foldConstantBranches(IrFunction &function) {
  const std::vector<bool> targets = jumpTargetMask(function);
  InstructionRewriter rewriter(function);
  for (size_t i = 0; i + 1 < function.instructions.size(); ++i) {
    const IrInstruction &push = function.instructions[i];
    const IrInstruction &branch = function.instructions[i + 1];
    if (!isConstantPush(push) || branch.op != IrOpcode::JumpIfZero || targets[i + 1] || rewriter.erased(i)) {
      continue;
    }
    rewriter.erase(i);
    if (constantIsZero(push)) {
      rewriter.replace(i + 1, IrOpcode::Jump, branch.imm);
    } else {
      rewriter.erase(i + 1);
    }
  }
  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

// A jump whose target is another unconditional jump goes to the final target.
bool threadJumps(IrFunction &function) {
  bool changed = false;
  const size_t count = function.instructions.size();
  for (size_t i = 0; i < count; ++i) {
    IrInstruction &instruction = function.instructions[i];
    if (!isJump(instruction.op) || instruction.imm >= count) {
      continue;
    }
    uint64_t target = instruction.imm;
    // The step bound guards against a cycle of jumps.
    for (size_t step = 0; step < count && target < count; ++step) {
      const IrInstruction &next = function.instructions[static_cast<size_t>(target)];
      if (next.op != IrOpcode::Jump || next.imm == target) {
        break;
      }
      target = next.imm;
    }
    if (target != instruction.imm) {
      instruction.imm = target;
      changed = true;
    }
  }
  return changed;
}

// A jump to the next instruction does nothing; a conditional one still pops its
// condition.
bool removeJumpsToNext(IrFunction &function) {
  InstructionRewriter rewriter(function);
  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const IrInstruction &instruction = function.instructions[i];
    if (!isJump(instruction.op) || instruction.imm != i + 1) {
      continue;
    }
    if (instruction.op == IrOpcode::Jump) {
      rewriter.erase(i);
    } else {
      rewriter.replace(i, IrOpcode::Pop, 0);
    }
  }
  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

// Deletes instructions no path from the entry reaches. Reachability follows
// control flow only; the stack is not modeled.
bool removeUnreachable(IrFunction &function) {
  const size_t count = function.instructions.size();
  if (count == 0) {
    return false;
  }
  std::vector<bool> reached(count, false);
  std::vector<size_t> worklist = {0};
  while (!worklist.empty()) {
    const size_t index = worklist.back();
    worklist.pop_back();
    if (index >= count || reached[index]) {
      continue;
    }
    reached[index] = true;
    const IrInstruction &instruction = function.instructions[index];
    if (isReturn(instruction.op)) {
      continue;
    }
    if (isJump(instruction.op)) {
      worklist.push_back(static_cast<size_t>(instruction.imm));
      if (instruction.op == IrOpcode::Jump) {
        continue;
      }
    }
    worklist.push_back(index + 1);
  }
  InstructionRewriter rewriter(function);
  for (size_t i = 0; i < count; ++i) {
    if (!reached[i]) {
      rewriter.erase(i);
    }
  }
  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

} // namespace

bool runCfgSimplifyPass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    // Each step can expose work for the others; the round bound keeps the
    // pass linear in practice and guarantees termination.
    for (int round = 0; round < 8; ++round) {
      bool roundChanged = false;
      roundChanged |= foldConstantBranches(function);
      roundChanged |= threadJumps(function);
      roundChanged |= removeJumpsToNext(function);
      roundChanged |= removeUnreachable(function);
      if (!roundChanged) {
        break;
      }
      changed = true;
    }
  }
  return true;
}

} // namespace primec::ir_opt
