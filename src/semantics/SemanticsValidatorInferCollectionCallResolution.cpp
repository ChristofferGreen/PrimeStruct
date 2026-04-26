#include "SemanticsValidator.h"

#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "MapConstructorHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

namespace primec::semantics {

bool SemanticsValidator::resolveCallCollectionTypePath(const Expr &target,
                                                       const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                                       std::string &typePathOut) {
  typePathOut.clear();
  auto explicitCallPath = [&](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.name.empty()) {
      return {};
    }
    std::string rawName = callExpr.name;
    if (!rawName.empty() && rawName.front() == '/') {
      return rawName;
    }
    std::string rawPrefix = callExpr.namespacePrefix;
    if (!rawPrefix.empty() && rawPrefix.front() != '/') {
      rawPrefix.insert(rawPrefix.begin(), '/');
    }
    if (rawPrefix.empty()) {
      return "/" + rawName;
    }
    return rawPrefix + "/" + rawName;
  };
  auto resolvedCallPath = [&](const Expr &callExpr) { return resolveCalleePath(callExpr); };
  auto inferCollectionTypePathFromType =
      [&](const std::string &typeName, auto &&inferCollectionTypePathFromTypeRef) -> std::string {
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    std::string normalized = normalizeCollectionTypePath(normalizedType);
    if (!normalized.empty()) {
      return normalized;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return {};
    }
    base = normalizeBindingTypeName(base);
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return {};
      }
      return inferCollectionTypePathFromTypeRef(args.front(), inferCollectionTypePathFromTypeRef);
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(arg, args)) {
      return {};
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      return "/" + base;
    }
    if (isExperimentalSoaVectorTypePath(base) && args.size() == 1) {
      return "/soa_vector";
    }
    if (base == "Buffer" && args.size() == 1) {
      return "/Buffer";
    }
    if ((base == "Vector" || base == "std/collections/experimental_vector/Vector") &&
        args.size() == 1) {
      return "/vector";
    }
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      return "/map";
    }
    return {};
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }
  const bool hasNamedTargetArgs = hasNamedArguments(target.argNames);

  const BuiltinCollectionDispatchResolverAdapters builtinCollectionDispatchResolverAdapters;
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals, builtinCollectionDispatchResolverAdapters);
  const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;

  std::string targetTypeText;
  bool inferredNonCollectionTargetType = false;
  if (inferQueryExprTypeText(target, params, locals, targetTypeText)) {
    const std::string inferred = inferCollectionTypePathFromType(targetTypeText, inferCollectionTypePathFromType);
    if (!inferred.empty()) {
      typePathOut = inferred;
      return true;
    }
    inferredNonCollectionTargetType = !targetTypeText.empty();
  }

  std::string bufferElemType;
  if (resolveBufferTarget != nullptr && resolveBufferTarget(target, bufferElemType) && !bufferElemType.empty()) {
    typePathOut = "/Buffer";
    return true;
  }

  const std::string resolvedTarget = resolvedCallPath(target);
  const std::string explicitTarget = explicitCallPath(target);
  const bool matchesVectorCtorFamily =
      !hasNamedTargetArgs &&
      !inferredNonCollectionTargetType &&
      (isResolvedVectorConstructorHelperPath(resolvedTarget) ||
       isResolvedVectorConstructorHelperPath(explicitTarget));
  if (matchesVectorCtorFamily) {
    typePathOut = "/vector";
    return true;
  }
  for (const std::string *candidatePath : {&resolvedTarget, &explicitTarget}) {
    if (candidatePath->empty()) {
      continue;
    }
    auto structIt = returnStructs_.find(*candidatePath);
    if (structIt == returnStructs_.end()) {
      continue;
    }
    std::string normalized = normalizeCollectionTypePath(structIt->second);
    if (!normalized.empty()) {
      typePathOut = normalized;
      return true;
    }
  }
  bool inferredDeclaredNonCollectionReturnType = false;
  for (const std::string *candidatePath : {&resolvedTarget, &explicitTarget}) {
    if (candidatePath->empty()) {
      continue;
    }
    auto defIt = defMap_.find(*candidatePath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      continue;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferredReturnTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
      const std::string inferred =
          inferCollectionTypePathFromType(inferredReturnTypeText, inferCollectionTypePathFromType);
      if (!inferred.empty()) {
        typePathOut = inferred;
        return true;
      }
      std::string inferredVectorElemType;
      if (extractExperimentalVectorElementType(inferredReturn, inferredVectorElemType)) {
        typePathOut = "/vector";
        return true;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(inferredReturn.typeName);
      if (normalizedReturnType == "Pointer" || normalizedReturnType == "Reference") {
        return false;
      }
      if (!normalizedReturnType.empty()) {
        inferredDeclaredNonCollectionReturnType = true;
      }
    }
  }
  if (inferredDeclaredNonCollectionReturnType) {
    return false;
  }
  auto kindIt = returnKinds_.find(resolvedTarget);
  if (kindIt != returnKinds_.end()) {
    if (kindIt->second == ReturnKind::Array) {
      typePathOut = "/array";
      return true;
    }
    if (kindIt->second == ReturnKind::String) {
      typePathOut = "/string";
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveCallCollectionTemplateArgs(const Expr &target,
                                                           const std::string &expectedBase,
                                                           const std::vector<ParameterInfo> &params,
                                                           const std::unordered_map<std::string, BindingInfo> &locals,
                                                           std::vector<std::string> &argsOut) {
  argsOut.clear();
  auto explicitCallPath = [&](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.name.empty()) {
      return {};
    }
    std::string rawName = callExpr.name;
    if (!rawName.empty() && rawName.front() == '/') {
      return rawName;
    }
    std::string rawPrefix = callExpr.namespacePrefix;
    if (!rawPrefix.empty() && rawPrefix.front() != '/') {
      rawPrefix.insert(rawPrefix.begin(), '/');
    }
    if (rawPrefix.empty()) {
      return "/" + rawName;
    }
    return rawPrefix + "/" + rawName;
  };
  auto resolvedCallPath = [&](const Expr &callExpr) { return resolveCalleePath(callExpr); };
  auto extractCollectionArgsFromType =
      [&](const std::string &typeName, auto &&extractCollectionArgsFromTypeRef) -> bool {
    std::string base;
    std::string arg;
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == expectedBase ||
        (expectedBase == "vector" &&
         (base == "Vector" || base == "std/collections/experimental_vector/Vector")) ||
        (expectedBase == "soa_vector" && isExperimentalSoaVectorTypePath(base)) ||
        (expectedBase == "map" && isMapCollectionTypeName(base))) {
      return splitTopLevelTemplateArgs(arg, argsOut);
    }
    std::vector<std::string> args;
    if ((base == "Reference" || base == "Pointer") && splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
      return extractCollectionArgsFromTypeRef(args.front(), extractCollectionArgsFromTypeRef);
    }
    return false;
  };

  if (target.kind != Expr::Kind::Call) {
    return false;
  }
  const bool hasNamedTargetArgs = hasNamedArguments(target.argNames);

  auto extractCollectionArgsFromBinding = [&](const BindingInfo &binding) {
    if (expectedBase == "vector") {
      std::string elemType;
      if (extractExperimentalVectorElementType(binding, elemType)) {
        argsOut = {elemType};
        return true;
      }
    }
    if (expectedBase == "soa_vector") {
      std::string elemType;
      if (extractExperimentalSoaVectorElementType(binding, elemType)) {
        argsOut = {elemType};
        return true;
      }
    }
    if (expectedBase == "map") {
      std::string keyType;
      std::string valueType;
      if (extractMapKeyValueTypes(binding, keyType, valueType) ||
          extractInferExperimentalMapFieldTypes(binding, keyType, valueType)) {
        argsOut = {keyType, valueType};
        return true;
      }
    }
    const std::string typeText = binding.typeTemplateArg.empty()
                                     ? binding.typeName
                                     : binding.typeName + "<" + binding.typeTemplateArg + ">";
    return extractCollectionArgsFromType(typeText, extractCollectionArgsFromType);
  };
  auto isRootMapAliasPath = [](const std::string &path) {
    return path == "/map" ||
           path.rfind("/map__t", 0) == 0 ||
           path.rfind("/map__ov", 0) == 0;
  };

  std::string targetTypeText;
  bool inferredNonCollectionTargetType = false;
  if (inferQueryExprTypeText(target, params, locals, targetTypeText) &&
      extractCollectionArgsFromType(targetTypeText, extractCollectionArgsFromType)) {
    return true;
  }
  inferredNonCollectionTargetType = !targetTypeText.empty();

  const bool hasVisibleCanonicalMapConstructor =
      hasVisibleDefinitionPathForCurrentImports("/std/collections/map/map");
  const bool allowRootMapConstructorAlias =
      hasVisibleCanonicalMapConstructor && !hasDeclaredDefinitionPath("/map");
  const std::string resolvedTarget = resolvedCallPath(target);
  const std::string explicitTarget = explicitCallPath(target);
  std::string collectionName;
  if (!inferredNonCollectionTargetType &&
      getBuiltinCollectionName(target, collectionName) && collectionName == expectedBase) {
    const size_t expectedArgCount = expectedBase == "map" ? 2u : 1u;
    if (target.templateArgs.size() == expectedArgCount &&
        (expectedBase != "map" ||
         ((!isRootMapAliasPath(resolvedTarget) && !isRootMapAliasPath(explicitTarget)) ||
          allowRootMapConstructorAlias))) {
      argsOut = target.templateArgs;
      return true;
    }
  }

  if (expectedBase == "vector" &&
      !hasNamedTargetArgs &&
      !inferredNonCollectionTargetType &&
      (isResolvedVectorConstructorHelperPath(resolvedTarget) ||
       isResolvedVectorConstructorHelperPath(explicitTarget)) &&
      target.templateArgs.size() == 1) {
    argsOut = target.templateArgs;
    return true;
  }

  if (expectedBase == "map" &&
      ((isDirectMapConstructorPath(resolvedTarget) &&
        (!isRootMapAliasPath(resolvedTarget) || allowRootMapConstructorAlias)) ||
       (isDirectMapConstructorPath(explicitTarget) &&
        (!isRootMapAliasPath(explicitTarget) || allowRootMapConstructorAlias))) &&
      target.templateArgs.size() == 2) {
    argsOut = target.templateArgs;
    return true;
  }

  for (const std::string *candidatePath : {&resolvedTarget, &explicitTarget}) {
    if (candidatePath->empty()) {
      continue;
    }
    auto defIt = defMap_.find(*candidatePath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      continue;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
        extractCollectionArgsFromBinding(inferredReturn)) {
      return true;
    }
  }

  return false;
}

} // namespace primec::semantics
