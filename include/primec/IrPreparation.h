#pragma once

#include "primec/Ast.h"
#include "primec/Ir.h"
#include "primec/IrValidation.h"
#include "primec/Options.h"

#include <string>

namespace primec {

enum class IrPreparationFailureStage {
  None,
  Lowering,
  Validation,
  Inlining,
};

struct IrPreparationFailure {
  IrPreparationFailureStage stage = IrPreparationFailureStage::None;
  std::string message;
};

bool prepareIrModule(Program &program,
                     const Options &options,
                     IrValidationTarget validationTarget,
                     IrModule &ir,
                     IrPreparationFailure &failure);

} // namespace primec
