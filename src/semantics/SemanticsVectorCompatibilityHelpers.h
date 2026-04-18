#pragma once

#include <string>
#include <string_view>

namespace primec::semantics {

inline bool isVectorCompatibilityHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

enum class VectorCompatibilityCountMapTargetDiagnostic {
  None,
  UnknownCallTarget,
  RequiresVectorTarget,
};

inline VectorCompatibilityCountMapTargetDiagnostic
classifyStdNamespacedVectorCountMapTargetDiagnostic(
    bool mapTargetDetected,
    bool preferUnknownCallTarget,
    bool hasDeclaredHelper,
    bool hasImportedHelper) {
  if (!mapTargetDetected) {
    return VectorCompatibilityCountMapTargetDiagnostic::None;
  }
  if (preferUnknownCallTarget) {
    return VectorCompatibilityCountMapTargetDiagnostic::UnknownCallTarget;
  }
  if (!hasDeclaredHelper || hasImportedHelper) {
    return VectorCompatibilityCountMapTargetDiagnostic::RequiresVectorTarget;
  }
  return VectorCompatibilityCountMapTargetDiagnostic::None;
}

inline std::string vectorCompatibilityRequiresVectorTargetDiagnostic(
    std::string_view helperName) {
  return std::string(helperName) + " requires vector target";
}

inline std::string vectorCompatibilityUnknownCallTargetDiagnostic(
    std::string_view helperName) {
  return "unknown call target: /std/collections/vector/" +
         std::string(helperName);
}

} // namespace primec::semantics
