#pragma once

namespace {

bool isSumDefinitionForMonomorphRefresh(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

std::string sumPayloadTypeTextForMonomorphRefresh(const Transform &transform) {
  if (transform.templateArgs.empty()) {
    return transform.name;
  }
  return transform.name + "<" + joinTemplateArgs(transform.templateArgs) + ">";
}

void refreshMonomorphizedSumVariants(Definition &def) {
  if (!isSumDefinitionForMonomorphRefresh(def)) {
    return;
  }

  def.sumVariants.clear();
  def.sumVariants.reserve(def.statements.size());
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding && stmt.transforms.size() == 1) {
      const Transform &payload = stmt.transforms.front();
      def.sumVariants.push_back(SumVariant{
          stmt.name,
          true,
          payload.name,
          payload.templateArgs,
          sumPayloadTypeTextForMonomorphRefresh(payload),
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
      continue;
    }
    if (stmt.kind == Expr::Kind::Name && !stmt.name.empty() &&
        stmt.args.empty() && stmt.argNames.empty() &&
        stmt.bodyArguments.empty() && !stmt.hasBodyArguments &&
        stmt.transforms.empty() && stmt.templateArgs.empty()) {
      def.sumVariants.push_back(SumVariant{
          stmt.name,
          false,
          {},
          {},
          {},
          def.sumVariants.size(),
          stmt.sourceLine,
          stmt.sourceColumn,
          stmt.semanticNodeId,
      });
    }
  }
}

} // namespace

bool rewriteDefinition(Definition &def,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       Context &ctx,
                       std::string &error) {
  const std::string previousDefinitionPath = ctx.currentDefinitionPath;
  ctx.currentDefinitionPath = def.fullPath;
  auto restoreDefinitionPath = [&]() { ctx.currentDefinitionPath = previousDefinitionPath; };
  if (!rewriteTransforms(def.transforms, mapping, allowedParams, def.namespacePrefix, ctx, error)) {
    restoreDefinitionPath();
    return false;
  }
  const bool allowMathBare = hasMathImport(ctx);
  std::vector<ParameterInfo> params;
  LocalTypeMap locals;
  const ExperimentalCollectionReturnRewritePlan returnRewritePlan =
      inferExperimentalCollectionReturnRewritePlan(def, mapping, allowedParams, allowMathBare, ctx);
  params.reserve(def.parameters.size());
  locals.reserve(def.parameters.size() + def.statements.size());
  const DefinitionReturnStatementSelection returnStatementSelection =
      determineDefinitionReturnStatementSelection(def);
  if (!rewriteDefinitionParameters(
          def.parameters, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
    restoreDefinitionPath();
    return false;
  }
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    auto &stmt = def.statements[stmtIndex];
    const bool isReturnPathStmt = isDefinitionReturnPathStatement(stmt, stmtIndex, returnStatementSelection);
    if (isReturnPathStmt &&
        !rewriteDefinitionExperimentalReturnConstructors(stmt,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      restoreDefinitionPath();
      return false;
    }
    if (!rewriteExpr(stmt, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params, allowMathBare)) {
      restoreDefinitionPath();
      return false;
    }
    if (isReturnPathStmt &&
        !rewriteDefinitionExperimentalReturnConstructors(stmt,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      restoreDefinitionPath();
      return false;
    }
    recordDefinitionStatementBindingLocal(stmt, params, locals, allowMathBare, ctx, locals);
  }
  if (def.returnExpr.has_value()) {
    if (!rewriteDefinitionExperimentalReturnConstructors(*def.returnExpr,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      restoreDefinitionPath();
      return false;
    }
    if (!rewriteExpr(*def.returnExpr, mapping, allowedParams, def.namespacePrefix, ctx, error, locals, params,
                     allowMathBare)) {
      restoreDefinitionPath();
      return false;
    }
    if (!rewriteDefinitionExperimentalReturnConstructors(*def.returnExpr,
                                                         returnRewritePlan,
                                                         locals,
                                                         params,
                                                         mapping,
                                                         allowedParams,
                                                         def.namespacePrefix,
                                                         ctx,
                                                         allowMathBare,
                                                         error)) {
      return false;
    }
  }
  refreshMonomorphizedSumVariants(def);
  restoreDefinitionPath();
  return true;
}
