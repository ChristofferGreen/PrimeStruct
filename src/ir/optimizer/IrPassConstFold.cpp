#include "IrPassUtil.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrPureSemantics.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace primec::ir_opt {
namespace {

constexpr size_t NoProducer = std::numeric_limits<size_t>::max();

bool isConstantPush(IrOpcode op) {
  return op == IrOpcode::PushI32 || op == IrOpcode::PushI64 || op == IrOpcode::PushF32 || op == IrOpcode::PushF64;
}

// The 64-bit slot the VM leaves on the stack for a constant push.
uint64_t pushedSlot(const IrInstruction &instruction) {
  if (instruction.op == IrOpcode::PushI32) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(instruction.imm)));
  }
  return instruction.imm;
}

bool isNanF32(uint64_t slot) {
  return std::isnan(std::bit_cast<float>(static_cast<uint32_t>(slot)));
}
bool isNanF64(uint64_t slot) {
  return std::isnan(std::bit_cast<double>(slot));
}

bool fitsInt32(uint64_t slot) {
  return static_cast<int64_t>(slot) == static_cast<int64_t>(static_cast<int32_t>(slot));
}

// The push that reproduces `result`, typed like the opcode's result so wasm's
// typed stack and the native emitter see the same kind of value as before.
// Refuses results that are not safe to bake in: an I32 result outside the
// 32-bit range (backends differ on overflow there) and NaNs (their bit
// pattern depends on the host that computed them).
bool foldedPush(IrOpcode op, uint64_t result, IrOpcode &pushOp, uint64_t &imm) {
  switch (op) {
    case IrOpcode::AddI32:
    case IrOpcode::SubI32:
    case IrOpcode::MulI32:
    case IrOpcode::DivI32:
    case IrOpcode::NegI32:
    case IrOpcode::ConvertF32ToI32:
    case IrOpcode::ConvertF64ToI32:
      if (!fitsInt32(result)) {
        return false;
      }
      pushOp = IrOpcode::PushI32;
      imm = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(result)));
      return true;
    case IrOpcode::AddI64:
    case IrOpcode::SubI64:
    case IrOpcode::MulI64:
    case IrOpcode::DivI64:
    case IrOpcode::DivU64:
    case IrOpcode::NegI64:
    case IrOpcode::ConvertF32ToI64:
    case IrOpcode::ConvertF64ToI64:
    case IrOpcode::ConvertF32ToU64:
    case IrOpcode::ConvertF64ToU64:
      pushOp = IrOpcode::PushI64;
      imm = result;
      return true;
    case IrOpcode::AddF32:
    case IrOpcode::SubF32:
    case IrOpcode::MulF32:
    case IrOpcode::DivF32:
    case IrOpcode::NegF32:
    case IrOpcode::ConvertI32ToF32:
    case IrOpcode::ConvertI64ToF32:
    case IrOpcode::ConvertU64ToF32:
    case IrOpcode::ConvertF64ToF32:
      if (isNanF32(result)) {
        return false;
      }
      pushOp = IrOpcode::PushF32;
      imm = result;
      return true;
    case IrOpcode::AddF64:
    case IrOpcode::SubF64:
    case IrOpcode::MulF64:
    case IrOpcode::DivF64:
    case IrOpcode::NegF64:
    case IrOpcode::ConvertI32ToF64:
    case IrOpcode::ConvertI64ToF64:
    case IrOpcode::ConvertU64ToF64:
    case IrOpcode::ConvertF32ToF64:
      if (isNanF64(result)) {
        return false;
      }
      pushOp = IrOpcode::PushF64;
      imm = result;
      return true;
    default:
      break;
  }
  if (irPureOpcodeArity(op) == 2) {
    // The remaining binary opcodes are the comparisons: a 0/1 boolean.
    pushOp = IrOpcode::PushI32;
    imm = result;
    return true;
  }
  return false;
}

struct Entry {
  bool known = false;
  uint64_t value = 0;
  // The single push instruction that produced a known value.
  size_t producer = NoProducer;
};

bool foldFunction(IrFunction &function, const IrModule &module) {
  const std::vector<bool> targets = jumpTargetMask(function);
  InstructionRewriter rewriter(function);
  // Simulated operand stack for the current straight-line run. Entries below
  // the window (pushed before the run began) are simply not tracked.
  std::vector<Entry> window;

  for (size_t i = 0; i < function.instructions.size(); ++i) {
    if (targets[i]) {
      window.clear();
    }
    const IrInstruction &instruction = function.instructions[i];
    if (isConstantPush(instruction.op)) {
      window.push_back({true, pushedSlot(instruction), i});
      continue;
    }

    const int arity = irPureOpcodeArity(instruction.op);
    if (arity != 0 && window.size() >= static_cast<size_t>(arity)) {
      const Entry &rhsEntry = window[window.size() - 1];
      const Entry &lhsEntry = arity == 2 ? window[window.size() - 2] : rhsEntry;
      if (lhsEntry.known && rhsEntry.known && lhsEntry.producer != NoProducer &&
          rhsEntry.producer != NoProducer) {
        const uint64_t lhs = lhsEntry.value;
        const uint64_t rhs = rhsEntry.value;
        uint64_t result = 0;
        IrOpcode pushOp = IrOpcode::PushI64;
        uint64_t imm = 0;
        if (irPureEvalIsPortable(instruction.op, lhs, rhs) &&
            evalPureOpcode(instruction.op, lhs, rhs, result) == IrPureEval::Ok &&
            foldedPush(instruction.op, result, pushOp, imm)) {
          const size_t rhsProducer = rhsEntry.producer;
          const size_t lhsProducer = lhsEntry.producer;
          rewriter.erase(rhsProducer);
          if (arity == 2) {
            rewriter.erase(lhsProducer);
          }
          rewriter.replace(i, pushOp, imm);
          window.resize(window.size() - static_cast<size_t>(arity));
          window.push_back({true, result, i});
          continue;
        }
      }
    }

    // Anything else: model its stack effect with unknown values, and forget
    // everything at a control transfer.
    IrStackEffect effect;
    if (!computeIrStackEffect(instruction, module, effect)) {
      window.clear();
      continue;
    }
    for (uint32_t pop = 0; pop < effect.pops && !window.empty(); ++pop) {
      window.pop_back();
    }
    // A read without a pop (dup) also consumes the value its producer pushed,
    // so that push can no longer be erased with the folded operation.
    for (uint32_t read = 0; read < effect.readsWithoutPop && read < window.size(); ++read) {
      window[window.size() - 1 - read].producer = NoProducer;
    }
    for (uint32_t push = 0; push < effect.pushes; ++push) {
      window.push_back({});
    }
    if (isJump(instruction.op) || isReturn(instruction.op)) {
      window.clear();
    }
  }

  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

} // namespace

bool runConstFoldPass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    changed |= foldFunction(function, module);
  }
  return true;
}

} // namespace primec::ir_opt
