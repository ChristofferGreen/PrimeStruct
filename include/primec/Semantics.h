#pragma once

#include "primec/Ast.h"
#include "primec/Diagnostics.h"
#include "primec/SemanticProduct.h"

#include <vector>

namespace primec {

using SemanticDiagnosticRelatedSpan = DiagnosticRelatedSpan;
using SemanticDiagnosticRecord = DiagnosticSinkRecord;
using SemanticDiagnosticInfo = DiagnosticSinkReport;

struct SemanticProductBuildConfig {
  bool disableAllCollectors = false;
  bool collectorAllowlistSpecified = false;
  std::vector<std::string> collectorAllowlist;
};

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
                SemanticProgram *semanticProgramOut = nullptr,
                const SemanticProductBuildConfig *semanticProductBuildConfig = nullptr) const;
};

} // namespace primec
