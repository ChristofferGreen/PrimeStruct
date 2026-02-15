#include "SemanticsValidator.h"

#include <functional>
#include <optional>

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
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemTypeOut = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end() || (it->second.typeName != "array" && it->second.typeName != "vector") ||
            it->second.typeTemplateArg.empty()) {
          return false;
        }
        elemTypeOut = it->second.typeTemplateArg;
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
            target.templateArgs.size() == 1) {
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
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Void) {
      return false;
    }
    if (arg.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(paramsIn, arg.name)) {
        ReturnKind paramKind = returnKindForBinding(*paramBinding);
        return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
               paramKind == ReturnKind::Bool;
      }
      auto it = localsIn.find(arg.name);
      if (it != localsIn.end()) {
        ReturnKind localKind = returnKindForBinding(it->second);
        return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
               localKind == ReturnKind::Bool;
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
  auto isIntegerOrBoolExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 || kind == ReturnKind::Bool) {
      return true;
    }
    if (kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || kind == ReturnKind::Void) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64 ||
                 paramKind == ReturnKind::Bool;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64 ||
                 localKind == ReturnKind::Bool;
        }
      }
      return true;
    }
    return false;
  };
  auto resolveVectorBinding = [&](const Expr &target, const BindingInfo *&bindingOut) -> bool {
    bindingOut = nullptr;
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      bindingOut = paramBinding;
      return true;
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      bindingOut = &it->second;
      return true;
    }
    return false;
  };
  auto validateVectorElementType = [&](const Expr &arg, const std::string &typeName) -> bool {
    if (typeName.empty()) {
      error_ = "push requires vector element type";
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    if (normalizedType == "string") {
      if (!isStringExpr(arg, params, locals)) {
        error_ = "push requires element type " + typeName;
        return false;
      }
      return true;
    }
    ReturnKind expectedKind = returnKindForTypeName(normalizedType);
    if (expectedKind == ReturnKind::Unknown) {
      return true;
    }
    if (isStringExpr(arg, params, locals)) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    ReturnKind argKind = inferExprReturnKind(arg, params, locals);
    if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    return true;
  };
  auto validateVectorHelperTarget = [&](const Expr &target, const char *helperName, const BindingInfo *&bindingOut) -> bool {
    if (!resolveVectorBinding(target, bindingOut) || bindingOut == nullptr || bindingOut->typeName != "vector") {
      error_ = std::string(helperName) + " requires vector binding";
      return false;
    }
    if (!bindingOut->isMutable) {
      error_ = std::string(helperName) + " requires mutable vector binding";
      return false;
    }
    return true;
  };
  auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    if (isSimpleCallName(candidate, "push")) {
      nameOut = "push";
      return true;
    }
    if (isSimpleCallName(candidate, "pop")) {
      nameOut = "pop";
      return true;
    }
    if (isSimpleCallName(candidate, "reserve")) {
      nameOut = "reserve";
      return true;
    }
    if (isSimpleCallName(candidate, "clear")) {
      nameOut = "clear";
      return true;
    }
    if (isSimpleCallName(candidate, "remove_at")) {
      nameOut = "remove_at";
      return true;
    }
    if (isSimpleCallName(candidate, "remove_swap")) {
      nameOut = "remove_swap";
      return true;
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
    if (!parseBindingInfo(stmt, namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
      return false;
    }
    if (stmt.args.size() != 1) {
      error_ = "binding requires exactly one argument";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args.front())) {
      return false;
    }
    auto isStructConstructor = [&](const Expr &expr) -> bool {
      if (expr.kind != Expr::Kind::Call || expr.isBinding) {
        return false;
      }
      const std::string resolved = resolveCalleePath(expr);
      return structNames_.count(resolved) > 0;
    };
    auto getEnvelopeValueExpr = [&](const Expr &candidate) -> const Expr * {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return nullptr;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
        return nullptr;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return nullptr;
      }
      const std::string resolved = resolveCalleePath(candidate);
      if (defMap_.find(resolved) != defMap_.end()) {
        return nullptr;
      }
      const Expr *valueExpr = nullptr;
      for (const auto &bodyExpr : candidate.bodyArguments) {
        if (bodyExpr.isBinding) {
          continue;
        }
        valueExpr = &bodyExpr;
      }
      return valueExpr;
    };
    std::function<bool(const Expr &)> isStructConstructorValueExpr;
    isStructConstructorValueExpr = [&](const Expr &candidate) -> bool {
      if (isStructConstructor(candidate)) {
        return true;
      }
      if (isIfCall(candidate) && candidate.args.size() == 3) {
        const Expr &thenArg = candidate.args[1];
        const Expr &elseArg = candidate.args[2];
        const Expr *thenValue = getEnvelopeValueExpr(thenArg);
        const Expr *elseValue = getEnvelopeValueExpr(elseArg);
        const Expr &thenExpr = thenValue ? *thenValue : thenArg;
        const Expr &elseExpr = elseValue ? *elseValue : elseArg;
        return isStructConstructorValueExpr(thenExpr) && isStructConstructorValueExpr(elseExpr);
      }
      if (const Expr *valueExpr = getEnvelopeValueExpr(candidate)) {
        return isStructConstructorValueExpr(*valueExpr);
      }
      return false;
    };
    ReturnKind initKind = inferExprReturnKind(stmt.args.front(), params, locals);
    if (initKind == ReturnKind::Void && !isStructConstructorValueExpr(stmt.args.front())) {
      error_ = "binding initializer requires a value";
      return false;
    }
    if (!hasExplicitBindingTypeTransform(stmt)) {
      (void)inferBindingTypeFromInitializer(stmt.args.front(), params, locals, info);
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
  std::optional<EffectScope> effectScope;
  if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && !stmt.transforms.empty()) {
    std::unordered_set<std::string> executionEffects;
    if (!resolveExecutionEffects(stmt, executionEffects)) {
      return false;
    }
    effectScope.emplace(*this, std::move(executionEffects));
  }
  if (stmt.kind != Expr::Kind::Call) {
    if (!allowBindings) {
      error_ = "execution body arguments must be calls";
      return false;
    }
    return validateExpr(params, locals, stmt);
  }
  if (isReturnCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
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
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
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
      return true;
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
  auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
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
  };
  auto validateLoopBody = [&](const Expr &body, const std::unordered_map<std::string, BindingInfo> &baseLocals) -> bool {
    if (!isLoopBlockEnvelope(body)) {
      error_ = "loop body requires a block envelope";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> blockLocals = baseLocals;
    for (const auto &bodyExpr : body.bodyArguments) {
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
  };
  auto isNegativeIntegerLiteral = [&](const Expr &expr) -> bool {
    if (expr.kind != Expr::Kind::Literal || expr.isUnsigned) {
      return false;
    }
    if (expr.intWidth == 32) {
      return static_cast<int32_t>(expr.literalValue) < 0;
    }
    return static_cast<int64_t>(expr.literalValue) < 0;
  };
  if (isLoopCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "loop does not accept trailing block arguments";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "loop does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 2) {
      error_ = "loop requires count and body";
      return false;
    }
    const Expr &count = stmt.args[0];
    if (!validateExpr(params, locals, count)) {
      return false;
    }
    ReturnKind countKind = inferExprReturnKind(count, params, locals);
    if (countKind != ReturnKind::Int && countKind != ReturnKind::Int64 && countKind != ReturnKind::UInt64) {
      error_ = "loop count requires integer";
      return false;
    }
    if (isNegativeIntegerLiteral(count)) {
      error_ = "loop count must be non-negative";
      return false;
    }
    return validateLoopBody(stmt.args[1], locals);
  }
  if (isWhileCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "while does not accept trailing block arguments";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "while does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 2) {
      error_ = "while requires condition and body";
      return false;
    }
    const Expr &cond = stmt.args[0];
    if (!validateExpr(params, locals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, locals);
    if (condKind != ReturnKind::Bool) {
      error_ = "while condition requires bool";
      return false;
    }
    return validateLoopBody(stmt.args[1], locals);
  }
  if (isForCall(stmt)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = "for does not accept trailing block arguments";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = "for does not accept template arguments";
      return false;
    }
    if (stmt.args.size() != 4) {
      error_ = "for requires init, condition, step, and body";
      return false;
    }
    std::unordered_map<std::string, BindingInfo> loopLocals = locals;
    if (!validateStatement(params, loopLocals, stmt.args[0], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    const Expr &cond = stmt.args[1];
    if (!validateExpr(params, loopLocals, cond)) {
      return false;
    }
    ReturnKind condKind = inferExprReturnKind(cond, params, loopLocals);
    if (condKind != ReturnKind::Bool) {
      error_ = "for condition requires bool";
      return false;
    }
    if (!validateStatement(params, loopLocals, stmt.args[2], returnKind, false, allowBindings, nullptr, namespacePrefix)) {
      return false;
    }
    return validateLoopBody(stmt.args[3], loopLocals);
  }
  if (isRepeatCall(stmt)) {
    if (!stmt.hasBodyArguments) {
      error_ = "repeat requires block arguments";
      return false;
    }
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
  if (isBlockCall(stmt) && !stmt.hasBodyArguments) {
    error_ = "block requires block arguments";
    return false;
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
      error_ = printBuiltin.name + " requires an integer/bool or string literal/binding argument";
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
        if (binding.typeName != "array" && binding.typeName != "vector") {
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
          if ((collection == "array" || collection == "vector") && target.templateArgs.size() == 1) {
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
  std::string vectorHelper;
  if (getVectorStatementHelperName(stmt, vectorHelper)) {
    if (hasNamedArguments(stmt.argNames)) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!stmt.templateArgs.empty()) {
      error_ = vectorHelper + " does not accept template arguments";
      return false;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error_ = vectorHelper + " does not accept block arguments";
      return false;
    }
    if (vectorHelper == "push") {
      if (stmt.args.size() != 2) {
        error_ = "push requires exactly two arguments";
        return false;
      }
      if (activeEffects_.count("heap_alloc") == 0) {
        error_ = "push requires heap_alloc effect";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), "push", binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front()) ||
          !validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      if (!validateVectorElementType(stmt.args[1], binding->typeTemplateArg)) {
        return false;
      }
      return true;
    }
    if (vectorHelper == "reserve") {
      if (stmt.args.size() != 2) {
        error_ = "reserve requires exactly two arguments";
        return false;
      }
      if (activeEffects_.count("heap_alloc") == 0) {
        error_ = "reserve requires heap_alloc effect";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), "reserve", binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front()) ||
          !validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      if (!isIntegerOrBoolExpr(stmt.args[1])) {
        error_ = "reserve requires integer capacity";
        return false;
      }
      return true;
    }
    if (vectorHelper == "remove_at" || vectorHelper == "remove_swap") {
      if (stmt.args.size() != 2) {
        error_ = vectorHelper + " requires exactly two arguments";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), vectorHelper.c_str(), binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front()) ||
          !validateExpr(params, locals, stmt.args[1])) {
        return false;
      }
      if (!isIntegerOrBoolExpr(stmt.args[1])) {
        error_ = vectorHelper + " requires integer index";
        return false;
      }
      return true;
    }
    if (vectorHelper == "pop" || vectorHelper == "clear") {
      if (stmt.args.size() != 1) {
        error_ = vectorHelper + " requires exactly one argument";
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!validateVectorHelperTarget(stmt.args.front(), vectorHelper.c_str(), binding)) {
        return false;
      }
      if (!validateExpr(params, locals, stmt.args.front())) {
        return false;
      }
      return true;
    }
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
      if (!validateStatement(params, blockLocals, bodyExpr, returnKind, false, true, nullptr, namespacePrefix)) {
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
    return true;
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
