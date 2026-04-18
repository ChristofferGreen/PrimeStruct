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

inline bool isStdNamespacedVectorCompatibilityHelperPath(
    std::string_view path,
    std::string_view helperName) {
  return path.rfind("/std/collections/vector/" + std::string(helperName), 0) ==
         0;
}

inline bool isStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName) {
  return !isMethodCall &&
         isStdNamespacedVectorCompatibilityHelperPath(path, helperName);
}

inline bool isImportedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper) {
  return hasImportedHelper &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

inline bool isUnimportedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper) {
  return !hasImportedHelper &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

inline bool isUnavailableStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool helperAvailable) {
  return !helperAvailable &&
         isStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                      path,
                                                      helperName);
}

inline bool isImportedResolvedStdNamespacedVectorCompatibilityDirectCall(
    bool isMethodCall,
    std::string_view path,
    std::string_view helperName,
    bool hasImportedHelper,
    bool resolvedMethod,
    size_t argCount,
    std::string_view resolvedPath) {
  return !resolvedMethod &&
         argCount == 1 &&
         isImportedStdNamespacedVectorCompatibilityDirectCall(isMethodCall,
                                                              path,
                                                              helperName,
                                                              hasImportedHelper) &&
         isStdNamespacedVectorCompatibilityHelperPath(resolvedPath, helperName);
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
