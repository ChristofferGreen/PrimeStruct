#include "primec/ir_lowerer/IrLowererErrorLocator.h"

#include "primec/support/DiagnosticNames.h"
#include "primec/support/SourceLocationMapper.h"

#include <unordered_map>

namespace primec::ir_lowerer {

void LoweringErrorLocator::noteFailure(const Expr &expr, const std::string &error) {
  if (error.empty() || expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return;
  }
  const bool sameFailure = hasSpan_ && error_ == error;
  if (sameFailure && spanInUserCode_) {
    return;
  }
  DiagnosticSpan span;
  span.line = expr.sourceLine;
  span.column = expr.sourceColumn;
  span.endLine = expr.sourceLine;
  span.endColumn = expr.sourceColumn;
  bool inUserCode = true;
  if (expandedSource_ != nullptr) {
    if (const std::optional<SourceUnitLocation> location =
            mapExpandedSourceLocation(*expandedSource_, span.line, span.column)) {
      inUserCode = location->unitKind == SourceUnitKind::Primary ||
                   location->unitKind == SourceUnitKind::Import;
    }
    span = mapDiagnosticSpanToSourceUnit(*expandedSource_, span);
  }
  if (sameFailure && !inUserCode) {
    return;
  }
  error_ = error;
  span_ = span;
  hasSpan_ = true;
  spanInUserCode_ = inUserCode;
}

std::optional<DiagnosticSpan> LoweringErrorLocator::spanOf(const std::string &error) const {
  if (!hasSpan_ || error_ != error) {
    return std::nullopt;
  }
  return span_;
}

std::string userFacingLoweringError(const std::string &error, const Program &program) {
  if (error.find("__t") == std::string::npos) {
    return error;
  }
  std::unordered_map<std::string, const Definition *> instances;
  for (const Definition &definition : program.definitions) {
    if (!definition.instanceOfTemplate.empty()) {
      instances.emplace(definition.fullPath, &definition);
    }
  }
  const TemplateInstanceLookup lookup =
      [&instances](const std::string &path) -> std::optional<TemplateInstanceName> {
    const auto found = instances.find(path);
    if (found == instances.end()) {
      return std::nullopt;
    }
    return TemplateInstanceName{found->second->instanceOfTemplate,
                                found->second->instanceTemplateArgs};
  };
  return userFacingDiagnosticText(error, lookup);
}

} // namespace primec::ir_lowerer
