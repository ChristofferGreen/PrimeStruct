#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/Ir.h"
#include "primec/IrValidation.h"
#include "primec/SemanticProduct.h"

namespace primec {

class IrLowerer {
 public:
  bool lower(const Program &program,
             const SemanticProgram *semanticProgram,
             const std::string &entryPath,
             const std::vector<std::string> &defaultEffects,
             const std::vector<std::string> &entryDefaultEffects,
             IrModule &out,
             std::string &error,
             DiagnosticSinkReport *diagnosticInfo = nullptr,
             IrValidationTarget validationTarget = IrValidationTarget::Native) const;
};

} // namespace primec
