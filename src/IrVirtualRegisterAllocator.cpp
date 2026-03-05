#include "primec/IrVirtualRegisterAllocator.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace primec {
namespace {

struct IntervalWorkItem {
  uint32_t virtualRegister = 0;
  uint32_t startPosition = 0;
  uint32_t endPosition = 0;
};

struct ActiveInterval {
  uint32_t assignmentIndex = 0;
  uint32_t physicalRegister = 0;
};

void sortActiveIntervals(std::vector<ActiveInterval> &active,
                         const std::vector<IrLinearScanRegisterAssignment> &assignments) {
  std::sort(active.begin(), active.end(), [&](const ActiveInterval &left, const ActiveInterval &right) {
    const auto &leftAssignment = assignments[left.assignmentIndex];
    const auto &rightAssignment = assignments[right.assignmentIndex];
    if (leftAssignment.endPosition != rightAssignment.endPosition) {
      return leftAssignment.endPosition < rightAssignment.endPosition;
    }
    return leftAssignment.virtualRegister < rightAssignment.virtualRegister;
  });
}

void sortFreeRegisters(std::vector<uint32_t> &freeRegisters) {
  std::sort(freeRegisters.begin(), freeRegisters.end());
}

bool pickSpillCandidate(const std::vector<ActiveInterval> &active,
                        const std::vector<IrLinearScanRegisterAssignment> &assignments,
                        size_t &candidateActiveIndex) {
  if (active.empty()) {
    return false;
  }
  candidateActiveIndex = 0;
  for (size_t index = 1; index < active.size(); ++index) {
    const auto &best = assignments[active[candidateActiveIndex].assignmentIndex];
    const auto &candidate = assignments[active[index].assignmentIndex];
    if (candidate.endPosition > best.endPosition) {
      candidateActiveIndex = index;
      continue;
    }
    if (candidate.endPosition == best.endPosition && candidate.virtualRegister > best.virtualRegister) {
      candidateActiveIndex = index;
    }
  }
  return true;
}

bool buildIntervalWorkItems(const IrVirtualRegisterFunctionLiveness &functionLiveness,
                            std::vector<IntervalWorkItem> &out,
                            std::string &error) {
  error.clear();
  out.clear();
  out.reserve(functionLiveness.intervals.size());
  for (const auto &interval : functionLiveness.intervals) {
    if (interval.ranges.empty()) {
      continue;
    }
    uint32_t start = std::numeric_limits<uint32_t>::max();
    uint32_t end = 0;
    for (const auto &range : interval.ranges) {
      if (range.endPosition < range.startPosition) {
        error = "linear-scan allocator found invalid interval range";
        return false;
      }
      start = std::min(start, range.startPosition);
      end = std::max(end, range.endPosition);
    }
    out.push_back({interval.virtualRegister, start, end});
  }
  std::sort(out.begin(), out.end(), [](const IntervalWorkItem &left, const IntervalWorkItem &right) {
    if (left.startPosition != right.startPosition) {
      return left.startPosition < right.startPosition;
    }
    if (left.endPosition != right.endPosition) {
      return left.endPosition < right.endPosition;
    }
    return left.virtualRegister < right.virtualRegister;
  });
  return true;
}

bool allocateFunctionLinearScan(const IrVirtualRegisterFunctionLiveness &functionLiveness,
                                const IrLinearScanAllocatorOptions &options,
                                IrLinearScanFunctionAllocation &out,
                                std::string &error) {
  error.clear();
  out = {};
  out.functionName = functionLiveness.functionName;

  if (options.spillPolicy != IrLinearScanSpillPolicy::SpillFarthestEnd) {
    error = "linear-scan allocator does not support spill policy";
    return false;
  }

  std::vector<IntervalWorkItem> intervals;
  if (!buildIntervalWorkItems(functionLiveness, intervals, error)) {
    return false;
  }

  out.assignments.reserve(intervals.size());
  std::vector<ActiveInterval> active;
  std::vector<uint32_t> freeRegisters;
  freeRegisters.reserve(options.physicalRegisterCount);
  for (uint32_t reg = 0; reg < options.physicalRegisterCount; ++reg) {
    freeRegisters.push_back(reg);
  }

  uint32_t nextSpillSlot = 0;
  for (const auto &interval : intervals) {
    std::vector<size_t> expired;
    for (size_t index = 0; index < active.size(); ++index) {
      const auto &activeAssignment = out.assignments[active[index].assignmentIndex];
      if (activeAssignment.endPosition < interval.startPosition) {
        freeRegisters.push_back(active[index].physicalRegister);
        expired.push_back(index);
      }
    }
    for (auto it = expired.rbegin(); it != expired.rend(); ++it) {
      active.erase(active.begin() + static_cast<std::ptrdiff_t>(*it));
    }
    sortFreeRegisters(freeRegisters);
    sortActiveIntervals(active, out.assignments);

    IrLinearScanRegisterAssignment assignment;
    assignment.virtualRegister = interval.virtualRegister;
    assignment.startPosition = interval.startPosition;
    assignment.endPosition = interval.endPosition;

    if (!freeRegisters.empty()) {
      assignment.physicalRegister = freeRegisters.front();
      freeRegisters.erase(freeRegisters.begin());
      const uint32_t assignmentIndex = static_cast<uint32_t>(out.assignments.size());
      out.assignments.push_back(assignment);
      active.push_back({assignmentIndex, assignment.physicalRegister});
      sortActiveIntervals(active, out.assignments);
      continue;
    }

    size_t spillCandidateActiveIndex = 0;
    if (!pickSpillCandidate(active, out.assignments, spillCandidateActiveIndex)) {
      assignment.spilled = true;
      assignment.spillSlot = nextSpillSlot++;
      out.assignments.push_back(assignment);
      continue;
    }

    const ActiveInterval spillCandidate = active[spillCandidateActiveIndex];
    auto &candidateAssignment = out.assignments[spillCandidate.assignmentIndex];
    const bool spillCandidateInstead =
        (candidateAssignment.endPosition > interval.endPosition) ||
        (candidateAssignment.endPosition == interval.endPosition &&
         candidateAssignment.virtualRegister > interval.virtualRegister);

    if (spillCandidateInstead) {
      candidateAssignment.spilled = true;
      candidateAssignment.spillSlot = nextSpillSlot++;

      assignment.physicalRegister = spillCandidate.physicalRegister;
      const uint32_t assignmentIndex = static_cast<uint32_t>(out.assignments.size());
      out.assignments.push_back(assignment);
      active[spillCandidateActiveIndex] = {assignmentIndex, spillCandidate.physicalRegister};
      sortActiveIntervals(active, out.assignments);
      continue;
    }

    assignment.spilled = true;
    assignment.spillSlot = nextSpillSlot++;
    out.assignments.push_back(assignment);
  }

  std::sort(out.assignments.begin(),
            out.assignments.end(),
            [](const IrLinearScanRegisterAssignment &left, const IrLinearScanRegisterAssignment &right) {
              return left.virtualRegister < right.virtualRegister;
            });
  out.spillSlotCount = nextSpillSlot;
  return true;
}

} // namespace

bool allocateIrVirtualRegistersLinearScan(const IrVirtualRegisterModuleLiveness &liveness,
                                          const IrLinearScanAllocatorOptions &options,
                                          IrLinearScanModuleAllocation &out,
                                          std::string &error) {
  error.clear();
  out = {};
  out.functions.resize(liveness.functions.size());

  for (size_t functionIndex = 0; functionIndex < liveness.functions.size(); ++functionIndex) {
    if (!allocateFunctionLinearScan(liveness.functions[functionIndex], options, out.functions[functionIndex], error)) {
      if (!error.empty()) {
        error = "linear-scan allocator failed in function " + liveness.functions[functionIndex].functionName + ": " +
                error;
      }
      return false;
    }
  }

  return true;
}

} // namespace primec
