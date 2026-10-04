#pragma once

#include <cstddef>
#include <vector>

#include "primec/ir/Ir.h"
#include "primec/ir/IrOptimizer.h"

namespace primec::ir_opt {

inline bool isJump(IrOpcode op) {
  return op == IrOpcode::Jump || op == IrOpcode::JumpIfZero;
}

inline bool isReturn(IrOpcode op) {
  return op == IrOpcode::ReturnVoid || op == IrOpcode::ReturnI32 || op == IrOpcode::ReturnI64 ||
         op == IrOpcode::ReturnF32 || op == IrOpcode::ReturnF64;
}

// Marks every instruction index that some jump targets. The extra last entry
// stands for "the end of the function".
inline std::vector<bool> jumpTargetMask(const IrFunction &function) {
  std::vector<bool> mask(function.instructions.size() + 1, false);
  for (const IrInstruction &instruction : function.instructions) {
    if (isJump(instruction.op) && instruction.imm <= function.instructions.size()) {
      mask[static_cast<size_t>(instruction.imm)] = true;
    }
  }
  return mask;
}

// Collects deletions and replacements against a function's original
// instruction list, then compacts it and retargets jumps. Jump immediates in
// replacement instructions are given in the ORIGINAL index space; apply()
// translates every jump. A jump to an erased instruction lands on the next
// surviving instruction, which is only correct when the erased instructions
// were a no-op; passes guarantee that.
class InstructionRewriter {
public:
  explicit InstructionRewriter(const IrFunction &function)
      : function_(function), erased_(function.instructions.size(), false),
        replaced_(function.instructions.size(), false), replacement_(function.instructions.size()) {
  }

  size_t size() const {
    return erased_.size();
  }
  bool erased(size_t index) const {
    return erased_[index];
  }

  void erase(size_t index) {
    if (!erased_[index]) {
      erased_[index] = true;
      changed_ = true;
    }
  }

  // Replaces the instruction at `index`; its debug id is kept.
  void replace(size_t index, IrOpcode op, uint64_t imm) {
    IrInstruction instruction;
    instruction.op = op;
    instruction.imm = imm;
    instruction.debugId = function_.instructions[index].debugId;
    replacement_[index] = instruction;
    replaced_[index] = true;
    changed_ = true;
  }

  // The instruction as it will be after the pending edits.
  IrInstruction current(size_t index) const {
    return replaced_[index] ? replacement_[index] : function_.instructions[index];
  }

  bool changed() const {
    return changed_;
  }

  void apply(IrFunction &function) const {
    const size_t count = erased_.size();
    std::vector<size_t> newIndex(count + 1, 0);
    size_t next = 0;
    for (size_t i = 0; i < count; ++i) {
      newIndex[i] = next;
      if (!erased_[i]) {
        ++next;
      }
    }
    newIndex[count] = next;
    std::vector<IrInstruction> out;
    out.reserve(next);
    for (size_t i = 0; i < count; ++i) {
      if (erased_[i]) {
        continue;
      }
      IrInstruction instruction = current(i);
      if (isJump(instruction.op) && instruction.imm <= count) {
        instruction.imm = newIndex[static_cast<size_t>(instruction.imm)];
      }
      out.push_back(instruction);
    }
    function.instructions = std::move(out);
  }

private:
  const IrFunction &function_;
  std::vector<bool> erased_;
  std::vector<bool> replaced_;
  std::vector<IrInstruction> replacement_;
  bool changed_ = false;
};

// Pass entry points (one translation unit each).
bool runCfgSimplifyPass(IrModule &module,
                        const IrPassContext &context,
                        bool &changed,
                        std::string &error);
bool runConstFoldPass(IrModule &module,
                      const IrPassContext &context,
                      bool &changed,
                      std::string &error);
bool runPeepholePass(IrModule &module,
                     const IrPassContext &context,
                     bool &changed,
                     std::string &error);
bool runCopyPropPass(IrModule &module,
                     const IrPassContext &context,
                     bool &changed,
                     std::string &error);
bool runDeadStorePass(IrModule &module,
                      const IrPassContext &context,
                      bool &changed,
                      std::string &error);
bool runLoopRotatePass(IrModule &module,
                       const IrPassContext &context,
                       bool &changed,
                       std::string &error);

} // namespace primec::ir_opt
