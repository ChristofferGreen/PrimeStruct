#include "IrPassUtil.h"

#include "primec/ir/IrCfg.h"
#include "primec/ir/IrLocalEscape.h"

#include <cstdint>
#include <vector>

namespace primec::ir_opt {
namespace {

// A StoreLocal into a local that is never read again before it is overwritten
// or the function returns does nothing but pop its value. The store becomes a
// Pop; the peephole pass then removes whatever pure push fed it.
//
// Only locals that cannot be reached through memory are considered (see
// IrLocalEscape.h), and only reachable blocks are analyzed.
bool eliminateDeadStores(IrFunction &function, const IrModule &module) {
  const IrLocalEscapeInfo escape = analyzeIrLocalEscape(function);
  if (escape.localCount == 0 || escape.pinnedSlots.size() == escape.localCount) {
    return false;  // nothing trackable
  }
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError)) {
    return false;  // malformed or inconsistent code: leave it alone
  }

  const size_t locals = escape.localCount;
  const size_t blockCount = cfg.blocks.size();
  std::vector<bool> trackable(locals, true);
  for (const uint32_t slot : escape.pinnedSlots) {
    trackable[slot] = false;
  }

  // use[b]: read before written in b; def[b]: written in b.
  std::vector<std::vector<bool>> use(blockCount, std::vector<bool>(locals, false));
  std::vector<std::vector<bool>> def(blockCount, std::vector<bool>(locals, false));
  for (size_t b = 0; b < blockCount; ++b) {
    if (!cfg.blocks[b].reachable) {
      continue;
    }
    for (size_t i = cfg.blocks[b].start; i < cfg.blocks[b].end; ++i) {
      const IrInstruction &instruction = function.instructions[i];
      const size_t slot = static_cast<size_t>(instruction.imm);
      if (instruction.op == IrOpcode::LoadLocal && slot < locals && !def[b][slot]) {
        use[b][slot] = true;
      } else if (instruction.op == IrOpcode::StoreLocal && slot < locals) {
        def[b][slot] = true;
      }
    }
  }

  // Backward liveness to a fixed point. Blocks are visited in reverse index
  // order, which converges quickly for the mostly forward-flowing code the
  // lowerer produces; the loop repeats until nothing changes.
  std::vector<std::vector<bool>> liveIn(blockCount, std::vector<bool>(locals, false));
  std::vector<std::vector<bool>> liveOut(blockCount, std::vector<bool>(locals, false));
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t b = blockCount; b-- > 0;) {
      if (!cfg.blocks[b].reachable) {
        continue;
      }
      std::vector<bool> out(locals, false);
      for (const size_t successor : cfg.blocks[b].successors) {
        for (size_t s = 0; s < locals; ++s) {
          if (liveIn[successor][s]) {
            out[s] = true;
          }
        }
      }
      std::vector<bool> in(locals, false);
      for (size_t s = 0; s < locals; ++s) {
        in[s] = use[b][s] || (out[s] && !def[b][s]);
      }
      if (out != liveOut[b] || in != liveIn[b]) {
        liveOut[b] = std::move(out);
        liveIn[b] = std::move(in);
        changed = true;
      }
    }
  }

  InstructionRewriter rewriter(function);
  for (size_t b = 0; b < blockCount; ++b) {
    if (!cfg.blocks[b].reachable) {
      continue;
    }
    std::vector<bool> live = liveOut[b];
    for (size_t i = cfg.blocks[b].end; i-- > cfg.blocks[b].start;) {
      const IrInstruction &instruction = function.instructions[i];
      const size_t slot = static_cast<size_t>(instruction.imm);
      if (instruction.op == IrOpcode::StoreLocal && slot < locals) {
        if (trackable[slot] && !live[slot]) {
          rewriter.replace(i, IrOpcode::Pop, 0);
        } else {
          live[slot] = false;
        }
      } else if (instruction.op == IrOpcode::LoadLocal && slot < locals) {
        live[slot] = true;
      }
    }
  }
  if (!rewriter.changed()) {
    return false;
  }
  rewriter.apply(function);
  return true;
}

} // namespace

bool runDeadStorePass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    changed |= eliminateDeadStores(function, module);
  }
  return true;
}

} // namespace primec::ir_opt
