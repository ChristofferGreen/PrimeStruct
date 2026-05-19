#pragma once

void rewriteDefinitionExperimentalKeyValueConstructorValue(Expr &valueExpr,
                                                           LocalTypeMap &locals,
                                                           std::vector<ParameterInfo> &params,
                                                           const SubstMap &mapping,
                                                           const std::unordered_set<std::string> &allowedParams,
                                                           const std::string &namespacePrefix,
                                                           Context &ctx,
                                                           bool allowMathBare,
                                                           std::string &error) {
  (void)rewriteCanonicalExperimentalKeyValueConstructorExpr(
      valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
}

void rewriteDefinitionExperimentalVectorConstructorValue(Expr &valueExpr,
                                                         LocalTypeMap &locals,
                                                         std::vector<ParameterInfo> &params,
                                                         const SubstMap &mapping,
                                                         const std::unordered_set<std::string> &allowedParams,
                                                         const std::string &namespacePrefix,
                                                         Context &ctx,
                                                         bool allowMathBare,
                                                         std::string &error) {
  (void)rewriteCanonicalExperimentalVectorConstructorExpr(
      valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
}

void rewriteDefinitionExperimentalVectorReturnConstructors(Expr &candidate,
                                                           LocalTypeMap &locals,
                                                           std::vector<ParameterInfo> &params,
                                                           const SubstMap &mapping,
                                                           const std::unordered_set<std::string> &allowedParams,
                                                           const std::string &namespacePrefix,
                                                           Context &ctx,
                                                           bool allowMathBare,
                                                           std::string &error) {
  rewriteExperimentalConstructorReturnTree(candidate, [&](Expr &valueExpr) {
    rewriteDefinitionExperimentalVectorConstructorValue(
        valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
  });
}

void rewriteDefinitionExperimentalKeyValueReturnConstructors(Expr &candidate,
                                                             LocalTypeMap &locals,
                                                             std::vector<ParameterInfo> &params,
                                                             const SubstMap &mapping,
                                                             const std::unordered_set<std::string> &allowedParams,
                                                             const std::string &namespacePrefix,
                                                             Context &ctx,
                                                             bool allowMathBare,
                                                             std::string &error) {
  rewriteExperimentalConstructorReturnTree(candidate, [&](Expr &valueExpr) {
    rewriteDefinitionExperimentalKeyValueConstructorValue(
        valueExpr, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
  });
}

bool rewriteDefinitionExperimentalReturnConstructors(Expr &expr,
                                                     const ExperimentalCollectionReturnRewritePlan &plan,
                                                     LocalTypeMap &locals,
                                                     std::vector<ParameterInfo> &params,
                                                     const SubstMap &mapping,
                                                     const std::unordered_set<std::string> &allowedParams,
                                                     const std::string &namespacePrefix,
                                                     Context &ctx,
                                                     bool allowMathBare,
                                                     std::string &error) {
  return rewriteDefinitionReturnConstructors(
      expr,
      plan,
      [&](Expr &candidate) {
        rewriteDefinitionExperimentalVectorReturnConstructors(
            candidate, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
      },
      [&](Expr &candidate) {
        rewriteDefinitionExperimentalKeyValueReturnConstructors(
            candidate, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
      },
      error);
}
