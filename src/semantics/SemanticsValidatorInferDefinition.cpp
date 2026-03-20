#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::recordDefinitionInferredReturn(
    const Definition &def,
    const Expr *valueExpr,
    const std::vector<ParameterInfo> &defParams,
    const std::unordered_map<std::string, BindingInfo> &activeLocals,
    DefinitionReturnInferenceState &state) {
  auto containsDeferredMapAliasInference = [&](const Expr &candidate, auto &&containsDeferredMapAliasInferenceRef)
      -> bool {
    std::string builtinAccessName;
    if (candidate.kind == Expr::Kind::Call && candidate.isMethodCall &&
        getBuiltinArrayAccessName(candidate, builtinAccessName)) {
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (resolvedPath == "/std/collections/map/at" || resolvedPath == "/std/collections/map/at_unsafe" ||
          resolvedPath == "/map/at" || resolvedPath == "/map/at_unsafe") {
        return true;
      }
    }
    for (const auto &arg : candidate.args) {
      if (containsDeferredMapAliasInferenceRef(arg, containsDeferredMapAliasInferenceRef)) {
        return true;
      }
    }
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (containsDeferredMapAliasInferenceRef(bodyExpr, containsDeferredMapAliasInferenceRef)) {
        return true;
      }
    }
    return false;
  };
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
      const bool shouldDeferExplicitMapAliasDiagnostic =
          valueExpr != nullptr &&
          containsDeferredMapAliasInference(*valueExpr, containsDeferredMapAliasInference);
      if (!error_.empty() && !shouldDeferExplicitMapAliasDiagnostic) {
        return false;
      }
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
      (void)inferBindingTypeFromInitializer(stmt.args.front(), defParams, activeLocals, info, &stmt);
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

bool SemanticsValidator::inferDefinitionReturnKind(const Definition &def) {
  auto kindIt = returnKinds_.find(def.fullPath);
  if (kindIt == returnKinds_.end()) {
    return false;
  }
  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArgs.front() == "auto") {
      hasReturnAuto = true;
    }
  }
  if (kindIt->second != ReturnKind::Unknown) {
    return true;
  }
  if (!inferenceStack_.insert(def.fullPath).second) {
    error_ = "return type inference requires explicit annotation on " + def.fullPath;
    return false;
  }
  DefinitionReturnInferenceState inferenceState;
  bool sawExplicitReturnStmt = false;
  size_t implicitReturnStmtIndex = def.statements.size();
  const auto &defParams = paramsByDef_[def.fullPath];
  std::unordered_map<std::string, BindingInfo> locals;
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    if (isReturnCall(def.statements[stmtIndex])) {
      sawExplicitReturnStmt = true;
      break;
    }
    if (!def.statements[stmtIndex].isBinding) {
      implicitReturnStmtIndex = stmtIndex;
    }
  }
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    const Expr &stmt = def.statements[stmtIndex];
    if (!inferDefinitionStatementReturns(def, defParams, stmt, locals, inferenceState)) {
      return false;
    }
    if (hasReturnTransform && hasReturnAuto && !sawExplicitReturnStmt &&
        stmtIndex == implicitReturnStmtIndex && !inferenceState.sawReturn) {
      if (!recordDefinitionInferredReturn(def, &stmt, defParams, locals, inferenceState)) {
        return false;
      }
    }
  }
  if (def.returnExpr.has_value()) {
    if (!recordDefinitionInferredReturn(def, &*def.returnExpr, defParams, locals, inferenceState)) {
      return false;
    }
  }
  if (!inferenceState.sawReturn) {
    if (hasReturnTransform && hasReturnAuto) {
      if (error_.empty()) {
        error_ = "unable to infer return type on " + def.fullPath;
      }
      return false;
    }
    kindIt->second = ReturnKind::Void;
  } else if (inferenceState.inferred == ReturnKind::Unknown) {
    if (error_.empty()) {
      error_ = "unable to infer return type on " + def.fullPath;
    }
    return false;
  } else {
    kindIt->second = inferenceState.inferred;
  }
  if (!inferenceState.inferredStructPath.empty()) {
    returnStructs_[def.fullPath] = inferenceState.inferredStructPath;
  }
  inferenceStack_.erase(def.fullPath);
  return true;
}

} // namespace primec::semantics
