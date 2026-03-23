#include "SemanticsValidator.h"

#include <string>
#include <string_view>

namespace primec::semantics {

bool SemanticsValidator::resolveNonCollectionAccessHelperPathFromTypeText(
    const std::string &typeText,
    const std::string &typeNamespace,
    std::string_view helperName,
    std::string &pathOut) const {
  pathOut.clear();
  std::string normalizedType = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
  if (normalizedType.empty() || !normalizeCollectionTypePath(normalizedType).empty() ||
      normalizedType == "Pointer" || normalizedType == "Reference") {
    return false;
  }
  if (isPrimitiveBindingTypeName(normalizedType)) {
    pathOut = "/" + normalizedType + "/" + std::string(helperName);
    return true;
  }
  std::string resolvedLookupType = normalizedType;
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    base = normalizeBindingTypeName(base);
    if (base.empty() || base == "args" || base == "Pointer" || base == "Reference" ||
        !normalizeCollectionTypePath(base).empty()) {
      return false;
    }
    resolvedLookupType = base;
  }
  std::string resolvedType = resolveStructTypePath(resolvedLookupType, typeNamespace, structNames_);
  if (resolvedType.empty()) {
    auto importIt = importAliases_.find(resolvedLookupType);
    if (importIt != importAliases_.end()) {
      resolvedType = importIt->second;
    }
  }
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(resolvedLookupType, typeNamespace);
  }
  if (resolvedType.empty() ||
      resolvedType.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    return false;
  }
  pathOut = resolvedType + "/" + std::string(helperName);
  return true;
}

bool SemanticsValidator::resolveLeadingNonCollectionAccessReceiverPath(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &receiverExpr,
    std::string_view helperName,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    std::string &pathOut) {
  pathOut.clear();
  auto formatBindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto isKnownCollectionLikeReceiver = [&](const Expr &candidate) {
    std::string elemType;
    std::string keyType;
    std::string valueType;
    return ((dispatchResolvers.resolveVectorTarget != nullptr &&
             dispatchResolvers.resolveVectorTarget(candidate, elemType)) ||
            (dispatchResolvers.resolveArrayTarget != nullptr &&
             dispatchResolvers.resolveArrayTarget(candidate, elemType)) ||
            (dispatchResolvers.resolveSoaVectorTarget != nullptr &&
             dispatchResolvers.resolveSoaVectorTarget(candidate, elemType)) ||
            (dispatchResolvers.resolveStringTarget != nullptr &&
             dispatchResolvers.resolveStringTarget(candidate)) ||
            (dispatchResolvers.resolveMapTarget != nullptr &&
             dispatchResolvers.resolveMapTarget(candidate, keyType, valueType)) ||
            isPointerExpr(candidate, params, locals) ||
            isPointerLikeExpr(candidate, params, locals));
  };

  if (receiverExpr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
      if (isArgsPackBinding(*paramBinding) || isKnownCollectionLikeReceiver(receiverExpr)) {
        return false;
      }
      return resolveNonCollectionAccessHelperPathFromTypeText(
          formatBindingTypeText(*paramBinding), receiverExpr.namespacePrefix, helperName, pathOut);
    }
    auto localIt = locals.find(receiverExpr.name);
    if (localIt != locals.end()) {
      if (isArgsPackBinding(localIt->second) || isKnownCollectionLikeReceiver(receiverExpr)) {
        return false;
      }
      return resolveNonCollectionAccessHelperPathFromTypeText(
          formatBindingTypeText(localIt->second), receiverExpr.namespacePrefix, helperName, pathOut);
    }
    return false;
  }
  if (receiverExpr.kind != Expr::Kind::Call) {
    return false;
  }
  if (isSimpleCallName(receiverExpr, "array") || isSimpleCallName(receiverExpr, "vector") ||
      isSimpleCallName(receiverExpr, "map") || isSimpleCallName(receiverExpr, "soa_vector")) {
    return false;
  }
  const std::string resolvedReceiverPath = resolveCalleePath(receiverExpr);
  if (resolvedReceiverPath == "/array" || resolvedReceiverPath == "/vector" ||
      resolvedReceiverPath == "/map" || resolvedReceiverPath == "/soa_vector") {
    return false;
  }
  auto defIt = defMap_.find(resolvedReceiverPath);
  if (defIt != defMap_.end() && defIt->second != nullptr) {
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      if (resolveNonCollectionAccessHelperPathFromTypeText(
              transform.templateArgs.front(), defIt->second->namespacePrefix, helperName, pathOut)) {
        return true;
      }
      break;
    }
    BindingInfo inferredReturnBinding;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturnBinding) &&
        resolveNonCollectionAccessHelperPathFromTypeText(
            inferredReturnBinding.typeTemplateArg.empty()
                ? inferredReturnBinding.typeName
                : inferredReturnBinding.typeName + "<" + inferredReturnBinding.typeTemplateArg + ">",
            defIt->second->namespacePrefix,
            helperName,
            pathOut)) {
      return true;
    }
  }
  if (isKnownCollectionLikeReceiver(receiverExpr)) {
    return false;
  }
  std::string receiverTypeText;
  if (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
      !receiverTypeText.empty() &&
      resolveNonCollectionAccessHelperPathFromTypeText(
          receiverTypeText, receiverExpr.namespacePrefix, helperName, pathOut)) {
    return true;
  }
  BindingInfo inferredReceiverBinding;
  if (inferBindingTypeFromInitializer(receiverExpr, params, locals, inferredReceiverBinding) &&
      resolveNonCollectionAccessHelperPathFromTypeText(
          inferredReceiverBinding.typeTemplateArg.empty()
              ? inferredReceiverBinding.typeName
              : inferredReceiverBinding.typeName + "<" + inferredReceiverBinding.typeTemplateArg + ">",
          receiverExpr.namespacePrefix,
          helperName,
          pathOut)) {
    return true;
  }
  const std::string structPath = inferStructReturnPath(receiverExpr, params, locals);
  if (structPath.empty() || !normalizeCollectionTypePath(structPath).empty() ||
      structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    return false;
  }
  pathOut = structPath + "/" + std::string(helperName);
  return true;
}

bool SemanticsValidator::resolveDirectCallTemporaryAccessReceiverPath(
    const Expr &receiverExpr,
    std::string_view helperName,
    std::string &pathOut) {
  pathOut.clear();
  if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
    return false;
  }
  if (isSimpleCallName(receiverExpr, "array") || isSimpleCallName(receiverExpr, "vector") ||
      isSimpleCallName(receiverExpr, "map") || isSimpleCallName(receiverExpr, "soa_vector")) {
    return false;
  }
  const std::string resolvedReceiverPath = resolveCalleePath(receiverExpr);
  auto defIt = defMap_.find(resolvedReceiverPath);
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    return resolveNonCollectionAccessHelperPathFromTypeText(
        transform.templateArgs.front(), defIt->second->namespacePrefix, helperName, pathOut);
  }
  BindingInfo inferredReturnBinding;
  return inferDefinitionReturnBinding(*defIt->second, inferredReturnBinding) &&
         resolveNonCollectionAccessHelperPathFromTypeText(
             inferredReturnBinding.typeTemplateArg.empty()
                 ? inferredReturnBinding.typeName
                 : inferredReturnBinding.typeName + "<" + inferredReturnBinding.typeTemplateArg + ">",
             defIt->second->namespacePrefix,
             helperName,
             pathOut);
}

} // namespace primec::semantics
