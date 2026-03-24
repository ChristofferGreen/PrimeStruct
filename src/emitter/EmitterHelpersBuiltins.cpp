#include "EmitterBuiltinCallPathHelpersInternal.h"
#include "EmitterBuiltinCollectionInferenceInternal.h"
#include "EmitterHelpers.h"

#include <functional>
#include <string_view>

namespace primec::emitter {

using BindingInfo = Emitter::BindingInfo;
using ReturnKind = Emitter::ReturnKind;

bool resolveMethodCallPath(const Expr &call,
                           const std::unordered_map<std::string, const Definition *> &defMap,
                           const std::unordered_map<std::string, BindingInfo> &localTypes,
                           const std::unordered_map<std::string, std::string> &importAliases,
                           const std::unordered_map<std::string, std::string> &structTypeMap,
                           const std::unordered_map<std::string, ReturnKind> &returnKinds,
                           const std::unordered_map<std::string, std::string> &returnStructs,
                           std::string &resolvedOut) {
  resolvedOut.clear();
  if (!call.isMethodCall || call.args.empty()) {
    return false;
  }
  auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
    return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
           helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
           helperName == "remove_at" || helperName == "remove_swap";
  };
  auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
    return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
  };
  auto isRemovedCollectionMethodAlias = [&](const std::string &rawMethodName) {
    std::string candidate = rawMethodName;
    if (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    std::string_view helperName;
    if (candidate.rfind("array/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("array/").size());
      return isRemovedVectorCompatibilityHelper(helperName);
    }
    if (candidate.rfind("vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("vector/").size());
      return isRemovedVectorCompatibilityHelper(helperName);
    }
    if (candidate.rfind("std/collections/vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/vector/").size());
      return isRemovedVectorCompatibilityHelper(helperName);
    }
    if (candidate.rfind("map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("map/").size());
      return isRemovedMapCompatibilityHelper(helperName);
    }
    if (candidate.rfind("std/collections/map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/map/").size());
      return isRemovedMapCompatibilityHelper(helperName);
    }
    return false;
  };
  if (isRemovedCollectionMethodAlias(call.name)) {
    resolvedOut.clear();
    return false;
  }
  std::string normalizedMethodName = call.name;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  const bool isExplicitVectorAliasMethod = normalizedMethodName.rfind("vector/", 0) == 0;
  const bool isExplicitStdlibVectorMethod =
      normalizedMethodName.rfind("std/collections/vector/", 0) == 0;
  if (normalizedMethodName.rfind("vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
  } else if (normalizedMethodName.rfind("map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
  } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
  }
  const Expr &receiver = call.args.front();
  auto normalizeCollectionReceiverType = [](const std::string &typePath) -> std::string {
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
  };
  auto collectionHelperPathCandidates = [](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

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
      if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
          suffix != "at" && suffix != "at_unsafe") {
        const std::string aliasPath = "/map/" + suffix;
        appendUnique(aliasPath);
      }
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [](const std::string &path,
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
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto pruneMapTryAtStructReturnCompatibilityCandidates = [](const std::string &path,
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
  };
  auto normalizeMapImportAliasPath = [](const std::string &path) {
    if (path.empty() || path.front() == '/') {
      return path;
    }
    if (path.rfind("map/", 0) == 0 || path.rfind("std/collections/map/", 0) == 0) {
      return "/" + path;
    }
    return path;
  };
  auto metadataPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };
    appendUnique(path);
    appendUnique(normalizeMapImportAliasPath(path));
    if (!path.empty() && path.front() == '/' &&
        (path.rfind("/map/", 0) == 0 || path.rfind("/std/collections/map/", 0) == 0)) {
      appendUnique(path.substr(1));
    }
    return candidates;
  };
  auto findStructTypeMetadata = [&](const std::string &path) -> const std::string * {
    for (const auto &candidate : metadataPathCandidates(path)) {
      auto it = structTypeMap.find(candidate);
      if (it != structTypeMap.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };
  auto findReturnStructMetadata = [&](const std::string &path) -> const std::string * {
    for (const auto &candidate : metadataPathCandidates(path)) {
      auto it = returnStructs.find(candidate);
      if (it != returnStructs.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };
  auto findReturnKindMetadata = [&](const std::string &path) -> const ReturnKind * {
    for (const auto &candidate : metadataPathCandidates(path)) {
      auto it = returnKinds.find(candidate);
      if (it != returnKinds.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };
  auto preferCanonicalMapMethodHelperPath = [&](const std::string &path) {
    if (path.rfind("/map/", 0) != 0) {
      return path;
    }
    const std::string suffix = path.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      return path;
    }
    const std::string canonicalPath = "/std/collections/map/" + suffix;
    if (defMap.count(canonicalPath) > 0 || findStructTypeMetadata(canonicalPath) != nullptr ||
        findReturnStructMetadata(canonicalPath) != nullptr || findReturnKindMetadata(canonicalPath) != nullptr) {
      return canonicalPath;
    }
    return path;
  };
  auto hasDefinitionOrMetadata = [&](const std::string &path) {
    return defMap.count(path) > 0 || findStructTypeMetadata(path) != nullptr ||
           findReturnStructMetadata(path) != nullptr || findReturnKindMetadata(path) != nullptr;
  };
  std::function<std::string(const Expr &)> inferPrimitiveTypeName;
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
      const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
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
      const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
      return inferredReceiverType == "vector";
    }
    return false;
  };
  auto resolveBareVectorAccessMethodHelperPath = [&](const Expr &candidate) -> std::string {
    if (!isBareVectorAccessMethod(candidate)) {
      return "";
    }
    const std::string aliasPath = "/vector/" + candidate.name;
    if (hasDefinitionOrMetadata(aliasPath)) {
      return aliasPath;
    }
    const std::string canonicalPath = "/std/collections/vector/" + candidate.name;
    if (hasDefinitionOrMetadata(canonicalPath)) {
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
      const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiverExpr));
      return inferredReceiverType == "map";
    }
    return false;
  };
  auto resolveBareMapAccessMethodHelperPath = [&](const Expr &candidate) -> std::string {
    if (!isBareMapAccessMethod(candidate)) {
      return "";
    }
    const std::string canonicalPath = "/std/collections/map/" + candidate.name;
    if (hasDefinitionOrMetadata(canonicalPath)) {
      return canonicalPath;
    }
    const std::string aliasPath = "/map/" + candidate.name;
    if (hasDefinitionOrMetadata(aliasPath)) {
      return aliasPath;
    }
    return "";
  };
  auto resolveCollectionElementTypeFromCall = [&](const Expr &candidate, std::string &typeOut) -> bool {
    typeOut.clear();
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall) {
      return false;
    }

    auto extractCollectionElementTypeFromReturn = [&](const std::string &typeName) -> bool {
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
    };

    const std::string resolvedExprPath = resolveExprPath(candidate);
    std::vector<std::string> resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
    pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
    pruneMapTryAtStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
    auto importIt = importAliases.find(candidate.name);
    if (importIt != importAliases.end()) {
      for (const auto &aliasCandidate : collectionHelperPathCandidates(importIt->second)) {
        bool seen = false;
        for (const auto &existing : resolvedCandidates) {
          if (existing == aliasCandidate) {
            seen = true;
            break;
          }
        }
        if (!seen) {
          resolvedCandidates.push_back(aliasCandidate);
        }
      }
    }

    for (const auto &path : resolvedCandidates) {
      auto defIt = defMap.find(path);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        continue;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        return extractCollectionElementTypeFromReturn(transform.templateArgs.front());
      }
    }
    return false;
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
    if (defMap.find("/" + normalized) != defMap.end()) {
      return false;
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    return receiverIndex < candidate.args.size() && isMapValue(candidate.args[receiverIndex], localTypes);
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
    if (defMap.find("/" + normalized) != defMap.end()) {
      return false;
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return false;
    }
    return isArrayValue(candidate.args[receiverIndex], localTypes) || isVectorValue(candidate.args[receiverIndex], localTypes) ||
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
            candidate.args[receiverIndex], localTypes, resolveCollectionElementTypeFromCall, elementType)) {
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
    const Expr &receiver = candidate.args[receiverIndex];
    if (isStringValue(receiver, localTypes)) {
      return "i32";
    }
    if (inferPrimitiveTypeName) {
      const std::string inferredReceiverType = normalizeBindingTypeName(inferPrimitiveTypeName(receiver));
      if (inferredReceiverType == "string") {
        return "i32";
      }
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(receiver, localTypes, resolveCollectionElementTypeFromCall, elementType)) {
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
    for (const auto &resolvedCandidate : resolvedCandidates) {
      if (const std::string *structPath = findReturnStructMetadata(resolvedCandidate)) {
        return normalizeCollectionReceiverType(*structPath);
      }
    }
    for (const auto &resolvedCandidate : resolvedCandidates) {
      const ReturnKind *kind = findReturnKindMetadata(resolvedCandidate);
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
    const std::vector<std::string> resolvedCandidates = {resolvedExprPath};
    for (const auto &resolvedCandidate : resolvedCandidates) {
      if (const std::string *structPath = findReturnStructMetadata(resolvedCandidate)) {
        return normalizeCollectionReceiverType(*structPath);
      }
    }
    for (const auto &resolvedCandidate : resolvedCandidates) {
      const ReturnKind *kind = findReturnKindMetadata(resolvedCandidate);
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
    for (const auto &resolvedCandidate : resolvedCandidates) {
      if (const std::string *structPath = findReturnStructMetadata(resolvedCandidate)) {
        return normalizeCollectionReceiverType(*structPath);
      }
    }
    for (const auto &resolvedCandidate : resolvedCandidates) {
      const ReturnKind *kind = findReturnKindMetadata(resolvedCandidate);
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
    if (normalized != "std/collections/map/at" && normalized != "std/collections/map/at_unsafe") {
      return "";
    }
    const size_t receiverIndex = getAccessCallReceiverIndex(candidate, localTypes);
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    std::string elementType;
    if (inferCollectionElementTypeNameFromExpr(
            candidate.args[receiverIndex], localTypes, resolveCollectionElementTypeFromCall, elementType)) {
      return normalizeBindingTypeName(elementType);
    }
    return "";
  };
  inferPrimitiveTypeName = [&](const Expr &expr) -> std::string {
    switch (expr.kind) {
      case Expr::Kind::Literal:
        if (expr.isUnsigned) {
          return "u64";
        }
        return expr.intWidth == 64 ? "i64" : "i32";
      case Expr::Kind::BoolLiteral:
        return "bool";
      case Expr::Kind::FloatLiteral:
        return expr.floatWidth == 64 ? "f64" : "f32";
      case Expr::Kind::StringLiteral:
        return "";
      case Expr::Kind::Name: {
        auto it = localTypes.find(expr.name);
        if (it != localTypes.end() && isPrimitiveBindingTypeName(it->second.typeName)) {
          return normalizeBindingTypeName(it->second.typeName);
        }
        return "";
      }
      case Expr::Kind::Call: {
        if (expr.isMethodCall) {
          if (const std::string explicitMapAccessType = inferExplicitMapAccessResolvedTypeName(expr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (const std::string explicitVectorAccessType = inferExplicitVectorAccessResolvedTypeName(expr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (isExplicitVectorAccessSlashMethod(expr)) {
            return "";
          }
          if (const std::string vectorAccessMethodPath = resolveBareVectorAccessMethodHelperPath(expr);
              !vectorAccessMethodPath.empty()) {
            if (const std::string *structPath = findReturnStructMetadata(vectorAccessMethodPath)) {
              return normalizeCollectionReceiverType(*structPath);
            }
            if (const ReturnKind *kind = findReturnKindMetadata(vectorAccessMethodPath)) {
              if (*kind == ReturnKind::Array) {
                return "array";
              }
              const std::string inferredType = typeNameForReturnKind(*kind);
              if (!inferredType.empty()) {
                return inferredType;
              }
            }
            return "";
          } else if (isBareVectorAccessMethod(expr)) {
            return "";
          }
        }
        if (!expr.isMethodCall) {
          if (const std::string explicitVectorCountCapacityType =
                  inferExplicitVectorCountCapacityResolvedTypeName(expr);
              !explicitVectorCountCapacityType.empty()) {
            return explicitVectorCountCapacityType;
          }
          if (isExplicitVectorCountCapacityDirectCall(expr)) {
            return "";
          }
          if (const std::string explicitVectorAccessType = inferExplicitVectorAccessResolvedTypeName(expr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (const std::string explicitMapAccessType = inferExplicitMapAccessResolvedTypeName(expr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (isExplicitVectorAccessDirectCall(expr)) {
            return "";
          }
          if (const std::string explicitVectorAccessType = inferExplicitVectorAccessCompatibilityTypeName(expr);
              !explicitVectorAccessType.empty()) {
            return explicitVectorAccessType;
          }
          if (const std::string explicitMapAccessType = inferExplicitMapAccessCompatibilityTypeName(expr);
              !explicitMapAccessType.empty()) {
            return explicitMapAccessType;
          }
          if (const std::string canonicalMapAccessType = inferCanonicalMapAccessTypeName(expr);
              !canonicalMapAccessType.empty()) {
            return canonicalMapAccessType;
          }
          const std::string resolvedExprPath = resolveExprPath(expr);
          std::vector<std::string> resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
          pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
          pruneMapTryAtStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
          auto importIt = importAliases.find(expr.name);
          if (importIt != importAliases.end()) {
            for (const auto &candidate : collectionHelperPathCandidates(importIt->second)) {
              bool seen = false;
              for (const auto &existing : resolvedCandidates) {
                if (existing == candidate) {
                  seen = true;
                  break;
                }
              }
              if (!seen) {
                resolvedCandidates.push_back(candidate);
              }
            }
          }
          for (const auto &candidate : resolvedCandidates) {
            if (const std::string *structPath = findReturnStructMetadata(candidate)) {
              return normalizeCollectionReceiverType(*structPath);
            }
          }
          for (const auto &candidate : resolvedCandidates) {
            const ReturnKind *kind = findReturnKindMetadata(candidate);
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
          }
        }
        std::string collectionName;
        if (getBuiltinCollectionName(expr, collectionName)) {
          if ((collectionName == "array" || collectionName == "vector") && expr.templateArgs.size() == 1) {
            return collectionName;
          }
          if (collectionName == "map" && expr.templateArgs.size() == 2) {
            return collectionName;
          }
        }
        if (isArrayCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isMapCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isStringCountCall(expr, localTypes)) {
          return "i32";
        }
        if (isVectorCapacityCall(expr, localTypes)) {
          return "i32";
        }
        if (const std::string bareMapAccessMethodPath = resolveBareMapAccessMethodHelperPath(expr);
            !bareMapAccessMethodPath.empty()) {
          if (const std::string *structPath = findReturnStructMetadata(bareMapAccessMethodPath)) {
            return normalizeCollectionReceiverType(*structPath);
          }
          if (const ReturnKind *kind = findReturnKindMetadata(bareMapAccessMethodPath)) {
            if (*kind == ReturnKind::Array) {
              return "array";
            }
            const std::string inferredType = typeNameForReturnKind(*kind);
            if (!inferredType.empty()) {
              return inferredType;
            }
          }
        }
        const std::string accessTypeName =
            inferAccessCallTypeName(expr, localTypes, inferPrimitiveTypeName, resolveCollectionElementTypeFromCall);
        if (!accessTypeName.empty()) {
          return accessTypeName;
        }
        if (!expr.isMethodCall) {
          return "";
        }
        std::string methodPath;
        if (resolveMethodCallPath(
                expr, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
          if (const std::string *structPath = findReturnStructMetadata(methodPath)) {
            return normalizeCollectionReceiverType(*structPath);
          }
          if (const ReturnKind *kind = findReturnKindMetadata(methodPath)) {
            if (*kind == ReturnKind::Array) {
              return "array";
            }
            return typeNameForReturnKind(*kind);
          }
        }
        return "";
      }
    }
    return "";
  };
  auto preferredFileErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap.find("/std/file/FileError/why") != defMap.end()) {
        return "/std/file/FileError/why";
      }
      if (defMap.find("/FileError/why") != defMap.end()) {
        return "/FileError/why";
      }
      return "/file_error/why";
    }
    if (helperName == "is_eof") {
      if (defMap.find("/std/file/FileError/is_eof") != defMap.end()) {
        return "/std/file/FileError/is_eof";
      }
      if (defMap.find("/FileError/is_eof") != defMap.end()) {
        return "/FileError/is_eof";
      }
      if (defMap.find("/std/file/fileErrorIsEof") != defMap.end()) {
        return "/std/file/fileErrorIsEof";
      }
      return "";
    }
    if (helperName == "eof") {
      if (defMap.find("/std/file/FileError/eof") != defMap.end()) {
        return "/std/file/FileError/eof";
      }
      if (defMap.find("/FileError/eof") != defMap.end()) {
        return "/FileError/eof";
      }
      if (defMap.find("/std/file/fileReadEof") != defMap.end()) {
        return "/std/file/fileReadEof";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap.find("/std/file/FileError/status") != defMap.end()) {
        return "/std/file/FileError/status";
      }
      if (defMap.find("/std/file/fileErrorStatus") != defMap.end()) {
        return "/std/file/fileErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap.find("/std/file/FileError/result") != defMap.end()) {
        return "/std/file/FileError/result";
      }
      if (defMap.find("/std/file/fileErrorResult") != defMap.end()) {
        return "/std/file/fileErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredImageErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap.find("/std/image/ImageError/why") != defMap.end()) {
        return "/std/image/ImageError/why";
      }
      if (defMap.find("/ImageError/why") != defMap.end()) {
        return "/ImageError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap.find("/std/image/ImageError/status") != defMap.end()) {
        return "/std/image/ImageError/status";
      }
      if (defMap.find("/ImageError/status") != defMap.end()) {
        return "/ImageError/status";
      }
      if (defMap.find("/std/image/imageErrorStatus") != defMap.end()) {
        return "/std/image/imageErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap.find("/std/image/ImageError/result") != defMap.end()) {
        return "/std/image/ImageError/result";
      }
      if (defMap.find("/ImageError/result") != defMap.end()) {
        return "/ImageError/result";
      }
      if (defMap.find("/std/image/imageErrorResult") != defMap.end()) {
        return "/std/image/imageErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredContainerErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap.find("/std/collections/ContainerError/why") != defMap.end()) {
        return "/std/collections/ContainerError/why";
      }
      if (defMap.find("/ContainerError/why") != defMap.end()) {
        return "/ContainerError/why";
      }
      return "";
    }
    if (helperName == "status") {
      if (defMap.find("/std/collections/ContainerError/status") != defMap.end()) {
        return "/std/collections/ContainerError/status";
      }
      if (defMap.find("/ContainerError/status") != defMap.end()) {
        return "/ContainerError/status";
      }
      if (defMap.find("/std/collections/containerErrorStatus") != defMap.end()) {
        return "/std/collections/containerErrorStatus";
      }
      return "";
    }
    if (helperName == "result") {
      if (defMap.find("/std/collections/ContainerError/result") != defMap.end()) {
        return "/std/collections/ContainerError/result";
      }
      if (defMap.find("/ContainerError/result") != defMap.end()) {
        return "/ContainerError/result";
      }
      if (defMap.find("/std/collections/containerErrorResult") != defMap.end()) {
        return "/std/collections/containerErrorResult";
      }
      return "";
    }
    return "";
  };
  auto preferredGfxErrorHelperTarget = [&](std::string_view helperName,
                                           const std::string &resolvedStructPath) -> std::string {
    auto helperForBasePath = [&](std::string_view basePath) -> std::string {
      const std::string helperPath = std::string(basePath) + "/" + std::string(helperName);
      if (defMap.find(helperPath) != defMap.end()) {
        return helperPath;
      }
      return "";
    };
    const std::string canonicalBase = "/std/gfx/GfxError";
    const std::string experimentalBase = "/std/gfx/experimental/GfxError";
    if (resolvedStructPath == canonicalBase) {
      return helperForBasePath(canonicalBase);
    }
    if (resolvedStructPath == experimentalBase) {
      return helperForBasePath(experimentalBase);
    }
    const bool hasCanonical = defMap.find(canonicalBase + "/" + std::string(helperName)) != defMap.end();
    const bool hasExperimental =
        defMap.find(experimentalBase + "/" + std::string(helperName)) != defMap.end();
    if (hasCanonical && !hasExperimental) {
      return helperForBasePath(canonicalBase);
    }
    if (!hasCanonical && hasExperimental) {
      return helperForBasePath(experimentalBase);
    }
    return "";
  };
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name) {
    auto it = localTypes.find(receiver.name);
    if (it != localTypes.end()) {
      typeName = it->second.typeName;
    } else {
      if (receiver.name == "FileError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
           normalizedMethodName == "eof" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
        return !resolvedOut.empty();
      }
      if (receiver.name == "ImageError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
        return !resolvedOut.empty();
      }
      if (receiver.name == "ContainerError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
        return !resolvedOut.empty();
      }
      if (receiver.name == "GfxError" &&
          (normalizedMethodName == "why" || normalizedMethodName == "status" ||
           normalizedMethodName == "result")) {
        std::string resolvedStructPath = resolveExprPath(receiver);
        if (findStructTypeMetadata(resolvedStructPath) == nullptr) {
          resolvedStructPath = resolveTypePath(receiver.name, receiver.namespacePrefix);
        }
        resolvedOut = preferredGfxErrorHelperTarget(normalizedMethodName, resolvedStructPath);
        if (!resolvedOut.empty()) {
          return true;
        }
      }
      std::string resolvedReceiverPath = resolveExprPath(receiver);
      if (findStructTypeMetadata(resolvedReceiverPath) != nullptr ||
          defMap.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap.end()) {
        resolvedOut = preferCanonicalMapMethodHelperPath(resolvedReceiverPath + "/" + normalizedMethodName);
        return true;
      }
      std::string resolvedType = resolveTypePath(receiver.name, receiver.namespacePrefix);
      auto importIt = importAliases.find(receiver.name);
      if (findStructTypeMetadata(resolvedType) == nullptr && importIt != importAliases.end()) {
        resolvedType = normalizeMapImportAliasPath(importIt->second);
      }
      if (findStructTypeMetadata(resolvedType) != nullptr) {
        resolvedOut = preferCanonicalMapMethodHelperPath(resolvedType + "/" + normalizedMethodName);
        return true;
      }
      return false;
    }
  } else if (receiver.kind == Expr::Kind::Call && !receiver.isMethodCall && !receiver.isBinding) {
    std::string resolved = resolveExprPath(receiver);
    auto normalizeResolvedPath = [](std::string path) {
      if (!path.empty() && path.front() != '/') {
        path.insert(path.begin(), '/');
      }
      return path;
    };
    auto hasStructPath = [&](const std::string &path) { return findStructTypeMetadata(path) != nullptr; };
    auto importIt = importAliases.find(receiver.name);
    if (!hasStructPath(resolved) && importIt != importAliases.end()) {
      resolved = importIt->second;
    }
    const std::string normalizedResolved = normalizeResolvedPath(resolved);
    if (!hasStructPath(resolved) && hasStructPath(normalizedResolved)) {
      resolved = normalizedResolved;
    }
    if (hasStructPath(resolved)) {
      resolved = normalizeResolvedPath(resolved);
      resolvedOut = resolved + "/" + normalizedMethodName;
      return true;
    }
    typeName = inferPrimitiveTypeName(receiver);
  } else {
    typeName = inferPrimitiveTypeName(receiver);
  }
  if (typeName.empty()) {
    return false;
  }
  if (typeName == "File") {
    resolvedOut = preferredFileMethodTargetLocal(normalizedMethodName, defMap);
    return true;
  }
  if (typeName == "FileError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
       normalizedMethodName == "status" || normalizedMethodName == "result")) {
    resolvedOut = preferredFileErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "ImageError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredImageErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "ContainerError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    resolvedOut = preferredContainerErrorHelperTarget(normalizedMethodName);
    return !resolvedOut.empty();
  }
  if (typeName == "GfxError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    std::string resolvedStructPath = resolveTypePath(typeName, call.namespacePrefix);
    auto importIt = importAliases.find(typeName);
    if (findStructTypeMetadata(resolvedStructPath) == nullptr && importIt != importAliases.end()) {
      resolvedStructPath = importIt->second;
    }
    resolvedOut = preferredGfxErrorHelperTarget(normalizedMethodName, resolvedStructPath);
    if (!resolvedOut.empty()) {
      return true;
    }
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    return false;
  }
  if (isPrimitiveBindingTypeName(typeName)) {
    resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
    return true;
  }
  typeName = normalizeMapImportAliasPath(typeName);
  std::string resolvedType = resolveTypePath(typeName, call.namespacePrefix);
  if (findReturnKindMetadata(resolvedType) == nullptr) {
    auto importIt = importAliases.find(typeName);
    if (importIt != importAliases.end()) {
      resolvedType = normalizeMapImportAliasPath(importIt->second);
    }
  }
  if (resolvedType == "/vector" || resolvedType == "vector") {
    const bool isCountLikeMethod = normalizedMethodName == "count";
    const bool isCapacityLikeMethod = normalizedMethodName == "capacity";
    if (isCountLikeMethod || isCapacityLikeMethod) {
      const std::string aliasPath = "/vector/" + normalizedMethodName;
      const std::string canonicalPath = "/std/collections/vector/" + normalizedMethodName;
      if (isExplicitVectorAliasMethod && !hasDefinitionOrMetadata(aliasPath)) {
        resolvedOut.clear();
        return false;
      }
      if (isExplicitStdlibVectorMethod && !hasDefinitionOrMetadata(canonicalPath)) {
        resolvedOut.clear();
        return false;
      }
      if (!hasDefinitionOrMetadata(aliasPath) && !hasDefinitionOrMetadata(canonicalPath)) {
        resolvedOut.clear();
        return false;
      }
    }
  }
  resolvedOut = preferCanonicalMapMethodHelperPath(resolvedType + "/" + normalizedMethodName);
  return true;
}

bool isBuiltinAssign(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  return name == "assign";
}

bool getVectorMutatorName(const Expr &expr,
                          const std::unordered_map<std::string, std::string> &nameMap,
                          std::string &out) {
  auto stripGeneratedHelperSuffix = [](std::string helperName) {
    const size_t generatedSuffix = helperName.find("__");
    if (generatedSuffix != std::string::npos) {
      helperName.erase(generatedSuffix);
    }
    return helperName;
  };
  if (expr.kind != Expr::Kind::Call || expr.name.empty()) {
    return false;
  }
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  std::string name = expr.name;
  if (!name.empty() && name[0] == '/') {
    name.erase(0, 1);
  }
  if (name.rfind("vector/", 0) == 0) {
    name = stripGeneratedHelperSuffix(name.substr(std::string("vector/").size()));
  } else if (name.rfind("std/collections/vector/", 0) == 0) {
    name = stripGeneratedHelperSuffix(name.substr(std::string("std/collections/vector/").size()));
  } else if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  if (name == "push" || name == "pop" || name == "reserve" || name == "clear" || name == "remove_at" ||
      name == "remove_swap") {
    out = name;
    return true;
  }
  return false;
}

std::vector<const Expr *> orderCallArguments(const Expr &expr,
                                             const std::string &resolvedPath,
                                             const std::vector<Expr> &params,
                                             const std::unordered_map<std::string, BindingInfo> &localTypes) {
  std::vector<const Expr *> ordered;
  size_t callArgStart = 0;
  if (expr.isMethodCall && !expr.args.empty() && !resolvedPath.empty()) {
    const Expr &receiver = expr.args.front();
    if (receiver.kind == Expr::Kind::Name && localTypes.find(receiver.name) == localTypes.end()) {
      const size_t methodSlash = resolvedPath.find_last_of('/');
      if (methodSlash != std::string::npos && methodSlash > 0) {
        const std::string receiverPath = resolvedPath.substr(0, methodSlash);
        const size_t receiverSlash = receiverPath.find_last_of('/');
        const std::string receiverTypeName =
            receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
        if (receiverTypeName == receiver.name) {
          callArgStart = 1;
        }
      }
    }
  }
  ordered.reserve(expr.args.size() >= callArgStart ? expr.args.size() - callArgStart : 0);
  auto fallback = [&]() {
    ordered.clear();
    ordered.reserve(expr.args.size() >= callArgStart ? expr.args.size() - callArgStart : 0);
    for (size_t i = callArgStart; i < expr.args.size(); ++i) {
      ordered.push_back(&expr.args[i]);
    }
  };
  auto isCollectionBindingType = [](const std::string &typeName) {
    const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
    return normalizedTypeName == "array" || normalizedTypeName == "vector" ||
           isMapCollectionTypeNameLocal(normalizedTypeName) || normalizedTypeName == "string" ||
           normalizedTypeName == "soa_vector";
  };
  const size_t slicedArgCount = expr.args.size() >= callArgStart ? expr.args.size() - callArgStart : 0;
  if (!hasNamedArguments(expr.argNames) && slicedArgCount == 2 && params.size() == 2 &&
      params[0].name == "values" && isCollectionBindingType(getBindingInfo(params[0]).typeName)) {
    const Expr &first = expr.args[callArgStart];
    const bool leadingNonReceiver =
        first.kind == Expr::Kind::Literal || first.kind == Expr::Kind::BoolLiteral ||
        first.kind == Expr::Kind::FloatLiteral || first.kind == Expr::Kind::StringLiteral;
    if (leadingNonReceiver) {
      ordered.assign(2, nullptr);
      ordered[0] = &expr.args[callArgStart + 1];
      ordered[1] = &expr.args[callArgStart];
      return ordered;
    }
  }
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = callArgStart; i < expr.args.size(); ++i) {
    if (i < expr.argNames.size() && expr.argNames[i].has_value()) {
      const std::string &name = *expr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        fallback();
        return ordered;
      }
      ordered[index] = &expr.args[i];
      continue;
    }
    while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= ordered.size()) {
      fallback();
      return ordered;
    }
    ordered[positionalIndex] = &expr.args[i];
    ++positionalIndex;
  }
  for (size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
    }
  }
  for (const auto *arg : ordered) {
    if (arg == nullptr) {
      fallback();
      return ordered;
    }
  }
  return ordered;
}

bool isBuiltinIf(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  return isSimpleCallName(expr, "if");
}

bool isBuiltinBlock(const Expr &expr, const std::unordered_map<std::string, std::string> &nameMap) {
  std::string full = resolveExprPath(expr);
  if (nameMap.count(full) > 0) {
    return false;
  }
  return isSimpleCallName(expr, "block");
}

bool isLoopCall(const Expr &expr) {
  return isSimpleCallName(expr, "loop");
}

bool isWhileCall(const Expr &expr) {
  return isSimpleCallName(expr, "while");
}

bool isForCall(const Expr &expr) {
  return isSimpleCallName(expr, "for");
}

bool isRepeatCall(const Expr &expr) {
  return isSimpleCallName(expr, "repeat");
}

bool hasNamedArguments(const std::vector<std::optional<std::string>> &argNames) {
  for (const auto &name : argNames) {
    if (name.has_value()) {
      return true;
    }
  }
  return false;
}

bool isReturnCall(const Expr &expr) {
  return isSimpleCallName(expr, "return");
}

} // namespace primec::emitter
