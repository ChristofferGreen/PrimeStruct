#pragma once

#include "primec/Ast.h"
#include "primec/SemanticsDefinitionPrepass.h"

#include <cstddef>
#include <string>
#include <string_view>
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

struct SemanticValidationExecutionDeclaration {
  std::string fullPath;
  std::size_t stableIndex = 0;
};

struct SemanticValidationExecutionSlice {
  std::vector<SemanticValidationExecutionDeclaration> executionsInStableOrder;
};

struct SemanticValidationPlan {
  DefinitionPrepassSnapshot definitionPrepass;
  SemanticValidationEntryMetadata entry;
  SemanticValidationImportSummary imports;
  SemanticValidationBuiltinCapabilityTables builtinCapabilityTables;
  SemanticValidationGraphLocalAutoInputs graphLocalAutoInputs;
  SemanticValidationExecutionSlice executionSlice;
};

enum class SemanticValidationPassKind {
  CoreCanonicalization,
  CompatibilityRewrite,
  TemplateMonomorphization,
  Validation,
  Publication,
};

enum class SemanticValidationPassOwnership {
  AstSyntax,
  SemanticCanonicalAst,
  SemanticProductFacts,
};

enum class SemanticValidationPassAction {
  MutatesAst,
  ValidatesOnly,
  PublishesFacts,
};

struct SemanticValidationPassManifestEntry {
  std::string_view name;
  SemanticValidationPassKind kind =
      SemanticValidationPassKind::CoreCanonicalization;
  SemanticValidationPassOwnership inputOwnership =
      SemanticValidationPassOwnership::AstSyntax;
  SemanticValidationPassOwnership outputOwnership =
      SemanticValidationPassOwnership::SemanticCanonicalAst;
  SemanticValidationPassAction action = SemanticValidationPassAction::MutatesAst;
  bool compatibilityRewrite = false;
  std::string_view description;
};

SemanticValidationPlan buildSemanticValidationPlan(const Program &program,
                                                   const std::string &entryPath);
const std::vector<SemanticValidationPassManifestEntry> &semanticValidationPassManifest();

} // namespace primec::semantics
