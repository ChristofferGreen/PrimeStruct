#include "IrPassUtil.h"

#include <cstdint>
#include <vector>

namespace primec::ir_opt {
namespace {

bool isIntegerComparison(IrOpcode op) {
  switch (op) {
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
    return true;
  default:
    return false;
  }
}

// A local stepped by a constant under an integer comparison:
//
//   Cmp; JumpIfZero L; LoadLocal x; Push c; AddI64|SubI64; StoreLocal x; L:
//
// becomes straight-line code that adds the comparison's 0 or 1 times the step:
//
//   Cmp; PushI64 ±c; MulI64; LoadLocal x; AddI64; StoreLocal x
//
// Only the comparison can reach the JumpIfZero (neither it nor the guarded statement is a
// jump target), so the tested value is exactly 0 or 1. Wrapping 64-bit arithmetic makes the
// two forms equal; a step of one loses its multiply in peephole.
bool convertGuardedSteps(IrFunction &function) {
  const std::vector<bool> targets = jumpTargetMask(function);
  InstructionRewriter rewriter(function);
  const size_t count = function.instructions.size();
  for (size_t i = 1; i + 4 < count; ++i) {
    const IrInstruction &branch = function.instructions[i];
    if (branch.op != IrOpcode::JumpIfZero || branch.imm != i + 5 ||
        !isIntegerComparison(function.instructions[i - 1].op)) {
      continue;
    }
    if (targets[i] || targets[i + 1] || targets[i + 2] || targets[i + 3] || targets[i + 4]) {
      continue;
    }
    const IrInstruction &load = function.instructions[i + 1];
    const IrInstruction &step = function.instructions[i + 2];
    const IrInstruction &arithmetic = function.instructions[i + 3];
    const IrInstruction &store = function.instructions[i + 4];
    if (load.op != IrOpcode::LoadLocal || store.op != IrOpcode::StoreLocal ||
        store.imm != load.imm || (step.op != IrOpcode::PushI32 && step.op != IrOpcode::PushI64) ||
        (arithmetic.op != IrOpcode::AddI64 && arithmetic.op != IrOpcode::SubI64)) {
      continue;
    }
    uint64_t stride =
        step.op == IrOpcode::PushI32
            ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(step.imm)))
            : step.imm;
    if (arithmetic.op == IrOpcode::SubI64) {
      stride = uint64_t{0} - stride;
    }
    rewriter.replace(i, IrOpcode::PushI64, stride);
    rewriter.replace(i + 1, IrOpcode::MulI64, 0);
    rewriter.replace(i + 2, IrOpcode::LoadLocal, load.imm);
    rewriter.replace(i + 3, IrOpcode::AddI64, 0);
    i += 4;
  }
  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

} // namespace

bool runIfConvertPass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    changed |= convertGuardedSteps(function);
  }
  return true;
}

} // namespace primec::ir_opt
