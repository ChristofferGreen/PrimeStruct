#pragma once

bool isDefinitionReturnPathStatement(const Expr &stmt,
                                     size_t stmtIndex,
                                     const DefinitionReturnStatementSelection &selection) {
  return isReturnCall(stmt) || (!selection.sawExplicitReturn && stmtIndex == selection.implicitReturnStmtIndex);
}

template <typename RewriteVectorFn, typename RewriteKeyValueFn>
bool rewriteDefinitionReturnConstructors(Expr &expr,
                                         const ExperimentalCollectionReturnRewritePlan &plan,
                                         RewriteVectorFn &&rewriteVectorReturn,
                                         RewriteKeyValueFn &&rewriteKeyValueReturn,
                                         std::string &error) {
  if (plan.expectedExperimentalVectorReturn) {
    rewriteVectorReturn(expr);
    if (!error.empty()) {
      return false;
    }
  }
  if (plan.expectedExperimentalKeyValueReturn) {
    rewriteKeyValueReturn(expr);
    if (!error.empty()) {
      return false;
    }
  }
  return true;
}
