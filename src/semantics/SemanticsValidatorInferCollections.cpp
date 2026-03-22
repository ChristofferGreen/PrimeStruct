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

namespace primec::semantics {
namespace {

struct ExperimentalMapHelperDescriptor {
  std::string_view helperName;
  std::string_view canonicalPath;
  std::string_view aliasPath;
  std::string_view wrapperPath;
  std::string_view experimentalPath;
  bool supportsMethodCompatibility;
  bool requiresValidatedMapReceiverForDirectCompatibility;
  std::array<std::string_view, 4> wrapperDefinitionNeedles;
  size_t wrapperDefinitionNeedlesCount;
};

constexpr ExperimentalMapHelperDescriptor kExperimentalMapHelperDescriptors[] = {
    {"count", "/std/collections/map/count", "/map/count", "/std/collections/mapCount",
     "/std/collections/experimental_map/mapCount", true, true,
     {"/mapCount", "", "", ""}, 1},
    {"contains", "/std/collections/map/contains", "/map/contains", "/std/collections/mapContains",
     "/std/collections/experimental_map/mapContains", false, true,
     {"/mapContains", "/mapTryAt", "", ""}, 2},
    {"tryAt", "/std/collections/map/tryAt", "/map/tryAt", "/std/collections/mapTryAt",
     "/std/collections/experimental_map/mapTryAt", false, true,
     {"/mapTryAt", "", "", ""}, 1},
    {"at", "/std/collections/map/at", "/map/at", "/std/collections/mapAt",
     "/std/collections/experimental_map/mapAt", true, false,
     {"/mapAt", "/mapAtUnsafe", "/mapTryAt", "/mapAtRef"}, 4},
    {"at_unsafe", "/std/collections/map/at_unsafe", "/map/at_unsafe", "/std/collections/mapAtUnsafe",
     "/std/collections/experimental_map/mapAtUnsafe", true, false,
    {"/mapAt", "/mapAtUnsafe", "/mapTryAt", "/mapAtRef"}, 4},
};

struct ExperimentalVectorHelperDescriptor {
  std::string_view helperName;
  std::string_view canonicalPath;
  std::string_view aliasPath;
  std::string_view wrapperPath;
  std::string_view experimentalPath;
};

constexpr ExperimentalVectorHelperDescriptor kExperimentalVectorHelperDescriptors[] = {
    {"count", "/std/collections/vector/count", "/vector/count", "/std/collections/vectorCount",
     "/std/collections/experimental_vector/vectorCount"},
    {"capacity", "/std/collections/vector/capacity", "/vector/capacity", "/std/collections/vectorCapacity",
     "/std/collections/experimental_vector/vectorCapacity"},
    {"push", "/std/collections/vector/push", "/vector/push", "/std/collections/vectorPush",
     "/std/collections/experimental_vector/vectorPush"},
    {"pop", "/std/collections/vector/pop", "/vector/pop", "/std/collections/vectorPop",
     "/std/collections/experimental_vector/vectorPop"},
    {"reserve", "/std/collections/vector/reserve", "/vector/reserve", "/std/collections/vectorReserve",
     "/std/collections/experimental_vector/vectorReserve"},
    {"clear", "/std/collections/vector/clear", "/vector/clear", "/std/collections/vectorClear",
     "/std/collections/experimental_vector/vectorClear"},
    {"remove_at", "/std/collections/vector/remove_at", "/vector/remove_at", "/std/collections/vectorRemoveAt",
     "/std/collections/experimental_vector/vectorRemoveAt"},
    {"remove_swap", "/std/collections/vector/remove_swap", "/vector/remove_swap",
     "/std/collections/vectorRemoveSwap", "/std/collections/experimental_vector/vectorRemoveSwap"},
    {"at", "/std/collections/vector/at", "/vector/at", "/std/collections/vectorAt",
     "/std/collections/experimental_vector/vectorAt"},
    {"at_unsafe", "/std/collections/vector/at_unsafe", "/vector/at_unsafe", "/std/collections/vectorAtUnsafe",
     "/std/collections/experimental_vector/vectorAtUnsafe"},
};

enum class RemovedCollectionHelperFamily {
  VectorLike,
  Map,
};

struct RemovedCollectionHelperDescriptor {
  RemovedCollectionHelperFamily family;
  std::string_view helperName;
  std::string_view removedPath;
  bool statementOnly;
  bool supportsUnnamespacedFallback;
};

constexpr RemovedCollectionHelperDescriptor kRemovedCollectionHelperDescriptors[] = {
    {RemovedCollectionHelperFamily::VectorLike, "count", "/vector/count", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "capacity", "/vector/capacity", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "at", "/vector/at", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "at_unsafe", "/vector/at_unsafe", false, false},
    {RemovedCollectionHelperFamily::VectorLike, "push", "/vector/push", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "pop", "/vector/pop", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "reserve", "/vector/reserve", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "clear", "/vector/clear", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "remove_at", "/vector/remove_at", true, false},
    {RemovedCollectionHelperFamily::VectorLike, "remove_swap", "/vector/remove_swap", true, false},
    {RemovedCollectionHelperFamily::Map, "count", "/map/count", false, true},
    {RemovedCollectionHelperFamily::Map, "at", "/map/at", false, false},
    {RemovedCollectionHelperFamily::Map, "at_unsafe", "/map/at_unsafe", false, false},
};

bool resolveRemovedCollectionHelperReference(std::string_view rawMethodName,
                                             std::string_view namespacePrefix,
                                             RemovedCollectionHelperFamily &familyOut,
                                             std::string_view &helperNameOut,
                                             bool &preserveArrayPathOut);

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool isDirectMapConstructorPath(std::string_view resolvedCandidate) {
  auto matchesDirectMapConstructorPath = [&](std::string_view basePath) {
    return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesDirectMapConstructorPath("/std/collections/map/map") ||
         matchesDirectMapConstructorPath("/std/collections/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/mapOct") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapNew") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSingle") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapDouble") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapPair") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapTriple") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuad") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapQuint") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSext") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapSept") ||
         matchesDirectMapConstructorPath("/std/collections/experimental_map/mapOct");
}

bool matchesResolvedPath(std::string_view resolvedPath, std::string_view basePath) {
  return resolvedPath == basePath || resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
}

std::string_view trimLeadingSlash(std::string_view text) {
  if (!text.empty() && text.front() == '/') {
    text.remove_prefix(1);
  }
  return text;
}

const ExperimentalMapHelperDescriptor *findExperimentalMapHelperByName(std::string_view helperName) {
  for (const auto &descriptor : kExperimentalMapHelperDescriptors) {
    if (descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  return nullptr;
}

const ExperimentalVectorHelperDescriptor *findExperimentalVectorHelperByName(std::string_view helperName) {
  for (const auto &descriptor : kExperimentalVectorHelperDescriptors) {
    if (descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  return nullptr;
}

const ExperimentalMapHelperDescriptor *findExperimentalMapCompatibilityHelper(std::string_view normalizedName,
                                                                              std::string_view normalizedPrefix,
                                                                              std::string_view resolvedPath,
                                                                              bool allowResolvedAliasMatch,
                                                                              bool requireMethodCompatibility) {
  normalizedName = trimLeadingSlash(normalizedName);
  normalizedPrefix = trimLeadingSlash(normalizedPrefix);
  for (const auto &descriptor : kExperimentalMapHelperDescriptors) {
    if (requireMethodCompatibility && !descriptor.supportsMethodCompatibility) {
      continue;
    }
    const std::string_view aliasPath = trimLeadingSlash(descriptor.aliasPath);
    const std::string_view canonicalPath = trimLeadingSlash(descriptor.canonicalPath);
    if (normalizedName == aliasPath || normalizedName == canonicalPath ||
        ((normalizedPrefix == "map" || normalizedPrefix == "std/collections/map") &&
         normalizedName == descriptor.helperName) ||
        (allowResolvedAliasMatch && matchesResolvedPath(resolvedPath, descriptor.aliasPath))) {
      return &descriptor;
    }
  }
  return nullptr;
}

const RemovedCollectionHelperDescriptor *findRemovedCollectionHelper(RemovedCollectionHelperFamily family,
                                                                     std::string_view helperName) {
  for (const auto &descriptor : kRemovedCollectionHelperDescriptors) {
    if (descriptor.family == family && descriptor.helperName == helperName) {
      return &descriptor;
    }
  }
  return nullptr;
}

const RemovedCollectionHelperDescriptor *findRemovedCollectionHelperReference(
    RemovedCollectionHelperFamily family,
    std::string_view rawMethodName,
    std::string_view namespacePrefix = "",
    bool allowWrappedReceiverPath = false) {
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  if (allowWrappedReceiverPath) {
    if (rawMethodName.rfind("Reference/", 0) == 0) {
      rawMethodName.remove_prefix(std::string_view("Reference/").size());
    } else if (rawMethodName.rfind("Pointer/", 0) == 0) {
      rawMethodName.remove_prefix(std::string_view("Pointer/").size());
    }
  }
  RemovedCollectionHelperFamily resolvedFamily = RemovedCollectionHelperFamily::VectorLike;
  std::string_view helperName;
  bool preserveArrayPath = false;
  if (resolveRemovedCollectionHelperReference(
          rawMethodName, namespacePrefix, resolvedFamily, helperName, preserveArrayPath) &&
      resolvedFamily == family) {
    return findRemovedCollectionHelper(family, helperName);
  }
  return findRemovedCollectionHelper(family, rawMethodName);
}

std::string removedCollectionMethodPath(RemovedCollectionHelperFamily family,
                                        std::string_view helperName,
                                        bool preserveArrayPath) {
  const RemovedCollectionHelperDescriptor *descriptor = findRemovedCollectionHelper(family, helperName);
  if (descriptor == nullptr) {
    return "";
  }
  if (family == RemovedCollectionHelperFamily::VectorLike && preserveArrayPath) {
    return "/array/" + std::string(helperName);
  }
  return std::string(descriptor->removedPath);
}

bool resolveRemovedCollectionHelperReference(std::string_view rawMethodName,
                                             std::string_view namespacePrefix,
                                             RemovedCollectionHelperFamily &familyOut,
                                             std::string_view &helperNameOut,
                                             bool &preserveArrayPathOut) {
  rawMethodName = trimLeadingSlash(rawMethodName);
  namespacePrefix = trimLeadingSlash(namespacePrefix);
  preserveArrayPathOut = false;

  auto setVectorLike = [&](std::string_view helperName, bool preserveArrayPath) {
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::VectorLike;
    preserveArrayPathOut = preserveArrayPath;
    return true;
  };
  auto setMap = [&](std::string_view helperName) {
    helperNameOut = helperName;
    familyOut = RemovedCollectionHelperFamily::Map;
    preserveArrayPathOut = false;
    return true;
  };

  if (namespacePrefix == "array") {
    return setVectorLike(rawMethodName, true);
  }
  if (namespacePrefix == "vector" || namespacePrefix == "std/collections/vector") {
    return setVectorLike(rawMethodName, false);
  }
  if (namespacePrefix == "map" || namespacePrefix == "std/collections/map") {
    return setMap(rawMethodName);
  }
  if (rawMethodName.rfind("array/", 0) == 0) {
    return setVectorLike(rawMethodName.substr(std::string_view("array/").size()), true);
  }
  if (rawMethodName.rfind("vector/", 0) == 0) {
    return setVectorLike(rawMethodName.substr(std::string_view("vector/").size()), false);
  }
  if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
    return setVectorLike(rawMethodName.substr(std::string_view("std/collections/vector/").size()), false);
  }
  if (rawMethodName.rfind("map/", 0) == 0) {
    return setMap(rawMethodName.substr(std::string_view("map/").size()));
  }
  if (rawMethodName.rfind("std/collections/map/", 0) == 0) {
    return setMap(rawMethodName.substr(std::string_view("std/collections/map/").size()));
  }
  return false;
}

} // namespace

std::string SemanticsValidator::normalizeCollectionTypePath(const std::string &typePath) const {
  if (typePath == "/array" || typePath == "array") {
    return "/array";
  }
  if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
      typePath == "std/collections/vector") {
    return "/vector";
  }
  if (typePath == "Vector" ||
      typePath == "/std/collections/experimental_vector/Vector" ||
      typePath == "std/collections/experimental_vector/Vector" ||
      typePath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
      typePath.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "/vector";
  }
  if (typePath == "/soa_vector" || typePath == "soa_vector") {
    return "/soa_vector";
  }
  if (typePath == "Map" || isMapCollectionTypeName(typePath) || typePath == "/map" ||
      typePath == "/std/collections/map") {
    return "/map";
  }
  if (typePath.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
      typePath.rfind("std/collections/experimental_map/Map__", 0) == 0) {
    return "/map";
  }
  if (typePath == "/string" || typePath == "string") {
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
    if (currentValidationContext_.definitionPath.empty()) {
      return {};
    }
    const size_t slash = currentValidationContext_.definitionPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return {};
    }
    return currentValidationContext_.definitionPath.substr(0, slash);
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
  return false;
}

bool SemanticsValidator::shouldBuiltinValidateCurrentMapWrapperHelper(std::string_view helperName) const {
  const ExperimentalMapHelperDescriptor *descriptor = findExperimentalMapHelperByName(helperName);
  if (descriptor == nullptr) {
    return false;
  }
  for (size_t i = 0; i < descriptor->wrapperDefinitionNeedlesCount; ++i) {
    if (currentValidationContext_.definitionPath.find(std::string(descriptor->wrapperDefinitionNeedles[i])) !=
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
  const BuiltinCollectionDispatchResolvers dispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, adapters);
  auto resolveAnyMapTarget = [&](const Expr &target) {
    std::string keyType;
    std::string valueType;
    return dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
           dispatchResolvers.resolveExperimentalMapTarget(target, keyType, valueType);
  };
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
  const std::string resolvedPath = resolveCalleePath(candidate);
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

  std::string removedPath = explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(resolveCalleePath(candidate), "");
  }
  if (removedPath.rfind("/vector/", 0) != 0 && removedPath.rfind("/array/", 0) != 0) {
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
      normalizedName.rfind("vector/", 0) == 0 || normalizedPrefix == "vector" ||
      resolvedPath.rfind("/vector/", 0) == 0;
  if (!spellsVectorCompatibility) {
    return "";
  }

  std::string removedPath = explicitRemovedCollectionMethodPath(candidate.name, candidate.namespacePrefix);
  if (removedPath.empty()) {
    removedPath = explicitRemovedCollectionMethodPath(resolvedPath, "");
  }
  if (removedPath.rfind("/vector/", 0) != 0) {
    return "";
  }
  return hasDefinitionPath(removedPath) ? "" : removedPath;
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

    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= candidate.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          appendReceiverIndex(i);
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      if (!resolveAnyMapTarget(candidate.args[receiverIndex])) {
        continue;
      }
      targetPathOut = preferredRemovedMapHelperPath(descriptor->helperName);
      return true;
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

bool SemanticsValidator::inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut) {
  auto cachedBindingIt = returnBindings_.find(def.fullPath);
  if (cachedBindingIt != returnBindings_.end() && !cachedBindingIt->second.typeName.empty()) {
    bindingOut = cachedBindingIt->second;
    return true;
  }

  auto findDefParamBinding = [&](const std::vector<ParameterInfo> &defParams,
                                 const std::string &name) -> const BindingInfo * {
    for (const auto &param : defParams) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };
  auto parseTypeText = [&](const std::string &typeText, BindingInfo &parsedOut) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
      parsedOut.typeName = base;
      parsedOut.typeTemplateArg = argText;
      return true;
    }
    parsedOut.typeName = normalized;
    parsedOut.typeTemplateArg.clear();
    return true;
  };
  std::function<bool(const std::string &)> isConcreteReturnTypeText;
  std::function<bool(const std::string &)> returnTypeReferencesTemplateParam;
  isConcreteReturnTypeText = [&](const std::string &typeText) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    if (returnKindForTypeName(normalized) != ReturnKind::Unknown) {
      return true;
    }
    if (!normalizeCollectionTypePath(normalized).empty()) {
      return true;
    }
    if (!resolveStructTypePath(normalized, def.namespacePrefix, structNames_).empty()) {
      return true;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalized, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base.empty()) {
      return false;
    }
    if (base == "Pointer" || base == "Reference" || base == "Result" ||
        base == "Buffer" || base == "uninitialized" || base == "array" ||
        base == "vector" || base == "soa_vector" || isMapCollectionTypeName(base) ||
        base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
        base == "Map" || base == "std/collections/experimental_map/Map" ||
        !resolveStructTypePath(base, def.namespacePrefix, structNames_).empty()) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.empty()) {
        return false;
      }
      for (const auto &arg : args) {
        if (!isConcreteReturnTypeText(arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };
  returnTypeReferencesTemplateParam = [&](const std::string &typeText) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    for (const auto &templateArg : def.templateArgs) {
      if (normalizeBindingTypeName(templateArg) == normalized) {
        return true;
      }
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalized, base, argText)) {
      return false;
    }
    if (returnTypeReferencesTemplateParam(base)) {
      return true;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args)) {
      return false;
    }
    for (const auto &arg : args) {
      if (returnTypeReferencesTemplateParam(arg)) {
        return true;
      }
    }
    return false;
  };

  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      break;
    }
    const bool isSpecializedDefinition = def.fullPath.find("__t") != std::string::npos;
    if (isSpecializedDefinition &&
        (returnTypeReferencesTemplateParam(returnType) || !isConcreteReturnTypeText(returnType))) {
      break;
    }
    return parseTypeText(returnType, bindingOut);
  }

  if (!returnBindingInferenceStack_.insert(def.fullPath).second) {
    return false;
  }
  struct ReturnBindingInferenceScopeGuard {
    std::unordered_set<std::string> &stack;
    const std::string &path;
    ~ReturnBindingInferenceScopeGuard() {
      stack.erase(path);
    }
  } returnBindingInferenceGuard{returnBindingInferenceStack_, def.fullPath};

  ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));

  std::vector<ParameterInfo> defParams;
  defParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    std::optional<std::string> restrictType;
    std::string parseError;
    (void)parseBindingInfo(
        paramExpr, def.namespacePrefix, structNames_, importAliases_, paramInfo.binding, restrictType, parseError);
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    defParams.push_back(std::move(paramInfo));
  }

  std::unordered_map<std::string, BindingInfo> defLocals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, parseError)) {
        defLocals[stmt.name] = binding;
      } else if (stmt.args.size() == 1 &&
                 inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding, &stmt)) {
        defLocals[stmt.name] = binding;
      }
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (def.returnExpr.has_value()) {
    valueExpr = &*def.returnExpr;
  }
  if (valueExpr == nullptr) {
    return false;
  }
  if (valueExpr->kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findDefParamBinding(defParams, valueExpr->name)) {
      bindingOut = *paramBinding;
      return true;
    }
    auto localIt = defLocals.find(valueExpr->name);
    if (localIt != defLocals.end()) {
      bindingOut = localIt->second;
      return true;
    }
  }
  return inferBindingTypeFromInitializer(*valueExpr, defParams, defLocals, bindingOut);
}

bool SemanticsValidator::inferQueryExprTypeText(const Expr &expr,
                                                const std::vector<ParameterInfo> &params,
                                                const std::unordered_map<std::string, BindingInfo> &locals,
                                                std::string &typeTextOut) {
  auto resolveBindingTypeText = [&](const std::string &name, std::string &resolvedTypeTextOut) -> bool {
    resolvedTypeTextOut.clear();
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      resolvedTypeTextOut = bindingTypeText(*paramBinding);
      return !resolvedTypeTextOut.empty();
    }
    auto localIt = locals.find(name);
    if (localIt == locals.end()) {
      return false;
    }
    resolvedTypeTextOut = bindingTypeText(localIt->second);
    return !resolvedTypeTextOut.empty();
  };
  auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return allowAnyName || isBuiltinBlockCall(candidate);
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
  std::function<bool(const Expr &, std::string &)> inferExprTypeText;
  inferExprTypeText = [&](const Expr &candidate, std::string &currentTypeTextOut) -> bool {
    currentTypeTextOut.clear();
    if (!queryTypeInferenceExprStack_.insert(&candidate).second) {
      return false;
    }
    struct ExprTypeScopeGuard {
      std::unordered_set<const Expr *> &stack;
      const Expr *expr = nullptr;
      ~ExprTypeScopeGuard() {
        if (expr != nullptr) {
          stack.erase(expr);
        }
      }
    } exprGuard{queryTypeInferenceExprStack_, &candidate};
    if (candidate.kind == Expr::Kind::Name) {
      return resolveBindingTypeText(candidate.name, currentTypeTextOut);
    }
    if (candidate.kind == Expr::Kind::Literal) {
      currentTypeTextOut = candidate.isUnsigned ? "u64" : (candidate.intWidth == 64 ? "i64" : "i32");
      return true;
    }
    if (candidate.kind == Expr::Kind::BoolLiteral) {
      currentTypeTextOut = "bool";
      return true;
    }
    if (candidate.kind == Expr::Kind::FloatLiteral) {
      currentTypeTextOut = candidate.floatWidth == 64 ? "f64" : "f32";
      return true;
    }
    if (candidate.kind == Expr::Kind::StringLiteral) {
      currentTypeTextOut = "string";
      return true;
    }
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      std::string thenTypeText;
      std::string elseTypeText;
      if (!inferExprTypeText(thenValue ? *thenValue : thenArg, thenTypeText) ||
          !inferExprTypeText(elseValue ? *elseValue : elseArg, elseTypeText)) {
        return false;
      }
      if (normalizeBindingTypeName(thenTypeText) != normalizeBindingTypeName(elseTypeText)) {
        return false;
      }
      currentTypeTextOut = thenTypeText;
      return true;
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferExprTypeText(valueExpr->args.front(), currentTypeTextOut);
      }
      return inferExprTypeText(*valueExpr, currentTypeTextOut);
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "move") && candidate.args.size() == 1) {
      return inferExprTypeText(candidate.args.front(), currentTypeTextOut);
    }
    if (isAssignCall(candidate) && candidate.args.size() == 2) {
      return inferExprTypeText(candidate.args[1], currentTypeTextOut);
    }
    if (isSimpleCallName(candidate, "dereference") && candidate.args.size() == 1) {
      std::string wrappedTypeText;
      if (!inferExprTypeText(candidate.args.front(), wrappedTypeText)) {
        return false;
      }
      currentTypeTextOut = unwrapReferencePointerTypeText(wrappedTypeText);
      return !currentTypeTextOut.empty();
    }
    std::string builtinAccessName;
    if (getBuiltinArrayAccessName(candidate, builtinAccessName) && candidate.args.size() == 2) {
      const Expr &receiver =
          candidate.isMethodCall ? candidate.args.front()
                                 : (candidate.args.empty() ? candidate : candidate.args.front());
      std::string elemType;
      if (builtinCollectionDispatchResolvers.resolveVectorTarget(receiver, elemType) ||
          builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget(receiver, elemType) ||
          builtinCollectionDispatchResolvers.resolveArrayTarget(receiver, elemType)) {
        currentTypeTextOut = normalizeBindingTypeName(elemType);
        return !currentTypeTextOut.empty();
      }
      if (builtinCollectionDispatchResolvers.resolveStringTarget(receiver)) {
        currentTypeTextOut = "i32";
        return true;
      }
    }

    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto canonicalizeResolvedPath = [](std::string path) {
      const size_t suffix = path.find("__t");
      if (suffix != std::string::npos) {
        path.erase(suffix);
      }
      return path;
    };
    auto hasDirectExperimentalVectorImport = [&]() {
      const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
      for (const auto &importPath : importPaths) {
        if (importPath == "/std/collections/experimental_vector/*" ||
            importPath == "/std/collections/experimental_vector/vector" ||
            importPath == "/std/collections/experimental_vector") {
          return true;
        }
      }
      return false;
    };
    const bool prefersImportedExperimentalVectorConstructor =
        !candidate.isMethodCall &&
        candidate.templateArgs.size() == 1 &&
        [&]() {
          if (candidate.name == "vector" && candidate.namespacePrefix.empty()) {
            auto aliasIt = importAliases_.find(candidate.name);
            if (aliasIt != importAliases_.end() &&
                canonicalizeResolvedPath(aliasIt->second) == "/std/collections/experimental_vector/vector") {
              return true;
            }
          }
          return canonicalizeResolvedPath(resolvedCandidate) == "/vector" &&
                 hasDirectExperimentalVectorImport();
        }();
    if (prefersImportedExperimentalVectorConstructor) {
      currentTypeTextOut = "Vector<" + candidate.templateArgs.front() + ">";
      return true;
    }
    const std::string canonicalResolvedCandidate = canonicalizeResolvedPath(resolvedCandidate);
    if (canonicalResolvedCandidate == "/vector" &&
        !hasDirectExperimentalVectorImport() &&
        candidate.templateArgs.size() == 1) {
      currentTypeTextOut = "vector<" + candidate.templateArgs.front() + ">";
      return true;
    }
    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      const bool preferResolvedCollectionDefinition =
          !canonicalResolvedCandidate.empty() &&
          canonicalResolvedCandidate != "/" + collection &&
          defMap_.count(canonicalResolvedCandidate) != 0;
      if (preferResolvedCollectionDefinition) {
        // Imported stdlib collection constructors should infer from their declared return type
        // instead of collapsing to the legacy builtin collection surface.
      } else if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
                 candidate.templateArgs.size() == 1) {
        currentTypeTextOut = collection + "<" + candidate.templateArgs.front() + ">";
        return true;
      } else if (collection == "map" && candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
    }
    auto preferredResolvedCandidate = [&]() -> std::string {
      std::string normalizedName = candidate.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      return {};
    };
    auto methodResolvedCandidate = [&]() -> std::string {
      if (!candidate.isMethodCall || candidate.args.empty() || candidate.name.empty()) {
        return {};
      }
      std::string methodName = candidate.name;
      if (!methodName.empty() && methodName.front() == '/') {
        methodName.erase(methodName.begin());
      }
      if (methodName.empty()) {
        return {};
      }
      auto resolveMethodOwnerPath = [&](const std::string &typeText, const std::string &typeNamespace) {
        std::string normalizedType = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
        if (normalizedType.empty()) {
          return std::string{};
        }
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedType, base, argText)) {
          const std::string normalizedBase = normalizeBindingTypeName(base);
          if (!normalizedBase.empty()) {
            if (!normalizeCollectionTypePath(normalizedBase).empty()) {
              return std::string{};
            }
            normalizedType = normalizedBase;
          }
        }
        if (isPrimitiveBindingTypeName(normalizedType)) {
          return "/" + normalizedType;
        }
        if (!normalizedType.empty() && normalizedType.front() == '/') {
          if (structNames_.count(normalizedType) > 0 || defMap_.count(normalizedType) > 0) {
            return normalizedType;
          }
        }
        if (!normalizedType.empty() && normalizedType.front() != '/') {
          const std::string rootPath = "/" + normalizedType;
          if (structNames_.count(rootPath) > 0 || defMap_.count(rootPath) > 0) {
            return rootPath;
          }
          auto importIt = importAliases_.find(normalizedType);
          if (importIt != importAliases_.end()) {
            return importIt->second;
          }
        }
        std::string resolvedType = resolveStructTypePath(normalizedType, typeNamespace, structNames_);
        if (resolvedType.empty()) {
          resolvedType = resolveTypePath(normalizedType, typeNamespace);
        }
        return resolvedType;
      };

      const Expr &receiver = candidate.args.front();
      std::string receiverTypeText;
      if (!inferExprTypeText(receiver, receiverTypeText) || receiverTypeText.empty()) {
        return std::string{};
      }
      const std::string ownerPath = resolveMethodOwnerPath(receiverTypeText, receiver.namespacePrefix);
      if (ownerPath.empty()) {
        return std::string{};
      }
      return ownerPath + "/" + methodName;
    };
    if (isDirectMapConstructorPath(resolvedCandidate)) {
      if (candidate.templateArgs.size() == 2) {
        currentTypeTextOut = "map<" + candidate.templateArgs[0] + ", " + candidate.templateArgs[1] + ">";
        return true;
      }
      if (candidate.args.empty() || candidate.args.size() % 2 != 0) {
        return false;
      }
      std::string keyTypeText;
      std::string valueTypeText;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyTypeText;
        std::string currentValueTypeText;
        if (!inferExprTypeText(candidate.args[i], currentKeyTypeText) ||
            !inferExprTypeText(candidate.args[i + 1], currentValueTypeText)) {
          return false;
        }
        if (keyTypeText.empty()) {
          keyTypeText = currentKeyTypeText;
        } else if (normalizeBindingTypeName(keyTypeText) != normalizeBindingTypeName(currentKeyTypeText)) {
          return false;
        }
        if (valueTypeText.empty()) {
          valueTypeText = currentValueTypeText;
        } else if (normalizeBindingTypeName(valueTypeText) != normalizeBindingTypeName(currentValueTypeText)) {
          return false;
        }
      }
      if (keyTypeText.empty() || valueTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = "map<" + keyTypeText + ", " + valueTypeText + ">";
      return true;
    }
    std::string collectionMethodFallbackTypeText;
    std::string inferredMethodReturnTypeText;
    if (candidate.isMethodCall) {
      const ReturnKind inferredKind = inferExprReturnKind(candidate, params, locals);
      if (inferredKind == ReturnKind::Array) {
        collectionMethodFallbackTypeText = inferStructReturnPath(candidate, params, locals);
        const std::string normalizedCollectionType = normalizeCollectionTypePath(collectionMethodFallbackTypeText);
        if (!normalizedCollectionType.empty()) {
          const bool keepExperimentalCollectionPath =
              collectionMethodFallbackTypeText == "Vector" ||
              collectionMethodFallbackTypeText == "/std/collections/experimental_vector/Vector" ||
              collectionMethodFallbackTypeText == "std/collections/experimental_vector/Vector" ||
              collectionMethodFallbackTypeText.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
              collectionMethodFallbackTypeText.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
              collectionMethodFallbackTypeText == "Map" ||
              collectionMethodFallbackTypeText.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
              collectionMethodFallbackTypeText.rfind("std/collections/experimental_map/Map__", 0) == 0;
          if (!keepExperimentalCollectionPath) {
            collectionMethodFallbackTypeText = normalizedCollectionType.substr(1);
          }
        }
      }
      if (inferredKind != ReturnKind::Unknown && inferredKind != ReturnKind::Void) {
        inferredMethodReturnTypeText = typeNameForReturnKind(inferredKind);
      }
    }

    std::vector<std::string> resolvedCandidates;
    auto appendResolvedCandidate = [&](const std::string &candidatePath) {
      if (candidatePath.empty()) {
        return;
      }
      for (const auto &existing : resolvedCandidates) {
        if (existing == candidatePath) {
          return;
        }
      }
      resolvedCandidates.push_back(candidatePath);
    };
    appendResolvedCandidate(resolvedCandidate);
    appendResolvedCandidate(canonicalResolvedCandidate);
    appendResolvedCandidate(preferredResolvedCandidate());
    appendResolvedCandidate(methodResolvedCandidate());
    const Definition *resolvedDefinition = nullptr;
    std::string resolvedDefinitionPath;
    for (const auto &candidatePath : resolvedCandidates) {
      auto defIt = defMap_.find(candidatePath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        continue;
      }
      resolvedDefinition = defIt->second;
      resolvedDefinitionPath = candidatePath;
      break;
    }
    if (resolvedDefinition == nullptr) {
      if (!inferredMethodReturnTypeText.empty()) {
        currentTypeTextOut = inferredMethodReturnTypeText;
        return true;
      }
      if (collectionMethodFallbackTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = collectionMethodFallbackTypeText;
      return true;
    }
    for (const auto &transform : resolvedDefinition->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (transform.templateArgs.front() == "auto") {
        break;
      }
      currentTypeTextOut = transform.templateArgs.front();
      return !currentTypeTextOut.empty();
    }
    if (returnBindingInferenceStack_.contains(resolvedDefinitionPath) ||
        inferenceStack_.contains(resolvedDefinitionPath)) {
      if (!inferredMethodReturnTypeText.empty()) {
        currentTypeTextOut = inferredMethodReturnTypeText;
        return true;
      }
      if (collectionMethodFallbackTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = collectionMethodFallbackTypeText;
      return true;
    }
    if (!queryTypeInferenceDefinitionStack_.insert(resolvedDefinitionPath).second) {
      return false;
    }
    const auto stackIt = queryTypeInferenceDefinitionStack_.find(resolvedDefinitionPath);
    struct ScopeGuard {
      std::unordered_set<std::string> &stack;
      std::unordered_set<std::string>::const_iterator it;
      ~ScopeGuard() {
        stack.erase(it);
      }
    } guard{queryTypeInferenceDefinitionStack_, stackIt};
    BindingInfo inferredReturn;
    if (!inferDefinitionReturnBinding(*resolvedDefinition, inferredReturn)) {
      if (!inferredMethodReturnTypeText.empty()) {
        currentTypeTextOut = inferredMethodReturnTypeText;
        return true;
      }
      if (collectionMethodFallbackTypeText.empty()) {
        return false;
      }
      currentTypeTextOut = collectionMethodFallbackTypeText;
      return true;
    }
    auto substituteCallTemplateArgs = [&](const std::string &typeText) {
      std::function<std::string(const std::string &)> substitute = [&](const std::string &currentTypeText) {
        const std::string normalized = normalizeBindingTypeName(currentTypeText);
        if (normalized.empty()) {
          return currentTypeText;
        }
        for (size_t i = 0; i < resolvedDefinition->templateArgs.size() && i < candidate.templateArgs.size(); ++i) {
          if (normalizeBindingTypeName(resolvedDefinition->templateArgs[i]) == normalized) {
            return candidate.templateArgs[i];
          }
        }
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(normalized, base, argText)) {
          return normalized;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.empty()) {
          return normalized;
        }
        std::string substitutedBase = substitute(base);
        for (auto &arg : args) {
          arg = substitute(arg);
        }
        return substitutedBase + "<" + joinTemplateArgs(args) + ">";
      };
      return substitute(typeText);
    };
    inferredReturn.typeName = substituteCallTemplateArgs(inferredReturn.typeName);
    inferredReturn.typeTemplateArg = substituteCallTemplateArgs(inferredReturn.typeTemplateArg);
    currentTypeTextOut = bindingTypeText(inferredReturn);
    return !currentTypeTextOut.empty();
  };

  return inferExprTypeText(expr, typeTextOut);
}

SemanticsValidator::BuiltinCollectionDispatchResolvers SemanticsValidator::makeBuiltinCollectionDispatchResolvers(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {
    pointeeTypeOut.clear();
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    pointeeTypeOut = args.front();
    return !pointeeTypeOut.empty();
  };
  auto extractCollectionElementType = [&](const std::string &typeText,
                                          const std::string &expectedBase,
                                          std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base != expectedBase) {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    elemTypeOut = args.front();
    return true;
  };
  auto extractExperimentalMapFieldTypes = [this](const BindingInfo &binding,
                                                 std::string &keyTypeOut,
                                                 std::string &valueTypeOut) -> bool {
    auto extractFromTypeText = [&](std::string normalizedType) -> bool {
      while (true) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
          base = normalizeBindingTypeName(base);
          if (base == "Reference" || base == "Pointer") {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
              return false;
            }
            normalizedType = normalizeBindingTypeName(args.front());
            continue;
          }
          std::string normalizedBase = base;
          if (!normalizedBase.empty() && normalizedBase.front() == '/') {
            normalizedBase.erase(normalizedBase.begin());
          }
          if ((normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map")) {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
              return false;
            }
            keyTypeOut = args[0];
            valueTypeOut = args[1];
            return true;
          }
        }

        std::string resolvedPath = normalizedType;
        if (!resolvedPath.empty() && resolvedPath.front() != '/') {
          resolvedPath.insert(resolvedPath.begin(), '/');
        }
        std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
        if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
          normalizedResolvedPath.erase(normalizedResolvedPath.begin());
        }
        if (normalizedResolvedPath.rfind("std/collections/experimental_map/Map__", 0) != 0) {
          return false;
        }
        auto defIt = defMap_.find(resolvedPath);
        if (defIt == defMap_.end() || !defIt->second) {
          return false;
        }
        std::string keysElementType;
        std::string payloadsElementType;
        for (const auto &fieldExpr : defIt->second->statements) {
          if (!fieldExpr.isBinding) {
            continue;
          }
          BindingInfo fieldBinding;
          std::optional<std::string> restrictType;
          std::string parseError;
          if (!parseBindingInfo(fieldExpr,
                                defIt->second->namespacePrefix,
                                structNames_,
                                importAliases_,
                                fieldBinding,
                                restrictType,
                                parseError)) {
            continue;
          }
          std::string vectorElementType;
          if (normalizeBindingTypeName(fieldBinding.typeName) == "vector" &&
              !fieldBinding.typeTemplateArg.empty()) {
            std::vector<std::string> fieldArgs;
            if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, fieldArgs) || fieldArgs.size() != 1) {
              continue;
            }
            vectorElementType = fieldArgs.front();
          } else if (!extractExperimentalVectorElementType(fieldBinding, vectorElementType)) {
            continue;
          }
          if (fieldExpr.name == "keys") {
            keysElementType = vectorElementType;
          } else if (fieldExpr.name == "payloads") {
            payloadsElementType = vectorElementType;
          }
        }
        if (keysElementType.empty() || payloadsElementType.empty()) {
          return false;
        }
        keyTypeOut = keysElementType;
        valueTypeOut = payloadsElementType;
        return true;
      }
    };

    keyTypeOut.clear();
    valueTypeOut.clear();
    if (binding.typeTemplateArg.empty()) {
      return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
    }
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
  };
  auto extractAnyMapKeyValueTypes = [extractExperimentalMapFieldTypes](const BindingInfo &binding,
                                                                       std::string &keyTypeOut,
                                                                       std::string &valueTypeOut) -> bool {
    return extractMapKeyValueTypes(binding, keyTypeOut, valueTypeOut) ||
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  auto resolveBindingTarget = [=, this](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        bindingOut = it->second;
        return true;
      }
    }
    return adapters.resolveBindingTarget != nullptr &&
           adapters.resolveBindingTarget(target, bindingOut);
  };
  auto resolveMethodOwnerPath = [=, this](const std::string &typeText,
                                          const std::string &typeNamespace) -> std::string {
    std::string normalizedType = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    if (normalizedType.empty()) {
      return {};
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if (!normalizedBase.empty()) {
        normalizedType = normalizedBase;
      }
    }
    if (normalizedType.empty() || !normalizeCollectionTypePath(normalizedType).empty()) {
      return {};
    }
    if (normalizedType.front() == '/') {
      if (structNames_.count(normalizedType) > 0 || defMap_.count(normalizedType) > 0) {
        return normalizedType;
      }
    } else {
      const std::string rootPath = "/" + normalizedType;
      if (structNames_.count(rootPath) > 0 || defMap_.count(rootPath) > 0) {
        return rootPath;
      }
      auto importIt = importAliases_.find(normalizedType);
      if (importIt != importAliases_.end()) {
        return importIt->second;
      }
    }
    std::string resolvedType = resolveStructTypePath(normalizedType, typeNamespace, structNames_);
    if (resolvedType.empty()) {
      resolvedType = resolveTypePath(normalizedType, typeNamespace);
    }
    return resolvedType;
  };
  auto inferCallBinding = [=, this](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (adapters.inferCallBinding != nullptr && adapters.inferCallBinding(target, bindingOut)) {
      return true;
    }
    auto inferResolvedDefinitionBinding = [&](const std::string &resolvedPath) -> bool {
      auto defIt = defMap_.find(resolvedPath);
      return defIt != defMap_.end() && defIt->second != nullptr &&
             inferDefinitionReturnBinding(*defIt->second, bindingOut);
    };
    if (inferResolvedDefinitionBinding(resolveCalleePath(target))) {
      return true;
    }
    if (!target.isMethodCall || target.args.empty() || target.name.empty()) {
      return false;
    }
    BindingInfo receiverBinding;
    if (!resolveBindingTarget(target.args.front(), receiverBinding)) {
      return false;
    }
    std::string methodName = target.name;
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    if (methodName.empty()) {
      return false;
    }
    const std::string ownerPath =
        resolveMethodOwnerPath(bindingTypeText(receiverBinding), target.args.front().namespacePrefix);
    return !ownerPath.empty() && inferResolvedDefinitionBinding(ownerPath + "/" + methodName);
  };
  auto resolveArrayLikeBinding = [](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    auto resolveReference = [&](const BindingInfo &candidate) -> bool {
      if (candidate.typeName != "Reference" || candidate.typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(candidate.typeTemplateArg, base, arg) ||
          normalizeBindingTypeName(base) != "array") {
        return false;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    };
    if (resolveReference(binding)) {
      return true;
    }
    if ((binding.typeName != "array" && binding.typeName != "vector") ||
        binding.typeTemplateArg.empty()) {
      return false;
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveVectorBinding = [this](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (binding.typeName != "vector" || binding.typeTemplateArg.empty()) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalVectorElementType(binding, elemTypeOut);
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveSoaVectorBinding = [](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (binding.typeName != "soa_vector" || binding.typeTemplateArg.empty()) {
      return false;
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveStringBinding = [](const BindingInfo &binding) -> bool {
    return normalizeBindingTypeName(binding.typeName) == "string";
  };
  auto resolveMapBinding = [extractAnyMapKeyValueTypes](const BindingInfo &binding,
                                                        std::string &keyTypeOut,
                                                        std::string &valueTypeOut) -> bool {
    return extractAnyMapKeyValueTypes(binding, keyTypeOut, valueTypeOut);
  };
  auto isDirectMapConstructorCall = [this](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto matchesPath = [&](std::string_view basePath) {
      return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
    };
    return matchesPath("/std/collections/map/map") ||
           matchesPath("/std/collections/mapNew") ||
           matchesPath("/std/collections/mapSingle") ||
           matchesPath("/std/collections/mapDouble") ||
           matchesPath("/std/collections/mapPair") ||
           matchesPath("/std/collections/mapTriple") ||
           matchesPath("/std/collections/mapQuad") ||
           matchesPath("/std/collections/mapQuint") ||
           matchesPath("/std/collections/mapSext") ||
           matchesPath("/std/collections/mapSept") ||
           matchesPath("/std/collections/mapOct") ||
           matchesPath("/std/collections/experimental_map/mapNew") ||
           matchesPath("/std/collections/experimental_map/mapSingle") ||
           matchesPath("/std/collections/experimental_map/mapDouble") ||
           matchesPath("/std/collections/experimental_map/mapPair") ||
           matchesPath("/std/collections/experimental_map/mapTriple") ||
           matchesPath("/std/collections/experimental_map/mapQuad") ||
           matchesPath("/std/collections/experimental_map/mapQuint") ||
           matchesPath("/std/collections/experimental_map/mapSext") ||
           matchesPath("/std/collections/experimental_map/mapSept") ||
           matchesPath("/std/collections/experimental_map/mapOct");
  };

  struct ReceiverResolverState {
    std::function<bool(const Expr &, std::string &)> resolveIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveDereferencedIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveWrappedIndexedArgsPackElementType;
    std::function<bool(const Expr &, std::string &)> resolveArgsPackAccessTarget;
    std::function<bool(const Expr &, std::string &)> resolveArrayTarget;
    std::function<bool(const Expr &, std::string &)> resolveVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveExperimentalVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveExperimentalVectorValueTarget;
    std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
    std::function<bool(const Expr &, std::string &)> resolveBufferTarget;
    std::function<bool(const Expr &)> resolveStringTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapTarget;
    std::function<bool(const Expr &, std::string &, std::string &)> resolveExperimentalMapValueTarget;
    std::function<bool(const Expr &, size_t &)> isDirectCanonicalVectorAccessCallOnBuiltinReceiver;
  };
  auto state = std::make_shared<ReceiverResolverState>();

  state->resolveArgsPackAccessTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    elemType.clear();
    auto resolveBinding = [&](const BindingInfo &binding) { return getArgsPackElementType(binding, elemType); };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        return resolveBinding(it->second);
      }
    }
    return false;
  };
  state->resolveIndexedArgsPackElementType = [=, this](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string accessName;
    if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(target);
    if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
      return false;
    }
    auto resolveBinding = [&](const BindingInfo &binding) { return getArgsPackElementType(binding, elemTypeOut); };
    if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
      return resolveBinding(*paramBinding);
    }
    auto it = locals.find(accessReceiver->name);
    return it != locals.end() && resolveBinding(it->second);
  };
  state->resolveDereferencedIndexedArgsPackElementType = [=](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
      return false;
    }
    std::string wrappedType;
    if (!state->resolveIndexedArgsPackElementType(target.args.front(), wrappedType)) {
      return false;
    }
    return extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  state->resolveWrappedIndexedArgsPackElementType = [=](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string wrappedType;
    if (!state->resolveIndexedArgsPackElementType(target, wrappedType)) {
      return false;
    }
    return extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  state->resolveArrayTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveArrayLikeBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (state->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
        std::vector<std::string> args;
        const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
        if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, args) && args.size() == 1) {
          elemType = args.front();
          return true;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(target, collectionName) &&
            (collectionName == "array" || collectionName == "vector") &&
            target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
        return false;
      }
    }
    return false;
  };
  state->resolveVectorTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveVectorBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (state->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "vector", params, locals, args) && args.size() == 1) {
          elemType = args.front();
          return true;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(target, collectionName) &&
            collectionName == "vector" &&
            target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
        return false;
      }
      if (!target.isMethodCall && isSimpleCallName(target, "to_aos") && target.args.size() == 1) {
        return state->resolveSoaVectorTarget(target.args.front(), elemType);
      }
    }
    if (inferCallBinding(target, binding)) {
      return resolveVectorBinding(binding, elemType);
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return resolveVectorBinding(inferredBinding, elemType);
  };
  state->resolveExperimentalVectorTarget =
      [=, this](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractExperimentalVectorElementType(binding, elemTypeOut);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (inferCallBinding(target, binding) &&
        extractExperimentalVectorElementType(binding, elemTypeOut)) {
      return true;
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return extractExperimentalVectorElementType(inferredBinding, elemTypeOut);
  };
  state->resolveExperimentalVectorValueTarget =
      [=, this](const Expr &target, std::string &elemTypeOut) -> bool {
    auto extractValueBinding = [&](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalVectorElementType(binding, elemTypeOut);
    };
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    elemTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractValueBinding(binding);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (inferCallBinding(target, binding) &&
        extractValueBinding(binding)) {
      return true;
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return extractValueBinding(inferredBinding);
  };
  state->resolveSoaVectorTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveSoaVectorBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (state->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/soa_vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "soa_vector", params, locals, args) && args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
      if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
        return state->resolveVectorTarget(target.args.front(), elemType);
      }
    }
    return false;
  };
  state->resolveBufferTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    auto resolveReferenceBufferType = [&](const std::string &typeName,
                                          const std::string &typeTemplateArg,
                                          std::string &elemTypeOut) -> bool {
      if ((typeName != "Reference" && typeName != "Pointer") || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) || normalizeBindingTypeName(base) != "Buffer" ||
          nestedArg.empty()) {
        return false;
      }
      elemTypeOut = nestedArg;
      return true;
    };
    auto resolveArgsPackReferenceBufferType = [&](const std::string &typeName,
                                                  const std::string &typeTemplateArg,
                                                  std::string &elemTypeOut) -> bool {
      if (typeName != "args" || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
          (normalizeBindingTypeName(base) != "Reference" && normalizeBindingTypeName(base) != "Pointer") ||
          nestedArg.empty()) {
        return false;
      }
      return resolveReferenceBufferType(base, nestedArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackReferenceBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (resolveArgsPackReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemTypeOut)) {
          return true;
        }
      }
      auto it = locals.find(targetName);
      return it != locals.end() &&
             resolveArgsPackReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackValueBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      auto resolveBinding = [&](const BindingInfo &binding) {
        std::string packElemType;
        std::string base;
        return getArgsPackElementType(binding, packElemType) &&
               splitTemplateTypeName(normalizeBindingTypeName(packElemType), base, elemTypeOut) &&
               normalizeBindingTypeName(base) == "Buffer";
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(targetName);
      return it != locals.end() && resolveBinding(it->second);
    };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (normalizeBindingTypeName(paramBinding->typeName) == "Buffer" &&
            !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        if (normalizeBindingTypeName(it->second.typeName) == "Buffer" &&
            !it->second.typeTemplateArg.empty()) {
          elemType = it->second.typeTemplateArg;
          return true;
        }
      }
      return false;
    }
    if (target.kind == Expr::Kind::Call) {
      if (resolveIndexedArgsPackValueBuffer(target, elemType)) {
        return true;
      }
      if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
        const Expr &innerTarget = target.args.front();
        if (innerTarget.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, innerTarget.name)) {
            if (resolveReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemType)) {
              return true;
            }
          }
          auto it = locals.find(innerTarget.name);
          if (it != locals.end() &&
              resolveReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemType)) {
            return true;
          }
        }
        if (resolveIndexedArgsPackReferenceBuffer(innerTarget, elemType)) {
          return true;
        }
      }
      if (isSimpleCallName(target, "buffer") && target.templateArgs.size() == 1) {
        elemType = target.templateArgs.front();
        return true;
      }
      if (isSimpleCallName(target, "upload") && target.args.size() == 1) {
        return state->resolveArrayTarget(target.args.front(), elemType);
      }
    }
    return false;
  };
  state->resolveMapTarget = [=, this](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding) &&
        resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
      return true;
    }
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        binding.typeName = normalizeBindingTypeName(base);
        binding.typeTemplateArg = argText;
      } else {
        binding.typeName = normalizedType;
        binding.typeTemplateArg.clear();
      }
      if (resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
        return true;
      }
    }
    if (target.kind == Expr::Kind::Call) {
      const std::string resolvedTarget = resolveCalleePath(target);
      if (isDirectMapConstructorPath(resolvedTarget) && target.templateArgs.size() == 2) {
        keyTypeOut = target.templateArgs[0];
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      std::string builtinCollectionName;
      if (getBuiltinCollectionName(target, builtinCollectionName) &&
          builtinCollectionName == "map" &&
          target.templateArgs.size() == 2) {
        keyTypeOut = target.templateArgs[0];
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      std::string elemType;
      if (state->resolveIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      if (state->resolveWrappedIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      if (state->resolveDereferencedIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
        const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(target);
        if (accessReceiver != nullptr && accessReceiver->kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
            std::string packElemType;
            if (getArgsPackElementType(*paramBinding, packElemType) &&
                extractMapKeyValueTypesFromTypeText(packElemType, keyTypeOut, valueTypeOut)) {
              return true;
            }
          }
          auto it = locals.find(accessReceiver->name);
          if (it != locals.end()) {
            std::string packElemType;
            if (getArgsPackElementType(it->second, packElemType) &&
                extractMapKeyValueTypesFromTypeText(packElemType, keyTypeOut, valueTypeOut)) {
              return true;
            }
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/map") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
          keyTypeOut = args[0];
          valueTypeOut = args[1];
          return true;
        }
        return false;
      }
      if (inferCallBinding(target, binding) &&
          resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
        return true;
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        if (!returnsMapCollectionType(transform.templateArgs.front())) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(normalizeBindingTypeName(transform.templateArgs.front()), base, arg)) {
          return false;
        }
        while (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return false;
          }
          if (!splitTemplateTypeName(normalizeBindingTypeName(args.front()), base, arg)) {
            return false;
          }
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 2) {
          return false;
        }
        keyTypeOut = args[0];
        valueTypeOut = args[1];
        return true;
      }
    }
    return false;
  };
  state->resolveExperimentalMapTarget =
      [=, this](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    auto assignBindingFromTypeText = [](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (isDirectMapConstructorCall(target)) {
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
        keyTypeOut = args[0];
        valueTypeOut = args[1];
        return true;
      }
    }
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      assignBindingFromTypeText(inferredTypeText, binding);
      if (extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut)) {
        return true;
      }
    }
    return inferCallBinding(target, binding) &&
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  state->resolveExperimentalMapValueTarget =
      [=](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    auto extractValueBinding = [&](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
    };
    keyTypeOut.clear();
    valueTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractValueBinding(binding);
    }
    return target.kind == Expr::Kind::Call &&
           inferCallBinding(target, binding) &&
           extractValueBinding(binding);
  };
  state->isDirectCanonicalVectorAccessCallOnBuiltinReceiver =
      [=](const Expr &candidate, size_t &receiverIndexOut) -> bool {
    receiverIndexOut = 0;
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/vector/at" && normalized != "std/collections/vector/at_unsafe") {
      return false;
    }
    if (candidate.args.empty()) {
      return false;
    }
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndexOut = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndexOut = 0;
      }
    }
    if (receiverIndexOut >= candidate.args.size()) {
      return false;
    }
    std::string elemType;
    return state->resolveArgsPackAccessTarget(candidate.args[receiverIndexOut], elemType) ||
           state->resolveVectorTarget(candidate.args[receiverIndexOut], elemType) ||
           state->resolveArrayTarget(candidate.args[receiverIndexOut], elemType) ||
           state->resolveStringTarget(candidate.args[receiverIndexOut]);
  };
  state->resolveStringTarget = [=, this](const Expr &target) -> bool {
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveStringBinding(binding);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/string") {
        return true;
      }
        if (target.isMethodCall && target.name == "why" && !target.args.empty()) {
          const Expr &receiver = target.args.front();
          if (resolveBindingTarget(receiver, binding) &&
              normalizeBindingTypeName(binding.typeName) == "FileError") {
            return true;
          }
          if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
            return true;
          }
          std::string elemType;
        if ((state->resolveIndexedArgsPackElementType(receiver, elemType) ||
             state->resolveDereferencedIndexedArgsPackElementType(receiver, elemType)) &&
            normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType)) == "FileError") {
          return true;
        }
      }
      std::string builtinName;
      if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
        if (const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(target)) {
          std::string elemType;
          std::string keyType;
          std::string valueType;
          if (state->resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
              state->resolveArrayTarget(*accessReceiver, elemType) ||
              state->resolveVectorTarget(*accessReceiver, elemType) ||
              state->resolveExperimentalVectorTarget(*accessReceiver, elemType)) {
            return normalizeBindingTypeName(elemType) == "string";
          }
          if (state->resolveMapTarget(*accessReceiver, keyType, valueType)) {
            return normalizeBindingTypeName(valueType) == "string";
          }
          if (state->resolveStringTarget(*accessReceiver)) {
            return false;
          }
        }
        size_t receiverIndex = 0;
        if (state->isDirectCanonicalVectorAccessCallOnBuiltinReceiver(target, receiverIndex)) {
          std::string elemType;
          return (state->resolveArgsPackAccessTarget(target.args[receiverIndex], elemType) ||
                  state->resolveArrayTarget(target.args[receiverIndex], elemType) ||
                  state->resolveVectorTarget(target.args[receiverIndex], elemType) ||
                  state->resolveExperimentalVectorTarget(target.args[receiverIndex], elemType)) &&
                 normalizeBindingTypeName(elemType) == "string";
        }
        const std::string resolvedTarget = resolveCalleePath(target);
        auto defIt = defMap_.find(resolvedTarget);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          if (!ensureDefinitionReturnKindReady(*defIt->second)) {
            return false;
          }
          auto kindIt = returnKinds_.find(resolvedTarget);
          if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
            return kindIt->second == ReturnKind::String;
          }
        }
        std::string elemType;
        if ((state->resolveArgsPackAccessTarget(target.args.front(), elemType) ||
             state->resolveArrayTarget(target.args.front(), elemType) ||
             state->resolveVectorTarget(target.args.front(), elemType) ||
             state->resolveExperimentalVectorTarget(target.args.front(), elemType)) &&
            elemType == "string") {
          return true;
        }
        std::string keyType;
        std::string valueType;
        if (state->resolveMapTarget(target.args.front(), keyType, valueType) &&
            normalizeBindingTypeName(valueType) == "string") {
          return true;
        }
      }
    }
    return false;
  };

  return {state->resolveIndexedArgsPackElementType,
          state->resolveDereferencedIndexedArgsPackElementType,
          state->resolveWrappedIndexedArgsPackElementType,
          state->resolveArgsPackAccessTarget,
          state->resolveArrayTarget,
          state->resolveVectorTarget,
          state->resolveExperimentalVectorTarget,
          state->resolveExperimentalVectorValueTarget,
          state->resolveSoaVectorTarget,
          state->resolveBufferTarget,
          state->resolveStringTarget,
          state->resolveMapTarget,
          state->resolveExperimentalMapTarget,
          state->resolveExperimentalMapValueTarget};
}

bool SemanticsValidator::resolveCallCollectionTypePath(const Expr &target,
                                                       const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                                       std::string &typePathOut) {
  typePathOut.clear();
  auto explicitCallPath = [&](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.name.empty()) {
      return {};
    }
    std::string rawName = callExpr.name;
    if (!rawName.empty() && rawName.front() == '/') {
      return rawName;
    }
    std::string rawPrefix = callExpr.namespacePrefix;
    if (!rawPrefix.empty() && rawPrefix.front() != '/') {
      rawPrefix.insert(rawPrefix.begin(), '/');
    }
    if (rawPrefix.empty()) {
      return "/" + rawName;
    }
    return rawPrefix + "/" + rawName;
  };
  auto resolvedCallPath = [&](const Expr &callExpr) { return resolveCalleePath(callExpr); };
  auto inferCollectionTypePathFromType =
      [&](const std::string &typeName, auto &&inferCollectionTypePathFromTypeRef) -> std::string {
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    std::string normalized = normalizeCollectionTypePath(normalizedType);
    if (!normalized.empty()) {
      return normalized;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return {};
    }
    base = normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return {};
      }
      return inferCollectionTypePathFromTypeRef(args.front(), inferCollectionTypePathFromTypeRef);
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return {};
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      return "/" + base;
    }
    if ((base == "Vector" || base == "std/collections/experimental_vector/Vector") &&
        args.size() == 1) {
      return "/vector";
    }
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      return "/map";
    }
    return {};
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  std::string targetTypeText;
  if (inferQueryExprTypeText(target, params, locals, targetTypeText)) {
    const std::string inferred = inferCollectionTypePathFromType(targetTypeText, inferCollectionTypePathFromType);
    if (!inferred.empty()) {
      typePathOut = inferred;
      return true;
    }
  }

  const std::string resolvedTarget = resolvedCallPath(target);
  const std::string explicitTarget = explicitCallPath(target);
  auto matchesCollectionCtorPath = [&](std::string_view basePath) {
    const std::string specializedPrefix = std::string(basePath) + "__t";
    return resolvedTarget == basePath ||
           resolvedTarget.rfind(specializedPrefix, 0) == 0 ||
           explicitTarget == basePath ||
           explicitTarget.rfind(specializedPrefix, 0) == 0;
  };
  if (matchesCollectionCtorPath("/std/collections/vector/vector") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vector") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorNew") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorSingle") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorPair") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorTriple") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorQuad") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorQuint") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorSext") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorSept") ||
      matchesCollectionCtorPath("/std/collections/experimental_vector/vectorOct")) {
    typePathOut = "/vector";
    return true;
  }
  for (const std::string *candidatePath : {&resolvedTarget, &explicitTarget}) {
    if (candidatePath->empty()) {
      continue;
    }
    auto structIt = returnStructs_.find(*candidatePath);
    if (structIt == returnStructs_.end()) {
      continue;
    }
    std::string normalized = normalizeCollectionTypePath(structIt->second);
    if (!normalized.empty()) {
      typePathOut = normalized;
      return true;
    }
  }
  for (const std::string *candidatePath : {&resolvedTarget, &explicitTarget}) {
    if (candidatePath->empty()) {
      continue;
    }
    auto defIt = defMap_.find(*candidatePath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      continue;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferredReturnTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
      const std::string inferred =
          inferCollectionTypePathFromType(inferredReturnTypeText, inferCollectionTypePathFromType);
      if (!inferred.empty()) {
        typePathOut = inferred;
        return true;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(inferredReturn.typeName);
      if (normalizedReturnType == "Pointer" || normalizedReturnType == "Reference") {
        return false;
      }
    }
  }
  auto kindIt = returnKinds_.find(resolvedTarget);
  if (kindIt != returnKinds_.end()) {
    if (kindIt->second == ReturnKind::Array) {
      typePathOut = "/array";
      return true;
    }
    if (kindIt->second == ReturnKind::String) {
      typePathOut = "/string";
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveCallCollectionTemplateArgs(const Expr &target,
                                                           const std::string &expectedBase,
                                                           const std::vector<ParameterInfo> &params,
                                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                                           std::vector<std::string> &argsOut) {
  argsOut.clear();
  auto explicitCallPath = [&](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.name.empty()) {
      return {};
    }
    std::string rawName = callExpr.name;
    if (!rawName.empty() && rawName.front() == '/') {
      return rawName;
    }
    std::string rawPrefix = callExpr.namespacePrefix;
    if (!rawPrefix.empty() && rawPrefix.front() != '/') {
      rawPrefix.insert(rawPrefix.begin(), '/');
    }
    if (rawPrefix.empty()) {
      return "/" + rawName;
    }
    return rawPrefix + "/" + rawName;
  };
  auto resolvedCallPath = [&](const Expr &callExpr) { return resolveCalleePath(callExpr); };
  auto extractCollectionArgsFromType =
      [&](const std::string &typeName, auto &&extractCollectionArgsFromTypeRef) -> bool {
    std::string base;
    std::string arg;
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == expectedBase ||
        (expectedBase == "vector" &&
         (base == "Vector" || base == "std/collections/experimental_vector/Vector")) ||
        (expectedBase == "map" && isMapCollectionTypeName(base))) {
      return splitTopLevelTemplateArgs(arg, argsOut);
    }
    std::vector<std::string> args;
    if ((base == "Reference" || base == "Pointer") && splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
      return extractCollectionArgsFromTypeRef(args.front(), extractCollectionArgsFromTypeRef);
    }
    return false;
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }

  auto extractCollectionArgsFromBinding = [&](const BindingInfo &binding) {
    const std::string typeText = binding.typeTemplateArg.empty()
                                     ? binding.typeName
                                     : binding.typeName + "<" + binding.typeTemplateArg + ">";
    return extractCollectionArgsFromType(typeText, extractCollectionArgsFromType);
  };
  auto matchesCollectionCtorPath = [&](std::string_view basePath) {
    const std::string resolvedTarget = resolvedCallPath(target);
    const std::string explicitTarget = explicitCallPath(target);
    const std::string specializedPrefix = std::string(basePath) + "__t";
    return resolvedTarget == basePath ||
           resolvedTarget.rfind(specializedPrefix, 0) == 0 ||
           explicitTarget == basePath ||
           explicitTarget.rfind(specializedPrefix, 0) == 0;
  };

  std::string targetTypeText;
  if (inferQueryExprTypeText(target, params, locals, targetTypeText) &&
      extractCollectionArgsFromType(targetTypeText, extractCollectionArgsFromType)) {
    return true;
  }

  std::string collectionName;
  if (getBuiltinCollectionName(target, collectionName) && collectionName == expectedBase) {
    const size_t expectedArgCount = expectedBase == "map" ? 2u : 1u;
    if (target.templateArgs.size() == expectedArgCount) {
      argsOut = target.templateArgs;
      return true;
    }
  }

  if (expectedBase == "vector" &&
      (matchesCollectionCtorPath("/std/collections/vector/vector") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vector") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorNew") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorSingle") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorPair") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorTriple") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorQuad") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorQuint") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorSext") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorSept") ||
       matchesCollectionCtorPath("/std/collections/experimental_vector/vectorOct")) &&
      target.templateArgs.size() == 1) {
    argsOut = target.templateArgs;
    return true;
  }

  const std::string resolvedTarget = resolvedCallPath(target);
  const std::string explicitTarget = explicitCallPath(target);

  if (expectedBase == "map" &&
      (isDirectMapConstructorPath(resolvedTarget) ||
       isDirectMapConstructorPath(explicitTarget)) &&
      target.templateArgs.size() == 2) {
    argsOut = target.templateArgs;
    return true;
  }

  for (const std::string *candidatePath : {&resolvedTarget, &explicitTarget}) {
    if (candidatePath->empty()) {
      continue;
    }
    auto defIt = defMap_.find(*candidatePath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      continue;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
        extractCollectionArgsFromBinding(inferredReturn)) {
      return true;
    }
  }

  return false;
}

} // namespace primec::semantics
