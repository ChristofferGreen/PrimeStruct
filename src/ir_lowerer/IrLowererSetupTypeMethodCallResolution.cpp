#include "IrLowererSetupTypeHelpers.h"

#include <functional>
#include <string_view>
#include <utility>

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

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

bool isKeyValueConstructorDirectTargetPath(std::string path) {
  const size_t specializationSuffix = path.find("__t");
  if (specializationSuffix != std::string::npos) {
    path.erase(specializationSuffix);
  }
  const size_t overloadSuffix = path.find("__ov");
  if (overloadSuffix != std::string::npos) {
    path.erase(overloadSuffix);
  }
  return path == canonicalKeyValueConstructorPath();
}

const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadataLocal() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors");
}

std::string findKeyValueConstructorBridgePathChoiceBySource(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr || expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata = keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  for (const auto *entry : bridgePathChoices) {
    if (entry == nullptr ||
        entry->sourceLine != expr.sourceLine ||
        entry->sourceColumn != expr.sourceColumn ||
        semanticProgramBridgePathChoiceStdlibSurfaceId(*entry) !=
            metadata->id ||
        entry->chosenPathId == InvalidSymbolId) {
      continue;
    }
    const std::string_view chosenPath =
        semanticProgramResolveCallTargetString(*semanticProgram, entry->chosenPathId);
    if (!chosenPath.empty()) {
      return std::string(chosenPath);
    }
  }
  return {};
}

std::string extractMethodLeafName(const std::string &methodPath) {
  if (methodPath.empty()) {
    return "";
  }
  const size_t slash = methodPath.find_last_of('/');
  if (slash == std::string::npos) {
    return methodPath;
  }
  return methodPath.substr(slash + 1);
}

bool isExperimentalSoaVectorSpecializedStructPath(std::string_view path) {
  return path.starts_with("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__") ||
         path.starts_with("std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__") ||
         path.starts_with("Soa" "Vector" "__");
}

std::string resolveSpecializedExperimentalSoaVectorStructPath(
    const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  while (true) {
    if (!normalized.empty() && normalized.front() != '/') {
      normalized.insert(normalized.begin(), '/');
    }
    if (isExperimentalSoaVectorSpecializedStructPath(normalized)) {
      return normalized;
    }

    std::string base;
    std::string argList;
    if (!splitTemplateTypeName(normalized, base, argList)) {
      return "";
    }

    const std::string normalizedBase =
        normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
    if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
        !argList.empty()) {
      std::vector<std::string> wrappedArgs;
      if (!splitTemplateArgs(argList, wrappedArgs) || wrappedArgs.size() != 1) {
        return "";
      }
      normalized = trimTemplateTypeText(wrappedArgs.front());
      continue;
    }

    if (normalizedBase != "soa" "_vector" || argList.empty()) {
      return "";
    }

    std::vector<std::string> templateArgs;
    if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 1) {
      return "";
    }

    std::string normalizedArg = trimTemplateTypeText(templateArgs.front());
    if (!normalizedArg.empty() && normalizedArg.front() == '/') {
      normalizedArg.erase(normalizedArg.begin());
    }
    return specializedExperimentalSoaVectorStructPathForElementType(
        normalizedArg);
  }
}

bool matchesGeneratedDefinitionFamilyPath(const std::string &candidatePath,
                                          const std::string &targetPath) {
  if (candidatePath == targetPath) {
    return true;
  }

  auto hasGeneratedTerminalSuffix = [&](size_t suffixPos) {
    if (suffixPos >= candidatePath.size()) {
      return false;
    }
    const bool isSpecialization =
        candidatePath.compare(suffixPos, 3, "__t") == 0;
    const bool isOverload =
        candidatePath.compare(suffixPos, 4, "__ov") == 0;
    return (isSpecialization || isOverload) &&
           candidatePath.find('/', suffixPos) == std::string::npos;
  };
  auto hasTerminalLeafOrGeneratedSuffix = [&](size_t leafEnd) {
    return leafEnd == candidatePath.size() ||
           hasGeneratedTerminalSuffix(leafEnd);
  };

  if (candidatePath.rfind(targetPath, 0) == 0 &&
      hasGeneratedTerminalSuffix(targetPath.size())) {
    return true;
  }

  const size_t slash = targetPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }

  const std::string parentPath = targetPath.substr(0, slash);
  const std::string leafPath = targetPath.substr(slash);
  if (candidatePath.rfind(parentPath, 0) != 0) {
    return false;
  }

  const size_t specializationPos = parentPath.size();
  const bool specializesParent =
      candidatePath.compare(specializationPos, 3, "__t") == 0 ||
      candidatePath.compare(specializationPos, 4, "__ov") == 0;
  if (!specializesParent) {
    return false;
  }

  const size_t leafPos = candidatePath.find(leafPath, specializationPos);
  if (leafPos == std::string::npos ||
      candidatePath.compare(leafPos, leafPath.size(), leafPath) != 0) {
    return false;
  }
  return hasTerminalLeafOrGeneratedSuffix(leafPos + leafPath.size());
}

bool blocksSyntheticCollectionFallbackDirectTarget(const std::string &targetPath) {
  const std::string normalized = normalizeCollectionHelperPath(targetPath);
  return normalized.rfind(normalizeBuiltinCollectionStructPath("vec" "tor") + "/", 0) == 0 ||
         normalized.rfind(collectionMemberRoot("vector"), 0) == 0 ||
         normalized.rfind(keyValueCollectionAliasRoot() + "/", 0) == 0 ||
         normalized.rfind(collectionMemberRoot("map"), 0) == 0 ||
         normalized.rfind("/soa" "_vector/", 0) == 0 ||
         normalized.rfind("/std/collections/" "soa" "_vector/", 0) == 0 ||
         normalized.rfind(experimentalCollectionMemberRoot("vec" "tor"), 0) == 0 ||
         normalized.rfind(experimentalCollectionMemberRoot("map"), 0) == 0 ||
         normalized.rfind("/std/collections/experimental" "_soa" "_vector/", 0) == 0;
}

bool isCollectionVectorMetadataMethodPath(const std::string &methodPath) {
  const std::string leaf = extractMethodLeafName(methodPath);
  return leaf == "field_count" || leaf == "field_capacity" ||
         leaf == "set_field_count" || leaf == "set_field_capacity";
}

bool isCollectionVectorOwnerPath(const std::string &path) {
  const std::string normalized = normalizeCollectionHelperPath(path);
  return isExperimentalCollectionTypeName(normalized, "vector", "Vector");
}

std::string unwrapSemanticReceiverTypeText(std::string typeText) {
  typeText = trimTemplateTypeText(std::move(typeText));
  while (true) {
    std::string base;
    std::string argList;
    if (!splitTemplateTypeName(typeText, base, argList)) {
      return typeText;
    }
    const std::string normalizedBase =
        normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
    if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
      return typeText;
    }
    std::vector<std::string> args;
    if (!splitTemplateArgs(argList, args) || args.size() != 1) {
      return typeText;
    }
    typeText = trimTemplateTypeText(args.front());
  }
}

std::string findSemanticProductMethodCallReceiverTypeText(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return "";
  }
  const auto methodCallTargets =
      semanticProgramMethodCallTargetView(*semanticProgram);
  for (const auto *entry : methodCallTargets) {
    if (entry == nullptr || entry->semanticNodeId != expr.semanticNodeId) {
      continue;
    }
    if (entry->receiverTypeTextId != InvalidSymbolId) {
      const std::string_view internedTypeText =
          semanticProgramResolveCallTargetString(*semanticProgram,
                                                 entry->receiverTypeTextId);
      if (!internedTypeText.empty()) {
        return std::string(internedTypeText);
      }
    }
    return entry->receiverTypeText;
  }
  return "";
}

std::string buildReceiverMethodTargetPath(const std::string &receiverPath,
                                          const std::string &explicitMethodPath) {
  if (receiverPath.empty()) {
    return "";
  }
  const std::string methodLeaf = extractMethodLeafName(explicitMethodPath);
  if (methodLeaf.empty()) {
    return "";
  }
  const std::string methodSuffix = "/" + methodLeaf;
  if (receiverPath.ends_with(methodSuffix)) {
    return receiverPath;
  }
  return receiverPath + methodSuffix;
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
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  const SemanticProductIndex *const semanticIndexPtr =
      semanticProgram != nullptr ? &semanticIndex : nullptr;

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

  const std::string explicitMethodPath = describeMethodCallExpr(callExpr);
  const bool allowsReceiverResolvedVectorMetadataFallback =
      isCollectionVectorMetadataMethodPath(explicitMethodPath);
  const std::string rootedKeyValuePrefix =
      keyValueCollectionAliasRoot(false) + "/";
  const std::string canonicalKeyValuePrefix = collectionMemberRoot("map", false);
  auto sourceKeyValueMethodHelperName = [&]() -> std::string {
    std::string helperName = explicitMethodPath;
    if (!helperName.empty() && helperName.front() == '/') {
      helperName.erase(helperName.begin());
    }
    if (helperName.rfind(canonicalKeyValuePrefix, 0) == 0) {
      helperName.erase(0, canonicalKeyValuePrefix.size());
    } else if (helperName.rfind(rootedKeyValuePrefix, 0) == 0) {
      helperName.erase(0, rootedKeyValuePrefix.size());
    }
    if (helperName == "count" || helperName == "count_ref" ||
        helperName == "size" ||
        helperName == "contains" || helperName == "contains_ref" ||
        helperName == "tryAt" || helperName == "tryAt_ref" ||
        helperName == "at" || helperName == "at_ref" ||
        helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
        helperName == "insert" || helperName == "insert_ref") {
      return helperName;
    }
    return {};
  };
  auto resolveDefinitionFamilyByArity = [&](const std::string &path,
                                            size_t argCount) -> const Definition * {
    auto defIt = defMap.find(path);
    if (defIt != defMap.end() && defIt->second != nullptr &&
        defIt->second->parameters.size() == argCount) {
      return defIt->second;
    }
    const std::string overloadPrefix =
        path + "__ov" + std::to_string(argCount);
    const std::string specializedPrefix = path + "__t";
    for (const auto &[candidatePath, candidateDef] : defMap) {
      if (candidateDef == nullptr) {
        continue;
      }
      if (candidatePath.rfind(overloadPrefix, 0) == 0 ||
          (candidatePath.rfind(specializedPrefix, 0) == 0 &&
           candidateDef->parameters.size() == argCount)) {
        return candidateDef;
      }
    }
    return nullptr;
  };
  if (!callExpr.args.empty()) {
    const std::string keyValueHelperName = sourceKeyValueMethodHelperName();
    auto receiverHasKeyValueLocalInfo = [&]() {
      const Expr &receiverExpr = callExpr.args.front();
      if (receiverExpr.kind != Expr::Kind::Name) {
        return false;
      }
      auto localIt = localsIn.find(receiverExpr.name);
      if (localIt == localsIn.end()) {
        return false;
      }
      const LocalInfo &info = localIt->second;
      return hasKeyValueKinds(info);
    };
    if (!keyValueHelperName.empty() &&
        (receiverHasKeyValueLocalInfo() ||
         resolveCollectionPairTypeInfo(callExpr.args.front(),
                                    localsIn,
                                    {},
                                    semanticProgram,
                                    semanticIndexPtr)
             .isKeyValueTarget)) {
      const std::string canonicalKeyValueHelper =
          canonicalKeyValueHelperPath(keyValueHelperName);
      if (const Definition *canonicalDef =
              resolveDefinitionFamilyByArity(canonicalKeyValueHelper,
                                             callExpr.args.size())) {
        return canonicalDef;
      }
    }
  }
  if (isExplicitKeyValueMethodAliasPath(explicitMethodPath)) {
    if (semanticProgram != nullptr && !callExpr.args.empty() &&
        callExpr.args.front().kind != Expr::Kind::Call) {
      std::string resolvedPath =
          findSemanticProductMethodCallTarget(semanticProgram, callExpr);
      if (resolvedPath.empty()) {
        resolvedPath = findSemanticProductDirectCallTarget(semanticProgram,
                                                           callExpr);
      }
      if (resolvedPath.empty()) {
        resolvedPath = findSemanticProductBridgePathChoice(semanticProgram,
                                                           callExpr);
      }
      if (!resolvedPath.empty()) {
        if (const Definition *semanticTarget =
                resolveDefinitionFamilyByArity(resolvedPath,
                                               callExpr.args.size())) {
          return semanticTarget;
        }
      }
    }
    errorOut = "unknown method: " + explicitMethodPath;
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
              matchesGeneratedDefinitionFamilyPath(candidatePath, path)) {
            return candidateDef;
          }
        }
        return nullptr;
      };

      if (allowsReceiverResolvedVectorMetadataFallback) {
        const std::string receiverMethodTargetPath =
            buildReceiverMethodTargetPath(targetPath, explicitMethodPath);
        if (!receiverMethodTargetPath.empty() &&
            receiverMethodTargetPath != targetPath) {
          if (const Definition *resolvedMethodDef =
                  tryResolvedPath(receiverMethodTargetPath);
              resolvedMethodDef != nullptr) {
            return resolvedMethodDef;
          }
        }
        if (isCollectionVectorOwnerPath(targetPath)) {
          return nullptr;
        }
      }
      if (const Definition *resolvedDef = tryResolvedPath(targetPath);
          resolvedDef != nullptr) {
        return resolvedDef;
      }
      const std::string receiverMethodTargetPath =
          buildReceiverMethodTargetPath(targetPath, explicitMethodPath);
      if (!receiverMethodTargetPath.empty() &&
          receiverMethodTargetPath != targetPath) {
        if (const Definition *resolvedMethodDef =
                tryResolvedPath(receiverMethodTargetPath);
            resolvedMethodDef != nullptr) {
          return resolvedMethodDef;
        }
      }
      const std::string normalizedTargetPath =
          normalizeCollectionHelperPath(targetPath);
      if (normalizedTargetPath != targetPath) {
        if (const Definition *normalizedResolvedDef =
                tryResolvedPath(normalizedTargetPath);
            normalizedResolvedDef != nullptr) {
          return normalizedResolvedDef;
        }
        const std::string normalizedReceiverMethodTargetPath =
            buildReceiverMethodTargetPath(normalizedTargetPath,
                                          explicitMethodPath);
        if (!normalizedReceiverMethodTargetPath.empty() &&
            normalizedReceiverMethodTargetPath != normalizedTargetPath) {
          return tryResolvedPath(normalizedReceiverMethodTargetPath);
        }
      }
      return nullptr;
    };
    const std::string canonicalVectorCountPath =
        stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::CollectionsVectorHelperSurface, "count");
    const bool requestsExplicitVectorCountMethod =
        (!canonicalVectorCountPath.empty() &&
         explicitMethodPath == canonicalVectorCountPath) ||
        explicitMethodPath ==
            normalizeBuiltinCollectionStructPath("vec" "tor") + "/count";
    if (callExpr.semanticNodeId == 0 &&
        (callExpr.sourceLine <= 0 || callExpr.sourceColumn <= 0 ||
         callExpr.name.empty()) &&
        (callExpr.args.empty() ||
         callExpr.args.front().kind != Expr::Kind::Call)) {
      errorOut = "missing semantic-product method-call semantic id: " +
                 describeMethodCallExpr(callExpr);
      return nullptr;
    }
    const std::string resolvedPath =
        findSemanticProductMethodCallTarget(semanticProgram, callExpr);
    if (resolvedPath.empty()) {
      const std::string directCallTarget =
          findSemanticProductDirectCallTarget(semanticProgram, callExpr);
      const std::string bridgePathChoice =
          findSemanticProductBridgePathChoice(semanticProgram, callExpr);
      const std::string sourceMatchedBridgePathChoice =
          bridgePathChoice.empty()
              ? findKeyValueConstructorBridgePathChoiceBySource(semanticProgram, callExpr)
              : std::string{};
      const std::string fallbackDirectTarget =
          !directCallTarget.empty()
              ? directCallTarget
              : (!bridgePathChoice.empty() ? bridgePathChoice : sourceMatchedBridgePathChoice);
      const bool directTargetKeepsSyntheticCollectionFallback =
          !fallbackDirectTarget.empty() &&
          (((isSimpleCallName(callExpr, "count") ||
             isSimpleCallName(callExpr, "capacity") ||
             isSimpleCallName(callExpr, "at") ||
             isSimpleCallName(callExpr, "at_unsafe")) &&
            !blocksSyntheticCollectionFallbackDirectTarget(fallbackDirectTarget)) ||
           (isSimpleCallName(callExpr, "map") &&
            isKeyValueConstructorDirectTargetPath(fallbackDirectTarget)));
      if (directTargetKeepsSyntheticCollectionFallback) {
        if (const Definition *resolvedDef =
                resolveLoweredDefinitionPath(fallbackDirectTarget);
            resolvedDef != nullptr) {
          return resolvedDef;
        }
        errorOut =
            "semantic-product method-call target missing lowered definition: " +
            fallbackDirectTarget;
        return nullptr;
      }
      if (!allowsReceiverResolvedVectorMetadataFallback &&
          (callExpr.args.empty() ||
           callExpr.args.front().kind != Expr::Kind::Call)) {
        errorOut = "missing semantic-product method-call target: " +
                   describeMethodCallExpr(callExpr);
        return nullptr;
      }
    }
    if (!resolvedPath.empty()) {
      const bool routesExplicitVectorCountMethodThroughMapMethodTarget =
          requestsExplicitVectorCountMethod &&
          normalizeCollectionHelperPath(resolvedPath) ==
              canonicalKeyValueHelperPath("count");
      const bool routesExplicitVectorCountMethodThroughBuiltinScalarTarget =
          requestsExplicitVectorCountMethod &&
          (resolvedPath == "/string/count" || resolvedPath == "/array/count");
      const bool routesExplicitVectorCountMethodThroughArgsPackCount =
          routesExplicitVectorCountMethodThroughBuiltinScalarTarget &&
          resolvedPath == "/array/count" &&
          [&]() {
            std::string receiverTypeText =
                unwrapSemanticReceiverTypeText(
                    findSemanticProductMethodCallReceiverTypeText(semanticProgram,
                                                                  callExpr));
            std::string base;
            std::string argText;
            return splitTemplateTypeName(receiverTypeText, base, argText) &&
                   normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base)) == "args";
      }();
      if (routesExplicitVectorCountMethodThroughArgsPackCount) {
        errorOut.clear();
        return nullptr;
      }
      if (routesExplicitVectorCountMethodThroughBuiltinScalarTarget) {
        if (const Definition *explicitVectorCountDef =
                resolveLoweredDefinitionPath(explicitMethodPath);
            explicitVectorCountDef != nullptr) {
          return explicitVectorCountDef;
        }
      }
      const std::string explicitVectorCountBridgePath =
          routesExplicitVectorCountMethodThroughMapMethodTarget
              ? findSemanticProductBridgePathChoice(semanticProgram, callExpr)
              : std::string{};
      const std::string preferredResolvedPath =
          !explicitVectorCountBridgePath.empty()
              ? explicitVectorCountBridgePath
              : resolvedPath;
      if (isExplicitKeyValueMethodAliasPath(explicitMethodPath) &&
          preferredResolvedPath == explicitMethodPath) {
        errorOut =
            "semantic-product method-call target missing lowered definition: " +
            preferredResolvedPath;
        return nullptr;
      }
      if (allowsReceiverResolvedVectorMetadataFallback) {
        std::string receiverTypeText =
            unwrapSemanticReceiverTypeText(
                findSemanticProductMethodCallReceiverTypeText(semanticProgram,
                                                              callExpr));
        if (!receiverTypeText.empty() && receiverTypeText.front() != '/') {
          receiverTypeText.insert(receiverTypeText.begin(), '/');
        }
        if (isCollectionVectorOwnerPath(receiverTypeText)) {
          const std::string receiverMethodPath =
              buildReceiverMethodTargetPath(receiverTypeText, explicitMethodPath);
          if (const Definition *receiverTypedDef =
                  resolveLoweredDefinitionPath(receiverMethodPath);
              receiverTypedDef != nullptr) {
            return receiverTypedDef;
          }
        }
      }
      if (const Definition *resolvedDef =
              resolveLoweredDefinitionPath(preferredResolvedPath);
          resolvedDef != nullptr) {
        return resolvedDef;
      }
      if (preferredResolvedPath.rfind("/file/", 0) == 0) {
        errorOut.clear();
        return nullptr;
      }
      if (allowsReceiverResolvedVectorMetadataFallback) {
        errorOut.clear();
      } else {
        errorOut =
            "semantic-product method-call target missing lowered definition: " +
            preferredResolvedPath;
        return nullptr;
      }
    }
  }

  std::string accessName;
  const bool isBuiltinAccessCall = getBuiltinArrayAccessName(callExpr, accessName) && callExpr.args.size() == 2;
  const bool isBuiltinCountOrCapacityCall =
      isUnqualifiedCollectionBuiltinName(callExpr, "count") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "capacity");
  const bool isBuiltinBareVectorCapacityMethod =
      isUnqualifiedCollectionBuiltinName(callExpr, "capacity") &&
      isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn);
  const bool isBuiltinBareVectorAccessMethod =
      callExpr.isMethodCall && callExpr.args.size() == 2 &&
      isBuiltinAccessCall &&
      resolveArrayVectorAccessTargetInfo(callExpr.args.front(),
                                         localsIn,
                                         {},
                                         semanticProgram,
                                         semanticIndexPtr)
          .isVectorTarget;
  const bool isBuiltinBareVectorMutatorMethod =
      callExpr.isMethodCall &&
      (isSimpleCallName(callExpr, "push") || isSimpleCallName(callExpr, "pop") ||
       isSimpleCallName(callExpr, "reserve") || isSimpleCallName(callExpr, "clear") ||
       isSimpleCallName(callExpr, "remove_at") || isSimpleCallName(callExpr, "remove_swap")) &&
      !callExpr.args.empty() &&
      resolveArrayVectorAccessTargetInfo(callExpr.args.front(),
                                         localsIn,
                                         {},
                                         semanticProgram,
                                         semanticIndexPtr)
          .isVectorTarget;
  const bool isBuiltinVectorMutatorCall =
      isUnqualifiedCollectionBuiltinName(callExpr, "push") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "pop") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "reserve") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "clear") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "remove_at") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "remove_swap");
  const bool isExplicitRemovedVectorMethodAlias =
      isExplicitRemovedVectorMethodAliasPath(explicitMethodPath);
  const bool isExplicitKeyValueMethodAlias =
      isExplicitKeyValueMethodAliasPath(explicitMethodPath);
  const bool isExplicitKeyValueContainsOrTryAtMethod =
      isExplicitKeyValueContainsOrTryAtMethodPath(explicitMethodPath);
  const bool isBuiltinKeyValueContainsOrTryAtCall =
      isSimpleCallName(callExpr, "contains") || isSimpleCallName(callExpr, "tryAt") ||
      isSimpleCallName(callExpr, "insert");
  const bool allowBuiltinFallback =
      !isExplicitRemovedVectorMethodAlias && !isExplicitKeyValueMethodAlias &&
      !isExplicitKeyValueContainsOrTryAtMethod &&
      !isBuiltinBareVectorCapacityMethod && !isBuiltinBareVectorAccessMethod &&
      !isBuiltinBareVectorMutatorMethod &&
      (isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
       isBuiltinKeyValueContainsOrTryAtCall ||
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
    std::string normalizedMethodName = explicitMethodPath;
    if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
      normalizedMethodName.erase(normalizedMethodName.begin());
    }
    const std::string rootedVectorPrefix =
        normalizeBuiltinCollectionStructPath("vec" "tor").substr(1) + "/";
    const std::string canonicalVectorPrefix =
        collectionMemberRoot("vector", false);
    if (normalizedMethodName.rfind(rootedVectorPrefix, 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(rootedVectorPrefix.size());
    } else if (normalizedMethodName.rfind("array/", 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
    } else if (normalizedMethodName.rfind(canonicalVectorPrefix, 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(canonicalVectorPrefix.size());
    } else if (normalizedMethodName.rfind(rootedKeyValuePrefix, 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(rootedKeyValuePrefix.size());
    } else if (normalizedMethodName.rfind(canonicalKeyValuePrefix, 0) == 0) {
      normalizedMethodName = normalizedMethodName.substr(canonicalKeyValuePrefix.size());
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
                                   explicitMethodPath,
                                   semanticAwareImportAliases,
                                   structNames,
                                   inferExprKind,
                                   resolveExprPath,
                                   typeName,
                                   resolvedTypePath,
                                   errorOut,
                                   semanticProgram,
                                   semanticIndexPtr)) {
    if (allowBuiltinFallback) {
      errorOut = priorError;
    }
    return nullptr;
  }
  std::string lookupError;
  const Definition *resolvedDef = resolveMethodDefinitionFromReceiverTarget(
      explicitMethodPath, typeName, resolvedTypePath, defMap, lookupError);
  auto resolveMethodDefinitionFromTypeNameWithAliasFallback = [&](const std::string &receiverTypeName,
                                                                  std::string &errorOutRef) -> const Definition * {
    if (receiverTypeName.empty()) {
      return nullptr;
    }
    if (receiverTypeName.front() == '/') {
      return resolveMethodDefinitionFromReceiverTarget(
          explicitMethodPath, "", receiverTypeName, defMap, errorOutRef);
    }
    const Definition *resolved = resolveMethodDefinitionFromReceiverTarget(
        explicitMethodPath, receiverTypeName, "", defMap, errorOutRef);
    if (resolved != nullptr) {
      return resolved;
    }
    if (receiverTypeName == "vector") {
      resolved = resolveMethodDefinitionFromReceiverTarget(
          explicitMethodPath, "", collectionTypePath("vector"), defMap, errorOutRef);
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
    return resolveMethodDefinitionFromReceiverTarget(
        explicitMethodPath, aliasTypeName, "", defMap, errorOutRef);
  };
  auto resolveStructTypePathFromScope = [&](const std::string &receiverTypeName,
                                            const std::string &namespacePrefix) -> std::string {
    if (receiverTypeName.empty()) {
      return "";
    }
    if (const std::string specializedSoaPath =
            resolveSpecializedExperimentalSoaVectorStructPath(receiverTypeName);
        !specializedSoaPath.empty()) {
      return specializedSoaPath;
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
            matchesGeneratedDefinitionFamilyPath(candidatePath, candidate)) {
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
    if (semanticProgram != nullptr) {
      if (receiverPath.empty()) {
        receiverPath =
            findSemanticProductDirectCallTarget(semanticProgram, *receiver);
      }
      if (receiverPath.empty()) {
        receiverPath =
            findSemanticProductBridgePathChoice(semanticProgram, *receiver);
      }
    }
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
            resolveMethodDefinitionFromReceiverTarget(
                explicitMethodPath, "", resolvedTypePath, defMap, lookupError);
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
      auto isBareKeyValueAccessReceiverProbeExpr = [&](const Expr &candidateExpr) {
        if (candidateExpr.kind != Expr::Kind::Call || candidateExpr.args.size() != 2) {
          return false;
        }
        std::string accessName;
        return getBuiltinArrayAccessName(candidateExpr, accessName) &&
               resolveCollectionPairTypeInfo(candidateExpr.args.front(),
                                          localsIn,
                                          {},
                                          semanticProgram,
                                          semanticIndexPtr)
                   .isKeyValueTarget;
      };
      auto isBareKeyValueTryAtReceiverProbeExpr = [&](const Expr &candidateExpr) {
        return candidateExpr.kind == Expr::Kind::Call && candidateExpr.args.size() == 2 &&
               isSimpleCallName(candidateExpr, "tryAt") &&
               resolveCollectionPairTypeInfo(candidateExpr.args.front(),
                                          localsIn,
                                          {},
                                          semanticProgram,
                                          semanticIndexPtr)
                   .isKeyValueTarget;
      };
      const bool blocksExplicitKeyValueReceiverProbeKindFallback =
          isExplicitKeyValueReceiverProbeHelperExpr(*receiver);
      const bool blocksBareKeyValueAccessReceiverProbeKindFallback =
          isBareKeyValueAccessReceiverProbeExpr(*receiver);
      const bool blocksBareKeyValueTryAtReceiverProbeKindFallback =
          isBareKeyValueTryAtReceiverProbeExpr(*receiver);
      const bool blocksExplicitVectorReceiverProbeKindFallback =
          blocksExplicitVectorReceiverProbeKindFallbackExpr(*receiver);
      if (!blocksExplicitKeyValueReceiverProbeKindFallback &&
          !blocksBareKeyValueAccessReceiverProbeKindFallback &&
          !blocksBareKeyValueTryAtReceiverProbeKindFallback &&
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
        isUnqualifiedCollectionBuiltinName(callExpr, "count") && typeName == "vector";
    const bool blocksBuiltinBareVectorAccessMethod =
        isBuiltinAccessCall && typeName == "vector";
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
