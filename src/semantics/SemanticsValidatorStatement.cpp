#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateStatement(const std::vector<ParameterInfo> &params,
                                           std::unordered_map<std::string, BindingInfo> &locals,
                                           const Expr &stmt,
                                           ReturnKind returnKind,
                                           bool allowReturn,
                                           bool allowBindings,
                                           bool *sawReturn,
                                           const std::string &namespacePrefix) {
  auto returnKindForBinding = [&](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };
  auto isStringExpr = [&](const Expr &arg,
                          const std::vector<ParameterInfo> &paramsIn,
                          const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          if (paramBinding->typeName != "array" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemTypeOut = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end() || it->second.typeName != "array" || it->second.typeTemplateArg.empty()) {
          return false;
        }
        elemTypeOut = it->second.typeTemplateArg;
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && collection == "array" && target.templateArgs.size() == 1) {
          elemTypeOut = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(paramsIn, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          valueTypeOut = parts[1];
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        valueTypeOut = parts[1];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
          return false;
        }
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      return false;
    };
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = localsIn.find(arg.name);
      return it != localsIn.end() && it->second.typeName == "string";
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName) && arg.args.size() == 2) {
        std::string elemType;
        if (resolveArrayTarget(arg.args.front(), elemType) && normalizeBindingTypeName(elemType) == "string") {
          return true;
        }
        std::string mapValueType;
        if (resolveMapValueType(arg.args.front(), mapValueType) &&
            normalizeBindingTypeName(mapValueType) == "string") {
          return true;
        }
      }
    }
    return false;
  };
  auto isPrintableExpr = [&](const Expr &arg,
                             const std::vector<ParameterInfo> &paramsIn,
                             const std::unordered_map<std::string, BindingInfo> &localsIn) -> bool {
    if (isStringExpr(arg, paramsIn, localsIn)) {
      return true;
    }
    ReturnKind kind = inferExprReturnKind(arg, paramsIn, localsIn);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64) {
      return true;
    }
    if (kind == ReturnKind::Void) {
      return false;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
               paramKind == ReturnKind::Bool || paramKind == ReturnKind::Float32 || paramKind == ReturnKind::Float64;
      }
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
               localKind == ReturnKind::Bool || localKind == ReturnKind::Float32 || localKind == ReturnKind::Float64;
      }
    }
    if (isPointerLikeExpr(arg, paramsIn, localsIn)) {
      return false;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string builtinName;
      if (getBuiltinCollectionName(arg, builtinName)) {
        return false;
      }
    }
    return false;
  };

  if (stmt.isBinding) {
    if (!allowBindings) {
      error_ = "binding not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "binding does not accept block arguments";
      return false;
    }
    if (isParam(params, stmt.name) || locals.count(stmt.name) > 0) {
      error_ = "duplicate binding name: " + stmt.name;
      return false;
    }
    BindingInfo info;
    std::optional<std::string> restrictType;
    if (!parseBindingInfo(stmt, namespacePrefix, structNames_, info, restrictType, error_)) {
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = "binding requires exactly one argument";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args.front())) {
      return false;
    }
    if (!hasExplicitBindingTypeTransform(stmt)) {
      (void)tryInferBindingTypeFromInitializer(stmt.args.front(), params, locals, info);
    }
    if (restrictType.has_value()) {
      const bool hasTemplate = !info.typeTemplateArg.empty();
      if (!restrictMatchesBinding(*restrictType, info.typeName, info.typeTemplateArg, hasTemplate, namespacePrefix)) {
        error_ = "restrict type does not match binding type";
        return false;
      }
    }
    if (info.typeName == "Reference") {
      const Expr &init = stmt.args.front();
      std::string pointerName;
      if (init.kind != Expr::Kind::Call || !getBuiltinPointerName(init, pointerName) ||
          pointerName != "location" || init.args.size() != 1) {
        error_ = "Reference bindings require location(...)";
        return false;
      }
    }
    locals.emplace(stmt.name, info);
    return true;
  }
  if (stmt.kind != Expr::Kind::Call) {
    if (!allowBindings) {
      error_ = "execution body arguments must be calls";
      return false;
    }
    return validateExpr(params, locals, stmt);
  }
  if (isReturnCall(stmt)) {
    if (!allowReturn) {
      error_ = "return not allowed in execution body";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "return does not accept block arguments";
      return false;
    }
    if (returnKind == ReturnKind::Void) {
      if (!stmt.args.empty()) {
        error_ = "return value not allowed for void definition";
        return false;
      }
    } else {
      if (stmt.args.size() != 1) {
        error_ = "return requires exactly one argument";
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      if (returnKind != ReturnKind::Unknown) {
        ReturnKind exprKind = inferExprReturnKind(stmt.args.front(), params, locals);
        if (exprKind != returnKind) {
          error_ = "return type mismatch: expected " + typeNameForReturnKind(returnKind);
          return false;
        }
      }
    }
    if (sawReturn) {
      *sawReturn = true;
    }
    return true;
  }
  if (isIfCall(stmt)) {
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "if does not accept trailing block arguments";
      return false;
    }
    if (stmt.args.size() != 3) {
      error_ = "if requires condition, then, else";
      return false;
    }
    const Expr &cond = stmt.args[0];
    const Expr &thenArg = stmt.args[1];
    const Expr &elseArg = stmt.args[2];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      error_ = "if condition requires bool";
      return false;
    }
    auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      const std::string resolved = resolveCalleePath(candidate);
      return defMap_.find(resolved) == defMap_.end();
    };
    auto validateBranch = [&](const Expr &branch) -> bool {
      std::unordered_map<std::string, BindingInfo> branchLocals = locals;
      if (isIfBlockEnvelope(branch)) {
        for (const auto &bodyExpr : branch.bodyArguments) {
          if (!validateStatement(params,
                                 branchLocals,
                                 bodyExpr,
                                 returnKind,
                                 allowReturn,
                                 allowBindings,
                                 sawReturn,
                                 namespacePrefix)) {
            return false;
          }
        }
        return true;
      }
      return validateStatement(params, branchLocals, branch, returnKind, allowReturn, allowBindings, sawReturn, namespacePrefix);
    };
    if (!validateBranch(thenArg)) {
      return false;
    }
    if (!validateBranch(elseArg)) {
      return false;
    }
    return true;
  }
  if (isRepeatCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "repeat does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = "repeat requires exactly one argument";
      return false;
    }
    const Expr &count = stmt.args.front();
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64 &&
        countKind != ReturnKind::Bool) {
      error_ = "repeat count requires integer or bool";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    for (const auto &bodyExpr : stmt.bodyArguments) {
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  if (isBlockCall(stmt) && stmt.hasBodyArguments) {
    const std::string resolved = resolveCalleePath(stmt);
    if (defMap_.find(resolved) != defMap_.end()) {
      error_ = "block arguments require a definition target: " + resolved;
      return false;
    }
    if (hasNamedArguments(stmt.argNames) || !stmt.args.empty() || !stmt.templateArgs.empty()) {
      error_ = "block does not accept arguments";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    for (const auto &bodyExpr : stmt.bodyArguments) {
      if (!validateStatement(params,
                             blockLocals,
                             bodyExpr,
                             returnKind,
                             allowReturn,
                             allowBindings,
                             sawReturn,
                             namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  PrintBuiltin printBuiltin;
  if (getPrintBuiltin(stmt, printBuiltin)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = printBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = printBuiltin.name + " requires exactly one argument";
      return false;
    }
    const std::string effectName = (printBuiltin.target == PrintTarget::Err) ? "io_err" : "io_out";
    if (activeEffects_.count(effectName) == 0) {
      error_ = printBuiltin.name + " requires " + effectName + " effect";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args.front())) {
      return false;
    }
    if (!isPrintableExpr(stmt.args.front(), params, locals)) {
      error_ = printBuiltin.name + " requires a numeric/bool or string literal/binding argument";
      return false;
    }
    return true;
  }
  PathSpaceBuiltin pathSpaceBuiltin;
  if (getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && defMap_.find(resolveCalleePath(stmt)) == defMap_.end()) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = pathSpaceBuiltin.name + " does not accept block arguments";
      return false;
    }
    if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
      error_ = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
               " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
      return false;
    }
    if (activeEffects_.count(pathSpaceBuiltin.requiredEffect) == 0) {
      error_ = pathSpaceBuiltin.name + " requires " + pathSpaceBuiltin.requiredEffect + " effect";
      return false;
    }
    auto isStringExpr = [&](const Expr &candidate) -> bool {
      if (candidate.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      auto isStringTypeName = [&](const std::string &typeName) -> bool {
        return normalizeBindingTypeName(typeName) == "string";
      };
      auto isStringArrayBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "array") {
          return false;
        }
        return isStringTypeName(binding.typeTemplateArg);
      };
      auto isStringMapBinding = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(binding.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        return isStringTypeName(parts[1]);
      };
      auto isStringCollectionTarget = [&](const Expr &target) -> bool {
        if (target.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            return isStringArrayBinding(*paramBinding) || isStringMapBinding(*paramBinding);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            return isStringArrayBinding(it->second) || isStringMapBinding(it->second);
          }
        }
        if (target.kind == Expr::Kind::Call) {
          std::string collection;
          if (!getBuiltinCollectionName(target, collection)) {
            return false;
          }
          if (collection == "array" && target.templateArgs.size() == 1) {
            return isStringTypeName(target.templateArgs.front());
          }
          if (collection == "map" && target.templateArgs.size() == 2) {
            return isStringTypeName(target.templateArgs[1]);
          }
        }
        return false;
      };
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          return isStringTypeName(paramBinding->typeName);
        }
        auto it = locals.find(candidate.name);
        return it != locals.end() && isStringTypeName(it->second.typeName);
      }
      if (candidate.kind == Expr::Kind::Call) {
        std::string accessName;
        if (getBuiltinArrayAccessName(candidate, accessName) && candidate.args.size() == 2) {
          return isStringCollectionTarget(candidate.args.front());
        }
      }
      return false;
    };
    if (!isStringExpr(stmt.args.front())) {
      error_ = pathSpaceBuiltin.name + " requires string path argument";
      return false;
    }
    for (const auto &arg : stmt.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    std::string collectionName;
    if (getBuiltinCollectionName(stmt, collectionName)) {
      error_ = collectionName + " literal does not accept block arguments";
      return false;
    }
    std::string resolved = resolveCalleePath(stmt);
    if (defMap_.count(resolved) == 0) {
      error_ = "block arguments require a definition target: " + resolved;
      return false;
    }
    Expr call = stmt;
    call.bodyArguments.clear();
    call.hasBodyArguments = false;
    if (!validateExpr(params, locals, call)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = locals;
    for (const auto &bodyExpr : stmt.bodyArguments) {
      if (!validateStatement(params, blockLocals, bodyExpr, returnKind, false, false, nullptr, namespacePrefix)) {
        return false;
      }
    }
    return true;
  }
  return validateExpr(params, locals, stmt);
}

bool SemanticsValidator::statementAlwaysReturns(const Expr &stmt) {
  auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    const std::string resolved = resolveCalleePath(candidate);
    return defMap_.find(resolved) == defMap_.end();
  };
  auto branchAlwaysReturns = [&](const Expr &branch) -> bool {
    if (isIfBlockEnvelope(branch)) {
      return blockAlwaysReturns(branch.bodyArguments);
    }
    return statementAlwaysReturns(branch);
  };

  if (isReturnCall(stmt)) {
    return true;
  }
  if (isIfCall(stmt) && stmt.args.size() == 3) {
    return branchAlwaysReturns(stmt.args[1]) && branchAlwaysReturns(stmt.args[2]);
  }
  if (isBlockCall(stmt) && stmt.hasBodyArguments) {
    const std::string resolved = resolveCalleePath(stmt);
    if (defMap_.find(resolved) == defMap_.end()) {
      return blockAlwaysReturns(stmt.bodyArguments);
    }
  }
  return false;
}

bool SemanticsValidator::blockAlwaysReturns(const std::vector<Expr> &statements) {
  for (const auto &stmt : statements) {
    if (statementAlwaysReturns(stmt)) {
      return true;
    }
  }
  return false;
}

} // namespace primec::semantics
