#include "SemanticsValidator.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::semantics {
namespace {

bool isCanonicalKeyValueAccessHelperName(const std::string &helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

std::string canonicalKeyValueHelperPathLocal(std::string_view helperName) {
  return metadataBackedCanonicalKeyValueHelperPath(helperName);
}

std::string canonicalKeyValueHelperNamespaceLocal() {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return "";
  }
  std::string namespacePath(metadata->canonicalPath);
  if (!namespacePath.empty() && namespacePath.front() == '/') {
    namespacePath.erase(namespacePath.begin());
  }
  return namespacePath;
}

bool resolveCanonicalKeyValueHelperNameFromSpelling(
    std::string path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (!path.empty() && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  return resolvePublishedCollectionHelperResolvedPath(
      path, metadata->id, helperNameOut);
}

std::string rootedKeyValueHelperAliasPathLocal(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return {};
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    std::string root(alias);
    if (root.empty()) {
      continue;
    }
    if (root.front() != '/') {
      root.insert(root.begin(), '/');
    }
    std::string unrootedRoot = std::string(trimLeadingSlash(root));
    if (unrootedRoot.find('/') != std::string::npos) {
      continue;
    }
    return root + "/" + std::string(helperName);
  }
  return {};
}

bool isCanonicalKeyValueAccessResolvedPath(const std::string &path) {
  std::string helperName;
  return resolveCanonicalKeyValueHelperNameFromSpelling(path, helperName) &&
         isCanonicalKeyValueAccessHelperName(helperName);
}

bool isSourceSpelledCanonicalKeyValueAccessCall(const Expr &expr) {
  const std::string &sourceName =
      expr.sourceName.empty() ? expr.name : expr.sourceName;
  return isCanonicalKeyValueAccessResolvedPath(sourceName);
}

bool isUnqualifiedCollectionAccessCall(const Expr &candidate,
                                       const std::string &helperName) {
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      !candidate.namespacePrefix.empty() || candidate.name.empty()) {
    return false;
  }
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (normalizedName.find('/') != std::string::npos) {
    return false;
  }
  std::string resolvedHelperName;
  return getBuiltinArrayAccessName(candidate, resolvedHelperName) &&
         resolvedHelperName == helperName;
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
  std::string resolvedKeyValueHelperName;
  if (resolveCanonicalKeyValueHelperNameFromSpelling(
          normalizedName, resolvedKeyValueHelperName) &&
      (resolvedKeyValueHelperName == "at_ref" ||
       resolvedKeyValueHelperName == "at_unsafe_ref")) {
    helperOut = resolvedKeyValueHelperName;
    return true;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
    namespacePrefix.erase(namespacePrefix.begin());
  }
  if (namespacePrefix == canonicalKeyValueHelperNamespaceLocal() &&
      isCanonicalKeyValueAccessHelperName(candidate.name)) {
    helperOut = candidate.name;
    return true;
  }
  if (candidate.name.find('/') == std::string::npos &&
      isCanonicalKeyValueAccessHelperName(candidate.name)) {
    helperOut = candidate.name;
    return true;
  }
  return false;
}

bool isExperimentalKeyValueTypeText(const std::string &typeText) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(normalizedType, base, arg)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    std::string normalizedBase = base;
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    const size_t leafStart = normalizedBase.find_last_of('/');
    const std::string leaf = leafStart == std::string::npos
                                 ? normalizedBase
                                 : normalizedBase.substr(leafStart + 1);
    if (leaf == "Map" &&
        isExperimentalCollectionBackingTypeName(
            "map", "Map", normalizedBase)) {
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
  auto failCollectionAccessKeyValueKeyMismatch = [&](const std::string &helperName,
                                                     const std::string &keyValueKeyType) {
    const bool hasSourceShape = !expr.sourceName.empty();
    const bool isExplicitCanonicalKeyValueAccessCall =
        expr.sourceIsMethodCall ||
        (!expr.isMethodCall &&
         (isSourceSpelledCanonicalKeyValueAccessCall(expr) ||
          (!hasSourceShape &&
           (isCanonicalKeyValueAccessResolvedPath(resolved) ||
            isCanonicalKeyValueAccessResolvedPath(expr.name) ||
            canonicalKeyValueHelperNamespaceLocal() ==
                trimLeadingSlash(expr.namespacePrefix)))));
    if (isExplicitCanonicalKeyValueAccessCall) {
      return failCollectionAccessDiagnostic("argument type mismatch for " +
                                            resolved + " parameter key");
    }
    if (normalizeBindingTypeName(keyValueKeyType) == "string") {
      return failCollectionAccessDiagnostic(helperName +
                                            " requires string map key");
    }
    return failCollectionAccessDiagnostic(helperName +
                                          " requires map key type " +
                                          keyValueKeyType);
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
  auto validateKeyValueKeyExpr = [&](const std::string &helperName,
                                     const Expr &keyExpr,
                                     const std::string &keyValueKeyType) -> bool {
    if (keyValueKeyType.empty()) {
      return true;
    }
    if (normalizeBindingTypeName(keyValueKeyType) == "string") {
      if (!isStringExpr(keyExpr)) {
        return failCollectionAccessKeyValueKeyMismatch(helperName, keyValueKeyType);
      }
      return true;
    }
    ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(keyValueKeyType));
    if (keyKind == ReturnKind::Unknown) {
      return true;
    }
    if (context.resolveStringTarget != nullptr &&
        context.resolveStringTarget(keyExpr)) {
      return failCollectionAccessKeyValueKeyMismatch(helperName, keyValueKeyType);
    }
    ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
    if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
      return failCollectionAccessKeyValueKeyMismatch(helperName, keyValueKeyType);
    }
    return true;
  };

  auto validateMethodKeyValueAccessBuiltin =
      [&](const std::string &helperName) -> bool {
    if (!expr.templateArgs.empty()) {
      std::string diagnosticTarget = resolved;
      return failCollectionAccessDiagnostic("unknown call target: " +
                                            diagnosticTarget);
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
    std::string keyValueKeyType;
    if (!(context.resolveKeyValueKeyType != nullptr &&
          context.resolveKeyValueKeyType(expr.args.front(), keyValueKeyType))) {
      return failCollectionAccessDiagnostic(helperName + " requires map target");
    }
    if (!validateKeyValueKeyExpr(helperName, expr.args[1], keyValueKeyType)) {
      return false;
    }
    if (!validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    return true;
  };

  if (expr.isMethodCall && resolvedMethod &&
      isCanonicalKeyValueAccessResolvedPath(resolved)) {
    handledOut = true;
    const std::string helperName =
        resolved.find("unsafe_ref") != std::string::npos
            ? "at_unsafe_ref"
            : (resolved.find("unsafe") != std::string::npos
                   ? "at_unsafe"
                   : (resolved.find("at_ref") != std::string::npos ? "at_ref"
                                                                   : "at"));
    return validateMethodKeyValueAccessBuiltin(helperName);
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
        const std::string canonicalVectorAccessPath =
            canonicalVectorCompatibilityHelperPathOrFallback(builtinName);
        return failCollectionAccessDiagnostic(
            "argument type mismatch for " + canonicalVectorAccessPath);
      }
    }
  }

  std::string builtinName;
  if (getCanonicalCollectionAccessBuiltinName(expr, builtinName) &&
      (!context.isStdNamespacedVectorAccessCall ||
       context.shouldAllowStdAccessCompatibilityFallback ||
       context.hasStdNamespacedVectorAccessDefinition) &&
      (!context.isStdNamespacedMapAccessCall ||
       context.hasStdNamespacedKeyValueAccessDefinition) &&
      !(context.isStdNamespacedVectorAccessCall &&
        hasNamedArguments(expr.argNames))) {
    if (!context.shouldBuiltinValidateBareKeyValueAccessCall) {
      Expr rewrittenKeyValueHelperCall;
      if (context.tryRewriteBareKeyValueHelperCall != nullptr &&
          context.tryRewriteBareKeyValueHelperCall(expr, builtinName,
                                             rewrittenKeyValueHelperCall)) {
        return validateExpr(params, locals, rewrittenKeyValueHelperCall);
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
      if (context.isStdNamespacedMapAccessCall ||
          isCanonicalKeyValueAccessResolvedPath(resolved)) {
        std::string diagnosticTarget = resolved;
        if (diagnosticTarget.empty()) {
          diagnosticTarget = canonicalKeyValueHelperPathLocal(builtinName);
        }
        return failCollectionAccessDiagnostic("unknown call target: " +
                                              diagnosticTarget);
      }
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
    std::string experimentalKeyValueKeyType;
    std::string experimentalKeyValueValueType;
    auto isRootKeyValueAliasPath = [](const std::string &path) {
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
    auto isRootKeyValueAliasExpr = [&](const Expr &candidate) {
      return candidate.kind == Expr::Kind::Call &&
             (isRootKeyValueAliasPath(resolveCalleePath(candidate)) ||
              isRootKeyValueAliasPath(explicitCallPath(candidate)));
    };
    auto resolvesNonRootExperimentalKeyValueTarget =
        [&](const Expr &candidate) {
          return !isRootKeyValueAliasExpr(candidate) &&
                 context.resolveExperimentalMapTarget(
                     candidate, experimentalKeyValueKeyType,
                     experimentalKeyValueValueType);
        };
    const bool isUnqualifiedAccessCall =
        isUnqualifiedCollectionAccessCall(expr, builtinName);
    if (isUnqualifiedAccessCall &&
        context.resolveExperimentalMapTarget != nullptr &&
        (resolvesNonRootExperimentalKeyValueTarget(expr.args.front()) ||
         resolvesNonRootExperimentalKeyValueTarget(expr.args[1]))) {
      handledOut = true;
      return failCollectionAccessDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal(builtinName));
    }
    auto isExperimentalKeyValueTypeReceiver = [&](const Expr &candidate) {
      if (isRootKeyValueAliasExpr(candidate)) {
        return false;
      }
      std::string receiverTypeText;
      return inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
             isExperimentalKeyValueTypeText(receiverTypeText);
    };
    if (isUnqualifiedAccessCall &&
        (isExperimentalKeyValueTypeReceiver(expr.args.front()) ||
         isExperimentalKeyValueTypeReceiver(expr.args[1]))) {
      handledOut = true;
      return failCollectionAccessDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal(builtinName));
    }
    if (!expr.isMethodCall &&
        context.isKeyValueLikeBareAccessReceiverTarget != nullptr &&
        (context.isKeyValueLikeBareAccessReceiverTarget(expr.args.front()) ||
         context.isKeyValueLikeBareAccessReceiverTarget(expr.args[1]))) {
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
    std::string keyValueKeyType;
    std::string keyValueValueType;
    auto isKeyValueTarget = [&](const Expr &candidate, std::string &keyValueKeyTypeOut) {
      if (context.resolveKeyValueKeyType != nullptr &&
          context.resolveKeyValueKeyType(candidate, keyValueKeyTypeOut)) {
        return true;
      }
      std::string builtinCollection;
      if (getBuiltinCollectionName(candidate, builtinCollection) &&
          builtinCollection == "map" &&
          candidate.templateArgs.size() == 2) {
        keyValueKeyTypeOut = candidate.templateArgs.front();
        return true;
      }
      return false;
    };
    bool isArrayOrString = isArrayOrStringTarget(expr.args.front(), elemType);
    if (!isArrayOrString) {
      isArrayOrString =
          resolveArgsPackElementTypeForExpr(expr.args.front(), params, locals,
                                            elemType);
    }
    bool isKeyValue = isKeyValueTarget(expr.args.front(), keyValueKeyType);
    bool isExperimentalKeyValue =
        context.resolveExperimentalMapTarget != nullptr &&
        context.resolveExperimentalMapTarget(expr.args.front(), keyValueKeyType,
                                             keyValueValueType);
    const bool shouldProbeReorderedReceiver =
        expr.args.size() == 2 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name && !isArrayOrString &&
          !isKeyValue && !isExperimentalKeyValue));
    if (!isArrayOrString && !isKeyValue && !isExperimentalKeyValue &&
        shouldProbeReorderedReceiver) {
      std::string reorderedElemType;
      std::string reorderedKeyValueKeyType;
      std::string reorderedKeyValueValueType;
      const bool reorderedArrayOrString = isArrayOrStringTarget(expr.args[1], reorderedElemType);
      const bool reorderedArgsPack =
          !reorderedArrayOrString &&
          resolveArgsPackElementTypeForExpr(expr.args[1], params, locals,
                                            reorderedElemType);
      const bool reorderedKeyValue = isKeyValueTarget(expr.args[1], reorderedKeyValueKeyType);
      const bool reorderedExperimentalKeyValue =
          context.resolveExperimentalMapTarget != nullptr &&
          context.resolveExperimentalMapTarget(expr.args[1], reorderedKeyValueKeyType,
                                               reorderedKeyValueValueType);
      if (reorderedArrayOrString || reorderedArgsPack || reorderedKeyValue ||
          reorderedExperimentalKeyValue) {
        indexArgIndex = 0;
        elemType = reorderedElemType;
        keyValueKeyType = reorderedKeyValueKeyType;
        keyValueValueType = reorderedKeyValueValueType;
        isArrayOrString = reorderedArrayOrString || reorderedArgsPack;
        isKeyValue = reorderedKeyValue;
        isExperimentalKeyValue = reorderedExperimentalKeyValue;
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
        keyValueKeyType = receiverExpr.templateArgs[0];
        keyValueValueType = receiverExpr.templateArgs[1];
        isKeyValue = true;
        isExperimentalKeyValue = false;
      } else if ((receiverBuiltinCollection == "array" ||
                  receiverBuiltinCollection == "vector") &&
                 receiverExpr.templateArgs.size() == 1) {
        elemType = receiverExpr.templateArgs.front();
        isArrayOrString = true;
      }
    }
    if (!isKeyValue && !isExperimentalKeyValue) {
      std::string receiverTypeText;
      if (!(receiverExpr.kind == Expr::Kind::Call &&
            (isRootKeyValueAliasPath(resolveCalleePath(receiverExpr)) ||
             isRootKeyValueAliasPath(explicitCallPath(receiverExpr)))) &&
          inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
          extractKeyValueCollectionTypesFromTypeText(receiverTypeText, keyValueKeyType, keyValueValueType)) {
        const bool isExperimentalKeyValueType = isExperimentalKeyValueTypeText(receiverTypeText);
        if (isExperimentalKeyValueType) {
          isExperimentalKeyValue = true;
        } else {
          isKeyValue = true;
        }
      }
    }
    const bool isExplicitKeyValueAccessHelper =
        isCanonicalKeyValueAccessResolvedPath(resolved);
    if (isExplicitKeyValueAccessHelper && !isKeyValue && !isExperimentalKeyValue) {
      handledOut = false;
      return true;
    }
    if (!expr.templateArgs.empty() &&
        (isKeyValue || isExperimentalKeyValue || isExplicitKeyValueAccessHelper)) {
      return true;
    }
    handledOut = true;
    const bool isStdlibVectorAccessWrapperDefinition =
        currentValidationState_.context.definitionPath.rfind("/std/collections/", 0) == 0 ||
        isLegacyExperimentalVectorCompatibilityPath(
            currentValidationState_.context.definitionPath) ||
        currentValidationState_.context.definitionPath.rfind("/std/image/", 0) == 0 ||
        currentValidationState_.context.definitionPath.rfind("/std/ui/", 0) == 0;
    const std::string canonicalVectorAccessPath =
        canonicalVectorCompatibilityHelperPathOrFallback(builtinName);
    if (!expr.isMethodCall &&
        (isDirectVectorReceiver || isDirectExperimentalVectorReceiver) &&
        !isStdlibVectorAccessWrapperDefinition &&
        !hasDeclaredDefinitionPath(canonicalVectorAccessPath) &&
        !hasImportedDefinitionPath(canonicalVectorAccessPath)) {
      return failCollectionAccessDiagnostic(
          "unknown call target: " + canonicalVectorAccessPath);
    }
    if (!isArrayOrString && !isKeyValue && !isExperimentalKeyValue) {
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
    if ((isKeyValue || isExperimentalKeyValue) &&
        !context.shouldBuiltinValidateBareKeyValueAccessCall &&
        !(context.isIndexedArgsPackKeyValueReceiverTarget != nullptr &&
          context.isIndexedArgsPackKeyValueReceiverTarget(expr.args.front())) &&
        !hasDeclaredDefinitionPath(rootedKeyValueHelperAliasPathLocal(builtinName)) &&
        !hasImportedDefinitionPath(canonicalKeyValueHelperPathLocal(builtinName)) &&
        !hasDeclaredDefinitionPath(canonicalKeyValueHelperPathLocal(builtinName))) {
      return failCollectionAccessDiagnostic(
          "unknown call target: " + canonicalKeyValueHelperPathLocal(builtinName));
    }
    if (!isKeyValue && !isExperimentalKeyValue) {
      if (!isIntegerExpr(expr.args[indexArgIndex])) {
        return failCollectionAccessDiagnostic(
            builtinName + " requires integer index [collection]");
      }
    } else if (!validateKeyValueKeyExpr(builtinName, expr.args[indexArgIndex], keyValueKeyType)) {
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
