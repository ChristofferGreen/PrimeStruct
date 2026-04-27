#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

#include <optional>

namespace primec::semantics {
namespace {
std::string bindingTypeText(const BindingInfo &binding) {
  return binding.typeTemplateArg.empty() ? binding.typeName
                                         : binding.typeName + "<" + binding.typeTemplateArg + ">";
}

bool extractBuiltinSoaVectorElementTypeFromTypeText(const std::string &typeText,
                                                    std::string &elemTypeOut) {
  elemTypeOut.clear();
  const std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if (base == "soa_vector" && !argText.empty()) {
    elemTypeOut = argText;
    return true;
  }
  if ((base != "Reference" && base != "Pointer") || argText.empty()) {
    return false;
  }
  std::string wrappedBase;
  std::string wrappedArgText;
  if (!splitTemplateTypeName(argText, wrappedBase, wrappedArgText)) {
    return false;
  }
  wrappedBase = normalizeBindingTypeName(wrappedBase);
  if (wrappedBase != "soa_vector" || wrappedArgText.empty()) {
    return false;
  }
  elemTypeOut = wrappedArgText;
  return true;
}
} // namespace
bool SemanticsValidator::inferCollectionBindingFromExpr(const Expr &expr,
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
  auto isImportedExperimentalVectorConstructorPath =
      [&](std::string resolvedPath) {
        resolvedPath = canonicalizeResolvedPath(std::move(resolvedPath));
        return resolvedPath == "/std/collections/experimental_vector/vector" ||
               (resolvedPath == "/vector" &&
                hasDirectExperimentalVectorImport());
      };
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
  std::string resolvedCollectionPath = preferredCollectionHelperResolvedPath(expr);
  if (resolvedCollectionPath.empty()) {
    resolvedCollectionPath = resolveCalleePath(expr);
  }
  if (!resolvedCollectionPath.empty()) {
    const std::string concreteResolvedCollectionPath =
        resolveExprConcreteCallPath(params, locals, expr, resolvedCollectionPath);
    if (!concreteResolvedCollectionPath.empty()) {
      resolvedCollectionPath = concreteResolvedCollectionPath;
    }
  }
  if (expr.templateArgs.size() == 1 &&
      (isImportedExperimentalVectorConstructorPath(resolvedCollectionPath) ||
       (expr.name == "vector" &&
        expr.namespacePrefix.empty() &&
        hasDirectExperimentalVectorImport()))) {
    bindingOut.typeName = "Vector";
    bindingOut.typeTemplateArg = expr.templateArgs.front();
    return true;
  }
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
  auto defIt = defMap_.find(resolvedCollectionPath);
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
  std::string resolvedCallPath = preferredCollectionHelperResolvedPath(expr);
  if (resolvedCallPath.empty()) {
    resolvedCallPath = resolveCalleePath(expr);
  }
  if (!resolvedCallPath.empty()) {
    const std::string concreteResolvedCallPath =
        resolveExprConcreteCallPath(params, locals, expr, resolvedCallPath);
    if (!concreteResolvedCallPath.empty()) {
      resolvedCallPath = concreteResolvedCallPath;
    }
  }
  const bool isCountLike =
      expr.args.size() == 1 &&
      (isSimpleCallName(expr, "count") ||
       isSimpleCallName(expr, "count_ref") ||
       isSimpleCallName(expr, "capacity") ||
       isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, "count") ||
       isLegacyOrCanonicalSoaHelperPath(resolvedCallPath, "count_ref") ||
       (resolvedCallPath == "/std/collections/map/count" &&
        defMap_.find("/std/collections/map/count") != defMap_.end()) ||
       (resolvedCallPath == "/std/collections/map/count_ref" &&
        defMap_.find("/std/collections/map/count_ref") != defMap_.end()) ||
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
                                                     BindingInfo &bindingOut,
                                                     const Expr *bindingExpr) {
  if (initializer.kind != Expr::Kind::Call) return false;
  const Expr *initializerExprForInference = &initializer;
  auto isResolvedMapConstructorPath = [](std::string resolvedPath) {
    return ::isResolvedPublishedMapConstructorPath(std::move(resolvedPath));
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
  auto inferRawBuiltinSoaCanonicalToAosBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.args.size() != 1) {
      return false;
    }
    std::string resolvedPath = preferredCollectionHelperResolvedPath(candidate);
    if (resolvedPath.empty()) {
      resolvedPath = resolveCalleePath(candidate);
    }
    if (!resolvedPath.empty()) {
      const std::string concreteResolvedPath =
          resolveExprConcreteCallPath(params, locals, candidate, resolvedPath);
      if (!concreteResolvedPath.empty()) {
        resolvedPath = concreteResolvedPath;
      }
    }
    if (resolvedPath == "/to_aos" || resolvedPath == "/to_aos_ref") {
      return false;
    }
    const std::string resolvedCanonical =
        canonicalizeLegacySoaToAosHelperPath(resolvedPath);
    if (resolvedCanonical != "/std/collections/soa_vector/to_aos" &&
        resolvedCanonical != "/std/collections/soa_vector/to_aos_ref") {
      return false;
    }
    std::string receiverTypeText;
    if (!inferQueryExprTypeText(candidate.args.front(), params, locals, receiverTypeText)) {
      return false;
    }
    std::string elemType;
    if (!extractBuiltinSoaVectorElementTypeFromTypeText(receiverTypeText, elemType) ||
        elemType.empty()) {
      return false;
    }
    bindingOut.typeName = "/std/collections/experimental_vector/Vector";
    bindingOut.typeTemplateArg = elemType;
    return true;
  };
  auto isUnresolvedActiveInferenceCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string resolvedPath = preferredCollectionHelperResolvedPath(candidate);
    if (resolvedPath.empty()) {
      resolvedPath = resolveCalleePath(candidate);
    }
    if (!resolvedPath.empty()) {
      const std::string concreteResolvedPath =
          resolveExprConcreteCallPath(params, locals, candidate, resolvedPath);
      if (!concreteResolvedPath.empty()) {
        resolvedPath = concreteResolvedPath;
      }
    }
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
  auto inferBuiltinSoaFieldViewBinding = [&](const Expr &candidate) -> bool {
    std::string fieldName;
    if (!isBuiltinSoaFieldViewExpr(candidate, params, locals, &fieldName) ||
        fieldName.empty() || candidate.args.empty()) {
      return false;
    }

    auto assignBindingTypeFromText = [](const std::string &typeText,
                                        BindingInfo &targetBinding) -> bool {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      if (normalizedType.empty()) {
        return false;
      }
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        targetBinding.typeName = normalizeBindingTypeName(base);
        targetBinding.typeTemplateArg = argText;
        return true;
      }
      targetBinding.typeName = normalizedType;
      targetBinding.typeTemplateArg.clear();
      return true;
    };
    auto inferReceiverBinding = [&](const Expr &receiver,
                                    BindingInfo &receiverBinding) -> bool {
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding =
                findParamBinding(params, receiver.name)) {
          receiverBinding = *paramBinding;
          return true;
        }
        auto localIt = locals.find(receiver.name);
        if (localIt != locals.end()) {
          receiverBinding = localIt->second;
          return true;
        }
      }
      if (inferBindingTypeFromInitializer(receiver, params, locals,
                                          receiverBinding)) {
        return true;
      }
      std::string receiverTypeText;
      return inferQueryExprTypeText(receiver, params, locals, receiverTypeText) &&
             assignBindingTypeFromText(receiverTypeText, receiverBinding);
    };
    auto resolveDirectReceiver = [&](const Expr &receiver,
                                     std::string &elemTypeOut) -> bool {
      BindingInfo receiverBinding;
      if (!inferReceiverBinding(receiver, receiverBinding)) {
        return false;
      }
      if (normalizeBindingTypeName(receiverBinding.typeName) == "soa_vector" &&
          !receiverBinding.typeTemplateArg.empty()) {
        elemTypeOut = receiverBinding.typeTemplateArg;
        return true;
      }
      return extractExperimentalSoaVectorElementType(receiverBinding,
                                                     elemTypeOut) &&
             !elemTypeOut.empty();
    };

    std::string elemType;
    if (!resolveSoaVectorOrExperimentalBorrowedReceiver(
            candidate.args.front(), params, locals, resolveDirectReceiver,
            elemType) ||
        elemType.empty()) {
      return false;
    }

    std::string currentNamespace;
    if (!currentValidationState_.context.definitionPath.empty()) {
      const size_t slash =
          currentValidationState_.context.definitionPath.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace =
            currentValidationState_.context.definitionPath.substr(0, slash);
      }
    }
    const std::string lookupNamespace =
        !candidate.args.front().namespacePrefix.empty()
            ? candidate.args.front().namespacePrefix
            : currentNamespace;
    const std::string elementStructPath = resolveStructTypePath(
        normalizeBindingTypeName(elemType), lookupNamespace, structNames_);
    auto structIt = defMap_.find(elementStructPath);
    if (elementStructPath.empty() || structIt == defMap_.end() ||
        structIt->second == nullptr) {
      return false;
    }

    for (const auto &fieldStmt : structIt->second->statements) {
      bool isStaticField = false;
      for (const auto &transform : fieldStmt.transforms) {
        if (transform.name == "static") {
          isStaticField = true;
          break;
        }
      }
      if (!fieldStmt.isBinding || isStaticField || fieldStmt.name != fieldName) {
        continue;
      }
      BindingInfo fieldBinding;
      std::optional<std::string> restrictType;
      std::string fieldError;
      if (!parseBindingInfo(fieldStmt,
                            structIt->second->namespacePrefix,
                            structNames_,
                            importAliases_,
                            fieldBinding,
                            restrictType,
                            fieldError)) {
        return false;
      }
      bindingOut.typeName = "SoaFieldView";
      bindingOut.typeTemplateArg = bindingTypeText(fieldBinding);
      return true;
    }
    return false;
  };
  if (inferBuiltinSoaFieldViewBinding(initializer)) {
    return true;
  }
  std::string graphPreferredResolvedInitializer;
  ReturnKind graphPreferredDirectCallReturnKind = ReturnKind::Unknown;
  const bool graphDirectCallFactAvailable =
      bindingExpr != nullptr &&
      lookupGraphLocalAutoDirectCallFact(
          *bindingExpr, graphPreferredResolvedInitializer, graphPreferredDirectCallReturnKind);
  const bool hasGraphPreferredResolvedInitializer =
      graphDirectCallFactAvailable && !graphPreferredResolvedInitializer.empty();
  std::string graphPreferredMethodResolvedInitializer;
  ReturnKind graphPreferredMethodCallReturnKind = ReturnKind::Unknown;
  const bool graphMethodCallFactAvailable =
      bindingExpr != nullptr &&
      !shouldBypassGraphBindingLookup(*bindingExpr) &&
      lookupGraphLocalAutoMethodCallFact(
          *bindingExpr, graphPreferredMethodResolvedInitializer, graphPreferredMethodCallReturnKind);
  const bool hasGraphPreferredMethodResolvedInitializer =
      graphMethodCallFactAvailable && !graphPreferredMethodResolvedInitializer.empty();
  std::string preferredResolvedInitializer = hasGraphPreferredMethodResolvedInitializer
                                                 ? graphPreferredMethodResolvedInitializer
                                             : hasGraphPreferredResolvedInitializer
                                                 ? graphPreferredResolvedInitializer
                                                 : preferredCollectionHelperResolvedPath(initializer);
  if (!preferredResolvedInitializer.empty()) {
    const std::string concretePreferredResolvedInitializer =
        resolveExprConcreteCallPath(
            params, locals, initializer, preferredResolvedInitializer);
    if (!concretePreferredResolvedInitializer.empty()) {
      preferredResolvedInitializer = concretePreferredResolvedInitializer;
    }
    if (inferRawBuiltinSoaCanonicalToAosBinding(initializer)) {
      return true;
    }
    if (inferResolvedDirectCallBindingType(preferredResolvedInitializer, bindingOut)) {
      return true;
    }
    if (inferDeclaredDirectCallBinding(preferredResolvedInitializer)) {
      return true;
    }
  }
  if (initializer.isMethodCall && !initializer.args.empty() && !initializer.name.empty()) {
    if (hasGraphPreferredMethodResolvedInitializer) {
      if (inferResolvedDirectCallBindingType(graphPreferredMethodResolvedInitializer, bindingOut)) {
        return true;
      }
      if (inferDeclaredDirectCallBinding(graphPreferredMethodResolvedInitializer)) {
        return true;
      }
    }
    std::string resolvedMethodInitializer;
    bool methodBuiltin = false;
    if (resolveMethodTarget(params,
                            locals,
                            initializer.namespacePrefix,
                            initializer.args.front(),
                            initializer.name,
                            resolvedMethodInitializer,
                            methodBuiltin) &&
        !resolvedMethodInitializer.empty()) {
      const std::string concreteResolvedMethodInitializer =
          resolveExprConcreteCallPath(
              params, locals, initializer, resolvedMethodInitializer);
      if (!concreteResolvedMethodInitializer.empty()) {
        resolvedMethodInitializer = concreteResolvedMethodInitializer;
      }
      if (inferResolvedDirectCallBindingType(resolvedMethodInitializer, bindingOut)) {
        return true;
      }
      if (inferDeclaredDirectCallBinding(resolvedMethodInitializer)) {
        return true;
      }
    }
    if (graphMethodCallFactAvailable &&
        graphPreferredMethodCallReturnKind != ReturnKind::Unknown &&
        graphPreferredMethodCallReturnKind != ReturnKind::Void &&
        graphPreferredMethodCallReturnKind != ReturnKind::Array) {
      const std::string inferredType = typeNameForReturnKind(graphPreferredMethodCallReturnKind);
      if (!inferredType.empty()) {
        bindingOut.typeName = inferredType;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
    }
  }

  std::string preferredResolvedInferencePath;
  if (initializerExprForInference != nullptr) {
    preferredResolvedInferencePath =
        preferredCollectionHelperResolvedPath(*initializerExprForInference);
    if (preferredResolvedInferencePath.empty()) {
      preferredResolvedInferencePath = resolveCalleePath(*initializerExprForInference);
    }
    if (!preferredResolvedInferencePath.empty()) {
      const std::string concreteResolvedInferencePath =
          resolveExprConcreteCallPath(
              params, locals, *initializerExprForInference, preferredResolvedInferencePath);
      if (!concreteResolvedInferencePath.empty()) {
        preferredResolvedInferencePath = concreteResolvedInferencePath;
      }
    }
  }
  if (initializerExprForInference != nullptr &&
      !isUnresolvedActiveInferenceCall(*initializerExprForInference)) {
    if (inferRawBuiltinSoaCanonicalToAosBinding(*initializerExprForInference)) {
      return true;
    }
    std::string builtinCollectionName;
    const bool isBuiltinCollectionCall =
        getBuiltinCollectionName(*initializerExprForInference, builtinCollectionName);
    std::string resolvedInferencePath = preferredResolvedInferencePath;
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
    std::string resolvedInferencePath = preferredResolvedInferencePath;
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
      if (!resolvedInferencePath.empty() &&
          inferResolvedDirectCallBindingType(resolvedInferencePath, resolvedCallBinding)) {
        bindingOut = std::move(resolvedCallBinding);
      }
    }
    return true;
  }
  ReturnKind resolvedKind = inferExprReturnKind(*initializerExprForInference, params, locals);
  if (resolvedKind != ReturnKind::Unknown && resolvedKind != ReturnKind::Void) {
    if (resolvedKind == ReturnKind::Array) {
      if (!hasGraphPreferredResolvedInitializer) {
        if (!preferredResolvedInferencePath.empty() &&
            inferResolvedDirectCallBindingType(preferredResolvedInferencePath, bindingOut)) {
          return true;
        }
        if (!preferredResolvedInferencePath.empty() &&
            inferDeclaredDirectCallBinding(preferredResolvedInferencePath)) {
          return true;
        }
      }
    }
    if (!hasGraphPreferredResolvedInitializer) {
      std::string fallbackPreferredResolvedInitializerPath =
          preferredCollectionHelperResolvedPath(initializer);
      if (fallbackPreferredResolvedInitializerPath.empty()) {
        fallbackPreferredResolvedInitializerPath = resolveCalleePath(initializer);
      }
      if (!fallbackPreferredResolvedInitializerPath.empty()) {
        const std::string concreteFallbackPreferredResolvedInitializerPath =
            resolveExprConcreteCallPath(
                params, locals, initializer, fallbackPreferredResolvedInitializerPath);
        if (!concreteFallbackPreferredResolvedInitializerPath.empty()) {
          fallbackPreferredResolvedInitializerPath =
              concreteFallbackPreferredResolvedInitializerPath;
        }
      }
      const std::string &preferredResolvedInitializerPath =
          !preferredResolvedInferencePath.empty()
              ? preferredResolvedInferencePath
              : fallbackPreferredResolvedInitializerPath;
      if (!preferredResolvedInitializerPath.empty() &&
          inferResolvedDirectCallBindingType(preferredResolvedInitializerPath, bindingOut)) {
        return true;
      }
      if (!preferredResolvedInitializerPath.empty() &&
          inferDeclaredDirectCallBinding(preferredResolvedInitializerPath)) {
        return true;
      }
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
  if (!hasGraphPreferredResolvedInitializer &&
      !preferredResolvedInferencePath.empty() &&
      inferResolvedDirectCallBindingType(preferredResolvedInferencePath, bindingOut)) {
    return true;
  }
  if (graphDirectCallFactAvailable &&
      graphPreferredDirectCallReturnKind != ReturnKind::Unknown &&
      graphPreferredDirectCallReturnKind != ReturnKind::Void &&
      graphPreferredDirectCallReturnKind != ReturnKind::Array) {
    const std::string inferredType = typeNameForReturnKind(graphPreferredDirectCallReturnKind);
    if (!inferredType.empty()) {
      bindingOut.typeName = inferredType;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
  }
  if (!hasGraphPreferredResolvedInitializer &&
      !preferredResolvedInferencePath.empty() &&
      inferDeclaredDirectCallBinding(preferredResolvedInferencePath)) {
    return true;
  }
  return false;
}

} // namespace primec::semantics
