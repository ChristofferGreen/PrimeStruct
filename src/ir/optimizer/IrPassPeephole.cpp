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
  return op == IrOpcode::AddI32 || op == IrOpcode::AddI64 || op == IrOpcode::SubI32 || op == IrOpcode::SubI64;
}

bool isMulOrDivU(IrOpcode op) {
  return op == IrOpcode::MulI32 || op == IrOpcode::MulI64 || isDivision(op);
}

// One sweep of adjacent-pair rewrites. `prev` is the nearest surviving earlier
// instruction; a rule may only fire when `cur` is not a join point, because a
// jump into `cur` would skip `prev`.
bool sweep(IrFunction &function) {
  std::vector<bool> targets = jumpTargetMask(function);
  InstructionRewriter rewriter(function);
  size_t prev = function.instructions.size();  // none
  bool carryTarget = false;                     // an erased join point hands its status to the next survivor

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    const bool isTarget = targets[i] || carryTarget;
    carryTarget = false;
    targets[i] = isTarget;  // remember inherited join points for later rules
    const IrInstruction cur = rewriter.current(i);
    if (prev != function.instructions.size() && !isTarget) {
      const IrInstruction p = rewriter.current(prev);
      bool removePair = false;
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
              targets[prev] = true;  // jumps into the dup now land on the store
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
      } else if (isIntegerConstant(p, 0) && isAddOrSub(cur.op)) {
        removePair = true;  // x + 0, x - 0
      } else if (isIntegerConstant(p, 1) && isMulOrDivU(cur.op)) {
        removePair = true;  // x * 1, x / 1
      } else if ((cur.op == IrOpcode::NegI32 || cur.op == IrOpcode::NegI64) && p.op == cur.op) {
        removePair = true;  // -(-x)
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
