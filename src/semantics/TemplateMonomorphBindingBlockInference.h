#pragma once

bool inferBlockBodyBindingTypeForMonomorph(const Expr &initializer,
                                           const std::vector<ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           BindingInfo &infoOut) {
  if (initializer.kind != Expr::Kind::Call || (!initializer.hasBodyArguments && initializer.bodyArguments.empty())) {
    return false;
  }
  const std::string resolved = resolveCalleePath(initializer, initializer.namespacePrefix, ctx);
  if (ctx.sourceDefs.count(resolved) > 0) {
    return false;
  }
  if (!initializer.args.empty() || !initializer.templateArgs.empty() || hasNamedArguments(initializer.argNames)) {
    return false;
  }
  if (initializer.bodyArguments.empty()) {
    return false;
  }

  LocalTypeMap blockLocals = locals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &bodyExpr : initializer.bodyArguments) {
    if (bodyExpr.isBinding) {
      BindingInfo binding;
      if (extractExplicitBindingType(bodyExpr, binding)) {
        if (binding.typeName == "auto" && bodyExpr.args.size() == 1 &&
            inferBindingTypeForMonomorph(bodyExpr.args.front(), params, blockLocals, allowMathBare, ctx, binding)) {
          blockLocals[bodyExpr.name] = binding;
        } else {
          blockLocals[bodyExpr.name] = binding;
        }
      } else if (bodyExpr.args.size() == 1) {
        if (inferBindingTypeForMonomorph(bodyExpr.args.front(), params, blockLocals, allowMathBare, ctx, binding)) {
          blockLocals[bodyExpr.name] = binding;
        }
      }
      continue;
    }
    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return false;
      }
      valueExpr = &bodyExpr.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &bodyExpr;
    }
  }
  if (!valueExpr) {
    return false;
  }
  return inferBindingTypeForMonomorph(*valueExpr, params, blockLocals, allowMathBare, ctx, infoOut);
}
