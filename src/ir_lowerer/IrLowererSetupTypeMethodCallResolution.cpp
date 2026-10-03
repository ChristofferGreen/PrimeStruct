#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"

#include <cctype>
#include <functional>
#include <string_view>
#include <utility>

#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/ir_lowerer/IrLowererLegacyCollectionBranchCounters.h"
#include "primec/support/CollectionHelperNames.h"
#include "IrLowererSetupTypeMethodCallResolutionFileLocal.h"

namespace primec::ir_lowerer {
using namespace ir_lowerer_setup_type_method_call_resolution_file_local;

const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprStringFn &resolveExprPath,
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
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprStringFn &resolveExprPath,
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
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprStringFn &resolveExprPath,
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
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    const ExprLocalsPredicateFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprStringFn &resolveExprPath,
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
  if (allowsReceiverResolvedVectorMetadataFallback) {
    recordLegacyCollectionBranchHitCollectionVectorMetadataMethodPath();
  }
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
    if (collection_helpers::isCountHelperName(helperName) ||
        helperName == "size" ||
        collection_helpers::isContainsHelperName(helperName) ||
        collection_helpers::isTryAtHelperName(helperName) ||
        collection_helpers::isAtHelperName(helperName) ||
        collection_helpers::isAtUnsafeHelperName(helperName) ||
        collection_helpers::isInsertHelperName(helperName)) {
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
    const CollectionPairTypeInfo pairInfo =
        resolveCollectionPairTypeInfo(callExpr.args.front(),
                                      localsIn,
                                      {},
                                      semanticProgram,
                                      semanticIndexPtr);
    if (!keyValueHelperName.empty() &&
        (receiverHasKeyValueLocalInfo() || pairInfo.isKeyValueTarget)) {
      const std::string canonicalKeyValueHelper =
          canonicalKeyValueHelperPath(keyValueHelperName);
      if (const Definition *canonicalDef =
              resolveDefinitionFamilyByArity(canonicalKeyValueHelper,
                                             callExpr.args.size())) {
        return canonicalDef;
      }
      // TODO-5300: a monomorph-rewritten key/value constructor call
      // used directly as a receiver gets materialized into a synthetic
      // "__collection_receiver_" temporary (see
      // emitMaterializedCollectionReceiverExpr) bound to the specialized
      // backing struct minted for this K/V pair, but the free-standing
      // helper template family for this operation never gets a matching
      // specialization (nothing else triggers it). The struct's own
      // nested member for this operation IS always specialized
      // alongside the struct itself, so fall back to it directly. Scope
      // this strictly to that synthetic receiver shape - an ordinary
      // named local of key/value type intentionally has no such
      // fallback (some call shapes on it are deliberately rejected by
      // specific backends).
      constexpr std::string_view materializedCollectionReceiverPrefix =
          "__collection_receiver_";
      const Expr &receiverExprForFallback = callExpr.args.front();
      const bool receiverIsMaterializedCollectionTemp =
          receiverExprForFallback.kind == Expr::Kind::Name &&
          receiverExprForFallback.name.rfind(materializedCollectionReceiverPrefix, 0) == 0;
      if (receiverIsMaterializedCollectionTemp && !pairInfo.structTypeName.empty()) {
        std::string memberMethodName = "map";
        bool capitalizeNext = true;
        for (char nameChar : keyValueHelperName) {
          if (nameChar == '_') {
            capitalizeNext = true;
            continue;
          }
          memberMethodName +=
              capitalizeNext ? static_cast<char>(std::toupper(nameChar)) : nameChar;
          capitalizeNext = false;
        }
        const std::string memberMethodPath =
            pairInfo.structTypeName + "/" + memberMethodName;
        auto memberDefIt = defMap.find(memberMethodPath);
        if (memberDefIt != defMap.end() && memberDefIt->second != nullptr &&
            memberDefIt->second->parameters.size() + 1 == callExpr.args.size()) {
          return memberDefIt->second;
        }
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
          recordLegacyCollectionBranchHitCollectionVectorOwnerPath();
          recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathSite();
          // targetPath already names the exact method (buildReceiverMethodTargetPath
          // was a no-op above), so try the direct lookup before giving up: a
          // stdlib-owned struct can have its own real field_count/field_capacity
          // definitions directly at this path.
          if (const Definition *resolvedDef = tryResolvedPath(targetPath);
              resolvedDef != nullptr) {
            recordLegacyCollectionBranchHitCollectionVectorOwnerPathTargetPathFallbackResolved();
            return resolvedDef;
          }
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
        stdlibSurfaceCanonicalHelperPath(StdlibSurfaceId::CollectionsManifestSurface0, "count");
    const bool requestsExplicitVectorCountMethod =
        (!canonicalVectorCountPath.empty() &&
         explicitMethodPath == canonicalVectorCountPath) ||
        explicitMethodPath ==
            vectorBuiltinStructNormalizedPath() + "/count";
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
    if (resolvedPath == collection_helpers::kCanonicalSoaToAos) {
      errorOut.clear();
      return nullptr;
    }
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
      if (!fallbackDirectTarget.empty() &&
          (isSimpleCallName(callExpr, "at") || isSimpleCallName(callExpr, "at_unsafe")) &&
          blocksSyntheticCollectionFallbackDirectTarget(fallbackDirectTarget)) {
        if (const Definition *resolvedDef = resolveLoweredDefinitionPath(fallbackDirectTarget);
            resolvedDef != nullptr && !resolvedDef->statements.empty()) {
          return resolvedDef;
        }
      }
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
      if (isBuiltinFileHandleMethodName(extractMethodLeafName(explicitMethodPath))) {
        errorOut.clear();
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
          (resolvedPath == collection_helpers::kRootedStringCount || resolvedPath == collection_helpers::kRootedArrayCount);
      const bool routesExplicitVectorCountMethodThroughArgsPackCount =
          routesExplicitVectorCountMethodThroughBuiltinScalarTarget &&
          resolvedPath == collection_helpers::kRootedArrayCount &&
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
          recordLegacyCollectionBranchHitCollectionVectorOwnerPath();
          recordLegacyCollectionBranchHitCollectionVectorOwnerPathReceiverTypeSite();
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
      if (!requestsExplicitVectorCountMethod &&
          (preferredResolvedPath == collection_helpers::kRootedStringCount ||
           preferredResolvedPath == collection_helpers::kCanonicalSoaToAos) &&
          isBuiltinClassifiedMethodCallTarget(preferredResolvedPath, callExpr)) {
        errorOut.clear();
        return nullptr;
      }
      errorOut =
          "semantic-product method-call target missing lowered definition: " +
          preferredResolvedPath;
      return nullptr;
    }
    if (errorOut.empty()) {
      errorOut =
          "semantic-product method-call target missing lowered definition: " +
          (resolvedPath.empty() ? explicitMethodPath : resolvedPath);
      return nullptr;
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
  // Note: a bare `at`/`at_unsafe` builtin access call deliberately does NOT
  // contribute to `allowBuiltinFallback` on its own - `isBuiltinBareVectorAccessMethod`
  // above already excludes the one case (a genuine vector-target receiver)
  // where this method resolves through the builtin path instead of a real
  // definition lookup. For every other receiver shape (e.g. a bare array,
  // or an entry-args receiver), an access call must surface its real
  // "unknown method" diagnostic rather than silently falling back and
  // discarding it (mirrors the same split in resolveMethodCallReceiverExpr;
  // see the "keeps builtin array count fallback and rejects bare vector
  // method fallback" test, which pins exactly this).
  const bool allowBuiltinFallback =
      !isExplicitRemovedVectorMethodAlias && !isExplicitKeyValueMethodAlias &&
      !isExplicitKeyValueContainsOrTryAtMethod &&
      !isBuiltinBareVectorCapacityMethod && !isBuiltinBareVectorAccessMethod &&
      !isBuiltinBareVectorMutatorMethod &&
      (isBuiltinCountOrCapacityCall || isBuiltinVectorMutatorCall ||
       isBuiltinKeyValueContainsOrTryAtCall ||
       (isArrayCountCall && isArrayCountCall(callExpr, localsIn)) ||
       (isVectorCapacityCall && isVectorCapacityCall(callExpr, localsIn)));

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
        vectorBuiltinStructNormalizedPath().substr(1) + "/";
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
