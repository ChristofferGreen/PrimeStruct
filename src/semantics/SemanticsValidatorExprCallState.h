#pragma once

// State shared by the validateExprCall* phase functions (split out of validateExpr, TODO-5385).
#include "SemanticsValidator.h"

#include <functional>
#include <optional>
#include <string>

namespace primec::semantics {

struct ValidateExprCallState {
  bool result{};
  PhaseStatus done(bool value) {
    result = std::move(value);
    return PhaseStatus::Done;
  }
  std::function<std::optional<std::string>()> pendingFieldViewNameFromRewrittenHelper;
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
  std::function<bool(std::string_view candidate, std::string_view familyPath)> matchesResolvedFamilyPath;
  const Definition * resolvedDefinition{};
  std::unordered_map<std::string, std::vector<ParameterInfo>>::const_iterator calleeParamsIt{};
  const std::vector<Expr> * enclosingStatements{};
  size_t statementIndex{};
  bool expressionIsStatementContext{};
};

} // namespace primec::semantics
