#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

namespace primec::semantics {
namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

} // namespace

bool SemanticsValidator::graphBindingIsUsable(const BindingInfo &binding) const {
  const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
  if (normalizedType.empty() || normalizedType == "auto") {
    return false;
  }
  if (normalizedType == "array" && binding.typeTemplateArg.empty()) {
    return false;
  }
  if (!binding.typeTemplateArg.empty()) {
    return true;
  }
  if (normalizedType == "Result" || normalizedType == "Pointer" || normalizedType == "Reference") {
    return true;
  }
  if (returnKindForTypeName(normalizedType) != ReturnKind::Unknown) {
    return true;
  }
  if (structNames_.count(binding.typeName) > 0) {
    return true;
  }
  std::string scopeNamespace;
  const auto scopeIt = defMap_.find(currentValidationContext_.definitionPath);
  if (scopeIt != defMap_.end() && scopeIt->second != nullptr) {
    scopeNamespace = scopeIt->second->namespacePrefix;
  }
  if (!resolveStructTypePath(normalizedType, scopeNamespace, structNames_).empty()) {
    return true;
  }
  auto importIt = importAliases_.find(normalizedType);
  return importIt != importAliases_.end() && structNames_.count(importIt->second) > 0;
}

bool SemanticsValidator::shouldBypassGraphBindingLookup(const Expr &candidate) const {
  if (candidate.args.size() != 1 || candidate.args.front().kind != Expr::Kind::Call) {
    return false;
  }
  const Expr &initializerCall = candidate.args.front();
  std::string collectionName;
  if (getBuiltinCollectionName(initializerCall, collectionName)) {
    return true;
  }
  std::string normalizedName = initializerCall.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = initializerCall.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  if (normalizedName == "vector/vector" ||
      normalizedName == "vector" ||
      (normalizedPrefix == "vector" && normalizedName == "vector") ||
      (normalizedPrefix == "std/collections/vector" && normalizedName == "vector")) {
    return true;
  }
  if ((normalizedPrefix == "std/collections/map" &&
       (normalizedName == "count" || normalizedName == "contains" ||
        normalizedName == "tryAt" || normalizedName == "at" ||
        normalizedName == "at_unsafe")) ||
      normalizedName == "std/collections/map/count" ||
      normalizedName == "std/collections/map/contains" ||
      normalizedName == "std/collections/map/tryAt" ||
      normalizedName == "std/collections/map/at" ||
      normalizedName == "std/collections/map/at_unsafe") {
    return true;
  }
  if ((normalizedPrefix == "std/collections/vector" &&
       (normalizedName == "at" || normalizedName == "at_unsafe" ||
        normalizedName == "push" || normalizedName == "pop" ||
        normalizedName == "reserve" || normalizedName == "clear" ||
        normalizedName == "remove_at" || normalizedName == "remove_swap")) ||
      normalizedName == "std/collections/vector/at" ||
      normalizedName == "std/collections/vector/at_unsafe" ||
      normalizedName == "std/collections/vector/push" ||
      normalizedName == "std/collections/vector/pop" ||
      normalizedName == "std/collections/vector/reserve" ||
      normalizedName == "std/collections/vector/clear" ||
      normalizedName == "std/collections/vector/remove_at" ||
      normalizedName == "std/collections/vector/remove_swap") {
    return true;
  }
  return false;
}

bool SemanticsValidator::isBuiltinSoaRefExpr(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
    return false;
  }

  auto isDirectSoaVectorTarget = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return normalizeBindingTypeName(paramBinding->typeName) == "soa_vector";
      }
      auto localIt = locals.find(target.name);
      return localIt != locals.end() &&
             normalizeBindingTypeName(localIt->second.typeName) == "soa_vector";
    }
    std::string builtinCollection;
    return getBuiltinCollectionName(target, builtinCollection) &&
           builtinCollection == "soa_vector";
  };

  if (hasImportedDefinitionPath("/soa_vector/ref") ||
      hasDeclaredDefinitionPath("/soa_vector/ref")) {
    return false;
  }

  const std::string resolved = resolveCalleePath(candidate);
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }

  const bool isExplicitSoaRefCall =
      (!candidate.isMethodCall && normalizedPrefix == "soa_vector" &&
       normalizedName == "ref") ||
      normalizedName == "soa_vector/ref";
  const bool isBuiltinSoaRefMethod =
      candidate.isMethodCall && normalizedName == "ref" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());

  return resolved == "/soa_vector/ref" ||
         resolved == "/std/collections/soa_vector/ref" ||
         isExplicitSoaRefCall ||
         isBuiltinSoaRefMethod ||
         (!candidate.isMethodCall && isSimpleCallName(candidate, "ref"));
}

bool SemanticsValidator::isBuiltinSoaFieldViewExpr(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string *fieldNameOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
    return false;
  }

  auto assignBindingTypeFromText = [](const std::string &typeText, BindingInfo &bindingOut) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto inferSoaReceiverBinding = [&](const Expr &receiver, BindingInfo &bindingOut) -> bool {
    if (receiver.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto localIt = locals.find(receiver.name);
      if (localIt != locals.end()) {
        bindingOut = localIt->second;
        return true;
      }
    }
    if (const_cast<SemanticsValidator *>(this)->inferBindingTypeFromInitializer(
            receiver, params, locals, bindingOut)) {
      return true;
    }
    std::string inferredTypeText;
    return const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(
               receiver, params, locals, inferredTypeText) &&
           assignBindingTypeFromText(inferredTypeText, bindingOut);
  };

  const std::string resolved = resolveCalleePath(candidate);
  if (splitSoaFieldViewHelperPath(resolved, fieldNameOut)) {
    return true;
  }

  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  if (normalizedName.empty() || normalizedName == "count" || normalizedName == "get" ||
      normalizedName == "ref" || normalizedName == "to_soa" || normalizedName == "to_aos") {
    return false;
  }

  const std::string helperPath = "/soa_vector/" + normalizedName;
  if (hasImportedDefinitionPath(helperPath) || hasDeclaredDefinitionPath(helperPath)) {
    return false;
  }

  const Expr *receiver = nullptr;
  if (candidate.isMethodCall) {
    if (candidate.args.empty()) {
      return false;
    }
    receiver = &candidate.args.front();
  } else {
    if (!normalizedPrefix.empty() || candidate.args.size() != 1) {
      return false;
    }
    receiver = &candidate.args.front();
  }

  BindingInfo receiverBinding;
  if (!inferSoaReceiverBinding(*receiver, receiverBinding) ||
      normalizeBindingTypeName(receiverBinding.typeName) != "soa_vector" ||
      receiverBinding.typeTemplateArg.empty()) {
    return false;
  }

  std::string currentNamespace;
  if (!currentValidationContext_.definitionPath.empty()) {
    const size_t slash = currentValidationContext_.definitionPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      currentNamespace = currentValidationContext_.definitionPath.substr(0, slash);
    }
  }
  const std::string lookupNamespace =
      !receiver->namespacePrefix.empty() ? receiver->namespacePrefix : currentNamespace;
  const std::string elementStructPath =
      resolveStructTypePath(normalizeBindingTypeName(receiverBinding.typeTemplateArg),
                            lookupNamespace,
                            structNames_);
  auto structIt = defMap_.find(elementStructPath);
  if (elementStructPath.empty() || structIt == defMap_.end() || structIt->second == nullptr) {
    return false;
  }

  for (const auto &stmt : structIt->second->statements) {
    if (stmt.isBinding && stmt.name == normalizedName) {
      if (fieldNameOut != nullptr) {
        *fieldNameOut = normalizedName;
      }
      return true;
    }
  }
  return false;
}

std::optional<std::string> SemanticsValidator::builtinSoaPendingExprDiagnostic(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  std::string soaFieldViewName;
  if (isBuiltinSoaFieldViewExpr(expr, params, locals, &soaFieldViewName)) {
    return soaFieldViewPendingDiagnostic(soaFieldViewName);
  }
  if (isBuiltinSoaRefExpr(expr, params, locals)) {
    return soaBorrowedViewPendingDiagnostic();
  }
  return std::nullopt;
}

bool SemanticsValidator::reportBuiltinSoaPendingExprDiagnostic(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  if (auto soaPending = builtinSoaPendingExprDiagnostic(expr, params, locals)) {
    error_ = *soaPending;
    return false;
  }
  return true;
}

bool SemanticsValidator::hasDirectExperimentalVectorImport() const {
  const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
  for (const auto &importPath : importPaths) {
    if (importPath == "/std/collections/experimental_vector/*" ||
        importPath == "/std/collections/experimental_vector/vector" ||
        importPath == "/std/collections/experimental_vector") {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::canonicalizeInferredCollectionBinding(
    const Expr *sourceExpr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  auto canonicalizeResolvedPath = [](std::string path) {
    const size_t suffix = path.find("__t");
    if (suffix != std::string::npos) {
      path.erase(suffix);
    }
    return path;
  };
  auto preferResolvedCollectionBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinCollectionName;
    if (getBuiltinCollectionName(candidate, builtinCollectionName)) {
      return false;
    }
    const std::string resolvedCandidate = canonicalizeResolvedPath(resolveCalleePath(candidate));
    if (resolvedCandidate.empty()) {
      return false;
    }
    BindingInfo resolvedBinding;
    if (!inferResolvedDirectCallBindingType(resolvedCandidate, resolvedBinding)) {
      return false;
    }
    if (normalizeCollectionTypePath(bindingTypeText(resolvedBinding)).empty()) {
      return false;
    }
    bindingOut = std::move(resolvedBinding);
    return true;
  };
  auto inferDirectMapConstructorBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || !isResolvedMapConstructorPath(resolveCalleePath(candidate))) {
      return false;
    }
    if (candidate.templateArgs.size() == 2) {
      bindingOut.typeName = "Map";
      bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
      return true;
    }
    if (candidate.args.size() < 2 || candidate.args.size() % 2 != 0) {
      return false;
    }
    auto inferArgumentTypeText = [&](const Expr &argument, std::string &typeTextOut) -> bool {
      typeTextOut.clear();
      if (inferQueryExprTypeText(argument, params, locals, typeTextOut) && !typeTextOut.empty()) {
        typeTextOut = normalizeBindingTypeName(typeTextOut);
        return true;
      }
      const ReturnKind argumentKind = inferExprReturnKind(argument, params, locals);
      if (argumentKind == ReturnKind::Unknown || argumentKind == ReturnKind::Void) {
        return false;
      }
      typeTextOut = typeNameForReturnKind(argumentKind);
      return !typeTextOut.empty();
    };
    std::string keyTypeText;
    std::string valueTypeText;
    if (!inferArgumentTypeText(candidate.args[0], keyTypeText) ||
        !inferArgumentTypeText(candidate.args[1], valueTypeText)) {
      return false;
    }
    bindingOut.typeName = "Map";
    bindingOut.typeTemplateArg = keyTypeText + ", " + valueTypeText;
    return true;
  };
  auto applySourceExprCollectionTemplateArgs = [&](const Expr &candidate) -> bool {
    std::vector<std::string> collectionArgs;
    if (resolveCallCollectionTemplateArgs(candidate,
                                         bindingOut.typeName,
                                         params,
                                         locals,
                                         collectionArgs) &&
        !collectionArgs.empty()) {
      bindingOut.typeTemplateArg = joinTemplateArgs(collectionArgs);
      return true;
    }
    std::string collectionName;
    if (!getBuiltinCollectionName(candidate, collectionName)) {
      return false;
    }
    if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
        candidate.templateArgs.size() == 1) {
      bindingOut.typeName = collectionName;
      bindingOut.typeTemplateArg = candidate.templateArgs.front();
      return true;
    }
    if (collectionName == "map" && candidate.templateArgs.size() == 2) {
      bindingOut.typeName = "map";
      bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
      return true;
    }
    return false;
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };

  if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call &&
      inferDirectMapConstructorBinding(*sourceExpr)) {
    return true;
  }
  if (sourceExpr != nullptr && preferResolvedCollectionBinding(*sourceExpr)) {
    return true;
  }
  const std::string normalizedBindingType = normalizeBindingTypeName(bindingOut.typeName);
  if ((normalizedBindingType == "Vector" && !bindingOut.typeTemplateArg.empty()) ||
      normalizedBindingType.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
      normalizedBindingType.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return true;
  }
  if ((normalizedBindingType == "Map" && !bindingOut.typeTemplateArg.empty()) ||
      normalizedBindingType.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
      normalizedBindingType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
    return true;
  }
  const std::string normalizedCollectionType = normalizeCollectionTypePath(bindingTypeText(bindingOut));
  if (normalizedCollectionType.empty()) {
    return false;
  }
  if (sourceExpr != nullptr) {
    std::string inferredTypeText;
    if (inferQueryExprTypeText(*sourceExpr, params, locals, inferredTypeText) &&
        assignBindingTypeFromText(inferredTypeText)) {
      (void)applySourceExprCollectionTemplateArgs(*sourceExpr);
      return true;
    }
  }
  bindingOut.typeName = normalizedCollectionType.substr(1);
  bindingOut.typeTemplateArg.clear();
  if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call) {
    if (applySourceExprCollectionTemplateArgs(*sourceExpr)) {
      return true;
    }
  }
  return true;
}

bool SemanticsValidator::inferBindingTypeFromInitializer(
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut,
    const Expr *bindingExpr) {
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto assignBindingTypeFromResultInfo = [&](const ResultTypeInfo &resultInfo) -> bool {
    if (!resultInfo.isResult || resultInfo.errorType.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    if (!resultInfo.hasValue) {
      bindingOut.typeTemplateArg = resultInfo.errorType;
      return true;
    }
    if (resultInfo.valueType.empty()) {
      return false;
    }
    bindingOut.typeTemplateArg = resultInfo.valueType + ", " + resultInfo.errorType;
    return true;
  };
  if (initializer.isLambda) {
    bindingOut.typeName = "lambda";
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  auto inferUninitializedTakeBorrowBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
        initializer.args.size() != 1 ||
        (!isSimpleCallName(initializer, "take") &&
         !isSimpleCallName(initializer, "borrow"))) {
      return false;
    }
    BindingInfo storageBinding;
    bool resolvedStorage = false;
    if (!resolveUninitializedStorageBinding(params,
                                           locals,
                                           initializer.args.front(),
                                           storageBinding,
                                           resolvedStorage) ||
        !resolvedStorage || storageBinding.typeName != "uninitialized" ||
        storageBinding.typeTemplateArg.empty()) {
      return false;
    }
    return assignBindingTypeFromText(storageBinding.typeTemplateArg);
  };
  if (bindingExpr != nullptr && !shouldBypassGraphBindingLookup(*bindingExpr) &&
      lookupGraphLocalAutoBinding(*bindingExpr, bindingOut)) {
    if (graphBindingIsUsable(bindingOut)) {
      return true;
    }
    bindingOut = {};
  }
  if (initializer.kind == Expr::Kind::Call &&
      !initializer.isMethodCall &&
      initializer.templateArgs.size() == 1 &&
      resolveCalleePath(initializer) == "/vector") {
    if (!hasDirectExperimentalVectorImport()) {
      bindingOut.typeName = "vector";
      bindingOut.typeTemplateArg = initializer.templateArgs.front();
      return true;
    }
    bindingOut.typeName = "Vector";
    bindingOut.typeTemplateArg = initializer.templateArgs.front();
    return true;
  }
  auto inferTryInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || !isSimpleCallName(initializer, "try") ||
        initializer.args.size() != 1 || !initializer.templateArgs.empty() || initializer.hasBodyArguments ||
        !initializer.bodyArguments.empty()) {
      return false;
    }
    ResultTypeInfo resultInfo;
    if (!resolveResultTypeForExpr(initializer.args.front(), params, locals, resultInfo) || !resultInfo.isResult ||
        !resultInfo.hasValue || resultInfo.valueType.empty()) {
      return false;
    }
    return assignBindingTypeFromText(resultInfo.valueType);
  };
  if (inferUninitializedTakeBorrowBinding()) {
    return true;
  }
  auto inferDirectResultOkBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || !initializer.isMethodCall || initializer.name != "ok" ||
        initializer.templateArgs.size() != 0 || initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
      return false;
    }
    if (initializer.args.empty()) {
      return false;
    }
    const Expr &receiver = initializer.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    auto inferCurrentErrorType = [&]() -> std::string {
      if (currentValidationContext_.resultType.has_value() &&
          currentValidationContext_.resultType->isResult &&
          !currentValidationContext_.resultType->errorType.empty()) {
        return currentValidationContext_.resultType->errorType;
      }
      if (currentValidationContext_.onError.has_value() &&
          !currentValidationContext_.onError->errorType.empty()) {
        return currentValidationContext_.onError->errorType;
      }
      return "_";
    };
    if (initializer.args.size() == 1) {
      bindingOut.typeName = "Result";
      bindingOut.typeTemplateArg = inferCurrentErrorType();
      return true;
    }
    if (initializer.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(initializer.args.back(), params, locals, payloadBinding)) {
      return false;
    }
    const std::string payloadTypeText = bindingTypeText(payloadBinding);
    if (payloadTypeText.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    bindingOut.typeTemplateArg = payloadTypeText + ", " + inferCurrentErrorType();
    return true;
  };
  if (initializer.kind == Expr::Kind::Call) {
    const BindingInfo previousBinding = bindingOut;
    bindingOut = {};
    if (inferCallInitializerBinding(initializer, params, locals, bindingOut)) {
      (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
      if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
        return true;
      }
    }
    bindingOut = previousBinding;
  }
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, bindingOut, hasAnyMathImport())) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
      return true;
    }
    if (inferCallInitializerBinding(initializer, params, locals, bindingOut)) {
      (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
      return true;
    }
    if (inferBuiltinCollectionValueBinding(initializer, params, locals, bindingOut)) {
      return true;
    }
    if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
      return true;
    }
    return true;
  }
  if (inferTryInitializerBinding()) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    return true;
  }
  if (inferDirectResultOkBinding()) {
    return true;
  }
  ResultTypeInfo resultInfo;
  if (resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
      assignBindingTypeFromResultInfo(resultInfo)) {
    return true;
  }
  if (inferCallInitializerBinding(initializer, params, locals, bindingOut)) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    return true;
  }
  if (inferBuiltinCollectionValueBinding(initializer, params, locals, bindingOut)) {
    return true;
  }
  if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
    return true;
  }
  ReturnKind kind = inferExprReturnKind(initializer, params, locals);
  if (kind == ReturnKind::Unknown || kind == ReturnKind::Void) {
    return false;
  }
  std::string inferred = typeNameForReturnKind(kind);
  if (inferred.empty()) {
    return false;
  }
  bindingOut.typeName = inferred;
  bindingOut.typeTemplateArg.clear();
  return true;
}

}  // namespace primec::semantics
