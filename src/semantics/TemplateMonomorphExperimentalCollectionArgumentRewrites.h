#pragma once

std::vector<ParameterInfo> buildExperimentalConstructorRewriteParams(const Definition &targetDef,
                                                                     bool allowMathBare,
                                                                     Context &ctx) {
  std::vector<ParameterInfo> callParams;
  if (!targetDef.parameters.empty()) {
    callParams.reserve(targetDef.parameters.size());
    for (const auto &paramExpr : targetDef.parameters) {
      ParameterInfo paramInfo;
      paramInfo.name = paramExpr.name;
      inferCallTargetBinding(paramExpr, allowMathBare, ctx, paramInfo.binding);
      if (paramExpr.args.size() == 1) {
        paramInfo.defaultExpr = &paramExpr.args.front();
      }
      callParams.push_back(std::move(paramInfo));
    }
    return callParams;
  }
  if (!isStructDefinition(targetDef)) {
    return callParams;
  }
  for (const auto &fieldExpr : targetDef.statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    ParameterInfo fieldInfo;
    fieldInfo.name = fieldExpr.name;
    inferCallTargetBinding(fieldExpr, allowMathBare, ctx, fieldInfo.binding);
    if (fieldExpr.args.size() == 1) {
      fieldInfo.defaultExpr = &fieldExpr.args.front();
    }
    callParams.push_back(std::move(fieldInfo));
  }
  return callParams;
}

template <typename RewriteTargetValueFn>
bool rewriteExperimentalConstructorArgsForTarget(Expr &callExpr,
                                                 const Definition &targetDef,
                                                 bool methodCallSyntax,
                                                 bool allowMathBare,
                                                 Context &ctx,
                                                 RewriteTargetValueFn &&rewriteTargetValueForType) {
  std::vector<ParameterInfo> callParams =
      buildExperimentalConstructorRewriteParams(targetDef, allowMathBare, ctx);
  if (callParams.empty()) {
    return true;
  }
  std::vector<const Expr *> orderedArgs(callParams.size(), nullptr);
  size_t callArgStart = 0;
  if (methodCallSyntax && callExpr.args.size() == callParams.size() + 1) {
    callArgStart = 1;
  }
  size_t positionalIndex = 0;
  for (size_t i = callArgStart; i < callExpr.args.size(); ++i) {
    if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
      const std::string &name = *callExpr.argNames[i];
      size_t index = callParams.size();
      for (size_t p = 0; p < callParams.size(); ++p) {
        if (callParams[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= callParams.size() || orderedArgs[index] != nullptr) {
        return true;
      }
      orderedArgs[index] = &callExpr.args[i];
      continue;
    }
    while (positionalIndex < orderedArgs.size() && orderedArgs[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= orderedArgs.size()) {
      return true;
    }
    orderedArgs[positionalIndex] = &callExpr.args[i];
    ++positionalIndex;
  }
  for (size_t paramIndex = 0; paramIndex < callParams.size() && paramIndex < orderedArgs.size(); ++paramIndex) {
    const BindingInfo &paramBinding = callParams[paramIndex].binding;
    std::string typeText = paramBinding.typeName;
    if (!paramBinding.typeTemplateArg.empty()) {
      typeText += "<" + paramBinding.typeTemplateArg + ">";
    }
    const Expr *orderedArg = orderedArgs[paramIndex];
    if (orderedArg == nullptr) {
      continue;
    }
    for (auto &argExpr : callExpr.args) {
      if (&argExpr != orderedArg) {
        continue;
      }
      if (!rewriteTargetValueForType(typeText, argExpr)) {
        return false;
      }
      break;
    }
  }
  return true;
}
