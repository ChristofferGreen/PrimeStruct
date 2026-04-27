#pragma once

#include "primec/Ast.h"
#include "primec/SemanticsDefinitionPrepass.h"

#include <cstddef>
#include <string>
#include <vector>

namespace primec::semantics {

struct SemanticValidationEntryMetadata {
  std::string fullPath;
  bool declared = false;
  std::size_t stableOrderOffset = 0;
  std::size_t stableIndex = 0;
};

struct SemanticValidationImportSummary {
  bool hasSourceImports = false;
  std::vector<std::string> sourceImportPaths;
  std::vector<std::string> programImportPaths;
  std::vector<std::string> directImportPaths;
  std::vector<std::string> transitiveImportPaths;
};

struct SemanticValidationBuiltinCapabilityTables {
  std::string effectsTransformName = "effects";
  std::string capabilitiesTransformName = "capabilities";
  bool allowIdentifierNamedEffects = true;
};

struct SemanticValidationGraphLocalAutoInputs {
  std::string entryPath;
  std::size_t definitionCount = 0;
  std::size_t executionCount = 0;
};

struct SemanticValidationPlan {
  DefinitionPrepassSnapshot definitionPrepass;
  SemanticValidationEntryMetadata entry;
  SemanticValidationImportSummary imports;
  SemanticValidationBuiltinCapabilityTables builtinCapabilityTables;
  SemanticValidationGraphLocalAutoInputs graphLocalAutoInputs;
};

SemanticValidationPlan buildSemanticValidationPlan(const Program &program,
                                                   const std::string &entryPath);

} // namespace primec::semantics
