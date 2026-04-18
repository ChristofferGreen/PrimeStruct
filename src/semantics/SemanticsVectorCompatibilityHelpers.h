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

inline std::string vectorCompatibilityCountMapTargetDiagnosticMessage(
    VectorCompatibilityCountMapTargetDiagnostic diagnostic);

struct StdNamespacedVectorCompatibilityHelperState {
  bool hasDeclaredHelper = false;
  bool hasImportedHelper = false;

  [[nodiscard]] bool lacksDeclaredHelper() const {
    return !hasDeclaredHelper;
  }

  [[nodiscard]] bool lacksVisibleHelper() const {
    return !hasDeclaredHelper && !hasImportedHelper;
  }

  [[nodiscard]] bool rejectsVectorLikeTargetWithoutVisibleHelper(
      bool vectorLikeTarget) const {
    return vectorLikeTarget && lacksVisibleHelper();
  }

  [[nodiscard]] bool rejectsWrapperMapTargetWithoutDeclaredHelper(
      bool wrapperMapTarget) const {
    return wrapperMapTarget && lacksDeclaredHelper();
  }

  [[nodiscard]] VectorCompatibilityCountMapTargetDiagnostic
  classifyCountMapTargetDiagnostic(bool mapTargetDetected,
                                   bool preferUnknownCallTarget) const {
    if (!mapTargetDetected) {
      return VectorCompatibilityCountMapTargetDiagnostic::None;
    }
    if (preferUnknownCallTarget) {
      return VectorCompatibilityCountMapTargetDiagnostic::UnknownCallTarget;
    }
    if (lacksDeclaredHelper() || hasImportedHelper) {
      return VectorCompatibilityCountMapTargetDiagnostic::RequiresVectorTarget;
    }
    return VectorCompatibilityCountMapTargetDiagnostic::None;
  }

  [[nodiscard]] std::string classifyCountMapTargetDiagnosticMessage(
      bool mapTargetDetected,
      bool preferUnknownCallTarget) const {
    return vectorCompatibilityCountMapTargetDiagnosticMessage(
        classifyCountMapTargetDiagnostic(mapTargetDetected,
                                         preferUnknownCallTarget));
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

} // namespace primec::semantics
