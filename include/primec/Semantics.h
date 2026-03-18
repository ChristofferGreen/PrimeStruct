#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"

namespace primec {

using SemanticDiagnosticRelatedSpan = DiagnosticRelatedSpan;
using SemanticDiagnosticRecord = DiagnosticSinkRecord;
using SemanticDiagnosticInfo = DiagnosticSinkReport;

class Semantics {
public:
  bool validate(Program &program,
                const std::string &entryPath,
                std::string &error,
                const std::vector<std::string> &defaultEffects,
                const std::vector<std::string> &entryDefaultEffects,
                const std::vector<std::string> &semanticTransforms = {},
                SemanticDiagnosticInfo *diagnosticInfo = nullptr,
                bool collectDiagnostics = false,
                const std::string &typeResolver = "graph") const;
};

} // namespace primec
