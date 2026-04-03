#include "SemanticsValidator.h"

#include <string>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprBodyArguments(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    std::string &resolved,
    bool &resolvedMethod,
    const BuiltinCollectionDispatchResolverAdapters &builtinCollectionDispatchResolverAdapters,
    const std::vector<Expr> *enclosingStatements,
    size_t statementIndex) {
  auto publishExprBodyArgumentDiagnostic = [&](const Expr &diagnosticExpr) -> bool {
    captureExprContext(diagnosticExpr);
    return publishCurrentStructuredDiagnosticNow();
  };
  auto failExprBodyArgumentDiagnostic = [&](const Expr &diagnosticExpr,
                                            std::string message) -> bool {
    error_ = std::move(message);
    return publishExprBodyArgumentDiagnostic(diagnosticExpr);
  };
  if (!(expr.hasBodyArguments || !expr.bodyArguments.empty()) ||
      isBuiltinBlockCall(expr)) {
    return true;
  }

  std::string remappedRemovedMapBodyArgumentTarget;
  const bool remappedRemovedMapTarget =
      this->resolveRemovedMapBodyArgumentTarget(
          expr, resolved, params, locals,
          builtinCollectionDispatchResolverAdapters,
          remappedRemovedMapBodyArgumentTarget);
  if (remappedRemovedMapTarget) {
    resolved = remappedRemovedMapBodyArgumentTarget;
    resolvedMethod = false;
  } else if (!resolvedMethod &&
             !this->shouldPreserveRemovedCollectionHelperPath(resolved)) {
    resolved = preferVectorStdlibHelperPath(resolved);
  }

  if (resolvedMethod ||
      (!hasDeclaredDefinitionPath(resolved) &&
       !hasImportedDefinitionPath(resolved))) {
    return failExprBodyArgumentDiagnostic(
        expr, "block arguments require a definition target: " + resolved);
  }

  std::unordered_map<std::string, BindingInfo> blockLocals = locals;
  std::vector<Expr> livenessStatements = expr.bodyArguments;
  if (enclosingStatements != nullptr &&
      statementIndex < enclosingStatements->size()) {
    for (size_t idx = statementIndex + 1; idx < enclosingStatements->size();
         ++idx) {
      livenessStatements.push_back((*enclosingStatements)[idx]);
    }
  }
  OnErrorScope onErrorScope(*this, std::nullopt);
  BorrowEndScope borrowScope(*this,
                             currentValidationContext_.endedReferenceBorrows);
  for (size_t bodyIndex = 0; bodyIndex < expr.bodyArguments.size();
       ++bodyIndex) {
    const Expr &bodyExpr = expr.bodyArguments[bodyIndex];
    if (!validateStatement(params, blockLocals, bodyExpr, ReturnKind::Unknown,
                           false, true, nullptr, expr.namespacePrefix,
                           &expr.bodyArguments, bodyIndex)) {
      return false;
    }
    expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements,
                                       bodyIndex + 1);
  }
  return true;
}

} // namespace primec::semantics
