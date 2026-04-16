#include "SemanticsValidator.h"
#include "SemanticsValidatorStatementLoopCountStep.h"

#include <algorithm>

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
  auto failStatementDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
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
    const std::vector<Expr> *postMergeStatements = enclosingStatements;
    const size_t postMergeStartIndex = enclosingStatements == nullptr
                                           ? 0
                                           : std::min(statementIndex + 1, enclosingStatements->size());
    auto validateBranch = [&](const Expr &branch) -> bool {
      if (!isIfBlockEnvelope(branch)) {
        return failStatementDiagnostic("if branches require block envelopes");
      }
      LocalBindingScope branchScope(*this, locals);
      std::vector<BorrowLivenessRange> livenessRanges;
      livenessRanges.reserve(2);
      OnErrorScope onErrorScope(*this, std::nullopt);
      BorrowEndScope borrowScope(*this, currentValidationState_.endedReferenceBorrows);
      for (size_t bodyIndex = 0; bodyIndex < branch.bodyArguments.size(); ++bodyIndex) {
        const Expr &bodyExpr = branch.bodyArguments[bodyIndex];
        if (!validateStatement(params,
                               locals,
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
        livenessRanges.clear();
        livenessRanges.push_back(BorrowLivenessRange{&branch.bodyArguments, bodyIndex + 1});
        if (postMergeStatements != nullptr && postMergeStartIndex < postMergeStatements->size()) {
          livenessRanges.push_back(BorrowLivenessRange{postMergeStatements, postMergeStartIndex});
        }
        expireReferenceBorrowsForRanges(params, locals, livenessRanges);
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
                              std::unordered_map<std::string, BindingInfo> &activeLocals,
                              const std::vector<Expr> &boundaryStatements,
                              bool includeNextIterationBody) -> bool {
    if (!isLoopBlockEnvelope(body)) {
      return failStatementDiagnostic("loop body requires a block envelope");
    }
    LocalBindingScope bodyScope(*this, activeLocals);
    std::vector<BorrowLivenessRange> livenessRanges;
    livenessRanges.reserve(includeNextIterationBody ? 3 : 2);
    OnErrorScope onErrorScope(*this, std::nullopt);
    BorrowEndScope borrowScope(*this, currentValidationState_.endedReferenceBorrows);
    for (size_t bodyIndex = 0; bodyIndex < body.bodyArguments.size(); ++bodyIndex) {
      const Expr &bodyExpr = body.bodyArguments[bodyIndex];
      if (!validateStatement(params,
                             activeLocals,
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
      livenessRanges.clear();
      livenessRanges.push_back(BorrowLivenessRange{&body.bodyArguments, bodyIndex + 1});
      if (!boundaryStatements.empty()) {
        livenessRanges.push_back(BorrowLivenessRange{&boundaryStatements, 0});
      }
      if (includeNextIterationBody) {
        livenessRanges.push_back(BorrowLivenessRange{&body.bodyArguments, 0});
      }
      expireReferenceBorrowsForRanges(params, activeLocals, livenessRanges);
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
    LocalBindingScope forScope(*this, locals);
    if (!validateStatement(
            params, locals, stmt.args[0], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    const Expr &cond = stmt.args[1];
    if (cond.isBinding) {
      if (!validateStatement(
              params, locals, cond, returnKind, false, allowBindings, nullptr, namespacePrefix)) {
        return false;
      }
      auto it = locals.find(cond.name);
      ReturnKind condKind = it == locals.end() ? ReturnKind::Unknown
                                               : returnKindForStatementControlFlowBinding(it->second);
      if (condKind != ReturnKind::Bool) {
        return failStatementDiagnostic("for condition requires bool");
      }
    } else {
      if (!validateExpr(params, locals, cond)) {
        return false;
      }
      ReturnKind condKind = inferExprReturnKind(cond, params, locals);
      if (condKind != ReturnKind::Bool) {
        return failStatementDiagnostic("for condition requires bool");
      }
    }
    if (!validateStatement(
            params, locals, stmt.args[2], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    std::vector<Expr> boundaryStatements;
    const bool conditionAlwaysFalse = !cond.isBinding && isBoolLiteralFalse(cond);
    if (!conditionAlwaysFalse) {
      boundaryStatements.push_back(stmt.args[2]);
      boundaryStatements.push_back(cond);
    }
    return validateLoopBody(stmt.args[3], locals, boundaryStatements, !conditionAlwaysFalse);
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
