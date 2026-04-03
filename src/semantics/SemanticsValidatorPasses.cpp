#include "SemanticsValidator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>

namespace primec::semantics {

bool SemanticsValidator::validateEntry() {
  auto failEntryDiagnostic = [&](std::string message, const Definition *def = nullptr) -> bool {
    error_ = std::move(message);
    if (def != nullptr) {
      captureDefinitionContext(*def);
    }
    return publishCurrentStructuredDiagnosticNow();
  };
  auto entryIt = defMap_.find(entryPath_);
  if (entryIt == defMap_.end()) {
    return failEntryDiagnostic("missing entry definition " + entryPath_);
  }
  const auto &entryParams = paramsByDef_[entryPath_];
  if (!entryParams.empty()) {
    if (entryParams.size() != 1) {
      return failEntryDiagnostic("entry definition must take a single array<string> parameter: " + entryPath_,
                                 entryIt->second);
    }
    const ParameterInfo &param = entryParams.front();
    if (param.binding.typeName != "array" || param.binding.typeTemplateArg != "string") {
      return failEntryDiagnostic("entry definition must take a single array<string> parameter: " + entryPath_,
                                 entryIt->second);
    }
    if (param.defaultExpr != nullptr) {
      return failEntryDiagnostic("entry parameter does not allow a default value: " + entryPath_, entryIt->second);
    }
  }
  return true;
}

} // namespace primec::semantics
