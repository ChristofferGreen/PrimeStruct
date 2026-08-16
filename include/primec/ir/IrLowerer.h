#pragma once

#include <string>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/support/Diagnostics.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrValidation.h"
#include "primec/frontend/SemanticProduct.h"

namespace primec {

struct ExpandedSource;

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
             IrValidationTarget validationTarget = IrValidationTarget::Native,
             const ExpandedSource *expandedSource = nullptr) const;
};

} // namespace primec
