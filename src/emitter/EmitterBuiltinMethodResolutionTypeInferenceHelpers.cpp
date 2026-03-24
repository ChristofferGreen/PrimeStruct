#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"

#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinCollectionInferenceInternal.h"

#include <functional>

namespace primec::emitter {

namespace {

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

} // namespace

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
      appendUniqueCandidate(candidates, "/vector/" + suffix);
      appendUniqueCandidate(candidates, "/std/collections/vector/" + suffix);
    }
  } else if (normalizedPath.rfind("/vector/", 0) == 0) {
    const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUniqueCandidate(candidates, "/std/collections/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUniqueCandidate(candidates, "/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
    const std::string suffix =
        normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      appendUniqueCandidate(candidates, "/vector/" + suffix);
    }
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      appendUniqueCandidate(candidates, "/array/" + suffix);
    }
  } else if (normalizedPath.rfind("/map/", 0) == 0) {
    appendUniqueCandidate(
        candidates,
        "/std/collections/map/" + normalizedPath.substr(std::string("/map/").size()));
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
    if (suffix == "at" || suffix == "at_unsafe") {
      eraseCandidate("/std/collections/map/" + suffix);
    }
  } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix =
        normalizedPath.substr(std::string("/std/collections/map/").size());
    if (suffix == "at" || suffix == "at_unsafe") {
      eraseCandidate("/map/" + suffix);
    }
  }
}

void pruneMapTryAtStructReturnCompatibilityCandidates(
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
  if (normalizedPath == "/map/tryAt") {
    eraseCandidate("/std/collections/map/tryAt");
  } else if (normalizedPath == "/std/collections/map/tryAt") {
    eraseCandidate("/map/tryAt");
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

std::string inferMethodResolutionPrimitiveTypeName(
    const Expr &expr,
    const MethodResolutionMetadataView &view,
    const std::unordered_map<std::string, BindingInfo> &localTypes) {
  std::function<std::string(const Expr &)> inferPrimitiveTypeName;
  auto resolveCollectionElementTypeFromCall = [&](const Expr &candidate, std::string &typeOut) -> bool {
    typeOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }

    const std::string resolvedExprPath = resolveExprPath(candidate);
    std::vector<std::string> resolvedCandidates =
        collectionHelperPathCandidates(resolvedExprPath);
    pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
    pruneMapTryAtStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
    auto importIt = view.importAliases.find(candidate.name);
    if (importIt != view.importAliases.end()) {
      for (const auto &aliasCandidate : collectionHelperPathCandidates(importIt->second)) {
        appendUniqueCandidate(resolvedCandidates, aliasCandidate);
      }
    }

    for (const auto &path : resolvedCandidates) {
      auto defIt = view.defMap.find(path);
      if (defIt == view.defMap.end() || defIt->second == nullptr) {
        continue;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return extractCollectionElementTypeFromReturnType(transform.templateArgs.front(), typeOut);
      }
    }
    return false;
  };
  auto isBareVectorAccessMethod = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
        candidate.args.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "at" && normalized != "at_unsafe") {
      return false;
    }
    if (normalized.find('/') != std::string::npos) {
      return false;
    }
    const Expr &receiverExpr = candidate.args.front();
    if (isVectorValue(receiverExpr, localTypes)) {
      return true;
    }
    if (inferPrimitiveTypeName) {
      const std::string inferredReceiverType =
          normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
      return inferredReceiverType == "vector";
    }
    return false;
  };
  auto isExplicitVectorAccessSlashMethod = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
        candidate.args.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "vector/at" && normalized != "vector/at_unsafe" &&
        normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
      return false;
    }
    const Expr &receiverExpr = candidate.args.front();
    if (isVectorValue(receiverExpr, localTypes)) {
      return true;
    }
    if (inferPrimitiveTypeName) {
      const std::string inferredReceiverType =
          normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
      return inferredReceiverType == "vector";
    }
    return false;
  };
  auto resolveBareVectorAccessMethodHelperPath = [&](const Expr &candidate) -> std::string {
    if (!isBareVectorAccessMethod(candidate)) {
      return "";
    }
    const std::string aliasPath = "/vector/" + candidate.name;
    if (hasDefinitionOrMetadata(view, aliasPath)) {
      return aliasPath;
    }
    const std::string canonicalPath = "/std/collections/vector/" + candidate.name;
    if (hasDefinitionOrMetadata(view, canonicalPath)) {
      return canonicalPath;
    }
    return "";
  };
  auto isBareMapAccessMethod = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
        candidate.args.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "at" && normalized != "at_unsafe") {
      return false;
    }
    if (normalized.find('/') != std::string::npos) {
      return false;
    }
    const Expr &receiverExpr = candidate.args.front();
    if (isMapValue(receiverExpr, localTypes)) {
      return true;
    }
    if (inferPrimitiveTypeName) {
      const std::string inferredReceiverType =
          normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
      return inferredReceiverType == "map";
    }
    return false;
  };
  auto resolveBareMapAccessMethodHelperPath = [&](const Expr &candidate) -> std::string {
    if (!isBareMapAccessMethod(candidate)) {
      return "";
    }
    const std::string canonicalPath = "/std/collections/map/" + candidate.name;
    if (hasDefinitionOrMetadata(view, canonicalPath)) {
      return canonicalPath;
    }
    const std::string aliasPath = "/map/" + candidate.name;
    if (hasDefinitionOrMetadata(view, aliasPath)) {
      return aliasPath;
    }
    return "";
  };
  auto isExplicitMapAccessCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "map/at" && normalized != "map/at_unsafe") {
      return false;
    }
    if (view.defMap.find("/" + normalized) != view.defMap.end()) {
      return false;
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    return receiverIndex < candidate.args.size() &&
           isMapValue(candidate.args[receiverIndex], localTypes);
  };
  auto isExplicitVectorAccessCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "vector/at" && normalized != "vector/at_unsafe") {
      return false;
    }
    if (view.defMap.find("/" + normalized) != view.defMap.end()) {
      return false;
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return false;
    }
    return isArrayValue(candidate.args[receiverIndex], localTypes) ||
           isVectorValue(candidate.args[receiverIndex], localTypes) ||
           isStringValue(candidate.args[receiverIndex], localTypes);
  };
  auto isExplicitVectorCountCapacityDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "vector/count" || normalized == "vector/capacity" ||
           normalized == "std/collections/vector/count" ||
           normalized == "std/collections/vector/capacity";
  };
  auto inferExplicitMapAccessCompatibilityTypeName = [&](const Expr &candidate) -> std::string {
    if (!isExplicitMapAccessCompatibilityCall(candidate)) {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(
            candidate.args[receiverIndex],
            localTypes,
            resolveCollectionElementTypeFromCall,
            elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };
  auto inferExplicitVectorAccessCompatibilityTypeName = [&](const Expr &candidate) -> std::string {
    if (!isExplicitVectorAccessCompatibilityCall(candidate)) {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    const Expr &receiverExpr = candidate.args[receiverIndex];
    if (isStringValue(receiverExpr, localTypes)) {
      return "i32";
    }
    if (inferPrimitiveTypeName) {
      const std::string inferredReceiverType =
          normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
      if (inferredReceiverType == "string") {
        return "i32";
      }
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(
            receiverExpr,
            localTypes,
            resolveCollectionElementTypeFromCall,
            elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };
  auto inferExplicitVectorAccessResolvedTypeName = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "vector/at" && normalized != "vector/at_unsafe" &&
        normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
      return "";
    }

    const std::string resolvedExprPath = resolveExprPath(candidate);
    std::vector<std::string> resolvedCandidates;
    if (candidate.isMethodCall &&
        (resolvedExprPath == "/vector/at" || resolvedExprPath == "/vector/at_unsafe" ||
         resolvedExprPath == "/std/collections/vector/at" ||
         resolvedExprPath == "/std/collections/vector/at_unsafe")) {
      resolvedCandidates.push_back(resolvedExprPath);
    } else {
      resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
    }
    return typeNameFromResolvedCandidates(view, resolvedCandidates);
  };
  auto inferExplicitMapAccessResolvedTypeName = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
      return "";
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    if (resolvedExprPath != "/map/at" && resolvedExprPath != "/map/at_unsafe" &&
        resolvedExprPath != "/std/collections/map/at" &&
        resolvedExprPath != "/std/collections/map/at_unsafe") {
      return "";
    }
    return typeNameFromResolvedCandidates(view, {resolvedExprPath});
  };
  auto inferExplicitVectorCountCapacityResolvedTypeName = [&](const Expr &candidate) -> std::string {
    if (!isExplicitVectorCountCapacityDirectCall(candidate)) {
      return "";
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    std::vector<std::string> resolvedCandidates;
    if (resolvedExprPath == "/vector/count" || resolvedExprPath == "/vector/capacity") {
      resolvedCandidates.push_back(resolvedExprPath);
    } else {
      resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
    }
    return typeNameFromResolvedCandidates(view, resolvedCandidates);
  };
  auto isExplicitVectorAccessDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    return resolvedExprPath == "/vector/at" || resolvedExprPath == "/vector/at_unsafe" ||
           resolvedExprPath == "/std/collections/vector/at" ||
           resolvedExprPath == "/std/collections/vector/at_unsafe";
  };
  auto inferCanonicalMapAccessTypeName = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/map/at" &&
        normalized != "std/collections/map/at_unsafe") {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(
            candidate.args[receiverIndex],
            localTypes,
            resolveCollectionElementTypeFromCall,
            elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };

  inferPrimitiveTypeName = [&](const Expr &candidateExpr) -> std::string {
    switch (candidateExpr.kind) {
      case Expr::Kind::Literal:
        if (candidateExpr.isUnsigned) {
          return "u64";
        }
        return candidateExpr.intWidth == 64 ? "i64" : "i32";
      case Expr::Kind::BoolLiteral:
        return "bool";
      case Expr::Kind::FloatLiteral:
        return candidateExpr.floatWidth == 64 ? "f64" : "f32";
      case Expr::Kind::StringLiteral:
        return "";
      case Expr::Kind::Name: {
        auto it = localTypes.find(candidateExpr.name);
        if (it != localTypes.end() && isPrimitiveBindingTypeName(it->second.typeName)) {
          return normalizeBindingTypeName(it->second.typeName);
        }
        return "";
      }
      case Expr::Kind::Call: {
        if (candidateExpr.isMethodCall) {
          if (const std::string explicitMapAccessType =
                  inferExplicitMapAccessResolvedTypeName(candidateExpr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (const std::string explicitVectorAccessType =
                  inferExplicitVectorAccessResolvedTypeName(candidateExpr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (isExplicitVectorAccessSlashMethod(candidateExpr)) {
            return "";
          }
          if (const std::string vectorAccessMethodPath =
                  resolveBareVectorAccessMethodHelperPath(candidateExpr);
              !vectorAccessMethodPath.empty()) {
            return typeNameFromResolvedCandidates(view, {vectorAccessMethodPath});
          } else if (isBareVectorAccessMethod(candidateExpr)) {
            return "";
          }
        }
        if (!candidateExpr.isMethodCall) {
          if (const std::string explicitVectorCountCapacityType =
                  inferExplicitVectorCountCapacityResolvedTypeName(candidateExpr);
              !explicitVectorCountCapacityType.empty()) {
            return explicitVectorCountCapacityType;
          }
          if (isExplicitVectorCountCapacityDirectCall(candidateExpr)) {
            return "";
          }
          if (const std::string explicitVectorAccessType =
                  inferExplicitVectorAccessResolvedTypeName(candidateExpr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (const std::string explicitMapAccessType =
                  inferExplicitMapAccessResolvedTypeName(candidateExpr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (isExplicitVectorAccessDirectCall(candidateExpr)) {
            return "";
          }
          if (const std::string explicitVectorAccessType =
                  inferExplicitVectorAccessCompatibilityTypeName(candidateExpr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (const std::string explicitMapAccessType =
                  inferExplicitMapAccessCompatibilityTypeName(candidateExpr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (const std::string canonicalMapAccessType =
                  inferCanonicalMapAccessTypeName(candidateExpr);
              !canonicalMapAccessType.empty()) {
            return canonicalMapAccessType;
          }
          const std::string resolvedExprPath = resolveExprPath(candidateExpr);
          std::vector<std::string> resolvedCandidates =
              collectionHelperPathCandidates(resolvedExprPath);
          pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
          pruneMapTryAtStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
          auto importIt = view.importAliases.find(candidateExpr.name);
          if (importIt != view.importAliases.end()) {
            for (const auto &candidatePath :
                 collectionHelperPathCandidates(importIt->second)) {
              appendUniqueCandidate(resolvedCandidates, candidatePath);
            }
          }
          if (const std::string inferredType =
                  typeNameFromResolvedCandidates(view, resolvedCandidates);
              !inferredType.empty()) {
            return inferredType;
          }
        }
        std::string collectionName;
        if (getBuiltinCollectionName(candidateExpr, collectionName)) {
          if ((collectionName == "array" || collectionName == "vector") &&
              candidateExpr.templateArgs.size() == 1) {
            return collectionName;
          }
          if (collectionName == "map" && candidateExpr.templateArgs.size() == 2) {
            return collectionName;
          }
        }
        if (isArrayCountCall(candidateExpr, localTypes) ||
            isMapCountCall(candidateExpr, localTypes) ||
            isStringCountCall(candidateExpr, localTypes) ||
            isVectorCapacityCall(candidateExpr, localTypes)) {
          return "i32";
        }
        if (const std::string bareMapAccessMethodPath =
                resolveBareMapAccessMethodHelperPath(candidateExpr);
            !bareMapAccessMethodPath.empty()) {
          if (const std::string inferredType =
                  typeNameFromResolvedCandidates(view, {bareMapAccessMethodPath});
              !inferredType.empty()) {
            return inferredType;
          }
        }
        const std::string accessTypeName = inferAccessCallTypeName(
            candidateExpr,
            localTypes,
            inferPrimitiveTypeName,
            resolveCollectionElementTypeFromCall);
        if (!accessTypeName.empty()) {
          return accessTypeName;
        }
        if (!candidateExpr.isMethodCall) {
          return "";
        }
        std::string methodPath;
        if (resolveMethodCallPath(candidateExpr,
                                  view.defMap,
                                  localTypes,
                                  view.importAliases,
                                  view.structTypeMap,
                                  view.returnKinds,
                                  view.returnStructs,
                                  methodPath)) {
          return typeNameFromResolvedCandidates(view, {methodPath});
        }
        return "";
      }
    }
    return "";
  };

  return inferPrimitiveTypeName(expr);
}

} // namespace primec::emitter
