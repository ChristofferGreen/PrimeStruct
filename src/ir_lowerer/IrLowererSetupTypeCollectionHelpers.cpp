// soa-surface-audit: exempt
#include "IrLowererSetupTypeCollectionHelpers.h"

#include <algorithm>
#include <optional>
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "primec/StdlibSurfaceRegistry.h"
#include "primec/StdlibCollectionPaths.h"

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
       path.rfind(keyValueCollectionAliasRoot(false) + "/", 0) == 0 ||
       path.rfind(std::string("vector") + "/", 0) == 0)) {
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

const StdlibSurfaceMetadata *findCollectionSurfaceMetadataByCanonicalPath(
    std::string_view canonicalPath,
    StdlibSurfaceShape expectedShape) {
  const auto *metadata = findStdlibSurfaceMetadataByCanonicalPath(canonicalPath);
  if (metadata == nullptr ||
      metadata->domain != StdlibSurfaceDomain::Collections ||
      metadata->shape != expectedShape) {
    return nullptr;
  }
  return metadata;
}

const StdlibSurfaceMetadata *findCollectionConstructorSurfaceMetadataForHelper(
    const StdlibSurfaceMetadata &helperMetadata) {
  if (helperMetadata.domain != StdlibSurfaceDomain::Collections ||
      helperMetadata.shape != StdlibSurfaceShape::HelperFamily ||
      helperMetadata.canonicalPath.empty()) {
    return nullptr;
  }
  for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
    if (metadata.domain != StdlibSurfaceDomain::Collections ||
        metadata.shape != StdlibSurfaceShape::ConstructorFamily) {
      continue;
    }
    if (matchesRegistrySpellingSet(metadata.importAliasSpellings,
                                   helperMetadata.canonicalPath)) {
      return &metadata;
    }
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

std::optional<StdlibSurfaceId> keyValueHelperSurfaceId() {
  const auto *metadata =
      keyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return std::nullopt;
  }
  return metadata->id;
}

std::optional<StdlibSurfaceId> keyValueConstructorSurfaceId() {
  const auto *metadata =
      keyValueConstructorSurfaceMetadata();
  if (metadata == nullptr) {
    return std::nullopt;
  }
  return metadata->id;
}

bool isKeyValueHelperSurfaceId(StdlibSurfaceId surfaceId) {
  const std::optional<StdlibSurfaceId> keyValueSurfaceId = keyValueHelperSurfaceId();
  return keyValueSurfaceId.has_value() && surfaceId == *keyValueSurfaceId;
}

bool resolveKeyValueSurfaceMemberToken(std::string_view memberToken,
                                       std::string &memberNameOut) {
  const std::optional<StdlibSurfaceId> surfaceId = keyValueHelperSurfaceId();
  return surfaceId.has_value() &&
         resolvePublishedStdlibSurfaceMemberToken(
             memberToken, *surfaceId, memberNameOut);
}

bool isBorrowedKeyValueHelperSurface(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  const std::optional<StdlibSurfaceId> surfaceId = keyValueHelperSurfaceId();
  if (!surfaceId.has_value()) {
    return false;
  }
  std::string helperName;
  return resolvePublishedStdlibSurfaceExprMemberName(expr, *surfaceId,
                                                     helperName) &&
         helperName.ends_with("_ref");
}

std::optional<StdlibSurfaceId> vectorHelperSurfaceId() {
  const auto *metadata =
      vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return std::nullopt;
  }
  return metadata->id;
}

bool resolveVectorSurfaceMemberToken(std::string_view memberToken,
                                     std::string &memberNameOut) {
  const std::optional<StdlibSurfaceId> surfaceId = vectorHelperSurfaceId();
  return surfaceId.has_value() &&
         resolvePublishedStdlibSurfaceMemberToken(
             memberToken, *surfaceId, memberNameOut);
}

bool resolveVectorSurfaceExprMemberName(const Expr &expr,
                                        std::string &memberNameOut) {
  const std::optional<StdlibSurfaceId> surfaceId = vectorHelperSurfaceId();
  return surfaceId.has_value() &&
         resolvePublishedStdlibSurfaceExprMemberName(
             expr, *surfaceId, memberNameOut);
}

} // namespace

const StdlibSurfaceMetadata *vectorHelperSurfaceMetadata() {
  return findCollectionSurfaceMetadataByCanonicalPath(
      collectionTypePath("vector"),
      StdlibSurfaceShape::HelperFamily);
}

const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadata() {
  return findCollectionSurfaceMetadataByCanonicalPath(
      collectionTypePath("map"),
      StdlibSurfaceShape::HelperFamily);
}

const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadata() {
  const auto *helperMetadata = keyValueHelperSurfaceMetadata();
  return helperMetadata == nullptr
             ? nullptr
             : findCollectionConstructorSurfaceMetadataForHelper(*helperMetadata);
}

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
  return resolveVectorSurfaceMemberToken(helperName, canonicalMemberName) &&
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
  if (normalized.rfind(std::string("vector") + "/", 0) == 0) {
    return false;
  }
  if (resolveVectorSurfaceExprMemberName(expr, helperNameOut)) {
    return true;
  }
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = collectionMemberRoot("vector", false);
  const std::string stdSoaVectorPrefix = "std/collections/soa_vector/";
  const std::string internalSoaVectorPrefix = collection_paths::modulePrefixBare(collection_paths::kInternalSoaVectorFolder);
  const std::string experimentalSoaVectorPrefix = collection_paths::modulePrefixBare(collection_paths::kExperimentalSoaVectorFolder);
  const std::string experimentalVectorPrefix =
      experimentalCollectionMemberRoot("vector", false);
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
    if (helperNameOut == "soaVectorCount") {
      helperNameOut = "count";
    } else if (helperNameOut == "soaVectorCountRef") {
      helperNameOut = "count_ref";
    }
    return helperNameOut == "count" ||
           helperNameOut == "count_ref" ||
           helperNameOut == "get" ||
           helperNameOut == "get_ref" ||
           helperNameOut == "ref" ||
           helperNameOut == "ref_ref";
  }
  if (normalized.rfind(experimentalSoaVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(
        normalized.substr(experimentalSoaVectorPrefix.size()));
    if (helperNameOut == "soaVectorCount") {
      helperNameOut = "count";
      return true;
    }
    if (helperNameOut == "soaVectorCountRef") {
      helperNameOut = "count_ref";
      return true;
    }
    return false;
  }
  if (normalized.rfind(internalSoaVectorPrefix, 0) == 0) {
    helperNameOut = stripGeneratedHelperSuffix(
        normalized.substr(internalSoaVectorPrefix.size()));
    if (helperNameOut == "soaVectorCount") {
      helperNameOut = "count";
      return true;
    }
    if (helperNameOut == "soaVectorCountRef") {
      helperNameOut = "count_ref";
      return true;
    }
    return false;
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    return resolveVectorSurfaceExprMemberName(expr, helperNameOut);
  }
  return false;
}

bool resolveKeyValueHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  (void)expr;
  helperNameOut.clear();
  return false;
}

bool resolveBorrowedKeyValueHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  (void)expr;
  helperNameOut.clear();
  return false;
}

std::string stdCollectionsRoot(bool leadingSlash) {
  return leadingSlash ? "/std/collections" : "std/collections";
}

std::string collectionTypePath(std::string_view collectionName,
                               bool leadingSlash) {
  return stdCollectionsRoot(leadingSlash) + "/" + std::string(collectionName);
}

std::string collectionMemberRoot(std::string_view collectionName,
                                 bool leadingSlash) {
  return collectionTypePath(collectionName, leadingSlash) + "/";
}

std::string collectionMemberPath(std::string_view collectionName,
                                 std::string_view memberName,
                                 bool leadingSlash) {
  return collectionMemberRoot(collectionName, leadingSlash) +
         std::string(memberName);
}

std::string canonicalKeyValueHelperPath(std::string_view memberName,
                                        bool leadingSlash) {
  const auto *metadata =
      keyValueHelperSurfaceMetadata();
  std::string path = metadata == nullptr ? "/std/collections/map"
                                         : std::string(metadata->canonicalPath);
  path += "/";
  path += std::string(memberName);
  if (!leadingSlash && !path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

std::string canonicalKeyValueConstructorPath(bool leadingSlash) {
  const auto *metadata =
      keyValueConstructorSurfaceMetadata();
  std::string path = metadata == nullptr ? "/std/collections/map/map"
                                         : std::string(metadata->canonicalPath);
  if (!leadingSlash && !path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

std::string experimentalCollectionMemberRoot(std::string_view collectionName,
                                             bool leadingSlash) {
  return stdCollectionsRoot(leadingSlash) + "/" +
         collection_paths::experimentalFolder(collectionName) + "/";
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName,
                                           bool leadingSlash) {
  return stdCollectionsRoot(leadingSlash) + "/" +
         collection_paths::typeIdentityFolder(collectionName) + "/" +
         std::string(typeName);
}

std::string collectionWrapperAlias(std::string_view collectionName,
                                   std::string_view suffix) {
  return std::string(collectionName) + std::string(suffix);
}

std::string keyValueCollectionAliasRoot(bool leadingSlash) {
  const auto *metadata = keyValueConstructorSurfaceMetadata();
  if (metadata != nullptr) {
    for (std::string_view alias : metadata->importAliasSpellings) {
      if (!alias.empty() && alias.front() != '/' &&
          alias.find('/') == std::string_view::npos) {
        return leadingSlash ? "/" + std::string(alias) : std::string(alias);
      }
    }
  }
  return leadingSlash ? "/" + std::string("map") : std::string("map");
}

bool isBuiltinCollectionTypeName(std::string_view typeName,
                                 std::string_view collectionName) {
  const std::string normalized(typeName);
  const std::string raw(collectionName);
  const std::string rooted = "/" + raw;
  const std::string canonical = collectionTypePath(collectionName, false);
  const std::string rootedCanonical = collectionTypePath(collectionName);
  return normalized == raw || normalized == rooted ||
         normalized == canonical || normalized == rootedCanonical ||
         normalized.rfind(raw + "<", 0) == 0 ||
         normalized.rfind(canonical + "<", 0) == 0 ||
         normalized.rfind(rootedCanonical + "<", 0) == 0;
}

bool isExperimentalCollectionTypeName(std::string_view typeName,
                                      std::string_view collectionName,
                                      std::string_view experimentalTypeName) {
  const std::string normalized(typeName);
  const std::string raw(experimentalTypeName);
  const std::string bareRoot = experimentalCollectionTypePath(
      collectionName, experimentalTypeName, false);
  const std::string rooted = experimentalCollectionTypePath(
      collectionName, experimentalTypeName);
  return normalized == raw || normalized.rfind(raw + "<", 0) == 0 ||
         normalized == bareRoot || normalized == rooted ||
         normalized.rfind(bareRoot + "<", 0) == 0 ||
         normalized.rfind(rooted + "<", 0) == 0 ||
         normalized.rfind(bareRoot + "__", 0) == 0 ||
         normalized.rfind(rooted + "__", 0) == 0;
}

std::string keyValueStorageStructRootPath(bool leadingSlash) {
  const auto *metadata = keyValueHelperSurfaceMetadata();
  std::string path =
      metadata == nullptr ? std::string{} : stdlibSurfaceBackingTypePath(*metadata);
  if (!leadingSlash && !path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return path;
}

bool isKeyValueStorageStructPath(std::string_view path) {
  const std::string storageRoot = keyValueStorageStructRootPath();
  const std::string experimentalStorageRoot =
      experimentalCollectionTypePath("map", "Map");
  return !storageRoot.empty() &&
         (path == storageRoot || path.rfind(storageRoot + "__", 0) == 0 ||
          path == experimentalStorageRoot ||
          path.rfind(experimentalStorageRoot + "__", 0) == 0);
}

std::string normalizeBuiltinCollectionStructPath(std::string_view collectionName) {
  return "/" + std::string(collectionName);
}

std::string normalizeExperimentalCollectionTypePath(std::string_view typeName,
                                                    std::string_view collectionName,
                                                    std::string_view experimentalTypeName) {
  const std::string normalized(typeName);
  if (normalized == experimentalTypeName) {
    return experimentalCollectionTypePath(collectionName, experimentalTypeName);
  }
  const std::string bareRoot =
      experimentalCollectionTypePath(collectionName, experimentalTypeName, false);
  if (normalized == bareRoot || normalized.rfind(bareRoot + "__", 0) == 0) {
    return "/" + normalized;
  }
  return normalized;
}

std::string normalizeCollectionHelperPath(const std::string &path) {
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 ||
        normalizedPath.rfind(std::string("vector") + "/", 0) == 0 ||
        normalizedPath.rfind(collectionMemberRoot("vector", false), 0) == 0 ||
        normalizedPath.rfind(experimentalCollectionMemberRoot("vector", false), 0) == 0 ||
        normalizedPath.rfind(keyValueCollectionAliasRoot(false) + "/", 0) == 0 ||
        normalizedPath.rfind(collectionMemberRoot("map", false), 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }
  const size_t leafStart =
      normalizedPath.find_last_of('/') == std::string::npos
          ? 0
          : normalizedPath.find_last_of('/') + 1;
  const size_t generatedSuffix = normalizedPath.find("__", leafStart);
  if (generatedSuffix != std::string::npos) {
    normalizedPath.erase(generatedSuffix);
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
  const std::string stdVectorPrefix = collectionMemberRoot("vector", false);
  const std::string experimentalVectorPrefix =
      experimentalCollectionMemberRoot("vector", false);
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(arrayPrefix.size())));
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    return isRemovedVectorCompatibilityHelper(stripGeneratedHelperSuffix(normalized.substr(stdVectorPrefix.size())));
  }
  if (normalized.rfind(experimentalVectorPrefix, 0) == 0) {
    std::string helperName;
    return resolveVectorSurfaceMemberToken(
        normalized.substr(experimentalVectorPrefix.size()), helperName);
  }
  return false;
}

bool isExplicitKeyValueMethodAliasPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string mapPrefix =
      keyValueCollectionAliasRoot(false) + "/";
  const std::string stdMapPrefix = collectionMemberRoot("map", false);
  if (normalized.rfind(mapPrefix, 0) == 0) {
    std::string helperName;
    return resolveKeyValueSurfaceMemberToken(
               normalized.substr(mapPrefix.size()), helperName) &&
           (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe" || helperName == "insert");
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    std::string helperName;
    return resolveKeyValueSurfaceMemberToken(
               normalized.substr(stdMapPrefix.size()), helperName) &&
           (helperName == "count" || helperName == "at" ||
            helperName == "at_unsafe" || helperName == "insert");
  }
  return false;
}

bool isExplicitKeyValueContainsOrTryAtMethodPath(const std::string &methodName) {
  if (methodName.empty()) {
    return false;
  }
  std::string normalized = methodName;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const std::string mapPrefix =
      keyValueCollectionAliasRoot(false) + "/";
  const std::string stdMapPrefix = collectionMemberRoot("map", false);
  if (normalized.rfind(mapPrefix, 0) == 0) {
    std::string helperName;
    return resolveKeyValueSurfaceMemberToken(
               normalized.substr(mapPrefix.size()), helperName) &&
           (helperName == "contains" || helperName == "tryAt");
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    std::string helperName;
    return resolveKeyValueSurfaceMemberToken(
               normalized.substr(stdMapPrefix.size()), helperName) &&
           (helperName == "contains" || helperName == "tryAt");
  }
  return false;
}

bool isUnqualifiedCollectionBuiltinName(const Expr &expr, const char *name) {
  if (expr.kind != Expr::Kind::Call || name == nullptr || expr.name != name) {
    return false;
  }
  if (!expr.namespacePrefix.empty()) {
    return false;
  }
  return expr.name.find('/') == std::string::npos;
}

bool isExplicitKeyValueHelperFallbackPath(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  if (isBorrowedKeyValueHelperSurface(expr)) {
    return false;
  }
  std::string helperName;
  return resolveKeyValueHelperAliasName(expr, helperName) &&
         (helperName == "count" || helperName == "contains" ||
          helperName == "tryAt" || helperName == "at" ||
          helperName == "at_unsafe" || helperName == "insert");
}

bool isExplicitKeyValueReceiverProbeHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolveKeyValueHelperAliasName(expr, helperName) &&
         (helperName == "count" || helperName == "contains" ||
          helperName == "at" || helperName == "at_unsafe" ||
          helperName == "tryAt" || helperName == "insert");
}

bool isExplicitVectorAccessHelperPath(const std::string &path) {
  const std::string normalizedPath = normalizeCollectionHelperPath(path);
  const std::string atPath = normalizeCollectionHelperPath(
      stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::CollectionsManifestSurface0, "at"));
  const std::string atUnsafePath = normalizeCollectionHelperPath(
      stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::CollectionsManifestSurface0, "at_unsafe"));
  return (!atPath.empty() && normalizedPath == atPath) ||
         (!atUnsafePath.empty() && normalizedPath == atUnsafePath);
}

bool isExplicitVectorAccessHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolveVectorSurfaceExprMemberName(expr, helperName) &&
         (helperName == "at" || helperName == "at_unsafe");
}

bool isExplicitVectorReceiverProbeHelperExpr(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string helperName;
  return resolveVectorSurfaceExprMemberName(expr, helperName) &&
         (helperName == "at" || helperName == "at_unsafe" ||
          helperName == "count" || helperName == "capacity");
}

bool blocksExplicitVectorReceiverProbeKindFallbackExpr(const Expr &expr) {
  if (!isExplicitVectorReceiverProbeHelperExpr(expr)) {
    return false;
  }
  if (expr.isMethodCall) {
    return true;
  }
  if (!expr.name.empty() && expr.name.front() == '/') {
    return isExplicitRemovedVectorMethodAliasPath(expr.name);
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scopedPath = expr.namespacePrefix;
    if (!scopedPath.empty() && scopedPath.front() != '/') {
      scopedPath.insert(scopedPath.begin(), '/');
    }
    scopedPath += "/" + expr.name;
    return isExplicitRemovedVectorMethodAliasPath(scopedPath);
  }
  return isExplicitRemovedVectorMethodAliasPath(expr.name);
}

bool isAllowedResolvedMapDirectCallPath(const std::string &callPath, const std::string &resolvedPath) {
  if (!isExplicitKeyValueMethodAliasPath(callPath)) {
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
  if (normalizedCallPath.rfind(collectionMemberRoot("vector"), 0) != 0) {
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

  if (isKeyValueHelperSurfaceId(surfaceId) &&
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

std::string inferPublishedKeyValueStorageStructPathFromConstructorPath(std::string_view path) {
  std::string memberName;
  const std::optional<StdlibSurfaceId> surfaceId = keyValueConstructorSurfaceId();
  if (!surfaceId.has_value()) {
    return "";
  }
  if (!resolvePublishedStdlibSurfaceConstructorMemberName(
          path,
          *surfaceId,
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
  const std::string storageRoot = keyValueStorageStructRootPath();
  if (storageRoot.empty()) {
    return "";
  }
  return storageRoot + "__" +
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
      const std::string vectorCandidate =
          stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::CollectionsManifestSurface0, suffix);
      if (!vectorCandidate.empty()) {
        appendUnique(vectorCandidate);
      }
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
  if (resolveKeyValueHelperAliasName(expr, helperOut)) {
    collectionOut = "map";
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
