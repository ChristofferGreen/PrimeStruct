#pragma once

void rewriteDefinitionExperimentalMapConstructorValue(Expr &valueExpr,
                                                      LocalTypeMap &locals,
                                                      std::vector<ParameterInfo> &params,
                                                      const SubstMap &mapping,
                                                      const std::unordered_set<std::string> &allowedParams,
                                                      const std::string &namespacePrefix,
                                                      Context &ctx,
                                                      bool allowMathBare,
                                                      std::string &error) {
  (void)rewriteCanonicalExperimentalMapConstructorExpr(
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

void rewriteDefinitionExperimentalMapReturnConstructors(Expr &candidate,
                                                        LocalTypeMap &locals,
                                                        std::vector<ParameterInfo> &params,
                                                        const SubstMap &mapping,
                                                        const std::unordered_set<std::string> &allowedParams,
                                                        const std::string &namespacePrefix,
                                                        Context &ctx,
                                                        bool allowMathBare,
                                                        std::string &error) {
  rewriteExperimentalConstructorReturnTree(candidate, [&](Expr &valueExpr) {
    rewriteDefinitionExperimentalMapConstructorValue(
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
        rewriteDefinitionExperimentalMapReturnConstructors(
            candidate, locals, params, mapping, allowedParams, namespacePrefix, ctx, allowMathBare, error);
      },
      error);
}
