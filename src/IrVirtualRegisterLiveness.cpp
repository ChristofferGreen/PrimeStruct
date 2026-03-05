#include "primec/IrVirtualRegisterLiveness.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <vector>

namespace primec {
namespace {

using RegisterList = std::vector<uint32_t>;

void sortUnique(RegisterList &registers) {
  std::sort(registers.begin(), registers.end());
  registers.erase(std::unique(registers.begin(), registers.end()), registers.end());
}

RegisterList sortedUnion(const RegisterList &left, const RegisterList &right) {
  RegisterList out;
  out.reserve(left.size() + right.size());
  std::set_union(
      left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(out));
  return out;
}

RegisterList sortedDifference(const RegisterList &left, const RegisterList &right) {
  RegisterList out;
  out.reserve(left.size());
  std::set_difference(
      left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(out));
  return out;
}

RegisterList mapSuccessorLiveInThroughEdge(const RegisterList &successorLiveIn,
                                           const IrVirtualRegisterEdge &edge) {
  RegisterList mapped;
  mapped.reserve(successorLiveIn.size());
  for (uint32_t reg : successorLiveIn) {
    uint32_t sourceReg = reg;
    for (const auto &move : edge.stackMoves) {
      if (move.destinationRegister == reg) {
        sourceReg = move.sourceRegister;
        break;
      }
    }
    mapped.push_back(sourceReg);
  }
  sortUnique(mapped);
  return mapped;
}

bool buildBlockUseDef(const IrVirtualRegisterFunction &function,
                      std::vector<RegisterList> &blockUseBeforeDef,
                      std::vector<RegisterList> &blockDef,
                      std::string &error) {
  error.clear();
  blockUseBeforeDef.clear();
  blockDef.clear();
  blockUseBeforeDef.resize(function.blocks.size());
  blockDef.resize(function.blocks.size());

  for (size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex) {
    const auto &block = function.blocks[blockIndex];
    if (!block.reachable) {
      continue;
    }

    RegisterList defined;
    defined.reserve(block.instructions.size());
    for (const auto &instruction : block.instructions) {
      for (uint32_t reg : instruction.useRegisters) {
        if (reg >= function.nextVirtualRegister) {
          error = "liveness pass found out-of-range use register";
          return false;
        }
        if (!std::binary_search(defined.begin(), defined.end(), reg)) {
          blockUseBeforeDef[blockIndex].push_back(reg);
        }
      }
      for (uint32_t reg : instruction.defRegisters) {
        if (reg >= function.nextVirtualRegister) {
          error = "liveness pass found out-of-range def register";
          return false;
        }
        defined.push_back(reg);
      }
      sortUnique(defined);
    }
    blockDef[blockIndex] = std::move(defined);
    sortUnique(blockUseBeforeDef[blockIndex]);
  }

  return true;
}

bool computeBlockLiveness(const IrVirtualRegisterFunction &function,
                          std::vector<IrVirtualRegisterBlockLiveness> &out,
                          std::string &error) {
  error.clear();
  out.clear();
  out.resize(function.blocks.size());

  std::vector<RegisterList> blockUseBeforeDef;
  std::vector<RegisterList> blockDef;
  if (!buildBlockUseDef(function, blockUseBeforeDef, blockDef, error)) {
    return false;
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t reverseBlock = function.blocks.size(); reverseBlock > 0; --reverseBlock) {
      const size_t blockIndex = reverseBlock - 1;
      const auto &block = function.blocks[blockIndex];
      if (!block.reachable) {
        continue;
      }

      RegisterList newLiveOut;
      for (const auto &edge : block.successorEdges) {
        if (edge.successorBlockIndex >= function.blocks.size()) {
          error = "liveness pass found out-of-range successor block index";
          return false;
        }
        const RegisterList mappedLiveIn =
            mapSuccessorLiveInThroughEdge(out[edge.successorBlockIndex].liveInRegisters, edge);
        newLiveOut = sortedUnion(newLiveOut, mappedLiveIn);
      }

      const RegisterList liveOutMinusDef = sortedDifference(newLiveOut, blockDef[blockIndex]);
      const RegisterList newLiveIn =
          sortedUnion(blockUseBeforeDef[blockIndex], liveOutMinusDef);

      if (newLiveOut != out[blockIndex].liveOutRegisters ||
          newLiveIn != out[blockIndex].liveInRegisters) {
        out[blockIndex].liveOutRegisters = std::move(newLiveOut);
        out[blockIndex].liveInRegisters = std::move(newLiveIn);
        changed = true;
      }
    }
  }

  return true;
}

void updateRange(IrVirtualRegisterInterval &interval, uint32_t position) {
  if (interval.ranges.empty()) {
    interval.ranges.push_back({position, position});
    return;
  }
  interval.ranges[0].startPosition = std::min(interval.ranges[0].startPosition, position);
  interval.ranges[0].endPosition = std::max(interval.ranges[0].endPosition, position);
}

bool buildIntervals(const IrVirtualRegisterFunction &function,
                    const std::vector<IrVirtualRegisterBlockLiveness> &blockLiveness,
                    std::vector<IrVirtualRegisterInterval> &intervals,
                    std::string &error) {
  error.clear();
  intervals.clear();
  intervals.resize(function.nextVirtualRegister);
  for (uint32_t reg = 0; reg < function.nextVirtualRegister; ++reg) {
    intervals[reg].virtualRegister = reg;
  }

  for (size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex) {
    const auto &block = function.blocks[blockIndex];
    if (!block.reachable) {
      continue;
    }
    const uint32_t blockStartPosition =
        static_cast<uint32_t>(block.startInstructionIndex) * 2u;
    const uint32_t blockEndPosition =
        (block.endInstructionIndex == block.startInstructionIndex)
            ? blockStartPosition
            : static_cast<uint32_t>(block.endInstructionIndex - 1) * 2u + 1u;

    for (uint32_t reg : blockLiveness[blockIndex].liveInRegisters) {
      if (reg >= function.nextVirtualRegister) {
        error = "liveness pass found out-of-range live-in register";
        return false;
      }
      updateRange(intervals[reg], blockStartPosition);
    }
    for (uint32_t reg : blockLiveness[blockIndex].liveOutRegisters) {
      if (reg >= function.nextVirtualRegister) {
        error = "liveness pass found out-of-range live-out register";
        return false;
      }
      updateRange(intervals[reg], blockEndPosition);
    }

    for (size_t instructionOffset = 0; instructionOffset < block.instructions.size();
         ++instructionOffset) {
      const uint32_t instructionPosition =
          (static_cast<uint32_t>(block.startInstructionIndex) +
           static_cast<uint32_t>(instructionOffset)) *
          2u;
      const auto &instruction = block.instructions[instructionOffset];
      for (uint32_t reg : instruction.useRegisters) {
        if (reg >= function.nextVirtualRegister) {
          error = "liveness pass found out-of-range use register";
          return false;
        }
        updateRange(intervals[reg], instructionPosition);
      }
      for (uint32_t reg : instruction.defRegisters) {
        if (reg >= function.nextVirtualRegister) {
          error = "liveness pass found out-of-range def register";
          return false;
        }
        updateRange(intervals[reg], instructionPosition + 1u);
      }
    }
  }

  intervals.erase(std::remove_if(intervals.begin(),
                                 intervals.end(),
                                 [](const IrVirtualRegisterInterval &interval) {
                                   return interval.ranges.empty();
                                 }),
                  intervals.end());
  std::sort(intervals.begin(), intervals.end(), [](const auto &left, const auto &right) {
    const IrVirtualRegisterLiveRange leftRange = left.ranges.front();
    const IrVirtualRegisterLiveRange rightRange = right.ranges.front();
    if (leftRange.startPosition != rightRange.startPosition) {
      return leftRange.startPosition < rightRange.startPosition;
    }
    if (leftRange.endPosition != rightRange.endPosition) {
      return leftRange.endPosition < rightRange.endPosition;
    }
    return left.virtualRegister < right.virtualRegister;
  });

  return true;
}

} // namespace

bool buildIrVirtualRegisterLiveness(const IrVirtualRegisterModule &module,
                                    IrVirtualRegisterModuleLiveness &out,
                                    std::string &error) {
  error.clear();
  out = {};
  out.functions.resize(module.functions.size());

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const auto &function = module.functions[functionIndex];
    auto &functionLiveness = out.functions[functionIndex];
    functionLiveness.functionName = function.name;

    if (!computeBlockLiveness(function, functionLiveness.blocks, error)) {
      if (!error.empty()) {
        error = "liveness pass failed in function " + function.name + ": " + error;
      }
      return false;
    }
    if (!buildIntervals(function, functionLiveness.blocks, functionLiveness.intervals, error)) {
      if (!error.empty()) {
        error = "liveness pass failed in function " + function.name + ": " + error;
      }
      return false;
    }
  }

  return true;
}

} // namespace primec
