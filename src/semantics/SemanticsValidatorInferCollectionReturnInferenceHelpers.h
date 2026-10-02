#pragma once

// Helpers shared by the SemanticsValidatorInferCollectionReturnInference*.cpp units (split out of
// SemanticsValidatorInferCollectionReturnInference.cpp without changes, TODO-5384).
#include "SemanticsValidator.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::semantics {

namespace collectionReturnInferenceHelpers {

inline bool extractBuiltinSoaVectorElementTypeFromTypeTextForQueryInference(
    const std::string &typeText,
    std::string &elemTypeOut) {
  elemTypeOut.clear();
  const std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if ((base == "soa" || isExperimentalSoaVectorTypePath(base)) &&
      !argText.empty()) {
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    elemTypeOut = args.front();
    return true;
  }
  if ((base != "Reference" && base != "Pointer") || argText.empty()) {
    return false;
  }
  std::string wrappedBase;
  std::string wrappedArgText;
  if (!splitTemplateTypeName(argText, wrappedBase, wrappedArgText)) {
    return false;
  }
  wrappedBase = normalizeBindingTypeName(wrappedBase);
  if ((wrappedBase != "soa" && !isExperimentalSoaVectorTypePath(wrappedBase)) ||
      wrappedArgText.empty()) {
    return false;
  }
  std::vector<std::string> wrappedArgs;
  if (!splitTopLevelTemplateArgs(wrappedArgText, wrappedArgs) ||
      wrappedArgs.size() != 1) {
    return false;
  }
  elemTypeOut = wrappedArgs.front();
  return true;
}

} // namespace collectionReturnInferenceHelpers
} // namespace primec::semantics
