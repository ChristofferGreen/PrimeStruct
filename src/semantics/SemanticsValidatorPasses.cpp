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
    if (def != nullptr) {
      return failDefinitionDiagnostic(*def, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
  };
  const auto &entryMetadata = validationPlan_.entry;
  auto entryIt = entryMetadata.declared ? defMap_.find(entryMetadata.fullPath) : defMap_.end();
  if (entryIt == defMap_.end()) {
    return failEntryDiagnostic("missing entry definition " + entryMetadata.fullPath);
  }
  const auto &entryParams = paramsByDef_[entryMetadata.fullPath];
  if (!entryParams.empty()) {
    if (entryParams.size() != 1) {
      return failEntryDiagnostic("entry definition must take a single array<string> parameter: " +
                                     entryMetadata.fullPath,
                                 entryIt->second);
    }
    const ParameterInfo &param = entryParams.front();
    if (param.binding.typeName != "array" || param.binding.typeTemplateArg != "string") {
      return failEntryDiagnostic("entry definition must take a single array<string> parameter: " +
                                     entryMetadata.fullPath,
                                 entryIt->second);
    }
    if (param.defaultExpr != nullptr) {
      return failEntryDiagnostic(
          "entry parameter does not allow a default value: " + entryMetadata.fullPath,
          entryIt->second);
    }
  }
  return true;
}

} // namespace primec::semantics
