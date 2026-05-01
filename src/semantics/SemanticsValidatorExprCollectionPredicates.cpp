#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {
namespace {

bool isCanonicalMapTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == "map" || base == "/map" ||
        base == "std/collections/map" || base == "/std/collections/map") {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(arg, args) && args.size() == 2;
    }
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    return false;
  }
}

} // namespace

const Expr *SemanticsValidator::resolveBuiltinAccessReceiverExpr(const Expr &accessExpr) const {
  if (accessExpr.kind != Expr::Kind::Call || accessExpr.args.size() != 2) {
    return nullptr;
  }
  if (accessExpr.isMethodCall) {
    return accessExpr.args.empty() ? nullptr : &accessExpr.args.front();
  }
  size_t receiverIndex = 0;
  if (hasNamedArguments(accessExpr.argNames)) {
    bool foundValues = false;
    for (size_t i = 0; i < accessExpr.args.size(); ++i) {
      if (i < accessExpr.argNames.size() && accessExpr.argNames[i].has_value() &&
          *accessExpr.argNames[i] == "values") {
        receiverIndex = i;
        foundValues = true;
        break;
      }
    }
    if (!foundValues) {
      receiverIndex = 0;
    }
  }
  return receiverIndex < accessExpr.args.size() ? &accessExpr.args[receiverIndex] : nullptr;
}

bool SemanticsValidator::isNamedArgsPackMethodAccessCall(
    const Expr &target,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (!target.isMethodCall) {
    return false;
  }
  std::string accessName;
  if (!getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
    return false;
  }
  std::string elemType;
  if (dispatchResolvers.resolveArgsPackAccessTarget == nullptr ||
      !dispatchResolvers.resolveArgsPackAccessTarget(target.args.front(), elemType)) {
    return false;
  }
  size_t namedCount = 0;
  for (const auto &argName : target.argNames) {
    if (!argName.has_value()) {
      continue;
    }
    ++namedCount;
    if (*argName != "index") {
      return false;
    }
  }
  return namedCount == 1;
}

bool SemanticsValidator::isNamedArgsPackWrappedFileBuiltinAccessCall(
    const Expr &target,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (target.isMethodCall) {
    return false;
  }
  std::string accessName;
  if (!getBuiltinArrayAccessName(target, accessName) || accessName != "at" ||
      target.args.size() != 2) {
    return false;
  }
  if (target.argNames.size() != 2 || !target.argNames[0].has_value() ||
      !target.argNames[1].has_value() || *target.argNames[0] != "values" ||
      *target.argNames[1] != "index") {
    return false;
  }
  std::string elemType;
  if (dispatchResolvers.resolveArgsPackAccessTarget == nullptr ||
      !dispatchResolvers.resolveArgsPackAccessTarget(target.args.front(), elemType)) {
    return false;
  }
  std::string pointeeType = unwrapReferencePointerTypeText(elemType);
  if (pointeeType.empty() || pointeeType == elemType) {
    return false;
  }
  const std::string normalizedPointeeType = normalizeBindingTypeName(pointeeType);
  if (normalizedPointeeType == "FileError") {
    return true;
  }
  std::string base;
  std::string argText;
  return splitTemplateTypeName(normalizedPointeeType, base, argText) &&
         normalizeBindingTypeName(base) == "File" && !argText.empty();
}

bool SemanticsValidator::isMapLikeBareAccessReceiver(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) {
  std::string keyType;
  std::string valueType;
  if (dispatchResolvers.resolveMapTarget != nullptr &&
      dispatchResolvers.resolveMapTarget(candidate, keyType, valueType)) {
    return true;
  }
  if (candidate.kind != Expr::Kind::Call) {
    return false;
  }
  const std::string resolvedCandidatePath = resolveCalleePath(candidate);
  if ((resolvedCandidatePath == "/map" ||
       resolvedCandidatePath.rfind("/map__", 0) == 0) &&
      hasDeclaredDefinitionPath("/map")) {
    return false;
  }
  auto defIt = defMap_.find(resolvedCandidatePath);
  if ((defIt == defMap_.end() || defIt->second == nullptr) &&
      !candidate.name.empty() && candidate.name.find('/') == std::string::npos) {
    defIt = defMap_.find("/" + candidate.name);
  }
  if (defIt != defMap_.end() && defIt->second != nullptr) {
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferredTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" +
                    inferredReturn.typeTemplateArg + ">";
      if (isCanonicalMapTypeText(inferredTypeText)) {
        return true;
      }
      // When a concrete return binding is available and is not map-like,
      // trust it over broad query fallback inference.
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          isCanonicalMapTypeText(transform.templateArgs.front())) {
        return true;
      }
    }
  }
  std::string receiverTypeText;
  return inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
         isCanonicalMapTypeText(receiverTypeText);
}

bool SemanticsValidator::isArrayNamespacedVectorCountCompatibilityCall(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }
  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const bool spellsArrayCount = (normalized == "array/count");
  const bool resolvesArrayCount = (resolveCalleePath(candidate) == "/array/count");
  if (!spellsArrayCount && !resolvesArrayCount) {
    return false;
  }
  if (hasDeclaredDefinitionPath("/array/count") || hasImportedDefinitionPath("/array/count")) {
    return false;
  }
  if (dispatchResolvers.resolveVectorTarget == nullptr) {
    return false;
  }
  for (const Expr &arg : candidate.args) {
    std::string elemType;
    if (dispatchResolvers.resolveVectorTarget(arg, elemType)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::isArrayNamespacedVectorAccessCompatibilityCall(
    const Expr &candidate,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
    return false;
  }
  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  const bool spellsArrayAccess =
      normalized == "array/at" || normalized == "array/at_unsafe";
  const std::string resolvedPath = resolveCalleePath(candidate);
  const bool resolvesArrayAccess =
      resolvedPath == "/array/at" || resolvedPath == "/array/at_unsafe";
  if (!spellsArrayAccess && !resolvesArrayAccess) {
    return false;
  }
  const std::string helperPath =
      resolvedPath == "/array/at" || normalized == "array/at" ? "/array/at"
                                                                : "/array/at_unsafe";
  if (hasDeclaredDefinitionPath(helperPath) || hasImportedDefinitionPath(helperPath)) {
    return false;
  }
  if (dispatchResolvers.resolveVectorTarget == nullptr) {
    return false;
  }
  for (const Expr &arg : candidate.args) {
    std::string elemType;
    if (dispatchResolvers.resolveVectorTarget(arg, elemType)) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::isIndexedArgsPackMapReceiverTarget(
    const Expr &receiverExpr,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) const {
  std::string elemType;
  std::string keyType;
  std::string valueType;
  if (!((dispatchResolvers.resolveIndexedArgsPackElementType != nullptr &&
         dispatchResolvers.resolveIndexedArgsPackElementType(receiverExpr, elemType)) ||
        (dispatchResolvers.resolveWrappedIndexedArgsPackElementType != nullptr &&
         dispatchResolvers.resolveWrappedIndexedArgsPackElementType(receiverExpr, elemType)) ||
        (dispatchResolvers.resolveDereferencedIndexedArgsPackElementType != nullptr &&
         dispatchResolvers.resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType)))) {
    return false;
  }
  const std::string unwrappedElemType =
      normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType));
  const std::string mapElemType =
      unwrappedElemType.empty() ? elemType : unwrappedElemType;
  return extractMapKeyValueTypesFromTypeText(mapElemType, keyType, valueType);
}

bool SemanticsValidator::validateCollectionElementType(
    const Expr &arg,
    const std::string &typeName,
    const std::string &errorPrefix,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers) {
  auto failCollectionPredicateDiagnostic =
      [&](std::string message) -> bool {
    return failExprDiagnostic(arg, std::move(message));
  };
  const std::string normalizedType = normalizeBindingTypeName(typeName);
  if (normalizedType == "string") {
    if (!isStringExprForArgumentValidation(arg, dispatchResolvers)) {
      return failCollectionPredicateDiagnostic(errorPrefix + typeName);
    }
    return true;
  }
  ReturnKind expectedKind = returnKindForTypeName(normalizedType);
  if (expectedKind == ReturnKind::Unknown) {
    return true;
  }
  if (isStringExprForArgumentValidation(arg, dispatchResolvers)) {
    return failCollectionPredicateDiagnostic(errorPrefix + typeName);
  }
  ReturnKind argKind = inferExprReturnKind(arg, params, locals);
  if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
    return failCollectionPredicateDiagnostic(errorPrefix + typeName);
  }
  return true;
}

} // namespace primec::semantics
