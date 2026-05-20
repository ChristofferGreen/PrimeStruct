#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"

#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::emitter {

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

constexpr std::string_view VectorHelperSurfaceBridgeKey = "collections.vector_helpers";
constexpr std::string_view KeyValueHelperSurfaceBridgeKey = "collections.map_helpers";

std::string_view trimLeadingSlash(std::string_view text) {
  return !text.empty() && text.front() == '/' ? text.substr(1) : text;
}

bool surfaceMemberSuffix(std::string_view path,
                         const StdlibSurfaceMetadata &metadata,
                         bool includeImportAliases,
                         std::string_view &suffixOut) {
  const std::string_view normalizedPath = trimLeadingSlash(path);
  auto matchRoot = [&](std::string_view root) {
    root = trimLeadingSlash(root);
    if (normalizedPath.size() <= root.size() ||
        normalizedPath.compare(0, root.size(), root) != 0 ||
        normalizedPath[root.size()] != '/') {
      return false;
    }
    suffixOut = normalizedPath.substr(root.size() + 1);
    return !suffixOut.empty();
  };
  if (matchRoot(metadata.canonicalPath)) {
    return true;
  }
  if (includeImportAliases) {
    for (const std::string_view alias : metadata.importAliasSpellings) {
      if (matchRoot(alias)) {
        return true;
      }
    }
  }
  return false;
}

bool collectionSurfaceMemberPathUsesKnownPrefix(std::string_view path) {
  for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
    if (metadata.domain != StdlibSurfaceDomain::Collections ||
        metadata.shape != StdlibSurfaceShape::HelperFamily) {
      continue;
    }
    std::string_view suffix;
    if (surfaceMemberSuffix(path, metadata, true, suffix) &&
        !resolveStdlibSurfaceMemberName(metadata, suffix).empty()) {
      return true;
    }
  }
  return false;
}

std::string experimentalCollectionMemberRoot(std::string_view collectionName) {
  return "std/collections/experimental_" + std::string(collectionName) + "/";
}

const StdlibSurfaceMetadata *findVectorHelperSurfaceMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey(VectorHelperSurfaceBridgeKey);
}

const StdlibSurfaceMetadata *findKeyValueHelperSurfaceMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey(KeyValueHelperSurfaceBridgeKey);
}

bool isSurfaceMetadataForBridgeKey(StdlibSurfaceId surfaceId,
                                   std::string_view bridgeKey) {
  const auto *metadata = findStdlibSurfaceMetadata(surfaceId);
  const auto *expectedMetadata = findStdlibSurfaceMetadataByBridgeKey(bridgeKey);
  return metadata != nullptr && metadata == expectedMetadata;
}

bool isKeyValueHelperSurface(StdlibSurfaceId surfaceId) {
  return isSurfaceMetadataForBridgeKey(surfaceId, KeyValueHelperSurfaceBridgeKey);
}

bool resolveSurfaceMemberToken(const StdlibSurfaceMetadata &metadata,
                               std::string_view memberToken,
                               std::string &memberNameOut) {
  memberNameOut.clear();
  const std::string normalizedToken =
      stripGeneratedHelperSuffix(std::string(memberToken));
  const std::string_view memberName =
      resolveStdlibSurfaceMemberName(metadata, normalizedToken);
  if (memberName.empty()) {
    return false;
  }
  memberNameOut.assign(memberName);
  return true;
}

bool resolveCanonicalSurfacePathMemberName(const StdlibSurfaceMetadata &metadata,
                                           std::string_view path,
                                           std::string &memberNameOut) {
  memberNameOut.clear();
  std::string_view suffix;
  if (!surfaceMemberSuffix(path, metadata, false, suffix)) {
    return false;
  }
  return resolveSurfaceMemberToken(metadata, suffix, memberNameOut);
}

bool resolveHelperSurfacePathMemberName(const StdlibSurfaceMetadata &metadata,
                                        std::string_view path,
                                        bool includeImportAliases,
                                        std::string &memberNameOut) {
  memberNameOut.clear();
  std::string_view suffix;
  if (!surfaceMemberSuffix(path, metadata, includeImportAliases, suffix)) {
    return false;
  }
  return resolveSurfaceMemberToken(metadata, suffix, memberNameOut);
}

std::string normalizeCollectionHelperPath(std::string path) {
  if (!path.empty() && path.front() != '/' &&
      (path.rfind("array/", 0) == 0 ||
       collectionSurfaceMemberPathUsesKnownPrefix(path) ||
       path.rfind(experimentalCollectionMemberRoot("vector"), 0) == 0)) {
    path.insert(path.begin(), '/');
  }
  return path;
}

const StdlibSurfaceMetadata *findPublishedCollectionSurfaceMetadata(
    std::string_view path,
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

bool resolvePublishedCollectionSurfaceMemberName(std::string_view path,
                                                 StdlibSurfaceId surfaceId,
                                                 std::string &memberNameOut) {
  memberNameOut.clear();
  const auto *metadata = findPublishedCollectionSurfaceMetadata(path, surfaceId);
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
  return normalizeCollectionHelperPath(std::move(normalized));
}

bool isRemovedExactPublishedVectorHelper(std::string_view helperName) {
  std::string canonicalMemberName;
  const auto *metadata = findVectorHelperSurfaceMetadata();
  return metadata != nullptr &&
         resolveSurfaceMemberToken(*metadata, helperName, canonicalMemberName) &&
         canonicalMemberName == stripGeneratedHelperSuffix(std::string(helperName));
}

bool isRemovedExactPublishedKeyValueHelper(std::string_view helperName) {
  std::string canonicalMemberName;
  const auto *metadata = findKeyValueHelperSurfaceMetadata();
  return metadata != nullptr &&
         resolveSurfaceMemberToken(*metadata, helperName, canonicalMemberName) &&
         canonicalMemberName == stripGeneratedHelperSuffix(std::string(helperName)) &&
         isCanonicalKeyValueHelperName(canonicalMemberName);
}

} // namespace

bool resolvePublishedCollectionSurfaceMemberToken(std::string_view memberToken,
                                                  StdlibSurfaceId surfaceId,
                                                  std::string &memberNameOut) {
  memberNameOut.clear();
  const auto *metadata = findStdlibSurfaceMetadata(surfaceId);
  if (metadata == nullptr) {
    return false;
  }
  return resolveSurfaceMemberToken(*metadata, memberToken, memberNameOut);
}

const StdlibSurfaceMetadata *findPublishedCollectionHelperSurfaceMetadataByBridgeKey(
    std::string_view bridgeKey) {
  const auto *metadata = findStdlibSurfaceMetadataByBridgeKey(bridgeKey);
  if (metadata == nullptr ||
      metadata->domain != StdlibSurfaceDomain::Collections ||
      metadata->shape != StdlibSurfaceShape::HelperFamily) {
    return nullptr;
  }
  return metadata;
}

bool resolvePublishedCollectionSurfacePathMemberName(std::string_view path,
                                                     std::string_view bridgeKey,
                                                     bool includeImportAliases,
                                                     std::string &memberNameOut) {
  memberNameOut.clear();
  const auto *metadata =
      findPublishedCollectionHelperSurfaceMetadataByBridgeKey(bridgeKey);
  if (metadata == nullptr) {
    return false;
  }
  std::string_view suffix;
  if (!surfaceMemberSuffix(path, *metadata, includeImportAliases, suffix)) {
    return false;
  }
  return resolveSurfaceMemberToken(*metadata, suffix, memberNameOut);
}

std::string publishedCollectionSurfaceHelperPath(std::string_view bridgeKey,
                                                 std::string_view memberName) {
  const auto *metadata =
      findPublishedCollectionHelperSurfaceMetadataByBridgeKey(bridgeKey);
  if (metadata == nullptr) {
    return {};
  }
  std::string resolvedMemberName;
  if (!resolveSurfaceMemberToken(*metadata, memberName, resolvedMemberName)) {
    return {};
  }
  return std::string(metadata->canonicalPath) + "/" + resolvedMemberName;
}

bool resolvePublishedCollectionSurfaceExprMemberName(const Expr &expr,
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
  if (resolvePublishedCollectionSurfaceMemberName(normalizedPath, surfaceId, memberNameOut)) {
    return true;
  }
  if (isKeyValueHelperSurface(surfaceId) &&
      normalizedPath.rfind("/std/collections/Map", 0) == 0) {
    return resolvePublishedCollectionSurfaceMemberToken(
        normalizedPath.substr(std::string("/std/collections/Map").size()),
        surfaceId,
        memberNameOut);
  }
  return false;
}

bool isRemovedCollectionMethodAliasPath(std::string_view rawMethodName) {
  std::string candidate(rawMethodName);
  if (!candidate.empty() && candidate.front() == '/') {
    candidate.erase(candidate.begin());
  }
  if (candidate.rfind("array/", 0) == 0) {
    return isRemovedExactPublishedVectorHelper(
        std::string_view(candidate).substr(std::string_view("array/").size()));
  }
  if (const auto *metadata = findVectorHelperSurfaceMetadata();
      metadata != nullptr) {
    std::string canonicalMemberName;
    if (resolveCanonicalSurfacePathMemberName(
            *metadata,
            normalizeCollectionHelperPath(candidate),
            canonicalMemberName)) {
      return isRemovedExactPublishedVectorHelper(canonicalMemberName);
    }
  }
  std::string canonicalKeyValueMemberName;
  const auto *keyValueMetadata = findKeyValueHelperSurfaceMetadata();
  const std::string normalizedCandidate = normalizeCollectionHelperPath(candidate);
  if (keyValueMetadata != nullptr &&
      resolveHelperSurfacePathMemberName(
          *keyValueMetadata,
          normalizedCandidate,
          true,
          canonicalKeyValueMemberName)) {
    return isRemovedExactPublishedKeyValueHelper(canonicalKeyValueMemberName);
  }
  return false;
}

bool removedCollectionAliasNeedsDefinitionPath(std::string_view rawMethodName) {
  const std::string normalizedPath = normalizeCollectionHelperPath(std::string(rawMethodName));
  const std::string_view keyValueHelperName =
      keyValueHelperNameFromPath(normalizedPath);
  std::string vectorHelperName;
  const auto *vectorMetadata = findVectorHelperSurfaceMetadata();
  return (!keyValueHelperName.empty() &&
          isCanonicalKeyValueCountHelperName(keyValueHelperName)) ||
         normalizedPath == "/array/count" ||
         normalizedPath == "/array/capacity" ||
         (vectorMetadata != nullptr &&
          resolveCanonicalSurfacePathMemberName(
              *vectorMetadata,
              normalizedPath,
              vectorHelperName) &&
          (vectorHelperName == "count" || vectorHelperName == "capacity"));
}

void appendUniqueCandidate(std::vector<std::string> &candidates, const std::string &candidate) {
  if (candidate.empty()) {
    return;
  }
  for (const auto &existing : candidates) {
    if (existing == candidate) {
      return;
    }
  }
  candidates.push_back(candidate);
}

namespace {

std::vector<std::string> metadataPathCandidates(const std::string &path) {
  std::vector<std::string> candidates;
  appendUniqueCandidate(candidates, path);
  return candidates;
}

} // namespace

std::string typeNameFromResolvedCandidates(const MethodResolutionMetadataView &view,
                                           const std::vector<std::string> &resolvedCandidates) {
  for (const auto &resolvedCandidate : resolvedCandidates) {
    if (const std::string *structPath = findReturnStructMetadata(view, resolvedCandidate)) {
      return normalizeCollectionReceiverType(*structPath);
    }
  }
  for (const auto &resolvedCandidate : resolvedCandidates) {
    const ReturnKind *kind = findReturnKindMetadata(view, resolvedCandidate);
    if (kind == nullptr) {
      continue;
    }
    if (*kind == ReturnKind::Array) {
      return "array";
    }
    const std::string inferredType = typeNameForReturnKind(*kind);
    if (!inferredType.empty()) {
      return inferredType;
    }
    return "";
  }
  return "";
}

bool extractCollectionElementTypeFromReturnType(const std::string &typeName, std::string &typeOut) {
  std::string normalizedType = normalizeBindingTypeName(typeName);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return false;
    }
    if ((base == "array" || base == "vector") && args.size() == 1) {
      typeOut = normalizeBindingTypeName(args.front());
      return true;
    }
    if (isKeyValueCollectionTypeNameLocal(base) && args.size() == 2) {
      typeOut = normalizeBindingTypeName(args[1]);
      return true;
    }
    if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

std::string normalizeCollectionReceiverType(const std::string &typePath) {
  if (typePath == "/array" || typePath == "array") {
    return "array";
  }
  if (typePath == "/vector" || typePath == "vector") {
    return "vector";
  }
  if (typePath == "soa" "_vector" ||
      typePath == "/std/collections/" "soa" "_vector" ||
      typePath == "std/collections/" "soa" "_vector" ||
      typePath == "/std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
      typePath == "std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
      typePath == "Soa" "Vector") {
    return "soa" "_vector";
  }
  if (isKeyValueCollectionTypeNameLocal(typePath)) {
    return "map";
  }
  return typePath;
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path) {
  std::vector<std::string> candidates;
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 ||
        collectionSurfaceMemberPathUsesKnownPrefix(normalizedPath) ||
        normalizedPath.rfind("std/collections/" "soa" "_vector/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }

  appendUniqueCandidate(candidates, path);
  appendUniqueCandidate(candidates, normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      if (const auto *metadata = findVectorHelperSurfaceMetadata();
          metadata != nullptr) {
        appendUniqueCandidate(
            candidates,
            std::string(metadata->canonicalPath) + "/" + suffix);
      }
    }
  }
  return candidates;
}

const std::string *findStructTypeMetadata(const MethodResolutionMetadataView &view,
                                          const std::string &path) {
  for (const auto &candidate : metadataPathCandidates(path)) {
    auto it = view.structTypeMap.find(candidate);
    if (it != view.structTypeMap.end()) {
      return &it->second;
    }
  }
  return nullptr;
}

const std::string *findReturnStructMetadata(const MethodResolutionMetadataView &view,
                                            const std::string &path) {
  for (const auto &candidate : metadataPathCandidates(path)) {
    auto it = view.returnStructs.find(candidate);
    if (it != view.returnStructs.end()) {
      return &it->second;
    }
  }
  return nullptr;
}

const ReturnKind *findReturnKindMetadata(const MethodResolutionMetadataView &view,
                                         const std::string &path) {
  for (const auto &candidate : metadataPathCandidates(path)) {
    auto it = view.returnKinds.find(candidate);
    if (it != view.returnKinds.end()) {
      return &it->second;
    }
  }
  return nullptr;
}

bool hasDefinitionOrMetadata(const MethodResolutionMetadataView &view, const std::string &path) {
  return view.defMap.count(path) > 0 || findStructTypeMetadata(view, path) != nullptr ||
         findReturnStructMetadata(view, path) != nullptr ||
         findReturnKindMetadata(view, path) != nullptr;
}

} // namespace primec::emitter
