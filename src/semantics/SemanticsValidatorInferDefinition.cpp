#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::recordDefinitionInferredReturn(
    const Definition &def,
    const Expr *valueExpr,
    const std::vector<ParameterInfo> &defParams,
    const std::unordered_map<std::string, BindingInfo> &activeLocals,
    DefinitionReturnInferenceState &state) {
  state.sawReturn = true;
  ReturnKind exprKind = ReturnKind::Void;
  std::string exprStructPath;
  if (valueExpr != nullptr) {
    exprKind = inferExprReturnKind(*valueExpr, defParams, activeLocals);
    if (exprKind == ReturnKind::Array || exprKind == ReturnKind::Unknown) {
      exprStructPath = inferStructReturnPath(*valueExpr, defParams, activeLocals);
      if (exprKind == ReturnKind::Unknown && !exprStructPath.empty()) {
        exprKind = ReturnKind::Array;
      }
    }
  }
  if (exprKind == ReturnKind::Unknown) {
    if (deferUnknownReturnInferenceErrors_) {
      state.sawUnresolvedReturnDependency = true;
      return true;
    }
    if (error_.empty()) {
      error_ = "unable to infer return type on " + def.fullPath;
    }
    return false;
  }
  if (state.inferred == ReturnKind::Unknown) {
    state.inferred = exprKind;
    if (!exprStructPath.empty()) {
      state.inferredStructPath = exprStructPath;
    }
    return true;
  }
  if (state.inferred != exprKind) {
    if (error_.empty()) {
      error_ = "conflicting return types on " + def.fullPath;
    }
    return false;
  }
  if (state.inferred == ReturnKind::Array) {
    if (!exprStructPath.empty()) {
      if (state.inferredStructPath.empty()) {
        state.inferredStructPath = exprStructPath;
      } else if (state.inferredStructPath != exprStructPath) {
        if (error_.empty()) {
          error_ = "conflicting return types on " + def.fullPath;
        }
        return false;
      }
    } else if (!state.inferredStructPath.empty()) {
      if (error_.empty()) {
        error_ = "conflicting return types on " + def.fullPath;
      }
      return false;
    }
  }
  return true;
}

bool SemanticsValidator::inferDefinitionStatementReturns(
    const Definition &def,
    const std::vector<ParameterInfo> &defParams,
    const Expr &stmt,
    std::unordered_map<std::string, BindingInfo> &activeLocals,
    DefinitionReturnInferenceState &state) {
  if (stmt.isBinding) {
    BindingInfo info;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
      return false;
    }
    if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
      (void)inferBindingTypeFromInitializer(stmt.args.front(), defParams, activeLocals, info);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, def.namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    activeLocals.emplace(stmt.name, std::move(info));
    return true;
  }
  if (isReturnCall(stmt)) {
    const Expr *returnValue = stmt.args.empty() ? nullptr : &stmt.args.front();
    return recordDefinitionInferredReturn(def, returnValue, defParams, activeLocals, state);
  }
  if (isMatchCall(stmt)) {
    Expr expanded;
    if (!lowerMatchToIf(stmt, expanded, error_)) {
      return false;
    }
    return inferDefinitionStatementReturns(def, defParams, expanded, activeLocals, state);
  }
  auto inferStatementBody = [&](const std::vector<Expr> &body,
                                std::unordered_map<std::string, BindingInfo> bodyLocals) {
    for (const auto &bodyExpr : body) {
      if (!inferDefinitionStatementReturns(def, defParams, bodyExpr, bodyLocals, state)) {
        return false;
      }
    }
    return true;
  };
  if (isIfCall(stmt) && stmt.args.size() == 3) {
    return inferStatementBody(stmt.args[1].bodyArguments, activeLocals) &&
           inferStatementBody(stmt.args[2].bodyArguments, activeLocals);
  }
  if (isLoopCall(stmt) && stmt.args.size() == 2) {
    return inferStatementBody(stmt.args[1].bodyArguments, activeLocals);
  }
  if (isWhileCall(stmt) && stmt.args.size() == 2) {
    return inferStatementBody(stmt.args[1].bodyArguments, activeLocals);
  }
  if (isForCall(stmt) && stmt.args.size() == 4) {
    std::unordered_map<std::string, BindingInfo> loopLocals = activeLocals;
    if (!inferDefinitionStatementReturns(def, defParams, stmt.args[0], loopLocals, state)) {
      return false;
    }
    if (stmt.args[1].isBinding &&
        !inferDefinitionStatementReturns(def, defParams, stmt.args[1], loopLocals, state)) {
      return false;
    }
    if (!inferDefinitionStatementReturns(def, defParams, stmt.args[2], loopLocals, state)) {
      return false;
    }
    return inferStatementBody(stmt.args[3].bodyArguments, std::move(loopLocals));
  }
  if (isRepeatCall(stmt)) {
    return inferStatementBody(stmt.bodyArguments, activeLocals);
  }
  if (isBlockCall(stmt) && stmt.hasBodyArguments) {
    return inferStatementBody(stmt.bodyArguments, activeLocals);
  }
  return true;
}

} // namespace primec::semantics
