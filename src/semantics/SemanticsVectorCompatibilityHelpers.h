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

struct StdNamespacedVectorCompatibilityHelperState {
  bool hasDeclaredHelper = false;
  bool hasImportedHelper = false;

  [[nodiscard]] std::string classifyCountMapTargetDiagnosticMessage(
      bool mapTargetDetected,
      bool preferUnknownCallTarget) const {
    if (!mapTargetDetected) {
      return "";
    }
    if (preferUnknownCallTarget) {
      return vectorCompatibilityUnknownCallTargetDiagnostic("count");
    }
    if (!hasDeclaredHelper || hasImportedHelper) {
      return vectorCompatibilityRequiresVectorTargetDiagnostic("count");
    }
    return "";
  }

  [[nodiscard]] std::string classifyNonVectorCountTargetDiagnosticMessage(
      bool wrapperMapTarget,
      bool mapTargetDetected,
      bool preferUnknownCallTarget) const {
    if (wrapperMapTarget && !hasDeclaredHelper) {
      return vectorCompatibilityUnknownCallTargetDiagnostic("count");
    }
    const std::string mapTargetDiagnosticMessage =
        classifyCountMapTargetDiagnosticMessage(mapTargetDetected,
                                                preferUnknownCallTarget);
    if (!mapTargetDiagnosticMessage.empty()) {
      return mapTargetDiagnosticMessage;
    }
    return vectorCompatibilityRequiresVectorTargetDiagnostic("count");
  }

  [[nodiscard]] std::string classifyVectorLikeCountTargetDiagnosticMessage(
      bool vectorLikeTarget) const {
    if (vectorLikeTarget && !hasDeclaredHelper && !hasImportedHelper) {
      return vectorCompatibilityUnknownCallTargetDiagnostic("count");
    }
    return "";
  }
};

inline StdNamespacedVectorCompatibilityHelperState
makeStdNamespacedVectorCompatibilityHelperState(bool hasDeclaredHelper,
                                                bool hasImportedHelper) {
  return StdNamespacedVectorCompatibilityHelperState{
      hasDeclaredHelper, hasImportedHelper};
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
