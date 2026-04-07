#include "IrLowererSetupTypeHelpers.h"

#include <algorithm>

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"

namespace primec::ir_lowerer {

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
                                 bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }
  if (!resolveMethodCallDefinition) {
    return false;
  }

  const Definition *callee = resolveMethodCallDefinition(methodCallExpr, localsIn);
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
    auto isBareVectorAccessMethodExpr = [&](const Expr &expr) {
      if (!expr.isMethodCall || expr.args.size() != 2 ||
          !(isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe"))) {
        return false;
      }

      const Expr &receiverExpr = expr.args.front();
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
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
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

      if (isBuiltinAccessMethodName(normalizedName)) {
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget &&
            arrayVectorTargetInfo.elemKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = arrayVectorTargetInfo.elemKind;
          return true;
        }
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget && mapTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = mapTargetInfo.mapValueKind;
          return true;
        }
        return inferDeclaredReceiverCallKind(builtinKindOut);
      }

      if (isBuiltinCountMethodName(normalizedName) || isBuiltinCapacityMethodName(normalizedName)) {
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
          builtinKindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget) {
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
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget) {
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
        const auto mapTargetInfo = resolveMapAccessTargetInfo(receiverExpr, localsIn);
        if (mapTargetInfo.isMapTarget && mapTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
          builtinKindOut = mapTargetInfo.mapValueKind;
          return true;
        }
        return inferDeclaredReceiverCallKind(builtinKindOut);
      }

      return false;
    };

    auto resolveExplicitRemovedBuiltinAccessMethodReturnKind = [&](LocalInfo::ValueKind &builtinKindOut) {
      builtinKindOut = LocalInfo::ValueKind::Unknown;
      if (!methodCallExpr.isMethodCall || requireArrayReturn || methodCallExpr.args.size() != 2) {
        return false;
      }

      std::string normalizedName = methodCallExpr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      if (normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
          normalizedName == "std/collections/vector/at" ||
          normalizedName == "std/collections/vector/at_unsafe" ||
          normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
          normalizedName == "std/collections/map/at" ||
          normalizedName == "std/collections/map/at_unsafe") {
        return false;
      }
      const Expr &receiverExpr = methodCallExpr.args.front();
      auto assignKnownElementKind = [&](LocalInfo::ValueKind valueKind) {
        if (valueKind == LocalInfo::ValueKind::Unknown) {
          return false;
        }
        builtinKindOut = valueKind;
        return true;
      };

      auto assignMapValueKind = [&](const LocalInfo &receiverInfo) {
        if (receiverInfo.kind == LocalInfo::Kind::Map ||
            (receiverInfo.kind == LocalInfo::Kind::Reference && receiverInfo.referenceToMap) ||
            (receiverInfo.kind == LocalInfo::Kind::Pointer && receiverInfo.pointerToMap)) {
          return assignKnownElementKind(receiverInfo.mapValueKind);
        }
        return false;
      };
      auto assignDeclaredReceiverCallKind = [&](const Definition &receiverDef) {
        std::string collectionName;
        std::vector<std::string> collectionArgs;
        if (inferDeclaredReturnCollection(receiverDef, collectionName, collectionArgs)) {
          if ((normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
               normalizedName == "std/collections/vector/at" ||
               normalizedName == "std/collections/vector/at_unsafe") &&
              (collectionName == "vector" || collectionName == "array") && collectionArgs.size() == 1) {
            return assignKnownElementKind(valueKindFromTypeName(collectionArgs.front()));
          }
          if ((normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
               normalizedName == "std/collections/map/at" ||
               normalizedName == "std/collections/map/at_unsafe") &&
              collectionName == "map" && collectionArgs.size() == 2) {
            return assignKnownElementKind(valueKindFromTypeName(collectionArgs[1]));
          }
        }

        ReturnInfo receiverInfo;
        if (getReturnInfo && getReturnInfo(receiverDef.fullPath, receiverInfo) && !receiverInfo.returnsVoid &&
            !receiverInfo.returnsArray &&
            (normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
             normalizedName == "std/collections/vector/at" ||
             normalizedName == "std/collections/vector/at_unsafe") &&
            receiverInfo.kind == LocalInfo::ValueKind::String) {
          return assignKnownElementKind(LocalInfo::ValueKind::Int32);
        }
        return false;
      };

      if (receiverExpr.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(receiverExpr.name);
        if (localIt == localsIn.end()) {
          return false;
        }
        const LocalInfo &receiverInfo = localIt->second;
        if (normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
            normalizedName == "std/collections/vector/at" ||
            normalizedName == "std/collections/vector/at_unsafe") {
          if (receiverInfo.kind == LocalInfo::Kind::Vector || receiverInfo.kind == LocalInfo::Kind::Array ||
              (receiverInfo.kind == LocalInfo::Kind::Reference &&
               (receiverInfo.referenceToArray || receiverInfo.referenceToVector))) {
            return assignKnownElementKind(receiverInfo.valueKind);
          }
          return false;
        }
        if (normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
            normalizedName == "std/collections/map/at" ||
            normalizedName == "std/collections/map/at_unsafe") {
          return assignMapValueKind(receiverInfo);
        }
        return false;
      }

      if (receiverExpr.kind != Expr::Kind::Call) {
        return false;
      }
      std::string collectionName;
      if (!getBuiltinCollectionName(receiverExpr, collectionName)) {
        return false;
      }
      if ((normalizedName == "vector/at" || normalizedName == "vector/at_unsafe" ||
           normalizedName == "std/collections/vector/at" ||
           normalizedName == "std/collections/vector/at_unsafe") &&
          (collectionName == "vector" || collectionName == "array") && receiverExpr.templateArgs.size() == 1) {
        return assignKnownElementKind(valueKindFromTypeName(receiverExpr.templateArgs.front()));
      }
      if ((normalizedName == "map/at" || normalizedName == "map/at_unsafe" ||
           normalizedName == "std/collections/map/at" ||
           normalizedName == "std/collections/map/at_unsafe") &&
          collectionName == "map" && receiverExpr.templateArgs.size() == 2) {
        return assignKnownElementKind(valueKindFromTypeName(receiverExpr.templateArgs[1]));
      }
      if (const Definition *receiverDef = resolveMethodCallDefinition(receiverExpr, localsIn)) {
        return assignDeclaredReceiverCallKind(*receiverDef);
      }
      return false;
    };

    if (resolveExplicitRemovedBuiltinAccessMethodReturnKind(kindOut)) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      return true;
    }
    if (!isBareVectorAccessMethodExpr(methodCallExpr) && !methodCallExpr.args.empty() &&
        resolveBuiltinMethodReceiverKind(
            methodCallExpr.args.front(), normalizeMethodName(methodCallExpr.name), kindOut)) {
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

  auto candidates = collectionHelperPathCandidates(resolveExprPath(callExpr));
  bool matchedDefinition = false;
  for (const auto &candidate : candidates) {
    auto defIt = defMap.find(candidate);
    if (defIt == defMap.end()) {
      continue;
    }
    matchedDefinition = true;
    if (resolveReturnInfoKindForPath(candidate, getReturnInfo, requireArrayReturn, kindOut)) {
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
                                      const InferReceiverExprKindFn &inferExprKind) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  if (isExplicitRemovedVectorMethodAliasPath(callExpr.name)) {
    return false;
  }
  if (isExplicitMapHelperFallbackPath(callExpr)) {
    return false;
  }
  const bool isCountCall = (isVectorBuiltinName(callExpr, "count") || isMapBuiltinName(callExpr, "count")) &&
                           callExpr.args.size() == 1;
  const bool isContainsCall = isSimpleCallName(callExpr, "contains") && callExpr.args.size() == 2;
  std::string accessName;
  const bool isCollectionAccessCall = getBuiltinArrayAccessName(callExpr, accessName);
  const bool isAccessCall = (isCollectionAccessCall || isSimpleCallName(callExpr, "get") ||
                             isSimpleCallName(callExpr, "ref")) &&
                            callExpr.args.size() == 2;
  const bool isVectorMutatorCall =
      isVectorBuiltinName(callExpr, "push") || isVectorBuiltinName(callExpr, "pop") ||
      isVectorBuiltinName(callExpr, "reserve") || isVectorBuiltinName(callExpr, "clear") ||
      isVectorBuiltinName(callExpr, "remove_at") || isVectorBuiltinName(callExpr, "remove_swap");
  auto expectedVectorMutatorArgCount = [&]() -> size_t {
    if (isVectorBuiltinName(callExpr, "pop") || isVectorBuiltinName(callExpr, "clear")) {
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
  auto isKnownCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
    if (candidate.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (resolveMapAccessTargetInfo(candidate, localsIn).isMapTarget) {
      return true;
    }
    if (resolveArrayVectorAccessTargetInfo(candidate, localsIn).isArrayOrVectorTarget) {
      return true;
    }
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
           it->second.valueKind == LocalInfo::ValueKind::String;
  };
  auto isKnownMapReceiverExpr = [&](const Expr &candidate) -> bool {
    return resolveMapAccessTargetInfo(candidate, localsIn).isMapTarget;
  };
  auto isKnownVectorMutatorReceiverExpr = [&](const Expr &candidate) -> bool {
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
  auto isKnownLocalName = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    return localsIn.find(candidate.name) != localsIn.end();
  };
  auto isStringAccessReceiverExpr = [&](const Expr &candidate) {
    return !requireArrayReturn && inferExprKind &&
           inferExprKind(candidate, localsIn) == LocalInfo::ValueKind::String;
  };
  auto buildMethodExprForReceiverIndex = [&](size_t receiverIndex) {
    Expr methodExpr = callExpr;
    methodExpr.isMethodCall = true;
    std::string normalizedHelperName;
    if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    } else if (resolveBorrowedMapHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    } else if (resolveMapHelperAliasName(methodExpr, normalizedHelperName)) {
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
        isKnownLocalName(callExpr.args.front()) &&
        !isKnownVectorMutatorReceiverExpr(callExpr.args.front())));
  if (probePositionalReorderedVectorMutatorReceiver) {
    for (size_t i = 1; i < callExpr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool probePositionalReorderedAccessReceiver =
      (isAccessCall || isContainsCall) && !hasNamedArgsValue && callExpr.args.size() > 1 &&
      (callExpr.args.front().kind == Expr::Kind::Literal || callExpr.args.front().kind == Expr::Kind::BoolLiteral ||
       callExpr.args.front().kind == Expr::Kind::FloatLiteral || callExpr.args.front().kind == Expr::Kind::StringLiteral ||
       (callExpr.args.front().kind == Expr::Kind::Name &&
        isKnownLocalName(callExpr.args.front()) &&
        !isKnownCollectionAccessReceiverExpr(callExpr.args.front())));
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

  for (size_t receiverIndex : receiverIndices) {
    if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
      continue;
    }
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    if (isAccessCall) {
      const auto arrayVectorTargetInfo =
          resolveArrayVectorAccessTargetInfo(methodExpr.args.front(), localsIn);
      if (arrayVectorTargetInfo.isArrayOrVectorTarget &&
          arrayVectorTargetInfo.elemKind != LocalInfo::ValueKind::Unknown) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = arrayVectorTargetInfo.elemKind;
        return true;
      }

      const auto mapTargetInfo = resolveMapAccessTargetInfo(methodExpr.args.front(), localsIn);
      if (mapTargetInfo.isMapTarget &&
          mapTargetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
        if (methodResolvedOut != nullptr) {
          *methodResolvedOut = true;
        }
        kindOut = mapTargetInfo.mapValueKind;
        return true;
      }
    }
    if (isContainsCall && isKnownMapReceiverExpr(methodExpr.args.front())) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    }
    if (!resolveMethodCallDefinition) {
      continue;
    }
    const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
    if (callee == nullptr || !isAllowedResolvedVectorDirectCallPath(callExpr.name, callee->fullPath) ||
        !isAllowedResolvedMapDirectCallPath(callExpr.name, callee->fullPath)) {
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
    if (isCountCall && !requireArrayReturn && inferExprKind &&
        inferExprKind(methodExpr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
      if (methodResolvedOut != nullptr) {
        *methodResolvedOut = true;
      }
      kindOut = LocalInfo::ValueKind::Int32;
      return true;
    }
    return false;
  }

  if (isCountCall && !requireArrayReturn && inferExprKind &&
      inferExprKind(callExpr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (isAccessCall && isStringAccessReceiverExpr(callExpr.args.front())) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (isContainsCall && isKnownMapReceiverExpr(callExpr.args.front())) {
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
                                         bool *methodResolvedOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (methodResolvedOut != nullptr) {
    *methodResolvedOut = false;
  }

  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
    return false;
  }
  if (isExplicitRemovedVectorMethodAliasPath(callExpr.name)) {
    return false;
  }
  if (!isVectorBuiltinName(callExpr, "capacity") || callExpr.args.size() != 1) {
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
                                     methodResolvedOut);
}

} // namespace primec::ir_lowerer
