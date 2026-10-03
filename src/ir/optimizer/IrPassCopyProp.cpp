#include "IrPassUtil.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrLocalEscape.h"

#include <cstdint>
#include <vector>

namespace primec::ir_opt {
namespace {

// Functions whose block count times local count exceeds this are left alone:
// the analysis keeps one state per block.
constexpr size_t MaxStateCells = 8u * 1024u * 1024u;

constexpr int32_t NotACopy = -1;

// For every local `t`, `copyOf[t]` is the local whose value `t` is known to
// hold (and `NotACopy` when unknown). A mapping is dropped as soon as either
// side is written.
using CopyState = std::vector<int32_t>;

void kill(CopyState &state, size_t local) {
  state[local] = NotACopy;
  for (int32_t &source : state) {
    if (source == static_cast<int32_t>(local)) {
      source = NotACopy;
    }
  }
}

// `load a; store t` copies a into t. Chains collapse to the root so a later
// write to an intermediate local does not invalidate the mapping.
void recordCopy(CopyState &state, size_t target, size_t source) {
  kill(state, target);
  if (source == target) {
    return;
  }
  const int32_t root = state[source] != NotACopy ? state[source] : static_cast<int32_t>(source);
  if (root != static_cast<int32_t>(target)) {
    state[target] = root;
  }
}

// Applies the effect of one instruction. `previous` is the instruction before
// it in the same block (or nullptr), which is how a copy is recognised.
void transfer(CopyState &state, const IrInstruction &instruction, const IrInstruction *previous) {
  const size_t locals = state.size();
  const size_t slot = static_cast<size_t>(instruction.imm);
  if (instruction.op == IrOpcode::StoreLocal && slot < locals) {
    if (previous != nullptr && previous->op == IrOpcode::LoadLocal &&
        static_cast<size_t>(previous->imm) < locals) {
      recordCopy(state, slot, static_cast<size_t>(previous->imm));
    } else {
      kill(state, slot);
    }
  } else if (instruction.op == IrOpcode::FileReadByte && slot < locals) {
    kill(state, slot);
  }
}

// Intersection of two states: a mapping survives only when both agree.
bool meetInto(CopyState &into, const CopyState &other) {
  bool changed = false;
  for (size_t i = 0; i < into.size(); ++i) {
    if (into[i] != NotACopy && into[i] != other[i]) {
      into[i] = NotACopy;
      changed = true;
    }
  }
  return changed;
}

bool propagateCopies(IrFunction &function, const IrModule &module) {
  const IrLocalEscapeInfo escape = analyzeIrLocalEscape(function);
  if (escape.localCount < 2 || !escape.pinnedSlots.empty()) {
    return false; // nothing to relate, or locals reachable through memory
  }
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError)) {
    return false;
  }
  const size_t locals = escape.localCount;
  const size_t blockCount = cfg.blocks.size();
  if (blockCount == 0 || blockCount * locals > MaxStateCells) {
    return false;
  }

  // Forward must-analysis to a fixed point. A block that has not been reached
  // yet has no state; the entry starts with no known copies.
  std::vector<CopyState> entry(blockCount);
  std::vector<bool> seen(blockCount, false);
  entry[0] = CopyState(locals, NotACopy);
  seen[0] = true;
  std::vector<size_t> worklist = {0};
  std::vector<bool> queued(blockCount, false);
  queued[0] = true;
  while (!worklist.empty()) {
    const size_t b = worklist.back();
    worklist.pop_back();
    queued[b] = false;
    CopyState state = entry[b];
    const IrCfgBlock &block = cfg.blocks[b];
    for (size_t i = block.start; i < block.end; ++i) {
      const IrInstruction *previous = i > block.start ? &function.instructions[i - 1] : nullptr;
      transfer(state, function.instructions[i], previous);
    }
    for (const size_t successor : block.successors) {
      if (!cfg.blocks[successor].reachable) {
        continue;
      }
      bool changed = false;
      if (!seen[successor]) {
        entry[successor] = state;
        seen[successor] = true;
        changed = true;
      } else {
        changed = meetInto(entry[successor], state);
      }
      if (changed && !queued[successor]) {
        queued[successor] = true;
        worklist.push_back(successor);
      }
    }
  }

  // Rewrite loads of copies into loads of the original.
  InstructionRewriter rewriter(function);
  for (size_t b = 0; b < blockCount; ++b) {
    if (!cfg.blocks[b].reachable || !seen[b]) {
      continue;
    }
    CopyState state = entry[b];
    const IrCfgBlock &block = cfg.blocks[b];
    for (size_t i = block.start; i < block.end; ++i) {
      const IrInstruction &instruction = function.instructions[i];
      const IrInstruction *previous = i > block.start ? &function.instructions[i - 1] : nullptr;
      if (instruction.op == IrOpcode::LoadLocal && static_cast<size_t>(instruction.imm) < locals) {
        const int32_t source = state[static_cast<size_t>(instruction.imm)];
        if (source != NotACopy) {
          rewriter.replace(i, IrOpcode::LoadLocal, static_cast<uint64_t>(source));
        }
      }
      transfer(state, instruction, previous);
    }
  }
  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

} // namespace

bool runCopyPropPass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    changed |= propagateCopies(function, module);
  }
  return true;
}

} // namespace primec::ir_opt
