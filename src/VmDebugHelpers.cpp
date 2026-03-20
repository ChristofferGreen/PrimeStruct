#include "primec/Vm.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace primec {
namespace {

const char *sourceMapProvenanceName(IrSourceMapProvenance provenance) {
  switch (provenance) {
    case IrSourceMapProvenance::CanonicalAst:
      return "canonical_ast";
    case IrSourceMapProvenance::SyntheticIr:
      return "synthetic_ir";
    case IrSourceMapProvenance::Unknown:
      break;
  }
  return "unknown";
}

} // namespace

bool resolveSourceBreakpoints(const IrModule &module,
                              uint32_t line,
                              std::optional<uint32_t> column,
                              std::vector<VmResolvedSourceBreakpoint> &outBreakpoints,
                              std::string &error) {
  outBreakpoints.clear();
  if (line == 0) {
    error = "invalid source breakpoint line";
    return false;
  }

  std::unordered_map<uint32_t, const IrInstructionSourceMapEntry *> entriesByDebugId;
  std::vector<uint32_t> matchingColumns;
  matchingColumns.reserve(module.instructionSourceMap.size());
  for (const auto &entry : module.instructionSourceMap) {
    if (entry.provenance != IrSourceMapProvenance::CanonicalAst) {
      continue;
    }
    if (entry.line != line || entry.column == 0) {
      continue;
    }
    matchingColumns.push_back(entry.column);
    if (column.has_value() && entry.column != *column) {
      continue;
    }
    entriesByDebugId.emplace(entry.debugId, &entry);
  }

  if (!column.has_value()) {
    if (matchingColumns.empty()) {
      error = "source breakpoint not found at line " + std::to_string(line);
      return false;
    }
    std::sort(matchingColumns.begin(), matchingColumns.end());
    matchingColumns.erase(std::unique(matchingColumns.begin(), matchingColumns.end()), matchingColumns.end());
    if (matchingColumns.size() > 1) {
      std::ostringstream columnsOut;
      for (size_t i = 0; i < matchingColumns.size(); ++i) {
        if (i > 0) {
          columnsOut << ",";
        }
        columnsOut << matchingColumns[i];
      }
      error = "ambiguous source breakpoint at line " + std::to_string(line) +
              "; specify column (matches: " + columnsOut.str() + ")";
      return false;
    }

    const uint32_t selectedColumn = matchingColumns.front();
    entriesByDebugId.clear();
    for (const auto &entry : module.instructionSourceMap) {
      if (entry.provenance != IrSourceMapProvenance::CanonicalAst) {
        continue;
      }
      if (entry.line == line && entry.column == selectedColumn) {
        entriesByDebugId.emplace(entry.debugId, &entry);
      }
    }
  } else if (entriesByDebugId.empty()) {
    error = "source breakpoint not found at line " + std::to_string(line) + ", column " + std::to_string(*column);
    return false;
  }

  for (size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex) {
    const IrFunction &function = module.functions[functionIndex];
    for (size_t instructionPointer = 0; instructionPointer < function.instructions.size(); ++instructionPointer) {
      const IrInstruction &instruction = function.instructions[instructionPointer];
      auto entryIt = entriesByDebugId.find(instruction.debugId);
      if (entryIt == entriesByDebugId.end()) {
        continue;
      }
      const IrInstructionSourceMapEntry &entry = *entryIt->second;
      outBreakpoints.push_back(
          {functionIndex, instructionPointer, instruction.debugId, entry.line, entry.column, entry.provenance});
    }
  }
  if (outBreakpoints.empty()) {
    if (column.has_value()) {
      error = "source breakpoint has no executable instructions at line " + std::to_string(line) + ", column " +
              std::to_string(*column);
    } else {
      error = "source breakpoint has no executable instructions at line " + std::to_string(line);
    }
    return false;
  }

  std::sort(outBreakpoints.begin(),
            outBreakpoints.end(),
            [](const VmResolvedSourceBreakpoint &left, const VmResolvedSourceBreakpoint &right) {
              if (left.functionIndex != right.functionIndex) {
                return left.functionIndex < right.functionIndex;
              }
              if (left.instructionPointer != right.instructionPointer) {
                return left.instructionPointer < right.instructionPointer;
              }
              return left.debugId < right.debugId;
            });
  outBreakpoints.erase(std::unique(outBreakpoints.begin(),
                                   outBreakpoints.end(),
                                   [](const VmResolvedSourceBreakpoint &left, const VmResolvedSourceBreakpoint &right) {
                                     return left.functionIndex == right.functionIndex &&
                                            left.instructionPointer == right.instructionPointer;
                                   }),
                       outBreakpoints.end());
  return true;
}

void VmDebugSession::appendMappedStackTrace(std::string &error) const {
  if (!module_ || module_->instructionSourceMap.empty() || frames_.empty()) {
    return;
  }

  std::unordered_map<uint32_t, const IrInstructionSourceMapEntry *> sourceByDebugId;
  sourceByDebugId.reserve(module_->instructionSourceMap.size());
  for (const auto &entry : module_->instructionSourceMap) {
    sourceByDebugId.emplace(entry.debugId, &entry);
  }

  std::ostringstream trace;
  bool emittedAnyFrame = false;
  size_t frameOrdinal = 0;
  for (size_t reverseIndex = 0; reverseIndex < frames_.size(); ++reverseIndex) {
    const Frame &currentFrame = frames_[frames_.size() - reverseIndex - 1];
    if (!currentFrame.function) {
      continue;
    }
    const IrFunction &function = *currentFrame.function;
    size_t traceIp = currentFrame.ip;
    if (reverseIndex > 0 && traceIp > 0 && traceIp <= function.instructions.size()) {
      const IrInstruction &callSite = function.instructions[traceIp - 1];
      if (callSite.op == IrOpcode::Call || callSite.op == IrOpcode::CallVoid) {
        traceIp -= 1;
      }
    }

    trace << (emittedAnyFrame ? "\n" : "\nstack trace:\n");
    trace << "  #" << frameOrdinal << " " << function.name << " ip " << traceIp;
    if (traceIp < function.instructions.size()) {
      const IrInstruction &instruction = function.instructions[traceIp];
      trace << " debug_id " << instruction.debugId;
      auto sourceIt = sourceByDebugId.find(instruction.debugId);
      if (sourceIt != sourceByDebugId.end() && sourceIt->second->line > 0 && sourceIt->second->column > 0) {
        trace << " at " << sourceIt->second->line << ":" << sourceIt->second->column << " ["
              << sourceMapProvenanceName(sourceIt->second->provenance) << "]";
      }
    }
    emittedAnyFrame = true;
    ++frameOrdinal;
  }
  if (emittedAnyFrame) {
    error += trace.str();
  }
}

} // namespace primec
