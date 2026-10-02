#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "primec/support/CollectionHelperNames.h"

#include <string>
#include <string_view>

namespace primec::semantics {

namespace {
bool isSpecializedExperimentalKeyValueBackingPath(std::string typeName) {
  typeName = normalizeBindingTypeName(typeName);
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return isExperimentalCollectionBackingTypeName("map", "Map", typeName) &&
         typeName.find("__") != std::string::npos;
}

bool isRootMapCollectionReceiverPath(std::string_view path) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (!alias.empty() && alias.front() == '/') {
      alias.remove_prefix(1);
    }
    if (alias.find('/') == std::string_view::npos &&
        path == "/" + std::string(alias)) {
      return true;
    }
  }
  return false;
}
} // namespace

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
      isSpecializedExperimentalKeyValueBackingPath(resolvedType)) {
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
      isSimpleCallName(receiverExpr, "map") || isSimpleCallName(receiverExpr, "soa")) {
    return false;
  }
  const std::string resolvedReceiverPath = resolveCalleePath(receiverExpr);
  if (collection_helpers::isCollectionFamilyRoot(resolvedReceiverPath, collection_helpers::CollectionFamily::Array) || collection_helpers::isCollectionFamilyRoot(resolvedReceiverPath, collection_helpers::CollectionFamily::Vector) ||
      isRootMapCollectionReceiverPath(resolvedReceiverPath) || collection_helpers::isCollectionFamilyRoot(resolvedReceiverPath, collection_helpers::CollectionFamily::Soa)) {
    return false;
  }
  auto defIt = defMap_.find(resolvedReceiverPath);
  if (defIt != defMap_.end() && defIt->second != nullptr) {
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1 ||
          normalizeBindingTypeName(transform.templateArgs.front()) == "auto") {
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
      isSpecializedExperimentalKeyValueBackingPath(structPath)) {
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
      isSimpleCallName(receiverExpr, "map") || isSimpleCallName(receiverExpr, "soa")) {
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

bool SemanticsValidator::resolveUserStructOwnAccessHelperCallPath(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &pathOut) {
  pathOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding ||
      expr.args.size() < 2 || hasNamedArguments(expr.argNames) ||
      !(isSimpleCallName(expr, "at") || isSimpleCallName(expr, collection_helpers::kAtRef) ||
        isSimpleCallName(expr, "at_unsafe") ||
        isSimpleCallName(expr, collection_helpers::kAtUnsafeRef)) ||
      defMap_.count("/" + expr.name) > 0) {
    return false;
  }
  ExprDispatchBootstrap dispatchBootstrap;
  prepareExprDispatchBootstrap(params, locals, dispatchBootstrap);
  std::string candidatePath;
  if (!resolveDirectCallTemporaryAccessReceiverPath(
          expr.args.front(), expr.name, candidatePath) &&
      !resolveLeadingNonCollectionAccessReceiverPath(
          params,
          locals,
          expr.args.front(),
          expr.name,
          dispatchBootstrap.dispatchResolvers,
          candidatePath)) {
    return false;
  }
  const size_t helperSlash = candidatePath.find_last_of('/');
  if (helperSlash == std::string::npos || helperSlash == 0) {
    return false;
  }
  const std::string structPath = candidatePath.substr(0, helperSlash);
  auto defIt = defMap_.find(candidatePath);
  if (structNames_.count(structPath) == 0 || defIt == defMap_.end() ||
      defIt->second == nullptr) {
    return false;
  }
  pathOut = std::move(candidatePath);
  return true;
}

} // namespace primec::semantics
