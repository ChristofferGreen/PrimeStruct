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

#include "StdlibCollectionSurfaceHelpers.h"
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
    if ((base == "array" || base == "vector" || base == "soa" "_vector") && args.size() == 1) {
      return "/" + base;
    }
    if (isExperimentalSoaVectorTypePath(base) && args.size() == 1) {
      return "/soa" "_vector";
    }
    if (base == "Buffer" && args.size() == 1) {
      return "/Buffer";
    }
    if ((base == "Vector" ||
         isLegacyExperimentalVectorCompatibilityPath("/" + base)) &&
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
      if (extractCollectionVectorElementType(inferredReturn, inferredVectorElemType)) {
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
         (base == "Vector" ||
          isLegacyExperimentalVectorCompatibilityPath("/" + base))) ||
        (expectedBase == "soa" "_vector" && isExperimentalSoaVectorTypePath(base)) ||
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
      if (extractCollectionVectorElementType(binding, elemType)) {
        argsOut = {elemType};
        return true;
      }
    }
    if (expectedBase == "soa" "_vector") {
      std::string elemType;
      if (extractExperimentalSoaVectorElementType(binding, elemType)) {
        argsOut = {elemType};
        return true;
      }
    }
    if (expectedBase == "map") {
      std::string keyType;
      std::string valueType;
      if (extractKeyValueCollectionTypes(binding, keyType, valueType) ||
          extractInferExperimentalKeyValueFieldTypes(binding, keyType, valueType)) {
        argsOut = {keyType, valueType};
        return true;
      }
    }
    const std::string typeText = binding.typeTemplateArg.empty()
                                     ? binding.typeName
                                     : binding.typeName + "<" + binding.typeTemplateArg + ">";
    return extractCollectionArgsFromType(typeText, extractCollectionArgsFromType);
  };
  auto isRootKeyValueAliasPath = [](const std::string &path) {
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

  const std::string resolvedTarget = resolvedCallPath(target);
  const std::string explicitTarget = explicitCallPath(target);
  const bool targetIsRootKeyValueAlias =
      isRootKeyValueAliasPath(resolvedTarget) ||
      isRootKeyValueAliasPath(explicitTarget);
  std::string collectionName;
  if (!inferredNonCollectionTargetType &&
      getBuiltinCollectionName(target, collectionName) && collectionName == expectedBase) {
    const size_t expectedArgCount = expectedBase == "map" ? 2u : 1u;
    if (target.templateArgs.size() == expectedArgCount &&
        (expectedBase != "map" || !targetIsRootKeyValueAlias)) {
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
      !targetIsRootKeyValueAlias &&
      (isDirectMapConstructorPath(resolvedTarget) ||
       isDirectMapConstructorPath(explicitTarget)) &&
      target.templateArgs.size() == 2) {
    argsOut = target.templateArgs;
    return true;
  }
  if (expectedBase == "map" &&
      !targetIsRootKeyValueAlias &&
      (isDirectMapConstructorPath(resolvedTarget) ||
       isDirectMapConstructorPath(explicitTarget)) &&
      target.templateArgs.empty() && !target.args.empty() &&
      target.args.size() % 2 == 0) {
    auto inferArgTypeText = [&](const Expr &arg, std::string &typeTextOut) {
      BindingInfo inferredBinding;
      if (!inferBindingTypeFromInitializer(arg, params, locals, inferredBinding) &&
          !tryInferBindingTypeFromInitializer(arg, params, locals, inferredBinding,
                                             hasAnyMathImport())) {
        return false;
      }
      if (inferredBinding.typeName.empty()) {
        return false;
      }
      typeTextOut = inferredBinding.typeTemplateArg.empty()
                        ? inferredBinding.typeName
                        : inferredBinding.typeName + "<" + inferredBinding.typeTemplateArg + ">";
      return true;
    };
    std::string keyType;
    std::string valueType;
    if (!inferArgTypeText(target.args[0], keyType) ||
        !inferArgTypeText(target.args[1], valueType)) {
      return false;
    }
    for (size_t i = 2; i + 1 < target.args.size(); i += 2) {
      std::string nextKeyType;
      std::string nextValueType;
      if (!inferArgTypeText(target.args[i], nextKeyType) ||
          !inferArgTypeText(target.args[i + 1], nextValueType) ||
          normalizeBindingTypeName(nextKeyType) != normalizeBindingTypeName(keyType) ||
          normalizeBindingTypeName(nextValueType) != normalizeBindingTypeName(valueType)) {
        return false;
      }
    }
    argsOut = {keyType, valueType};
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
