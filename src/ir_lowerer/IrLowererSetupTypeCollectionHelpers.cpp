#include "IrLowererSetupTypeCollectionHelpers.h"

#include <algorithm>
#include "IrLowererHelpers.h"

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

bool resolveCollectionsMapWrapperAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "count" || helperName == "count_ref" ||
      helperName == "Count" || helperName == "CountRef" ||
      helperName == "mapCount" || helperName == "mapCountRef") {
    helperNameOut = "count";
    return true;
  }
  if (helperName == "contains" || helperName == "contains_ref" ||
      helperName == "Contains" || helperName == "ContainsRef" ||
      helperName == "mapContains" || helperName == "mapContainsRef") {
    helperNameOut = "contains";
    return true;
  }
  if (helperName == "tryAt" || helperName == "tryAt_ref" ||
      helperName == "TryAt" || helperName == "TryAtRef" ||
      helperName == "mapTryAt" || helperName == "mapTryAtRef") {
    helperNameOut = "tryAt";
    return true;
  }
  if (helperName == "at" || helperName == "at_ref" ||
      helperName == "At" || helperName == "AtRef" ||
      helperName == "mapAt" || helperName == "mapAtRef") {
    helperNameOut = "at";
    return true;
  }
  if (helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
      helperName == "AtUnsafe" || helperName == "AtUnsafeRef" ||
      helperName == "mapAtUnsafe" || helperName == "mapAtUnsafeRef") {
    helperNameOut = "at_unsafe";
    return true;
  }
  if (helperName == "insert" || helperName == "insert_ref" ||
      helperName == "Insert" || helperName == "InsertRef" ||
      helperName == "mapInsert" || helperName == "mapInsertRef" ||
      helperName == "MapInsert" || helperName == "MapInsertRef") {
    helperNameOut = "insert";
    return true;
  }
  return false;
}

bool resolveCollectionsBorrowedMapWrapperAliasName(std::string helperName, std::string &helperNameOut) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == "count_ref" || helperName == "CountRef" || helperName == "mapCountRef") {
    helperNameOut = "count_ref";
    return true;
  }
  if (helperName == "contains_ref" || helperName == "ContainsRef" || helperName == "mapContainsRef") {
    helperNameOut = "contains_ref";
    return true;
  }
  if (helperName == "tryAt_ref" || helperName == "TryAtRef" || helperName == "mapTryAtRef") {
    helperNameOut = "tryAt_ref";
    return true;
  }
  if (helperName == "at_ref" || helperName == "AtRef" || helperName == "mapAtRef") {
    helperNameOut = "at_ref";
    return true;
  }
  if (helperName == "at_unsafe_ref" || helperName == "AtUnsafeRef" || helperName == "mapAtUnsafeRef") {
    helperNameOut = "at_unsafe_ref";
    return true;
  }
  if (helperName == "insert_ref" || helperName == "InsertRef" ||
      helperName == "mapInsertRef" || helperName == "MapInsertRef") {
    helperNameOut = "insert_ref";
    return true;
  }
  return false;
}

bool isBorrowedMapHelperSurface(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string normalizedName = expr.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  normalizedName = stripGeneratedHelperSuffix(std::move(normalizedName));
  const size_t lastSlash = normalizedName.find_last_of('/');
  const std::string helperName =
      lastSlash == std::string::npos ? normalizedName : normalizedName.substr(lastSlash + 1);
  return helperName == "count_ref" || helperName == "contains_ref" ||
         helperName == "tryAt_ref" || helperName == "at_ref" ||
         helperName == "at_unsafe_ref" || helperName == "insert_ref" ||
         helperName == "CountRef" || helperName == "ContainsRef" ||
         helperName == "TryAtRef" || helperName == "AtRef" ||
         helperName == "AtUnsafeRef" || helperName == "InsertRef" ||
         helperName == "mapCountRef" || helperName == "mapContainsRef" ||
         helperName == "mapTryAtRef" || helperName == "mapAtRef" ||
         helperName == "mapAtUnsafeRef" || helperName == "mapInsertRef" ||
         helperName == "MapInsertRef";
}

} // namespace

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
    if (hasPath("/std/file/fileErrorStatus")) {
      return "/std/file/fileErrorStatus";
    }
    return "";
  }
  if (helperName == "result") {
    if (hasPath("/std/file/FileError/result")) {
      return "/std/file/FileError/result";
    }
    if (hasPath("/std/file/fileErrorResult")) {
      return "/std/file/fileErrorResult";
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
    const std::string &resolvedTypePath) {
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
    if (helperNameOut == "count" || helperNameOut == "capacity") {
      return true;
    }
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
  const std::string collectionsMapWrapperPrefix = "std/collections/map";
  const std::string experimentalMapPrefix = "std/collections/experimental_map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(mapPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(stdMapPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(collectionsMapWrapperPrefix, 0) == 0 &&
      normalized.rfind(stdMapPrefix, 0) != 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(collectionsMapWrapperPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(experimentalMapPrefix, 0) == 0) {
    return resolveCollectionsMapWrapperAliasName(
        normalized.substr(experimentalMapPrefix.size()), helperNameOut);
  }
  return false;
}

bool resolveBorrowedMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
  const std::string collectionsMapWrapperPrefix = "std/collections/map";
  const std::string experimentalMapPrefix = "std/collections/experimental_map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    return resolveCollectionsBorrowedMapWrapperAliasName(
        normalized.substr(mapPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    return resolveCollectionsBorrowedMapWrapperAliasName(
        normalized.substr(stdMapPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(collectionsMapWrapperPrefix, 0) == 0 &&
      normalized.rfind(stdMapPrefix, 0) != 0) {
    return resolveCollectionsBorrowedMapWrapperAliasName(
        normalized.substr(collectionsMapWrapperPrefix.size()), helperNameOut);
  }
  if (normalized.rfind(experimentalMapPrefix, 0) == 0) {
    return resolveCollectionsBorrowedMapWrapperAliasName(
        normalized.substr(experimentalMapPrefix.size()), helperNameOut);
  }
  return false;
}

std::string normalizeCollectionHelperPath(const std::string &path) {
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/experimental_vector/", 0) == 0 ||
        normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/experimental_map/", 0) == 0) {
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
  if (normalized.rfind(mapPrefix, 0) == 0) {
    std::string helperName;
    return resolveCollectionsMapWrapperAliasName(normalized.substr(mapPrefix.size()), helperName) &&
           (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe" || helperName == "insert");
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    std::string helperName;
    return resolveCollectionsMapWrapperAliasName(normalized.substr(stdMapPrefix.size()), helperName) &&
           (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe" || helperName == "insert");
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
  const std::string stdMapPrefix = "std/collections/map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    std::string helperName;
    return resolveCollectionsMapWrapperAliasName(normalized.substr(mapPrefix.size()), helperName) &&
           (helperName == "contains" || helperName == "tryAt" ||
            helperName == "insert");
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
  if (isBorrowedMapHelperSurface(expr)) {
    return false;
  }
  std::string helperName;
  return resolveMapHelperAliasName(expr, helperName) &&
         (helperName == "count" || helperName == "contains" ||
          helperName == "tryAt" || helperName == "at" ||
          helperName == "at_unsafe" || helperName == "insert");
}

bool isExplicitMapReceiverProbeHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolveMapHelperAliasName(expr, helperName) &&
         (helperName == "count" || helperName == "contains" ||
          helperName == "at" || helperName == "at_unsafe" ||
          helperName == "tryAt" || helperName == "insert");
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

bool isExplicitVectorReceiverProbeHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (isExplicitVectorAccessHelperExpr(expr)) {
    return true;
  }
  const std::string normalizedPath = normalizeCollectionHelperPath(expr.name);
  return normalizedPath == "/vector/count" || normalizedPath == "/vector/capacity" ||
         normalizedPath == "/std/collections/vector/count" ||
         normalizedPath == "/std/collections/vector/capacity";
}

bool isAllowedResolvedMapDirectCallPath(const std::string &callPath, const std::string &resolvedPath) {
  if (!isExplicitMapMethodAliasPath(callPath)) {
    return true;
  }

  auto allowedCandidates = collectionHelperPathCandidates(callPath);
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
  const std::string normalizedResolvedPath = normalizeCollectionHelperPath(resolvedPath);
  return std::any_of(allowedCandidates.begin(), allowedCandidates.end(), [&](const std::string &candidate) {
    return candidate == normalizedResolvedPath;
  });
}

std::string normalizeMapImportAliasPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0 ||
      path.rfind("std/collections/experimental_map/", 0) == 0) {
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
        suffix != "at" && suffix != "at_unsafe" && suffix != "insert") {
      appendUnique("/map/" + suffix);
    }
  }

  return candidates;
}

bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut) {
  collectionOut.clear();
  helperOut.clear();

  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }

  if (resolveVectorHelperAliasName(expr, helperOut)) {
    collectionOut = "vector";
    return true;
  }
  if (resolveMapHelperAliasName(expr, helperOut)) {
    collectionOut = "map";
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
