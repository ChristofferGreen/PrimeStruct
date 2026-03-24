#include "IrLowererSetupTypeHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

bool resolveExperimentalVectorHelperAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "vectorCount") {
    helperNameOut = "count";
    return true;
  }
  if (helperName == "vectorCapacity") {
    helperNameOut = "capacity";
    return true;
  }
  if (helperName == "vectorAt") {
    helperNameOut = "at";
    return true;
  }
  if (helperName == "vectorAtUnsafe") {
    helperNameOut = "at_unsafe";
    return true;
  }
  if (helperName == "vectorPush") {
    helperNameOut = "push";
    return true;
  }
  if (helperName == "vectorPop") {
    helperNameOut = "pop";
    return true;
  }
  if (helperName == "vectorReserve") {
    helperNameOut = "reserve";
    return true;
  }
  if (helperName == "vectorClear") {
    helperNameOut = "clear";
    return true;
  }
  if (helperName == "vectorRemoveAt") {
    helperNameOut = "remove_at";
    return true;
  }
  if (helperName == "vectorRemoveSwap") {
    helperNameOut = "remove_swap";
    return true;
  }
  return false;
}

bool resolveStdCollectionsVectorWrapperAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "vectorCount") {
    helperNameOut = "count";
    return true;
  }
  if (helperName == "vectorCapacity") {
    helperNameOut = "capacity";
    return true;
  }
  if (helperName == "vectorAt") {
    helperNameOut = "at";
    return true;
  }
  if (helperName == "vectorAtUnsafe") {
    helperNameOut = "at_unsafe";
    return true;
  }
  if (helperName == "vectorPush") {
    helperNameOut = "push";
    return true;
  }
  if (helperName == "vectorPop") {
    helperNameOut = "pop";
    return true;
  }
  if (helperName == "vectorReserve") {
    helperNameOut = "reserve";
    return true;
  }
  if (helperName == "vectorClear") {
    helperNameOut = "clear";
    return true;
  }
  if (helperName == "vectorRemoveAt") {
    helperNameOut = "remove_at";
    return true;
  }
  if (helperName == "vectorRemoveSwap") {
    helperNameOut = "remove_swap";
    return true;
  }
  return false;
}

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool allowsVectorStdlibCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

std::string preferredFileErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  auto hasPath = [&](std::string_view path) { return defMap.find(std::string(path)) != defMap.end(); };
  if (helperName == "why") {
    if (hasPath("/std/file/FileError/why")) {
      return "/std/file/FileError/why";
    }
    if (hasPath("/FileError/why")) {
      return "/FileError/why";
    }
    return "";
  }
  if (helperName == "is_eof") {
    if (hasPath("/std/file/FileError/is_eof")) {
      return "/std/file/FileError/is_eof";
    }
    if (hasPath("/FileError/is_eof")) {
      return "/FileError/is_eof";
    }
    if (hasPath("/std/file/fileErrorIsEof")) {
      return "/std/file/fileErrorIsEof";
    }
    return "";
  }
  if (helperName == "eof") {
    if (hasPath("/std/file/FileError/eof")) {
      return "/std/file/FileError/eof";
    }
    if (hasPath("/FileError/eof")) {
      return "/FileError/eof";
    }
    if (hasPath("/std/file/fileReadEof")) {
      return "/std/file/fileReadEof";
    }
    return "";
  }
  if (helperName == "status") {
    if (hasPath("/std/file/FileError/status")) {
      return "/std/file/FileError/status";
    }
    return "";
  }
  if (helperName == "result") {
    if (hasPath("/std/file/FileError/result")) {
      return "/std/file/FileError/result";
    }
    return "";
  }
  return "";
}

std::string preferredImageErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  auto hasPath = [&](std::string_view path) { return defMap.find(std::string(path)) != defMap.end(); };
  if (helperName == "why") {
    if (hasPath("/std/image/ImageError/why")) {
      return "/std/image/ImageError/why";
    }
    if (hasPath("/ImageError/why")) {
      return "/ImageError/why";
    }
    return "";
  }
  if (helperName == "status") {
    if (hasPath("/std/image/ImageError/status")) {
      return "/std/image/ImageError/status";
    }
    if (hasPath("/ImageError/status")) {
      return "/ImageError/status";
    }
    if (hasPath("/std/image/imageErrorStatus")) {
      return "/std/image/imageErrorStatus";
    }
    return "";
  }
  if (helperName == "result") {
    if (hasPath("/std/image/ImageError/result")) {
      return "/std/image/ImageError/result";
    }
    if (hasPath("/ImageError/result")) {
      return "/ImageError/result";
    }
    if (hasPath("/std/image/imageErrorResult")) {
      return "/std/image/imageErrorResult";
    }
    return "";
  }
  return "";
}

std::string preferredContainerErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  auto hasPath = [&](std::string_view path) { return defMap.find(std::string(path)) != defMap.end(); };
  if (helperName == "why") {
    if (hasPath("/std/collections/ContainerError/why")) {
      return "/std/collections/ContainerError/why";
    }
    if (hasPath("/ContainerError/why")) {
      return "/ContainerError/why";
    }
    return "";
  }
  if (helperName == "status") {
    if (hasPath("/std/collections/ContainerError/status")) {
      return "/std/collections/ContainerError/status";
    }
    if (hasPath("/ContainerError/status")) {
      return "/ContainerError/status";
    }
    if (hasPath("/std/collections/containerErrorStatus")) {
      return "/std/collections/containerErrorStatus";
    }
    return "";
  }
  if (helperName == "result") {
    if (hasPath("/std/collections/ContainerError/result")) {
      return "/std/collections/ContainerError/result";
    }
    if (hasPath("/ContainerError/result")) {
      return "/ContainerError/result";
    }
    if (hasPath("/std/collections/containerErrorResult")) {
      return "/std/collections/containerErrorResult";
    }
    return "";
  }
  return "";
}

std::string preferredGfxErrorHelperTarget(
    std::string_view helperName,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &resolvedTypePath = "") {
  auto helperForBasePath = [&](std::string_view basePath) -> std::string {
    const std::string helperPath = std::string(basePath) + "/" + std::string(helperName);
    return defMap.find(helperPath) != defMap.end() ? helperPath : "";
  };
  const std::string canonicalBase = "/std/gfx/GfxError";
  const std::string experimentalBase = "/std/gfx/experimental/GfxError";
  if (resolvedTypePath == canonicalBase) {
    return helperForBasePath(canonicalBase);
  }
  if (resolvedTypePath == experimentalBase) {
    return helperForBasePath(experimentalBase);
  }
  const bool hasCanonical =
      defMap.find(canonicalBase + "/" + std::string(helperName)) != defMap.end();
  const bool hasExperimental =
      defMap.find(experimentalBase + "/" + std::string(helperName)) != defMap.end();
  if (hasCanonical && !hasExperimental) {
    return helperForBasePath(canonicalBase);
  }
  if (!hasCanonical && hasExperimental) {
    return helperForBasePath(experimentalBase);
  }
  return "";
}

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  const std::string collectionsVectorWrapperPrefix = "std/collections/vector";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(vectorPrefix.size()));
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(arrayPrefix.size()));
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(collectionsVectorWrapperPrefix, 0) == 0 &&
      normalized.rfind(stdVectorPrefix, 0) != 0) {
    return resolveStdCollectionsVectorWrapperAliasName(
        normalized.substr(collectionsVectorWrapperPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    return resolveExperimentalVectorHelperAliasName(normalized.substr(experimentalVectorPrefix.size()), helperNameOut);
  }
  return false;
}

bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(mapPrefix.size());
    return true;
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdMapPrefix.size());
    return true;
  }
  return false;
}

std::string normalizeCollectionHelperPath(const std::string &path);

std::string normalizeCollectionHelperPath(const std::string &path) {
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/experimental_vector/", 0) == 0 ||
        normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }
  return normalizedPath;
}

bool isExplicitRemovedVectorMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(vectorPrefix.size())));
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(arrayPrefix.size())));
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size())));
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    std::string helperName;
    return resolveExperimentalVectorHelperAliasName(normalized.substr(experimentalVectorPrefix.size()), helperName);
  }
  return false;
}

bool isExplicitMapMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  auto isBuiltinMapHelper = [](const std::string &helperName) {
    return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
  };
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return isBuiltinMapHelper(normalized.substr(mapPrefix.size()));
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    return isBuiltinMapHelper(normalized.substr(stdMapPrefix.size()));
  }
  return false;
}

bool isExplicitMapContainsOrTryAtCompatibilityMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string mapPrefix = "map/";
  auto isRemovedCompatibilityMethod = [](const std::string &helperName) {
    return helperName == "contains" || helperName == "tryAt";
  };
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return isRemovedCompatibilityMethod(normalized.substr(mapPrefix.size()));
  }
  return false;
}

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isMapBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isExplicitMapHelperFallbackPath(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  const std::string normalizedPath = normalizeCollectionHelperPath(expr.name);
  return normalizedPath == "/map/count" || normalizedPath == "/map/at" || normalizedPath == "/map/at_unsafe" ||
         normalizedPath == "/std/collections/map/count" || normalizedPath == "/std/collections/map/at" ||
         normalizedPath == "/std/collections/map/at_unsafe";
}

bool isExplicitVectorAccessHelperPath(const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  return normalizedPath == "/vector/at" || normalizedPath == "/vector/at_unsafe" ||
         normalizedPath == "/std/collections/vector/at" ||
         normalizedPath == "/std/collections/vector/at_unsafe";
}

bool isExplicitVectorAccessHelperExpr(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && !expr.name.empty() &&
         isExplicitVectorAccessHelperPath(expr.name);
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path);

bool isBuiltinMapHelperSuffix(const std::string &suffix) {
  return suffix == "count" || suffix == "contains" || suffix == "tryAt" || suffix == "at" ||
         suffix == "at_unsafe";
}

void pruneRemovedMapCompatibilityCallReturnCandidates(std::vector<std::string> &candidates,
                                                      const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  if (normalizedPath.rfind("/map/", 0) != 0) {
    return;
  }

  const std::string suffix = normalizedPath.substr(std::string("/map/").size());
  if (!isBuiltinMapHelperSuffix(suffix)) {
    return;
  }

  const std::string canonicalCandidate = "/std/collections/map/" + suffix;
  for (auto it = candidates.begin(); it != candidates.end();) {
    if (*it == canonicalCandidate) {
      it = candidates.erase(it);
    } else {
      ++it;
    }
  }
}

void pruneRemovedVectorCompatibilityCallReturnCandidates(std::vector<std::string> &candidates,
                                                         const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  if (normalizedPath.rfind("/vector/", 0) != 0) {
    return;
  }

  const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
  if (suffix != "at" && suffix != "at_unsafe") {
    return;
  }

  const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
  for (auto it = candidates.begin(); it != candidates.end();) {
    if (*it == canonicalCandidate) {
      it = candidates.erase(it);
    } else {
      ++it;
    }
  }
}

bool isAllowedResolvedMapDirectCallPath(const std::string &callPath, const std::string &resolvedPath) {
  if (!isExplicitMapMethodAliasPath(callPath)) {
    return true;
  }

  auto allowedCandidates = collectionHelperPathCandidates(callPath);
  pruneRemovedMapCompatibilityCallReturnCandidates(allowedCandidates, callPath);
  const std::string normalizedResolvedPath = normalizeCollectionHelperPath(resolvedPath);
  return std::any_of(allowedCandidates.begin(), allowedCandidates.end(), [&](const std::string &candidate) {
    return candidate == normalizedResolvedPath;
  });
}

bool isAllowedResolvedVectorDirectCallPath(const std::string &callPath, const std::string &resolvedPath) {
  const std::string normalizedCallPath = normalizeCollectionHelperPath(callPath);
  if (normalizedCallPath.rfind("/vector/", 0) != 0 &&
      normalizedCallPath.rfind("/std/collections/vector/", 0) != 0) {
    return true;
  }
  auto allowedCandidates = collectionHelperPathCandidates(callPath);
  pruneRemovedVectorCompatibilityCallReturnCandidates(allowedCandidates, callPath);
  const std::string normalizedResolvedPath = normalizeCollectionHelperPath(resolvedPath);
  return std::any_of(allowedCandidates.begin(), allowedCandidates.end(), [&](const std::string &candidate) {
    return candidate == normalizedResolvedPath;
  });
}

std::string normalizeMapImportAliasPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
    return "/" + path;
  }
  return path;
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path) {
  std::vector<std::string> candidates;
  auto appendUnique = [&](const std::string &candidate) {
    for (const auto &existing : candidates) {
      if (existing == candidate) {
        return;
      }
    }
    candidates.push_back(candidate);
  };

  const std::string normalizedPath = normalizeCollectionHelperPath(path);

  appendUnique(path);
  appendUnique(normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/vector/" + suffix);
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUnique("/std/collections/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUnique("/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUnique("/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    appendUnique("/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      appendUnique("/map/" + suffix);
    }
  }

  return candidates;
}

} // namespace
SetupTypeAdapters makeSetupTypeAdapters() {
  SetupTypeAdapters adapters;
  adapters.valueKindFromTypeName = makeValueKindFromTypeName();
  adapters.combineNumericKinds = makeCombineNumericKinds();
  return adapters;
}

ValueKindFromTypeNameFn makeValueKindFromTypeName() {
  return [](const std::string &name) {
    return valueKindFromTypeName(name);
  };
}

CombineNumericKindsFn makeCombineNumericKinds() {
  return [](LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
    return combineNumericKinds(left, right);
  };
}

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name) {
  if (name == "int" || name == "i32") {
    return LocalInfo::ValueKind::Int32;
  }
  if (name == "i64") {
    return LocalInfo::ValueKind::Int64;
  }
  if (name == "u64") {
    return LocalInfo::ValueKind::UInt64;
  }
  if (name == "float" || name == "f32") {
    return LocalInfo::ValueKind::Float32;
  }
  if (name == "f64") {
    return LocalInfo::ValueKind::Float64;
  }
  if (name == "bool") {
    return LocalInfo::ValueKind::Bool;
  }
  if (name == "string") {
    return LocalInfo::ValueKind::String;
  }
  if (name == "FileError" || name == "ImageError" || name == "ContainerError" ||
      name == "GfxError") {
    return LocalInfo::ValueKind::Int32;
  }

  std::string base;
  std::string arg;
  if (splitTemplateTypeName(name, base, arg) && base == "File") {
    return LocalInfo::ValueKind::Int64;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Unknown || right == LocalInfo::ValueKind::Unknown) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::String || right == LocalInfo::ValueKind::String) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Bool || right == LocalInfo::ValueKind::Bool) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Float32 || right == LocalInfo::ValueKind::Float32 ||
      left == LocalInfo::ValueKind::Float64 || right == LocalInfo::ValueKind::Float64) {
    if (left == LocalInfo::ValueKind::Float32 && right == LocalInfo::ValueKind::Float32) {
      return LocalInfo::ValueKind::Float32;
    }
    if (left == LocalInfo::ValueKind::Float64 && right == LocalInfo::ValueKind::Float64) {
      return LocalInfo::ValueKind::Float64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::UInt64 || right == LocalInfo::ValueKind::UInt64) {
    return (left == LocalInfo::ValueKind::UInt64 && right == LocalInfo::ValueKind::UInt64)
               ? LocalInfo::ValueKind::UInt64
               : LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int64) {
    if ((left == LocalInfo::ValueKind::Int64 || left == LocalInfo::ValueKind::Int32) &&
        (right == LocalInfo::ValueKind::Int64 || right == LocalInfo::ValueKind::Int32)) {
      return LocalInfo::ValueKind::Int64;
    }
    return LocalInfo::ValueKind::Unknown;
  }
  if (left == LocalInfo::ValueKind::Int32 && right == LocalInfo::ValueKind::Int32) {
    return LocalInfo::ValueKind::Int32;
  }
  return LocalInfo::ValueKind::Unknown;
}

LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right) {
  if (left == LocalInfo::ValueKind::Bool) {
    left = LocalInfo::ValueKind::Int32;
  }
  if (right == LocalInfo::ValueKind::Bool) {
    right = LocalInfo::ValueKind::Int32;
  }
  return combineNumericKinds(left, right);
}

std::string typeNameForValueKind(LocalInfo::ValueKind kind) {
  switch (kind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
    case LocalInfo::ValueKind::Float32:
      return "f32";
    case LocalInfo::ValueKind::Float64:
      return "f64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    case LocalInfo::ValueKind::String:
      return "string";
    default:
      return "";
  }
}

std::string normalizeDeclaredCollectionTypeBase(const std::string &base) {
  if (base == "Vector" || base == "std/collections/experimental_vector/Vector" ||
      base == "/std/collections/experimental_vector/Vector" ||
      base.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
      base.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (base == "/map" || base == "std/collections/map" || base == "/std/collections/map") {
    return "map";
  }
  if (base == "Buffer" || base == "std/gfx/Buffer" || base == "/std/gfx/Buffer" ||
      base == "std/gfx/experimental/Buffer" || base == "/std/gfx/experimental/Buffer") {
    return "Buffer";
  }
  return base;
}

bool inferDeclaredReturnCollection(const Definition &definition,
                                   std::string &collectionNameOut,
                                   std::vector<std::string> &collectionArgsOut) {
  collectionNameOut.clear();
  collectionArgsOut.clear();
  auto inferCollectionFromType = [&](const std::string &typeName,
                                     auto &&inferCollectionFromTypeRef) -> bool {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeName, base, argText)) {
      return false;
    }
    base = normalizeDeclaredCollectionTypeBase(base);
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args)) {
      return false;
    }
    if ((base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer") && args.size() == 1) {
      collectionNameOut = base;
      collectionArgsOut = std::move(args);
      return true;
    }
    if (base == "map" && args.size() == 2) {
      collectionNameOut = base;
      collectionArgsOut = std::move(args);
      return true;
    }
    if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
      return inferCollectionFromTypeRef(trimTemplateTypeText(args.front()), inferCollectionFromTypeRef);
    }
    return false;
  };
  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string declaredType = trimTemplateTypeText(transform.templateArgs.front());
    if (declaredType == "auto") {
      break;
    }
    return inferCollectionFromType(declaredType, inferCollectionFromType);
  }
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
    return allowAnyName || isBlockCall(candidate);
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
  auto inferDirectCollectionCall = [&](const Expr &candidate,
                                       std::string &nameOut,
                                       std::vector<std::string> &argsOut) -> bool {
    nameOut.clear();
    argsOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    std::string collection;
    if (getBuiltinCollectionName(candidate, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          candidate.templateArgs.size() == 1) {
        nameOut = collection;
        argsOut = candidate.templateArgs;
        return true;
      }
      if (collection == "map" && candidate.templateArgs.size() == 2) {
        nameOut = "map";
        argsOut = candidate.templateArgs;
        return true;
      }
    }
    auto normalizedName = candidate.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    auto matchesPath = [&](std::string_view basePath) {
      return normalizedName == basePath || normalizedName.rfind(std::string(basePath) + "__", 0) == 0;
    };
    auto isDirectMapConstructor = [&]() {
      return matchesPath("std/collections/map/map") ||
             matchesPath("std/collections/mapNew") ||
             matchesPath("std/collections/mapSingle") ||
             matchesPath("std/collections/mapDouble") ||
             matchesPath("std/collections/mapPair") ||
             matchesPath("std/collections/mapTriple") ||
             matchesPath("std/collections/mapQuad") ||
             matchesPath("std/collections/mapQuint") ||
             matchesPath("std/collections/mapSext") ||
             matchesPath("std/collections/mapSept") ||
             matchesPath("std/collections/mapOct") ||
             matchesPath("std/collections/experimental_map/mapNew") ||
             matchesPath("std/collections/experimental_map/mapSingle") ||
             matchesPath("std/collections/experimental_map/mapDouble") ||
             matchesPath("std/collections/experimental_map/mapPair") ||
             matchesPath("std/collections/experimental_map/mapTriple") ||
             matchesPath("std/collections/experimental_map/mapQuad") ||
             matchesPath("std/collections/experimental_map/mapQuint") ||
             matchesPath("std/collections/experimental_map/mapSext") ||
             matchesPath("std/collections/experimental_map/mapSept") ||
             matchesPath("std/collections/experimental_map/mapOct") ||
             isSimpleCallName(candidate, "mapNew") ||
             isSimpleCallName(candidate, "mapSingle") ||
             isSimpleCallName(candidate, "mapDouble") ||
             isSimpleCallName(candidate, "mapPair") ||
             isSimpleCallName(candidate, "mapTriple") ||
             isSimpleCallName(candidate, "mapQuad") ||
             isSimpleCallName(candidate, "mapQuint") ||
             isSimpleCallName(candidate, "mapSext") ||
             isSimpleCallName(candidate, "mapSept") ||
             isSimpleCallName(candidate, "mapOct");
    };
    auto isDirectVectorConstructor = [&]() {
      return matchesPath("std/collections/vector/vector") ||
             matchesPath("std/collections/vectorNew") ||
             matchesPath("std/collections/vectorSingle") ||
             isSimpleCallName(candidate, "vectorNew") ||
             isSimpleCallName(candidate, "vectorSingle");
    };
    if (isDirectMapConstructor() && candidate.templateArgs.size() == 2) {
      nameOut = "map";
      argsOut = candidate.templateArgs;
      return true;
    }
    if (isDirectMapConstructor() && candidate.args.size() % 2 == 0 && !candidate.args.empty()) {
      auto inferLiteralType = [&](const Expr &value, std::string &typeOut) -> bool {
        typeOut.clear();
        if (value.kind == Expr::Kind::Literal) {
          typeOut = value.isUnsigned ? "u64" : (value.intWidth == 64 ? "i64" : "i32");
          return true;
        }
        if (value.kind == Expr::Kind::BoolLiteral) {
          typeOut = "bool";
          return true;
        }
        if (value.kind == Expr::Kind::FloatLiteral) {
          typeOut = value.floatWidth == 64 ? "f64" : "f32";
          return true;
        }
        if (value.kind == Expr::Kind::StringLiteral) {
          typeOut = "string";
          return true;
        }
        return false;
      };
      std::string keyType;
      std::string valueType;
      for (size_t i = 0; i < candidate.args.size(); i += 2) {
        std::string currentKeyType;
        std::string currentValueType;
        if (!inferLiteralType(candidate.args[i], currentKeyType) ||
            !inferLiteralType(candidate.args[i + 1], currentValueType)) {
          return false;
        }
        if (keyType.empty()) {
          keyType = currentKeyType;
        } else if (keyType != currentKeyType) {
          return false;
        }
        if (valueType.empty()) {
          valueType = currentValueType;
        } else if (valueType != currentValueType) {
          return false;
        }
      }
      nameOut = "map";
      argsOut = {keyType, valueType};
      return true;
    }
    if (isDirectVectorConstructor() && candidate.templateArgs.size() == 1) {
      nameOut = "vector";
      argsOut = candidate.templateArgs;
      return true;
    }
    return false;
  };
  std::function<bool(const Expr &)> inferCollectionFromExpr;
  inferCollectionFromExpr = [&](const Expr &candidate) -> bool {
    if (isIfCall(candidate) && candidate.args.size() == 3) {
      const Expr &thenArg = candidate.args[1];
      const Expr &elseArg = candidate.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      std::string thenName;
      std::vector<std::string> thenArgs;
      std::string elseName;
      std::vector<std::string> elseArgs;
      if (!inferCollectionFromExpr(thenValue ? *thenValue : thenArg)) {
        return false;
      }
      thenName = collectionNameOut;
      thenArgs = collectionArgsOut;
      if (!inferCollectionFromExpr(elseValue ? *elseValue : elseArg)) {
        return false;
      }
      elseName = collectionNameOut;
      elseArgs = collectionArgsOut;
      if (thenName != elseName || thenArgs != elseArgs) {
        return false;
      }
      collectionNameOut = std::move(thenName);
      collectionArgsOut = std::move(thenArgs);
      return true;
    }
    if (const Expr *valueExpr = getEnvelopeValueExpr(candidate, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferCollectionFromExpr(valueExpr->args.front());
      }
      return inferCollectionFromExpr(*valueExpr);
    }
    return inferDirectCollectionCall(candidate, collectionNameOut, collectionArgsOut);
  };
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
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
  if (definition.returnExpr.has_value()) {
    valueExpr = &*definition.returnExpr;
  }
  if (valueExpr != nullptr && inferCollectionFromExpr(*valueExpr)) {
    return true;
  }
  return false;
}
bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut) {
  std::vector<std::string> collectionArgs;
  if (inferDeclaredReturnCollection(definition, typeNameOut, collectionArgs)) {
    return true;
  }

  for (const auto &transform : definition.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &declaredType = transform.templateArgs.front();
    if (declaredType.empty() || declaredType == "void" || declaredType == "auto") {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(declaredType, base, argText)) {
      // Non-collection templated returns are not method receiver targets.
      return false;
    }
    typeNameOut = declaredType;
    if (!typeNameOut.empty() && typeNameOut.front() == '/') {
      typeNameOut.erase(0, 1);
    }
    return !typeNameOut.empty();
  }
  return false;
}

} // namespace primec::ir_lowerer
