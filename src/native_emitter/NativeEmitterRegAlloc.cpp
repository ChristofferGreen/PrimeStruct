#include "NativeEmitterRegAlloc.h"

#include "NativeEmitterPromotion.h"
#include "primec/ir/IrVirtualRegisterLowering.h"

#include <algorithm>
#include <limits>

namespace primec::native_emitter {
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

constexpr uint32_t NoPosition = std::numeric_limits<uint32_t>::max();

} // namespace

bool planNativeRegisterAllocation(const IrModule &module,
                                  size_t functionIndex,
                                  const std::vector<uint8_t> &pool,
                                  RegAllocFunctionPlan &out,
                                  std::string &error) {
  out = {};
  error.clear();
  IrVirtualRegisterFunction registerForm;
  IrVirtualRegisterLoweringOptions options;
  options.promoteLocals = true;
  if (!lowerIrFunctionToBlockVirtualRegisters(
          module, functionIndex, registerForm, error, options)) {
    return false;
  }
  const size_t valueCount = registerForm.nextVirtualRegister;
  std::vector<uint32_t> canonical(valueCount);
  for (uint32_t value = 0; value < valueCount; ++value) {
    canonical[value] = value;
  }
  std::vector<bool> isConstant(valueCount, false);
  std::vector<uint64_t> constant(valueCount, 0);
  const auto canon = [&](uint32_t value) { return canonical[value]; };

  out.blocks.resize(registerForm.blocks.size());
  for (size_t blockIndex = 0; blockIndex < registerForm.blocks.size(); ++blockIndex) {
    const IrVirtualRegisterBlock &in = registerForm.blocks[blockIndex];
    RegAllocBlock &block = out.blocks[blockIndex];
    block.start = in.startInstructionIndex;
    block.end = in.endInstructionIndex;
    block.reachable = in.reachable;
    if (!in.reachable) {
      continue;
    }
    if (blockIndex == 0) {
      block.entryValues = in.entryRegisters;
      for (const IrVirtualRegisterLocalValue &local : in.entryLocals) {
        block.zeroValues.push_back(local.reg);
      }
    }
    block.instructions.reserve(in.instructions.size());
    for (size_t offset = 0; offset < in.instructions.size(); ++offset) {
      const IrVirtualRegisterInstruction &source = in.instructions[offset];
      RegAllocInstruction instruction;
      instruction.irIndex = in.startInstructionIndex + offset;
      const IrInstruction &ir = source.instruction;
      switch (ir.op) {
      case IrOpcode::PushI32:
        instruction.folded = true;
        isConstant[source.defRegisters.front()] = true;
        constant[source.defRegisters.front()] =
            static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(ir.imm)));
        break;
      case IrOpcode::PushF32:
        // Float constants are their bit patterns, as the templates push them.
        instruction.folded = true;
        isConstant[source.defRegisters.front()] = true;
        constant[source.defRegisters.front()] = static_cast<uint32_t>(ir.imm);
        break;
      case IrOpcode::PushI64:
      case IrOpcode::PushF64:
        instruction.folded = true;
        isConstant[source.defRegisters.front()] = true;
        constant[source.defRegisters.front()] = ir.imm;
        break;
      case IrOpcode::Dup:
        instruction.folded = true;
        canonical[source.defRegisters.front()] = canon(source.useRegisters.front());
        break;
      case IrOpcode::Pop:
        instruction.folded = true;
        break;
      default:
        if (ir.op == IrOpcode::LoadLocal && source.localUseRegister.has_value()) {
          instruction.folded = true;
          canonical[source.defRegisters.front()] = canon(*source.localUseRegister);
          break;
        }
        if (ir.op == IrOpcode::StoreLocal && source.localDefRegister.has_value()) {
          instruction.folded = true;
          canonical[*source.localDefRegister] = canon(source.useRegisters.front());
          break;
        }
        for (const uint32_t use : source.useRegisters) {
          instruction.uses.push_back(canon(use));
        }
        instruction.defs = source.defRegisters;
        break;
      }
      block.instructions.push_back(std::move(instruction));
    }
    for (const IrVirtualRegisterEdge &inEdge : in.successorEdges) {
      RegAllocEdge edge;
      edge.successorBlock = inEdge.successorBlockIndex;
      for (const IrVirtualRegisterEdgeMove &move : inEdge.stackMoves) {
        edge.moves.push_back({canon(move.sourceRegister), move.destinationRegister});
      }
      for (const IrVirtualRegisterLocalMove &move : inEdge.localMoves) {
        edge.moves.push_back({canon(move.sourceRegister), move.destinationRegister});
      }
      block.edges.push_back(std::move(edge));
    }
  }

  // Readers per value, to find compares whose only reader is the branch after them.
  std::vector<uint32_t> readers(valueCount, 0);
  for (const RegAllocBlock &block : out.blocks) {
    for (const RegAllocInstruction &instruction : block.instructions) {
      for (const uint32_t use : instruction.uses) {
        ++readers[use];
      }
    }
    for (const RegAllocEdge &edge : block.edges) {
      for (const RegAllocMove &move : edge.moves) {
        ++readers[move.source];
      }
    }
  }
  std::vector<bool> fused(valueCount, false);
  for (RegAllocBlock &block : out.blocks) {
    for (size_t i = 0; i + 1 < block.instructions.size(); ++i) {
      RegAllocInstruction &compare = block.instructions[i];
      const RegAllocInstruction &branch = block.instructions[i + 1];
      const IrOpcode compareOp = module.functions[functionIndex].instructions[compare.irIndex].op;
      const IrOpcode branchOp = module.functions[functionIndex].instructions[branch.irIndex].op;
      if (compare.folded || !isIntegerComparison(compareOp) || branchOp != IrOpcode::JumpIfZero ||
          compare.defs.size() != 1 || branch.uses.size() != 1 ||
          branch.uses.front() != compare.defs.front() || readers[compare.defs.front()] != 1) {
        continue;
      }
      compare.fusedIntoBranch = true;
      fused[compare.defs.front()] = true;
    }
  }

  // Live ranges. Every value is written and read inside one block.
  out.rangeStart.assign(valueCount, NoPosition);
  out.rangeEnd.assign(valueCount, 0);
  const auto define = [&](uint32_t value, uint32_t position) {
    out.rangeStart[value] = std::min(out.rangeStart[value], position);
    out.rangeEnd[value] = std::max(out.rangeEnd[value], position);
  };
  const auto read = [&](uint32_t value, uint32_t position) {
    out.rangeEnd[value] = std::max(out.rangeEnd[value], position);
  };
  for (const RegAllocBlock &block : out.blocks) {
    if (!block.reachable) {
      continue;
    }
    const uint32_t entryPosition = static_cast<uint32_t>(block.start * 4);
    const uint32_t exitPosition = static_cast<uint32_t>(block.end * 4 - 1);
    for (const uint32_t value : block.entryValues) {
      define(value, entryPosition);
    }
    for (const uint32_t value : block.zeroValues) {
      define(value, entryPosition);
    }
    for (const RegAllocInstruction &instruction : block.instructions) {
      for (const uint32_t use : instruction.uses) {
        read(use, regAllocUsePosition(instruction.irIndex));
      }
      for (const uint32_t def : instruction.defs) {
        define(def, regAllocDefPosition(instruction.irIndex));
      }
    }
    for (const RegAllocEdge &edge : block.edges) {
      for (const RegAllocMove &move : edge.moves) {
        read(move.source, exitPosition);
      }
      // A successor's entry values are written where it starts.
      const RegAllocBlock &successor = out.blocks[edge.successorBlock];
      for (const RegAllocMove &move : edge.moves) {
        define(move.destination, static_cast<uint32_t>(successor.start * 4));
      }
    }
  }

  for (RegAllocBlock &block : out.blocks) {
    block.values.insert(block.values.end(), block.entryValues.begin(), block.entryValues.end());
    block.values.insert(block.values.end(), block.zeroValues.begin(), block.zeroValues.end());
    for (const RegAllocInstruction &instruction : block.instructions) {
      block.values.insert(block.values.end(), instruction.defs.begin(), instruction.defs.end());
    }
  }
  for (const RegAllocBlock &block : out.blocks) {
    for (const RegAllocEdge &edge : block.edges) {
      for (const RegAllocMove &move : edge.moves) {
        out.blocks[edge.successorBlock].values.push_back(move.destination);
      }
    }
  }
  for (RegAllocBlock &block : out.blocks) {
    std::sort(block.values.begin(), block.values.end());
    block.values.erase(std::unique(block.values.begin(), block.values.end()), block.values.end());
  }

  out.locations.assign(valueCount, RegAllocLocation{});
  std::vector<uint32_t> homed;
  for (uint32_t value = 0; value < valueCount; ++value) {
    if (isConstant[value]) {
      out.locations[value].kind = RegAllocLocationKind::Imm;
      out.locations[value].imm = constant[value];
      continue;
    }
    if (fused[value] || out.rangeStart[value] == NoPosition || canonical[value] != value) {
      continue;
    }
    homed.push_back(value);
  }

  // How often each value is touched, from the estimated execution count of the instructions
  // that read or write it (NativeEmitterPromotion.h), so the hot values win the registers.
  const std::vector<uint64_t> frequency =
      estimateInstructionFrequency(module.functions[functionIndex]);
  std::vector<double> weight(valueCount, 0.0);
  // Two-address preferences: the result of `a OP b` (and of Neg/SextI32) is best computed in a's
  // register when a dies there.
  std::vector<std::vector<uint32_t>> preferred(valueCount);
  for (const RegAllocBlock &block : out.blocks) {
    for (const RegAllocInstruction &instruction : block.instructions) {
      const double executions = static_cast<double>(frequency[instruction.irIndex]);
      for (const uint32_t use : instruction.uses) {
        weight[use] += executions;
      }
      for (const uint32_t def : instruction.defs) {
        weight[def] += executions;
      }
      const IrOpcode op = module.functions[functionIndex].instructions[instruction.irIndex].op;
      const bool twoAddress =
          op == IrOpcode::AddI32 || op == IrOpcode::AddI64 || op == IrOpcode::SubI32 ||
          op == IrOpcode::SubI64 || op == IrOpcode::MulI32 || op == IrOpcode::MulI64 ||
          op == IrOpcode::NegI32 || op == IrOpcode::NegI64 || op == IrOpcode::SextI32 ||
          op == IrOpcode::NegF32 || op == IrOpcode::NegF64;
      if (twoAddress && instruction.defs.size() == 1) {
        // Add and multiply commute, so either operand's register will do.
        const bool commutes = op == IrOpcode::AddI32 || op == IrOpcode::AddI64 ||
                              op == IrOpcode::MulI32 || op == IrOpcode::MulI64;
        for (size_t operand = 0; operand < instruction.uses.size() && (operand == 0 || commutes);
             ++operand) {
          const uint32_t use = instruction.uses[operand];
          if (!isConstant[use]) {
            preferred[instruction.defs.front()].push_back(use);
            preferred[use].push_back(instruction.defs.front());
          }
        }
      }
    }
  }

  // Classes: values joined by an edge copy share a home when their ranges do not meet, which
  // removes the copy. A local becomes one class across the blocks where it is live.
  std::vector<uint32_t> parent(valueCount);
  for (uint32_t value = 0; value < valueCount; ++value) {
    parent[value] = value;
  }
  const auto find = [&](uint32_t value) {
    while (parent[value] != value) {
      parent[value] = parent[parent[value]];
      value = parent[value];
    }
    return value;
  };
  std::vector<std::vector<uint32_t>> members(valueCount);
  for (const uint32_t value : homed) {
    members[value].push_back(value);
  }
  const auto overlaps = [&](uint32_t a, uint32_t b) {
    return out.rangeStart[a] <= out.rangeEnd[b] && out.rangeStart[b] <= out.rangeEnd[a];
  };
  for (const RegAllocBlock &block : out.blocks) {
    for (const RegAllocEdge &edge : block.edges) {
      for (const RegAllocMove &move : edge.moves) {
        const RegAllocLocation &sourceKind = out.locations[move.source];
        if (sourceKind.kind == RegAllocLocationKind::Imm || fused[move.source] ||
            out.rangeStart[move.destination] == NoPosition) {
          continue;
        }
        uint32_t left = find(move.source);
        uint32_t right = find(move.destination);
        if (left == right) {
          continue;
        }
        bool conflict = false;
        for (const uint32_t a : members[left]) {
          for (const uint32_t b : members[right]) {
            if (overlaps(a, b)) {
              conflict = true;
              break;
            }
          }
          if (conflict) {
            break;
          }
        }
        if (conflict) {
          continue;
        }
        if (members[left].size() < members[right].size()) {
          std::swap(left, right);
        }
        parent[right] = left;
        members[left].insert(members[left].end(), members[right].begin(), members[right].end());
        members[right].clear();
      }
    }
  }
  // Two-address results join the class of an operand that dies at the operation, so a local
  // updated in place (`x = x + 1`) keeps one register through the operation.
  const auto tryUnion = [&](uint32_t a, uint32_t b) {
    uint32_t left = find(a);
    uint32_t right = find(b);
    if (left == right) {
      return true;
    }
    for (const uint32_t x : members[left]) {
      for (const uint32_t y : members[right]) {
        if (overlaps(x, y)) {
          return false;
        }
      }
    }
    if (members[left].size() < members[right].size()) {
      std::swap(left, right);
    }
    parent[right] = left;
    members[left].insert(members[left].end(), members[right].begin(), members[right].end());
    members[right].clear();
    return true;
  };
  for (uint32_t value = 0; value < valueCount; ++value) {
    if (out.rangeStart[value] == NoPosition || canonical[value] != value || isConstant[value] ||
        fused[value]) {
      continue;
    }
    // Prefer the operand from the larger class: a local carried through the function rather
    // than a temporary such as a comparison result.
    std::vector<uint32_t> partners;
    for (const uint32_t partner : preferred[value]) {
      if (partner < value && out.rangeStart[partner] != NoPosition && !isConstant[partner] &&
          !fused[partner]) {
        partners.push_back(partner);
      }
    }
    std::stable_sort(partners.begin(), partners.end(), [&](uint32_t a, uint32_t b) {
      return members[find(a)].size() > members[find(b)].size();
    });
    for (const uint32_t partner : partners) {
      if (tryUnion(value, partner)) {
        break;
      }
    }
  }

  std::vector<uint32_t> classes;
  std::vector<double> classWeight(valueCount, 0.0);
  for (const uint32_t value : homed) {
    const uint32_t root = find(value);
    if (root == value) {
      classes.push_back(value);
    }
    classWeight[root] += weight[value];
  }

  // Interference between values: their ranges meet.
  std::vector<std::vector<uint32_t>> neighbours(valueCount);
  {
    std::vector<uint32_t> byStart = homed;
    std::sort(byStart.begin(), byStart.end(), [&](uint32_t a, uint32_t b) {
      return out.rangeStart[a] < out.rangeStart[b];
    });
    std::vector<uint32_t> open;
    for (const uint32_t value : byStart) {
      for (size_t i = 0; i < open.size();) {
        if (out.rangeEnd[open[i]] < out.rangeStart[value]) {
          open[i] = open.back();
          open.pop_back();
        } else {
          ++i;
        }
      }
      for (const uint32_t other : open) {
        neighbours[value].push_back(other);
        neighbours[other].push_back(value);
      }
      open.push_back(value);
    }
  }

  // Greedy colouring by spill cost per position held, so short busy values (operands,
  // comparison results) come before long-lived locals that sit idle for most of a loop. A class
  // takes one register everywhere when one is free across all its members; otherwise it is
  // split at block boundaries and each member gets a register of its own or a slot.
  std::vector<double> classLength(valueCount, 0.0);
  for (const uint32_t value : homed) {
    classLength[find(value)] +=
        static_cast<double>(out.rangeEnd[value] - out.rangeStart[value] + 1);
  }
  std::sort(classes.begin(), classes.end(), [&](uint32_t a, uint32_t b) {
    const double left = classWeight[a] / classLength[a];
    const double right = classWeight[b] / classLength[b];
    if (left != right) {
      return left > right;
    }
    return a < b;
  });
  std::vector<RegAllocLocation> home(valueCount);
  uint32_t slotCount = 0;
  const auto registersAround = [&](uint32_t value, uint32_t root) {
    uint32_t used = 0;
    for (const uint32_t other : neighbours[value]) {
      if (find(other) != root && home[other].kind == RegAllocLocationKind::Reg) {
        used |= 1u << home[other].reg;
      }
    }
    return used;
  };
  const auto preferredRegister = [&](uint32_t value, uint32_t used) -> int {
    for (const uint32_t partner : preferred[value]) {
      if (home[partner].kind == RegAllocLocationKind::Reg &&
          (used & (1u << home[partner].reg)) == 0) {
        return home[partner].reg;
      }
    }
    return -1;
  };
  const auto firstFree = [&](uint32_t used) -> int {
    for (const uint8_t reg : pool) {
      if ((used & (1u << reg)) == 0) {
        return reg;
      }
    }
    return -1;
  };
  for (const uint32_t root : classes) {
    uint32_t used = 0;
    for (const uint32_t member : members[root]) {
      used |= registersAround(member, root);
    }
    int chosen = -1;
    for (const uint32_t member : members[root]) {
      chosen = preferredRegister(member, used);
      if (chosen >= 0) {
        break;
      }
    }
    if (chosen < 0) {
      chosen = firstFree(used);
    }
    if (chosen >= 0) {
      for (const uint32_t member : members[root]) {
        home[member] = {RegAllocLocationKind::Reg, static_cast<uint8_t>(chosen), 0, 0};
      }
      continue;
    }
    // No register is free across the whole class: it lives in one slot (its members never
    // overlap). Splitting it at block boundaries instead measured slower: a member's one or two
    // uses gain less from a register than the copies to and from the slot on its edges cost.
    std::vector<bool> usedSlots(slotCount, false);
    for (const uint32_t member : members[root]) {
      for (const uint32_t neighbour : neighbours[member]) {
        if (find(neighbour) != root && home[neighbour].kind == RegAllocLocationKind::Slot) {
          usedSlots[home[neighbour].slot] = true;
        }
      }
    }
    uint32_t slot = 0;
    while (slot < slotCount && usedSlots[slot]) {
      ++slot;
    }
    if (slot == slotCount) {
      ++slotCount;
    }
    for (const uint32_t member : members[root]) {
      home[member] = {RegAllocLocationKind::Slot, 0, slot, 0};
    }
  }
  for (const uint32_t value : homed) {
    out.locations[value] = home[value];
  }
  out.spillSlotCount = slotCount;
  return true;
}

std::vector<uint8_t>
regAllocRegistersLiveAcross(const RegAllocFunctionPlan &plan, size_t blockIndex, size_t index) {
  std::vector<uint8_t> registers;
  const uint32_t use = regAllocUsePosition(index);
  const uint32_t def = regAllocDefPosition(index);
  for (const uint32_t value : plan.blocks[blockIndex].values) {
    const RegAllocLocation &location = plan.locations[value];
    if (location.kind == RegAllocLocationKind::Reg && plan.rangeStart[value] < use &&
        plan.rangeEnd[value] > def) {
      registers.push_back(location.reg);
    }
  }
  std::sort(registers.begin(), registers.end());
  registers.erase(std::unique(registers.begin(), registers.end()), registers.end());
  return registers;
}

} // namespace primec::native_emitter
