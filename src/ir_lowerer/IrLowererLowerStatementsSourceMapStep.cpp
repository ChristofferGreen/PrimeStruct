#include "IrLowererLowerStatementsSourceMapStep.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace primec::ir_lowerer {

bool runLowerStatementsSourceMapStep(const LowerStatementsSourceMapStepInput &input,
                                     std::string &errorOut) {
  if (input.functionSyntaxProvenanceByName == nullptr) {
    errorOut = "native backend missing statements source-map step dependency: functionSyntaxProvenanceByName";
    return false;
  }
  if (input.instructionSourceRangesByFunction == nullptr) {
    errorOut = "native backend missing statements source-map step dependency: instructionSourceRangesByFunction";
    return false;
  }
  if (input.stringTable == nullptr) {
    errorOut = "native backend missing statements source-map step dependency: stringTable";
    return false;
  }
  if (input.outModule == nullptr) {
    errorOut = "native backend missing statements source-map step dependency: outModule";
    return false;
  }

  for (auto &rangeEntry : *input.instructionSourceRangesByFunction) {
    auto &ranges = rangeEntry.second;
    std::sort(ranges.begin(), ranges.end(), [](const InstructionSourceRange &left, const InstructionSourceRange &right) {
      if (left.beginIndex != right.beginIndex) {
        return left.beginIndex < right.beginIndex;
      }
      if (left.endIndex != right.endIndex) {
        return left.endIndex < right.endIndex;
      }
      if (left.line != right.line) {
        return left.line < right.line;
      }
      return left.column < right.column;
    });
  }

  size_t totalInstructionCount = 0;
  for (const auto &loweredFunction : input.outModule->functions) {
    totalInstructionCount += loweredFunction.instructions.size();
  }
  input.outModule->instructionSourceMap.clear();
  input.outModule->instructionSourceMap.reserve(totalInstructionCount);

  uint64_t nextInstructionDebugId = 1;
  for (auto &loweredFunction : input.outModule->functions) {
    uint32_t fallbackSourceLine = 0;
    uint32_t fallbackSourceColumn = 0;
    auto provenanceIt = input.functionSyntaxProvenanceByName->find(loweredFunction.name);
    if (provenanceIt != input.functionSyntaxProvenanceByName->end()) {
      fallbackSourceLine = provenanceIt->second.line;
      fallbackSourceColumn = provenanceIt->second.column;
    }
    const auto sourceRangesIt = input.instructionSourceRangesByFunction->find(loweredFunction.name);
    const std::vector<InstructionSourceRange> *sourceRanges =
        sourceRangesIt == input.instructionSourceRangesByFunction->end() ? nullptr : &sourceRangesIt->second;
    for (size_t instructionIndex = 0; instructionIndex < loweredFunction.instructions.size(); ++instructionIndex) {
      auto &instruction = loweredFunction.instructions[instructionIndex];
      if (nextInstructionDebugId > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        errorOut = "too many IR instructions for debug id metadata";
        return false;
      }
      instruction.debugId = static_cast<uint32_t>(nextInstructionDebugId);
      uint32_t sourceLine = fallbackSourceLine;
      uint32_t sourceColumn = fallbackSourceColumn;
      IrSourceMapProvenance sourceProvenance = IrSourceMapProvenance::SyntheticIr;
      if (sourceRanges != nullptr) {
        const InstructionSourceRange *bestRange = nullptr;
        for (const auto &range : *sourceRanges) {
          if (range.beginIndex > instructionIndex) {
            break;
          }
          if (instructionIndex < range.endIndex) {
            if (bestRange == nullptr) {
              bestRange = &range;
            } else {
              const size_t bestWidth = bestRange->endIndex - bestRange->beginIndex;
              const size_t width = range.endIndex - range.beginIndex;
              if (width < bestWidth || (width == bestWidth && range.beginIndex >= bestRange->beginIndex)) {
                bestRange = &range;
              }
            }
          }
        }
        if (bestRange != nullptr) {
          sourceLine = bestRange->line;
          sourceColumn = bestRange->column;
          sourceProvenance = bestRange->provenance;
        }
      }
      input.outModule->instructionSourceMap.push_back(
          {instruction.debugId, sourceLine, sourceColumn, sourceProvenance});
      ++nextInstructionDebugId;
    }
  }

  input.outModule->stringTable = std::move(*input.stringTable);
  return true;
}

} // namespace primec::ir_lowerer
