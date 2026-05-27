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
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::semantics {
namespace {

bool isSoaSamePathHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "get" || helperName == "get_ref" ||
         helperName == "ref" || helperName == "ref_ref" ||
         helperName == "to" "_aos" || helperName == "to" "_aos_ref" ||
         helperName == "push" || helperName == "reserve";
}

std::string explicitOldSoaHelperPath(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  if (normalizedPrefix == "soa" "_vector" &&
      isSoaSamePathHelperName(normalizedName)) {
    return "/soa" "_vector/" + normalizedName;
  }
  constexpr std::string_view kOldExplicitPrefix = "soa" "_vector/";
  if (normalizedName.rfind(kOldExplicitPrefix, 0) != 0) {
    return "";
  }
  const std::string_view helperName = std::string_view(normalizedName).substr(kOldExplicitPrefix.size());
  if (!isSoaSamePathHelperName(helperName)) {
    return "";
  }
  return "/soa" "_vector/" + std::string(helperName);
}

std::string explicitCallPathForCandidate(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  if (!candidate.name.empty() && candidate.name.front() == '/') {
    return candidate.name;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() != '/') {
    namespacePrefix.insert(namespacePrefix.begin(), '/');
  }
  if (namespacePrefix.empty()) {
    return "/" + candidate.name;
  }
  return namespacePrefix + "/" + candidate.name;
}

bool hasExplicitDefinitionFamilyPath(
    const Program &program,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string_view path) {
  const std::string pathText(path);
  if (defMap.find(pathText) != defMap.end()) {
    return true;
  }
  const std::string templatedPrefix = pathText + "<";
  const std::string specializedPrefix = pathText + "__";
  for (const Definition &definition : program.definitions) {
    if (definition.fullPath == pathText ||
        definition.fullPath.rfind(templatedPrefix, 0) == 0 ||
        definition.fullPath.rfind(specializedPrefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataLocal() {
  return ::keyValueHelperSurfaceMetadataLocal();
}

std::string canonicalKeyValueHelperRootPathLocal() {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata == nullptr ? std::string{} : std::string(metadata->canonicalPath);
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  return canonicalCollectionHelperPath(metadata->id, helperName);
}

bool resolvePublishedKeyValueHelperMemberTokenLocal(
    std::string_view memberToken,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperMemberToken(memberToken,
                                                     metadata->id,
                                                     memberNameOut);
}

bool resolvePublishedKeyValueHelperResolvedPathLocal(
    std::string_view resolvedPath,
    std::string &memberNameOut) {
  memberNameOut.clear();
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         resolvePublishedCollectionHelperResolvedPath(resolvedPath,
                                                      metadata->id,
                                                      memberNameOut);
}

bool isCanonicalKeyValueHelperResolvedPathLocal(std::string path) {
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  std::string helperName;
  return resolvePublishedKeyValueHelperResolvedPathLocal(path, helperName);
}

bool isCanonicalMapCollectionTypeRootLocal(std::string_view typeName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  return metadata != nullptr &&
         trimLeadingSlash(typeName) == trimLeadingSlash(metadata->canonicalPath);
}

bool isSpecializedExperimentalKeyValueBackingPath(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", typeName) &&
         typeName.find("__") != std::string::npos;
}

} // namespace

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
    if ((base == "array" || base == "vector" || base == "soa" "_vector" || base == "Buffer") &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return "/" + base;
    }
    if (isExperimentalSoaVectorTypePath(base) &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return "/soa" "_vector";
    }
    if ((base == "Map" || isKeyValueCollectionTypeName(base) || base == "/map" ||
         isCanonicalMapCollectionTypeRootLocal(base)) &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 2) {
      return "/map";
    }
    normalizedType = base;
  }
  if (normalizedType == "/array" || normalizedType == "array") {
    return "/array";
  }
  if (normalizedType == "/vector" || normalizedType == "vector" ||
      trimLeadingSlash(normalizedType) ==
          trimLeadingSlash(canonicalVectorCompatibilityPrefixOrFallback())) {
    return "/vector";
  }
  if (normalizedType == "Vector" ||
      isLegacyExperimentalVectorCompatibilityTypePath(normalizedType) ||
      isLegacyExperimentalVectorCompatibilityTypePath("/" + normalizedType)) {
    return "/vector";
  }
  if (normalizedType == "Buffer" || normalizedType == "std/gfx/Buffer" || normalizedType == "/std/gfx/Buffer" ||
      normalizedType == "std/gfx/experimental/Buffer" || normalizedType == "/std/gfx/experimental/Buffer" ||
      normalizedType.rfind("/std/gfx/Buffer__", 0) == 0 || normalizedType.rfind("std/gfx/Buffer__", 0) == 0 ||
      normalizedType.rfind("/std/gfx/experimental/Buffer__", 0) == 0 ||
      normalizedType.rfind("std/gfx/experimental/Buffer__", 0) == 0) {
    return "/Buffer";
  }
  if (normalizedType == "/soa" "_vector" || normalizedType == "soa" "_vector" ||
      normalizedType == "Soa" "Vector" ||
      normalizedType == "/std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
      normalizedType == "std/collections/experimental" "_soa" "_vector/Soa" "Vector") {
    return "/soa" "_vector";
  }
  if (normalizedType.rfind("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0 ||
      normalizedType.rfind("std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0) {
    return "/soa" "_vector";
  }
  if (normalizedType == "Map" || isKeyValueCollectionTypeName(normalizedType) ||
      normalizedType == "/map" ||
      isCanonicalMapCollectionTypeRootLocal(normalizedType)) {
    return "/map";
  }
  if (isSpecializedExperimentalKeyValueBackingPath(normalizedType)) {
    return "/map";
  }
  if (normalizedType == "/string" || normalizedType == "string") {
    return "/string";
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
      if ((importPath == "/std/collections/internal_vector" ||
           importPath == "/std/collections/internal_vector/*") &&
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
  static const std::string Prefix = "/" + std::string("vector") + "/";
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
  if (helperName == "count_ref") {
    helperName = "count";
  } else if (helperName == "contains_ref") {
    helperName = "contains";
  } else if (helperName == "tryAt_ref") {
    helperName = "tryAt";
  } else if (helperName == "at_ref") {
    helperName = "at";
  } else if (helperName == "at_unsafe_ref") {
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
  if (helperName == "count_ref") {
    return definitionPathContains("/count_ref");
  }
  if (helperName == "contains") {
    return definitionPathContains("/Reference/contains") ||
           definitionPathContains("/contains_ref");
  }
  if (helperName == "contains_ref") {
    return definitionPathContains("/contains_ref");
  }
  if (helperName == "tryAt") {
    return definitionPathContains("/Reference/tryAt") ||
           definitionPathContains("/tryAt_ref");
  }
  if (helperName == "tryAt_ref") {
    return definitionPathContains("/tryAt_ref");
  }
  if (helperName == "at") {
    return definitionPathContains("/Reference/at") ||
           definitionPathContains("/at_ref");
  }
  if (helperName == "at_ref") {
    return definitionPathContains("/at_ref");
  }
  if (helperName == "at_unsafe") {
    return definitionPathContains("/Reference/at_unsafe") ||
           definitionPathContains("/at_unsafe_ref");
  }
  if (helperName == "at_unsafe_ref") {
    return definitionPathContains("/at_unsafe_ref");
  }
  if (helperName == "insert") {
    return definitionPathContains("/Reference/insert") ||
           definitionPathContains("/insert_ref");
  }
  if (helperName == "insert_ref") {
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

std::string SemanticsValidator::directKeyValueHelperCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
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
  const std::string resolvedPath = [&]() {
    const std::string resolved = resolveCalleePath(candidate);
    if (!resolved.empty()) {
      return resolved;
    }
    return explicitCallPathForCandidate(candidate);
  }();
  const std::string explicitPath = explicitCallPathForCandidate(candidate);
  std::string helperName;
  const bool resolvedCompatibilityHelper =
      resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
          resolvedPath, helperName);
  bool resolvedBareKeyValueHelper = false;
  std::string explicitSurfaceHelperName;
  const bool spellsCurrentKeyValueWrapperSurface =
      candidate.namespacePrefix.empty() &&
      resolvePublishedKeyValueHelperMemberTokenLocal(candidate.name,
                                                     explicitSurfaceHelperName) &&
      explicitSurfaceHelperName == helperName &&
      trimLeadingSlash(candidate.name) != helperName &&
      metadataBackedKeyValueHelperRootAliasMethodName(explicitPath).empty() &&
      !isCanonicalKeyValueHelperResolvedPathLocal(explicitPath);
  if (!resolvedCompatibilityHelper &&
      !resolveExplicitPublishedKeyValueHelperExprMemberName(
          candidate.name, candidate.namespacePrefix, helperName)) {
    if (candidate.namespacePrefix.empty() &&
        resolvePublishedKeyValueHelperMemberTokenLocal(candidate.name, helperName) &&
        isPublishedKeyValueBaseHelperName(helperName)) {
      resolvedBareKeyValueHelper = true;
    } else {
      return "";
    }
  }
  const std::string canonicalPath = canonicalKeyValueHelperPathLocal(helperName);
  std::string resolvedExperimentalHelperName;
  if (isCanonicalKeyValueHelperResolvedPathLocal(
          currentValidationState_.context.definitionPath) &&
      resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
      resolvePublishedKeyValueHelperResolvedPathLocal(resolvedPath,
                                                      resolvedExperimentalHelperName)) {
    return "";
  }
  if (resolvedCompatibilityHelper && spellsCurrentKeyValueWrapperSurface) {
    return "";
  }
  const std::string removedPath = rootedKeyValueCompatibilityHelperPath(helperName);
  if (removedPath.empty()) {
    return "";
  }
  if (hasExplicitDefinitionFamilyPath(program_, defMap_, removedPath) ||
      candidate.args.empty()) {
    return "";
  }
  if (!metadataBackedKeyValueHelperRootAliasMethodName(explicitPath).empty()) {
    return removedPath;
  }
  if (resolvedCompatibilityHelper &&
      matchesResolvedPath(resolvedPath, canonicalPath)) {
    return "";
  }
  auto canonicalAccessHelperReturnsStruct = [&]() {
    if (helperName != "at" && helperName != "at_unsafe" &&
        helperName != "at_ref" && helperName != "at_unsafe_ref") {
      return false;
    }
    const std::string canonicalPath = canonicalKeyValueHelperPathLocal(helperName);
    auto defIt = defMap_.find(canonicalPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string returnType = normalizeBindingTypeName(transform.templateArgs.front());
      if (!returnType.empty() && returnType.front() == '/') {
        returnType.erase(returnType.begin());
      }
      return !returnType.empty() && !isRootBuiltinName(returnType) &&
             returnType != "string" && returnType != "map" &&
             returnType != "vector" && returnType != "array";
    }
    return false;
  };
  if (canonicalAccessHelperReturnsStruct()) {
    return "";
  }
  auto resolveReceiverIndex = [&]() -> size_t {
    if (!hasNamedArguments(candidate.argNames)) {
      return 0;
    }
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        return i;
      }
    }
    return 0;
  };
  auto hasKeyValueReceiver = [&]() {
    const size_t receiverIndex = resolveReceiverIndex();
    return receiverIndex < candidate.args.size() &&
           resolveAnyKeyValueTarget(candidate.args[receiverIndex]);
  };
  if (resolvedBareKeyValueHelper) {
    if (hasKeyValueReceiver() &&
        !hasDeclaredDefinitionPath(canonicalPath) &&
        !hasImportedDefinitionPath(canonicalPath)) {
      return canonicalPath;
    }
    return "";
  }
  if (helperName == "at" || helperName == "at_unsafe") {
    return removedPath;
  }
  const size_t receiverIndex = resolveReceiverIndex();
  if (receiverIndex >= candidate.args.size() || !resolveAnyKeyValueTarget(candidate.args[receiverIndex])) {
    return "";
  }
  return removedPath;
}

std::string SemanticsValidator::explicitRemovedCollectionMethodPath(std::string_view rawMethodName,
                                                                    std::string_view namespacePrefix) const {
  std::string rawName = std::string(trimLeadingSlash(rawMethodName));
  std::string rawNamespace = std::string(trimLeadingSlash(namespacePrefix));
  auto declaredRootVectorAliasPath = [&]() -> std::string {
    std::string_view helperName = rawName;
    if (rawNamespace == "vector") {
      helperName = rawName;
    } else if (rawNamespace.empty() && helperName.rfind(unrootedVectorHelperPrefix(), 0) == 0) {
      helperName.remove_prefix(unrootedVectorHelperPrefix().size());
    } else {
      return "";
    }
    if (!isPublishedVectorMutatorHelperName(helperName)) {
      return "";
    }
    std::string aliasPath = rootedVectorHelperPath(helperName);
    return hasDefinitionPath(aliasPath) ? aliasPath : "";
  };
  if (std::string aliasPath = declaredRootVectorAliasPath(); !aliasPath.empty()) {
    return aliasPath;
  }

  RemovedCollectionHelperFamily family = RemovedCollectionHelperFamily::VectorLike;
  std::string_view helperName;
  bool preserveArrayPath = false;
  if (!resolveRemovedCollectionHelperReference(rawMethodName, namespacePrefix, family, helperName, preserveArrayPath)) {
    return "";
  }
  return removedCollectionMethodPath(family, helperName, preserveArrayPath);
}

std::string SemanticsValidator::methodRemovedCollectionCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
      candidate.args.empty()) {
    return "";
  }

  RemovedCollectionHelperFamily family = RemovedCollectionHelperFamily::VectorLike;
  std::string_view helperName;
  bool preserveArrayPath = false;
  if (!resolveRemovedCollectionHelperReference(
          candidate.name, candidate.namespacePrefix, family, helperName, preserveArrayPath)) {
    return "";
  }

  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  if (family == RemovedCollectionHelperFamily::Map) {
    const std::string removedPath = removedCollectionMethodPath(family, helperName, preserveArrayPath);
    if (removedPath.empty() ||
        hasExplicitDefinitionFamilyPath(program_, defMap_, removedPath)) {
      return "";
    }
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(candidate.args.front(), keyType, valueType) ? removedPath : "";
  }

  std::string elemType;
  if ((helperName == "at" || helperName == "at_unsafe") &&
      dispatchResolvers.resolveVectorTarget(candidate.args.front(), elemType)) {
    return "";
  }
  if (!dispatchResolvers.resolveVectorTarget(candidate.args.front(), elemType) &&
      !dispatchResolvers.resolveArrayTarget(candidate.args.front(), elemType) &&
      !dispatchResolvers.resolveSoaVectorTarget(candidate.args.front(), elemType)) {
    return "";
  }
  return removedCollectionMethodPath(family, helperName, preserveArrayPath);
}

bool SemanticsValidator::getVectorMutatorHelperName(const Expr &candidate,
                                                    std::string &helperNameOut) const {
  helperNameOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }

  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  if (isPublishedVectorMutatorHelperName(normalizedName)) {
    helperNameOut = normalizedName;
    return true;
  }
  const std::string oldExplicitSoaPath = explicitOldSoaHelperPath(candidate);
  if (!oldExplicitSoaPath.empty()) {
    helperNameOut = oldExplicitSoaPath.substr(oldExplicitSoaPath.find_last_of('/') + 1);
    return true;
  }
  auto canonicalSoaMutatorHelperName = [&]() -> std::string {
    std::string normalizedCanonicalName = normalizedName;
    std::string normalizedCanonicalPrefix =
        std::string(trimLeadingSlash(candidate.namespacePrefix));
    if (const size_t specializationSuffix = normalizedCanonicalName.find("__");
        specializationSuffix != std::string::npos) {
      normalizedCanonicalName.erase(specializationSuffix);
    }
    if (const size_t specializationSuffix = normalizedCanonicalPrefix.find("__");
        specializationSuffix != std::string::npos) {
      normalizedCanonicalPrefix.erase(specializationSuffix);
    }
    constexpr std::string_view kCanonicalSoaPrefix = "std/collections/" "soa" "_vector/";
    if (normalizedCanonicalPrefix == "std/collections/" "soa" "_vector" &&
        (normalizedCanonicalName == "push" || normalizedCanonicalName == "reserve")) {
      return normalizedCanonicalName;
    }
    if (normalizedCanonicalName.rfind(kCanonicalSoaPrefix, 0) != 0) {
      return {};
    }
    std::string helperName =
        normalizedCanonicalName.substr(kCanonicalSoaPrefix.size());
    if (helperName == "push" || helperName == "reserve") {
      return helperName;
    }
    return {};
  }();
  if (!canonicalSoaMutatorHelperName.empty()) {
    helperNameOut = canonicalSoaMutatorHelperName;
    return true;
  }

  std::string removedPath = explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(resolveCalleePath(candidate), "");
  }
  if (!isCanonicalVectorCompatibilityPath(removedPath) &&
      !isRootedVectorHelperPath(removedPath) &&
      removedPath.rfind("/array/", 0) != 0) {
    return false;
  }

  const std::string helperName = removedPath.substr(removedPath.find_last_of('/') + 1);
  if (removedPath.rfind("/array/", 0) == 0) {
    const std::string canonicalPath = canonicalVectorCompatibilityHelperPathOrFallback(helperName);
    if (hasDefinitionPath(canonicalPath) || hasImportedDefinitionPath(canonicalPath)) {
      return false;
    }
  }
  if (!isPublishedVectorMutatorHelperName(helperName)) {
    return false;
  }
  helperNameOut = helperName;
  return true;
}

std::string SemanticsValidator::getRemovedRootedVectorDirectCallPath(
    const Expr &candidate) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
    return "";
  }

  const std::string explicitPath = explicitCallPathForCandidate(candidate);
  if (!isRootedVectorHelperPath(explicitPath)) {
    return "";
  }

  std::string removedPath =
      explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(explicitPath, "");
  }
  if (!isRootedVectorHelperPath(removedPath)) {
    return "";
  }
  return (hasDefinitionPath(removedPath) || hasImportedDefinitionPath(removedPath))
             ? ""
             : removedPath;
}

std::string SemanticsValidator::getRemovedRootedVectorDirectCallDiagnostic(
    const Expr &candidate) const {
  const std::string removedPath = getRemovedRootedVectorDirectCallPath(candidate);
  if (removedPath.empty()) {
    return "";
  }
  return "unknown call target: " + removedPath;
}

bool SemanticsValidator::shouldPreserveRemovedCollectionHelperPath(const std::string &path) const {
  RemovedCollectionHelperFamily family = RemovedCollectionHelperFamily::VectorLike;
  std::string_view helperName;
  bool preserveArrayPath = false;
  if (!resolveRemovedCollectionHelperReference(path, "", family, helperName, preserveArrayPath)) {
    return false;
  }
  return !removedCollectionMethodPath(family, helperName, preserveArrayPath).empty();
}

bool SemanticsValidator::isUnnamespacedMapCountBuiltinFallbackCall(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyKeyValueTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveKeyValueTarget(target, keyType, valueType);
  };
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }
  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const bool spellsCount = normalized == "count";
  const bool resolvesCount = resolveCalleePath(candidate) == "/count";
  if (!spellsCount && !resolvesCount) {
    return false;
  }
  if (defMap_.find("/count") != defMap_.end() || candidate.args.empty()) {
    return false;
  }
  size_t receiverIndex = 0;
  if (hasNamedArguments(candidate.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < candidate.args.size(); ++i) {
      if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
          *candidate.argNames[i] == "values") {
        receiverIndex = i;
        foundValues = true;
        break;
      }
    }
    if (!foundValues) {
      receiverIndex = 0;
    }
  }
  if (receiverIndex >= candidate.args.size()) {
    return false;
  }
  return resolveAnyKeyValueTarget(candidate.args[receiverIndex]);
}

bool SemanticsValidator::resolveRemovedMapBodyArgumentTarget(const Expr &candidate,
                                                             const std::string &resolvedPath,
                                                             const std::vector<ParameterInfo> &params,
                                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                                             const BuiltinCollectionDispatchResolverAdapters &adapters,
                                                             std::string &targetPathOut) {
  targetPathOut.clear();
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyKeyValueTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveKeyValueTarget(target, keyType, valueType);
  };

  auto preferredRemovedKeyValueHelperPath = [&](std::string_view helperName) {
    return canonicalKeyValueHelperPathLocal(helperName);
  };

  if (candidate.kind != Expr::Kind::Call || candidate.name.empty() || candidate.args.empty()) {
    return false;
  }

  if (!candidate.isMethodCall) {
    std::string helperName;
    if (!resolveExplicitPublishedKeyValueHelperExprMemberName(
            candidate.name, "", helperName) &&
        !resolveCanonicalCompatibilityKeyValueHelperNameFromResolvedPath(
            resolvedPath, helperName)) {
      return false;
    }
    if (!isPublishedKeyValueBaseHelperName(helperName) ||
        defMap_.count("/" + helperName) > 0) {
      return false;
    }

    auto tryResolveReceiverIndex = [&](size_t index) -> bool {
      if (index >= candidate.args.size()) {
        return false;
      }
      if (!resolveAnyKeyValueTarget(candidate.args[index])) {
        return false;
      }
      targetPathOut = preferredRemovedKeyValueHelperPath(helperName);
      return true;
    };
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          foundValues = true;
          if (tryResolveReceiverIndex(i)) {
            return true;
          }
          break;
        }
      }
      if (!foundValues) {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (tryResolveReceiverIndex(i)) {
            return true;
          }
        }
      }
    } else {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (tryResolveReceiverIndex(i)) {
          return true;
        }
      }
    }
    return false;
  }

  std::string wrappedResolvedPath = resolvedPath;
  if (wrappedResolvedPath.rfind("Reference/", 0) == 0) {
    wrappedResolvedPath.erase(0, std::string("Reference/").size());
  } else if (wrappedResolvedPath.rfind("Pointer/", 0) == 0) {
    wrappedResolvedPath.erase(0, std::string("Pointer/").size());
  }
  std::string helperName;
  if (!resolveExplicitPublishedKeyValueHelperExprMemberName(
          wrappedResolvedPath, "", helperName)) {
    if (!resolvePublishedKeyValueHelperMemberTokenLocal(wrappedResolvedPath,
                                                   helperName)) {
      return false;
    }
  }
  if (!isPublishedKeyValueBaseHelperName(helperName)) {
    return false;
  }

  auto isWrappedKeyValueReceiverCall = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(receiverExpr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      return returnsKeyValueCollectionType(transform.templateArgs.front());
    }
    return false;
  };

  if (!(resolveAnyKeyValueTarget(candidate.args.front()) || isWrappedKeyValueReceiverCall(candidate.args.front()))) {
    return false;
  }

  targetPathOut = preferredRemovedKeyValueHelperPath(helperName);
  return true;
}


} // namespace primec::semantics
