#pragma once

// State shared by the rewriteExprReference* phase functions (split out of rewriteExprPhase6).
#include "TemplateMonomorphExpressionRewriteState.h"

#include <functional>
#include <optional>
#include <string>

namespace primec {

struct RewriteExprInnerState {
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
  std::function<std::string(const std::string &path)> preferredConcreteSamePathSoaHelperPath;
};

PhaseStatus rewriteExprReferencePhase1(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase2(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase3(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase4(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase5(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase6(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase7(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);
PhaseStatus rewriteExprReferencePhase8(Expr &expr, const SubstMap &mapping, const std::unordered_set<std::string> &allowedParams, const std::string &namespacePrefix, Context &ctx, std::string &error, const LocalTypeMap &locals, const std::vector<semantics::ParameterInfo> &params, RewriteExprState &st, RewriteExprInnerState &st2);

} // namespace primec
