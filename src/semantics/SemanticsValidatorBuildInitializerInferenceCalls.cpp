#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"
namespace primec::semantics {
namespace {
std::string bindingTypeText(const BindingInfo &binding) {
  return binding.typeTemplateArg.empty() ? binding.typeName
                                         : binding.typeName + "<" + binding.typeTemplateArg + ">";
}
} // namespace
bool SemanticsValidator::inferCollectionBindingFromExpr(const Expr &expr,
                                                        const std::vector<ParameterInfo> &params,
                                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                                        BindingInfo &bindingOut) {
  auto copyNamedBinding = [&](const std::string &name) -> bool {
    if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
      bindingOut = *paramBinding;
      return true;
    }
    auto it = locals.find(name);
    if (it == locals.end()) {
      return false;
    }
    bindingOut = it->second;
    return true;
  };
  if (expr.kind == Expr::Kind::Name) {
    return copyNamedBinding(expr.name);
  }
  if (expr.kind != Expr::Kind::Call) return false;
  std::string collection;
  if (getBuiltinCollectionName(expr, collection)) {
    if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
        expr.templateArgs.size() == 1) {
      bindingOut.typeName = collection;
      bindingOut.typeTemplateArg = expr.templateArgs.front();
      return true;
    }
    if (collection == "map" && expr.templateArgs.size() == 2) {
      bindingOut.typeName = "map";
      bindingOut.typeTemplateArg = joinTemplateArgs(expr.templateArgs);
      return true;
    }
  }
  std::string resolvedCollectionPath = resolveCalleePath(expr);
  const size_t collectionSuffix = resolvedCollectionPath.find("__t");
  if (collectionSuffix != std::string::npos) resolvedCollectionPath.erase(collectionSuffix);
  if (resolvedCollectionPath == "/std/collections/vector/vector" &&
      expr.templateArgs.size() == 1) {
    bindingOut.typeName = "vector";
    bindingOut.typeTemplateArg = expr.templateArgs.front();
    return true;
  }
  std::string namespacedCollection;
  std::string namespacedHelper;
  if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
      namespacedCollection == "vector" &&
      namespacedHelper == "vector" &&
      expr.templateArgs.size() == 1) {
    bindingOut.typeName = "vector";
    bindingOut.typeTemplateArg = expr.templateArgs.front();
    return true;
  }
  auto defIt = defMap_.find(resolveCalleePath(expr));
  if (defIt == defMap_.end() || defIt->second == nullptr) {
    return false;
  }
  for (const auto &transform : defIt->second->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedReturnType, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args)) {
      return false;
    }
    if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
      bindingOut.typeName = base;
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    if (isMapCollectionTypeName(base) && args.size() == 2) {
      bindingOut.typeName = base;
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
      bindingOut.typeName = base;
      bindingOut.typeTemplateArg = args.front();
      return true;
    }
    return false;
  }
  return false;
}
bool SemanticsValidator::inferBuiltinCollectionValueBinding(const Expr &expr,
                                                            const std::vector<ParameterInfo> &params,
                                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                                            BindingInfo &bindingOut) {
  auto inferArrayElementType = [&](const BindingInfo &binding, std::string &elemTypeOut) {
    elemTypeOut.clear();
    if ((binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "soa_vector") &&
        !binding.typeTemplateArg.empty()) {
      elemTypeOut = binding.typeTemplateArg;
      return true;
    }
    if ((binding.typeName == "Reference" || binding.typeName == "Pointer") &&
        !binding.typeTemplateArg.empty()) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(binding.typeTemplateArg, base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      if ((base == "array" || base == "vector" || base == "soa_vector") && !argText.empty()) {
        elemTypeOut = argText;
        return true;
      }
    }
    return false;
  };
  if (expr.kind != Expr::Kind::Call) return false;
  std::string builtinAccessName;
  if (getBuiltinArrayAccessName(expr, builtinAccessName) && expr.args.size() == 2) {
    BindingInfo collectionBinding;
    if (!inferCollectionBindingFromExpr(expr.args.front(), params, locals, bindingOut)) {
      return false;
    }
    collectionBinding = bindingOut;
    std::string keyType;
    std::string valueType;
    if (extractMapKeyValueTypes(collectionBinding, keyType, valueType)) {
      bindingOut.typeName = normalizeBindingTypeName(valueType);
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    std::string elemType;
    if (inferArrayElementType(collectionBinding, elemType)) {
      bindingOut.typeName = normalizeBindingTypeName(elemType);
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    return false;
  }
  const std::string resolvedCallPath = resolveCalleePath(expr);
  const bool isCountLike =
      expr.args.size() == 1 &&
      (isSimpleCallName(expr, "count") ||
       isSimpleCallName(expr, "capacity") ||
       (resolvedCallPath == "/std/collections/map/count" &&
        defMap_.find("/std/collections/map/count") != defMap_.end()) ||
       resolvedCallPath == "/std/collections/vector/count" ||
       resolvedCallPath == "/std/collections/vector/capacity");
  const bool isMapContainsLike =
      !expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
      (currentValidationState_.context.definitionPath == "/std/collections/mapContains" ||
       currentValidationState_.context.definitionPath == "/std/collections/mapTryAt");
  if (isMapContainsLike) {
    BindingInfo collectionBinding;
    if (!inferCollectionBindingFromExpr(expr.args.front(), params, locals, bindingOut)) {
      return false;
    }
    collectionBinding = bindingOut;
    std::string keyType;
    std::string valueType;
    if (!extractMapKeyValueTypes(collectionBinding, keyType, valueType)) {
      return false;
    }
    bindingOut.typeName = "bool";
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  if (!isCountLike) {
    return false;
  }
  BindingInfo collectionBinding;
  if (!inferCollectionBindingFromExpr(expr.args.front(), params, locals, bindingOut)) {
    return false;
  }
  collectionBinding = bindingOut;
  std::string keyType;
  std::string valueType;
  std::string elemType;
  if (extractMapKeyValueTypes(collectionBinding, keyType, valueType) ||
      inferArrayElementType(collectionBinding, elemType) ||
      collectionBinding.typeName == "string") {
    bindingOut.typeName = "i32";
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  return false;
}
bool SemanticsValidator::inferBuiltinPointerBinding(const Expr &expr,
                                                    const std::vector<ParameterInfo> &params,
                                                    const std::unordered_map<std::string, BindingInfo> &locals,
                                                    BindingInfo &bindingOut) {
  std::function<bool(const Expr &, std::string &)> resolvePointerTargetType;
  resolvePointerTargetType = [&](const Expr &candidate, std::string &targetOut) -> bool {
    auto formatBindingType = [](const BindingInfo &binding) -> std::string {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    if (candidate.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
        if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
            !paramBinding->typeTemplateArg.empty()) {
          targetOut = paramBinding->typeTemplateArg;
          return true;
        }
        return false;
      }
      auto it = locals.find(candidate.name);
      if (it == locals.end()) {
        return false;
      }
      if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
          !it->second.typeTemplateArg.empty()) {
        targetOut = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (getBuiltinPointerName(candidate, builtinName) && builtinName == "location" &&
        candidate.args.size() == 1) {
      const Expr &target = candidate.args.front();
      if (target.kind != Expr::Kind::Name) {
        return false;
      }
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
          targetOut = paramBinding->typeTemplateArg;
        } else {
          targetOut = formatBindingType(*paramBinding);
        }
        return true;
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
        targetOut = it->second.typeTemplateArg;
      } else {
        targetOut = formatBindingType(it->second);
      }
      return true;
    }
    if (getBuiltinMemoryName(candidate, builtinName)) {
      if (builtinName == "alloc" && candidate.templateArgs.size() == 1 && candidate.args.size() == 1) {
        targetOut = candidate.templateArgs.front();
        return true;
      }
      if (builtinName == "realloc" && candidate.args.size() == 2) {
        return resolvePointerTargetType(candidate.args.front(), targetOut);
      }
      if (builtinName == "at" && candidate.templateArgs.empty() && candidate.args.size() == 3) {
        return resolvePointerTargetType(candidate.args.front(), targetOut);
      }
      if (builtinName == "at_unsafe" && candidate.templateArgs.empty() && candidate.args.size() == 2) {
        return resolvePointerTargetType(candidate.args.front(), targetOut);
      }
      if (builtinName == "reinterpret" && candidate.templateArgs.size() == 1 && candidate.args.size() == 1) {
        targetOut = candidate.templateArgs.front();
        return true;
      }
    }
    std::string opName;
    if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
        candidate.args.size() == 2) {
      if (isPointerLikeExpr(candidate.args[1], params, locals)) {
        return false;
      }
      return resolvePointerTargetType(candidate.args.front(), targetOut);
    }
    return false;
  };
  if (expr.kind != Expr::Kind::Call) return false;
  std::string builtinName;
  std::string targetType;
  if (getBuiltinPointerName(expr, builtinName) && builtinName == "location") {
    if (expr.args.size() != 1 || !resolvePointerTargetType(expr, targetType)) {
      return false;
    }
  } else if (getBuiltinMemoryName(expr, builtinName)) {
    if (builtinName == "alloc") {
      if (expr.templateArgs.size() != 1 || expr.args.size() != 1) {
        return false;
      }
      targetType = expr.templateArgs.front();
    } else if (builtinName == "realloc") {
      if (expr.args.size() != 2 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "at") {
      if (!expr.templateArgs.empty() || expr.args.size() != 3 ||
          !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "at_unsafe") {
      if (!expr.templateArgs.empty() || expr.args.size() != 2 ||
          !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "reinterpret") {
      if (expr.templateArgs.size() != 1 || expr.args.size() != 1) {
        return false;
      }
      targetType = expr.templateArgs.front();
    } else {
      return false;
    }
  } else {
    std::string opName;
    if (!getBuiltinOperatorName(expr, opName) || (opName != "plus" && opName != "minus") ||
        expr.args.size() != 2) {
      return false;
    }
    if (isPointerLikeExpr(expr.args[1], params, locals)) {
      return false;
    }
    if (!resolvePointerTargetType(expr.args.front(), targetType)) {
      return false;
    }
  }
  if (targetType.empty()) {
    return false;
  }
  bindingOut.typeName = "Pointer";
  bindingOut.typeTemplateArg = targetType;
  return true;
}
bool SemanticsValidator::inferCallInitializerBinding(const Expr &initializer,
                                                     const std::vector<ParameterInfo> &params,
                                                     const std::unordered_map<std::string, BindingInfo> &locals,
                                                     BindingInfo &bindingOut) {
  if (initializer.kind != Expr::Kind::Call) return false;
  Expr initializerForInference = initializer;
  const Expr *initializerExprForInference = &initializer;
  std::string namespacedCollection;
  std::string namespacedHelper;
  const std::string resolvedInitializer = resolveCalleePath(initializer);
  auto isResolvedMapConstructorPath = [](std::string resolvedPath) {
    resolvedPath = stripMapConstructorSuffixes(std::move(resolvedPath));
    return resolvedPath == "/std/collections/map/map" ||
           resolvedPath == "/std/collections/mapNew" ||
           resolvedPath == "/std/collections/mapSingle" ||
           resolvedPath == "/std/collections/mapPair" ||
           resolvedPath == "/std/collections/mapDouble" ||
           resolvedPath == "/std/collections/mapTriple" ||
           resolvedPath == "/std/collections/mapQuad" ||
           resolvedPath == "/std/collections/mapQuint" ||
           resolvedPath == "/std/collections/mapSext" ||
           resolvedPath == "/std/collections/mapSept" ||
           resolvedPath == "/std/collections/mapOct" ||
           resolvedPath == "/std/collections/experimental_map/mapNew" ||
           resolvedPath == "/std/collections/experimental_map/mapSingle" ||
           resolvedPath == "/std/collections/experimental_map/mapPair" ||
           resolvedPath == "/std/collections/experimental_map/mapDouble" ||
           resolvedPath == "/std/collections/experimental_map/mapTriple" ||
           resolvedPath == "/std/collections/experimental_map/mapQuad" ||
           resolvedPath == "/std/collections/experimental_map/mapQuint" ||
           resolvedPath == "/std/collections/experimental_map/mapSext" ||
           resolvedPath == "/std/collections/experimental_map/mapSept" ||
           resolvedPath == "/std/collections/experimental_map/mapOct";
  };
  auto inferDeclaredDirectCallBinding = [&](const std::string &resolvedPath) -> bool {
    const auto defIt = defMap_.find(resolvedPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      if (normalizedReturnType.empty() || normalizedReturnType == "auto") {
        return false;
      }
      if (normalizedReturnType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
        bindingOut.typeName = "/" + normalizedReturnType;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      const std::string resolvedStructPath =
          resolveStructTypePath(normalizedReturnType, defIt->second->namespacePrefix, structNames_);
      if (!resolvedStructPath.empty()) {
        bindingOut.typeName = resolvedStructPath;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedReturnType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      bindingOut.typeName = normalizedReturnType;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    return false;
  };
  auto hasImportedInitializerDefinitionPath = [&](const std::string &path) {
    std::string canonicalPath = path;
    const size_t suffix = canonicalPath.find("__t");
    if (suffix != std::string::npos) {
      canonicalPath.erase(suffix);
    }
    for (const auto &importPath : program_.imports) {
      if (importPath == canonicalPath) {
        return true;
      }
      if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
        const std::string prefix = importPath.substr(0, importPath.size() - 2);
        if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
          return true;
        }
      }
    }
    return false;
  };
  const bool isUnresolvedStdNamespacedVectorCountCapacityCall =
      initializer.kind == Expr::Kind::Call &&
      !initializer.isMethodCall &&
      getNamespacedCollectionHelperName(initializer, namespacedCollection, namespacedHelper) &&
      namespacedCollection == "vector" &&
      (namespacedHelper == "count" || namespacedHelper == "capacity") &&
      resolvedInitializer == "/std/collections/vector/" + namespacedHelper &&
      defMap_.find(resolvedInitializer) == defMap_.end() &&
      !hasImportedInitializerDefinitionPath(resolvedInitializer);
  if (isUnresolvedStdNamespacedVectorCountCapacityCall) {
    initializerForInference.name = "/vector/" + namespacedHelper;
    initializerForInference.namespacePrefix.clear();
    initializerExprForInference = &initializerForInference;
  }
  auto isUnresolvedActiveInferenceCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    const std::string resolvedPath = resolveCalleePath(candidate);
    if (resolvedPath.empty()) {
      return false;
    }
    BindingInfo resolvedBinding;
    if (inferResolvedDirectCallBindingType(resolvedPath, resolvedBinding)) {
      return false;
    }
    return returnBindingInferenceStack_.contains(resolvedPath) ||
           inferenceStack_.contains(resolvedPath) ||
           queryTypeInferenceDefinitionStack_.contains(resolvedPath);
  };
  auto preferredResolvedInitializerPath = [&]() -> std::string {
    auto normalizedName = initializer.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    auto normalizedPrefix = initializer.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }

    auto explicitStdMapHelperName = [&]() -> std::string {
      if (normalizedPrefix == "std/collections/map" &&
          (normalizedName == "count" || normalizedName == "contains" ||
           normalizedName == "tryAt" || normalizedName == "at" ||
           normalizedName == "at_unsafe" || normalizedName == "insert")) {
        return normalizedName;
      }
      if (normalizedName.rfind("std/collections/map/", 0) == 0) {
        const std::string helperName =
            normalizedName.substr(std::string("std/collections/map/").size());
        if (helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
            helperName == "at" || helperName == "at_unsafe" ||
            helperName == "insert") {
          return helperName;
        }
      }
      return {};
    };
    auto explicitStdVectorHelperName = [&]() -> std::string {
      if (normalizedPrefix == "std/collections/vector" &&
          (normalizedName == "at" || normalizedName == "at_unsafe" ||
           normalizedName == "count" || normalizedName == "capacity" ||
           normalizedName == "push" || normalizedName == "pop" ||
           normalizedName == "reserve" || normalizedName == "clear" ||
           normalizedName == "remove_at" || normalizedName == "remove_swap")) {
        return normalizedName;
      }
      if (normalizedName.rfind("std/collections/vector/", 0) == 0) {
        const std::string helperName =
            normalizedName.substr(std::string("std/collections/vector/").size());
        if (helperName == "at" || helperName == "at_unsafe" ||
            helperName == "count" || helperName == "capacity" ||
            helperName == "push" || helperName == "pop" ||
            helperName == "reserve" || helperName == "clear" ||
            helperName == "remove_at" || helperName == "remove_swap") {
          return helperName;
        }
      }
      return {};
    };
    if (const std::string helperName = explicitStdMapHelperName(); !helperName.empty()) {
      const std::string canonical = "/std/collections/map/" + helperName;
      const bool prefersAlias = helperName == "at" || helperName == "at_unsafe";
      const std::string alias = "/map/" + helperName;
      if (prefersAlias && defMap_.count(alias) > 0) {
        return alias;
      }
      if (defMap_.count(canonical) > 0) {
        return canonical;
      }
      if (defMap_.count(alias) > 0) {
        return alias;
      }
    }
    if (const std::string helperName = explicitStdVectorHelperName(); !helperName.empty()) {
      const std::string alias = "/vector/" + helperName;
      if (defMap_.count(alias) > 0) {
        return alias;
      }
      const std::string canonical = "/std/collections/vector/" + helperName;
      if (defMap_.count(canonical) > 0) {
        return canonical;
      }
    }
    return {};
  };
  const std::string preferredResolvedInitializer = preferredResolvedInitializerPath();
  if (!preferredResolvedInitializer.empty()) {
    if (inferResolvedDirectCallBindingType(preferredResolvedInitializer, bindingOut)) {
      return true;
    }
    if (inferDeclaredDirectCallBinding(preferredResolvedInitializer)) {
      return true;
    }
  }
  if (initializer.isMethodCall && !initializer.args.empty() && !initializer.name.empty()) {
    std::string resolvedMethodTarget;
    bool isBuiltinMethod = false;
    if (resolveMethodTarget(params,
                            locals,
                            initializer.namespacePrefix,
                            initializer.args.front(),
                            initializer.name,
                            resolvedMethodTarget,
                            isBuiltinMethod)) {
      if (inferResolvedDirectCallBindingType(resolvedMethodTarget, bindingOut)) {
        return true;
      }
      if (inferDeclaredDirectCallBinding(resolvedMethodTarget)) {
        return true;
      }
    }
  }
  if (initializerExprForInference != nullptr &&
      !isUnresolvedActiveInferenceCall(*initializerExprForInference)) {
    std::string builtinCollectionName;
    const bool isBuiltinCollectionCall =
        getBuiltinCollectionName(*initializerExprForInference, builtinCollectionName);
    std::string resolvedInferencePath = resolveCalleePath(*initializerExprForInference);
    const size_t suffix = resolvedInferencePath.find("__t");
    if (suffix != std::string::npos) {
      resolvedInferencePath.erase(suffix);
    }
    if (!isBuiltinCollectionCall && !resolvedInferencePath.empty()) {
      BindingInfo resolvedCollectionBinding;
      if (inferResolvedDirectCallBindingType(resolvedInferencePath, resolvedCollectionBinding) &&
          !normalizeCollectionTypePath(bindingTypeText(resolvedCollectionBinding)).empty()) {
        bindingOut = std::move(resolvedCollectionBinding);
        return true;
      }
    }
    std::string directStructReturn = inferStructReturnPath(*initializerExprForInference, params, locals);
    if (!directStructReturn.empty() && normalizeCollectionTypePath(directStructReturn).empty()) {
      bindingOut.typeName = directStructReturn;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
  }
  std::string inferredTypeText;
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
  if (initializerExprForInference != nullptr &&
      !isUnresolvedActiveInferenceCall(*initializerExprForInference) &&
      inferQueryExprTypeText(*initializerExprForInference, params, locals, inferredTypeText) &&
      assignBindingTypeFromText(inferredTypeText)) {
    const std::string resolvedInferencePath = resolveCalleePath(*initializerExprForInference);
    if (isResolvedMapConstructorPath(resolvedInferencePath)) {
      BindingInfo resolvedCallBinding;
      if (inferResolvedDirectCallBindingType(resolvedInferencePath, resolvedCallBinding)) {
        bindingOut = std::move(resolvedCallBinding);
        return true;
      }
      std::vector<std::string> mapArgs;
      if (resolveCallCollectionTemplateArgs(*initializerExprForInference, "map", params, locals, mapArgs) &&
          mapArgs.size() == 2) {
        bindingOut.typeName = "Map";
        bindingOut.typeTemplateArg = joinTemplateArgs(mapArgs);
        return true;
      }
    }
    const std::string normalizedBindingType = normalizeBindingTypeName(bindingOut.typeName);
    const bool shouldPreferResolvedDirectCallBinding =
        ((!bindingOut.typeTemplateArg.empty() && isMapCollectionTypeName(normalizedBindingType)) ||
         (bindingOut.typeTemplateArg.empty() &&
          !normalizedBindingType.empty() &&
          normalizedBindingType.front() != '/' &&
          returnKindForTypeName(normalizedBindingType) == ReturnKind::Unknown));
    if (shouldPreferResolvedDirectCallBinding) {
      BindingInfo resolvedCallBinding;
      if (inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), resolvedCallBinding)) {
        bindingOut = std::move(resolvedCallBinding);
      }
    }
    return true;
  }
  ReturnKind resolvedKind = inferExprReturnKind(*initializerExprForInference, params, locals);
  if (resolvedKind != ReturnKind::Unknown && resolvedKind != ReturnKind::Void) {
    if (resolvedKind == ReturnKind::Array) {
      if (inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), bindingOut)) {
        return true;
      }
      if (inferDeclaredDirectCallBinding(resolveCalleePath(*initializerExprForInference))) {
        return true;
      }
    }
    if (inferResolvedDirectCallBindingType(resolveCalleePath(initializer), bindingOut)) {
      return true;
    }
    if (inferDeclaredDirectCallBinding(resolveCalleePath(initializer))) {
      return true;
    }
    std::string inferredStruct = inferStructReturnPath(initializer, params, locals);
    if (!inferredStruct.empty()) {
      const std::string normalizedCollectionType = normalizeCollectionTypePath(inferredStruct);
      if (!normalizedCollectionType.empty()) {
        const bool keepExperimentalCollectionPath =
            inferredStruct == "/std/collections/experimental_vector/Vector" ||
            inferredStruct.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
            inferredStruct.rfind("/std/collections/experimental_map/Map__", 0) == 0;
        if (keepExperimentalCollectionPath) {
          bindingOut.typeName = inferredStruct;
          bindingOut.typeTemplateArg.clear();
          return true;
        }
        if (initializerExprForInference != nullptr &&
            !isUnresolvedActiveInferenceCall(*initializerExprForInference)) {
          std::string recoveredTypeText;
          if (inferQueryExprTypeText(*initializerExprForInference, params, locals, recoveredTypeText) &&
              assignBindingTypeFromText(recoveredTypeText)) {
            return true;
          }
        }
        bindingOut.typeName = normalizedCollectionType.substr(1);
        bindingOut.typeTemplateArg.clear();
        if (initializerExprForInference != nullptr) {
          std::vector<std::string> collectionArgs;
          if (resolveCallCollectionTemplateArgs(*initializerExprForInference,
                                               bindingOut.typeName,
                                               params,
                                               locals,
                                               collectionArgs) &&
              !collectionArgs.empty()) {
            bindingOut.typeTemplateArg = joinTemplateArgs(collectionArgs);
            return true;
          }
        }
        std::string collectionName;
        if (initializerExprForInference != nullptr &&
            getBuiltinCollectionName(*initializerExprForInference, collectionName)) {
          if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
              initializerExprForInference->templateArgs.size() == 1) {
            bindingOut.typeName = collectionName;
            bindingOut.typeTemplateArg = initializerExprForInference->templateArgs.front();
          } else if (collectionName == "map" && initializerExprForInference->templateArgs.size() == 2) {
            bindingOut.typeName = "map";
            bindingOut.typeTemplateArg = joinTemplateArgs(initializerExprForInference->templateArgs);
          }
        }
        return true;
      }
      bindingOut.typeName = inferredStruct;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (resolvedKind == ReturnKind::Array) {
      return false;
    }
    std::string inferredType = typeNameForReturnKind(resolvedKind);
    if (inferredType.empty()) {
      return false;
    }
    bindingOut.typeName = inferredType;
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  if (inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), bindingOut)) {
    return true;
  }
  if (inferDeclaredDirectCallBinding(resolveCalleePath(*initializerExprForInference))) {
    return true;
  }
  return false;
}

} // namespace primec::semantics
