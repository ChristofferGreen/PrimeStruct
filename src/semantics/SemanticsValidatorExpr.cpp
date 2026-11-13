#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/frontend/StringLiteral.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>
#include "SemanticsValidatorExprCallContext.h"

namespace primec::semantics {

bool SemanticsValidator::validateExpr(const std::vector<ParameterInfo> &params,
                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                      const Expr &expr,
                                      const std::vector<Expr> *enclosingStatements,
                                      size_t statementIndex,
                                      bool expressionIsStatementContext) {
  ExprContextScope exprScope(*this, expr);
  observeLocalMapSize(locals.size());
  if (expr.kind == Expr::Kind::Call) {
    observeCallVisited();
  }
  auto publishExprRootDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  auto failExprRootDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  if (expr.isLambda) {
    return validateLambdaExpr(params, locals, expr, enclosingStatements, statementIndex);
  }
  if (!allowEntryArgStringUse_) {
    if (isEntryArgsAccess(expr) || isEntryArgStringBinding(locals, expr)) {
      return failExprRootDiagnostic(
          "entry argument strings are only supported in print calls or string bindings");
    }
  }
  std::optional<EffectScope> effectScope;
  if (expr.kind == Expr::Kind::Call && !expr.isBinding && !expr.transforms.empty()) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(expr, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  if (expr.kind == Expr::Kind::Literal) {
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    ParsedStringLiteral parsed;
    if (!parseStringLiteralToken(expr.stringValue, parsed, error_)) {
      return publishExprRootDiagnostic();
    }
    if (parsed.encoding == StringEncoding::Ascii && !isAsciiText(parsed.decoded)) {
      return failExprRootDiagnostic("ascii string literal contains non-ASCII characters");
    }
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isParam(params, expr.name) || locals.count(expr.name) > 0) {
      if (currentValidationState_.movedBindings.count(expr.name) > 0) {
        // The argument that moved the binding into a `move` parameter, validated again.
        const auto site = currentValidationState_.moveArgumentSites.find(expr.name);
        if (site == currentValidationState_.moveArgumentSites.end() ||
            !site->second.matches(expr)) {
          return failExprRootDiagnostic(useAfterMoveMessage(expr.name, params, locals));
        }
      }
      return true;
    }
    if (currentValidationState_.compileTimeTypeLocals.count(expr.name) > 0) {
      return failExprRootDiagnostic("type local is not a runtime value: " + expr.name);
    }
    if (!allowMathBareName(expr.name) && expr.name.find('/') == std::string::npos &&
        isBuiltinMathConstant(expr.name, true)) {
      return failExprRootDiagnostic(
          "math constant requires import /std/math/* or /std/math/<name>: " + expr.name);
    }
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return true;
    }
    return failExprRootDiagnostic("unknown identifier: " + expr.name);
  }
  if (expr.kind == Expr::Kind::Call) {
    ExprCallContext cc{params, locals, expr};
    cc.enclosingStatements = enclosingStatements;
    cc.statementIndex = statementIndex;
    cc.expressionIsStatementContext = expressionIsStatementContext;
    // Ordered rules: each either decides the call (Done) or leaves it to the next.
    using Rule = PhaseStatus (SemanticsValidator::*)(ExprCallContext &);
    static constexpr Rule Rules[] = {&SemanticsValidator::checkCallFormAndTasks,
                                     &SemanticsValidator::checkNamedArgumentsAndControlFlow,
                                     &SemanticsValidator::rewriteVectorAndFieldAccessCalls,
                                     &SemanticsValidator::resolveVectorHelperCalls,
                                     &SemanticsValidator::resolveCollectionMethodTargets,
                                     &SemanticsValidator::resolveCollectionTargets,
                                     &SemanticsValidator::resolveCallee,
                                     &SemanticsValidator::validateResolvedCall};
    for (const Rule rule : Rules) {
      if ((this->*rule)(cc) == PhaseStatus::Done) {
        return cc.result && checkBuiltinVectorMutation(params, locals, expr);
      }
    }
  }
  return false;
}

} // namespace primec::semantics
