#pragma once

bool resolveExperimentalConstructorTargetTypeText(const Expr &targetExpr,
                                                  const std::vector<ParameterInfo> &params,
                                                  const LocalTypeMap &locals,
                                                  bool allowMathBare,
                                                  const std::string &namespacePrefix,
                                                  Context &ctx,
                                                  std::string &targetTypeTextOut) {
  targetTypeTextOut.clear();
  BindingInfo targetInfo;
  if (!resolveAssignmentTargetBinding(targetExpr, params, locals, allowMathBare, namespacePrefix, ctx, targetInfo)) {
    return false;
  }
  targetTypeTextOut = targetInfo.typeName;
  if (!targetInfo.typeTemplateArg.empty()) {
    targetTypeTextOut += "<" + targetInfo.typeTemplateArg + ">";
  }
  return true;
}

template <typename RewriteTargetValueFn>
void rewriteExperimentalAssignTargetValue(Expr &callExpr,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          bool allowMathBare,
                                          const std::string &namespacePrefix,
                                          Context &ctx,
                                          RewriteTargetValueFn &&rewriteTargetValueForType) {
  if (!isAssignCall(callExpr) || callExpr.args.size() != 2) {
    return;
  }
  std::string targetTypeText;
  if (!resolveExperimentalConstructorTargetTypeText(
          callExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, targetTypeText)) {
    return;
  }
  (void)rewriteTargetValueForType(targetTypeText, callExpr.args[1]);
}

template <typename RewriteTargetValueFn>
void rewriteExperimentalInitTargetValue(Expr &callExpr,
                                        const std::vector<ParameterInfo> &params,
                                        const LocalTypeMap &locals,
                                        bool allowMathBare,
                                        const std::string &namespacePrefix,
                                        Context &ctx,
                                        RewriteTargetValueFn &&rewriteTargetValueForType) {
  if (!isSimpleCallName(callExpr, "init") || callExpr.args.size() != 2) {
    return;
  }
  std::string targetTypeText;
  if (!resolveExperimentalConstructorTargetTypeText(
          callExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, targetTypeText)) {
    return;
  }
  (void)rewriteTargetValueForType(targetTypeText, callExpr.args[1]);
}
