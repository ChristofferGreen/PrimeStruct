#pragma once

// Helpers shared by the SemanticsValidatorStatementBindings*.cpp units (split out of
// SemanticsValidatorStatementBindings.cpp without changes, TODO-5384).
#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_set>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::semantics {
namespace statementBindingsHelpers {

inline bool isSoaFieldViewBindingType(const BindingInfo &binding) {
  return isSoaFieldViewTypePath(binding.typeName);
}

inline bool isBorrowTrackedBindingType(const BindingInfo &binding) {
  return binding.typeName == "Reference" || isSoaFieldViewBindingType(binding) ||
         (binding.typeName == "auto" && !binding.referenceRoot.empty()) || binding.isContainerView;
}

inline bool isExperimentalSoaColumnBindingType(const BindingInfo &binding) {
  std::string normalized = normalizeBindingTypeName(binding.typeName);
  if (normalized.empty()) {
    return false;
  }
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalized, base, arg)) {
    normalized = normalizeBindingTypeName(base);
  }
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "SoaColumn" ||
         normalized == collection_paths::memberPathBare(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
         normalized.rfind(
             collection_paths::specializedTypePrefixBare(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) == 0;
}

inline std::string referenceRootForBorrowBinding(const std::string &bindingName, const BindingInfo &binding) {
  if (!isBorrowTrackedBindingType(binding)) {
    return "";
  }
  if (!binding.referenceRoot.empty()) {
    return binding.referenceRoot;
  }
  return bindingName;
}

inline std::string bindingTypeTextForTypeof(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

inline std::string formatPathListForTypeof(const std::vector<std::string> &paths) {
  std::string out;
  for (size_t index = 0; index < paths.size(); ++index) {
    if (index == 0) {
      out += paths[index];
    } else if (index + 1 == paths.size()) {
      out += " and " + paths[index];
    } else {
      out += ", " + paths[index];
    }
  }
  return out;
}

} // namespace statementBindingsHelpers
} // namespace primec::semantics
