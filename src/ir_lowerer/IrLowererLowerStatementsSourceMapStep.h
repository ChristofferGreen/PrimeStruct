#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/Ir.h"

namespace primec::ir_lowerer {

struct InstructionSourceRange {
  size_t beginIndex = 0;
  size_t endIndex = 0;
  uint32_t line = 0;
  uint32_t column = 0;
  IrSourceMapProvenance provenance = IrSourceMapProvenance::CanonicalAst;
};

struct FunctionSyntaxProvenance {
  uint32_t line = 0;
  uint32_t column = 0;
};

struct LowerStatementsSourceMapStepInput {
  const std::unordered_map<std::string, FunctionSyntaxProvenance> *functionSyntaxProvenanceByName = nullptr;
  std::unordered_map<std::string, std::vector<InstructionSourceRange>> *instructionSourceRangesByFunction = nullptr;
  std::vector<std::string> *stringTable = nullptr;
  IrModule *outModule = nullptr;
};

bool runLowerStatementsSourceMapStep(const LowerStatementsSourceMapStepInput &input,
                                     std::string &errorOut);

} // namespace primec::ir_lowerer
