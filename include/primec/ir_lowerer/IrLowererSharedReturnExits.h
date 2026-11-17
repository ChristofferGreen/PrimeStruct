#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "primec/ir/Ir.h"

namespace primec::ir_lowerer {

// The scope-exit path that every early `return` of one function body shares (docs/spec/vm-design.md, Shared exit
// paths). A `return` that leaves owning locals alive does not expand their destruction: it stores its value, jumps to
// a cleanup chain and the chain destroys the live scope entries last declared first, then returns the value. Exits
// that have the same live entries up to a point share the chain from there on, so one body holds each destruction
// once per distinct live set, not once per `return`. A scope entry is a file-handle local (>= 0) or a drop entry
// (< 0, see LowerSetupStageState::DropEntry); the drop flags decide at run time which owning locals are destroyed.
class SharedReturnExits {
public:
  // Destroys one scope entry, as the scope's own exit does.
  using EmitEntryCleanupFn = std::function<bool(int32_t scopeEntry)>;

  // Starts a body. A body shares its exits only when `enabled` (a body with one `return` keeps the inline cleanup).
  void begin(bool enabled);
  bool active() const {
    return enabled_;
  }

  // Whether an exit with these live entries shares the cleanup chain: it holds at least one drop entry.
  bool shouldShare(const std::vector<int32_t> &liveEntries) const;

  // Emits the store of the value (none for ReturnVoid) and the jump of an exit; the value is on the operand stack.
  void emitExit(std::vector<IrInstruction> &instructions,
                IrOpcode returnOp,
                const std::vector<int32_t> &liveEntries,
                const std::function<int32_t()> &allocLocal);

  // Whether the last instruction is the jump of the last recorded exit, so that no cleanup follows it.
  bool endsWithSharedExit(size_t instructionCount) const;

  // Emits the cleanup chains after the body and points the exits at them.
  bool finish(std::vector<IrInstruction> &instructions, const EmitEntryCleanupFn &emitEntryCleanup);

private:
  struct Exit {
    size_t jumpIndex = 0;
    size_t node = 0;
  };
  struct Node {
    IrOpcode returnOp = IrOpcode::ReturnVoid;
    size_t parent = 0;
    int32_t entry = 0;
    bool isRoot = true;
    int32_t returnLocal = -1;
  };

  bool enabled_ = false;
  std::vector<Exit> exits_;
  std::vector<Node> nodes_;
  std::map<std::pair<size_t, int32_t>, size_t> childByEntry_;
  std::map<IrOpcode, size_t> rootByOp_;
};

// Emits the exit of a `return` that shares its cleanup, or returns false and emits nothing. The value is on the
// operand stack, or in the local `valueLocal` when that is not negative.
using SharedReturnExitFn = std::function<bool(IrOpcode, int32_t valueLocal)>;

// What a body lowering needs of the shared exit path: `begin` (with whether the body shares), whether the last
// instruction is a shared exit, and `finish`. All empty: the body keeps its inline cleanups.
struct SharedReturnExitHooks {
  std::function<void(bool)> begin;
  std::function<bool()> endsWithExit;
  std::function<bool()> finish;
};

} // namespace primec::ir_lowerer
