// soa-surface-audit: exempt
#include "SemanticsValidator.h"

#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CompileArena.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorInferCollectionCompatibilityHelpers.h"

namespace primec::semantics {
using namespace collectionCompatibilityHelpers;

std::string SemanticsValidator::normalizeCollectionTypePath(const std::string &typePath) const {
  std::string normalizedType = normalizeBindingTypeName(typePath);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    base = normalizeBindingTypeName(base);
    std::vector<std::string> args;
    if ((base == "Reference" || base == "Pointer") &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return normalizeCollectionTypePath(args.front());
    }
    if ((base == "array" || base == "vector" || base == "soa" || base == "Buffer") &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return "/" + base;
    }
    if (isExperimentalSoaVectorTypePath(base) &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return collection_helpers::kRootedSoa;
    }
    if ((isKeyValueSurfaceTypeName(base) || collection_helpers::isCollectionFamilyRoot(base, collection_helpers::CollectionFamily::Map) ||
         isCanonicalMapCollectionTypeRootLocal(base)) &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 2) {
      return collection_helpers::kRootedMap;
    }
    normalizedType = base;
  }
  if (collection_helpers::isCollectionFamilyRoot(normalizedType, collection_helpers::CollectionFamily::Array) || normalizedType == "array") {
    return collection_helpers::kRootedArray;
  }
  if (collection_helpers::isCollectionFamilyRoot(normalizedType, collection_helpers::CollectionFamily::Vector) || normalizedType == "vector" ||
      trimLeadingSlash(normalizedType) ==
          trimLeadingSlash(canonicalVectorCompatibilityPrefixOrFallback())) {
    return collection_helpers::kRootedVector;
  }
  if (normalizedType == "Vector" ||
      isLegacyExperimentalVectorCompatibilityTypePath(normalizedType) ||
      isLegacyExperimentalVectorCompatibilityTypePath("/" + normalizedType)) {
    return collection_helpers::kRootedVector;
  }
  if (normalizedType == "Buffer" || normalizedType == "std/gfx/Buffer" || normalizedType == "/std/gfx/Buffer" ||
      normalizedType == "std/gfx/experimental/Buffer" || normalizedType == "/std/gfx/experimental/Buffer" ||
      normalizedType.rfind("/std/gfx/Buffer__", 0) == 0 || normalizedType.rfind("std/gfx/Buffer__", 0) == 0 ||
      normalizedType.rfind("/std/gfx/experimental/Buffer__", 0) == 0 ||
      normalizedType.rfind("std/gfx/experimental/Buffer__", 0) == 0) {
    return "/Buffer";
  }
  if (collection_helpers::isCollectionFamilyRoot(normalizedType, collection_helpers::CollectionFamily::Soa) || normalizedType == "soa" ||
      normalizedType == "SoaVector" ||
      normalizedType == collection_paths::memberPath(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
      normalizedType == collection_paths::memberPathBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName)) {
    return collection_helpers::kRootedSoa;
  }
  if (normalizedType.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0 ||
      normalizedType.rfind(collection_paths::specializedTypePrefixBare(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0) {
    return collection_helpers::kRootedSoa;
  }
  if (isKeyValueSurfaceTypeName(normalizedType) ||
      collection_helpers::isCollectionFamilyRoot(normalizedType, collection_helpers::CollectionFamily::Map) ||
      isCanonicalMapCollectionTypeRootLocal(normalizedType)) {
    return collection_helpers::kRootedMap;
  }
  if (isSpecializedExperimentalKeyValueBackingPath(normalizedType)) {
    return collection_helpers::kRootedMap;
  }
  if (collection_helpers::isCollectionFamilyRoot(normalizedType, collection_helpers::CollectionFamily::String) || normalizedType == "string") {
    return collection_helpers::kRootedString;
  }
  return "";
}

bool SemanticsValidator::hasImportedDefinitionPath(const std::string &path) const {
  std::string canonicalPath = path;
  const size_t suffix = canonicalPath.find("__");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  if (canonicalPath.rfind("/File/", 0) == 0 || canonicalPath.rfind("/FileError/", 0) == 0) {
    canonicalPath.insert(0, "/std/file");
  }
  auto isImportedBy = [&](const std::vector<std::string> &importPaths) {
    for (const auto &importPath : importPaths) {
      if (importPath == canonicalPath) {
        return true;
      }
      if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(importPath);
          metadata != nullptr &&
          metadata->shape != StdlibSurfaceShape::ConstructorFamily &&
          canonicalPath.rfind(std::string(metadata->canonicalPath) + "/", 0) == 0) {
        return true;
      }
      if (importPath == canonicalVectorCompatibilityPrefixOrFallback() &&
          isCanonicalVectorCompatibilityPath(canonicalPath)) {
        return true;
      }
      if ((importPath == collection_paths::moduleRoot(collection_paths::kVectorFolder) ||
           importPath ==
               collection_paths::modulePrefix(collection_paths::kVectorFolder) + "*") &&
          isCanonicalVectorCompatibilityPath(canonicalPath)) {
        return true;
      }
      if (importPath == canonicalKeyValueHelperRootPathLocal() &&
          canonicalPath.rfind(importPath + "/", 0) == 0) {
        return true;
      }
      if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
        const std::string prefix = importPath.substr(0, importPath.size() - 2);
        if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
          return true;
        }
      }
    }
    return false;
  };
  const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
  if (isImportedBy(importPaths)) {
    return true;
  }
  if (!program_.sourceImports.empty() &&
      currentValidationState_.context.definitionPath.rfind("/std/", 0) == 0 &&
      isImportedBy(program_.imports)) {
    return true;
  }
  return false;
}

bool SemanticsValidator::hasDefinitionPath(const std::string &path) const {
  std::string canonicalPath = path;
  const size_t suffix = canonicalPath.find("__");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  const std::string templatedPrefix = canonicalPath + "<";
  for (const auto &[resolvedPath, definition] : defMap_) {
    (void)definition;
    if (matchesResolvedPath(resolvedPath, canonicalPath) ||
        resolvedPath.rfind(templatedPrefix, 0) == 0) {
      return true;
    }
  }
  for (const auto &definition : program_.definitions) {
    if (matchesResolvedPath(definition.fullPath, canonicalPath) ||
        definition.fullPath.rfind(templatedPrefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::typeHasCollectionCategoryTrait(
    const std::string &typeName,
    const std::string &namespacePrefix,
    std::string_view traitName) const {
  if (traitName != "Collection" && traitName != "KeyValue") {
    return false;
  }
  std::string normalizedType = normalizeBindingTypeName(typeName);
  if (normalizedType.empty()) {
    return false;
  }
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    normalizedType = normalizeBindingTypeName(base);
  }
  const std::string lookupNamespace = [&]() -> std::string {
    if (!namespacePrefix.empty()) {
      return namespacePrefix;
    }
    const std::string &definitionPath = currentValidationState_.context.definitionPath;
    const size_t slash = definitionPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return {};
    }
    return definitionPath.substr(0, slash);
  }();
  std::string resolvedType = resolveTypePath(normalizedType, lookupNamespace);
  if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
    auto importIt = importAliases_.find(normalizedType);
    if (importIt != importAliases_.end()) {
      resolvedType = importIt->second;
    }
  }
  if (resolvedType.empty()) {
    return false;
  }
  auto defIt = defMap_.find(resolvedType);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    const size_t specializationSuffix = resolvedType.find("__");
    if (specializationSuffix != std::string::npos) {
      defIt = defMap_.find(resolvedType.substr(0, specializationSuffix));
    }
  }
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name == "collection_type" && traitName == "Collection") {
      return true;
    }
    if (transform.name == "key_value_type") {
      return true;
    }
  }
  return false;
}

std::string SemanticsValidator::preferredExperimentalKeyValueHelperTarget(
    std::string_view helperName) const {
  const std::string prefix = experimentalCollectionConstructorRootLocal("map");
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return std::string(helperName);
  }
  std::string experimentalPath = preferredPublishedCollectionLoweringPath(
      helperName, metadata->id, prefix);
  if (experimentalPath.empty()) {
    return std::string(helperName);
  }
  experimentalPath.erase(0, prefix.size());
  return experimentalPath;
}

std::string SemanticsValidator::preferredCanonicalExperimentalKeyValueHelperTarget(
    std::string_view helperName) const {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return experimentalCollectionConstructorPathLocal("map", helperName);
  }
  const std::string experimentalPath = preferredPublishedCollectionLoweringPath(
      helperName,
      metadata->id,
      experimentalCollectionConstructorRootLocal("map"));
  if (experimentalPath.empty()) {
    return experimentalCollectionConstructorPathLocal("map", helperName);
  }
  return experimentalPath;
}

std::string SemanticsValidator::preferredCanonicalExperimentalVectorHelperTarget(
    std::string_view helperName) const {
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return legacyExperimentalVectorCompatibilityPrefix() +
           std::string(helperName);
  }
  const std::string experimentalPath = preferredPublishedCollectionLoweringPath(
      helperName,
      metadata->id,
      legacyExperimentalVectorCompatibilityPrefix());
  if (experimentalPath.empty()) {
    return legacyExperimentalVectorCompatibilityPrefix() +
           std::string(helperName);
  }
  return experimentalPath;
}

std::string_view SemanticsValidator::rootedVectorHelperPrefix() const {
  // TODO-5235: built via systemHeapValue() so this magic static's backing
  // memory is never arena-allocated - see docs/CompilerArenaAllocator.md.
  static const std::string Prefix =
      primec::systemHeapValue([] { return "/" + std::string("vector") + "/"; });
  return Prefix;
}

std::string_view SemanticsValidator::unrootedVectorHelperPrefix() const {
  return rootedVectorHelperPrefix().substr(1);
}

std::string SemanticsValidator::rootedVectorHelperPath(
    std::string_view helperName) const {
  return std::string(rootedVectorHelperPrefix()) + std::string(helperName);
}

bool SemanticsValidator::isRootedVectorHelperPath(
    std::string_view path) const {
  return path.rfind(rootedVectorHelperPrefix(), 0) == 0;
}

bool SemanticsValidator::isUnrootedVectorHelperPath(
    std::string_view path) const {
  return path.rfind(unrootedVectorHelperPrefix(), 0) == 0;
}

std::string_view SemanticsValidator::stripRootedVectorHelperPrefix(
    std::string_view path) const {
  return path.substr(rootedVectorHelperPrefix().size());
}

std::string_view SemanticsValidator::stripUnrootedVectorHelperPrefix(
    std::string_view path) const {
  return path.substr(unrootedVectorHelperPrefix().size());
}

std::string SemanticsValidator::specializedExperimentalVectorHelperTarget(
    std::string_view helperName,
    const std::string &elemType) const {
  auto fnv1a64 = [](const std::string &text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
      hash ^= static_cast<uint64_t>(ch);
      hash *= 1099511628211ULL;
    }
    return hash;
  };
  auto stripWhitespace = [](const std::string &text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
      if (!std::isspace(ch)) {
        out.push_back(static_cast<char>(ch));
      }
    }
    return out;
  };
  const std::string currentNamespacePrefix = [&]() -> std::string {
    if (currentValidationState_.context.definitionPath.empty()) {
      return {};
    }
    const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return {};
    }
    return currentValidationState_.context.definitionPath.substr(0, slash);
  }();
  std::function<std::string(const std::string &)> canonicalizeTypeText =
      [&](const std::string &typeText) -> std::string {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return normalizedType;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return normalizedType;
      }
      for (std::string &arg : args) {
        arg = canonicalizeTypeText(arg);
      }
      std::string canonicalBase = normalizeBindingTypeName(base);
      if (!canonicalBase.empty() && canonicalBase.front() != '/' &&
          std::isupper(static_cast<unsigned char>(canonicalBase.front()))) {
        std::string resolved = resolveStructTypePath(canonicalBase, currentNamespacePrefix, structNames_);
        if (resolved.empty()) {
          resolved = resolveTypePath(canonicalBase, currentNamespacePrefix);
        }
        if (!resolved.empty()) {
          canonicalBase = resolved;
        }
      }
      return canonicalBase + "<" + joinTemplateArgs(args) + ">";
    }
    if (!normalizedType.empty() && normalizedType.front() != '/' &&
        std::isupper(static_cast<unsigned char>(normalizedType.front()))) {
      std::string resolved = resolveStructTypePath(normalizedType, currentNamespacePrefix, structNames_);
      if (resolved.empty()) {
        resolved = resolveTypePath(normalizedType, currentNamespacePrefix);
      }
      if (!resolved.empty()) {
        return resolved;
      }
    }
    return normalizedType;
  };

  const std::string basePath = preferredCanonicalExperimentalVectorHelperTarget(helperName);
  std::ostringstream specializedPath;
  specializedPath << basePath
                  << "__t"
                  << std::hex
                  << fnv1a64(stripWhitespace(joinTemplateArgs({canonicalizeTypeText(elemType)})));
  if (defMap_.count(specializedPath.str()) > 0) {
    return specializedPath.str();
  }
  const std::string canonicalElemType = canonicalizeTypeText(elemType);
  const std::string specializationPrefix = basePath + "__t";
  for (const auto &[path, params] : paramsByDef_) {
    if (path.rfind(specializationPrefix, 0) != 0 || params.empty()) {
      continue;
    }
    std::string candidateElemType;
    if (!extractCollectionVectorElementType(params.front().binding, candidateElemType)) {
      continue;
    }
    if (canonicalizeTypeText(candidateElemType) == canonicalElemType) {
      return path;
    }
  }
  return basePath;
}

std::string SemanticsValidator::categoryCollectionVectorHelperTarget(
    std::string_view helperName,
    const std::string &elemType) const {
  return specializedExperimentalVectorHelperTarget(helperName, elemType);
}

bool SemanticsValidator::canonicalExperimentalVectorHelperPath(
    const std::string &resolvedPath,
    std::string &canonicalPathOut,
    std::string &helperNameOut) const {
  helperNameOut.clear();
  std::string normalizedPath = resolvedPath;
  if (!normalizedPath.empty() && normalizedPath.front() != '/' &&
      normalizedPath.find('/') == std::string::npos) {
    if (isVectorCompatibilityHelperName(normalizedPath)) {
      helperNameOut = normalizedPath;
    }
  } else {
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
    resolveCanonicalVectorHelperNameFromResolvedPath(normalizedPath,
                                                     helperNameOut);
  }
  if (helperNameOut.empty() || helperNameOut == "vector") {
    canonicalPathOut.clear();
    helperNameOut.clear();
    return false;
  }
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    canonicalPathOut.clear();
    return false;
  }
  canonicalPathOut = canonicalCollectionHelperPath(metadata->id, helperNameOut);
  return !canonicalPathOut.empty();
}

bool SemanticsValidator::canonicalExperimentalKeyValueHelperPath(
    const std::string &resolvedPath,
    std::string &canonicalPathOut,
    std::string &helperNameOut) const {
  if (!resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
          resolvedPath, helperNameOut)) {
    canonicalPathOut.clear();
    helperNameOut.clear();
    return false;
  }
  canonicalPathOut = canonicalKeyValueHelperPathLocal(helperNameOut);
  return !canonicalPathOut.empty();
}

bool SemanticsValidator::canonicalizeExperimentalKeyValueHelperResolvedPath(
    const std::string &resolvedPath,
    std::string &canonicalPathOut) const {
  canonicalPathOut.clear();
  if (resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) != 0) {
    return false;
  }
  std::string helperName;
  if (!resolvePublishedKeyValueHelperResolvedPathLocal(resolvedPath, helperName)) {
    return false;
  }
  if (helperName == collection_helpers::kCountRef) {
    helperName = "count";
  } else if (helperName == collection_helpers::kContainsRef) {
    helperName = "contains";
  } else if (helperName == collection_helpers::kTryAtRef) {
    helperName = "tryAt";
  } else if (helperName == collection_helpers::kAtRef) {
    helperName = "at";
  } else if (helperName == collection_helpers::kAtUnsafeRef) {
    helperName = "at_unsafe";
  }
  canonicalPathOut = canonicalKeyValueHelperPathLocal(helperName);
  return !canonicalPathOut.empty();
}

bool SemanticsValidator::shouldLogicalCanonicalizeDefinedExperimentalKeyValueHelperPath(
    const std::string &resolvedPath) const {
  if (resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) != 0) {
    return false;
  }
  const std::string &definitionPath =
      currentValidationState_.context.definitionPath;
  return isCanonicalKeyValueHelperResolvedPathLocal(definitionPath);
}

bool SemanticsValidator::shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const {
  auto definitionPathContains = [&](std::string_view needle) {
    return currentValidationState_.context.definitionPath.find(std::string(needle)) !=
           std::string::npos;
  };
  // Wrapper validation depends on the current implementation body being visited,
  // so this stays as an explicit definition-path rule rather than a surface table.
  if (helperName == "count") {
    return definitionPathContains("/Reference/count") ||
           definitionPathContains("/count_ref");
  }
  if (helperName == collection_helpers::kCountRef) {
    return definitionPathContains("/count_ref");
  }
  if (helperName == "contains") {
    return definitionPathContains("/Reference/contains") ||
           definitionPathContains("/contains_ref");
  }
  if (helperName == collection_helpers::kContainsRef) {
    return definitionPathContains("/contains_ref");
  }
  if (helperName == "tryAt") {
    return definitionPathContains("/Reference/tryAt") ||
           definitionPathContains("/tryAt_ref");
  }
  if (helperName == collection_helpers::kTryAtRef) {
    return definitionPathContains("/tryAt_ref");
  }
  if (helperName == "at") {
    return definitionPathContains("/Reference/at") ||
           definitionPathContains("/at_ref");
  }
  if (helperName == collection_helpers::kAtRef) {
    return definitionPathContains("/at_ref");
  }
  if (helperName == "at_unsafe") {
    return definitionPathContains("/Reference/at_unsafe") ||
           definitionPathContains("/at_unsafe_ref");
  }
  if (helperName == collection_helpers::kAtUnsafeRef) {
    return definitionPathContains("/at_unsafe_ref");
  }
  if (helperName == "insert") {
    return definitionPathContains("/Reference/insert") ||
           definitionPathContains("/insert_ref");
  }
  if (helperName == collection_helpers::kInsertRef) {
    return definitionPathContains("/insert_ref");
  }
  return false;
}

std::string SemanticsValidator::keyValueNamespacedMethodCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
      candidate.args.empty()) {
    return "";
  }
  std::string helperName;
  if (!resolveExplicitPublishedKeyValueHelperExprMemberName(
          candidate.name, candidate.namespacePrefix, helperName)) {
    return "";
  }
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyKeyValueTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveKeyValueTarget(target, keyType, valueType);
  };
  const std::string removedPath = rootedKeyValueCompatibilityHelperPath(helperName);
  if (removedPath.empty()) {
    return "";
  }
  if (hasExplicitDefinitionFamilyPath(program_, defMap_, removedPath)) {
    return "";
  }
  if (!resolveAnyKeyValueTarget(candidate.args.front())) {
    return "";
  }
  return removedPath;
}

} // namespace primec::semantics
