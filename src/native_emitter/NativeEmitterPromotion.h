#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/ir/IrLocalEscape.h"

namespace primec::native_emitter {

// A local kept in a machine register for the whole function instead of its
// frame slot (docs/OptimizingBackendsPlan.md, Phase 3).
struct PromotedLocal {
  uint32_t index = 0;
  uint8_t reg = 0;
};

// Registers the x86_64 emitter's inline templates never touch for the opcodes in
// opcodeKeepsPromotedRegisters: rsi, rdi, r8, r10 and r11. (rbx and r9 are left
// out because LoadStringLength uses them; r12/r13 hold argc/argv, r14 is the
// operand cache, r15 the operand stack pointer.)
inline constexpr uint8_t X64PromotionPool[] = {6, 7, 8, 10, 11};

// True for opcodes whose template uses only rax, rcx, rdx, xmm registers, the
// operand cache and flags. Every other opcode (printing, file and heap
// operations, string table lookups, calls) runs syscalls or helper sequences
// that clobber the pool, so promoted locals are written to their frame slots
// before it and reloaded after.
inline bool opcodeKeepsPromotedRegisters(IrOpcode op) {
  switch (op) {
  case IrOpcode::PushI32:
  case IrOpcode::PushI64:
  case IrOpcode::PushF32:
  case IrOpcode::PushF64:
  case IrOpcode::PushArgc:
  case IrOpcode::LoadLocal:
  case IrOpcode::StoreLocal:
  case IrOpcode::LoadIndirect:
  case IrOpcode::StoreIndirect:
  case IrOpcode::LoadStringByte:
  case IrOpcode::Dup:
  case IrOpcode::Pop:
  case IrOpcode::AddI32:
  case IrOpcode::SubI32:
  case IrOpcode::MulI32:
  case IrOpcode::DivI32:
  case IrOpcode::NegI32:
  case IrOpcode::AddI64:
  case IrOpcode::SubI64:
  case IrOpcode::MulI64:
  case IrOpcode::DivI64:
  case IrOpcode::DivU64:
  case IrOpcode::NegI64:
  case IrOpcode::AddF32:
  case IrOpcode::SubF32:
  case IrOpcode::MulF32:
  case IrOpcode::DivF32:
  case IrOpcode::NegF32:
  case IrOpcode::AddF64:
  case IrOpcode::SubF64:
  case IrOpcode::MulF64:
  case IrOpcode::DivF64:
  case IrOpcode::NegF64:
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
  case IrOpcode::ConvertI32ToF32:
  case IrOpcode::ConvertI32ToF64:
  case IrOpcode::ConvertI64ToF32:
  case IrOpcode::ConvertI64ToF64:
  case IrOpcode::ConvertU64ToF32:
  case IrOpcode::ConvertU64ToF64:
  case IrOpcode::ConvertF32ToI32:
  case IrOpcode::ConvertF32ToI64:
  case IrOpcode::ConvertF32ToU64:
  case IrOpcode::ConvertF64ToI32:
  case IrOpcode::ConvertF64ToI64:
  case IrOpcode::ConvertF64ToU64:
  case IrOpcode::ConvertF32ToF64:
  case IrOpcode::ConvertF64ToF32:
  case IrOpcode::Jump:
  case IrOpcode::JumpIfZero:
  case IrOpcode::ReturnVoid:
  case IrOpcode::ReturnI32:
  case IrOpcode::ReturnI64:
  case IrOpcode::ReturnF32:
  case IrOpcode::ReturnF64:
    return true;
  default:
    return false;
  }
}

// Chooses which locals of `function` live in the registers of `pool`. A local is
// a candidate when no memory access can reach it (see IrLocalEscape.h); the most
// used ones win, with uses inside loops counting ten times per nesting level
// (loops are the backward jumps).
inline std::vector<PromotedLocal>
planPromotedLocals(const IrFunction &function, const uint8_t *pool, size_t poolSize) {
  std::vector<PromotedLocal> plan;
  const IrLocalEscapeInfo escape = analyzeIrLocalEscape(function);
  if (escape.localCount == 0 || poolSize == 0) {
    return plan;
  }
  const size_t count = function.instructions.size();
  // Loop nesting depth per instruction from the backward jumps.
  std::vector<int32_t> delta(count + 1, 0);
  for (size_t i = 0; i < count; ++i) {
    const IrInstruction &instruction = function.instructions[i];
    if ((instruction.op == IrOpcode::Jump || instruction.op == IrOpcode::JumpIfZero) &&
        instruction.imm <= i) {
      delta[static_cast<size_t>(instruction.imm)] += 1;
      delta[i + 1] -= 1;
    }
  }
  std::vector<uint64_t> weight(escape.localCount, 0);
  int32_t depth = 0;
  for (size_t i = 0; i < count; ++i) {
    depth += delta[i];
    const IrInstruction &instruction = function.instructions[i];
    if ((instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal) &&
        instruction.imm < escape.localCount) {
      uint64_t use = 1;
      for (int32_t level = 0; level < std::min<int32_t>(depth, 6); ++level) {
        use *= 10;
      }
      weight[static_cast<size_t>(instruction.imm)] += use;
    }
  }
  for (const uint32_t slot : escape.pinnedSlots) {
    weight[slot] = 0;
  }
  std::vector<uint32_t> order;
  for (uint32_t local = 0; local < escape.localCount; ++local) {
    // A local touched once or twice outside any loop is not worth a register.
    if (weight[local] >= 3) {
      order.push_back(local);
    }
  }
  std::stable_sort(
      order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return weight[a] > weight[b]; });
  for (size_t i = 0; i < order.size() && i < poolSize; ++i) {
    plan.push_back({order[i], pool[i]});
  }
  return plan;
}

} // namespace primec::native_emitter
