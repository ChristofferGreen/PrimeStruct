#include "IrPassUtil.h"

#include "primec/ir/IrPureSemantics.h"

#include <cstdint>
#include <vector>

namespace primec::ir_opt {
namespace {

// Instructions that only push a value and have no other effect.
bool isPurePush(IrOpcode op) {
  switch (op) {
  case IrOpcode::PushI32:
  case IrOpcode::PushI64:
  case IrOpcode::PushF32:
  case IrOpcode::PushF64:
  case IrOpcode::PushArgc:
  case IrOpcode::LoadLocal:
  case IrOpcode::AddressOfLocal:
    return true;
  default:
    return false;
  }
}

// Comparisons produce exactly 0 or 1.
bool isComparison(IrOpcode op) {
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
    return true;
  default:
    return false;
  }
}

bool isDivision(IrOpcode op) {
  return op == IrOpcode::DivI32 || op == IrOpcode::DivI64 || op == IrOpcode::DivU64;
}

bool isIntegerConstant(const IrInstruction &instruction, int64_t value) {
  if (instruction.op == IrOpcode::PushI32) {
    return static_cast<int32_t>(instruction.imm) == value;
  }
  return instruction.op == IrOpcode::PushI64 && static_cast<int64_t>(instruction.imm) == value;
}

bool isAddOrSub(IrOpcode op) {
  return op == IrOpcode::AddI32 || op == IrOpcode::AddI64 || op == IrOpcode::SubI32 ||
         op == IrOpcode::SubI64;
}

bool isMulOrDivU(IrOpcode op) {
  return op == IrOpcode::MulI32 || op == IrOpcode::MulI64 || isDivision(op);
}

// True when the opcode leaves a value that is already a sign-extended 32-bit
// integer, so a following SextI32 changes nothing.
bool producesCanonicalI32(IrOpcode op) {
  switch (op) {
  case IrOpcode::PushI32:
  case IrOpcode::SextI32:
  case IrOpcode::ConvertF32ToI32:
  case IrOpcode::ConvertF64ToI32:
  case IrOpcode::LoadStringByte:
  case IrOpcode::LoadStringByteDynamic:
    return true;
  default:
    return isComparison(op);
  }
}

// One sweep of adjacent-pair rewrites. `prev` is the nearest surviving earlier
// instruction; a rule may only fire when `cur` is not a join point, because a
// jump into `cur` would skip `prev`.
bool sweep(IrFunction &function) {
  std::vector<bool> targets = jumpTargetMask(function);
  InstructionRewriter rewriter(function);
  size_t prev = function.instructions.size(); // none
  bool carryTarget = false; // an erased join point hands its status to the next survivor

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const bool isTarget = targets[i] || carryTarget;
    carryTarget = false;
    targets[i] = isTarget; // remember inherited join points for later rules
    const IrInstruction cur = rewriter.current(i);
    if (prev != function.instructions.size() && !isTarget) {
      const IrInstruction p = rewriter.current(prev);
      bool removePair = false;
      if (cur.op == IrOpcode::SextI32 && producesCanonicalI32(p.op)) {
        rewriter.erase(i); // the operand is already canonical
        continue;
      }
      if (cur.op == IrOpcode::Pop) {
        if (p.op == IrOpcode::StoreLocal && !targets[prev]) {
          // `dup; store x; pop` is how an assignment statement discards its
          // value; it means `store x`. The dup may be a join point (entering
          // there still leaves the stack and the local the same way), but the
          // store and pop may not.
          size_t before = prev;
          while (before > 0 && rewriter.erased(before - 1)) {
            --before;
          }
          if (before > 0 && rewriter.current(before - 1).op == IrOpcode::Dup) {
            const size_t dup = before - 1;
            const bool dupWasTarget = targets[dup];
            rewriter.erase(dup);
            rewriter.erase(i);
            if (dupWasTarget) {
              targets[prev] = true; // jumps into the dup now land on the store
            }
            continue;
          }
        }
        if (isPurePush(p.op) || p.op == IrOpcode::Dup) {
          // The pushed value is discarded at once.
          removePair = true;
        } else if (isIrPureOpcode(p.op) && !isDivision(p.op)) {
          const int arity = irPureOpcodeArity(p.op);
          if (arity == 1) {
            // `x; neg; pop` -> `x; pop`.
            rewriter.erase(prev);
            prev = i;
            continue;
          }
          // `a; b; op; pop` -> `a; b; pop; pop`: keep the result unused.
          rewriter.replace(prev, IrOpcode::Pop, 0);
          prev = i;
          continue;
        }
      } else if (isIntegerConstant(p, 0) &&
                 (cur.op == IrOpcode::CmpNeI32 || cur.op == IrOpcode::CmpNeI64) && !targets[prev]) {
        // `cmp; push 0; ne` re-tests a boolean: a comparison already yields 0 or
        // 1, so `x != 0` is `x`. (The lowering emits this for `a && b`.)
        size_t before = prev;
        while (before > 0 && rewriter.erased(before - 1)) {
          --before;
        }
        if (before > 0 && isComparison(rewriter.current(before - 1).op)) {
          removePair = true;
        }
      } else if (isIntegerConstant(p, 0) && isAddOrSub(cur.op)) {
        removePair = true; // x + 0, x - 0
      } else if (isIntegerConstant(p, 1) && isMulOrDivU(cur.op)) {
        removePair = true; // x * 1, x / 1
      } else if ((cur.op == IrOpcode::NegI32 || cur.op == IrOpcode::NegI64) && p.op == cur.op) {
        removePair = true; // -(-x)
      }
      if (removePair) {
        const bool prevWasTarget = targets[prev];
        rewriter.erase(prev);
        rewriter.erase(i);
        // Locate the new `prev`: the nearest surviving instruction before it.
        size_t back = prev;
        prev = function.instructions.size();
        while (back > 0) {
          --back;
          if (!rewriter.erased(back)) {
            prev = back;
            break;
          }
        }
        if (prevWasTarget) {
          // A jump into the erased pair now lands on whatever follows; that
          // instruction is a join point and must not pair with `prev`.
          carryTarget = true;
        }
        continue;
      }
    }
    prev = i;
  }

  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

} // namespace

bool runPeepholePass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    for (int round = 0; round < 16; ++round) {
      if (!sweep(function)) {
        break;
      }
      changed = true;
    }
  }
  return true;
}

} // namespace primec::ir_opt
