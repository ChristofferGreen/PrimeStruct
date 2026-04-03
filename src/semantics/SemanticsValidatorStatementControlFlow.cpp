#include "SemanticsValidator.h"
#include "SemanticsValidatorStatementLoopCountStep.h"

namespace primec::semantics {
namespace {

ReturnKind returnKindForStatementControlFlowBinding(const BindingInfo &binding) {
  if (binding.typeName == "Reference") {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    return returnKindForTypeName(binding.typeTemplateArg);
  }
  return returnKindForTypeName(binding.typeName);
}

bool isLoopBlockEnvelope(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
    return false;
  }
  if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
    return false;
  }
  return true;
}

bool isBoolLiteralFalse(const Expr &expr) {
  return expr.kind == Expr::Kind::BoolLiteral && !expr.boolValue;
}

} // namespace

bool SemanticsValidator::validateControlFlowStatement(const std::vector<ParameterInfo> &params,
                                                      std::unordered_map<std::string, BindingInfo> &locals,
                                                      const Expr &stmt,
                                                      ReturnKind returnKind,
                                                      bool allowReturn,
                                                      bool allowBindings,
                                                      bool *sawReturn,
                                                      const std::string &namespacePrefix,
                                                      const std::vector<Expr> *enclosingStatements,
                                                      size_t statementIndex,
                                                      bool &handled) {
  handled = false;
  auto publishStatementDiagnostic = [&]() -> bool {
    captureExprContext(stmt);
    return publishCurrentStructuredDiagnosticNow();
  };
  auto failStatementDiagnostic = [&](std::string message) -> bool {
    error_ = std::move(message);
    return publishStatementDiagnostic();
  };

  if (isMatchCall(stmt)) {
    handled = true;
    Expr expanded;
    if (!lowerMatchToIf(stmt, expanded, error_)) {
      return false;
    }
    return validateStatement(params,
                             locals,
                             expanded,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             enclosingStatements,
                             statementIndex);
  }

  if (isIfCall(stmt)) {
    handled = true;
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic("if does not accept trailing block arguments");
    }
    if (stmt.args.size() != 3) {
      return failStatementDiagnostic("if requires condition, then, else");
    }
    const Expr &cond = stmt.args[0];
    const Expr &thenArg = stmt.args[1];
    const Expr &elseArg = stmt.args[2];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      return failStatementDiagnostic("if condition requires bool");
    }
    std::vector<Expr> postMergeStatements;
    if (enclosingStatements != nullptr) {
      size_t start = statementIndex + 1;
      if (start > enclosingStatements->size()) {
        start = enclosingStatements->size();
      }
      postMergeStatements.insert(postMergeStatements.end(),
                                 enclosingStatements->begin() + start,
                                 enclosingStatements->end());
    }
    auto validateBranch = [&](const Expr &branch) -> bool {
      if (!isIfBlockEnvelope(branch)) {
        return failStatementDiagnostic("if branches require block envelopes");
      }
      std::unordered_map<std::string, BindingInfo> branchLocals = locals;
      std::vector<Expr> livenessStatements = branch.bodyArguments;
      livenessStatements.insert(livenessStatements.end(), postMergeStatements.begin(), postMergeStatements.end());
      OnErrorScope onErrorScope(*this, std::nullopt);
      BorrowEndScope borrowScope(*this, currentValidationContext_.endedReferenceBorrows);
      for (size_t bodyIndex = 0; bodyIndex < branch.bodyArguments.size(); ++bodyIndex) {
        const Expr &bodyExpr = branch.bodyArguments[bodyIndex];
        if (!validateStatement(params,
                               branchLocals,
                               bodyExpr,
                               returnKind,
                               allowReturn,
                               allowBindings,
                               sawReturn,
                               namespacePrefix,
                               &branch.bodyArguments,
                               bodyIndex)) {
          return false;
        }
        expireReferenceBorrowsForRemainder(params, branchLocals, livenessStatements, bodyIndex + 1);
      }
      return true;
    };
    if (!validateBranch(thenArg)) {
      return false;
    }
    if (!validateBranch(elseArg)) {
      return false;
    }
    return true;
  }

  auto validateLoopBody = [&](const Expr &body,
                              const std::unordered_map<std::string, BindingInfo> &baseLocals,
                              const std::vector<Expr> &boundaryStatements,
                              bool includeNextIterationBody) -> bool {
    if (!isLoopBlockEnvelope(body)) {
      return failStatementDiagnostic("loop body requires a block envelope");
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = baseLocals;
    std::vector<Expr> livenessStatements = body.bodyArguments;
    livenessStatements.insert(livenessStatements.end(), boundaryStatements.begin(), boundaryStatements.end());
    if (includeNextIterationBody) {
      livenessStatements.insert(livenessStatements.end(), body.bodyArguments.begin(), body.bodyArguments.end());
    }
    OnErrorScope onErrorScope(*this, std::nullopt);
    BorrowEndScope borrowScope(*this, currentValidationContext_.endedReferenceBorrows);
    for (size_t bodyIndex = 0; bodyIndex < body.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = body.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix,
                             &body.bodyArguments,
                             bodyIndex)) {
        return false;
      }
      expireReferenceBorrowsForRemainder(params, blockLocals, livenessStatements, bodyIndex + 1);
    }
    return true;
  };

  auto canIterateMoreThanOnce = [&](const Expr &countExpr, bool allowBoolCount) -> bool {
    return runSemanticsValidatorStatementCanIterateMoreThanOnceStep(countExpr, allowBoolCount);
  };

  if (isLoopCall(stmt)) {
    handled = true;
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic("loop does not accept template arguments");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic("loop does not accept trailing block arguments");
    }
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("loop requires count and body");
    }
    const Expr &count = stmt.args[0];
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64) {
      return failStatementDiagnostic("loop count requires integer");
    }
    if (runSemanticsValidatorStatementIsNegativeIntegerLiteralStep(count)) {
      return failStatementDiagnostic("loop count must be non-negative");
    }
    const bool includeNextIterationBody = canIterateMoreThanOnce(count, false);
    return validateLoopBody(stmt.args[1], locals, {}, includeNextIterationBody);
  }

  if (isWhileCall(stmt)) {
    handled = true;
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic("while does not accept template arguments");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic("while does not accept trailing block arguments");
    }
    if (stmt.args.size() != 2) {
      return failStatementDiagnostic("while requires condition and body");
    }
    const Expr &cond = stmt.args[0];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      return failStatementDiagnostic("while condition requires bool");
    }
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[1], locals, boundaryStatements, !conditionAlwaysFalse);
  }

  if (isForCall(stmt)) {
    handled = true;
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic("for does not accept template arguments");
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      return failStatementDiagnostic("for does not accept trailing block arguments");
    }
    if (stmt.args.size() != 4) {
      return failStatementDiagnostic("for requires init, condition, step, and body");
    }
    std::unordered_map<std::string, BindingInfo> loopLocals = locals;
    if (!validateStatement(
            params, loopLocals, stmt.args[0], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    const Expr &cond = stmt.args[1];
    if (cond.isBinding) {
      if (!validateStatement(
              params, loopLocals, cond, returnKind, false, allowBindings, nullptr, namespacePrefix)) {
        return false;
      }
      auto it = loopLocals.find(cond.name);
      ReturnKind condKind = it == loopLocals.end() ? ReturnKind::Unknown
                                                   : returnKindForStatementControlFlowBinding(it->second);
      if (condKind != ReturnKind::Bool) {
        return failStatementDiagnostic("for condition requires bool");
      }
    } else {
      if (!validateExpr(params, loopLocals, cond)) {
        return false;
      }
      ReturnKind condKind = inferExprReturnKind(cond, params, loopLocals);
      if (condKind != ReturnKind::Bool) {
        return failStatementDiagnostic("for condition requires bool");
      }
    }
    if (!validateStatement(
            params, loopLocals, stmt.args[2], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = !cond.isBinding && isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(stmt.args[2]);
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[3], loopLocals, boundaryStatements, !conditionAlwaysFalse);
  }

  if (isRepeatCall(stmt)) {
    handled = true;
    if (!stmt.hasBodyArguments) {
      return failStatementDiagnostic("repeat requires block arguments");
    }
    if (hasNamedArguments(stmt.argNames)) {
      return failStatementDiagnostic("named arguments not supported for builtin calls");
    }
    if (!stmt.templateArgs.empty()) {
      return failStatementDiagnostic("repeat does not accept template arguments");
    }
    if (stmt.args.size() != 1) {
      return failStatementDiagnostic("repeat requires exactly one argument");
    }
    const Expr &count = stmt.args.front();
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64 &&
        countKind != ReturnKind::Bool) {
      return failStatementDiagnostic("repeat count requires integer or bool");
    }
    const bool includeNextIterationBody = canIterateMoreThanOnce(count, true);
    Expr repeatBody;
    repeatBody.kind = Expr::Kind::Call;
    repeatBody.name = "repeat_body";
    repeatBody.bodyArguments = stmt.bodyArguments;
    repeatBody.hasBodyArguments = stmt.hasBodyArguments;
    return validateLoopBody(repeatBody, locals, {}, includeNextIterationBody);
  }

  return true;
}

} // namespace primec::semantics
