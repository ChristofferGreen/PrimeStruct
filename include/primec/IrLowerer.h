#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/Ir.h"

namespace primec {

class IrLowerer {
 public:
  bool lower(const Program &program,
             const std::string &entryPath,
             const std::vector<std::string> &defaultEffects,
             const std::vector<std::string> &entryDefaultEffects,
             IrModule &out,
             std::string &error,
             DiagnosticSinkReport *diagnosticInfo = nullptr) const;
};

} // namespace primec
