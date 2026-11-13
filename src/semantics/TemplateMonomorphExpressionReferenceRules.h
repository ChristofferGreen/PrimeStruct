#pragma once

// What the rules that rewrite a plain call expression read and fill in (the `rewriteExpr` reference rules): the
// expression and its scope, the outer rewrite state, and the facts the rules settle in order. Data only.
#include "TemplateMonomorphExpressionRewriteContext.h"

#include <string>
#include <vector>

namespace primec {

struct ExprReferenceContext {
  Expr &expr;
  const SubstMap &mapping;
  const std::unordered_set<std::string> &allowedParams;
  const std::string &namespacePrefix;
  Context &ctx;
  std::string &error;
  const LocalTypeMap &locals;
  const std::vector<semantics::ParameterInfo> &params;
  ExprRewriteContext &st;
  // The outcome once a rule has returned Done.
  PhaseStatus result{};
  PhaseStatus done(PhaseStatus value) {
    result = std::move(value);
    return PhaseStatus::Done;
  }
  std::string resolvedPath{};
  bool usesKeyValueEntryConstructorArgs{};
  std::string borrowedCanonicalKeyValueUnknownTarget{};
  std::string removedKeyValueCompatibilityPath{};
  bool inferredCanonicalKeyValueReceiverTemplateArgs{};
  bool isKnownDef{};
};

// Runs the rules in order; Done when one has decided the expression (the outcome is in the returned status of the
// caller's state), Continue when none has.
PhaseStatus rewriteExprReference(Expr &expr,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 const LocalTypeMap &locals,
                                 const std::vector<semantics::ParameterInfo> &params,
                                 ExprRewriteContext &st);

PhaseStatus prerewriteReceiverConstructors(ExprReferenceContext &rc);
PhaseStatus rewriteKeyValuePairConstructor(ExprReferenceContext &rc);
PhaseStatus rewriteBorrowedVectorHelperCall(ExprReferenceContext &rc);
PhaseStatus rewriteKeyValueEntryConstructor(ExprReferenceContext &rc);
PhaseStatus rewriteExperimentalVectorTemporary(ExprReferenceContext &rc);
PhaseStatus resolveCanonicalHelperTarget(ExprReferenceContext &rc);
PhaseStatus rewriteSamePathHelperTemplate(ExprReferenceContext &rc);
PhaseStatus rewriteConstructorArgsForTarget(ExprReferenceContext &rc);
std::string preferredConcreteSamePathSoaHelperPath(ExprReferenceContext &rc,
                                                   const std::string &path);

} // namespace primec
