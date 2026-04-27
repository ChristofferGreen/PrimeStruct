#include "SemanticsValidator.h"

#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
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
         helperName == "to_aos" || helperName == "to_aos_ref" ||
         helperName == "push" || helperName == "reserve";
}

std::string explicitOldSoaHelperPath(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  if (normalizedPrefix == "soa_vector" &&
      isSoaSamePathHelperName(normalizedName)) {
    return "/soa_vector/" + normalizedName;
  }
  constexpr std::string_view kOldExplicitPrefix = "soa_vector/";
  if (normalizedName.rfind(kOldExplicitPrefix, 0) != 0) {
    return "";
  }
  const std::string_view helperName = std::string_view(normalizedName).substr(kOldExplicitPrefix.size());
  if (!isSoaSamePathHelperName(helperName)) {
    return "";
  }
  return "/soa_vector/" + std::string(helperName);
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
    if ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer") &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return "/" + base;
    }
    if (isExperimentalSoaVectorTypePath(base) &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 1) {
      return "/soa_vector";
    }
    if ((base == "Map" || isMapCollectionTypeName(base) || base == "/map" ||
         base == "/std/collections/map") &&
        splitTopLevelTemplateArgs(argText, args) && args.size() == 2) {
      return "/map";
    }
    normalizedType = base;
  }
  if (normalizedType == "/array" || normalizedType == "array") {
    return "/array";
  }
  if (normalizedType == "/vector" || normalizedType == "vector" || normalizedType == "/std/collections/vector" ||
      normalizedType == "std/collections/vector") {
    return "/vector";
  }
  if (normalizedType == "Vector" ||
      normalizedType == "/std/collections/experimental_vector/Vector" ||
      normalizedType == "std/collections/experimental_vector/Vector" ||
      normalizedType.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
      normalizedType.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "/vector";
  }
  if (normalizedType == "Buffer" || normalizedType == "std/gfx/Buffer" || normalizedType == "/std/gfx/Buffer" ||
      normalizedType == "std/gfx/experimental/Buffer" || normalizedType == "/std/gfx/experimental/Buffer" ||
      normalizedType.rfind("/std/gfx/Buffer__", 0) == 0 || normalizedType.rfind("std/gfx/Buffer__", 0) == 0 ||
      normalizedType.rfind("/std/gfx/experimental/Buffer__", 0) == 0 ||
      normalizedType.rfind("std/gfx/experimental/Buffer__", 0) == 0) {
    return "/Buffer";
  }
  if (normalizedType == "/soa_vector" || normalizedType == "soa_vector" ||
      normalizedType == "SoaVector" ||
      normalizedType == "/std/collections/experimental_soa_vector/SoaVector" ||
      normalizedType == "std/collections/experimental_soa_vector/SoaVector") {
    return "/soa_vector";
  }
  if (normalizedType.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0 ||
      normalizedType.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) == 0) {
    return "/soa_vector";
  }
  if (normalizedType == "Map" || isMapCollectionTypeName(normalizedType) || normalizedType == "/map" ||
      normalizedType == "/std/collections/map") {
    return "/map";
  }
  if (normalizedType.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
      normalizedType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
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
      if (importPath == "/std/collections/vector" &&
          canonicalPath.rfind(importPath + "/", 0) == 0) {
        return true;
      }
      if (importPath == "/std/collections/map" &&
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

std::string SemanticsValidator::preferredExperimentalMapHelperTarget(std::string_view helperName) const {
  constexpr std::string_view prefix = "/std/collections/experimental_map/";
  std::string experimentalPath = preferredPublishedCollectionLoweringPath(
      helperName, StdlibSurfaceId::CollectionsMapHelpers, prefix);
  if (experimentalPath.empty()) {
    return std::string(helperName);
  }
  experimentalPath.erase(0, prefix.size());
  return experimentalPath;
}

std::string SemanticsValidator::preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const {
  const std::string experimentalPath = preferredPublishedCollectionLoweringPath(
      helperName,
      StdlibSurfaceId::CollectionsMapHelpers,
      "/std/collections/experimental_map/");
  if (experimentalPath.empty()) {
    return "/std/collections/experimental_map/" + std::string(helperName);
  }
  return experimentalPath;
}

std::string SemanticsValidator::preferredCanonicalExperimentalVectorHelperTarget(
    std::string_view helperName) const {
  const std::string experimentalPath = preferredPublishedCollectionLoweringPath(
      helperName,
      StdlibSurfaceId::CollectionsVectorHelpers,
      "/std/collections/experimental_vector/");
  if (experimentalPath.empty()) {
    return "/std/collections/experimental_vector/" + std::string(helperName);
  }
  return experimentalPath;
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
    if (!extractExperimentalVectorElementType(params.front().binding, candidateElemType)) {
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
  if (!resolveCanonicalVectorHelperNameFromResolvedPath(resolvedPath, helperNameOut) ||
      helperNameOut == "vector") {
    canonicalPathOut.clear();
    helperNameOut.clear();
    return false;
  }
  canonicalPathOut = canonicalCollectionHelperPath(
      StdlibSurfaceId::CollectionsVectorHelpers, helperNameOut);
  return !canonicalPathOut.empty();
}

bool SemanticsValidator::canonicalExperimentalMapHelperPath(const std::string &resolvedPath,
                                                            std::string &canonicalPathOut,
                                                            std::string &helperNameOut) const {
  if (!resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(
          resolvedPath, helperNameOut)) {
    canonicalPathOut.clear();
    helperNameOut.clear();
    return false;
  }
  canonicalPathOut = canonicalCollectionHelperPath(
      StdlibSurfaceId::CollectionsMapHelpers, helperNameOut);
  return !canonicalPathOut.empty();
}

bool SemanticsValidator::canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,
                                                                       std::string &canonicalPathOut) const {
  canonicalPathOut.clear();
  if (resolvedPath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return false;
  }
  std::string helperName;
  if (!resolvePublishedCollectionHelperResolvedPath(
          resolvedPath, StdlibSurfaceId::CollectionsMapHelpers, helperName)) {
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
  canonicalPathOut = canonicalCollectionHelperPath(
      StdlibSurfaceId::CollectionsMapHelpers, helperName);
  return !canonicalPathOut.empty();
}

bool SemanticsValidator::shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath(
    const std::string &resolvedPath) const {
  if (resolvedPath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return false;
  }
  const std::string &definitionPath =
      currentValidationState_.context.definitionPath;
  return definitionPath.rfind("/std/collections/map/", 0) == 0;
}

bool SemanticsValidator::shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const {
  auto definitionPathContains = [&](std::string_view needle) {
    return currentValidationState_.context.definitionPath.find(std::string(needle)) !=
           std::string::npos;
  };
  // Wrapper validation depends on the current implementation body being visited,
  // so this stays as an explicit definition-path rule rather than a surface table.
  if (helperName == "count") {
    return definitionPathContains("/mapCount") ||
           definitionPathContains("/Reference/count") ||
           definitionPathContains("/count_ref");
  }
  if (helperName == "count_ref") {
    return definitionPathContains("/mapCountRef");
  }
  if (helperName == "contains") {
    return definitionPathContains("/mapContains") ||
           definitionPathContains("/Reference/contains") ||
           definitionPathContains("/contains_ref") ||
           definitionPathContains("/mapTryAt");
  }
  if (helperName == "contains_ref") {
    return definitionPathContains("/mapContainsRef") ||
           definitionPathContains("/mapTryAtRef");
  }
  if (helperName == "tryAt") {
    return definitionPathContains("/mapTryAt") ||
           definitionPathContains("/Reference/tryAt") ||
           definitionPathContains("/tryAt_ref");
  }
  if (helperName == "tryAt_ref") {
    return definitionPathContains("/mapTryAtRef");
  }
  if (helperName == "at") {
    return definitionPathContains("/mapAt") ||
           definitionPathContains("/mapAtUnsafe") ||
           definitionPathContains("/mapTryAt") ||
           definitionPathContains("/mapAtRef") ||
           definitionPathContains("/Reference/at") ||
           definitionPathContains("/at_ref");
  }
  if (helperName == "at_ref") {
    return definitionPathContains("/mapAtRef");
  }
  if (helperName == "at_unsafe") {
    return definitionPathContains("/mapAt") ||
           definitionPathContains("/mapAtUnsafe") ||
           definitionPathContains("/mapTryAt") ||
           definitionPathContains("/mapAtRef") ||
           definitionPathContains("/Reference/at_unsafe") ||
           definitionPathContains("/at_unsafe_ref");
  }
  if (helperName == "at_unsafe_ref") {
    return definitionPathContains("/mapAtUnsafeRef");
  }
  if (helperName == "insert") {
    return definitionPathContains("/mapInsert") ||
           definitionPathContains("/Reference/insert") ||
           definitionPathContains("/insert_ref");
  }
  if (helperName == "insert_ref") {
    return definitionPathContains("/mapInsertRef");
  }
  return false;
}

std::string SemanticsValidator::mapNamespacedMethodCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
      candidate.args.empty()) {
    return "";
  }
  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  const std::string normalizedPrefix =
      std::string(trimLeadingSlash(candidate.namespacePrefix));
  const bool explicitlyNamespacedMapHelper =
      normalizedPrefix == "map" ||
      normalizedPrefix == "std/collections/map" ||
      normalizedName.rfind("map/", 0) == 0 ||
      normalizedName.rfind("std/collections/map/", 0) == 0;
  if (!explicitlyNamespacedMapHelper) {
    return "";
  }
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyMapTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveExperimentalMapTarget(target, keyType, valueType);
  };
  std::string helperName;
  if (!resolveExplicitPublishedMapHelperExprMemberName(
          candidate.name, candidate.namespacePrefix, helperName)) {
    return "";
  }
  const std::string removedPath = "/map/" + helperName;
  if (hasDefinitionPath(removedPath)) {
    return "";
  }
  if (!resolveAnyMapTarget(candidate.args.front())) {
    return "";
  }
  return removedPath;
}

std::string SemanticsValidator::directMapHelperCompatibilityPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
    return "";
  }
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyMapTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveExperimentalMapTarget(target, keyType, valueType);
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
      resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(
          resolvedPath, helperName);
  std::string explicitSurfaceHelperName;
  const bool spellsCurrentMapWrapperSurface =
      candidate.namespacePrefix.empty() &&
      resolvePublishedCollectionHelperMemberToken(
          candidate.name,
          StdlibSurfaceId::CollectionsMapHelpers,
          explicitSurfaceHelperName) &&
      explicitSurfaceHelperName == helperName &&
      trimLeadingSlash(candidate.name) != helperName &&
      explicitPath.rfind("/map/", 0) != 0 &&
      explicitPath.rfind("/std/collections/map/", 0) != 0;
  if (!resolvedCompatibilityHelper &&
      !resolveExplicitPublishedMapHelperExprMemberName(
          candidate.name, candidate.namespacePrefix, helperName)) {
    return "";
  }
  const std::string canonicalPath = canonicalCollectionHelperPath(
      StdlibSurfaceId::CollectionsMapHelpers, helperName);
  if (resolvedCompatibilityHelper &&
      matchesResolvedPath(resolvedPath, canonicalPath)) {
    return "";
  }
  if (resolvedCompatibilityHelper && spellsCurrentMapWrapperSurface) {
    return "";
  }
  const std::string removedPath = "/map/" + helperName;
  if (hasDefinitionPath(removedPath) || candidate.args.empty()) {
    return "";
  }
  if (helperName == "at" || helperName == "at_unsafe") {
    return removedPath;
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
  if (receiverIndex >= candidate.args.size() || !resolveAnyMapTarget(candidate.args[receiverIndex])) {
    return "";
  }
  return removedPath;
}

std::string SemanticsValidator::explicitRemovedCollectionMethodPath(std::string_view rawMethodName,
                                                                    std::string_view namespacePrefix) const {
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
    if (removedPath.empty() || hasDefinitionPath(removedPath)) {
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

bool SemanticsValidator::getVectorStatementHelperName(const Expr &candidate,
                                                      std::string &helperNameOut) const {
  helperNameOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }

  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  if (isRemovedPublishedVectorStatementHelperName(normalizedName)) {
    helperNameOut = normalizedName;
    return true;
  }
  const std::string oldExplicitSoaPath = explicitOldSoaHelperPath(candidate);
  if (!oldExplicitSoaPath.empty()) {
    helperNameOut = oldExplicitSoaPath.substr(oldExplicitSoaPath.find_last_of('/') + 1);
    return true;
  }

  std::string removedPath = explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(resolveCalleePath(candidate), "");
  }
  constexpr std::string_view kCanonicalVectorPrefix = "/std/collections/vector/";
  if (removedPath.rfind(kCanonicalVectorPrefix, 0) != 0 &&
      removedPath.rfind("/array/", 0) != 0) {
    return false;
  }

  const std::string helperName = removedPath.substr(removedPath.find_last_of('/') + 1);
  if (removedPath.rfind("/array/", 0) == 0) {
    const std::string canonicalPath = "/std/collections/vector/" + helperName;
    if (hasDefinitionPath(canonicalPath) || hasImportedDefinitionPath(canonicalPath)) {
      return false;
    }
  }
  if (!isRemovedPublishedVectorStatementHelperName(helperName)) {
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
  if (explicitPath.rfind("/vector/", 0) != 0) {
    return "";
  }

  std::string removedPath =
      explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(explicitPath, "");
  }
  if (removedPath.rfind("/vector/", 0) != 0) {
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
  auto resolveAnyMapTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveExperimentalMapTarget(target, keyType, valueType);
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
  return resolveAnyMapTarget(candidate.args[receiverIndex]);
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
  auto resolveAnyMapTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveExperimentalMapTarget(target, keyType, valueType);
  };

  auto preferredRemovedMapHelperPath = [&](std::string_view helperName) {
    const std::string canonical = "/std/collections/map/" + std::string(helperName);
    const std::string alias = "/map/" + std::string(helperName);
    if (defMap_.count(canonical) > 0) {
      return canonical;
    }
    if (defMap_.count(alias) > 0) {
      return alias;
    }
    return canonical;
  };

  if (candidate.kind != Expr::Kind::Call || candidate.name.empty() || candidate.args.empty()) {
    return false;
  }

  if (!candidate.isMethodCall) {
    std::string helperName;
    if (!resolveExplicitPublishedMapHelperExprMemberName(
            candidate.name, "", helperName) &&
        !resolveCanonicalCompatibilityMapHelperNameFromResolvedPath(
            resolvedPath, helperName)) {
      return false;
    }
    if (!isPublishedMapBaseHelperName(helperName) ||
        defMap_.count("/" + helperName) > 0) {
      return false;
    }

    auto tryResolveReceiverIndex = [&](size_t index) -> bool {
      if (index >= candidate.args.size()) {
        return false;
      }
      if (!resolveAnyMapTarget(candidate.args[index])) {
        return false;
      }
      targetPathOut = preferredRemovedMapHelperPath(helperName);
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
  if (!resolveExplicitPublishedMapHelperExprMemberName(
          wrappedResolvedPath, "", helperName)) {
    if (!resolvePublishedCollectionHelperMemberToken(
            wrappedResolvedPath,
            StdlibSurfaceId::CollectionsMapHelpers,
            helperName)) {
      return false;
    }
  }
  if (!isPublishedMapBaseHelperName(helperName)) {
    return false;
  }

  auto isWrappedMapReceiverCall = [&](const Expr &receiverExpr) {
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
      return returnsMapCollectionType(transform.templateArgs.front());
    }
    return false;
  };

  if (!(resolveAnyMapTarget(candidate.args.front()) || isWrappedMapReceiverCall(candidate.args.front()))) {
    return false;
  }

  targetPathOut = preferredRemovedMapHelperPath(helperName);
  return true;
}


} // namespace primec::semantics
