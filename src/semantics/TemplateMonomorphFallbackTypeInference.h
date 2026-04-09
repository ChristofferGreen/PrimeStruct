#pragma once

bool isSoftwareNumericParamCompatible(ReturnKind expectedKind, ReturnKind actualKind) {
  switch (expectedKind) {
    case ReturnKind::Integer:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Integer;
    case ReturnKind::Decimal:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
             actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal;
    case ReturnKind::Complex:
      return actualKind == ReturnKind::Int || actualKind == ReturnKind::Int64 || actualKind == ReturnKind::UInt64 ||
             actualKind == ReturnKind::Bool || actualKind == ReturnKind::Float32 || actualKind == ReturnKind::Float64 ||
             actualKind == ReturnKind::Integer || actualKind == ReturnKind::Decimal ||
             actualKind == ReturnKind::Complex;
    default:
      return false;
  }
}

std::string resolveStructLikeTypePathForTemplatedVectorFallback(const std::string &typeName,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx) {
  std::string normalized = normalizeBindingTypeName(typeName);
  if (normalized.empty()) {
    return {};
  }
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
    normalized = base;
  }
  if (isPrimitiveBindingTypeName(normalized) || isSoftwareNumericTypeName(normalized) || normalized == "string" ||
      isBuiltinTemplateContainer(normalized)) {
    return {};
  }
  if (!normalized.empty() && normalized[0] == '/') {
    return ctx.sourceDefs.count(normalized) > 0 ? normalized : std::string{};
  }
  auto aliasIt = ctx.importAliases.find(normalized);
  if (aliasIt != ctx.importAliases.end() && ctx.sourceDefs.count(aliasIt->second) > 0) {
    return aliasIt->second;
  }
  std::string resolved = resolveTypePath(normalized, namespacePrefix);
  if (ctx.sourceDefs.count(resolved) > 0) {
    return resolved;
  }
  return {};
}

std::string resolveStructLikeExprPathForTemplatedVectorFallback(const Expr &expr,
                                                                const LocalTypeMap &locals,
                                                                const std::string &namespacePrefix,
                                                                const Context &ctx,
                                                                bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return {};
  }
  std::string resolved;
  if (expr.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(expr, locals, ctx, resolved)) {
      return {};
    }
  } else {
    resolved = resolveCalleePath(expr, namespacePrefix, ctx);
  }
  if (!expr.isMethodCall && expr.templateArgs.size() == 2) {
    const std::string experimentalPath = experimentalMapConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      return "/std/collections/experimental_map/Map<" + joinTemplateArgs(expr.templateArgs) + ">";
    }
    if (isExperimentalMapConstructorHelperPath(resolved)) {
      return "/std/collections/experimental_map/Map<" + joinTemplateArgs(expr.templateArgs) + ">";
    }
  }
  if (!expr.isMethodCall && expr.templateArgs.size() == 1) {
    const std::string experimentalPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      return "/std/collections/experimental_vector/Vector<" + joinTemplateArgs(expr.templateArgs) + ">";
    }
    if (isExperimentalVectorConstructorHelperPath(resolved)) {
      return "/std/collections/experimental_vector/Vector<" + joinTemplateArgs(expr.templateArgs) + ">";
    }
  }
  if (!expr.isMethodCall && expr.templateArgs.empty()) {
    const std::string experimentalPath = experimentalMapConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      const auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      expr,
                                      locals,
                                      {},
                                      SubstMap{},
                                      {},
                                      namespacePrefix,
                                      const_cast<Context &>(ctx),
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 2) {
          return "/std/collections/experimental_map/Map<" + joinTemplateArgs(inferredArgs) + ">";
        }
      }
    }
    if (isExperimentalMapConstructorHelperPath(resolved)) {
      const auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      expr,
                                      locals,
                                      {},
                                      SubstMap{},
                                      {},
                                      namespacePrefix,
                                      const_cast<Context &>(ctx),
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 2) {
          return "/std/collections/experimental_map/Map<" + joinTemplateArgs(inferredArgs) + ">";
        }
      }
    }
    const std::string experimentalVectorPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0) {
      const auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      expr,
                                      locals,
                                      {},
                                      SubstMap{},
                                      {},
                                      namespacePrefix,
                                      const_cast<Context &>(ctx),
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 1) {
          return "/std/collections/experimental_vector/Vector<" + joinTemplateArgs(inferredArgs) + ">";
        }
      }
    }
    if (isExperimentalVectorConstructorHelperPath(resolved)) {
      const auto defIt = ctx.sourceDefs.find(resolved);
      if (defIt != ctx.sourceDefs.end()) {
        std::vector<std::string> inferredArgs;
        std::string inferError;
        if (inferImplicitTemplateArgs(defIt->second,
                                      expr,
                                      locals,
                                      {},
                                      SubstMap{},
                                      {},
                                      namespacePrefix,
                                      const_cast<Context &>(ctx),
                                      allowMathBare,
                                      inferredArgs,
                                      inferError) &&
            inferredArgs.size() == 1) {
          return "/std/collections/experimental_vector/Vector<" + joinTemplateArgs(inferredArgs) + ">";
        }
      }
    }
  }
  const auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return {};
  }
  if (!expr.isMethodCall) {
    const std::string experimentalVectorPath = experimentalVectorConstructorInferencePath(resolved);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0) {
      for (const auto &transform : defIt->second.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string valueType;
        if (extractVectorValueTypeFromTypeText(transform.templateArgs.front(), valueType)) {
          return "/std/collections/experimental_vector/Vector<" + valueType + ">";
        }
      }
    }
    const std::string experimentalPath = experimentalMapConstructorInferencePath(resolved);
    if (!experimentalPath.empty() && ctx.sourceDefs.count(experimentalPath) > 0) {
      for (const auto &transform : defIt->second.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string keyType;
        std::string valueType;
        if (extractMapKeyValueTypesFromTypeText(transform.templateArgs.front(), keyType, valueType)) {
          return "/std/collections/experimental_map/Map<" + joinTemplateArgs({keyType, valueType}) + ">";
        }
      }
    }
  }
  if (isStructDefinition(defIt->second)) {
    return resolved;
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      continue;
    }
    return resolveStructLikeTypePathForTemplatedVectorFallback(returnType, defIt->second.namespacePrefix, ctx);
  }
  return {};
}

bool resolvesExperimentalMapValueTypeText(const std::string &typeText,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx) {
  if (typeText.empty()) {
    return false;
  }
  std::string normalizedInput = normalizeBindingTypeName(typeText);
  std::string inputBase;
  std::string inputArgText;
  if (splitTemplateTypeName(normalizedInput, inputBase, inputArgText)) {
    std::string normalizedInputBase = normalizeBindingTypeName(inputBase);
    if (!normalizedInputBase.empty() && normalizedInputBase.front() == '/') {
      normalizedInputBase.erase(normalizedInputBase.begin());
    }
    if (normalizedInputBase != "Map" && normalizedInputBase != "std/collections/experimental_map/Map") {
      return false;
    }
  } else {
    if (!normalizedInput.empty() && normalizedInput.front() == '/') {
      normalizedInput.erase(normalizedInput.begin());
    }
    if (normalizedInput.rfind("std/collections/experimental_map/Map__", 0) != 0) {
      return false;
    }
  }
  std::string localError;
  ResolvedType resolvedType = resolveTypeString(typeText, mapping, allowedParams, namespacePrefix, ctx, localError);
  if (!localError.empty()) {
    return false;
  }
  std::string normalized = normalizeBindingTypeName(resolvedType.text);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalized, base, argText)) {
    std::string normalizedBase = normalizeBindingTypeName(base);
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    if (normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map") {
      std::vector<std::string> args;
      return splitTopLevelTemplateArgs(argText, args) && args.size() == 2;
    }
  }
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized.rfind("std/collections/experimental_map/Map__", 0) == 0;
}

struct TemplatedFallbackQueryStateAdapterData {
  std::string queryTypeText;
  BindingInfo receiverBinding;
  bool hasResultType = false;
  bool resultTypeHasValue = false;
  std::string resultValueType;
  std::string resultErrorType;
  std::string mismatchDiagnostic;
};

void populateTemplatedFallbackQueryStateAdapterFromQueryTypeText(
    const std::string &queryTypeText,
    TemplatedFallbackQueryStateAdapterData &out) {
  out.hasResultType = false;
  out.resultTypeHasValue = false;
  out.resultValueType.clear();
  out.resultErrorType.clear();
  out.mismatchDiagnostic.clear();

  std::string normalizedQueryType = normalizeBindingTypeName(queryTypeText);
  std::string resultBase;
  std::string resultArgText;
  if (!splitTemplateTypeName(normalizedQueryType, resultBase, resultArgText)) {
    if (normalizeBindingTypeName(normalizedQueryType) == "Result") {
      out.mismatchDiagnostic = "result query type missing template arguments: " + queryTypeText;
    }
    return;
  }

  resultBase = normalizeBindingTypeName(resultBase);
  if (!resultBase.empty() && resultBase.front() == '/') {
    resultBase.erase(resultBase.begin());
  }
  if (resultBase != "Result") {
    return;
  }

  std::vector<std::string> resultArgs;
  if (!splitTopLevelTemplateArgs(resultArgText, resultArgs) || resultArgs.empty() || resultArgs.size() > 2) {
    out.mismatchDiagnostic = "invalid Result query type envelope: " + queryTypeText;
    return;
  }

  out.hasResultType = true;
  if (resultArgs.size() == 2) {
    out.resultTypeHasValue = true;
    out.resultValueType = normalizeBindingTypeName(resultArgs.front());
    out.resultErrorType = normalizeBindingTypeName(resultArgs.back());
  } else {
    out.resultTypeHasValue = false;
    out.resultErrorType = normalizeBindingTypeName(resultArgs.front());
  }
}

bool inferDefinitionReturnBindingForTemplatedFallback(const Definition &def,
                                                      bool allowMathBare,
                                                      Context &ctx,
                                                      BindingInfo &infoOut) {
  if (!def.templateArgs.empty()) {
    return false;
  }
  if (!ctx.returnInferenceStack.insert(def.fullPath).second) {
    return false;
  }
  struct InferenceScopeGuard {
    std::unordered_set<std::string> &stack;
    std::string fullPath;
    ~InferenceScopeGuard() { stack.erase(fullPath); }
  } inferenceScopeGuard{ctx.returnInferenceStack, def.fullPath};

  std::vector<ParameterInfo> defParams;
  defParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, paramInfo.binding);
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    defParams.push_back(std::move(paramInfo));
  }

  LocalTypeMap locals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      BindingInfo binding;
      if (extractExplicitBindingType(stmt, binding)) {
        if (binding.typeName == "auto" && stmt.args.size() == 1 &&
            inferBindingTypeForMonomorph(stmt.args.front(), defParams, locals, allowMathBare, ctx, binding)) {
          locals[stmt.name] = binding;
        } else {
          locals[stmt.name] = binding;
        }
      } else if (stmt.args.size() == 1 &&
                 inferBindingTypeForMonomorph(stmt.args.front(), defParams, locals, allowMathBare, ctx, binding)) {
        locals[stmt.name] = binding;
      }
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (def.returnExpr.has_value()) {
    valueExpr = &*def.returnExpr;
  }
  if (valueExpr == nullptr) {
    return false;
  }
  return inferBindingTypeForMonomorph(*valueExpr, defParams, locals, allowMathBare, ctx, infoOut);
}

std::string inferExprTypeTextForTemplatedVectorFallback(const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::string &namespacePrefix,
                                                        const Context &ctx,
                                                        bool allowMathBare) {
  if (expr.kind != Expr::Kind::Call || expr.isBinding) {
    return {};
  }
  std::string builtinCollection;
  if (getBuiltinCollectionName(expr, builtinCollection)) {
    if ((builtinCollection == "array" || builtinCollection == "vector" || builtinCollection == "soa_vector") &&
        expr.templateArgs.size() == 1) {
      return builtinCollection + "<" + expr.templateArgs.front() + ">";
    }
    if (builtinCollection == "map" && expr.templateArgs.size() == 2) {
      return "map<" + expr.templateArgs.front() + ", " + expr.templateArgs[1] + ">";
    }
  }
  std::string resolved;
  if (expr.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(expr, locals, ctx, resolved)) {
      return {};
    }
  } else {
    resolved = resolveCalleePath(expr, namespacePrefix, ctx);
  }
  const auto defIt = ctx.sourceDefs.find(resolved);
  if (defIt == ctx.sourceDefs.end()) {
    return {};
  }
  if (isStructDefinition(defIt->second)) {
    return resolved;
  }
  const Definition &resolvedDef = defIt->second;
  std::unordered_set<std::string> allowedParams(resolvedDef.templateArgs.begin(),
                                                resolvedDef.templateArgs.end());
  SubstMap returnTypeMapping;
  if (expr.templateArgs.size() == resolvedDef.templateArgs.size()) {
    returnTypeMapping.reserve(expr.templateArgs.size());
    for (size_t i = 0; i < expr.templateArgs.size(); ++i) {
      returnTypeMapping.emplace(resolvedDef.templateArgs[i], expr.templateArgs[i]);
    }
  }
  for (const auto &transform : defIt->second.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      continue;
    }
    std::string resolvedError;
    ResolvedType resolvedReturnType =
        resolveTypeString(returnType, returnTypeMapping, allowedParams, resolvedDef.namespacePrefix,
                          const_cast<Context &>(ctx), resolvedError);
    if (!resolvedError.empty() || !resolvedReturnType.concrete || resolvedReturnType.text.empty()) {
      continue;
    }
    return resolvedReturnType.text;
  }
  BindingInfo inferredReturn;
  Context &mutableCtx = const_cast<Context &>(ctx);
  if (inferDefinitionReturnBindingForTemplatedFallback(defIt->second, allowMathBare, mutableCtx, inferredReturn)) {
    return bindingTypeToString(inferredReturn);
  }
  return {};
}

bool inferTemplatedFallbackQueryStateAdapter(const Expr &expr,
                                             const LocalTypeMap &locals,
                                             const std::vector<ParameterInfo> &params,
                                             const std::string &namespacePrefix,
                                             Context &ctx,
                                             bool allowMathBare,
                                             TemplatedFallbackQueryStateAdapterData &out) {
  out = {};
  out.queryTypeText =
      inferExprTypeTextForTemplatedVectorFallback(expr, locals, namespacePrefix, ctx, allowMathBare);
  if (out.queryTypeText.empty()) {
    return false;
  }

  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty()) {
    BindingInfo receiverBinding;
    if (inferBindingTypeForMonomorph(expr.args.front(), params, locals, allowMathBare, ctx, receiverBinding) &&
        !receiverBinding.typeName.empty()) {
      out.receiverBinding = std::move(receiverBinding);
    }
  }
  populateTemplatedFallbackQueryStateAdapterFromQueryTypeText(out.queryTypeText, out);
  return true;
}

bool shouldPreferTemplatedVectorFallbackForTypeMismatch(const Definition &def,
                                                        const Expr &expr,
                                                        const LocalTypeMap &locals,
                                                        const std::vector<ParameterInfo> &params,
                                                        bool allowMathBare,
                                                        Context &ctx,
                                                        const std::string &namespacePrefix) {
  auto isTemplateParamName = [&](const std::string &name) {
    for (const auto &templateArg : def.templateArgs) {
      if (templateArg == name) {
        return true;
      }
    }
    return false;
  };
  auto isCollectionEnvelopeBase = [&](const std::string &base) {
    return base == "array" || base == "vector" || base == "map" || base == "soa_vector";
  };
  auto hasUnknownEnvelopeMismatch = [&](const std::string &normalizedExpected,
                                        const std::string &normalizedActual) {
    if (normalizedExpected == normalizedActual) {
      return false;
    }
    std::string expectedBase;
    std::string expectedArgText;
    std::string actualBase;
    std::string actualArgText;
    const bool expectedIsTemplate = splitTemplateTypeName(normalizedExpected, expectedBase, expectedArgText);
    const bool actualIsTemplate = splitTemplateTypeName(normalizedActual, actualBase, actualArgText);
    if (expectedIsTemplate && actualIsTemplate) {
      const std::string normalizedExpectedBase = normalizeBindingTypeName(expectedBase);
      const std::string normalizedActualBase = normalizeBindingTypeName(actualBase);
      if (normalizedExpectedBase == normalizedActualBase) {
        return true;
      }
      return isCollectionEnvelopeBase(normalizedExpectedBase) || isCollectionEnvelopeBase(normalizedActualBase);
    }
    if (expectedIsTemplate == actualIsTemplate) {
      return false;
    }
    const std::string &nonTemplateText = expectedIsTemplate ? normalizedActual : normalizedExpected;
    if (isTemplateParamName(nonTemplateText)) {
      return false;
    }
    const std::string templateBase = normalizeBindingTypeName(expectedIsTemplate ? expectedBase : actualBase);
    return isCollectionEnvelopeBase(templateBase);
  };
  std::vector<ParameterInfo> callParams;
  callParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    callParams.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string orderError;
  if (!buildOrderedArguments(callParams, expr.args, expr.argNames, ordered, orderError)) {
    return false;
  }
  std::unordered_set<const Expr *> explicitArgs;
  explicitArgs.reserve(expr.args.size());
  for (const auto &arg : expr.args) {
    explicitArgs.insert(&arg);
  }
  for (size_t i = 0; i < callParams.size(); ++i) {
    const auto &param = callParams[i];
    if (param.binding.typeName.empty() || !ordered[i]) {
      continue;
    }
    if (explicitArgs.count(ordered[i]) == 0) {
      continue;
    }
    BindingInfo actual;
    if (!inferBindingTypeForMonomorph(*ordered[i], params, locals, allowMathBare, ctx, actual)) {
      const std::string expectedTypeText = bindingTypeToString(param.binding);
      const std::string inferredActualTypeText =
          inferExprTypeTextForTemplatedVectorFallback(*ordered[i], locals, namespacePrefix, ctx, allowMathBare);
      if (!expectedTypeText.empty() && !inferredActualTypeText.empty()) {
        const std::string normalizedExpected = normalizeBindingTypeName(expectedTypeText);
        const std::string normalizedActual = normalizeBindingTypeName(inferredActualTypeText);
        if (normalizedExpected == "string" && normalizedActual != "string") {
          return true;
        }
        if (normalizedExpected != "string" && normalizedActual == "string") {
          return true;
        }
        const ReturnKind expectedKind = returnKindForTypeName(normalizedExpected);
        const ReturnKind actualKind = returnKindForTypeName(normalizedActual);
        if (expectedKind != ReturnKind::Unknown && actualKind != ReturnKind::Unknown) {
          if (!isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
            if (expectedKind == actualKind && expectedKind == ReturnKind::Array &&
                normalizedExpected != normalizedActual) {
              return true;
            }
            if (expectedKind != actualKind) {
              return true;
            }
          }
        } else if (expectedKind != actualKind) {
          if (normalizedExpected != normalizedActual) {
            return true;
          }
        } else if (hasUnknownEnvelopeMismatch(normalizedExpected, normalizedActual)) {
          return true;
        }
      }
      const std::string expectedStructPath =
          resolveStructLikeTypePathForTemplatedVectorFallback(param.binding.typeName, def.namespacePrefix, ctx);
      if (expectedStructPath.empty()) {
        continue;
      }
      const std::string actualStructPath =
          resolveStructLikeExprPathForTemplatedVectorFallback(*ordered[i], locals, namespacePrefix, ctx, allowMathBare);
      if (!actualStructPath.empty() && actualStructPath != expectedStructPath) {
        return true;
      }
      continue;
    }
    const std::string expectedTypeText = bindingTypeToString(param.binding);
    const std::string actualTypeText = bindingTypeToString(actual);
    const std::string normalizedExpected = normalizeBindingTypeName(expectedTypeText);
    const std::string normalizedActual = normalizeBindingTypeName(actualTypeText);
    if (normalizedExpected == "string" && normalizedActual != "string") {
      return true;
    }
    if (normalizedExpected != "string" && normalizedActual == "string") {
      return true;
    }
    const ReturnKind expectedKind = returnKindForTypeName(normalizedExpected);
    const ReturnKind actualKind = returnKindForTypeName(normalizedActual);
    if (expectedKind == ReturnKind::Unknown || actualKind == ReturnKind::Unknown) {
      if (expectedKind == ReturnKind::Unknown && actualKind == ReturnKind::Unknown) {
        const std::string expectedStructPath =
            resolveStructLikeTypePathForTemplatedVectorFallback(param.binding.typeName, def.namespacePrefix, ctx);
        const std::string actualStructPath =
            resolveStructLikeTypePathForTemplatedVectorFallback(actual.typeName, namespacePrefix, ctx);
        if (!expectedStructPath.empty() && !actualStructPath.empty() && expectedStructPath != actualStructPath) {
          return true;
        }
        if (hasUnknownEnvelopeMismatch(normalizedExpected, normalizedActual)) {
          return true;
        }
      } else if (expectedKind != actualKind) {
        if (normalizedExpected != normalizedActual) {
          return true;
        }
      }
      continue;
    }
    if (isSoftwareNumericParamCompatible(expectedKind, actualKind)) {
      continue;
    }
    if (expectedKind == actualKind && expectedKind == ReturnKind::Array && normalizedExpected != normalizedActual) {
      return true;
    }
    if (actualKind != expectedKind) {
      return true;
    }
  }
  return false;
}

std::string preferVectorStdlibImplicitTemplatePath(const Expr &expr,
                                                   const std::string &path,
                                                   const LocalTypeMap &locals,
                                                   const std::vector<ParameterInfo> &params,
                                                   bool allowMathBare,
                                                   Context &ctx,
                                                   const std::string &namespacePrefix) {
  if (!expr.templateArgs.empty()) {
    return path;
  }
  const auto defIt = ctx.sourceDefs.find(path);
  if (defIt == ctx.sourceDefs.end() || ctx.templateDefs.count(path) > 0) {
    return path;
  }
  if (path == "/soa_vector/ref" || path == "/soa_vector/ref_ref") {
    return path;
  }
  const bool preserveCompatibilityTemplatePath = isCollectionCompatibilityTemplateFallbackPath(path);
  const bool preserveCanonicalMapTemplatePath = shouldPreserveCanonicalMapTemplatePath(path, ctx);
  const bool acceptsCallShape = definitionAcceptsCallShape(defIt->second, expr);
  if (!acceptsCallShape && (preserveCompatibilityTemplatePath || preserveCanonicalMapTemplatePath) &&
      (hasNamedCallArguments(expr) || definitionHasArgumentCountMismatch(defIt->second, expr))) {
    // Keep diagnostics on explicit compatibility helpers and canonical map
    // helpers when named arguments or argument counts do not match.
    return path;
  }
  const bool prefersTypeMismatchFallback = shouldPreferTemplatedVectorFallbackForTypeMismatch(
      defIt->second, expr, locals, params, allowMathBare, ctx, namespacePrefix);
  if ((preserveCompatibilityTemplatePath || preserveCanonicalMapTemplatePath) && prefersTypeMismatchFallback) {
    // Keep diagnostics on explicit compatibility helpers and canonical map
    // helpers when argument types mismatch the declared helper shape.
    return path;
  }
  const std::string preferred = preferVectorStdlibTemplatePath(path, ctx);
  if (acceptsCallShape && !prefersTypeMismatchFallback) {
    return path;
  }
  if (preferred != path && ctx.sourceDefs.count(preferred) > 0 && ctx.templateDefs.count(preferred) > 0) {
    return preferred;
  }
  return path;
}
