#include "EmitterBuiltinMethodResolutionTypeInferenceInternal.h"

#include "EmitterBuiltinCollectionInferenceInternal.h"

#include <functional>

namespace primec::emitter {
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
    if (normalized != "std/collections/vector/at" &&
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
    if (view.defMap.count(canonicalPath) > 0) {
      return canonicalPath;
    }
    const std::string aliasPath = "/map/" + candidate.name;
    if (view.defMap.count(aliasPath) > 0) {
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
    if (normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
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
    return normalized == "std/collections/vector/count" ||
           normalized == "std/collections/vector/capacity";
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
    if (normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
      return "";
    }

    const std::string resolvedExprPath = resolveExprPath(candidate);
    if (resolvedExprPath == "/std/collections/vector/at" ||
        resolvedExprPath == "/std/collections/vector/at_unsafe") {
      return typeNameFromResolvedCandidates(view, collectionHelperPathCandidates(resolvedExprPath));
    }
    return "";
  };
  auto inferExplicitMapAccessResolvedTypeName = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "map/at" && normalized != "map/at_unsafe" &&
        normalized != "std/collections/map/at" &&
        normalized != "std/collections/map/at_unsafe") {
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
    std::vector<std::string> resolvedCandidates =
        collectionHelperPathCandidates(resolvedExprPath);
    return typeNameFromResolvedCandidates(view, resolvedCandidates);
  };
  auto isExplicitVectorAccessDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    return resolvedExprPath == "/std/collections/vector/at" ||
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
  auto isRemovedMapDirectCallResultCompatibility = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    return resolvedExprPath == "/map/contains" || resolvedExprPath == "/map/tryAt";
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
          if (isExplicitMapAccessCompatibilityCall(candidateExpr)) {
            return "";
          }
          if (const std::string canonicalMapAccessType =
                  inferCanonicalMapAccessTypeName(candidateExpr);
              !canonicalMapAccessType.empty()) {
            return canonicalMapAccessType;
          }
          if (isRemovedMapDirectCallResultCompatibility(candidateExpr)) {
            return "";
          }
          const std::string resolvedExprPath = resolveExprPath(candidateExpr);
          std::vector<std::string> resolvedCandidates =
              collectionHelperPathCandidates(resolvedExprPath);
          pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
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
        } else if (isBareMapAccessMethod(candidateExpr)) {
          return "";
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
