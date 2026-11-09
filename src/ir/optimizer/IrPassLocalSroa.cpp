#include "IrPassUtil.h"

#include "primec/ir/IrCfg.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace primec::ir_opt {
namespace {

// local-sroa: a function whose local addresses never leave it reads and writes
// those locals directly.
//
// AddressOfLocal pushes a frame address (the slot index times IrSlotBytes).
// When every value derived from one (copied through locals with
// LoadLocal/StoreLocal, moved by a constant with AddI*/SubI*) is only ever the
// address operand of a LoadIndirect or StoreIndirect, or dropped by a Pop, and
// each such address names one known, aligned slot of this frame on every path,
// the indirect accesses become LoadLocal/StoreLocal of that slot and the
// address computations disappear. With no AddressOfLocal left, IrLocalEscape
// no longer pins the function's slots, so copy-prop, dead-store and the native
// register allocators treat them as ordinary locals.
//
// An indirect access through an address built only from pushed constants (a
// null reference plus a field offset) is left to memory too: the VM resolves
// it to a slot of the entry frame, so the function keeps all its slots. Other
// addresses are taken not to name this frame, as IrLocalEscape does for every
// function without AddressOfLocal.
//
// It is all or nothing per function: when one derived value reaches anything
// else (a call, a store as data, a comparison, a return, a merge with a
// different value, an address outside the frame), the function is left as it
// is. Exactness: the rewritten accesses touch the very slot the address named,
// and an in-range, aligned frame address cannot fault, so results and fault
// order are unchanged.

// Functions whose block count times tracked value count exceeds this are left
// alone: the analysis keeps one state per block.
constexpr size_t MaxStateCells = 8u * 1024u * 1024u;

enum class Kind : uint8_t {
  Unknown,  // anything that is not a frame address of this function
  Address,  // the frame byte offset `offset`
  Mixed,    // an address on some path and something else on another
  Constant, // the integer `offset`, built from pushed constants (not an address)
};

struct Value {
  Kind kind = Kind::Unknown;
  int64_t offset = 0;

  bool operator==(const Value &other) const {
    return kind == other.kind &&
           ((kind != Kind::Address && kind != Kind::Constant) || offset == other.offset);
  }
};

Value meet(const Value &a, const Value &b) {
  if (a == b) {
    return a;
  }
  if (a.kind == Kind::Address || b.kind == Kind::Address || a.kind == Kind::Mixed ||
      b.kind == Kind::Mixed) {
    return Value{Kind::Mixed, 0};
  }
  return Value{}; // two different integers, or an integer and anything else
}

struct State {
  std::vector<Value> locals;
  std::vector<Value> stack;
};

bool isConstPush(IrOpcode op) {
  return op == IrOpcode::PushI32 || op == IrOpcode::PushI64;
}

int64_t constValue(const IrInstruction &instruction) {
  if (instruction.op == IrOpcode::PushI32) {
    return static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(instruction.imm)));
  }
  return static_cast<int64_t>(instruction.imm);
}

bool isOffsetArithmetic(IrOpcode op) {
  return op == IrOpcode::AddI64 || op == IrOpcode::AddI32 || op == IrOpcode::SubI64 ||
         op == IrOpcode::SubI32;
}

// What happens to one instruction when the function is rewritten.
enum class Action : uint8_t {
  Keep,
  Erase,
  ToLoadLocal,  // LoadIndirect of a known slot
  ToStoreLocal, // StoreIndirect of a known slot whose result the next Pop drops
  ToDupStore,   // StoreIndirect of a known slot whose result stays: Dup; StoreLocal
};

class Analyzer {
public:
  Analyzer(const IrFunction &function, const IrModule &module, const IrCfg &cfg, size_t frameSlots)
      : function_(function), module_(module), cfg_(cfg), frameSlots_(frameSlots),
        targets_(jumpTargetMask(function)), actions_(function.instructions.size(), Action::Keep),
        slots_(function.instructions.size(), 0) {
  }

  // Runs the analysis to a fixed point and then decides every instruction.
  // Returns false when the function must be left alone.
  bool run() {
    const size_t blockCount = cfg_.blocks.size();
    entry_.assign(blockCount, State{});
    seen_.assign(blockCount, false);
    State start;
    start.locals.assign(frameSlots_, Value{});
    start.stack.assign(function_.parameterCount, Value{});
    entry_[0] = std::move(start);
    seen_[0] = true;
    std::vector<size_t> worklist = {0};
    std::vector<bool> queued(blockCount, false);
    queued[0] = true;
    while (!worklist.empty()) {
      const size_t b = worklist.back();
      worklist.pop_back();
      queued[b] = false;
      State state = entry_[b];
      if (!runBlock(b, state, false)) {
        return false;
      }
      for (const size_t successor : cfg_.blocks[b].successors) {
        if (!cfg_.blocks[successor].reachable) {
          continue;
        }
        bool changed = false;
        if (!seen_[successor]) {
          entry_[successor] = state;
          seen_[successor] = true;
          changed = true;
        } else {
          State &into = entry_[successor];
          if (into.stack.size() != state.stack.size()) {
            return false; // IrCfg rejects this; be safe anyway
          }
          for (size_t i = 0; i < into.locals.size(); ++i) {
            const Value merged = meet(into.locals[i], state.locals[i]);
            if (!(merged == into.locals[i])) {
              into.locals[i] = merged;
              changed = true;
            }
          }
          for (size_t i = 0; i < into.stack.size(); ++i) {
            const Value merged = meet(into.stack[i], state.stack[i]);
            if (!(merged == into.stack[i])) {
              into.stack[i] = merged;
              changed = true;
            }
          }
        }
        if (changed && !queued[successor]) {
          queued[successor] = true;
          worklist.push_back(successor);
        }
      }
    }
    for (size_t b = 0; b < blockCount; ++b) {
      if (!cfg_.blocks[b].reachable || !seen_[b]) {
        continue;
      }
      State state = entry_[b];
      // A value that is an address on one path and not on another cannot be
      // removed from only one of them.
      for (const Value &value : state.stack) {
        if (value.kind == Kind::Mixed) {
          return false;
        }
      }
      if (!runBlock(b, state, true)) {
        return false;
      }
    }
    return true;
  }

  bool rewrites() const {
    return std::any_of(
        actions_.begin(), actions_.end(), [](Action a) { return a != Action::Keep; });
  }

  Action action(size_t index) const {
    return actions_[index];
  }
  uint64_t slot(size_t index) const {
    return slots_[index];
  }

private:
  static bool isAddress(const Value &value) {
    return value.kind == Kind::Address;
  }
  static bool isTracked(const Value &value) {
    return value.kind == Kind::Address || value.kind == Kind::Mixed;
  }
  // The VM resolves an address it can name as a frame slot even when the
  // program built it from an integer (a null reference plus a field offset
  // reads slot 1 of the entry frame). Such an access may read or write any
  // slot, so a function that makes one keeps its slots in memory.
  static bool isForged(const Value &value) {
    return value.kind == Kind::Constant;
  }

  // The slot an address names, or false when it is unaligned or outside the frame.
  bool slotOf(const Value &value, uint64_t &slot) const {
    if (!isAddress(value) || value.offset < 0 ||
        value.offset % static_cast<int64_t>(IrSlotBytes) != 0) {
      return false;
    }
    const uint64_t index = static_cast<uint64_t>(value.offset) / IrSlotBytes;
    if (index >= frameSlots_) {
      return false;
    }
    slot = index;
    return true;
  }

  // Interprets one block. With `decide` set, records the action of every
  // instruction (the states are final then). Returns false when a tracked
  // value is used in a way the rewrite cannot express.
  bool runBlock(size_t b, State &state, bool decide) {
    const IrCfgBlock &block = cfg_.blocks[b];
    auto &stack = state.stack;
    auto &locals = state.locals;
    for (size_t i = block.start; i < block.end; ++i) {
      const IrInstruction &instruction = function_.instructions[i];
      const auto mark = [&](Action a, uint64_t slot = 0) {
        if (decide) {
          actions_[i] = a;
          slots_[i] = slot;
        }
      };
      switch (instruction.op) {
      case IrOpcode::AddressOfLocal: {
        if (instruction.imm >= frameSlots_) {
          return false;
        }
        stack.push_back(Value{Kind::Address, static_cast<int64_t>(instruction.imm * IrSlotBytes)});
        mark(Action::Erase);
        continue;
      }
      case IrOpcode::LoadLocal: {
        const Value value =
            instruction.imm < frameSlots_ ? locals[static_cast<size_t>(instruction.imm)] : Value{};
        if (value.kind == Kind::Mixed) {
          return false;
        }
        stack.push_back(value);
        if (isAddress(value)) {
          mark(Action::Erase);
        }
        continue;
      }
      case IrOpcode::StoreLocal: {
        if (stack.empty()) {
          return false;
        }
        const Value value = stack.back();
        stack.pop_back();
        if (value.kind == Kind::Mixed) {
          return false;
        }
        if (instruction.imm < frameSlots_) {
          locals[static_cast<size_t>(instruction.imm)] = value;
        } else if (isAddress(value)) {
          return false;
        }
        if (isAddress(value)) {
          mark(Action::Erase);
        }
        continue;
      }
      case IrOpcode::FileReadByte:
        if (instruction.imm < frameSlots_) {
          locals[static_cast<size_t>(instruction.imm)] = Value{};
        }
        break; // stack effect below
      case IrOpcode::Pop: {
        if (stack.empty()) {
          return false;
        }
        const Value value = stack.back();
        stack.pop_back();
        if (value.kind == Kind::Mixed) {
          return false;
        }
        if (isAddress(value)) {
          mark(Action::Erase);
        }
        continue;
      }
      case IrOpcode::LoadIndirect: {
        if (stack.empty()) {
          return false;
        }
        const Value address = stack.back();
        stack.pop_back();
        if (isForged(address) && decide) {
          return false; // judged on the final states, where loops may widen it
        }
        if (isTracked(address)) {
          uint64_t slot = 0;
          if (!slotOf(address, slot)) {
            return false;
          }
          mark(Action::ToLoadLocal, slot);
          stack.push_back(locals[static_cast<size_t>(slot)]);
          if (isTracked(stack.back())) {
            // An address stored in memory and read back: keep it simple.
            return false;
          }
        } else {
          stack.push_back(Value{});
        }
        continue;
      }
      case IrOpcode::StoreIndirect: {
        if (stack.size() < 2) {
          return false;
        }
        const Value value = stack.back();
        const Value address = stack[stack.size() - 2];
        stack.pop_back();
        stack.pop_back();
        if (isTracked(value)) {
          return false; // an address stored as data leaves through memory
        }
        if (isForged(address) && decide) {
          return false; // judged on the final states, where loops may widen it
        }
        if (isTracked(address)) {
          uint64_t slot = 0;
          if (!slotOf(address, slot)) {
            return false;
          }
          locals[static_cast<size_t>(slot)] = value;
          const bool popped = i + 1 < block.end &&
                              function_.instructions[i + 1].op == IrOpcode::Pop && !targets_[i + 1];
          mark(popped ? Action::ToStoreLocal : Action::ToDupStore, slot);
        } else {
          // A store through any other address is taken not to reach this
          // frame, the rule IrLocalEscape applies to every function without
          // AddressOfLocal; addresses built from constants are rejected above.
        }
        stack.push_back(Value{});
        continue;
      }
      default:
        break;
      }

      if (isOffsetArithmetic(instruction.op) && stack.size() >= 2 &&
          isTracked(stack[stack.size() - 2])) {
        const Value base = stack[stack.size() - 2];
        const Value &top = stack.back();
        if (base.kind == Kind::Mixed || isTracked(top) || i == block.start ||
            !isConstPush(function_.instructions[i - 1].op) || targets_[i]) {
          return false;
        }
        int64_t delta = constValue(function_.instructions[i - 1]);
        if (instruction.op == IrOpcode::SubI64 || instruction.op == IrOpcode::SubI32) {
          delta = -delta;
        }
        stack.pop_back();
        stack.pop_back();
        stack.push_back(Value{Kind::Address, base.offset + delta});
        if (decide) {
          actions_[i - 1] = Action::Erase;
        }
        mark(Action::Erase);
        continue;
      }

      if (isOffsetArithmetic(instruction.op) && stack.size() >= 2 &&
          isForged(stack[stack.size() - 2]) && isForged(stack.back())) {
        const int64_t right = stack.back().offset;
        stack.pop_back();
        const bool add = instruction.op == IrOpcode::AddI64 || instruction.op == IrOpcode::AddI32;
        stack.back().offset = add ? stack.back().offset + right : stack.back().offset - right;
        continue;
      }
      if ((instruction.op == IrOpcode::LoadU8 && !stack.empty() && isForged(stack.back())) ||
          (instruction.op == IrOpcode::StoreU8 && stack.size() >= 2 &&
           isForged(stack[stack.size() - 2]))) {
        if (decide) {
          return false;
        }
      }

      IrStackEffect effect;
      if (!computeIrStackEffect(instruction, module_, effect)) {
        return false;
      }
      if (stack.size() < effect.pops || stack.size() < effect.readsWithoutPop) {
        return false;
      }
      // Any other use of an address (a call, a comparison, a print, a return,
      // arithmetic with a non-constant, Dup) leaves the function alone.
      const size_t inspected = std::max<size_t>(effect.pops, effect.readsWithoutPop);
      for (size_t k = 0; k < inspected; ++k) {
        if (isTracked(stack[stack.size() - 1 - k])) {
          return false;
        }
      }
      if (isReturn(instruction.op)) {
        for (const Value &value : stack) {
          if (isTracked(value)) {
            return false;
          }
        }
      }
      stack.resize(stack.size() - effect.pops);
      for (uint32_t k = 0; k < effect.pushes; ++k) {
        stack.push_back(Value{});
      }
      if (isConstPush(instruction.op)) {
        stack.back() = Value{Kind::Constant, constValue(instruction)};
      }
    }
    return true;
  }

  const IrFunction &function_;
  const IrModule &module_;
  const IrCfg &cfg_;
  const size_t frameSlots_;
  const std::vector<bool> targets_;
  std::vector<Action> actions_;
  std::vector<uint64_t> slots_;
  std::vector<State> entry_;
  std::vector<bool> seen_;
};

bool replaceLocalAddresses(IrFunction &function, const IrModule &module) {
  size_t frameSlots = 0;
  bool addressTaken = false;
  for (const IrInstruction &instruction : function.instructions) {
    if (instruction.op == IrOpcode::LoadLocal || instruction.op == IrOpcode::StoreLocal ||
        instruction.op == IrOpcode::AddressOfLocal) {
      // The frame every backend gives the function (VM, optcpp and native
      // count it this way).
      frameSlots = std::max<size_t>(
          frameSlots, static_cast<size_t>(std::min<uint64_t>(instruction.imm, UINT32_MAX)) + 1);
      addressTaken |= instruction.op == IrOpcode::AddressOfLocal;
    }
  }
  if (!addressTaken) {
    return false;
  }
  IrCfg cfg;
  IrCfgError cfgError;
  if (!buildIrCfg(function, module, cfg, cfgError)) {
    return false;
  }
  if (cfg.blocks.empty() ||
      cfg.blocks.size() * (frameSlots + static_cast<size_t>(cfg.maxStackDepth)) > MaxStateCells) {
    return false;
  }
  Analyzer analyzer(function, module, cfg, frameSlots);
  if (!analyzer.run() || !analyzer.rewrites()) {
    return false;
  }
  // An AddressOfLocal in an unreachable block is still an address the escape
  // analysis would see; leave such functions to cfg-simplify first.
  for (size_t b = 0; b < cfg.blocks.size(); ++b) {
    if (cfg.blocks[b].reachable) {
      continue;
    }
    for (size_t i = cfg.blocks[b].start; i < cfg.blocks[b].end; ++i) {
      if (function.instructions[i].op == IrOpcode::AddressOfLocal) {
        return false;
      }
    }
  }

  // Build the new body. A jump to an erased instruction lands on the next one
  // that survives; erased instructions only moved address values, which no
  // longer exist on any path.
  const size_t count = function.instructions.size();
  std::vector<size_t> newIndex(count + 1, 0);
  std::vector<IrInstruction> out;
  out.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    newIndex[i] = out.size();
    const IrInstruction &original = function.instructions[i];
    IrInstruction instruction = original;
    switch (analyzer.action(i)) {
    case Action::Keep:
      out.push_back(instruction);
      break;
    case Action::Erase:
      break;
    case Action::ToLoadLocal:
      instruction.op = IrOpcode::LoadLocal;
      instruction.imm = analyzer.slot(i);
      out.push_back(instruction);
      break;
    case Action::ToStoreLocal:
      instruction.op = IrOpcode::StoreLocal;
      instruction.imm = analyzer.slot(i);
      out.push_back(instruction);
      // The Pop that dropped StoreIndirect's result goes with it.
      newIndex[i + 1] = out.size();
      ++i;
      break;
    case Action::ToDupStore: {
      IrInstruction dup = original;
      dup.op = IrOpcode::Dup;
      dup.imm = 0;
      out.push_back(dup);
      instruction.op = IrOpcode::StoreLocal;
      instruction.imm = analyzer.slot(i);
      out.push_back(instruction);
      break;
    }
    }
  }
  newIndex[count] = out.size();
  for (IrInstruction &instruction : out) {
    if (isJump(instruction.op) && instruction.imm <= count) {
      instruction.imm = newIndex[static_cast<size_t>(instruction.imm)];
    }
  }
  function.instructions = std::move(out);
  return true;
}

} // namespace

bool runLocalSroaPass(IrModule &module, const IrPassContext &, bool &changed, std::string &) {
  changed = false;
  for (IrFunction &function : module.functions) {
    changed |= replaceLocalAddresses(function, module);
  }
  return true;
}

} // namespace primec::ir_opt
