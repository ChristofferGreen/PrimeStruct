#pragma once

// State shared by the validateBindingReference* phase functions (split out of validateBindingPhase5).
#include "SemanticsValidator.h"

#include <functional>
#include <optional>
#include <string>

namespace primec::semantics {

using ExprSubstitutions = std::vector<std::pair<std::string, const Expr *>>;

struct ValidateBindingReferenceState {
  PhaseStatus result{};
  PhaseStatus done(PhaseStatus value) {
    result = std::move(value);
    return PhaseStatus::Done;
  }
  const Expr * init{};
  std::function<bool(const Expr &expr, std::string &targetOut)> resolveDirectBorrowStorageTargetType;
  std::function<bool(const Expr &, std::string &)> resolvePointerTargetType{};
  std::function<bool(const std::string &targetName, std::string &rootOut)> resolveBorrowRoot;
  std::function<const Expr *(const ExprSubstitutions &substitutions, const std::string &name, size_t *matchedIndexOut)> findSubstitutedExpr;
  std::function<ExprSubstitutions(const ExprSubstitutions &substitutions, size_t indexToSkip)> removeSubstitutionAt;
  std::function<bool(const Expr &callExpr, const ExprSubstitutions &baseSubstitutions, ExprSubstitutions &extendedSubstitutions, const Expr *&returnedValueExprOut)> appendCallSubstitutions;
  std::function<bool(const Expr &callExpr, std::string &resolvedPathOut)> resolveConcreteCallPath;
  std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)> resolveReceiverRootExpr{};
  std::function<bool(const Expr &, const ExprSubstitutions &, std::string &)> resolveStandaloneRefRootExpr{};
  std::function<bool(const Expr &expr)> isStandaloneRefCall;
  std::function<bool(const std::string &borrowRoot, bool requestMutable)> hasBorrowConflictForRoot;
  std::function<bool(const std::string &borrowRoot)> isMutableRootBinding;
  const Expr * target{};
  std::function<bool(const Expr &, std::string &)> resolveBorrowRootExpr{};
};

} // namespace primec::semantics
