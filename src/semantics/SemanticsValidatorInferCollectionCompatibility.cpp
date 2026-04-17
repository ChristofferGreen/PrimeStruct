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

namespace primec::semantics {
namespace {

bool isSoaMutatorName(std::string_view helperName) {
  return helperName == "push" || helperName == "reserve";
}

std::string explicitOldSoaMutatorPath(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return "";
  }
  std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  if (normalizedPrefix == "soa_vector" && isSoaMutatorName(normalizedName)) {
    return "/soa_vector/" + normalizedName;
  }
  constexpr std::string_view kOldExplicitPrefix = "soa_vector/";
  if (normalizedName.rfind(kOldExplicitPrefix, 0) != 0) {
    return "";
  }
  const std::string_view helperName = std::string_view(normalizedName).substr(kOldExplicitPrefix.size());
  if (!isSoaMutatorName(helperName)) {
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
  if (normalizedType == "/soa_vector" || normalizedType == "soa_vector") {
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
  const size_t suffix = canonicalPath.find("__t");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  if (canonicalPath.rfind("/File/", 0) == 0 || canonicalPath.rfind("/FileError/", 0) == 0) {
    canonicalPath.insert(0, "/std/file");
  }
  const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
  for (const auto &importPath : importPaths) {
    if (importPath == canonicalPath) {
      return true;
    }
    if (importPath == "/std/collections/vector" &&
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
}

bool SemanticsValidator::hasDefinitionPath(const std::string &path) const {
  std::string canonicalPath = path;
  const size_t suffix = canonicalPath.find("__t");
  if (suffix != std::string::npos) {
    canonicalPath.erase(suffix);
  }
  for (const auto &[resolvedPath, definition] : defMap_) {
    (void)definition;
    if (matchesResolvedPath(resolvedPath, canonicalPath)) {
      return true;
    }
  }
  return false;
}

std::string SemanticsValidator::preferredExperimentalMapHelperTarget(std::string_view helperName) const {
  if (const auto *borrowedDescriptor = findBorrowedExperimentalMapHelperByName(helperName)) {
    constexpr std::string_view prefix = "/std/collections/experimental_map/";
    std::string_view experimentalPath = borrowedDescriptor->experimentalPath;
    if (experimentalPath.rfind(prefix, 0) == 0) {
      experimentalPath.remove_prefix(prefix.size());
    }
    return std::string(experimentalPath);
  }
  const ExperimentalMapHelperDescriptor *descriptor = findExperimentalMapHelperByName(helperName);
  if (descriptor == nullptr) {
    return std::string(helperName);
  }
  constexpr std::string_view prefix = "/std/collections/experimental_map/";
  std::string_view experimentalPath = descriptor->experimentalPath;
  if (experimentalPath.rfind(prefix, 0) == 0) {
    experimentalPath.remove_prefix(prefix.size());
  }
  return std::string(experimentalPath);
}

std::string SemanticsValidator::preferredCanonicalExperimentalMapHelperTarget(std::string_view helperName) const {
  if (const auto *borrowedDescriptor = findBorrowedExperimentalMapHelperByName(helperName)) {
    return std::string(borrowedDescriptor->experimentalPath);
  }
  const ExperimentalMapHelperDescriptor *descriptor = findExperimentalMapHelperByName(helperName);
  if (descriptor == nullptr) {
    return "/std/collections/experimental_map/" + std::string(helperName);
  }
  return std::string(descriptor->experimentalPath);
}

std::string SemanticsValidator::preferredCanonicalExperimentalVectorHelperTarget(
    std::string_view helperName) const {
  const ExperimentalVectorHelperDescriptor *descriptor =
      findExperimentalVectorHelperByName(helperName);
  if (descriptor == nullptr) {
    return "/std/collections/experimental_vector/" + std::string(helperName);
  }
  return std::string(descriptor->experimentalPath);
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
  canonicalPathOut.clear();
  helperNameOut.clear();
  for (const auto &descriptor : kExperimentalVectorHelperDescriptors) {
    if (matchesResolvedPath(resolvedPath, descriptor.canonicalPath) ||
        matchesResolvedPath(resolvedPath, descriptor.aliasPath) ||
        matchesResolvedPath(resolvedPath, descriptor.wrapperPath)) {
      canonicalPathOut = descriptor.canonicalPath;
      helperNameOut = descriptor.helperName;
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::canonicalExperimentalMapHelperPath(const std::string &resolvedPath,
                                                            std::string &canonicalPathOut,
                                                            std::string &helperNameOut) const {
  canonicalPathOut.clear();
  helperNameOut.clear();
  for (const auto &descriptor : kBorrowedExperimentalMapHelperDescriptors) {
    if (matchesResolvedPath(resolvedPath, descriptor.canonicalPath) ||
        matchesResolvedPath(resolvedPath, descriptor.aliasPath) ||
        matchesResolvedPath(resolvedPath, descriptor.wrapperPath)) {
      canonicalPathOut = descriptor.canonicalPath;
      helperNameOut = descriptor.helperName;
      return true;
    }
  }
  for (const auto &descriptor : kExperimentalMapHelperDescriptors) {
    if (matchesResolvedPath(resolvedPath, descriptor.canonicalPath) ||
        matchesResolvedPath(resolvedPath, descriptor.aliasPath) ||
        matchesResolvedPath(resolvedPath, descriptor.wrapperPath)) {
      canonicalPathOut = descriptor.canonicalPath;
      helperNameOut = descriptor.helperName;
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::canonicalizeExperimentalMapHelperResolvedPath(const std::string &resolvedPath,
                                                                       std::string &canonicalPathOut) const {
  canonicalPathOut.clear();
  for (const auto &descriptor : kExperimentalMapHelperDescriptors) {
    if (matchesResolvedPath(resolvedPath, descriptor.experimentalPath)) {
      canonicalPathOut = descriptor.canonicalPath;
      return true;
    }
  }
  if (matchesResolvedPath(resolvedPath, "/std/collections/experimental_map/mapCountRef")) {
    canonicalPathOut = "/std/collections/map/count";
    return true;
  }
  if (matchesResolvedPath(resolvedPath, "/std/collections/experimental_map/mapContainsRef")) {
    canonicalPathOut = "/std/collections/map/contains";
    return true;
  }
  if (matchesResolvedPath(resolvedPath, "/std/collections/experimental_map/mapTryAtRef")) {
    canonicalPathOut = "/std/collections/map/tryAt";
    return true;
  }
  if (matchesResolvedPath(resolvedPath, "/std/collections/experimental_map/mapAtRef")) {
    canonicalPathOut = "/std/collections/map/at";
    return true;
  }
  if (matchesResolvedPath(resolvedPath, "/std/collections/experimental_map/mapAtUnsafeRef")) {
    canonicalPathOut = "/std/collections/map/at_unsafe";
    return true;
  }
  if (matchesResolvedPath(resolvedPath, "/std/collections/experimental_map/mapInsertRef")) {
    canonicalPathOut = "/std/collections/map/insert_ref";
    return true;
  }
  return false;
}

bool SemanticsValidator::shouldLogicalCanonicalizeDefinedExperimentalMapHelperPath(
    const std::string &resolvedPath) const {
  (void)resolvedPath;
  return false;
}

bool SemanticsValidator::shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const {
  const ExperimentalMapHelperDescriptor *descriptor = findExperimentalMapHelperByName(helperName);
  if (descriptor == nullptr) {
    return false;
  }
  for (size_t i = 0; i < descriptor->wrapperDefinitionNeedlesCount; ++i) {
    if (currentValidationState_.context.definitionPath.find(std::string(descriptor->wrapperDefinitionNeedles[i])) !=
        std::string::npos) {
      return true;
    }
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
  const std::string borrowedRemovedPath =
      [&]() -> std::string {
        const std::string rawHelperName = std::string(trimLeadingSlash(candidate.name));
        if (const auto *descriptor = findBorrowedExperimentalMapHelperByName(rawHelperName)) {
          return std::string(descriptor->aliasPath);
        }
        return "";
      }();
  if (!borrowedRemovedPath.empty()) {
    if (hasDefinitionPath(borrowedRemovedPath) || !resolveAnyMapTarget(candidate.args.front())) {
      return "";
    }
    return borrowedRemovedPath;
  }
  const ExperimentalMapHelperDescriptor *descriptor =
      findExperimentalMapCompatibilityHelper(candidate.name, candidate.namespacePrefix, "", false, true);
  if (descriptor == nullptr) {
    return "";
  }
  const std::string removedPath(descriptor->aliasPath);
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
  const std::string borrowedRemovedPath =
      [&]() -> std::string {
        std::string canonicalPath;
        std::string helperName;
        if (!canonicalExperimentalMapHelperPath(resolvedPath, canonicalPath, helperName)) {
          return "";
        }
        const auto *descriptor = findBorrowedExperimentalMapHelperByName(helperName);
        if (descriptor == nullptr) {
          return "";
        }
        if (matchesResolvedPath(resolvedPath, descriptor->canonicalPath)) {
          return "";
        }
        return std::string(descriptor->aliasPath);
      }();
  if (!borrowedRemovedPath.empty()) {
    if (hasDefinitionPath(borrowedRemovedPath) || candidate.args.empty()) {
      return "";
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
    return borrowedRemovedPath;
  }
  const ExperimentalMapHelperDescriptor *descriptor =
      findExperimentalMapCompatibilityHelper(candidate.name,
                                             candidate.namespacePrefix,
                                             resolvedPath,
                                             true,
                                             false);
  if (descriptor == nullptr) {
    return "";
  }
  if (matchesResolvedPath(resolvedPath, descriptor->canonicalPath)) {
    return "";
  }
  const std::string removedPath(descriptor->aliasPath);
  if (hasDefinitionPath(removedPath) || candidate.args.empty()) {
    return "";
  }
  if (!descriptor->requiresValidatedMapReceiverForDirectCompatibility) {
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
  if (const RemovedCollectionHelperDescriptor *descriptor =
          findRemovedCollectionHelper(RemovedCollectionHelperFamily::VectorLike, normalizedName);
      descriptor != nullptr && descriptor->statementOnly) {
    helperNameOut = std::string(descriptor->helperName);
    return true;
  }
  const std::string oldExplicitSoaPath = explicitOldSoaMutatorPath(candidate);
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
  const RemovedCollectionHelperDescriptor *descriptor =
      findRemovedCollectionHelper(RemovedCollectionHelperFamily::VectorLike, helperName);
  if (descriptor == nullptr || !descriptor->statementOnly) {
    return false;
  }
  helperNameOut = helperName;
  return true;
}

std::string SemanticsValidator::getDirectVectorHelperCompatibilityPath(const Expr &candidate) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
    return "";
  }

  const std::string normalizedName = std::string(trimLeadingSlash(candidate.name));
  const std::string normalizedPrefix = std::string(trimLeadingSlash(candidate.namespacePrefix));
  const std::string resolvedPath = resolveCalleePath(candidate);
  const bool spellsVectorCompatibility =
      normalizedName.rfind("std/collections/vector/", 0) == 0 ||
      normalizedPrefix == "std/collections/vector" ||
      resolvedPath.rfind("/std/collections/vector/", 0) == 0;
  if (!spellsVectorCompatibility) {
    return "";
  }

  std::string removedPath = explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(resolvedPath, "");
  }
  if (removedPath.rfind("/std/collections/vector/", 0) != 0) {
    return "";
  }
  return hasDefinitionPath(removedPath) ? "" : removedPath;
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
    const Expr &candidate,
    const std::string &primaryHelperName,
    const std::string &secondaryHelperName) const {
  const std::string removedPath = getRemovedRootedVectorDirectCallPath(candidate);
  if (removedPath.empty()) {
    return "";
  }

  auto matchesHelperName = [&](const std::string &helperName) {
    return !helperName.empty() && removedPath == "/vector/" + helperName;
  };
  if (!matchesHelperName(primaryHelperName) &&
      !matchesHelperName(secondaryHelperName)) {
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
  const RemovedCollectionHelperDescriptor *descriptor =
      findRemovedCollectionHelper(RemovedCollectionHelperFamily::Map, "count");
  if (descriptor == nullptr || !descriptor->supportsUnnamespacedFallback) {
    return false;
  }
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
    const RemovedCollectionHelperDescriptor *descriptor =
        findRemovedCollectionHelperReference(RemovedCollectionHelperFamily::Map, candidate.name);
    if (descriptor == nullptr) {
      descriptor =
          findRemovedCollectionHelperReference(RemovedCollectionHelperFamily::Map, resolvedPath);
    }
    if (descriptor == nullptr || defMap_.count("/" + std::string(descriptor->helperName)) > 0) {
      return false;
    }

    auto tryResolveReceiverIndex = [&](size_t index) -> bool {
      if (index >= candidate.args.size()) {
        return false;
      }
      if (!resolveAnyMapTarget(candidate.args[index])) {
        return false;
      }
      targetPathOut = preferredRemovedMapHelperPath(descriptor->helperName);
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

  const RemovedCollectionHelperDescriptor *descriptor =
      findRemovedCollectionHelperReference(RemovedCollectionHelperFamily::Map,
                                           resolvedPath,
                                           "",
                                           true);
  if (descriptor == nullptr) {
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

  targetPathOut = preferredRemovedMapHelperPath(descriptor->helperName);
  return true;
}


} // namespace primec::semantics
