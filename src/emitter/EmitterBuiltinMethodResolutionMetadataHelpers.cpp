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

std::string normalizeCollectionHelperPath(std::string path) {
  if (!path.empty() && path.front() != '/' &&
      (path.rfind("array/", 0) == 0 || path.rfind("vector/", 0) == 0 ||
       path.rfind("std/collections/vector/", 0) == 0 ||
       path.rfind("std/collections/experimental_vector/", 0) == 0 ||
       path.rfind("map/", 0) == 0 ||
       path.rfind("std/collections/map/", 0) == 0 ||
       path.rfind("std/collections/experimental_map/", 0) == 0)) {
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
  return resolvePublishedCollectionSurfaceMemberToken(
             helperName,
             StdlibSurfaceId::CollectionsVectorHelpers,
             canonicalMemberName) &&
         canonicalMemberName == stripGeneratedHelperSuffix(std::string(helperName));
}

bool isRemovedExactPublishedMapHelper(std::string_view helperName) {
  std::string canonicalMemberName;
  return resolvePublishedCollectionSurfaceMemberToken(
             helperName,
             StdlibSurfaceId::CollectionsMapHelpers,
             canonicalMemberName) &&
         canonicalMemberName == stripGeneratedHelperSuffix(std::string(helperName)) &&
         isCanonicalMapHelperName(canonicalMemberName);
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
  if (surfaceId == StdlibSurfaceId::CollectionsMapHelpers &&
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
  if (candidate.rfind("std/collections/vector/", 0) == 0) {
    return isRemovedExactPublishedVectorHelper(
        std::string_view(candidate).substr(
            std::string_view("std/collections/vector/").size()));
  }
  if (candidate.rfind("map/", 0) == 0) {
    return isRemovedExactPublishedMapHelper(
        std::string_view(candidate).substr(std::string_view("map/").size()));
  }
  if (candidate.rfind("std/collections/map/", 0) == 0) {
    return isRemovedExactPublishedMapHelper(
        std::string_view(candidate).substr(
            std::string_view("std/collections/map/").size()));
  }
  return false;
}

bool removedCollectionAliasNeedsDefinitionPath(std::string_view rawMethodName) {
  const std::string normalizedPath = normalizeCollectionHelperPath(std::string(rawMethodName));
  const std::string_view mapHelperName = mapHelperNameFromPath(normalizedPath);
  return (!mapHelperName.empty() &&
          isCanonicalMapCountHelperName(mapHelperName)) ||
         normalizedPath == "/array/count" ||
         normalizedPath == "/array/capacity" ||
         normalizedPath == "/std/collections/vector/count" ||
         normalizedPath == "/std/collections/vector/capacity";
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
  appendUniqueCandidate(candidates, normalizeMapImportAliasPath(path));
  if (!path.empty() && path.front() == '/' &&
      (path.rfind("/map/", 0) == 0 || path.rfind("/std/collections/map/", 0) == 0)) {
    appendUniqueCandidate(candidates, path.substr(1));
  }
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
    if (isMapCollectionTypeNameLocal(base) && args.size() == 2) {
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
  if (typePath == "/soa_vector" || typePath == "soa_vector" ||
      typePath == "/std/collections/soa_vector" ||
      typePath == "std/collections/soa_vector" ||
      typePath == "/std/collections/experimental_soa_vector/SoaVector" ||
      typePath == "std/collections/experimental_soa_vector/SoaVector" ||
      typePath == "SoaVector") {
    return "soa_vector";
  }
  if (isMapCollectionTypeNameLocal(typePath) || typePath == "/map") {
    return "map";
  }
  return typePath;
}

std::vector<std::string> collectionHelperPathCandidates(const std::string &path) {
  std::vector<std::string> candidates;
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
        normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }

  appendUniqueCandidate(candidates, path);
  appendUniqueCandidate(candidates, normalizedPath);
  if (normalizedPath.rfind("/array/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUniqueCandidate(candidates, "/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/map/").size());
    if (!isCanonicalMapHelperName(suffix)) {
      appendUniqueCandidate(candidates, "/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix =
        normalizedPath.substr(std::string("/std/collections/map/").size());
    if (!isCanonicalMapHelperName(suffix)) {
      appendUniqueCandidate(candidates, "/map/" + suffix);
    }
  }
  return candidates;
}

void pruneMapAccessStructReturnCompatibilityCandidates(
    const std::string &path,
    std::vector<std::string> &candidates) {
  std::string normalizedPath = path;
  if (!normalizedPath.empty() && normalizedPath.front() != '/') {
    if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
  }
  auto eraseCandidate = [&](const std::string &candidate) {
    for (auto it = candidates.begin(); it != candidates.end();) {
      if (*it == candidate) {
        it = candidates.erase(it);
      } else {
        ++it;
      }
    }
  };
  if (normalizedPath.rfind("/map/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/map/").size());
    if (isCanonicalMapAccessHelperName(suffix)) {
      eraseCandidate("/map/" + suffix);
      eraseCandidate("/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix =
        normalizedPath.substr(std::string("/std/collections/map/").size());
    if (isCanonicalMapAccessHelperName(suffix)) {
      eraseCandidate("/map/" + suffix);
    }
  }
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

std::string preferCanonicalMapMethodHelperPath(const MethodResolutionMetadataView &view,
                                               const std::string &path) {
  if (path.rfind("/map/", 0) != 0) {
    return path;
  }
  const std::string suffix = path.substr(std::string("/map/").size());
  if (!isCanonicalMapHelperName(suffix)) {
    return path;
  }
  const std::string canonicalPath = "/std/collections/map/" + suffix;
  if (hasDefinitionOrMetadata(view, canonicalPath)) {
    return canonicalPath;
  }
  return path;
}

bool hasDefinitionOrMetadata(const MethodResolutionMetadataView &view, const std::string &path) {
  return view.defMap.count(path) > 0 || findStructTypeMetadata(view, path) != nullptr ||
         findReturnStructMetadata(view, path) != nullptr ||
         findReturnKindMetadata(view, path) != nullptr;
}

} // namespace primec::emitter
