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

  [[nodiscard]] bool lacksDeclaredHelper() const {
    return !hasDeclaredHelper;
  }

  [[nodiscard]] bool lacksVisibleHelper() const {
    return !hasDeclaredHelper && !hasImportedHelper;
  }
};

inline StdNamespacedVectorCompatibilityHelperState
makeStdNamespacedVectorCompatibilityHelperState(bool hasDeclaredHelper,
                                                bool hasImportedHelper) {
  return StdNamespacedVectorCompatibilityHelperState{
      hasDeclaredHelper, hasImportedHelper};
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

inline std::string vectorCompatibilityCountMapTargetDiagnosticMessage(
    VectorCompatibilityCountMapTargetDiagnostic diagnostic) {
  switch (diagnostic) {
  case VectorCompatibilityCountMapTargetDiagnostic::UnknownCallTarget:
    return vectorCompatibilityUnknownCallTargetDiagnostic("count");
  case VectorCompatibilityCountMapTargetDiagnostic::RequiresVectorTarget:
    return vectorCompatibilityRequiresVectorTargetDiagnostic("count");
  case VectorCompatibilityCountMapTargetDiagnostic::None:
    return "";
  }
  return "";
}

inline std::string classifyStdNamespacedVectorCountMapTargetDiagnosticMessage(
    bool mapTargetDetected,
    bool preferUnknownCallTarget,
    const StdNamespacedVectorCompatibilityHelperState &helperState) {
  return vectorCompatibilityCountMapTargetDiagnosticMessage(
      classifyStdNamespacedVectorCountMapTargetDiagnostic(
          mapTargetDetected, preferUnknownCallTarget,
          helperState.hasDeclaredHelper, helperState.hasImportedHelper));
}

} // namespace primec::semantics
