#pragma once

struct ExperimentalCollectionReturnRewritePlan {
  bool hasExplicitNonAutoReturn = false;
  bool expectedExperimentalVectorReturn = false;
  bool expectedExperimentalMapReturn = false;
};

ExperimentalCollectionReturnRewritePlan inferExperimentalCollectionReturnRewritePlan(
    const Definition &def,
    const SubstMap &mapping,
    const std::unordered_set<std::string> &allowedParams,
    bool allowMathBare,
    Context &ctx) {
  ExperimentalCollectionReturnRewritePlan plan;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    if (transform.templateArgs.front() != "auto") {
      plan.hasExplicitNonAutoReturn = true;
    }
    if (resolvesExperimentalVectorValueTypeText(transform.templateArgs.front())) {
      plan.expectedExperimentalVectorReturn = true;
    }
    if (resolvesExperimentalMapValueTypeText(
            transform.templateArgs.front(), mapping, allowedParams, def.namespacePrefix, ctx)) {
      plan.expectedExperimentalMapReturn = true;
    }
  }

  if (!plan.expectedExperimentalVectorReturn && !plan.expectedExperimentalMapReturn &&
      !plan.hasExplicitNonAutoReturn) {
    BindingInfo inferredReturnInfo;
    if (inferDefinitionReturnBindingForTemplatedFallback(def, allowMathBare, ctx, inferredReturnInfo)) {
      std::string inferredReturnType = inferredReturnInfo.typeName;
      if (!inferredReturnInfo.typeTemplateArg.empty()) {
        inferredReturnType += "<" + inferredReturnInfo.typeTemplateArg + ">";
      }
      plan.expectedExperimentalVectorReturn = resolvesExperimentalVectorValueTypeText(inferredReturnType);
      plan.expectedExperimentalMapReturn = resolvesExperimentalMapValueTypeText(
          inferredReturnType, mapping, allowedParams, def.namespacePrefix, ctx);
    }
  }

  return plan;
}

struct DefinitionReturnStatementSelection {
  bool sawExplicitReturn = false;
  size_t implicitReturnStmtIndex = 0;
};

DefinitionReturnStatementSelection determineDefinitionReturnStatementSelection(const Definition &def) {
  DefinitionReturnStatementSelection selection;
  selection.implicitReturnStmtIndex = def.statements.size();
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    if (isReturnCall(def.statements[stmtIndex])) {
      selection.sawExplicitReturn = true;
      break;
    }
    if (!def.statements[stmtIndex].isBinding) {
      selection.implicitReturnStmtIndex = stmtIndex;
    }
  }
  return selection;
}
