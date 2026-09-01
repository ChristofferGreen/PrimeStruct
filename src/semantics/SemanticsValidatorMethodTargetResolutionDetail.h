// Shared, internal-use-only free-function helpers used by several of the
// SemanticsValidator method-target resolver families (vector/array/soa,
// string, key-value, args-pack, struct/sum - see TODO-5270..5274 in
// docs/todo.md). These were originally private to one translation unit
// (an anonymous namespace in SemanticsValidatorExprMethodTargetResolution.cpp)
// but are used by resolveMethodTarget/resolveMethodTargetGenericFallback
// (which stayed in that file) as well as by resolver functions that moved
// into their own family-specific files, so they were promoted to a real,
// nested-namespace-scoped shared header instead of being duplicated.
#pragma once

#include <string>
#include <string_view>

namespace primec::semantics {
namespace method_target_detail {

bool isRemovedVectorCompatibilityHelper(std::string_view helperName);
bool isRemovedKeyValueCompatibilityHelper(std::string_view helperName);
std::string canonicalKeyValueHelperPathLocal(std::string_view helperName);
std::string canonicalKeyValueHelperNamespaceLocal();
bool resolveCanonicalKeyValueHelperNameFromSpelling(std::string path,
                                                     std::string &helperNameOut);
bool isKeyValueHelperImportAliasNamespaceForMethodTargets(
    std::string_view namespacePrefix);
std::string rootedKeyValueHelperAliasPathForMethodTargets(
    std::string_view helperName);
std::string rootAliasKeyValueHelperNameForMethodTargets(
    std::string_view rawPath, std::string_view namespacePrefix);
bool isRootedKeyValueHelperAliasPathForMethodTargets(std::string_view rawPath);
bool isCanonicalMapBuiltinMethodHelper(std::string_view helperName);
std::string experimentalKeyValueBackingLeafForMethodTargets(std::string typeName);
bool isUnspecializedExperimentalKeyValueBackingTypeForMethodTargets(
    std::string typeName);
bool isSpecializedExperimentalKeyValueBackingTypeForMethodTargets(
    std::string typeName);
bool isFileMethodName(std::string_view methodName);
bool isRetiredMaybeMutableHelperName(std::string_view methodName);
bool isMaybeSumTypePath(std::string_view typePath);

}  // namespace method_target_detail
}  // namespace primec::semantics
