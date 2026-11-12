#pragma once

// What the rules and helpers that validate a `Reference<T>` binding read: the statement and its scope,
// the outer binding state, and the two things the rules find as they go. Data only; the helpers are members of
// SemanticsValidator (SemanticsValidatorBindingReferenceResolver.cpp).
#include "SemanticsValidator.h"
#include "SemanticsValidatorBindingContext.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace primec::semantics {

using ExprSubstitutions = std::vector<std::pair<std::string, const Expr *>>;

struct BindingReferenceContext {
  const std::vector<ParameterInfo> &params;
  std::unordered_map<std::string, BindingInfo> &locals;
  const Expr &stmt;
  const std::string &namespacePrefix;
  BindingContext &st;
  // The initializer of the binding.
  const Expr &init;
  // What the binding borrows from: the argument of `location(...)`/`borrow(...)`, else the initializer.
  const Expr *target{};
  PhaseStatus result{};
  PhaseStatus done(PhaseStatus value) {
    result = std::move(value);
    return PhaseStatus::Done;
  }
};

} // namespace primec::semantics
