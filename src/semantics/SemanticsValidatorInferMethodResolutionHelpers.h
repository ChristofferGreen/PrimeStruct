#pragma once

// Pure helpers of SemanticsValidator::resolveInferMethodCallPath (split out of
// SemanticsValidatorInferMethodResolution.cpp without changes, TODO-5385).
#include "SemanticsValidator.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace primec::semantics::inferMethodResolutionHelpers {

inline std::string receiverHelperFamilyLeaf(std::string_view resolvedType) {
  if (resolvedType.empty()) {
    return {};
  }
  const size_t slash = resolvedType.find_last_of('/');
  const size_t nameStart = slash == std::string_view::npos ? 0 : slash + 1;
  size_t nameEnd = resolvedType.size();
  const size_t specialization = resolvedType.find("__t", nameStart);
  const size_t overload = resolvedType.find("__ov", nameStart);
  const size_t templateStart = resolvedType.find('<', nameStart);
  if (specialization != std::string_view::npos) {
    nameEnd = std::min(nameEnd, specialization);
  }
  if (overload != std::string_view::npos) {
    nameEnd = std::min(nameEnd, overload);
  }
  if (templateStart != std::string_view::npos) {
    nameEnd = std::min(nameEnd, templateStart);
  }
  if (nameEnd <= nameStart) {
    return {};
  }
  return std::string(resolvedType.substr(nameStart, nameEnd - nameStart));
}

inline std::string normalizedTypeLeafName(std::string value) {
  value = normalizeBindingTypeName(value);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(value, base, argText) && !base.empty()) {
    value = base;
  }
  if (!value.empty() && value.front() == '/') {
    value.erase(value.begin());
  }
  const size_t slash = value.find_last_of('/');
  return slash == std::string::npos ? value : value.substr(slash + 1);
}

} // namespace primec::semantics::inferMethodResolutionHelpers
