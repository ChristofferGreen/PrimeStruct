#include "IrLowererSetupTypeCollectionHelpers.h"

#include <algorithm>
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

std::string normalizeCollectionHelperPath(const std::string &path);

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

bool matchesRegistrySpellingSet(std::span<const std::string_view> spellings,
                                std::string_view spelling) {
  return std::any_of(
      spellings.begin(), spellings.end(), [&](std::string_view candidate) {
        return candidate == spelling;
      });
}

std::string normalizePublishedCollectionPath(std::string path) {
  path = normalizeCollectionHelperPath(path);
  if (!path.empty() && path.front() != '/' &&
      (path.rfind("std/collections/", 0) == 0 ||
       path.rfind("map/", 0) == 0 ||
       path.rfind("vector/", 0) == 0)) {
    path.insert(path.begin(), '/');
  }
  return path;
}

std::string stripCollectionConstructorPathSuffix(std::string path) {
  const size_t leafStart = path.find_last_of('/');
  const size_t suffixStart = path.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (suffixStart != std::string::npos) {
    path.erase(suffixStart);
  }
  return path;
}

const StdlibSurfaceMetadata *findPublishedStdlibSurfaceMetadata(std::string_view path,
                                                                StdlibSurfaceId surfaceId) {
  if (const auto *metadata = findStdlibSurfaceMetadataBySpelling(path);
      metadata != nullptr && metadata->id == surfaceId) {
    return metadata;
  }
  if (const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(path);
      metadata != nullptr && metadata->id == surfaceId) {
    return metadata;
  }
  return nullptr;
}

bool matchesResolvedRootedPublishedCollectionMemberPath(
    std::string_view path,
    std::string_view rootPath,
    std::span<const std::string_view> memberNames) {
  if (rootPath.empty() || path.size() <= rootPath.size() ||
      path.rfind(rootPath, 0) != 0 || path[rootPath.size()] != '/') {
    return false;
  }
  const std::string memberPath =
      stripCollectionConstructorPathSuffix(std::string(path.substr(rootPath.size() + 1)));
  if (memberPath.empty() || memberPath.find('/') != std::string::npos) {
    return false;
  }
  return std::find(memberNames.begin(), memberNames.end(), memberPath) != memberNames.end();
}

std::string rebuildScopedCollectionHelperPath(const Expr &expr) {
  std::string normalized = expr.name;
  if (!expr.namespacePrefix.empty() && normalized.find('/') == std::string::npos) {
    std::string scopedPrefix = expr.namespacePrefix;
    if (!scopedPrefix.empty() && scopedPrefix.front() != '/') {
      scopedPrefix.insert(scopedPrefix.begin(), '/');
    }
    if (!scopedPrefix.empty()) {
      normalized = scopedPrefix + "/" + normalized;
    }
  }
  return normalizeCollectionHelperPath(normalized);
}

bool isBorrowedMapHelperSurface(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolvePublishedStdlibSurfaceExprMemberName(
             expr,
             StdlibSurfaceId::CollectionsMapHelpers,
             helperName) &&
         helperName.ends_with("_ref");
}

} // namespace

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
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
  std::string canonicalMemberName;
  return resolvePublishedStdlibSurfaceMemberToken(
             helperName,
             StdlibSurfaceId::CollectionsVectorHelpers,
             canonicalMemberName) &&
         canonicalMemberName == stripGeneratedHelperSuffix(helperName);
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = rebuildScopedCollectionHelperPath(expr);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("vector/", 0) == 0) {
    return false;
  }
  if (resolvePublishedStdlibSurfaceExprMemberName(
          expr,
          StdlibSurfaceId::CollectionsVectorHelpers,
          helperNameOut)) {
    return true;
  }
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string stdSoaVectorPrefix = "std/collections/soa_vector/";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(arrayPrefix.size()));
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size()));
    return true;
  }
  if (normalized.rfind(stdSoaVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(
        normalized.substr(stdSoaVectorPrefix.size()));
    return helperNameOut == "count";
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    return resolvePublishedStdlibSurfaceExprMemberName(
        expr,
        StdlibSurfaceId::CollectionsVectorHelpers,
        helperNameOut);
  }
  return false;
}

bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  return resolvePublishedStdlibSurfaceExprMemberName(
      expr,
      StdlibSurfaceId::CollectionsMapHelpers,
      helperNameOut);
}

bool resolveBorrowedMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  return resolvePublishedStdlibSurfaceExprMemberName(
             expr,
             StdlibSurfaceId::CollectionsMapHelpers,
             helperNameOut) &&
         helperNameOut.ends_with("_ref");
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
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  const std::string experimentalVectorPrefix = "std/collections/experimental_vector/";
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(arrayPrefix.size())));
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size())));
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    std::string helperName;
    return resolvePublishedStdlibSurfaceMemberToken(
        normalized.substr(experimentalVectorPrefix.size()),
        StdlibSurfaceId::CollectionsVectorHelpers,
        helperName);
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
    return resolvePublishedStdlibSurfaceMemberToken(
               normalized.substr(mapPrefix.size()),
               StdlibSurfaceId::CollectionsMapHelpers,
               helperName) &&
           (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe" || helperName == "insert");
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    std::string helperName;
    return resolvePublishedStdlibSurfaceMemberToken(
               normalized.substr(stdMapPrefix.size()),
               StdlibSurfaceId::CollectionsMapHelpers,
               helperName) &&
           (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe" || helperName == "insert");
  }
  return false;
}

bool isExplicitMapContainsOrTryAtMethodPath(const std::string &methodName) {
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
    return resolvePublishedStdlibSurfaceMemberToken(
               normalized.substr(mapPrefix.size()),
               StdlibSurfaceId::CollectionsMapHelpers,
               helperName) &&
           (helperName == "contains" || helperName == "tryAt");
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    std::string helperName;
    return resolvePublishedStdlibSurfaceMemberToken(
               normalized.substr(stdMapPrefix.size()),
               StdlibSurfaceId::CollectionsMapHelpers,
               helperName) &&
           (helperName == "contains" || helperName == "tryAt");
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
  return normalizedPath == "/std/collections/vector/at" ||
         normalizedPath == "/std/collections/vector/at_unsafe";
}

bool isExplicitVectorAccessHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolvePublishedStdlibSurfaceExprMemberName(
             expr,
             StdlibSurfaceId::CollectionsVectorHelpers,
             helperName) &&
         (helperName == "at" || helperName == "at_unsafe");
}

bool isExplicitVectorReceiverProbeHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolvePublishedStdlibSurfaceExprMemberName(
             expr,
             StdlibSurfaceId::CollectionsVectorHelpers,
             helperName) &&
         (helperName == "at" || helperName == "at_unsafe" ||
          helperName == "count" || helperName == "capacity");
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
  if (normalizedCallPath.rfind("/std/collections/vector/", 0) != 0) {
    return true;
  }
  auto allowedCandidates = collectionHelperPathCandidates(callPath);
  const std::string normalizedResolvedPath = normalizeCollectionHelperPath(resolvedPath);
  return std::any_of(allowedCandidates.begin(), allowedCandidates.end(), [&](const std::string &candidate) {
    return candidate == normalizedResolvedPath;
  });
}

bool resolvePublishedStdlibSurfaceMemberToken(std::string_view memberToken,
                                              StdlibSurfaceId surfaceId,
                                              std::string &memberNameOut) {
  memberNameOut.clear();
  const auto *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedToken =
      stripGeneratedHelperSuffix(std::string(memberToken));
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, normalizedToken);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

bool resolvePublishedStdlibSurfaceExprMemberName(const Expr &expr,
                                                 StdlibSurfaceId surfaceId,
                                                 std::string &memberNameOut) {
  memberNameOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  if (expr.namespacePrefix.empty() && expr.name.find('/') == std::string::npos) {
    return false;
  }

  const std::string normalizedPath = rebuildScopedCollectionHelperPath(expr);
  if (resolvePublishedStdlibSurfaceMemberName(
          normalizedPath, surfaceId, memberNameOut)) {
    return true;
  }

  if (surfaceId == StdlibSurfaceId::CollectionsMapHelpers &&
      normalizedPath.rfind("/std/collections/Map", 0) == 0) {
    return resolvePublishedStdlibSurfaceMemberToken(
        normalizedPath.substr(std::string("/std/collections/Map").size()),
        surfaceId,
        memberNameOut);
  }

  return false;
}

bool resolvePublishedStdlibSurfaceConstructorMemberName(std::string_view path,
                                                        StdlibSurfaceId surfaceId,
                                                        std::string &memberNameOut) {
  memberNameOut.clear();
  const auto *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr ||
      metadata->shape != StdlibSurfaceShape::ConstructorFamily) {
    return false;
  }

  const std::string normalizedPath =
      stripCollectionConstructorPathSuffix(
          normalizePublishedCollectionPath(std::string(path)));
  const auto matchesRootedMemberPath = [&](std::string_view rootPath) {
    return matchesResolvedRootedPublishedCollectionMemberPath(
        normalizedPath,
        rootPath,
        metadata->memberNames);
  };

  if (!stdlibSurfaceMatchesSpelling(*metadata, normalizedPath) &&
      !matchesRootedMemberPath(metadata->canonicalPath)) {
    bool matchedImportAliasRoot = false;
    for (const std::string_view alias : metadata->importAliasSpellings) {
      if (matchesRootedMemberPath(alias)) {
        matchedImportAliasRoot = true;
        break;
      }
    }
    if (!matchedImportAliasRoot) {
      return false;
    }
  }

  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(*metadata, normalizedPath);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

bool resolvePublishedStdlibSurfaceConstructorExprMemberName(const Expr &expr,
                                                            StdlibSurfaceId surfaceId,
                                                            std::string &memberNameOut) {
  memberNameOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }

  if (resolvePublishedStdlibSurfaceConstructorMemberName(
          rebuildScopedCollectionHelperPath(expr),
          surfaceId,
          memberNameOut)) {
    return true;
  }

  if (!expr.namespacePrefix.empty() || expr.name.find('/') != std::string::npos) {
    return false;
  }

  if (!resolvePublishedStdlibSurfaceMemberToken(expr.name, surfaceId, memberNameOut)) {
    return false;
  }
  return memberNameOut != "map" && memberNameOut != "vector";
}

bool isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(std::string_view path,
                                                              StdlibSurfaceId surfaceId) {
  std::string memberName;
  if (!resolvePublishedStdlibSurfaceConstructorMemberName(path, surfaceId, memberName)) {
    return false;
  }
  const auto *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedPath =
      stripCollectionConstructorPathSuffix(
          normalizePublishedCollectionPath(std::string(path)));
  return !matchesRegistrySpellingSet(metadata->compatibilitySpellings, normalizedPath);
}

bool isPublishedStdlibSurfaceConstructorExpr(const Expr &expr,
                                             StdlibSurfaceId surfaceId) {
  std::string memberName;
  return resolvePublishedStdlibSurfaceConstructorExprMemberName(
      expr, surfaceId, memberName);
}

std::string inferPublishedExperimentalMapStructPathFromConstructorPath(std::string_view path) {
  std::string memberName;
  if (!resolvePublishedStdlibSurfaceConstructorMemberName(
          path,
          StdlibSurfaceId::CollectionsMapConstructors,
          memberName) ||
      memberName == "map") {
    return "";
  }

  const std::string normalizedPath =
      normalizePublishedCollectionPath(std::string(path));
  const size_t slash = normalizedPath.find_last_of('/');
  const std::string leaf =
      slash == std::string::npos ? normalizedPath : normalizedPath.substr(slash + 1);
  const std::string memberPrefix = memberName + "__";
  if (leaf.rfind(memberPrefix, 0) != 0) {
    return "";
  }
  return "/std/collections/experimental_map/Map__" +
         leaf.substr(memberPrefix.size());
}

bool resolvePublishedSemanticStdlibSurfaceMemberName(const SemanticProgram *semanticProgram,
                                                     const Expr &expr,
                                                     StdlibSurfaceId surfaceId,
                                                     std::string &memberNameOut) {
  memberNameOut.clear();
  if (semanticProgram == nullptr || expr.kind != Expr::Kind::Call) {
    return false;
  }

  const auto trySemanticSurface = [&](std::optional<StdlibSurfaceId> semanticSurfaceId,
                                      const std::string &resolvedPath) {
    if (!semanticSurfaceId.has_value() || *semanticSurfaceId != surfaceId) {
      return false;
    }
    return (!resolvedPath.empty() &&
            resolvePublishedStdlibSurfaceMemberName(
                resolvedPath, surfaceId, memberNameOut)) ||
           resolvePublishedStdlibSurfaceExprMemberName(
               expr, surfaceId, memberNameOut);
  };

  return trySemanticSurface(
             findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr),
             findSemanticProductBridgePathChoice(semanticProgram, expr)) ||
         trySemanticSurface(
             findSemanticProductMethodCallStdlibSurfaceId(semanticProgram, expr),
             findSemanticProductMethodCallTarget(semanticProgram, expr)) ||
         trySemanticSurface(
             findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr),
             findSemanticProductDirectCallTarget(semanticProgram, expr));
}

bool resolvePublishedStdlibSurfaceMemberName(std::string_view path,
                                             StdlibSurfaceId surfaceId,
                                             std::string &memberNameOut) {
  memberNameOut.clear();
  const auto *metadata = findPublishedStdlibSurfaceMetadata(path, surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view memberName = resolveStdlibSurfaceMemberName(*metadata, path);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

bool isPublishedStdlibSurfaceLoweringPath(std::string_view path,
                                          StdlibSurfaceId surfaceId) {
  const auto *metadata = findPublishedStdlibSurfaceMetadata(path, surfaceId);
  return metadata != nullptr &&
         matchesRegistrySpellingSet(metadata->loweringSpellings, path);
}

bool isCanonicalPublishedStdlibSurfaceHelperPath(std::string_view path,
                                                 StdlibSurfaceId surfaceId) {
  const auto *metadata = findPublishedStdlibSurfaceMetadata(path, surfaceId);
  if (metadata == nullptr || metadata->canonicalImportRoot.empty()) {
    return false;
  }
  const std::string canonicalPrefix = std::string(metadata->canonicalImportRoot) + "/";
  return path.rfind(canonicalPrefix, 0) == 0 ||
         (matchesRegistrySpellingSet(metadata->loweringSpellings, path) &&
          path.rfind(canonicalPrefix, 0) == 0);
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
      appendUnique("/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/soa_vector/", 0) == 0) {
    appendUnique("/std/collections/soa_vector/" +
                 normalizedPath.substr(std::string("/soa_vector/").size()));
  } else if (normalizedPath.rfind("/std/collections/soa_vector/", 0) == 0) {
    appendUnique("/soa_vector/" +
                 normalizedPath.substr(std::string("/std/collections/soa_vector/").size()));
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
