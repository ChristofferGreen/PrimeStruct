#pragma once

#include "primec/ast/Ast.h"
#include "primec/support/Diagnostics.h"
#include "primec/ir/Ir.h"
#include "primec/frontend/SemanticProduct.h"
#include "primec/ir/IrValidation.h"
#include "primec/support/Options.h"

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
