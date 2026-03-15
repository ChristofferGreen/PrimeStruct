#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::statementAlwaysReturns(const Expr &stmt) {
  auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return true;
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return nullptr;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return nullptr;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return nullptr;
    }
    if (!allowAnyName && !isBuiltinBlockCall(candidate)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  auto branchAlwaysReturns = [&](const Expr &branch) -> bool {
    if (isIfBlockEnvelope(branch)) {
      return blockAlwaysReturns(branch.bodyArguments) || getEnvelopeValueExpr(branch, true) != nullptr;
    }
    return statementAlwaysReturns(branch);
  };

  if (isReturnCall(stmt)) {
    return true;
  }
  if (isMatchCall(stmt)) {
    Expr expanded;
    std::string error;
    if (!lowerMatchToIf(stmt, expanded, error)) {
      return false;
    }
    return statementAlwaysReturns(expanded);
  }
  if (isIfCall(stmt) && stmt.args.size() == 3) {
    return branchAlwaysReturns(stmt.args[1]) && branchAlwaysReturns(stmt.args[2]);
  }
  if (getEnvelopeValueExpr(stmt, false) != nullptr) {
    return true;
  }
  if (isBuiltinBlockCall(stmt) && stmt.hasBodyArguments) {
    return blockAlwaysReturns(stmt.bodyArguments);
  }
  return false;
}

bool SemanticsValidator::blockAlwaysReturns(const std::vector<Expr> &statements) {
  for (const auto &stmt : statements) {
    if (statementAlwaysReturns(stmt)) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
