#include "IrLowererSetupTypeHelpers.h"

#include <algorithm>
#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedCallPath(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return expr.name;
}

bool prefersExactDirectMapCountLikeReturnPath(const Expr &callExpr) {
  const std::string scopedCallPath = resolveScopedCallPath(callExpr);
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
      scopedCallPath.empty() || scopedCallPath.front() != '/') {
    return false;
  }
  std::string helperName;
  return resolveKeyValueHelperAliasName(callExpr, helperName) &&
         (helperName == "count" || helperName == "count_ref" || helperName == "contains" ||
          helperName == "tryAt");
}

std::string canonicalKeyValueHelperPathForSetupReturnKind(
    std::string_view helperName) {
  const auto *metadata = findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
}

struct SemanticReturnKindTargetInfo {
  ArrayVectorAccessTargetInfo arrayVectorInfo{};
  CollectionPairTypeInfo keyValueInfo{};
  LocalInfo::ValueKind valueKind{LocalInfo::ValueKind::Unknown};
};

bool classifySemanticReturnKindCollectionTypeText(
    const std::string &typeText,
    SemanticReturnKindTargetInfo &infoOut) {
  const std::string normalizedType = trimTemplateTypeText(typeText);
  if (normalizedType.empty()) {
    return false;
  }
  infoOut.valueKind = valueKindFromTypeName(normalizedType);

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return true;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args)) {
    return true;
  }
  if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
    return classifySemanticReturnKindCollectionTypeText(args.front(), infoOut);
  }
  if ((base == "array" || base == "vector" || base == "Buffer" ||
       base == "soa" "_vector") &&
      args.size() == 1) {
    infoOut.arrayVectorInfo.isArrayOrVectorTarget = true;
    infoOut.arrayVectorInfo.isVectorTarget = base == "vector";
    infoOut.arrayVectorInfo.isSoaVector = base == "soa" "_vector";
    infoOut.arrayVectorInfo.elemKind = valueKindFromTypeName(args.front());
    return true;
  }
  if (base == "map" && args.size() == 2) {
    infoOut.keyValueInfo.isKeyValueTarget = true;
    infoOut.keyValueInfo.keyValueKeyKind = valueKindFromTypeName(args.front());
    infoOut.keyValueInfo.keyValueValueKind = valueKindFromTypeName(args.back());
    return true;
  }
  return true;
}

bool classifySemanticReturnKindCollectionSpecialization(
    const SemanticProgram *semanticProgram,
    const SemanticProgramCollectionSpecialization &fact,
    SemanticReturnKindTargetInfo &infoOut) {
  const std::string family = normalizeCollectionBindingTypeName(
      resolveSemanticProductTypeText(semanticProgram,
                                     fact.collectionFamily,
                                     fact.collectionFamilyId));
  if (family == "array" || family == "vector" || family == "Buffer" ||
      family == "soa" "_vector") {
    infoOut.arrayVectorInfo.isArrayOrVectorTarget = true;
    infoOut.arrayVectorInfo.isVectorTarget = family == "vector";
    infoOut.arrayVectorInfo.isSoaVector = family == "soa" "_vector";
    infoOut.arrayVectorInfo.elemKind =
        valueKindFromTypeName(resolveSemanticProductTypeText(
            semanticProgram, fact.elementTypeText, fact.elementTypeTextId));
    return true;
  }
  if (family == "map") {
    infoOut.keyValueInfo.isKeyValueTarget = true;
    infoOut.keyValueInfo.keyValueKeyKind =
        valueKindFromTypeName(resolveSemanticProductTypeText(
            semanticProgram, fact.keyTypeText, fact.keyTypeTextId));
    infoOut.keyValueInfo.keyValueValueKind =
        valueKindFromTypeName(resolveSemanticProductTypeText(
            semanticProgram, fact.valueTypeText, fact.valueTypeTextId));
    return true;
  }
  return true;
}

bool resolveSemanticReturnKindTargetInfo(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    SemanticReturnKindTargetInfo &infoOut) {
  infoOut = {};
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      target.semanticNodeId == 0) {
    return false;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, target);
      collectionFact != nullptr) {
    return classifySemanticReturnKindCollectionSpecialization(
        semanticProgram, *collectionFact, infoOut);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, target);
      bindingFact != nullptr) {
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       bindingFact->bindingTypeText,
                                       bindingFact->bindingTypeTextId),
        infoOut);
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, target);
      localAutoFact != nullptr) {
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       localAutoFact->bindingTypeText,
                                       localAutoFact->bindingTypeTextId),
        infoOut);
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFactBySemanticId(*semanticIndex, target);
      queryFact != nullptr) {
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       queryFact->queryTypeText,
                                       queryFact->queryTypeTextId),
        infoOut);
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       queryFact->bindingTypeText,
                                       queryFact->bindingTypeTextId),
        infoOut);
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       queryFact->receiverBindingTypeText,
                                       queryFact->receiverBindingTypeTextId),
        infoOut);
    return true;
  }
  return false;
}

} // namespace

bool resolveReturnInfoKindForPath(const std::string &path,
                                  const GetReturnInfoForPathFn &getReturnInfo,
                                  bool requireArrayReturn,
                                  LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (!getReturnInfo) {
    return false;
  }

  ReturnInfo info;
  if (!getReturnInfo(path, info) || info.returnsVoid) {
    return false;
  }

  if (requireArrayReturn) {
    if (!info.returnsArray) {
      return false;
    }
  } else if (info.returnsArray) {
    return false;
  }

  kindOut = info.kind;
  return true;
}

bool resolveMethodCallReturnKind(const Expr &methodCallExpr,
                                 const LocalMap &localsIn,
                                 const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                 const ResolveDefinitionCallFn &resolveDefinitionCall,
                                 const GetReturnInfoForPathFn &getReturnInfo,
                                 bool requireArrayReturn,
                                 LocalInfo::ValueKind &kindOut,
                                 bool *methodResolvedOut,
                                 const SemanticProgram *semanticProgram,
                                 const SemanticProductIndex *semanticIndex) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }
  if (!resolveMethodCallDefinition) {
    return false;
  }

  const Definition *callee = resolveMethodCallDefinition(methodCallExpr, localsIn);
  if (callee != nullptr &&
      methodCallExpr.isMethodCall &&
      !requireArrayReturn &&
      methodCallExpr.args.size() == 2 &&
      (isSimpleCallName(methodCallExpr, "at") ||
       isSimpleCallName(methodCallExpr, "at_unsafe"))) {
    auto definitionReturnsString = [](const Definition &definition) {
      for (const auto &transform : definition.transforms) {
        if (transform.name == "return" &&
            transform.templateArgs.size() == 1 &&
            normalizeCollectionBindingTypeName(transform.templateArgs.front()) ==
                "string") {
          return true;
        }
      }
      return false;
    };
    if (definitionReturnsString(*callee)) {
      kindOut = LocalInfo::ValueKind::String;
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    LocalInfo::ValueKind declaredReturnKind = LocalInfo::ValueKind::Unknown;
    if (resolveReturnInfoKindForPath(
            callee->fullPath, getReturnInfo, false, declaredReturnKind) &&
        declaredReturnKind == LocalInfo::ValueKind::String) {
      kindOut = declaredReturnKind;
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    const Expr &receiverExpr = methodCallExpr.args.front();
    SemanticReturnKindTargetInfo semanticInfo;
    const bool hasSemanticReceiverFact = resolveSemanticReturnKindTargetInfo(
        receiverExpr, semanticProgram, semanticIndex, semanticInfo);
    if (hasSemanticReceiverFact) {
      if (semanticInfo.arrayVectorInfo.isArrayOrVectorTarget &&
          semanticInfo.arrayVectorInfo.elemKind != LocalInfo::ValueKind::Unknown) {
        kindOut = semanticInfo.arrayVectorInfo.elemKind;
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        return true;
      }
      if (semanticInfo.keyValueInfo.isKeyValueTarget &&
          semanticInfo.keyValueInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
        kindOut = semanticInfo.keyValueInfo.keyValueValueKind;
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        return true;
      }
    } else {
      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(receiverExpr.name);
        if (localIt != localsIn.end()) {
          const LocalInfo &receiverInfo = localIt->second;
          const bool isVectorLikeReceiver =
              receiverInfo.kind == LocalInfo::Kind::Vector ||
              receiverInfo.kind == LocalInfo::Kind::Array ||
              (receiverInfo.kind == LocalInfo::Kind::Reference &&
               (receiverInfo.referenceToVector || receiverInfo.referenceToArray)) ||
              (receiverInfo.kind == LocalInfo::Kind::Pointer &&
               (receiverInfo.pointerToVector || receiverInfo.pointerToArray));
          if (isVectorLikeReceiver &&
              receiverInfo.valueKind != LocalInfo::ValueKind::Unknown) {
            kindOut = receiverInfo.valueKind;
            if (methodResolvedOut != nullptr) {
              *methodResolvedOut = true;
            }
            return true;
          }
        }
      }
      if (receiverExpr.kind == Expr::Kind::Call) {
        std::string collectionName;
        if (getBuiltinCollectionName(receiverExpr, collectionName) &&
            (collectionName == "vector" || collectionName == "array") &&
            receiverExpr.templateArgs.size() == 1) {
          const LocalInfo::ValueKind elemKind =
              valueKindFromTypeName(receiverExpr.templateArgs.front());
          if (elemKind != LocalInfo::ValueKind::Unknown) {
            kindOut = elemKind;
            if (methodResolvedOut != nullptr) {
              *methodResolvedOut = true;
            }
            return true;
          }
        }
      }
    }
  }
  if (callee == nullptr) {
    auto normalizeMethodName = [](std::string name) {
      if (!name.empty() && name.front() == '/') {
        name.erase(name.begin());
      }
      return name;
    };
    auto isBuiltinAccessMethodName = [](const std::string &name) {
      return name == "at" || name == "at_unsafe";
    };
    auto isBuiltinCountMethodName = [](const std::string &name) {
      return name == "count";
    };
    auto isBuiltinCapacityMethodName = [](const std::string &name) {
      return name == "capacity";
    };
    auto isBuiltinContainsMethodName = [](const std::string &name) {
      return name == "contains";
    };
    auto isBuiltinTryAtMethodName = [](const std::string &name) {
      return name == "tryAt";
    };
    auto resolveCanonicalMapAccessStringOverride =
        [&](const std::string &normalizedName,
            LocalInfo::ValueKind &overrideKindOut) {
          overrideKindOut = LocalInfo::ValueKind::Unknown;
          if (normalizedName != "at" && normalizedName != "at_unsafe") {
            return false;
          }
          const std::string canonicalAccessPath =
              canonicalKeyValueHelperPathForSetupReturnKind(normalizedName);
          ReturnInfo info;
          if (!getReturnInfo || canonicalAccessPath.empty() ||
              !getReturnInfo(canonicalAccessPath, info) ||
              info.returnsVoid || info.returnsArray ||
              info.kind != LocalInfo::ValueKind::String) {
            return false;
          }
          overrideKindOut = info.kind;
          return true;
        };
    auto isBareVectorAccessMethodExpr = [&](const Expr &expr) {
      if (!expr.isMethodCall || expr.args.size() != 2 ||
          !(isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe"))) {
        return false;
      }

      const Expr &receiverExpr = expr.args.front();
      SemanticReturnKindTargetInfo semanticInfo;
      if (resolveSemanticReturnKindTargetInfo(
              receiverExpr, semanticProgram, semanticIndex, semanticInfo)) {
        return semanticInfo.arrayVectorInfo.isVectorTarget;
      }
      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(receiverExpr.name);
        if (localIt == localsIn.end()) {
          return false;
        }
        const LocalInfo &receiverInfo = localIt->second;
        return receiverInfo.kind == LocalInfo::Kind::Vector ||
               (receiverInfo.kind == LocalInfo::Kind::Reference && receiverInfo.referenceToVector) ||
               (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToVector);
      }

      if (receiverExpr.kind == Expr::Kind::Call) {
        std::string collectionName;
        if (getBuiltinCollectionName(receiverExpr, collectionName)) {
          return collectionName == "vector" && receiverExpr.templateArgs.size() == 1;
        }

        const Definition *receiverDef = receiverExpr.isMethodCall
                                            ? resolveMethodCallDefinition(receiverExpr, localsIn)
                                            : (resolveDefinitionCall ? resolveDefinitionCall(receiverExpr) : nullptr);
        if (receiverDef == nullptr) {
          return false;
        }
        std::string receiverTypeName;
        return inferReceiverTypeFromDeclaredReturn(*receiverDef, receiverTypeName) &&
               receiverTypeName == "vector";
      }

      return false;
    };
    auto resolveBuiltinMethodReceiverKind = [&](const Expr &receiverExpr,
                                                const std::string &normalizedName,
                                                LocalInfo::ValueKind &builtinKindOut) {
      builtinKindOut = LocalInfo::ValueKind::Unknown;
      if (!methodCallExpr.isMethodCall || requireArrayReturn) {
        return false;
      }
      auto inferDeclaredReceiverCallKind = [&](LocalInfo::ValueKind &declaredKindOut) {
        declaredKindOut = LocalInfo::ValueKind::Unknown;
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        const Definition *receiverDef = receiverExpr.isMethodCall
                                            ? resolveMethodCallDefinition(receiverExpr, localsIn)
                                            : (resolveDefinitionCall ? resolveDefinitionCall(receiverExpr) : nullptr);
        if (receiverDef == nullptr) {
          return false;
        }
        std::string collectionName;
        std::vector<std::string> collectionArgs;
        if (!inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs)) {
          return false;
        }
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa" "_vector") &&
            collectionArgs.size() == 1) {
          declaredKindOut = valueKindFromTypeName(collectionArgs.front());
          return declaredKindOut != LocalInfo::ValueKind::Unknown;
        }
        if (collectionName == "map" && collectionArgs.size() == 2) {
          declaredKindOut = valueKindFromTypeName(collectionArgs.back());
          return declaredKindOut != LocalInfo::ValueKind::Unknown;
        }
        return false;
      };

      SemanticReturnKindTargetInfo semanticInfo;
      const bool hasSemanticReceiverFact = resolveSemanticReturnKindTargetInfo(
          receiverExpr, semanticProgram, semanticIndex, semanticInfo);
      if (isBuiltinAccessMethodName(normalizedName)) {
        if (hasSemanticReceiverFact) {
          if (semanticInfo.arrayVectorInfo.isArrayOrVectorTarget &&
              semanticInfo.arrayVectorInfo.elemKind != LocalInfo::ValueKind::Unknown) {
            builtinKindOut = semanticInfo.arrayVectorInfo.elemKind;
            return true;
          }
          if (semanticInfo.keyValueInfo.isKeyValueTarget &&
              semanticInfo.keyValueInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
            builtinKindOut = semanticInfo.keyValueInfo.keyValueValueKind;
            return true;
          }
          return false;
        }
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget &&
            arrayVectorTargetInfo.elemKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = arrayVectorTargetInfo.elemKind;
          return true;
        }
        const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(receiverExpr, localsIn);
        if (keyValueTargetInfo.isKeyValueTarget && keyValueTargetInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = keyValueTargetInfo.keyValueValueKind;
          return true;
        }
        return inferDeclaredReceiverCallKind(builtinKindOut);
      }

      if (isBuiltinCountMethodName(normalizedName) || isBuiltinCapacityMethodName(normalizedName)) {
        if (hasSemanticReceiverFact) {
          if (semanticInfo.arrayVectorInfo.isArrayOrVectorTarget ||
              semanticInfo.keyValueInfo.isKeyValueTarget) {
            builtinKindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
          return false;
        }
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
        const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(receiverExpr, localsIn);
        if (keyValueTargetInfo.isKeyValueTarget) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
        LocalInfo::ValueKind declaredReceiverKind = LocalInfo::ValueKind::Unknown;
        if (inferDeclaredReceiverCallKind(declaredReceiverKind)) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }

      if (isBuiltinContainsMethodName(normalizedName)) {
        if (hasSemanticReceiverFact) {
          if (semanticInfo.keyValueInfo.isKeyValueTarget) {
            builtinKindOut = LocalInfo::ValueKind::Bool;
            return true;
          }
          return false;
        }
        const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(receiverExpr, localsIn);
        if (keyValueTargetInfo.isKeyValueTarget) {
          builtinKindOut = LocalInfo::ValueKind::Bool;
          return true;
        }
        LocalInfo::ValueKind declaredReceiverKind = LocalInfo::ValueKind::Unknown;
        if (inferDeclaredReceiverCallKind(declaredReceiverKind)) {
          builtinKindOut = LocalInfo::ValueKind::Bool;
          return true;
        }
      }

      if (isBuiltinTryAtMethodName(normalizedName)) {
        if (hasSemanticReceiverFact) {
          if (semanticInfo.keyValueInfo.isKeyValueTarget &&
              semanticInfo.keyValueInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
            builtinKindOut = semanticInfo.keyValueInfo.keyValueValueKind;
            return true;
          }
          return false;
        }
        const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(receiverExpr, localsIn);
        if (keyValueTargetInfo.isKeyValueTarget && keyValueTargetInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = keyValueTargetInfo.keyValueValueKind;
          return true;
        }
        return inferDeclaredReceiverCallKind(builtinKindOut);
      }

      return false;
    };

    if (!isBareVectorAccessMethodExpr(methodCallExpr) && !methodCallExpr.args.empty() &&
        resolveBuiltinMethodReceiverKind(
            methodCallExpr.args.front(), normalizeMethodName(methodCallExpr.name), kindOut)) {
      LocalInfo::ValueKind overrideKind = LocalInfo::ValueKind::Unknown;
      if (resolveCanonicalMapAccessStringOverride(
              normalizeMethodName(methodCallExpr.name), overrideKind)) {
        kindOut = overrideKind;
      }
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    return false;
  }

  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = true;
  }
  return resolveReturnInfoKindForPath(callee->fullPath, getReturnInfo, requireArrayReturn, kindOut);
}

bool resolveDefinitionCallReturnKind(const Expr &callExpr,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const ResolveReceiverExprPathFn &resolveExprPath,
                                     const GetReturnInfoForPathFn &getReturnInfo,
                                     bool requireArrayReturn,
                                     LocalInfo::ValueKind &kindOut,
                                     bool *definitionMatchedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (definitionMatchedOut != nullptr) {
    *definitionMatchedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }

  std::vector<std::string> candidates;
  auto appendCandidate = [&](const std::string &candidate) {
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
  auto appendCandidates = [&](const std::vector<std::string> &extraCandidates) {
    for (const auto &candidate : extraCandidates) {
      appendCandidate(candidate);
    }
  };
  const std::string scopedCallPath = resolveScopedCallPath(callExpr);
  if (prefersExactDirectMapCountLikeReturnPath(callExpr)) {
    appendCandidates(collectionHelperPathCandidates(scopedCallPath));
    appendCandidates(collectionHelperPathCandidates(resolveExprPath(callExpr)));
  } else {
    appendCandidates(collectionHelperPathCandidates(resolveExprPath(callExpr)));
    appendCandidates(collectionHelperPathCandidates(scopedCallPath));
  }
  auto resolveCandidatePath = [&](const std::string &candidate,
                                  std::string &resolvedPathOut) {
    resolvedPathOut.clear();
    auto defIt = defMap.find(candidate);
    if (defIt != defMap.end()) {
      resolvedPathOut = candidate;
      return true;
    }
    const std::string generatedPrefix = candidate + "__";
    for (const auto &entry : defMap) {
      if (entry.first.rfind(generatedPrefix, 0) == 0) {
        resolvedPathOut = entry.first;
        return true;
      }
    }
    return false;
  };
  bool matchedDefinition = false;
  for (const auto &candidate : candidates) {
    std::string resolvedCandidatePath;
    if (!resolveCandidatePath(candidate, resolvedCandidatePath)) {
      continue;
    }
    matchedDefinition = true;
    if (resolveReturnInfoKindForPath(resolvedCandidatePath, getReturnInfo, requireArrayReturn, kindOut)) {
      if (definitionMatchedOut != nullptr) {
        *definitionMatchedOut = true;
      }
      return true;
    }
  }

  if (definitionMatchedOut != nullptr) {
    *definitionMatchedOut = matchedDefinition;
  }
  return false;
}

bool resolveCountMethodCallReturnKind(const Expr &callExpr,
                                      const LocalMap &localsIn,
                                      const IsMethodCallClassifierFn &isArrayCountCall,
                                      const IsMethodCallClassifierFn &isStringCountCall,
                                      const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                      const GetReturnInfoForPathFn &getReturnInfo,
                                      bool requireArrayReturn,
                                      LocalInfo::ValueKind &kindOut,
                                      bool *methodResolvedOut,
                                      const InferReceiverExprKindFn &inferExprKind,
                                      const SemanticProgram *semanticProgram,
                                      const SemanticProductIndex *semanticIndex) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const std::string scopedCallPath = resolveScopedCallPath(callExpr);
  if (isExplicitRemovedVectorMethodAliasPath(scopedCallPath)) {
    return false;
  }
  if (isExplicitKeyValueHelperFallbackPath(callExpr)) {
    std::string explicitAccessName;
    if (!getBuiltinArrayAccessName(callExpr, explicitAccessName) ||
        (explicitAccessName != "at" && explicitAccessName != "at_ref" &&
         explicitAccessName != "at_unsafe" &&
         explicitAccessName != "at_unsafe_ref")) {
      return false;
    }
  }
  const bool isCountCall =
      isUnqualifiedCollectionBuiltinName(callExpr, "count") &&
      callExpr.args.size() == 1;
  const bool isContainsCall = isSimpleCallName(callExpr, "contains") && callExpr.args.size() == 2;
  std::string accessName;
  const bool isCollectionAccessCall = getBuiltinArrayAccessName(callExpr, accessName);
  const bool isAccessCall = (isCollectionAccessCall || isSimpleCallName(callExpr, "get") ||
                             isSimpleCallName(callExpr, "ref")) &&
                            callExpr.args.size() == 2;
  const bool isVectorMutatorCall =
      isUnqualifiedCollectionBuiltinName(callExpr, "push") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "pop") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "reserve") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "clear") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "remove_at") ||
      isUnqualifiedCollectionBuiltinName(callExpr, "remove_swap");
  auto expectedVectorMutatorArgCount = [&]() -> size_t {
    if (isUnqualifiedCollectionBuiltinName(callExpr, "pop") ||
        isUnqualifiedCollectionBuiltinName(callExpr, "clear")) {
      return 1u;
    }
    return 2u;
  };
  size_t expectedArgCount = 1u;
  if (isAccessCall || isContainsCall) {
    expectedArgCount = 2u;
  } else if (isVectorMutatorCall) {
    expectedArgCount = expectedVectorMutatorArgCount();
  }
  const bool isSoaFieldHelperCall =
      callExpr.args.size() == 1 && !isCountCall && isSoaVectorReceiverExpr(callExpr.args.front(), localsIn);
  if ((!isCountCall && !isContainsCall && !isAccessCall && !isSoaFieldHelperCall && !isVectorMutatorCall) ||
      callExpr.args.size() != expectedArgCount) {
    return false;
  }
  (void)isArrayCountCall;
  (void)isStringCountCall;

  auto hasNamedArgs = [&]() {
    for (const auto &argName : callExpr.argNames) {
      if (argName.has_value()) {
        return true;
      }
    }
    return false;
  };
  auto resolveSemanticArrayVectorTargetInfo =
      [&](const Expr &candidate, ArrayVectorAccessTargetInfo &infoOut) {
        SemanticReturnKindTargetInfo semanticInfo;
        if (!resolveSemanticReturnKindTargetInfo(
                candidate, semanticProgram, semanticIndex, semanticInfo)) {
          return false;
        }
        infoOut = semanticInfo.arrayVectorInfo;
        return true;
      };
  auto resolveSemanticKeyValueTargetInfo =
      [&](const Expr &candidate, CollectionPairTypeInfo &infoOut) {
        SemanticReturnKindTargetInfo semanticInfo;
        if (!resolveSemanticReturnKindTargetInfo(
                candidate, semanticProgram, semanticIndex, semanticInfo)) {
          return false;
        }
        infoOut = semanticInfo.keyValueInfo;
        return true;
      };
  auto arrayVectorTargetInfoFor = [&](const Expr &candidate) {
    return resolveArrayVectorAccessTargetInfo(
        candidate, localsIn, resolveSemanticArrayVectorTargetInfo);
  };
  auto keyValueTargetInfoFor = [&](const Expr &candidate) {
    return resolveCollectionPairTypeInfo(
        candidate, localsIn, resolveSemanticKeyValueTargetInfo);
  };

  auto isKnownCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (keyValueTargetInfoFor(candidate).isKeyValueTarget) {
      return true;
    }
    if (arrayVectorTargetInfoFor(candidate).isArrayOrVectorTarget) {
      return true;
    }
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    SemanticReturnKindTargetInfo semanticInfo;
    if (resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return semanticInfo.valueKind == LocalInfo::ValueKind::String;
    }
    const LocalInfo::ValueKind inferredKind =
        inferExprKind ? inferExprKind(candidate, localsIn) : LocalInfo::ValueKind::Unknown;
    if (inferredKind == LocalInfo::ValueKind::String) {
      return true;
    }
    if (inferredKind != LocalInfo::ValueKind::Unknown) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
           it->second.valueKind == LocalInfo::ValueKind::String;
  };
  auto isKnownKeyValueReceiverExpr = [&](const Expr &candidate) -> bool {
    return keyValueTargetInfoFor(candidate).isKeyValueTarget;
  };
  auto hasNonKeyValueReceiverSemanticFact = [&](const Expr &candidate) {
    SemanticReturnKindTargetInfo semanticInfo;
    if (!resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return false;
    }
    return !semanticInfo.keyValueInfo.isKeyValueTarget;
  };
  auto hasNonCountReceiverSemanticFact = [&](const Expr &candidate) {
    SemanticReturnKindTargetInfo semanticInfo;
    if (!resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return false;
    }
    return !semanticInfo.arrayVectorInfo.isArrayOrVectorTarget &&
           !semanticInfo.keyValueInfo.isKeyValueTarget &&
           semanticInfo.valueKind != LocalInfo::ValueKind::String;
  };
  auto isKnownVectorMutatorReceiverExpr = [&](const Expr &candidate) -> bool {
    SemanticReturnKindTargetInfo semanticInfo;
    if (resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return semanticInfo.arrayVectorInfo.isVectorTarget ||
             semanticInfo.arrayVectorInfo.isSoaVector;
    }
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    if (it == localsIn.end()) {
      return false;
    }
    const LocalInfo &info = it->second;
    return info.kind == LocalInfo::Kind::Vector || info.isSoaVector ||
           (info.kind == LocalInfo::Kind::Pointer && info.pointerToVector);
  };
  auto hasNonVectorMutatorReceiverSemanticFact = [&](const Expr &candidate) {
    SemanticReturnKindTargetInfo semanticInfo;
    if (!resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return false;
    }
    return !semanticInfo.arrayVectorInfo.isVectorTarget &&
           !semanticInfo.arrayVectorInfo.isSoaVector;
  };
  auto hasNonCollectionAccessReceiverSemanticFact = [&](const Expr &candidate) {
    SemanticReturnKindTargetInfo semanticInfo;
    if (!resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return false;
    }
    return !semanticInfo.arrayVectorInfo.isArrayOrVectorTarget &&
           !semanticInfo.keyValueInfo.isKeyValueTarget &&
           semanticInfo.valueKind != LocalInfo::ValueKind::String;
  };
  auto isKnownLocalName = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    return localsIn.find(candidate.name) != localsIn.end();
  };
  auto isStringAccessReceiverExpr = [&](const Expr &candidate) {
    if (requireArrayReturn) {
      return false;
    }
    SemanticReturnKindTargetInfo semanticInfo;
    if (resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return semanticInfo.valueKind == LocalInfo::ValueKind::String;
    }
    return inferExprKind &&
           inferExprKind(candidate, localsIn) == LocalInfo::ValueKind::String;
  };
  auto isStringCountReceiverExpr = [&](const Expr &candidate) {
    if (requireArrayReturn) {
      return false;
    }
    SemanticReturnKindTargetInfo semanticInfo;
    if (resolveSemanticReturnKindTargetInfo(
            candidate, semanticProgram, semanticIndex, semanticInfo)) {
      return semanticInfo.valueKind == LocalInfo::ValueKind::String;
    }
    return inferExprKind &&
           inferExprKind(candidate, localsIn) == LocalInfo::ValueKind::String;
  };
  auto buildMethodExprForReceiverIndex = [&](size_t receiverIndex) {
    Expr methodExpr = callExpr;
    methodExpr.isMethodCall = true;
    methodExpr.semanticNodeId = 0;
    std::string normalizedHelperName;
    if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    } else if (resolveBorrowedKeyValueHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    } else if (resolveKeyValueHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    }
    if (receiverIndex != 0 && receiverIndex < methodExpr.args.size()) {
      std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
      if (methodExpr.argNames.size() < methodExpr.args.size()) {
        methodExpr.argNames.resize(methodExpr.args.size());
      }
      std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
    }
    return methodExpr;
  };

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= callExpr.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };
  const bool hasNamedArgsValue = hasNamedArgs();
  if (hasNamedArgsValue) {
    bool hasValuesNamedReceiver = false;
    if (isVectorMutatorCall || isAccessCall || isContainsCall) {
      for (size_t i = 0; i < callExpr.args.size(); ++i) {
        if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
            *callExpr.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < callExpr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }
  const bool probePositionalReorderedVectorMutatorReceiver =
      isVectorMutatorCall && !hasNamedArgsValue && callExpr.args.size() > 1 &&
      (callExpr.args.front().kind == Expr::Kind::Literal ||
       callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
       callExpr.args.front().kind == Expr::Kind::FloatLiteral ||
       callExpr.args.front().kind == Expr::Kind::StringLiteral ||
       (callExpr.args.front().kind == Expr::Kind::Name &&
        ((isKnownLocalName(callExpr.args.front()) &&
          !isKnownVectorMutatorReceiverExpr(callExpr.args.front())) ||
         hasNonVectorMutatorReceiverSemanticFact(callExpr.args.front()))));
  if (probePositionalReorderedVectorMutatorReceiver) {
    for (size_t i = 1; i < callExpr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool hasAlternativeVectorMutatorReceiver =
      probePositionalReorderedVectorMutatorReceiver &&
      std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
        return index > 0 && index < callExpr.args.size() &&
               isKnownVectorMutatorReceiverExpr(callExpr.args[index]);
      });
  const bool probePositionalReorderedAccessReceiver =
      (isAccessCall || isContainsCall) && !hasNamedArgsValue && callExpr.args.size() > 1 &&
      (callExpr.args.front().kind == Expr::Kind::Literal || callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
       callExpr.args.front().kind == Expr::Kind::FloatLiteral || callExpr.args.front().kind == Expr::Kind::StringLiteral ||
       (callExpr.args.front().kind == Expr::Kind::Name &&
        ((isKnownLocalName(callExpr.args.front()) &&
          !isKnownCollectionAccessReceiverExpr(callExpr.args.front())) ||
         hasNonCollectionAccessReceiverSemanticFact(callExpr.args.front()))));
  if (probePositionalReorderedAccessReceiver) {
    for (size_t i = 1; i < callExpr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool hasAlternativeCollectionReceiver =
      probePositionalReorderedAccessReceiver &&
      std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
        return index > 0 && index < callExpr.args.size() &&
               isKnownCollectionAccessReceiverExpr(callExpr.args[index]);
      });
  const bool preferDeclaredAccessReturnKind =
      isAccessCall && isExplicitKeyValueHelperFallbackPath(callExpr);
  if (preferDeclaredAccessReturnKind) {
    std::vector<std::string> candidatePaths = {scopedCallPath};
    for (const auto &candidatePath : candidatePaths) {
      if (resolveReturnInfoKindForPath(
              candidatePath, getReturnInfo, requireArrayReturn, kindOut)) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        return true;
      }
    }
  }

  for (size_t receiverIndex : receiverIndices) {
    if ((hasAlternativeCollectionReceiver || hasAlternativeVectorMutatorReceiver) &&
        receiverIndex == 0) {
      continue;
    }
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    if (isVectorMutatorCall) {
      SemanticReturnKindTargetInfo semanticInfo;
      if (resolveSemanticReturnKindTargetInfo(
              methodExpr.args.front(), semanticProgram, semanticIndex, semanticInfo) &&
          !semanticInfo.arrayVectorInfo.isVectorTarget &&
          !semanticInfo.arrayVectorInfo.isSoaVector) {
        continue;
      }
    }
    if (isAccessCall && !preferDeclaredAccessReturnKind) {
      const auto arrayVectorTargetInfo =
          arrayVectorTargetInfoFor(methodExpr.args.front());
      if (arrayVectorTargetInfo.isArrayOrVectorTarget &&
          arrayVectorTargetInfo.elemKind != LocalInfo::ValueKind::Unknown) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = arrayVectorTargetInfo.elemKind;
        return true;
      }

      const auto keyValueTargetInfo = keyValueTargetInfoFor(methodExpr.args.front());
      if (keyValueTargetInfo.isKeyValueTarget &&
          keyValueTargetInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = keyValueTargetInfo.keyValueValueKind;
        return true;
      }
      if (isStringAccessReceiverExpr(methodExpr.args.front())) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
      if (hasNonCollectionAccessReceiverSemanticFact(methodExpr.args.front())) {
        continue;
      }
    }
    if (isContainsCall && isKnownKeyValueReceiverExpr(methodExpr.args.front())) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    }
    if (isContainsCall && hasNonKeyValueReceiverSemanticFact(methodExpr.args.front())) {
      continue;
    }
    if (isCountCall && !requireArrayReturn) {
      const auto arrayVectorTargetInfo =
          arrayVectorTargetInfoFor(methodExpr.args.front());
      const auto keyValueTargetInfo =
          keyValueTargetInfoFor(methodExpr.args.front());
      if (arrayVectorTargetInfo.isArrayOrVectorTarget || keyValueTargetInfo.isKeyValueTarget) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
      if (isStringCountReceiverExpr(methodExpr.args.front())) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
      if (hasNonCountReceiverSemanticFact(methodExpr.args.front())) {
        continue;
      }
    }
    if (!resolveMethodCallDefinition) {
      continue;
    }
    const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
    if (callee == nullptr || !isAllowedResolvedVectorDirectCallPath(scopedCallPath, callee->fullPath) ||
        !isAllowedResolvedMapDirectCallPath(scopedCallPath, callee->fullPath)) {
      continue;
    }
    if (resolveReturnInfoKindForPath(callee->fullPath, getReturnInfo, requireArrayReturn, kindOut)) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    if (methodResolvedOut != nullptr) {
      *methodResolvedOut = true;
    }
    if (isAccessCall && isStringAccessReceiverExpr(methodExpr.args.front())) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Int32;
      return true;
    }
    if (isCountCall && isStringCountReceiverExpr(methodExpr.args.front())) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Int32;
      return true;
    }
    return false;
  }

  if (isCountCall && isStringCountReceiverExpr(callExpr.args.front())) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (isAccessCall && isStringAccessReceiverExpr(callExpr.args.front())) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (isContainsCall && isKnownKeyValueReceiverExpr(callExpr.args.front())) {
    kindOut = LocalInfo::ValueKind::Bool;
    return true;
  }

  return false;
}

bool resolveCapacityMethodCallReturnKind(const Expr &callExpr,
                                         const LocalMap &localsIn,
                                         const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                         const GetReturnInfoForPathFn &getReturnInfo,
                                         bool requireArrayReturn,
                                         LocalInfo::ValueKind &kindOut,
                                         bool *methodResolvedOut,
                                         const SemanticProgram *semanticProgram,
                                         const SemanticProductIndex *semanticIndex) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  const std::string scopedCallPath = resolveScopedCallPath(callExpr);
  if (isExplicitRemovedVectorMethodAliasPath(scopedCallPath)) {
    return false;
  }
  if (!isUnqualifiedCollectionBuiltinName(callExpr, "capacity") ||
      callExpr.args.size() != 1) {
    return false;
  }

  Expr methodExpr = callExpr;
  methodExpr.isMethodCall = true;
  std::string normalizedHelperName;
  if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
    methodExpr.name = normalizedHelperName;
  }
  return resolveMethodCallReturnKind(methodExpr,
                                     localsIn,
                                     resolveMethodCallDefinition,
                                     {},
                                     getReturnInfo,
                                     requireArrayReturn,
                                     kindOut,
                                     methodResolvedOut,
                                     semanticProgram,
                                     semanticIndex);
}

} // namespace primec::ir_lowerer
