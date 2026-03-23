#pragma once

bool inferCallTargetBinding(const Expr &bindingExpr,
                            bool allowMathBare,
                            Context &ctx,
                            BindingInfo &bindingOut) {
  const bool hasExplicitBinding = extractExplicitBindingType(bindingExpr, bindingOut);
  if (hasExplicitBinding && bindingOut.typeName != "auto") {
    return true;
  }
  if (bindingExpr.args.size() != 1) {
    return hasExplicitBinding;
  }
  BindingInfo inferredBinding;
  if (!inferBindingTypeForMonomorph(bindingExpr.args.front(), {}, {}, allowMathBare, ctx, inferredBinding)) {
    return hasExplicitBinding;
  }
  bindingOut = inferredBinding;
  return true;
}

bool resolveAssignmentTargetBinding(const Expr &target,
                                    const std::vector<ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx,
                                    BindingInfo &bindingOut);

bool resolveFieldBindingTarget(const Expr &target,
                               const std::vector<ParameterInfo> &params,
                               const LocalTypeMap &locals,
                               bool allowMathBare,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               BindingInfo &bindingOut) {
  if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
    return false;
  }
  const Expr &receiver = target.args.front();
  std::string receiverTypeText;
  BindingInfo receiverInfo;
  if (resolveAssignmentTargetBinding(receiver, params, locals, allowMathBare, namespacePrefix, ctx, receiverInfo)) {
    receiverTypeText = bindingTypeToString(receiverInfo);
  } else if (inferBindingTypeForMonomorph(receiver, params, locals, allowMathBare, ctx, receiverInfo)) {
    receiverTypeText = bindingTypeToString(receiverInfo);
  }
  if (receiverTypeText.empty()) {
    receiverTypeText =
        inferExprTypeTextForTemplatedVectorFallback(receiver, locals, namespacePrefix, ctx, allowMathBare);
  }
  if (receiverTypeText.empty()) {
    return false;
  }
  receiverTypeText = normalizeBindingTypeName(receiverTypeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
      break;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      receiverTypeText = base;
      break;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    receiverTypeText = normalizeBindingTypeName(args.front());
  }
  std::string receiverStructPath = receiverTypeText;
  std::string receiverBase;
  std::string receiverArgText;
  if (splitTemplateTypeName(receiverStructPath, receiverBase, receiverArgText) && !receiverBase.empty()) {
    receiverStructPath = normalizeBindingTypeName(receiverBase);
  }
  if (!receiverStructPath.empty() && receiverStructPath.front() != '/') {
    receiverStructPath = resolveTypePath(receiverStructPath, receiver.namespacePrefix);
  }
  auto structIt = ctx.sourceDefs.find(receiverStructPath);
  if (structIt == ctx.sourceDefs.end() || !isStructDefinition(structIt->second)) {
    return false;
  }
  for (const auto &fieldStmt : structIt->second.statements) {
    if (!fieldStmt.isBinding || fieldStmt.name != target.name) {
      continue;
    }
    return inferCallTargetBinding(fieldStmt, allowMathBare, ctx, bindingOut);
  }
  return false;
}

bool resolveDereferenceBindingTarget(const Expr &target,
                                     const std::vector<ParameterInfo> &params,
                                     const LocalTypeMap &locals,
                                     bool allowMathBare,
                                     const std::string &namespacePrefix,
                                     Context &ctx,
                                     BindingInfo &bindingOut) {
  if (target.kind != Expr::Kind::Call || target.args.size() != 1) {
    return false;
  }
  std::string pointerBuiltin;
  if (!getBuiltinPointerName(target, pointerBuiltin) || pointerBuiltin != "dereference") {
    return false;
  }
  auto inferPointerBinding = [&](const Expr &pointerExpr, BindingInfo &pointerOut) -> bool {
    if (inferBindingTypeForMonomorph(pointerExpr, params, locals, allowMathBare, ctx, pointerOut)) {
      return true;
    }
    if (pointerExpr.kind != Expr::Kind::Call || pointerExpr.args.size() != 1) {
      return false;
    }
    std::string nestedPointerBuiltin;
    if (!getBuiltinPointerName(pointerExpr, nestedPointerBuiltin) || nestedPointerBuiltin != "location") {
      return false;
    }
    BindingInfo pointeeInfo;
    if (!resolveAssignmentTargetBinding(
            pointerExpr.args.front(), params, locals, allowMathBare, namespacePrefix, ctx, pointeeInfo)) {
      return false;
    }
    const std::string pointeeTypeText = bindingTypeToString(pointeeInfo);
    if (pointeeTypeText.empty()) {
      return false;
    }
    pointerOut.typeName = "Reference";
    pointerOut.typeTemplateArg = pointeeTypeText;
    return true;
  };
  BindingInfo pointerInfo;
  if (!inferPointerBinding(target.args.front(), pointerInfo)) {
    return false;
  }
  const std::string normalizedPointerType = normalizeBindingTypeName(pointerInfo.typeName);
  if ((normalizedPointerType != "Reference" && normalizedPointerType != "Pointer") ||
      pointerInfo.typeTemplateArg.empty()) {
    return false;
  }
  std::string pointeeBase;
  std::string pointeeArgText;
  if (splitTemplateTypeName(pointerInfo.typeTemplateArg, pointeeBase, pointeeArgText) && !pointeeBase.empty()) {
    bindingOut.typeName = pointeeBase;
    bindingOut.typeTemplateArg = pointeeArgText;
  } else {
    bindingOut.typeName = pointerInfo.typeTemplateArg;
    bindingOut.typeTemplateArg.clear();
  }
  return true;
}

bool resolveAssignmentTargetBinding(const Expr &target,
                                    const std::vector<ParameterInfo> &params,
                                    const LocalTypeMap &locals,
                                    bool allowMathBare,
                                    const std::string &namespacePrefix,
                                    Context &ctx,
                                    BindingInfo &bindingOut) {
  return inferBindingTypeForMonomorph(target, params, locals, allowMathBare, ctx, bindingOut) ||
         resolveFieldBindingTarget(target, params, locals, allowMathBare, namespacePrefix, ctx, bindingOut) ||
         resolveDereferenceBindingTarget(target, params, locals, allowMathBare, namespacePrefix, ctx, bindingOut);
}
