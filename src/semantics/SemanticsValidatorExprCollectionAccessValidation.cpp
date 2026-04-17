#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {
namespace {

bool isCanonicalMapAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

bool isCanonicalMapAccessResolvedPath(const std::string &path) {
  return path == "/map/at" || path == "/map/at_ref" ||
         path == "/map/at_unsafe" || path == "/map/at_unsafe_ref" ||
         path == "/std/collections/map/at" ||
         path == "/std/collections/map/at_ref" ||
         path == "/std/collections/map/at_unsafe" ||
         path == "/std/collections/map/at_unsafe_ref";
}

bool getCanonicalCollectionAccessBuiltinName(const Expr &candidate,
                                             std::string &helperOut) {
  helperOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
    return false;
  }
  if (getBuiltinArrayAccessName(candidate, helperOut)) {
    return true;
  }
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (normalizedName == "map/at_ref" ||
      normalizedName == "std/collections/map/at_ref") {
    helperOut = "at_ref";
    return true;
  }
  if (normalizedName == "map/at_unsafe_ref" ||
      normalizedName == "std/collections/map/at_unsafe_ref") {
    helperOut = "at_unsafe_ref";
    return true;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
    namespacePrefix.erase(namespacePrefix.begin());
  }
  if ((namespacePrefix == "map" ||
       namespacePrefix == "std/collections/map") &&
      isCanonicalMapAccessHelperName(candidate.name)) {
    helperOut = candidate.name;
    return true;
  }
  if (candidate.name.find('/') == std::string::npos &&
      isCanonicalMapAccessHelperName(candidate.name)) {
    helperOut = candidate.name;
    return true;
  }
  return false;
}

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

bool isExperimentalMapTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base == "Map" || base == "/Map" ||
        base == "std/collections/experimental_map/Map" ||
        base == "/std/collections/experimental_map/Map") {
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

bool getRemovedVectorAccessBuiltinName(const Expr &candidate, std::string &helperOut) {
  helperOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
    return false;
  }
  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized == "vector/at") {
    helperOut = "at";
    return true;
  }
  if (normalized == "vector/at_unsafe") {
    helperOut = "at_unsafe";
    return true;
  }
  return false;
}

} // namespace

bool SemanticsValidator::validateExprCollectionAccessFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprCollectionAccessValidationContext &context,
    bool &handledOut) {
  handledOut = false;
  const bool resolvedMissing = defMap_.find(resolved) == defMap_.end();

  auto failCollectionAccessDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto failCollectionAccessMapKeyMismatch = [&](const std::string &helperName,
                                                const std::string &mapKeyType) {
    const bool isExplicitCanonicalMapAccessCall =
        !expr.isMethodCall &&
        (isCanonicalMapAccessResolvedPath(resolved) ||
         expr.name.rfind("/std/collections/map/", 0) == 0 ||
         expr.namespacePrefix == "/std/collections/map" ||
         expr.namespacePrefix == "std/collections/map");
    if (isExplicitCanonicalMapAccessCall) {
      return failCollectionAccessDiagnostic("argument type mismatch for " +
                                            resolved + " parameter key");
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      return failCollectionAccessDiagnostic(helperName +
                                            " requires string map key");
    }
    return failCollectionAccessDiagnostic(helperName +
                                          " requires map key type " +
                                          mapKeyType);
  };

  auto returnKindForBinding = [](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };
  auto isIntegerExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
        kind == ReturnKind::String || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral ||
          arg.kind == Expr::Kind::BoolLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                 paramKind == ReturnKind::UInt64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                 localKind == ReturnKind::UInt64;
        }
      }
      return true;
    }
    return false;
  };
  auto isStringExpr = [&](const Expr &arg) -> bool {
    if (context.resolveStringTarget != nullptr && context.resolveStringTarget(arg)) {
      return true;
    }
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    return kind == ReturnKind::String || arg.kind == Expr::Kind::StringLiteral;
  };
  auto validateMapKeyExpr = [&](const std::string &helperName,
                                const Expr &keyExpr,
                                const std::string &mapKeyType) -> bool {
    if (mapKeyType.empty()) {
      return true;
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      if (!isStringExpr(keyExpr)) {
        return failCollectionAccessMapKeyMismatch(helperName, mapKeyType);
      }
      return true;
    }
    ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
    if (keyKind == ReturnKind::Unknown) {
      return true;
    }
    if (context.resolveStringTarget != nullptr &&
        context.resolveStringTarget(keyExpr)) {
      return failCollectionAccessMapKeyMismatch(helperName, mapKeyType);
    }
    ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
    if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
      return failCollectionAccessMapKeyMismatch(helperName, mapKeyType);
    }
    return true;
  };

  auto validateMethodMapAccessBuiltin = [&](const std::string &helperName) -> bool {
    if (!expr.templateArgs.empty()) {
      return failCollectionAccessDiagnostic(helperName +
                                            " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCollectionAccessDiagnostic(helperName +
                                            " does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failCollectionAccessDiagnostic("argument count mismatch for builtin " +
                                            helperName);
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    std::string mapKeyType;
    if (!(context.resolveMapKeyType != nullptr &&
          context.resolveMapKeyType(expr.args.front(), mapKeyType))) {
      return failCollectionAccessDiagnostic(helperName + " requires map target");
    }
    if (!validateMapKeyExpr(helperName, expr.args[1], mapKeyType)) {
      return false;
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    return true;
  };

  if (expr.isMethodCall && resolvedMethod &&
      isCanonicalMapAccessResolvedPath(resolved)) {
    handledOut = true;
    const std::string helperName =
        resolved.find("unsafe_ref") != std::string::npos
            ? "at_unsafe_ref"
            : (resolved.find("unsafe") != std::string::npos
                   ? "at_unsafe"
                   : (resolved.find("at_ref") != std::string::npos ? "at_ref"
                                                                   : "at"));
    return validateMethodMapAccessBuiltin(helperName);
  }

  if (!resolvedMethod && resolvedMissing &&
      context.isStdNamespacedVectorAccessCall &&
      context.hasStdNamespacedVectorAccessDefinition) {
    std::string builtinName;
    if (getBuiltinArrayAccessName(expr, builtinName) && expr.args.size() == 2) {
      size_t receiverIndex = 0;
      if (hasNamedArguments(expr.argNames)) {
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
      std::string elemType;
      const bool isBuiltinVectorReceiver =
          receiverIndex < expr.args.size() &&
          context.resolveVectorTarget != nullptr &&
          context.resolveVectorTarget(expr.args[receiverIndex], elemType);
      const bool isExperimentalVectorReceiver =
          receiverIndex < expr.args.size() &&
          context.resolveExperimentalVectorValueTarget != nullptr &&
          context.resolveExperimentalVectorValueTarget(expr.args[receiverIndex], elemType);
      if (!isBuiltinVectorReceiver && !isExperimentalVectorReceiver) {
        return failCollectionAccessDiagnostic(
            "argument type mismatch for /std/collections/vector/" + builtinName);
      }
    }
  }

  if (!resolvedMethod && resolvedMissing &&
      !(context.isStdNamespacedVectorAccessCall &&
        hasNamedArguments(expr.argNames))) {
    std::string removedVectorAccessBuiltinName;
    if (getRemovedVectorAccessBuiltinName(expr, removedVectorAccessBuiltinName)) {
      if (hasNamedArguments(expr.argNames)) {
        return failCollectionAccessDiagnostic(
            "named arguments not supported for builtin calls");
      }
      if (!expr.templateArgs.empty()) {
        return failCollectionAccessDiagnostic(
            removedVectorAccessBuiltinName + " does not accept template arguments");
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        return failCollectionAccessDiagnostic(
            removedVectorAccessBuiltinName + " does not accept block arguments");
      }
      if (expr.args.size() != 2) {
        return failCollectionAccessDiagnostic(
            "argument count mismatch for builtin " + removedVectorAccessBuiltinName);
      }
    }
  }

  std::string builtinName;
  if (getCanonicalCollectionAccessBuiltinName(expr, builtinName) &&
      (!context.isStdNamespacedVectorAccessCall ||
       context.shouldAllowStdAccessCompatibilityFallback ||
       context.hasStdNamespacedVectorAccessDefinition) &&
      (!context.isStdNamespacedMapAccessCall ||
       context.hasStdNamespacedMapAccessDefinition) &&
      !(context.isStdNamespacedVectorAccessCall &&
        hasNamedArguments(expr.argNames))) {
    if (!context.shouldBuiltinValidateBareMapAccessCall) {
      Expr rewrittenMapHelperCall;
      if (context.tryRewriteBareMapHelperCall != nullptr &&
          context.tryRewriteBareMapHelperCall(expr, builtinName,
                                             rewrittenMapHelperCall)) {
        return validateExpr(params, locals, rewrittenMapHelperCall);
      }
    }
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      return failCollectionAccessDiagnostic(
          "named arguments not supported for builtin calls");
    }
    if (!expr.templateArgs.empty()) {
      return failCollectionAccessDiagnostic(builtinName +
                                            " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failCollectionAccessDiagnostic(builtinName +
                                            " does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failCollectionAccessDiagnostic("argument count mismatch for builtin " +
                                            builtinName);
    }
    std::string experimentalMapKeyType;
    std::string experimentalMapValueType;
    auto isRootMapAliasPath = [](const std::string &path) {
      return path == "/map" || path.rfind("/map__", 0) == 0;
    };
    auto explicitCallPath = [](const Expr &candidate) {
      if (candidate.name.empty() || candidate.name.front() == '/') {
        return candidate.name;
      }
      if (candidate.namespacePrefix.empty()) {
        return "/" + candidate.name;
      }
      return candidate.namespacePrefix + "/" + candidate.name;
    };
    if (!expr.isMethodCall &&
        context.resolveExperimentalMapTarget != nullptr &&
        ((context.resolveExperimentalMapTarget(
              expr.args.front(), experimentalMapKeyType,
              experimentalMapValueType)) ||
         (context.resolveExperimentalMapTarget(
             expr.args[1], experimentalMapKeyType,
             experimentalMapValueType)))) {
      handledOut = true;
      return failCollectionAccessDiagnostic(
          "unknown call target: /std/collections/map/" + builtinName);
    }
    auto isExperimentalMapTypeReceiver = [&](const Expr &candidate) {
      if (candidate.kind == Expr::Kind::Call &&
          (isRootMapAliasPath(resolveCalleePath(candidate)) ||
           isRootMapAliasPath(explicitCallPath(candidate)))) {
        return false;
      }
      std::string receiverTypeText;
      return inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
             isExperimentalMapTypeText(receiverTypeText);
    };
    if (!expr.isMethodCall &&
        (isExperimentalMapTypeReceiver(expr.args.front()) ||
         isExperimentalMapTypeReceiver(expr.args[1]))) {
      handledOut = true;
      return failCollectionAccessDiagnostic(
          "unknown call target: /std/collections/map/" + builtinName);
    }
    if (!expr.isMethodCall &&
        context.isMapLikeBareAccessReceiverTarget != nullptr &&
        (context.isMapLikeBareAccessReceiverTarget(expr.args.front()) ||
         context.isMapLikeBareAccessReceiverTarget(expr.args[1]))) {
      return true;
    }

    size_t indexArgIndex = 1;
    std::string elemType;
    auto isArrayOrStringTarget = [&](const Expr &candidate, std::string &elemTypeOut) {
      if ((context.resolveArgsPackAccessTarget != nullptr &&
           context.resolveArgsPackAccessTarget(candidate, elemTypeOut)) ||
          (context.resolveVectorTarget != nullptr &&
           context.resolveVectorTarget(candidate, elemTypeOut)) ||
          (context.resolveExperimentalVectorValueTarget != nullptr &&
           context.resolveExperimentalVectorValueTarget(candidate, elemTypeOut)) ||
          (context.resolveArrayTarget != nullptr &&
           context.resolveArrayTarget(candidate, elemTypeOut)) ||
          (context.resolveStringTarget != nullptr &&
           context.resolveStringTarget(candidate))) {
        return true;
      }
      std::string builtinCollection;
      if (getBuiltinCollectionName(candidate, builtinCollection) &&
          (builtinCollection == "array" || builtinCollection == "vector") &&
          candidate.templateArgs.size() == 1) {
        elemTypeOut = candidate.templateArgs.front();
        return true;
      }
      return false;
    };
    std::string mapKeyType;
    std::string mapValueType;
    auto isMapTarget = [&](const Expr &candidate, std::string &mapKeyTypeOut) {
      if (context.resolveMapKeyType != nullptr &&
          context.resolveMapKeyType(candidate, mapKeyTypeOut)) {
        return true;
      }
      std::string builtinCollection;
      if (getBuiltinCollectionName(candidate, builtinCollection) &&
          builtinCollection == "map" &&
          candidate.templateArgs.size() == 2) {
        mapKeyTypeOut = candidate.templateArgs.front();
        return true;
      }
      return false;
    };
    bool isArrayOrString = isArrayOrStringTarget(expr.args.front(), elemType);
    bool isMap = isMapTarget(expr.args.front(), mapKeyType);
    bool isExperimentalMap =
        context.resolveExperimentalMapTarget != nullptr &&
        context.resolveExperimentalMapTarget(expr.args.front(), mapKeyType,
                                             mapValueType);
    const bool shouldProbeReorderedReceiver =
        expr.args.size() == 2 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name && !isArrayOrString &&
          !isMap && !isExperimentalMap));
    if (!isArrayOrString && !isMap && !isExperimentalMap &&
        shouldProbeReorderedReceiver) {
      std::string reorderedElemType;
      std::string reorderedMapKeyType;
      std::string reorderedMapValueType;
      const bool reorderedArrayOrString = isArrayOrStringTarget(expr.args[1], reorderedElemType);
      const bool reorderedMap = isMapTarget(expr.args[1], reorderedMapKeyType);
      const bool reorderedExperimentalMap =
          context.resolveExperimentalMapTarget != nullptr &&
          context.resolveExperimentalMapTarget(expr.args[1], reorderedMapKeyType,
                                               reorderedMapValueType);
      if (reorderedArrayOrString || reorderedMap || reorderedExperimentalMap) {
        indexArgIndex = 0;
        elemType = reorderedElemType;
        mapKeyType = reorderedMapKeyType;
        mapValueType = reorderedMapValueType;
        isArrayOrString = reorderedArrayOrString;
        isMap = reorderedMap;
        isExperimentalMap = reorderedExperimentalMap;
      }
    }
    const Expr &receiverExpr = expr.args[indexArgIndex == 0 ? 1 : 0];
    std::string vectorElemType;
    const bool isDirectVectorReceiver =
        context.resolveVectorTarget != nullptr &&
        context.resolveVectorTarget(receiverExpr, vectorElemType);
    std::string experimentalVectorElemType;
    const bool isDirectExperimentalVectorReceiver =
        context.resolveExperimentalVectorValueTarget != nullptr &&
        context.resolveExperimentalVectorValueTarget(receiverExpr, experimentalVectorElemType);
    std::string receiverBuiltinCollection;
    if (getBuiltinCollectionName(receiverExpr, receiverBuiltinCollection)) {
      if (receiverBuiltinCollection == "map" &&
          receiverExpr.templateArgs.size() == 2) {
        mapKeyType = receiverExpr.templateArgs[0];
        mapValueType = receiverExpr.templateArgs[1];
        isMap = true;
        isExperimentalMap = false;
      } else if ((receiverBuiltinCollection == "array" ||
                  receiverBuiltinCollection == "vector") &&
                 receiverExpr.templateArgs.size() == 1) {
        elemType = receiverExpr.templateArgs.front();
        isArrayOrString = true;
      }
    }
    if (!isMap && !isExperimentalMap) {
      std::string receiverTypeText;
      if (!(receiverExpr.kind == Expr::Kind::Call &&
            (isRootMapAliasPath(resolveCalleePath(receiverExpr)) ||
             isRootMapAliasPath(explicitCallPath(receiverExpr)))) &&
          inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
          extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyType, mapValueType)) {
        if (isExperimentalMapTypeText(receiverTypeText)) {
          isExperimentalMap = true;
        } else if (isCanonicalMapTypeText(receiverTypeText)) {
          isMap = true;
        }
      }
    }
    const bool isExplicitMapAccessHelper =
        isCanonicalMapAccessResolvedPath(resolved);
    if (!expr.templateArgs.empty() &&
        (isMap || isExperimentalMap || isExplicitMapAccessHelper)) {
      return true;
    }
    handledOut = true;
    const bool isStdlibVectorAccessWrapperDefinition =
        currentValidationState_.context.definitionPath.rfind("/std/collections/", 0) == 0 ||
        currentValidationState_.context.definitionPath.rfind("/std/collections/experimental_vector/", 0) == 0 ||
        currentValidationState_.context.definitionPath.rfind("/std/image/", 0) == 0;
    if (!expr.isMethodCall &&
        (isDirectVectorReceiver || isDirectExperimentalVectorReceiver) &&
        !isStdlibVectorAccessWrapperDefinition &&
        !hasDeclaredDefinitionPath("/std/collections/vector/" + builtinName) &&
        !hasImportedDefinitionPath("/std/collections/vector/" + builtinName)) {
      return failCollectionAccessDiagnostic(
          "unknown call target: /std/collections/vector/" + builtinName);
    }
    if (!isArrayOrString && !isMap && !isExperimentalMap) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (resolveMethodTarget(params, locals, expr.namespacePrefix,
                              expr.args.front(), builtinName, methodResolved,
                              isBuiltinMethod) &&
          !isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
          context.isNonCollectionStructAccessTarget != nullptr &&
          context.isNonCollectionStructAccessTarget(methodResolved)) {
        return failCollectionAccessDiagnostic("unknown method: " + methodResolved);
      }
      if (expr.args.front().kind == Expr::Kind::Call || expr.args.front().kind == Expr::Kind::Name) {
        std::string receiverStructPath;
        if (expr.args.front().kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, expr.args.front().name)) {
            receiverStructPath = resolveTypePath(paramBinding->typeName, expr.args.front().namespacePrefix);
          } else if (auto it = locals.find(expr.args.front().name); it != locals.end()) {
            receiverStructPath = resolveTypePath(it->second.typeName, expr.args.front().namespacePrefix);
          }
        } else {
          receiverStructPath = inferStructReturnPath(expr.args.front(), params, locals);
          if (receiverStructPath.empty()) {
            auto defIt = defMap_.find(resolveCalleePath(expr.args.front()));
            if (defIt != defMap_.end() && defIt->second != nullptr) {
              BindingInfo inferredReturn;
              if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
                receiverStructPath = resolveTypePath(inferredReturn.typeName, expr.args.front().namespacePrefix);
              }
            }
          }
        }
        if (!receiverStructPath.empty() && receiverStructPath.front() != '/') {
          receiverStructPath.insert(receiverStructPath.begin(), '/');
        }
        if (!receiverStructPath.empty() && context.isNonCollectionStructAccessTarget != nullptr &&
            context.isNonCollectionStructAccessTarget(receiverStructPath + "/" + builtinName)) {
          return failCollectionAccessDiagnostic(
              "unknown method: " + receiverStructPath + "/" + builtinName);
        }
      }
      return failCollectionAccessDiagnostic(
          builtinName + " requires array, vector, map, or string target");
    }
    if ((isMap || isExperimentalMap) &&
        !context.shouldBuiltinValidateBareMapAccessCall &&
        !(context.isIndexedArgsPackMapReceiverTarget != nullptr &&
          context.isIndexedArgsPackMapReceiverTarget(expr.args.front())) &&
        !hasDeclaredDefinitionPath("/map/" + builtinName) &&
        !hasImportedDefinitionPath("/std/collections/map/" + builtinName) &&
        !hasDeclaredDefinitionPath("/std/collections/map/" + builtinName)) {
      return failCollectionAccessDiagnostic(
          "unknown call target: /std/collections/map/" + builtinName);
    }
    if (!isMap && !isExperimentalMap) {
      if (!isIntegerExpr(expr.args[indexArgIndex])) {
        return failCollectionAccessDiagnostic(
            builtinName + " requires integer index [collection]");
      }
    } else if (!validateMapKeyExpr(builtinName, expr.args[indexArgIndex], mapKeyType)) {
      return false;
    }

    for (const auto &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
