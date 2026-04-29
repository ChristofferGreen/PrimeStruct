#include "SemanticsValidator.h"

namespace primec::semantics {

std::string SemanticsValidator::inferStructReturnPath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  if (!structReturnMemoizationEnabled_) {
    return inferStructReturnPathImpl(expr, params, locals);
  }
  const uint64_t localsRevision = currentLocalBindingMemoRevision(&locals);
  const ExprStructReturnMemoKey memoKey{
      &expr,
      currentDefinitionContext_,
      currentExecutionContext_,
      params.empty() ? nullptr : params.data(),
      params.size(),
      &locals,
      locals.size(),
  };
  if (const auto memoIt = inferStructReturnMemo_.find(memoKey);
      memoIt != inferStructReturnMemo_.end() &&
      memoIt->second.localsRevision == localsRevision) {
    return memoIt->second.structPath;
  }
  std::string inferred = inferStructReturnPathImpl(expr, params, locals);
  inferStructReturnMemo_[memoKey] = ExprStructReturnMemoValue{inferred, localsRevision};
  return inferred;
}

std::string SemanticsValidator::inferStructReturnPathImpl(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  auto isMatrixQuaternionTypePath = [](const std::string &typePath) {
    return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
           typePath == "/std/math/Quat";
  };
  auto matrixDimensionForTypePath = [](const std::string &typePath) -> size_t {
    if (typePath == "/std/math/Mat2") {
      return 2;
    }
    if (typePath == "/std/math/Mat3") {
      return 3;
    }
    if (typePath == "/std/math/Mat4") {
      return 4;
    }
    return 0;
  };
  auto isVectorTypePath = [](const std::string &typePath) {
    return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
  };
  auto vectorDimensionForTypePath = [](const std::string &typePath) -> size_t {
    if (typePath == "/std/math/Vec2") {
      return 2;
    }
    if (typePath == "/std/math/Vec3") {
      return 3;
    }
    if (typePath == "/std/math/Vec4") {
      return 4;
    }
    return 0;
  };
  auto resolveStructReturnCallTarget = [&](const Expr &callExpr) -> std::string {
    std::string resolvedPath = resolveCalleePath(callExpr);
    if (callExpr.kind != Expr::Kind::Call) {
      return resolvedPath;
    }
    if (callExpr.isMethodCall) {
      if (callExpr.args.empty()) {
        return resolvedPath;
      }
      bool isBuiltin = false;
      if (!resolveMethodTarget(params,
                               locals,
                               callExpr.namespacePrefix,
                               callExpr.args.front(),
                               callExpr.name,
                               resolvedPath,
                               isBuiltin)) {
        return resolvedPath;
      }
    }
    return resolveExprConcreteCallPath(params, locals, callExpr, resolvedPath);
  };
  if (expr.kind == Expr::Kind::Call) {
    if (!expr.isMethodCall &&
        (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
        expr.args.size() == 1) {
      BindingInfo storageBinding;
      bool resolvedStorage = false;
      if (resolveUninitializedStorageBinding(params, locals, expr.args.front(), storageBinding, resolvedStorage) &&
          resolvedStorage && storageBinding.typeName == "uninitialized" &&
          !storageBinding.typeTemplateArg.empty()) {
        if (std::string collectionPath =
                inferStructReturnCollectionPath(storageBinding.typeTemplateArg, std::string{});
            !collectionPath.empty()) {
          return collectionPath;
        }
        const std::string structPath =
            resolveInferStructTypePath(unwrapReferencePointerTypeText(storageBinding.typeTemplateArg),
                                       expr.namespacePrefix);
        if (!structPath.empty()) {
          return structPath;
        }
      }
    }
    std::string pointerBuiltin;
    if (getBuiltinPointerName(expr, pointerBuiltin) && pointerBuiltin == "dereference" && expr.args.size() == 1) {
      const std::string pointeeType = inferStructReturnPointerTargetTypeText(expr.args.front(), params, locals);
      if (!pointeeType.empty()) {
        const std::string collectionPath = normalizeCollectionTypePath(pointeeType);
        if (!collectionPath.empty()) {
          return collectionPath;
        }
        std::string unwrappedType = unwrapReferencePointerTypeText(pointeeType);
        std::string structPath = resolveInferStructTypePath(unwrappedType, expr.namespacePrefix);
        if (!structPath.empty()) {
          return structPath;
        }
      }
    }
  }
  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);

  if (expr.isLambda) {
    return "";
  }

  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (std::string collectionPath =
              inferStructReturnCollectionPath(paramBinding->typeName, paramBinding->typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (paramBinding->typeName != "Reference" && paramBinding->typeName != "Pointer") {
        return resolveInferStructTypePath(paramBinding->typeName, expr.namespacePrefix);
      }
      return "";
    }
    auto it = locals.find(expr.name);
    if (it != locals.end()) {
      if (std::string collectionPath =
              inferStructReturnCollectionPath(it->second.typeName, it->second.typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (it->second.typeName != "Reference" && it->second.typeName != "Pointer") {
        return resolveInferStructTypePath(it->second.typeName, expr.namespacePrefix);
      }
    }
    return "";
  }

  if (expr.kind == Expr::Kind::Call) {
    if (isSimpleCallName(expr, "move") && expr.args.size() == 1) {
      return inferStructReturnPath(expr.args.front(), params, locals);
    }
    if (isAssignCall(expr) && expr.args.size() == 2) {
      return inferStructReturnPath(expr.args[1], params, locals);
    }
    if (isPickCall(expr)) {
      return inferPickExprStructReturnPath(expr, params, locals);
    }
    std::string builtinName;
    if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
        expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType) && leftType == rightType) {
        return leftType;
      }
    }
    if (getBuiltinOperatorName(expr, builtinName) && builtinName == "multiply" && expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType)) {
        if (leftType == "/std/math/Quat") {
          if (rightType == "/std/math/Quat") {
            return leftType;
          }
          if (rightType == "/std/math/Vec3") {
            return rightType;
          }
          if (isInferStructReturnNumericScalarExpr(expr.args[1], params, locals)) {
            return leftType;
          }
          return "";
        }
        if (isMatrixQuaternionTypePath(rightType) && leftType == rightType) {
          return leftType;
        }
        if (isVectorTypePath(rightType) &&
            matrixDimensionForTypePath(leftType) == vectorDimensionForTypePath(rightType)) {
          return rightType;
        }
        if (isInferStructReturnNumericScalarExpr(expr.args[1], params, locals)) {
          return leftType;
        }
        return "";
      }
      if (isMatrixQuaternionTypePath(rightType)) {
        if (isInferStructReturnNumericScalarExpr(expr.args[0], params, locals)) {
          return rightType;
        }
        return "";
      }
    }
    if (getBuiltinOperatorName(expr, builtinName) && builtinName == "divide" && expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType) && rightType.empty() &&
          isInferStructReturnNumericScalarExpr(expr.args[1], params, locals)) {
        return leftType;
      }
      if (isMatrixQuaternionTypePath(leftType) || isMatrixQuaternionTypePath(rightType)) {
        return "";
      }
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::string receiverStruct;
      if (expr.args.front().kind == Expr::Kind::Name &&
          findParamBinding(params, expr.args.front().name) == nullptr &&
          locals.find(expr.args.front().name) == locals.end()) {
        receiverStruct =
            resolveInferStructTypePath(expr.args.front().name, expr.args.front().namespacePrefix);
      }
      if (receiverStruct.empty()) {
        receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      }
      if (receiverStruct.empty()) {
        return "";
      }
      auto defIt = defMap_.find(receiverStruct);
      if (defIt == defMap_.end() || !defIt->second) {
        return "";
      }
      const bool isTypeReceiver =
          expr.args.front().kind == Expr::Kind::Name &&
          findParamBinding(params, expr.args.front().name) == nullptr &&
          locals.find(expr.args.front().name) == locals.end();
      for (const auto &stmt : defIt->second->statements) {
        if (!stmt.isBinding || stmt.name != expr.name) {
          continue;
        }
        if (isTypeReceiver ? !isStaticField(stmt) : isStaticField(stmt)) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
          return "";
        }
        std::string fieldType = fieldBinding.typeName;
        if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
          fieldType = fieldBinding.typeTemplateArg;
        }
        return resolveInferStructTypePath(fieldType, expr.namespacePrefix);
      }
      return "";
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        return "";
      }
      std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      if (receiverStruct.empty()) {
        return "";
      }
      std::string methodName = expr.name;
      std::string rawMethodName = expr.name;
      if (!rawMethodName.empty() && rawMethodName.front() == '/') {
        rawMethodName.erase(rawMethodName.begin());
      }
      if (!methodName.empty() && methodName.front() == '/') {
        methodName.erase(methodName.begin());
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
          !namespacedHelper.empty()) {
        methodName = namespacedHelper;
      }
      const std::string explicitRemovedMethodPath =
          explicitRemovedCollectionMethodPath(rawMethodName, expr.namespacePrefix);
      const bool preservesExplicitRemovedMethodPath =
          !explicitRemovedMethodPath.empty() &&
          hasDefinitionPath(explicitRemovedMethodPath) &&
          (receiverStruct == "/vector" || receiverStruct == "/array");
      const bool blocksBuiltinVectorAccessStructReturnForwarding =
          methodName == "at" || methodName == "at_unsafe";
      if (blocksBuiltinVectorAccessStructReturnForwarding &&
          (receiverStruct == "/vector" || receiverStruct == "/array")) {
        return "";
      }
      const bool isExplicitRemovedMapAliasStructReturnMethod =
          rawMethodName == "map/at" || rawMethodName == "map/at_unsafe" ||
          rawMethodName == "std/collections/map/at" || rawMethodName == "std/collections/map/at_unsafe";
      std::vector<std::string> methodCandidates;
      auto appendMethodCandidate = [&](const std::string &candidate) {
        if (candidate.empty()) {
          return;
        }
        for (const auto &existing : methodCandidates) {
          if (existing == candidate) {
            return;
          }
        }
        methodCandidates.push_back(candidate);
      };
      if (preservesExplicitRemovedMethodPath) {
        appendMethodCandidate(explicitRemovedMethodPath);
      }
      if (receiverStruct == "/vector") {
        appendMethodCandidate("/std/collections/vector/" + methodName);
        if (methodName != "count" && !blocksBuiltinVectorAccessStructReturnForwarding) {
          appendMethodCandidate("/array/" + methodName);
        }
      } else if (receiverStruct == "/array") {
        appendMethodCandidate("/array/" + methodName);
        if (methodName != "count") {
          if (!blocksBuiltinVectorAccessStructReturnForwarding) {
            appendMethodCandidate("/std/collections/vector/" + methodName);
          }
        }
      } else if (receiverStruct == "/map") {
        if (!isExplicitRemovedMapAliasStructReturnMethod) {
          methodCandidates = {"/std/collections/map/" + methodName, "/map/" + methodName};
        }
      } else {
        methodCandidates = {receiverStruct + "/" + methodName};
      }
      for (const auto &candidate : methodCandidates) {
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      for (const auto &candidate : methodCandidates) {
        auto defIt = defMap_.find(candidate);
        if (defIt == defMap_.end()) {
          continue;
        }
        if (!ensureDefinitionReturnKindReady(*defIt->second)) {
          return "";
        }
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      return "";
    }

    if (isMatchCall(expr)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(expr, expanded, error)) {
        return "";
      }
      return inferStructReturnPath(expanded, params, locals);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      const Expr &thenArg = expr.args[1];
      const Expr &elseArg = expr.args[2];
      const Expr *thenValue = this->getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = this->getEnvelopeValueExpr(elseArg, true);
      const Expr &thenExpr = thenValue ? *thenValue : thenArg;
      const Expr &elseExpr = elseValue ? *elseValue : elseArg;
      std::string thenPath = inferStructReturnPath(thenExpr, params, locals);
      if (thenPath.empty()) {
        return "";
      }
      std::string elsePath = inferStructReturnPath(elseExpr, params, locals);
      return (thenPath == elsePath) ? thenPath : "";
    }

    if (const Expr *valueExpr = this->getEnvelopeValueExpr(expr, false)) {
      return inferStructReturnPath(*valueExpr, params, locals);
    }

    const std::string resolvedDirectPath = resolveStructReturnCallTarget(expr);
    auto directDefIt = defMap_.find(resolvedDirectPath);
    if (directDefIt != defMap_.end() && directDefIt->second != nullptr) {
      for (const auto &transform : directDefIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        const std::string normalizedReturnType =
            normalizeBindingTypeName(transform.templateArgs.front());
        if (std::string structPath =
                resolveInferStructTypePath(normalizedReturnType,
                                           directDefIt->second->namespacePrefix);
            !structPath.empty()) {
          return structPath;
        }
        if (std::string collectionPath = normalizeCollectionTypePath(normalizedReturnType);
            !collectionPath.empty()) {
          return collectionPath;
        }
        const std::string unwrappedReturnType =
            unwrapReferencePointerTypeText(normalizedReturnType);
        if (unwrappedReturnType != normalizedReturnType) {
          if (std::string structPath =
                  resolveInferStructTypePath(unwrappedReturnType,
                                             directDefIt->second->namespacePrefix);
              !structPath.empty()) {
            return structPath;
          }
          if (std::string collectionPath = normalizeCollectionTypePath(unwrappedReturnType);
              !collectionPath.empty()) {
            return collectionPath;
          }
        }
        break;
      }
    }

    if (expr.kind == Expr::Kind::Call) {
      std::string accessHelperName;
      if (getBuiltinArrayAccessName(expr, accessHelperName) && !expr.args.empty()) {
        const std::string explicitRemovedMethodPath =
            explicitRemovedCollectionMethodPath(expr.name, expr.namespacePrefix);
        const bool preservesExplicitRemovedArrayAccessMethod =
            explicitRemovedMethodPath.rfind("/array/", 0) == 0 &&
            hasDefinitionPath(explicitRemovedMethodPath);
        size_t receiverIndex = expr.isMethodCall ? 0 : 0;
        if (!expr.isMethodCall && hasNamedArguments(expr.argNames)) {
          bool foundValues = false;
          for (size_t i = 0; i < expr.args.size(); ++i) {
            if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
                *expr.argNames[i] == "values") {
              receiverIndex = i;
              foundValues = true;
              break;
            }
          }
          if (!foundValues) {
            receiverIndex = 0;
          }
        }
        if (receiverIndex < expr.args.size()) {
          std::string elemType;
          if (builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget(expr.args[receiverIndex], elemType) ||
              builtinCollectionDispatchResolvers.resolveVectorTarget(expr.args[receiverIndex], elemType) ||
              builtinCollectionDispatchResolvers.resolveArrayTarget(expr.args[receiverIndex], elemType) ||
              builtinCollectionDispatchResolvers.resolveStringTarget(expr.args[receiverIndex])) {
            if (!preservesExplicitRemovedArrayAccessMethod) {
              return "";
            }
          }
          if (builtinCollectionDispatchResolvers.resolveSoaVectorTarget(expr.args[receiverIndex], elemType)) {
            if (std::string structPath =
                    resolveInferStructTypePath(normalizeBindingTypeName(elemType), expr.namespacePrefix);
                !structPath.empty()) {
              return structPath;
            }
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(expr, params, locals, collectionTypePath) &&
          collectionTypePath == "/map") {
        const std::string resolvedCollectionPath = resolveStructReturnCallTarget(expr);
        std::string normalizedCollectionPath = normalizeBindingTypeName(resolvedCollectionPath);
        if (!normalizedCollectionPath.empty() && normalizedCollectionPath.front() != '/') {
          normalizedCollectionPath.insert(normalizedCollectionPath.begin(), '/');
        }
        if (normalizedCollectionPath.rfind("/std/collections/experimental_map/Map__", 0) == 0 &&
            structNames_.count(resolvedCollectionPath) > 0) {
          return resolvedCollectionPath;
        }
        std::vector<std::string> collectionTemplateArgs;
        if (resolveCallCollectionTemplateArgs(expr, "map", params, locals, collectionTemplateArgs) &&
            collectionTemplateArgs.size() == 2) {
          return specializedExperimentalMapStructReturnPath(collectionTemplateArgs);
        }
        auto defIt = defMap_.find(resolvedCollectionPath);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          for (const auto &transform : defIt->second->transforms) {
            if (transform.name != "return" || transform.templateArgs.size() != 1) {
              continue;
            }
            std::string keyType;
            std::string valueType;
            if (extractMapKeyValueTypesFromTypeText(transform.templateArgs.front(), keyType, valueType)) {
              return specializedExperimentalMapStructReturnPath({keyType, valueType});
            }
          }
        }
        return "/map";
      }
    }

    const std::string resolvedCallee = resolveStructReturnCallTarget(expr);
    const bool isExplicitMapAccessCompatibilityCall =
        isExplicitMapAccessStructReturnCompatibilityCall(expr, builtinCollectionDispatchResolvers);
    const std::string structReturnProbePath = isExplicitMapAccessCompatibilityCall ? expr.name : resolvedCallee;
    auto resolvedCandidates = inferStructReturnCollectionHelperPathCandidates(structReturnProbePath);
    pruneInferStructReturnMapAccessCompatibilityCandidates(structReturnProbePath, resolvedCandidates);
    pruneInferStructReturnBuiltinVectorAccessCandidates(
        expr,
        structReturnProbePath,
        builtinCollectionDispatchResolvers,
        resolvedCandidates);
    for (const auto &candidate : resolvedCandidates) {
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
    }
    std::string resolved = resolvedCandidates.empty() ? std::string() : resolvedCandidates.front();
    bool hasDefinitionCandidate = false;
    for (const auto &candidate : resolvedCandidates) {
      auto defIt = defMap_.find(candidate);
      if (defIt == defMap_.end()) {
        continue;
      }
      hasDefinitionCandidate = true;
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        return "";
      }
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
      if (resolved.empty()) {
        resolved = candidate;
      }
    }
    std::string collection;
    if (!hasDefinitionCandidate && getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector") && expr.templateArgs.size() == 1) {
        return "/" + collection;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return "/map";
      }
    }
    if (!resolved.empty() && structNames_.count(resolved) > 0) {
      return resolved;
    }
  }

  return "";
}

} // namespace primec::semantics
