#include "primec/IrPreparation.h"

#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrValidation.h"

namespace primec {

bool prepareIrModule(Program &program,
                     const SemanticProgram *semanticProgram,
                     const Options &options,
                     IrValidationTarget validationTarget,
                     IrModule &ir,
                     IrPreparationFailure &failure) {
  failure = {};
  std::string error;
  DiagnosticSink diagnosticSink(&failure.diagnosticInfo);
  diagnosticSink.reset();

  if (semanticProgram == nullptr) {
    failure.stage = IrPreparationFailureStage::Lowering;
    failure.message = "semantic product is required for IR preparation";
    diagnosticSink.setSummary(failure.message);
    return false;
  }

  IrLowerer lowerer;
  if (!lowerer.lower(program,
                     semanticProgram,
                     options.entryPath,
                     options.defaultEffects,
                     options.entryDefaultEffects,
                     ir,
                     error,
                     &failure.diagnosticInfo)) {
    failure.stage = IrPreparationFailureStage::Lowering;
    failure.message = std::move(error);
    return false;
  }

  if (!validateIrModule(ir, validationTarget, error)) {
    failure.stage = IrPreparationFailureStage::Validation;
    failure.message = std::move(error);
    diagnosticSink.setSummary(failure.message);
    return false;
  }

  if (options.inlineIrCalls) {
    if (!inlineIrModuleCalls(ir, error)) {
      failure.stage = IrPreparationFailureStage::Inlining;
      failure.message = std::move(error);
      diagnosticSink.setSummary(failure.message);
      return false;
    }
    if (!validateIrModule(ir, validationTarget, error)) {
      failure.stage = IrPreparationFailureStage::Validation;
      failure.message = std::move(error);
      diagnosticSink.setSummary(failure.message);
      return false;
    }
  }

  return true;
}

} // namespace primec
