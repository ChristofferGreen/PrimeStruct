#pragma once

bool isDefinitionReturnPathStatement(const Expr &stmt,
                                     size_t stmtIndex,
                                     const DefinitionReturnStatementSelection &selection) {
  return isReturnCall(stmt) || (!selection.sawExplicitReturn && stmtIndex == selection.implicitReturnStmtIndex);
}

template <typename RewriteVectorFn, typename RewriteMapFn>
bool rewriteDefinitionReturnConstructors(Expr &expr,
                                         const ExperimentalCollectionReturnRewritePlan &plan,
                                         RewriteVectorFn &&rewriteVectorReturn,
                                         RewriteMapFn &&rewriteMapReturn,
                                         std::string &error) {
  if (plan.expectedExperimentalVectorReturn) {
    rewriteVectorReturn(expr);
    if (!error.empty()) {
      return false;
    }
  }
  if (plan.expectedExperimentalMapReturn) {
    rewriteMapReturn(expr);
    if (!error.empty()) {
      return false;
    }
  }
  return true;
}
