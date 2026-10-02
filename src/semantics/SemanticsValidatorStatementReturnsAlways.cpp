#include "SemanticsValidator.h"

#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionHelperNames.h"

#include <array>
#include <cctype>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "SemanticsValidatorStatementReturnsHelpers.h"

namespace primec::semantics {
using namespace statementReturnsHelpers;

bool SemanticsValidator::statementAlwaysReturns(const Expr &stmt) {
  auto branchAlwaysReturns = [&](const Expr &branch) -> bool {
    if (isEnvelopeValueExpr(branch, true)) {
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
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.isMethodCall &&
      !stmt.isBraceConstructor && !stmt.isLambda && !stmt.name.empty()) {
    // A call to a never-returning definition terminates this control path.
    const std::string resolved = resolveCalleePath(stmt);
    const auto defIt = defMap_.find(resolved.empty() ? stmt.name : resolved);
    if (defIt != defMap_.end() && defIt->second != nullptr &&
        definitionHasNeverReturn(*defIt->second)) {
      return true;
    }
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
