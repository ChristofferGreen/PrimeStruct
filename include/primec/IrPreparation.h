#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/Ir.h"
#include "primec/SemanticProduct.h"
#include "primec/IrValidation.h"
#include "primec/Options.h"

#include <string>
#include <string_view>
#include <vector>

namespace primec {

struct ExpandedSource;

enum class IrPreparationPhaseOwnership {
  CompilePipelineAstAndSemanticProduct,
  IrPreparationLoweredIr,
  IrPreparationValidatedIr,
  IrPreparationInlinedIr,
  CompilerAstStorage,
};

enum class IrPreparationPhaseAction {
  ValidatesOnly,
  CreatesOutput,
  MutatesOutput,
  ReleasesInputStorage,
};

struct IrPreparationPhaseManifestEntry {
  std::string_view name;
  IrPreparationPhaseOwnership inputOwnership =
      IrPreparationPhaseOwnership::CompilePipelineAstAndSemanticProduct;
  IrPreparationPhaseOwnership outputOwnership =
      IrPreparationPhaseOwnership::IrPreparationLoweredIr;
  IrPreparationPhaseAction action = IrPreparationPhaseAction::ValidatesOnly;
  bool optional = false;
  std::string_view requiredInputs;
  std::string_view invalidationNotes;
  std::string_view consumerNotes;
};

enum class IrPreparationFailureStage {
  None,
  Lowering,
  Validation,
  Inlining,
};

struct IrPreparationFailure {
  IrPreparationFailureStage stage = IrPreparationFailureStage::None;
  std::string message;
  DiagnosticSinkReport diagnosticInfo;
};

const std::vector<IrPreparationPhaseManifestEntry> &irPreparationPhaseManifest();

bool prepareIrModule(Program &program,
                     const SemanticProgram *semanticProgram,
                     const Options &options,
                     IrValidationTarget validationTarget,
                     IrModule &ir,
                     IrPreparationFailure &failure,
                     const ExpandedSource *expandedSource = nullptr);

} // namespace primec
