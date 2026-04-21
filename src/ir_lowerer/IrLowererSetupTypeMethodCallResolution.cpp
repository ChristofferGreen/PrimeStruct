#include "IrLowererSetupTypeHelpers.h"

#include <functional>
#include <utility>

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string describeMethodCallExpr(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string displayName = expr.namespacePrefix;
    if (!displayName.empty() && displayName.front() != '/') {
      displayName.insert(displayName.begin(), '/');
    }
    displayName += "/" + expr.name;
    return displayName;
  }
  if (!expr.name.empty()) {
    return expr.name;
  }
  return "<unnamed>";
}

} // namespace

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  return resolveMethodCallDefinitionFromExpr(callExpr,
                                             localsIn,
                                             isArrayCountCall,
                                             isVectorCapacityCall,
                                             isEntryArgsName,
                                             importAliases,
                                             structNames,
                                             inferExprKind,
                                             resolveExprPath,
                                             nullptr,
                                             defMap,
                                             errorOut);
}

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const SemanticProgram *semanticProgram,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  return resolveMethodCallDefinitionFromExpr(callExpr,
                                             localsIn,
                                             isArrayCountCall,
                                             isVectorCapacityCall,
                                             isEntryArgsName,
                                             importAliases,
                                             structNames,
                                             inferExprKind,
                                             resolveExprPath,
                                             semanticProgram,
                                             {},
                                             defMap,
                                             errorOut);
}

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const GetReturnInfoForPathFn &getReturnInfo,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  return resolveMethodCallDefinitionFromExpr(callExpr,
                                             localsIn,
                                             isArrayCountCall,
                                             isVectorCapacityCall,
                                             isEntryArgsName,
                                             importAliases,
                                             structNames,
                                             inferExprKind,
                                             resolveExprPath,
                                             nullptr,
                                             getReturnInfo,
                                             defMap,
                                             errorOut);
}

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const SemanticProgram *semanticProgram,
    const GetReturnInfoForPathFn &getReturnInfo,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut) {
  static const std::unordered_map<std::string, std::string> noImportAliases;
  const auto &semanticAwareImportAliases =
      semanticProgram != nullptr ? noImportAliases : importAliases;

  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding) {
    return nullptr;
  }
  if (!callExpr.isMethodCall) {
    const std::string resolvedPath = resolveExprPath(callExpr);
    auto defIt = defMap.find(resolvedPath);
    if (defIt != defMap.end()) {
      return defIt->second;
    }
    return nullptr;
  }

  if (semanticProgram != nullptr) {
    auto resolveLoweredDefinitionPath = [&](const std::string &targetPath)
        -> const Definition * {
      auto tryResolvedPath = [&](const std::string &path) -> const Definition * {
        auto defIt = defMap.find(path);
        if (defIt != defMap.end() && defIt->second != nullptr) {
          return defIt->second;
        }
        const std::string overloadPrefix =
            path + "__ov" + std::to_string(callExpr.args.size());
        for (const auto &[candidatePath, candidateDef] : defMap) {
          if (candidateDef == nullptr) {
            continue;
          }
          if (candidatePath.rfind(overloadPrefix, 0) == 0 ||
              candidatePath.rfind(path + "__t", 0) == 0 ||
              candidatePath.rfind(path + "__ov", 0) == 0) {
            return candidateDef;
          }
        }
        return nullptr;
      };

      if (const Definition *resolvedDef = tryResolvedPath(targetPath);
          resolvedDef != nullptr) {
        return resolvedDef;
      }
      const std::string normalizedTargetPath =
          normalizeCollectionHelperPath(targetPath);
      if (normalizedTargetPath != targetPath) {
        return tryResolvedPath(normalizedTargetPath);
      }
      return nullptr;
    };
    const std::string explicitMethodPath = describeMethodCallExpr(callExpr);
    const bool requestsExplicitVectorCountMethod =
        explicitMethodPath == "/vector/count" ||
        explicitMethodPath == "/std/collections/vector/count";
    if (callExpr.semanticNodeId == 0) {
      errorOut = "missing semantic-product method-call semantic id: " +
                 describeMethodCallExpr(callExpr);
      return nullptr;
    }
    const std::string resolvedPath =
        findSemanticProductMethodCallTarget(semanticProgram, callExpr);
    if (resolvedPath.empty()) {
      if (!findSemanticProductDirectCallTarget(semanticProgram, callExpr).empty()) {
        errorOut.clear();
      } else {
        errorOut = "missing semantic-product method-call target: " +
                   describeMethodCallExpr(callExpr);
        return nullptr;
      }
    } else {
      if (const Definition *resolvedDef =
              resolveLoweredDefinitionPath(resolvedPath);
          resolvedDef != nullptr) {
        return resolvedDef;
      }
      const bool
          missesExplicitVectorCountMethodThroughMapMethodTarget =
              requestsExplicitVectorCountMethod &&
              (normalizeCollectionHelperPath(resolvedPath) == "/map/count" ||
               normalizeCollectionHelperPath(resolvedPath) ==
                   "/std/collections/map/count");
      if (missesExplicitVectorCountMethodThroughMapMethodTarget) {
        if (const std::string bridgePath =
                findSemanticProductBridgePathChoice(semanticProgram, callExpr);
            !bridgePath.empty()) {
          if (const Definition *bridgeDef =
                  resolveLoweredDefinitionPath(bridgePath);
              bridgeDef != nullptr) {
            return bridgeDef;
          }
        }
      }
      if (resolvedPath.rfind("/file/", 0) == 0) {
        errorOut.clear();
        return nullptr;
      }
      errorOut =
          "semantic-product method-call target missing lowered definition: " +
          resolvedPath;
      return nullptr;
    }
  }

  std::string accessName;
  const bool isBuiltinAccessCall = getBuiltinArrayAccessName(callExpr, accessName) && callExpr.args.size() == 2;
  const bool isBuiltinCountOrCapacityCall =
      isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count") ||
      isVectorBuiltinName(callExpr, "capacity");
  const bool isBuiltinBareVectorCapacityMethod =
      isSimpleCallName(callExpr, "capacity") &&
      isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn);
  const bool isBuiltinBareVectorAccessMethod =
      callExpr.isMethodCall && callExpr.args.size() == 2 &&
      (isSimpleCallName(callExpr, "at") || isSimpleCallName(callExpr, "at_unsafe")) &&
      resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn).isVectorTarget;
  const bool isBuiltinBareVectorMutatorMethod =
      callExpr.isMethodCall &&
      (isSimpleCallName(callExpr, "push") || isSimpleCallName(callExpr, "pop") ||
       isSimpleCallName(callExpr, "reserve") || isSimpleCallName(callExpr, "clear") ||
       isSimpleCallName(callExpr, "remove_at") || isSimpleCallName(callExpr, "remove_swap")) &&
      !callExpr.args.empty() && resolveArrayVectorAccessTargetInfo(callExpr.args.front(), localsIn).isVectorTarget;
  const bool isBuiltinVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  const bool isExplicitRemovedVectorMethodAlias = isExplicitRemovedVectorMethodAliasPath(callExpr.name);
  const bool isExplicitMapMethodAlias = isExplicitMapMethodAliasPath(callExpr.name);
  const bool isExplicitMapContainsOrTryAtMethod =
      isExplicitMapContainsOrTryAtMethodPath(callExpr.name);
  const bool isBuiltinMapContainsOrTryAtCall =
      isSimpleCallName(callExpr, "contains") || isSimpleCallName(callExpr, "tryAt") ||
      isSimpleCallName(callExpr, "insert");
  const bool allowBuiltinFallback =
      !isExplicitRemovedVectorMethodAlias && !isExplicitMapMethodAlias &&
      !isExplicitMapContainsOrTryAtMethod &&
      !isBuiltinBareVectorCapacityMethod && !isBuiltinBareVectorAccessMethod &&
      !isBuiltinBareVectorMutatorMethod &&
      (isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
       isBuiltinMapContainsOrTryAtCall ||
       (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
       (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)) || isBuiltinAccessCall);

  const std::string priorError = errorOut;
  const Expr *receiver = nullptr;
  if (!resolveMethodCallReceiverExpr(callExpr,
                                     localsIn,
                                     isArrayCountCall,
                                     isVectorCapacityCall,
                                     isEntryArgsName,
                                     receiver,
                                     errorOut)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  if (receiver == nullptr) {
    return nullptr;
  }

  if (receiver->kind == Expr::Kind::Name && localsIn.find(receiver->name) == localsIn.end()) {
    std::string normalizedMethodName = callExpr.name;
    if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
      normalizedMethodName.erase(normalizedMethodName.begin());
    }
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
    std::string helperPath;
    if (receiver->name == "FileError" &&
        (normalizedMethodName == "why" || normalizedMethodName == "is_eof" ||
         normalizedMethodName == "eof" || normalizedMethodName == "status" ||
         normalizedMethodName == "result")) {
      helperPath = preferredFileErrorHelperTarget(normalizedMethodName, defMap);
    } else if (receiver->name == "ImageError" &&
               (normalizedMethodName == "why" || normalizedMethodName == "status" ||
                normalizedMethodName == "result")) {
      helperPath = preferredImageErrorHelperTarget(normalizedMethodName, defMap);
    } else if (receiver->name == "ContainerError" &&
               (normalizedMethodName == "why" || normalizedMethodName == "status" ||
                normalizedMethodName == "result")) {
      helperPath = preferredContainerErrorHelperTarget(normalizedMethodName, defMap);
    } else if (receiver->name == "GfxError" &&
               (normalizedMethodName == "why" || normalizedMethodName == "status" ||
                normalizedMethodName == "result")) {
      helperPath = preferredGfxErrorHelperTarget(normalizedMethodName, defMap);
    }
    if (!helperPath.empty()) {
      auto defIt = defMap.find(helperPath);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
    }
  }

  std::string typeName;
  std::string resolvedTypePath;
  if (!resolveMethodReceiverTarget(*receiver,
                                   localsIn,
                                   callExpr.name,
                                   semanticAwareImportAliases,
                                   structNames,
                                   inferExprKind,
                                   resolveExprPath,
                                   typeName,
                                   resolvedTypePath,
                                   errorOut)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  std::string lookupError;
  const Definition *resolvedDef = resolveMethodDefinitionFromReceiverTarget(
      callExpr.name, typeName, resolvedTypePath, defMap, lookupError);
  auto resolveMethodDefinitionFromTypeNameWithAliasFallback = [&](const std::string &receiverTypeName,
                                                                  std::string &errorOutRef) -> const Definition * {
    if (receiverTypeName.empty()) {
      return nullptr;
    }
    if (receiverTypeName.front() == '/') {
      return resolveMethodDefinitionFromReceiverTarget(callExpr.name, "", receiverTypeName, defMap, errorOutRef);
    }
    const Definition *resolved = resolveMethodDefinitionFromReceiverTarget(
        callExpr.name, receiverTypeName, "", defMap, errorOutRef);
    if (resolved != nullptr) {
      return resolved;
    }
    if (receiverTypeName == "vector") {
      resolved = resolveMethodDefinitionFromReceiverTarget(
          callExpr.name, "", "/std/collections/vector", defMap, errorOutRef);
      if (resolved != nullptr) {
        return resolved;
      }
    }
    auto aliasIt = semanticAwareImportAliases.find(receiverTypeName);
    if (aliasIt == semanticAwareImportAliases.end()) {
      return nullptr;
    }
    const std::string aliasTypeName = normalizeMapImportAliasPath(aliasIt->second);
    if (aliasTypeName.empty()) {
      return nullptr;
    }
    return resolveMethodDefinitionFromReceiverTarget(callExpr.name, aliasTypeName, "", defMap, errorOutRef);
  };
  auto resolveStructTypePathFromScope = [&](const std::string &receiverTypeName,
                                            const std::string &namespacePrefix) -> std::string {
    if (receiverTypeName.empty()) {
      return "";
    }
    if (receiverTypeName.front() == '/') {
      return structNames.count(receiverTypeName) > 0 ? receiverTypeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        const std::string scoped = current + "/" + receiverTypeName;
        if (structNames.count(scoped) > 0) {
          return scoped;
        }
      } else {
        const std::string root = "/" + receiverTypeName;
        if (structNames.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = semanticAwareImportAliases.find(receiverTypeName);
    if (importIt != semanticAwareImportAliases.end() && structNames.count(importIt->second) > 0) {
      return importIt->second;
    }
    return "";
  };
  auto inferStructReturnPathFromReceiverDef = [&](const Definition &definition) -> std::string {
    std::function<std::string(const Expr &, std::unordered_set<std::string> &)> inferStructExprPathForCall;
    inferStructExprPathForCall = [&](const Expr &expr, std::unordered_set<std::string> &visitedDefs) -> std::string {
      if (expr.kind == Expr::Kind::Name) {
        return resolveStructTypePathFromScope(expr.name, expr.namespacePrefix);
      }
      if (expr.kind != Expr::Kind::Call) {
        return "";
      }
      const std::string resolvedPath = resolveExprPath(expr);
      if (structNames.count(resolvedPath) > 0) {
        return resolvedPath;
      }
      auto defIt = defMap.find(resolvedPath);
      if (defIt != defMap.end() && defIt->second != nullptr && visitedDefs.insert(resolvedPath).second) {
        const Definition &nestedDef = *defIt->second;
        std::string inferred = inferStructReturnPathFromDefinition(
            nestedDef,
            [&](const std::string &nestedTypeName, const std::string &namespacePrefix, std::string &resolvedOut) {
              resolvedOut = resolveStructTypePathFromScope(nestedTypeName, namespacePrefix);
              return !resolvedOut.empty();
            },
            [&](const Expr &nestedExpr) { return inferStructExprPathForCall(nestedExpr, visitedDefs); });
        visitedDefs.erase(resolvedPath);
        if (!inferred.empty()) {
          return inferred;
        }
      }
      return resolveMethodReceiverStructTypePathFromCallExpr(
          expr, resolvedPath, semanticAwareImportAliases, structNames);
    };
    std::unordered_set<std::string> visitedDefs = {definition.fullPath};
    return inferStructReturnPathFromDefinition(
        definition,
        [&](const std::string &nestedTypeName, const std::string &namespacePrefix, std::string &resolvedOut) {
          resolvedOut = resolveStructTypePathFromScope(nestedTypeName, namespacePrefix);
          return !resolvedOut.empty();
        },
        [&](const Expr &expr) { return inferStructExprPathForCall(expr, visitedDefs); });
  };
  auto findDefinitionByReceiverPath = [&](const std::string &rawPath,
                                          size_t argCount) -> const Definition * {
    if (rawPath.empty()) {
      return nullptr;
    }

    std::vector<std::string> candidates;
    auto appendCandidate = [&](std::string candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(std::move(candidate));
    };

    appendCandidate(rawPath);
    if (rawPath.front() != '/') {
      appendCandidate("/" + rawPath);
    }

    auto resolveCandidate = [&](const std::string &candidate) -> const Definition * {
      auto defIt = defMap.find(candidate);
      if (defIt != defMap.end()) {
        return defIt->second;
      }

      const std::string overloadPrefix =
          candidate + "__ov" + std::to_string(argCount);
      for (const auto &[candidatePath, candidateDef] : defMap) {
        if (candidateDef == nullptr) {
          continue;
        }
        if (candidatePath.rfind(overloadPrefix, 0) == 0 ||
            candidatePath.rfind(candidate + "__t", 0) == 0 ||
            candidatePath.rfind(candidate + "__ov", 0) == 0) {
          return candidateDef;
        }
      }
      return nullptr;
    };

    for (const auto &candidate : candidates) {
      if (const Definition *resolved = resolveCandidate(candidate)) {
        return resolved;
      }
    }

    return nullptr;
  };
  if (resolvedDef == nullptr && resolvedTypePath.empty() && receiver->kind == Expr::Kind::Call) {
    std::string nestedError = lookupError;
    const Definition *receiverDef = nullptr;
    std::string receiverPath = resolveExprPath(*receiver);
    if (receiverPath.empty() && !receiver->name.empty()) {
      receiverPath = receiver->name;
      if (!receiverPath.empty() && receiverPath.front() != '/') {
        receiverPath.insert(receiverPath.begin(), '/');
      }
    }
    receiverDef = findDefinitionByReceiverPath(receiverPath, receiver->args.size());
    if (receiverDef == nullptr && receiver->isMethodCall) {
      receiverDef = resolveMethodCallDefinitionFromExpr(*receiver,
                                                        localsIn,
                                                        isArrayCountCall,
                                                        isVectorCapacityCall,
                                                        isEntryArgsName,
                                                        semanticAwareImportAliases,
                                                        structNames,
                                                        inferExprKind,
                                                        resolveExprPath,
                                                        semanticProgram,
                                                        getReturnInfo,
                                                        defMap,
                                                        nestedError);
      if (receiverDef == nullptr && isExplicitVectorReceiverProbeHelperExpr(*receiver) &&
          !nestedError.empty()) {
        errorOut = std::move(nestedError);
        return nullptr;
      }
    }
    if (receiverDef != nullptr) {
      resolvedTypePath = inferStructReturnPathFromReceiverDef(*receiverDef);
      if (!resolvedTypePath.empty()) {
        lookupError.clear();
        resolvedDef =
            resolveMethodDefinitionFromReceiverTarget(callExpr.name, "", resolvedTypePath, defMap, lookupError);
      }
    }
    if (resolvedDef == nullptr && receiverDef != nullptr && inferReceiverTypeFromDeclaredReturn(*receiverDef, typeName)) {
      lookupError.clear();
      resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(typeName, lookupError);
    } else if (resolvedDef == nullptr && receiverDef != nullptr) {
      LocalInfo::ValueKind receiverKind = LocalInfo::ValueKind::Unknown;
      if (resolveReturnInfoKindForPath(receiverDef->fullPath, getReturnInfo, false, receiverKind)) {
        typeName = typeNameForValueKind(receiverKind);
        if (!typeName.empty()) {
          lookupError.clear();
          resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(typeName, lookupError);
        }
      }
    } else {
      LocalInfo::ValueKind inferredReceiverKind = LocalInfo::ValueKind::Unknown;
      auto isBareMapAccessReceiverProbeExpr = [&](const Expr &candidateExpr) {
        if (candidateExpr.kind != Expr::Kind::Call || candidateExpr.args.size() != 2) {
          return false;
        }
        std::string accessName;
        return getBuiltinArrayAccessName(candidateExpr, accessName) &&
               resolveMapAccessTargetInfo(candidateExpr.args.front(), localsIn).isMapTarget;
      };
      auto isBareMapTryAtReceiverProbeExpr = [&](const Expr &candidateExpr) {
        return candidateExpr.kind == Expr::Kind::Call && candidateExpr.args.size() == 2 &&
               isSimpleCallName(candidateExpr, "tryAt") &&
               resolveMapAccessTargetInfo(candidateExpr.args.front(), localsIn).isMapTarget;
      };
      const bool blocksExplicitMapReceiverProbeKindFallback =
          isExplicitMapReceiverProbeHelperExpr(*receiver);
      const bool blocksBareMapAccessReceiverProbeKindFallback =
          isBareMapAccessReceiverProbeExpr(*receiver);
      const bool blocksBareMapTryAtReceiverProbeKindFallback =
          isBareMapTryAtReceiverProbeExpr(*receiver);
      const bool blocksExplicitVectorReceiverProbeKindFallback =
          isExplicitVectorReceiverProbeHelperExpr(*receiver);
      if (!blocksExplicitMapReceiverProbeKindFallback &&
          !blocksBareMapAccessReceiverProbeKindFallback &&
          !blocksBareMapTryAtReceiverProbeKindFallback &&
          !blocksExplicitVectorReceiverProbeKindFallback) {
        if (!inferBuiltinAccessReceiverResultKind(
                *receiver, localsIn, inferExprKind, resolveExprPath, getReturnInfo, defMap, inferredReceiverKind) &&
            inferExprKind) {
          inferredReceiverKind = inferExprKind(*receiver, localsIn);
        }
      }
      const std::string inferredReceiverTypeName = typeNameForValueKind(inferredReceiverKind);
      if (!inferredReceiverTypeName.empty()) {
        lookupError.clear();
        resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(inferredReceiverTypeName, lookupError);
      }
      if (resolvedDef != nullptr) {
        return resolvedDef;
      }
      std::vector<std::string> receiverPaths = collectionHelperPathCandidates(resolveExprPath(*receiver));
      auto pruneRemovedMapCompatibilityReceiverPaths = [&](const std::string &path) {
        std::string normalizedPath = path;
        if (!normalizedPath.empty() && normalizedPath.front() != '/') {
          if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
            normalizedPath.insert(normalizedPath.begin(), '/');
          }
        }
        auto isRemovedMapCompatibilityHelper = [](const std::string &suffix) {
          return suffix == "count" || suffix == "contains" || suffix == "tryAt" ||
                 suffix == "at" || suffix == "at_unsafe" || suffix == "insert";
        };
        auto eraseCandidate = [&](const std::string &candidate) {
          for (auto it = receiverPaths.begin(); it != receiverPaths.end();) {
            if (*it == candidate) {
              it = receiverPaths.erase(it);
            } else {
              ++it;
            }
          }
        };
        if (normalizedPath.rfind("/map/", 0) == 0) {
          const std::string suffix = normalizedPath.substr(std::string("/map/").size());
          if (isRemovedMapCompatibilityHelper(suffix)) {
            eraseCandidate("/std/collections/map/" + suffix);
          }
        } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
          const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
          if (isRemovedMapCompatibilityHelper(suffix)) {
            eraseCandidate("/map/" + suffix);
          }
        }
      };
      pruneRemovedMapCompatibilityReceiverPaths(resolveExprPath(*receiver));
      auto appendUniqueReceiverPath = [&](const std::string &candidate) {
        if (candidate.empty()) {
          return;
        }
        for (const auto &existing : receiverPaths) {
          if (existing == candidate) {
            return;
          }
        }
        receiverPaths.push_back(candidate);
      };
      if (receiverDef != nullptr) {
        const auto resolvedReceiverPaths = collectionHelperPathCandidates(receiverDef->fullPath);
        for (const auto &resolvedReceiverPath : resolvedReceiverPaths) {
          appendUniqueReceiverPath(resolvedReceiverPath);
        }
        pruneRemovedMapCompatibilityReceiverPaths(receiverDef->fullPath);
      }
      for (const auto &receiverPath : receiverPaths) {
        auto receiverDefIt = defMap.find(receiverPath);
        if (receiverDefIt == defMap.end() || receiverDefIt->second == nullptr) {
          continue;
        }
        if (!inferReceiverTypeFromDeclaredReturn(*receiverDefIt->second, typeName)) {
          continue;
        }
        lookupError.clear();
        resolvedDef = resolveMethodDefinitionFromTypeNameWithAliasFallback(typeName, lookupError);
        break;
      }
    }
  }
  if (resolvedDef == nullptr) {
    const bool blocksBuiltinBareVectorCountMethod =
        isSimpleCallName(callExpr, "count") && typeName == "vector";
    const bool blocksBuiltinBareVectorAccessMethod =
        (isSimpleCallName(callExpr, "at") || isSimpleCallName(callExpr, "at_unsafe")) &&
        typeName == "vector";
    const bool blocksBuiltinBareVectorMutatorMethod =
        (isSimpleCallName(callExpr, "push") || isSimpleCallName(callExpr, "pop") ||
         isSimpleCallName(callExpr, "reserve") || isSimpleCallName(callExpr, "clear") ||
         isSimpleCallName(callExpr, "remove_at") || isSimpleCallName(callExpr, "remove_swap")) &&
        typeName == "vector";
    if (allowBuiltinFallback && !blocksBuiltinBareVectorCountMethod &&
        !blocksBuiltinBareVectorAccessMethod && !blocksBuiltinBareVectorMutatorMethod) {
      errorOut = priorError;
      return nullptr;
    }
    errorOut = std::move(lookupError);
    return nullptr;
  }
  return resolvedDef;
}

} // namespace primec::ir_lowerer
