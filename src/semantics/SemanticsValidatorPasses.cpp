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
  auto entryIt = defMap_.find(entryPath_);
  if (entryIt == defMap_.end()) {
    error_ = "missing entry definition " + entryPath_;
    return false;
  }
  const auto &entryParams = paramsByDef_[entryPath_];
  if (!entryParams.empty()) {
    if (entryParams.size() != 1) {
      error_ = "entry definition must take a single array<string> parameter: " + entryPath_;
      return false;
    }
    const ParameterInfo &param = entryParams.front();
    if (param.binding.typeName != "array" || param.binding.typeTemplateArg != "string") {
      error_ = "entry definition must take a single array<string> parameter: " + entryPath_;
      return false;
    }
    if (param.defaultExpr != nullptr) {
      error_ = "entry parameter does not allow a default value: " + entryPath_;
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
