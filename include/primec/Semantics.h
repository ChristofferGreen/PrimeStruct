#pragma once

#include <string>
#include <vector>

#include "primec/Ast.h"

namespace primec {

struct SemanticDiagnosticRelatedSpan {
  int line = 0;
  int column = 0;
  std::string label;
};

struct SemanticDiagnosticInfo {
  int line = 0;
  int column = 0;
  std::vector<SemanticDiagnosticRelatedSpan> relatedSpans;
};

class Semantics {
public:
  bool validate(Program &program,
                const std::string &entryPath,
                std::string &error,
                const std::vector<std::string> &defaultEffects,
                const std::vector<std::string> &entryDefaultEffects,
                const std::vector<std::string> &semanticTransforms = {},
                SemanticDiagnosticInfo *diagnosticInfo = nullptr) const;
};

} // namespace primec
