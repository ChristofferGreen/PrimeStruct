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
   bool lower(
       const Program &program,
       const SemanticProgram *semanticProgram,
       const std::string &entryPath,
       const std::vector<std::string> &defaultEffects,
       const std::vector<std::string> &entryDefaultEffects,
       IrModule &out,
       std::string &error,
       DiagnosticSinkReport *diagnosticInfo = nullptr,
       IrValidationTarget validationTarget = IrValidationTarget::Native,
       const ExpandedSource *expandedSource = nullptr,
       // --opt-report lines about lowering decisions (a spawned call that runs inline, and why).
       std::vector<std::string> *loweringNotes = nullptr,
       // `--emit=lib`: the paths of the public functions a library holds. Each is lowered as a function
       // with the address ABI whatever calls it, with what it reaches. `entryPath` names a stub that is lowered with
       // them and removed, so `out.entryIndex` is -1.
       const std::vector<std::string> *libraryRoots = nullptr) const;
};

} // namespace primec
