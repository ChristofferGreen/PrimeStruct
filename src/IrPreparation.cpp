#include "primec/IrPreparation.h"

#include "primec/IrInliner.h"
#include "primec/IrLowerer.h"
#include "primec/IrValidation.h"

namespace primec {

bool prepareIrModule(Program &program,
                     const Options &options,
                     IrValidationTarget validationTarget,
                     IrModule &ir,
                     IrPreparationFailure &failure) {
  failure = {};
  std::string error;

  IrLowerer lowerer;
  if (!lowerer.lower(program, options.entryPath, options.defaultEffects, options.entryDefaultEffects, ir, error)) {
    failure.stage = IrPreparationFailureStage::Lowering;
    failure.message = std::move(error);
    return false;
  }

  if (!validateIrModule(ir, validationTarget, error)) {
    failure.stage = IrPreparationFailureStage::Validation;
    failure.message = std::move(error);
    return false;
  }

  if (options.inlineIrCalls) {
    if (!inlineIrModuleCalls(ir, error)) {
      failure.stage = IrPreparationFailureStage::Inlining;
      failure.message = std::move(error);
      return false;
    }
    if (!validateIrModule(ir, validationTarget, error)) {
      failure.stage = IrPreparationFailureStage::Validation;
      failure.message = std::move(error);
      return false;
    }
  }

  return true;
}

} // namespace primec
