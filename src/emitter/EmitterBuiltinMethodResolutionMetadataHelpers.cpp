#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"

#include "EmitterBuiltinCallPathHelpersInternal.h"

namespace primec::emitter {

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
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      appendUniqueCandidate(candidates, "/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix =
        normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
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
    if (suffix == "contains" || suffix == "tryAt" ||
        suffix == "at" || suffix == "at_unsafe") {
      eraseCandidate("/map/" + suffix);
      eraseCandidate("/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix =
        normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix == "contains" || suffix == "tryAt" ||
        suffix == "at" || suffix == "at_unsafe") {
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
  if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
      suffix != "at" && suffix != "at_unsafe") {
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
