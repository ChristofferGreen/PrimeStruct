#pragma once

bool isBuiltinResultOkPayloadCall(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
      candidate.args.size() != 2) {
    return false;
  }
  const Expr &receiver = candidate.args.front();
  return receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "Result";
}

template <typename RewriteCurrentFn>
bool rewriteExperimentalConstructorValueTree(Expr &candidate, RewriteCurrentFn &&rewriteCurrent) {
  if (candidate.isBinding && candidate.args.size() == 1) {
    return rewriteExperimentalConstructorValueTree(candidate.args.front(), rewriteCurrent);
  }
  if (candidate.kind != Expr::Kind::Call) {
    return true;
  }
  if (!rewriteCurrent(candidate)) {
    return false;
  }
  for (auto &arg : candidate.args) {
    if (!rewriteExperimentalConstructorValueTree(arg, rewriteCurrent)) {
      return false;
    }
  }
  for (auto &bodyArg : candidate.bodyArguments) {
    if (!rewriteExperimentalConstructorValueTree(bodyArg, rewriteCurrent)) {
      return false;
    }
  }
  return true;
}

template <typename RewriteMapValueFn>
bool rewriteExperimentalMapResultOkPayloadTree(Expr &candidate, RewriteMapValueFn &&rewriteMapValue) {
  if (candidate.isBinding && candidate.args.size() == 1) {
    return rewriteExperimentalMapResultOkPayloadTree(candidate.args.front(), rewriteMapValue);
  }
  if (candidate.kind != Expr::Kind::Call) {
    return true;
  }
  if (isBuiltinResultOkPayloadCall(candidate)) {
    return rewriteMapValue(candidate.args.back());
  }
  for (auto &arg : candidate.args) {
    if (!rewriteExperimentalMapResultOkPayloadTree(arg, rewriteMapValue)) {
      return false;
    }
  }
  for (auto &bodyArg : candidate.bodyArguments) {
    if (!rewriteExperimentalMapResultOkPayloadTree(bodyArg, rewriteMapValue)) {
      return false;
    }
  }
  return true;
}

template <typename RewriteNestedMapValueFn, typename RewriteMapPayloadFn>
bool rewriteExperimentalMapTargetValueForType(const std::string &typeText,
                                              Expr &valueExpr,
                                              const SubstMap &mapping,
                                              const std::unordered_set<std::string> &allowedParams,
                                              const std::string &namespacePrefix,
                                              Context &ctx,
                                              RewriteNestedMapValueFn &&rewriteNestedMapValue,
                                              RewriteMapPayloadFn &&rewriteMapPayload) {
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "uninitialized") {
    std::vector<std::string> storageArgs;
    if (!splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
      return true;
    }
    return rewriteExperimentalMapTargetValueForType(trimWhitespace(storageArgs.front()),
                                                    valueExpr,
                                                    mapping,
                                                    allowedParams,
                                                    namespacePrefix,
                                                    ctx,
                                                    rewriteNestedMapValue,
                                                    rewriteMapPayload);
  }
  if (resolvesExperimentalMapValueTypeText(typeText, mapping, allowedParams, namespacePrefix, ctx)) {
    return rewriteNestedMapValue(valueExpr);
  }
  if (!splitTemplateTypeName(typeText, base, argText) || normalizeBindingTypeName(base) != "Result") {
    return true;
  }
  std::vector<std::string> resultArgs;
  if (!splitTopLevelTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
    return true;
  }
  if (!resolvesExperimentalMapValueTypeText(trimWhitespace(resultArgs.front()),
                                            mapping,
                                            allowedParams,
                                            namespacePrefix,
                                            ctx)) {
    return true;
  }
  return rewriteMapPayload(valueExpr);
}

template <typename RewriteNestedVectorValueFn>
bool rewriteExperimentalVectorTargetValueForType(const std::string &typeText,
                                                 Expr &valueExpr,
                                                 RewriteNestedVectorValueFn &&rewriteNestedVectorValue) {
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(typeText, base, argText) && normalizeBindingTypeName(base) == "uninitialized") {
    std::vector<std::string> storageArgs;
    if (!splitTopLevelTemplateArgs(argText, storageArgs) || storageArgs.size() != 1) {
      return true;
    }
    return rewriteExperimentalVectorTargetValueForType(trimWhitespace(storageArgs.front()),
                                                       valueExpr,
                                                       rewriteNestedVectorValue);
  }
  if (!resolvesExperimentalVectorValueTypeText(typeText)) {
    return true;
  }
  return rewriteNestedVectorValue(valueExpr);
}

template <typename ExpectedTypeFn, typename RewriteTargetValueFn>
bool rewriteExperimentalConstructorBinding(Expr &bindingExpr,
                                           const std::vector<ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           ExpectedTypeFn &&hasExpectedType,
                                           std::string_view compatibilityEnvelope,
                                           RewriteTargetValueFn &&rewriteTargetValue) {
  if (!bindingExpr.isBinding || bindingExpr.args.size() != 1) {
    return true;
  }
  BindingInfo bindingInfo;
  const bool hasExplicitBindingTransform = hasExplicitBindingTypeTransform(bindingExpr);
  const bool hasExplicitBindingType = extractExplicitBindingType(bindingExpr, bindingInfo);
  if (hasExplicitBindingType) {
    const std::string bindingTypeText = bindingTypeToString(bindingInfo);
    if (!hasExpectedType(bindingTypeText) &&
        unwrapCollectionReceiverEnvelope(bindingInfo.typeName, bindingInfo.typeTemplateArg) ==
            compatibilityEnvelope) {
      return true;
    }
  }
  if (!hasExplicitBindingType) {
    if (hasExplicitBindingTransform) {
      return true;
    }
    if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), params, locals, allowMathBare, ctx, bindingInfo)) {
      return true;
    }
  } else if (bindingInfo.typeName == "auto") {
    if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), params, locals, allowMathBare, ctx, bindingInfo)) {
      return true;
    }
  }
  std::string bindingTypeText = bindingInfo.typeName;
  if (!bindingInfo.typeTemplateArg.empty()) {
    bindingTypeText += "<" + bindingInfo.typeTemplateArg + ">";
  }
  return rewriteTargetValue(bindingTypeText, bindingExpr.args.front());
}
