  auto isParam = [](const std::vector<Expr> &params, const std::string &name) -> bool {
    for (const auto &param : params) {
      if (param.name == name) {
        return true;
      }
    }
    return false;
  };

  std::unordered_set<std::string> inferenceStack;
  std::function<ReturnKind(const Definition &)> inferDefinitionReturnKind;
  std::function<ReturnKind(const Expr &,
                           const std::vector<Expr> &,
                           const std::unordered_map<std::string, ReturnKind> &)>
      inferExprReturnKind;
  std::function<std::string(const Expr &,
                            const std::vector<Expr> &,
                            const std::unordered_map<std::string, std::string> &)>
      inferStructReturnPath;
#include "EmitterEmitSetupReturnInferenceCollections.h"

  inferExprReturnKind = [&](const Expr &expr,
                            const std::vector<Expr> &params,
                            const std::unordered_map<std::string, ReturnKind> &locals) -> ReturnKind {
    auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
      if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
          right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
        return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64
                                                                           : ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
        if (left == ReturnKind::Int64 || left == ReturnKind::Int) {
          if (right == ReturnKind::Int64 || right == ReturnKind::Int) {
            return ReturnKind::Int64;
          }
        }
        return ReturnKind::Unknown;
      }
      if (left == ReturnKind::Int && right == ReturnKind::Int) {
        return ReturnKind::Int;
      }
      return ReturnKind::Unknown;
    };

    if (expr.kind == Expr::Kind::Literal) {
      if (expr.isUnsigned) {
        return ReturnKind::UInt64;
      }
      if (expr.intWidth == 64) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Int;
    }
    if (expr.kind == Expr::Kind::BoolLiteral) {
      return ReturnKind::Bool;
    }
    if (expr.kind == Expr::Kind::FloatLiteral) {
      return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
    }
    if (expr.kind == Expr::Kind::StringLiteral) {
      return ReturnKind::String;
    }
    if (expr.kind == Expr::Kind::Name) {
      if (isParam(params, expr.name)) {
        return ReturnKind::Unknown;
      }
      if (isBuiltinMathConstantName(resolveExprPath(expr), hasMathImport)) {
        return ReturnKind::Float64;
      }
      auto it = locals.find(expr.name);
      if (it == locals.end()) {
        return ReturnKind::Unknown;
      }
      return it->second;
    }
    if (expr.kind == Expr::Kind::Call) {
      const std::string resolvedPath = resolveExprPath(expr);
      const auto resolvedCandidates = collectionHelperPathCandidates(resolvedPath);
      bool sawDefinitionCandidate = false;
      ReturnKind firstValueKind = ReturnKind::Unknown;
      for (const auto &candidate : resolvedCandidates) {
        auto defIt = defMap.find(candidate);
        if (defIt == defMap.end()) {
          continue;
        }
        sawDefinitionCandidate = true;
        ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
        if (calleeKind == ReturnKind::Void || calleeKind == ReturnKind::Unknown) {
          continue;
        }
        if (firstValueKind == ReturnKind::Unknown) {
          firstValueKind = calleeKind;
        }
        auto structIt = returnStructs.find(candidate);
        const bool candidateReturnsStruct =
            calleeKind == ReturnKind::Array && structIt != returnStructs.end() && !structIt->second.empty();
        if (candidateReturnsStruct) {
          return calleeKind;
        }
      }
      if (firstValueKind != ReturnKind::Unknown) {
        return firstValueKind;
      }
      if (resolvedCandidates.empty()) {
        std::string preferred = preferCollectionHelperPath(resolvedPath);
        auto defIt = defMap.find(preferred);
        if (defIt != defMap.end()) {
          sawDefinitionCandidate = true;
          ReturnKind calleeKind = inferDefinitionReturnKind(*defIt->second);
          if (calleeKind != ReturnKind::Void && calleeKind != ReturnKind::Unknown) {
            return calleeKind;
          }
        }
      }
      if (sawDefinitionCandidate) {
        return ReturnKind::Unknown;
      }
      if (isBuiltinBlock(expr, nameMap) && expr.hasBodyArguments) {
        if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
          return ReturnKind::Unknown;
        }
        std::unordered_map<std::string, ReturnKind> blockLocals = locals;
        bool sawValue = false;
        ReturnKind last = ReturnKind::Unknown;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo info = getBindingInfo(bodyExpr);
            ReturnKind bindingKind = returnKindForTypeName(info.typeName);
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, blockLocals);
              if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
                bindingKind = initKind;
              }
            }
            blockLocals.emplace(bodyExpr.name, bindingKind);
            continue;
          }
          sawValue = true;
          last = inferExprReturnKind(bodyExpr, params, blockLocals);
        }
        return sawValue ? last : ReturnKind::Unknown;
      }
      if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
        auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
          if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
            return false;
          }
          if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
            return false;
          }
          if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
            return false;
          }
          return true;
        };
        auto inferBranchValueKind = [&](const Expr &candidate,
                                        const std::unordered_map<std::string, ReturnKind> &localsBase)
            -> ReturnKind {
          if (!isIfBlockEnvelope(candidate)) {
            return inferExprReturnKind(candidate, params, localsBase);
          }
          std::unordered_map<std::string, ReturnKind> branchLocals = localsBase;
          bool sawValue = false;
          ReturnKind lastKind = ReturnKind::Unknown;
          for (const auto &bodyExpr : candidate.bodyArguments) {
            if (bodyExpr.isBinding) {
              BindingInfo info = getBindingInfo(bodyExpr);
              ReturnKind bindingKind = returnKindForTypeName(info.typeName);
              if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
                ReturnKind initKind = inferExprReturnKind(bodyExpr.args.front(), params, branchLocals);
                if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
                  bindingKind = initKind;
                }
              }
              branchLocals.emplace(bodyExpr.name, bindingKind);
              continue;
            }
            sawValue = true;
            lastKind = inferExprReturnKind(bodyExpr, params, branchLocals);
          }
          return sawValue ? lastKind : ReturnKind::Unknown;
        };

        ReturnKind thenKind = inferBranchValueKind(expr.args[1], locals);
        ReturnKind elseKind = inferBranchValueKind(expr.args[2], locals);
        if (thenKind == ReturnKind::Unknown || elseKind == ReturnKind::Unknown) {
          return ReturnKind::Unknown;
        }
        if (thenKind == elseKind) {
          return thenKind;
        }
        return combineNumeric(thenKind, elseKind);
      }
      const char *cmp = nullptr;
      if (getBuiltinComparison(expr, cmp)) {
        return ReturnKind::Bool;
      }
      char op = '\0';
      if (getBuiltinOperator(expr, op)) {
        if (expr.args.size() != 2) {
          return ReturnKind::Unknown;
        }
        ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
        ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
        return combineNumeric(left, right);
      }
      if (isBuiltinNegate(expr)) {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (isBuiltinClamp(expr, hasMathImport)) {
        if (expr.args.size() != 3) {
          return ReturnKind::Unknown;
        }
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      std::string mathName;
      if (getBuiltinMathName(expr, mathName, hasMathImport)) {
        if (mathName == "is_nan" || mathName == "is_inf" || mathName == "is_finite") {
          return ReturnKind::Bool;
        }
        if (mathName == "lerp" && expr.args.size() == 3) {
          ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
          result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
          result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
          return result;
        }
        if (mathName == "pow" && expr.args.size() == 2) {
          ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
          result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
          return result;
        }
        if (expr.args.empty()) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Float64) {
          return ReturnKind::Float64;
        }
        if (argKind == ReturnKind::Float32) {
          return ReturnKind::Float32;
        }
        return ReturnKind::Unknown;
      }
      std::string convertName;
      if (getBuiltinConvertName(expr, convertName) && expr.templateArgs.size() == 1) {
        return returnKindForTypeName(expr.templateArgs.front());
      }
      return ReturnKind::Unknown;
    }
    return ReturnKind::Unknown;
  };

  auto resolveParamStructPath = [&](const Expr &param, const std::string &namespacePrefix) -> std::string {
    BindingInfo info = getBindingInfo(param);
    if (info.typeName == "Reference" || info.typeName == "Pointer") {
      return "";
    }
    const std::string typeNamespace = param.namespacePrefix.empty() ? namespacePrefix : param.namespacePrefix;
    return resolveStructReturnPath(info.typeName, typeNamespace);
  };

  inferStructReturnPath = [&](const Expr &expr,
                              const std::vector<Expr> &params,
                              const std::unordered_map<std::string, std::string> &locals) -> std::string {
    auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return allowAnyName || isBuiltinBlock(candidate, nameMap);
    };
    auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
      if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
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

    if (expr.isLambda) {
      return "";
    }

    if (expr.kind == Expr::Kind::Name) {
      for (const auto &param : params) {
        if (param.name == expr.name) {
          return resolveParamStructPath(param, expr.namespacePrefix);
        }
      }
      auto it = locals.find(expr.name);
      return it != locals.end() ? it->second : "";
    }

    if (expr.kind == Expr::Kind::Call) {
      if (expr.isMethodCall) {
        if (expr.args.empty()) {
          return "";
        }
        std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
        if (receiverStruct.empty()) {
          return "";
        }
        std::string rawMethodName = expr.name;
        if (!rawMethodName.empty() && rawMethodName.front() == '/') {
          rawMethodName.erase(rawMethodName.begin());
        }
        const std::string methodName = normalizeCollectionMethodName(receiverStruct, expr.name);
        auto candidates = collectionMethodPathCandidates(receiverStruct, methodName, rawMethodName);
        if ((receiverStruct == "/vector" || receiverStruct == "/array" || receiverStruct == "/string") &&
            (methodName == "at" || methodName == "at_unsafe")) {
          const std::string canonicalCandidate = "/std/collections/vector/" + methodName;
          for (auto it = candidates.begin(); it != candidates.end();) {
            if (*it == canonicalCandidate) {
              it = candidates.erase(it);
            } else {
              ++it;
            }
          }
        }
        for (const auto &candidate : candidates) {
          auto it = returnStructs.find(candidate);
          if (it != returnStructs.end()) {
            return it->second;
          }
        }
        for (const auto &candidate : candidates) {
          auto defIt = defMap.find(candidate);
          if (defIt == defMap.end()) {
            continue;
          }
          (void)inferDefinitionReturnKind(*defIt->second);
          auto it = returnStructs.find(candidate);
          if (it != returnStructs.end()) {
            return it->second;
          }
        }
        return "";
      }

      if (isBuiltinIf(expr, nameMap) && expr.args.size() == 3) {
        const Expr &thenArg = expr.args[1];
        const Expr &elseArg = expr.args[2];
        const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
        const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
        const Expr &thenExpr = thenValue ? *thenValue : thenArg;
        const Expr &elseExpr = elseValue ? *elseValue : elseArg;
        std::string thenPath = inferStructReturnPath(thenExpr, params, locals);
        if (thenPath.empty()) {
          return "";
        }
        std::string elsePath = inferStructReturnPath(elseExpr, params, locals);
        return thenPath == elsePath ? thenPath : "";
      }

      if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
        if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
          return inferStructReturnPath(valueExpr->args.front(), params, locals);
        }
        return inferStructReturnPath(*valueExpr, params, locals);
      }

      std::string collectionName;
      if (getBuiltinCollectionName(expr, collectionName)) {
        if ((collectionName == "array" || collectionName == "vector") && expr.templateArgs.size() == 1) {
          return "/" + collectionName;
        }
        if (collectionName == "map" && expr.templateArgs.size() == 2) {
          return "/map";
        }
      }

      const std::string resolvedExprPath = resolveExprPath(expr);
      auto resolvedCandidates = collectionHelperPathCandidates(resolvedExprPath);
      pruneMapAccessStructReturnCompatibilityCandidates(resolvedExprPath, resolvedCandidates);
      pruneBuiltinVectorAccessStructReturnCandidates(expr, resolvedExprPath, params, locals, resolvedCandidates);
      for (const auto &candidate : resolvedCandidates) {
        auto it = returnStructs.find(candidate);
        if (it != returnStructs.end()) {
          return it->second;
        }
      }
      std::string resolved =
          resolvedCandidates.empty() ? preferCollectionHelperPath(resolveExprPath(expr)) : resolvedCandidates.front();
      if (structTypeMap.count(resolved) == 0) {
        auto importIt = importAliases.find(expr.name);
        if (importIt != importAliases.end() && structTypeMap.count(importIt->second) > 0) {
          return importIt->second;
        }
      }
      if (structTypeMap.count(resolved) > 0) {
        return resolved;
      }
      bool hasDefinitionCandidate = false;
      for (const auto &candidate : resolvedCandidates) {
        auto defIt = defMap.find(candidate);
        if (defIt == defMap.end()) {
          continue;
        }
        hasDefinitionCandidate = true;
        (void)inferDefinitionReturnKind(*defIt->second);
        auto it = returnStructs.find(candidate);
        if (it != returnStructs.end()) {
          return it->second;
        }
      }
      if (!hasDefinitionCandidate && resolvedCandidates.empty()) {
        auto defIt = defMap.find(resolved);
        if (defIt != defMap.end()) {
          (void)inferDefinitionReturnKind(*defIt->second);
          auto it = returnStructs.find(resolved);
          return it != returnStructs.end() ? it->second : "";
        }
      }
    }

    return "";
  };

  inferDefinitionReturnKind = [&](const Definition &def) -> ReturnKind {
    ReturnKind &kind = returnKinds[def.fullPath];
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (!inferenceStack.insert(def.fullPath).second) {
      return ReturnKind::Unknown;
    }
    bool hasExplicitReturnTransform = false;
    bool hasAutoReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasExplicitReturnTransform = true;
        hasAutoReturnTransform = transform.templateArgs.size() == 1 && transform.templateArgs.front() == "auto";
        break;
      }
    }
    const auto &params = paramMap[def.fullPath];
    ReturnKind inferred = ReturnKind::Unknown;
    std::string inferredStructPath;
    bool inferredStructConflict = false;
    bool sawNonCollectionReturn = false;
    bool sawReturn = false;
    std::unordered_map<std::string, ReturnKind> locals;
    std::unordered_map<std::string, std::string> localStructs;
    std::function<void(const Expr &,
                       std::unordered_map<std::string, ReturnKind> &,
                       std::unordered_map<std::string, std::string> &)>
        visitStmt;
    visitStmt = [&](const Expr &stmt,
                    std::unordered_map<std::string, ReturnKind> &activeLocals,
                    std::unordered_map<std::string, std::string> &activeStructs) {
      if (stmt.isBinding) {
        BindingInfo info = getBindingInfo(stmt);
        ReturnKind bindingKind = returnKindForTypeName(info.typeName);
        if (!hasExplicitBindingTypeTransform(stmt) && stmt.args.size() == 1) {
          ReturnKind initKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
          if (initKind != ReturnKind::Unknown && initKind != ReturnKind::Void) {
            bindingKind = initKind;
          }
        }
        activeLocals.emplace(stmt.name, bindingKind);
        std::string structPath = resolveStructReturnPath(info.typeName, def.namespacePrefix);
        if (structPath.empty() && stmt.args.size() == 1) {
          structPath = inferStructReturnPath(stmt.args.front(), params, activeStructs);
        }
        if (!structPath.empty()) {
          activeStructs.emplace(stmt.name, structPath);
        }
        return;
      }
      if (isReturnCall(stmt)) {
        sawReturn = true;
        ReturnKind exprKind = ReturnKind::Void;
        std::string exprStructPath;
        if (!stmt.args.empty()) {
          exprKind = inferExprReturnKind(stmt.args.front(), params, activeLocals);
          exprStructPath = inferStructReturnPath(stmt.args.front(), params, activeStructs);
        }
        if (stmt.args.empty() || exprStructPath.empty()) {
          sawNonCollectionReturn = true;
        }
        if (exprKind != ReturnKind::Unknown && exprKind != ReturnKind::Array) {
          sawNonCollectionReturn = true;
        }
        if (!exprStructPath.empty()) {
          if (inferredStructPath.empty()) {
            inferredStructPath = exprStructPath;
          } else if (inferredStructPath != exprStructPath) {
            inferredStructConflict = true;
          }
        } else if (!inferredStructPath.empty() && !stmt.args.empty()) {
          inferredStructConflict = true;
        }
        if (exprKind == ReturnKind::Unknown) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Unknown) {
          inferred = exprKind;
          if (!exprStructPath.empty()) {
            inferredStructPath = exprStructPath;
          }
          return;
        }
        if (inferred != exprKind) {
          inferred = ReturnKind::Unknown;
          return;
        }
        if (inferred == ReturnKind::Array) {
          if (!exprStructPath.empty()) {
            if (inferredStructPath.empty()) {
              inferredStructPath = exprStructPath;
            } else if (inferredStructPath != exprStructPath) {
              inferred = ReturnKind::Unknown;
              inferredStructConflict = true;
            }
          } else if (!inferredStructPath.empty()) {
            inferred = ReturnKind::Unknown;
            inferredStructConflict = true;
          }
        }
        return;
      }
      if (isBuiltinIf(stmt, nameMap) && stmt.args.size() == 3) {
        const Expr &thenBlock = stmt.args[1];
        const Expr &elseBlock = stmt.args[2];
        auto walkBlock = [&](const Expr &block) {
          std::unordered_map<std::string, ReturnKind> blockLocals = activeLocals;
          std::unordered_map<std::string, std::string> blockStructs = activeStructs;
          for (const auto &bodyStmt : block.bodyArguments) {
            visitStmt(bodyStmt, blockLocals, blockStructs);
          }
        };
        walkBlock(thenBlock);
        walkBlock(elseBlock);
        return;
      }
    };
    for (const auto &stmt : def.statements) {
      visitStmt(stmt, locals, localStructs);
    }
    if (!sawReturn) {
      kind = ReturnKind::Void;
    } else if (inferred == ReturnKind::Unknown) {
      if (hasExplicitReturnTransform) {
        kind = ReturnKind::Unknown;
      } else {
        kind = ReturnKind::Int;
      }
    } else {
      kind = inferred;
    }
    const bool keepInferredStructPath =
        (!hasExplicitReturnTransform || hasAutoReturnTransform || kind == ReturnKind::Array);
    if (keepInferredStructPath && !inferredStructPath.empty() && !inferredStructConflict &&
        !sawNonCollectionReturn) {
      returnStructs[def.fullPath] = inferredStructPath;
    }
    inferenceStack.erase(def.fullPath);
    return kind;
  };

  for (const auto &def : program.definitions) {
    if (returnKinds[def.fullPath] == ReturnKind::Unknown) {
      inferDefinitionReturnKind(def);
    }
  }

  for (const auto &def : program.definitions) {
    if (returnStructs.count(def.fullPath) > 0) {
      continue;
    }
    for (const auto &transform : def.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string structPath = resolveStructReturnPath(transform.templateArgs.front(), def.namespacePrefix);
      if (!structPath.empty()) {
        returnStructs[def.fullPath] = structPath;
      }
      break;
    }
  }
