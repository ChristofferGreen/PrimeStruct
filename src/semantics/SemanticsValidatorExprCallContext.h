#pragma once

// What the rules that validate a call expression read and fill in: the expression and its scope, and the facts the
// rules settle in order. Data only; the lookups the rules share are members of SemanticsValidator.
#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

struct ExprCallContext {
  const std::vector<ParameterInfo> &params;
  const std::unordered_map<std::string, BindingInfo> &locals;
  const Expr &expr;
  // The outcome once a rule has returned Done.
  bool result{};
  PhaseStatus done(bool value) {
    result = value;
    return PhaseStatus::Done;
  }
  std::optional<SemanticsValidator::EffectScope> effectScope{};
  bool hasVectorHelperCallResolution{};
  std::string vectorHelperCallResolvedPath{};
  size_t vectorHelperCallReceiverIndex{};
  SemanticsValidator::ExprDispatchBootstrap dispatchBootstrap{};
  bool shouldBuiltinValidateBareKeyValueContainsCall{};
  bool shouldBuiltinValidateBareKeyValueAccessCall{};
  std::string resolved{};
  SemanticsValidator::ExprMethodCompatibilitySetup methodCompatibilitySetup{};
  bool resolvedMethod{};
  bool usedMethodTarget{};
  bool hasMethodReceiverIndex{};
  size_t methodReceiverIndex{};
  SemanticsValidator::ExprCollectionDispatchSetup collectionDispatchSetup{};
  const Definition *resolvedDefinition{};
  std::unordered_map<std::string, std::vector<ParameterInfo>>::const_iterator calleeParamsIt{};
  const std::vector<Expr> *enclosingStatements{};
  size_t statementIndex{};
  bool expressionIsStatementContext{};
};

} // namespace primec::semantics
