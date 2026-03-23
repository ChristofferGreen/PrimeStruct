#pragma once

bool tryAppendDefinitionParameterBinding(Expr &param,
                                         bool allowMathBare,
                                         Context &ctx,
                                         LocalTypeMap &locals,
                                         std::vector<ParameterInfo> &paramsOut) {
  BindingInfo info;
  if (extractExplicitBindingType(param, info)) {
    if (info.typeName == "auto" && param.args.size() == 1 &&
        inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
      locals[param.name] = info;
    } else {
      locals[param.name] = info;
    }
    ParameterInfo paramInfo;
    paramInfo.name = param.name;
    paramInfo.binding = info;
    if (param.args.size() == 1) {
      paramInfo.defaultExpr = &param.args.front();
    }
    paramsOut.push_back(std::move(paramInfo));
    return true;
  }
  if (!param.isBinding || param.args.size() != 1) {
    return false;
  }
  if (!inferBindingTypeForMonomorph(param.args.front(), {}, {}, allowMathBare, ctx, info)) {
    return false;
  }
  locals[param.name] = info;
  ParameterInfo paramInfo;
  paramInfo.name = param.name;
  paramInfo.binding = info;
  paramInfo.defaultExpr = &param.args.front();
  paramsOut.push_back(std::move(paramInfo));
  return true;
}

bool rewriteDefinitionParameters(std::vector<Expr> &parameters,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 LocalTypeMap &locals,
                                 std::vector<ParameterInfo> &paramsOut,
                                 bool allowMathBare) {
  for (auto &param : parameters) {
    if (!rewriteExpr(param, mapping, allowedParams, namespacePrefix, ctx, error, locals, paramsOut, allowMathBare)) {
      return false;
    }
    tryAppendDefinitionParameterBinding(param, allowMathBare, ctx, locals, paramsOut);
  }
  return true;
}

void recordDefinitionStatementBindingLocal(Expr &stmt,
                                           const std::vector<ParameterInfo> &params,
                                           const LocalTypeMap &locals,
                                           bool allowMathBare,
                                           Context &ctx,
                                           LocalTypeMap &localsOut) {
  BindingInfo info;
  if (extractExplicitBindingType(stmt, info)) {
    if (info.typeName == "auto" && stmt.args.size() == 1 &&
        inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
      localsOut[stmt.name] = info;
    } else {
      localsOut[stmt.name] = info;
    }
    return;
  }
  if (!stmt.isBinding || stmt.args.size() != 1) {
    return;
  }
  if (inferBindingTypeForMonomorph(stmt.args.front(), params, locals, allowMathBare, ctx, info)) {
    localsOut[stmt.name] = info;
  }
}
