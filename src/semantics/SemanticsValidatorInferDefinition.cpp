#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::recordDefinitionInferredReturn(
    const Definition &def,
    const Expr *valueExpr,
    const std::vector<ParameterInfo> &defParams,
    const std::unordered_map<std::string, BindingInfo> &activeLocals,
    DefinitionReturnInferenceState &state) {
  auto failInferDefinitionDiagnostic = [&](std::string message) -> bool {
    if (!error_.empty()) {
      return false;
    }
    return failDefinitionDiagnostic(def, std::move(message));
  };
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto assignBindingFromTypeText = [](const std::string &typeText, BindingInfo &bindingOut) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto normalizedBindingTypeText = [&](const BindingInfo &binding) {
    const std::string normalizedCollectionType = normalizeCollectionTypePath(binding.typeName);
    const std::string normalizedBase =
        normalizedCollectionType.empty() ? normalizeBindingTypeName(binding.typeName) : normalizedCollectionType;
    if (binding.typeTemplateArg.empty()) {
      return normalizedBase;
    }
    return normalizedBase + "<" + normalizeBindingTypeName(binding.typeTemplateArg) + ">";
  };
  auto inferVisibleSoaFieldViewHelperBinding = [&](const Expr &candidate, BindingInfo &bindingOut) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall ||
        !candidate.namespacePrefix.empty() || candidate.args.size() != 1 || candidate.name.empty()) {
      return false;
    }
    std::string helperName = candidate.name;
    if (!helperName.empty() && helperName.front() == '/') {
      helperName.erase(helperName.begin());
    }
    if (helperName.empty() || helperName == "count" || helperName == "count_ref" ||
        helperName == "get" || helperName == "get_ref" || helperName == "ref" ||
        helperName == "ref_ref" || helperName == "to_soa" ||
        helperName == "to_aos" || helperName == "to_aos_ref") {
      return false;
    }
    if (!hasVisibleSoaHelperTargetForCurrentImports(helperName)) {
      return false;
    }
    const std::string helperPath =
        preferredSoaHelperTargetForCurrentImports(helperName);
    BindingInfo receiverBinding;
    const Expr &receiver = candidate.args.front();
    if (!inferBindingTypeFromInitializer(receiver, defParams, activeLocals, receiverBinding)) {
      std::string receiverTypeText;
      if (!inferQueryExprTypeText(receiver, defParams, activeLocals, receiverTypeText) ||
          !assignBindingFromTypeText(receiverTypeText, receiverBinding)) {
        return false;
      }
    }
    if (normalizeBindingTypeName(receiverBinding.typeName) != "soa_vector" ||
        receiverBinding.typeTemplateArg.empty()) {
      return false;
    }
    return inferResolvedDirectCallBindingType(helperPath, bindingOut);
  };
  auto containsDeferredMapAliasInference = [&](const Expr &candidate, auto &&containsDeferredMapAliasInferenceRef)
      -> bool {
    std::string builtinAccessName;
    if (candidate.kind == Expr::Kind::Call && candidate.isMethodCall &&
        (getBuiltinArrayAccessName(candidate, builtinAccessName) ||
         resolveCalleePath(candidate) == "/std/collections/map/at_ref" ||
         resolveCalleePath(candidate) == "/std/collections/map/at_unsafe_ref")) {
      if (builtinAccessName.empty()) {
        builtinAccessName =
            resolveCalleePath(candidate) == "/std/collections/map/at_ref"
                ? "at_ref"
                : "at_unsafe_ref";
      }
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (resolvedPath == "/std/collections/map/at" ||
          resolvedPath == "/std/collections/map/at_ref" ||
          resolvedPath == "/std/collections/map/at_unsafe" ||
          resolvedPath == "/std/collections/map/at_unsafe_ref" ||
          resolvedPath == "/map/at" || resolvedPath == "/map/at_ref" ||
          resolvedPath == "/map/at_unsafe" ||
          resolvedPath == "/map/at_unsafe_ref") {
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
  BindingInfo exprBinding;
  bool hasExprBinding = false;
  if (valueExpr != nullptr) {
    exprKind = inferExprReturnKind(*valueExpr, defParams, activeLocals);
    if (exprKind == ReturnKind::Array || exprKind == ReturnKind::Unknown) {
      exprStructPath = inferStructReturnPath(*valueExpr, defParams, activeLocals);
      if (exprKind == ReturnKind::Unknown && !exprStructPath.empty()) {
        exprKind = ReturnKind::Array;
      }
    }
    const std::string previousError = error_;
    error_.clear();
    hasExprBinding = inferBindingTypeFromInitializer(*valueExpr, defParams, activeLocals, exprBinding);
    if (!hasExprBinding) {
      std::string inferredTypeText;
      hasExprBinding =
          inferQueryExprTypeText(*valueExpr, defParams, activeLocals, inferredTypeText) &&
          assignBindingFromTypeText(inferredTypeText, exprBinding);
    }
    if (!hasExprBinding) {
      hasExprBinding = inferVisibleSoaFieldViewHelperBinding(*valueExpr, exprBinding);
    }
    error_.clear();
    error_ = previousError;
    if (exprKind == ReturnKind::Unknown && hasExprBinding) {
      const std::string normalizedTypeName = normalizeBindingTypeName(exprBinding.typeName);
      if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
           normalizedTypeName == "soa_vector" || isMapCollectionTypeName(normalizedTypeName)) &&
          !exprBinding.typeTemplateArg.empty()) {
        exprKind = ReturnKind::Array;
      } else {
        exprKind = returnKindForTypeName(bindingTypeText(exprBinding));
      }
      if (exprKind == ReturnKind::Array) {
        exprStructPath = resolveStructTypePath(exprBinding.typeName, def.namespacePrefix, structNames_);
      }
      if (exprKind != ReturnKind::Unknown) {
        error_.clear();
      }
    }
  }
  if (exprKind == ReturnKind::Unknown) {
    if (valueExpr != nullptr) {
      if (const auto pendingPath =
              builtinSoaDirectPendingHelperPath(*valueExpr, defParams,
                                                activeLocals)) {
        std::string pendingFieldName;
        if (splitSoaFieldViewHelperPath(*pendingPath, &pendingFieldName)) {
          return failInferDefinitionDiagnostic("field-view escapes via return");
        }
        return failInferDefinitionDiagnostic(
            soaUnavailableMethodDiagnostic(*pendingPath));
      }
    }
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
    return failInferDefinitionDiagnostic("unable to infer return type on " +
                                         def.fullPath);
  }
  if (state.inferred == ReturnKind::Unknown) {
    state.inferred = exprKind;
    if (!exprStructPath.empty()) {
      state.inferredStructPath = exprStructPath;
    }
    if (hasExprBinding) {
      state.inferredBinding = exprBinding;
      state.hasInferredBinding = true;
    }
    return true;
  }
  if (state.inferred != exprKind) {
    return failInferDefinitionDiagnostic("conflicting return types on " +
                                         def.fullPath);
  }
  if (state.inferred == ReturnKind::Array) {
    if (!exprStructPath.empty()) {
      if (state.inferredStructPath.empty()) {
        state.inferredStructPath = exprStructPath;
      } else if (state.inferredStructPath != exprStructPath) {
        return failInferDefinitionDiagnostic("conflicting return types on " +
                                             def.fullPath);
      }
    } else if (!state.inferredStructPath.empty()) {
      return failInferDefinitionDiagnostic("conflicting return types on " +
                                           def.fullPath);
    }
  }
  if (hasExprBinding) {
    if (!state.hasInferredBinding) {
      state.inferredBinding = exprBinding;
      state.hasInferredBinding = true;
    } else if (normalizedBindingTypeText(state.inferredBinding) !=
               normalizedBindingTypeText(exprBinding)) {
      return failInferDefinitionDiagnostic("conflicting return types on " +
                                           def.fullPath);
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
  auto failInferDefinitionStatementDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(stmt, std::move(message));
  };
  if (stmt.isBinding) {
    BindingInfo info;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, info, restrictType, error_,
                          &sumNames_)) {
      return false;
    }
    const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
    const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(info.typeName) == "auto";
    if (stmt.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
      (void)inferBindingTypeFromInitializer(stmt.args.front(), defParams, activeLocals, info, &stmt);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, def.namespacePrefix)) {
        return failInferDefinitionStatementDiagnostic(
            "restrict type does not match binding type");
      }
    }
    insertLocalBinding(activeLocals, stmt.name, std::move(info));
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
                                std::unordered_map<std::string, BindingInfo> &bodyLocals) {
    LocalBindingScope bodyScope(*this, bodyLocals);
    for (const auto &bodyExpr : body) {
      if (!inferDefinitionStatementReturns(def, defParams, bodyExpr, bodyLocals, state)) {
        return false;
      }
    }
    return true;
  };
  if (isIfCall(stmt) && stmt.args.size() == 3) {
    {
      LocalBindingScope thenScope(*this, activeLocals);
      if (!inferStatementBody(stmt.args[1].bodyArguments, activeLocals)) {
        return false;
      }
    }
    {
      LocalBindingScope elseScope(*this, activeLocals);
      return inferStatementBody(stmt.args[2].bodyArguments, activeLocals);
    }
  }
  if (isLoopCall(stmt) && stmt.args.size() == 2) {
    LocalBindingScope loopScope(*this, activeLocals);
    return inferStatementBody(stmt.args[1].bodyArguments, activeLocals);
  }
  if (isWhileCall(stmt) && stmt.args.size() == 2) {
    LocalBindingScope whileScope(*this, activeLocals);
    return inferStatementBody(stmt.args[1].bodyArguments, activeLocals);
  }
  if (isForCall(stmt) && stmt.args.size() == 4) {
    LocalBindingScope loopScope(*this, activeLocals);
    if (!inferDefinitionStatementReturns(def, defParams, stmt.args[0], activeLocals, state)) {
      return false;
    }
    if (stmt.args[1].isBinding &&
        !inferDefinitionStatementReturns(def, defParams, stmt.args[1], activeLocals, state)) {
      return false;
    }
    if (!inferDefinitionStatementReturns(def, defParams, stmt.args[2], activeLocals, state)) {
      return false;
    }
    return inferStatementBody(stmt.args[3].bodyArguments, activeLocals);
  }
  if (isRepeatCall(stmt)) {
    LocalBindingScope repeatScope(*this, activeLocals);
    return inferStatementBody(stmt.bodyArguments, activeLocals);
  }
  if (isBlockCall(stmt) && stmt.hasBodyArguments) {
    LocalBindingScope blockScope(*this, activeLocals);
    return inferStatementBody(stmt.bodyArguments, activeLocals);
  }
  return true;
}

bool SemanticsValidator::inferDefinitionReturnKind(const Definition &def) {
  auto failInferDefinitionDiagnostic = [&](std::string message) -> bool {
    if (!error_.empty()) {
      return false;
    }
    return failDefinitionDiagnostic(def, std::move(message));
  };
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
    return failInferDefinitionDiagnostic(
        "return type inference requires explicit annotation on " +
        def.fullPath);
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
      return failInferDefinitionDiagnostic("unable to infer return type on " +
                                           def.fullPath);
    }
    kindIt->second = ReturnKind::Void;
    returnBindings_.erase(def.fullPath);
  } else if (inferenceState.inferred == ReturnKind::Unknown) {
    return failInferDefinitionDiagnostic("unable to infer return type on " +
                                         def.fullPath);
  } else {
    kindIt->second = inferenceState.inferred;
    if (inferenceState.hasInferredBinding) {
      returnBindings_[def.fullPath] = inferenceState.inferredBinding;
    } else {
      returnBindings_.erase(def.fullPath);
    }
  }
  if (!inferenceState.inferredStructPath.empty()) {
    returnStructs_[def.fullPath] = inferenceState.inferredStructPath;
  }
  inferenceStack_.erase(def.fullPath);
  return true;
}

} // namespace primec::semantics
