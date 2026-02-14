#include "SemanticsValidator.h"

#include <functional>

namespace primec::semantics {

bool SemanticsValidator::inferUnknownReturnKinds() {
  for (const auto &def : program_.definitions) {
    if (returnKinds_[def.fullPath] == ReturnKind::Unknown) {
      if (!inferDefinitionReturnKind(def)) {
        return false;
      }
    }
  }
  return true;
}

ReturnKind SemanticsValidator::inferExprReturnKind(const Expr &expr,
                                                   const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals) {
  auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
    if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::Void || right == ReturnKind::Void) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
      return ReturnKind::Float64;
    }
    if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
      return ReturnKind::Float32;
    }
    if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
      return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
      if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
          (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
        return ReturnKind::Int64;
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
    return ReturnKind::Unknown;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return ReturnKind::Float64;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
        ReturnKind refKind = returnKindForTypeName(paramBinding->typeTemplateArg);
        if (refKind != ReturnKind::Unknown) {
          return refKind;
        }
      }
      return returnKindForTypeName(paramBinding->typeName);
    }
    auto it = locals.find(expr.name);
    if (it == locals.end()) {
      return ReturnKind::Unknown;
    }
    if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
      ReturnKind refKind = returnKindForTypeName(it->second.typeTemplateArg);
      if (refKind != ReturnKind::Unknown) {
        return refKind;
      }
    }
    return returnKindForTypeName(it->second.typeName);
  }
  if (expr.kind == Expr::Kind::Call) {
    if (isIfCall(expr) && expr.args.size() == 3) {
      auto isIfBranchEnvelopeName = [&](const Expr &candidate) -> bool {
        return candidate.name == "then" || candidate.name == "else" || candidate.name == "/then" ||
               candidate.name == "/else";
      };
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
        if (isIfBranchEnvelopeName(candidate)) {
          return true;
        }
        const std::string resolved = resolveCalleePath(candidate);
        return defMap_.find(resolved) == defMap_.end();
      };
      auto inferBlockEnvelopeValue = [&](const Expr &candidate,
                                         const std::unordered_map<std::string, BindingInfo> &localsIn,
                                         const Expr *&valueExprOut,
                                         std::unordered_map<std::string, BindingInfo> &localsOut) -> bool {
        valueExprOut = nullptr;
        localsOut = localsIn;
        if (!isIfBlockEnvelope(candidate)) {
          valueExprOut = &candidate;
          return true;
        }
        for (const auto &bodyExpr : candidate.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo binding;
            std::optional<std::string> restrictType;
            if (!parseBindingInfo(bodyExpr, candidate.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
              return false;
            }
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, localsOut, binding);
            }
            localsOut.emplace(bodyExpr.name, std::move(binding));
            continue;
          }
          valueExprOut = &bodyExpr;
        }
        return valueExprOut != nullptr;
      };

      const Expr *thenValueExpr = nullptr;
      const Expr *elseValueExpr = nullptr;
      std::unordered_map<std::string, BindingInfo> thenLocals;
      std::unordered_map<std::string, BindingInfo> elseLocals;
      if (!inferBlockEnvelopeValue(expr.args[1], locals, thenValueExpr, thenLocals) ||
          !inferBlockEnvelopeValue(expr.args[2], locals, elseValueExpr, elseLocals)) {
        return ReturnKind::Unknown;
      }

      ReturnKind thenKind = inferExprReturnKind(*thenValueExpr, params, thenLocals);
      ReturnKind elseKind = inferExprReturnKind(*elseValueExpr, params, elseLocals);
      if (thenKind == elseKind) {
        return thenKind;
      }
      return combineNumeric(thenKind, elseKind);
    }
    if (isBlockCall(expr) && expr.hasBodyArguments) {
      const std::string resolved = resolveCalleePath(expr);
      if (defMap_.find(resolved) == defMap_.end() && expr.args.empty() && expr.templateArgs.empty() &&
          !hasNamedArguments(expr.argNames)) {
        if (expr.bodyArguments.empty()) {
          return ReturnKind::Unknown;
        }
        std::unordered_map<std::string, BindingInfo> blockLocals = locals;
        ReturnKind result = ReturnKind::Unknown;
        for (const auto &bodyExpr : expr.bodyArguments) {
          if (bodyExpr.isBinding) {
            BindingInfo info;
            std::optional<std::string> restrictType;
            if (!parseBindingInfo(bodyExpr, bodyExpr.namespacePrefix, structNames_, importAliases_, info, restrictType, error_)) {
              return ReturnKind::Unknown;
            }
            if (!hasExplicitBindingTypeTransform(bodyExpr) && bodyExpr.args.size() == 1) {
              (void)inferBindingTypeFromInitializer(bodyExpr.args.front(), params, blockLocals, info);
            }
            if (restrictType.has_value()) {
              const bool hasTemplate = !info.typeTemplateArg.empty();
              if (!restrictMatchesBinding(*restrictType,
                                          info.typeName,
                                          info.typeTemplateArg,
                                          hasTemplate,
                                          bodyExpr.namespacePrefix)) {
                return ReturnKind::Unknown;
              }
            }
            blockLocals.emplace(bodyExpr.name, std::move(info));
            continue;
          }
          result = inferExprReturnKind(bodyExpr, params, blockLocals);
        }
        return result;
      }
    }
    auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if ((paramBinding->typeName != "array" && paramBinding->typeName != "vector") ||
              paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if ((it->second.typeName != "array" && it->second.typeName != "vector") ||
              it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
            target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "vector" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName != "vector" || it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(target, collection) && collection == "vector" && target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
      }
      return false;
    };
    auto resolveStringTarget = [&](const Expr &target) -> bool {
      if (target.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(target.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      if (target.kind == Expr::Kind::Call) {
        std::string builtinName;
        if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
          std::string elemType;
          if (resolveArrayTarget(target.args.front(), elemType) && elemType == "string") {
            return true;
          }
        }
      }
      return false;
    };
    auto resolveMapTarget = [&](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
      keyTypeOut.clear();
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "map") {
            return false;
          }
          std::vector<std::string> parts;
          if (!splitTopLevelTemplateArgs(paramBinding->typeTemplateArg, parts) || parts.size() != 2) {
            return false;
          }
          keyTypeOut = parts[0];
          valueTypeOut = parts[1];
          return true;
        }
        auto it = locals.find(target.name);
        if (it == locals.end() || it->second.typeName != "map") {
          return false;
        }
        std::vector<std::string> parts;
        if (!splitTopLevelTemplateArgs(it->second.typeTemplateArg, parts) || parts.size() != 2) {
          return false;
        }
        keyTypeOut = parts[0];
        valueTypeOut = parts[1];
        return true;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string collection;
        if (!getBuiltinCollectionName(target, collection) || collection != "map" || target.templateArgs.size() != 2) {
          return false;
        }
        keyTypeOut = target.templateArgs[0];
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      return false;
    };
    std::function<ReturnKind(const Expr &)> pointerTargetKind = [&](const Expr &pointerExpr) -> ReturnKind {
      if (pointerExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
          if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
              !paramBinding->typeTemplateArg.empty()) {
            return returnKindForTypeName(paramBinding->typeTemplateArg);
          }
          return ReturnKind::Unknown;
        }
        auto it = locals.find(pointerExpr.name);
        if (it != locals.end()) {
          if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
              !it->second.typeTemplateArg.empty()) {
            return returnKindForTypeName(it->second.typeTemplateArg);
          }
        }
        return ReturnKind::Unknown;
      }
      if (pointerExpr.kind == Expr::Kind::Call) {
        std::string pointerName;
        if (getBuiltinPointerName(pointerExpr, pointerName) && pointerName == "location" && pointerExpr.args.size() == 1) {
          const Expr &target = pointerExpr.args.front();
          if (target.kind != Expr::Kind::Name) {
            return ReturnKind::Unknown;
          }
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
              return returnKindForTypeName(paramBinding->typeTemplateArg);
            }
            return returnKindForTypeName(paramBinding->typeName);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
              return returnKindForTypeName(it->second.typeTemplateArg);
            }
            return returnKindForTypeName(it->second.typeName);
          }
        }
        std::string opName;
        if (getBuiltinOperatorName(pointerExpr, opName) && (opName == "plus" || opName == "minus") &&
            pointerExpr.args.size() == 2) {
          ReturnKind leftKind = pointerTargetKind(pointerExpr.args[0]);
          if (leftKind != ReturnKind::Unknown) {
            return leftKind;
          }
          return pointerTargetKind(pointerExpr.args[1]);
        }
      }
      return ReturnKind::Unknown;
    };
    auto resolveMethodCallPath = [&](std::string &resolvedOut) -> bool {
      if (expr.args.empty()) {
        return false;
      }
      const Expr &receiver = expr.args.front();
      std::string typeName;
      std::string typeTemplateArg;
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
          typeTemplateArg = paramBinding->typeTemplateArg;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
            typeTemplateArg = it->second.typeTemplateArg;
          }
        }
      }
      if (typeName.empty()) {
        std::string inferred = typeNameForReturnKind(inferExprReturnKind(receiver, params, locals));
        if (!inferred.empty()) {
          typeName = inferred;
        }
      }
      if (typeName.empty()) {
        return false;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + expr.name;
        return true;
      }
      std::string resolvedType = resolveTypePath(typeName, expr.namespacePrefix);
      if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
        auto importIt = importAliases_.find(typeName);
        if (importIt != importAliases_.end()) {
          resolvedType = importIt->second;
        }
      }
      resolvedOut = resolvedType + "/" + expr.name;
      return true;
    };
    std::string resolved = resolveCalleePath(expr);
    bool hasResolvedPath = !expr.isMethodCall;
    if (expr.isMethodCall && expr.name == "count" && expr.args.size() == 1) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (resolveVectorTarget(expr.args.front(), elemType) || resolveArrayTarget(expr.args.front(), elemType) ||
          resolveStringTarget(expr.args.front()) ||
          resolveMapTarget(expr.args.front(), keyType, valueType)) {
        return ReturnKind::Int;
      }
    }
    if (expr.isMethodCall && expr.name == "capacity" && expr.args.size() == 1) {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        return ReturnKind::Int;
      }
    }
    if (expr.isMethodCall) {
      std::string methodResolved;
      if (resolveMethodCallPath(methodResolved)) {
        resolved = methodResolved;
        hasResolvedPath = true;
      }
    }
    auto defIt = hasResolvedPath ? defMap_.find(resolved) : defMap_.end();
    if (defIt != defMap_.end()) {
      if (!inferDefinitionReturnKind(*defIt->second)) {
        return ReturnKind::Unknown;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end()) {
        return kindIt->second;
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && expr.name == "count" && expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (!resolveVectorTarget(expr.args.front(), elemType) && !resolveArrayTarget(expr.args.front(), elemType) &&
          !resolveStringTarget(expr.args.front()) &&
          !resolveMapTarget(expr.args.front(), keyType, valueType)) {
        std::string methodResolved;
        if (resolveMethodCallPath(methodResolved)) {
          auto methodIt = defMap_.find(methodResolved);
          if (methodIt != defMap_.end()) {
            if (!inferDefinitionReturnKind(*methodIt->second)) {
              return ReturnKind::Unknown;
            }
            auto kindIt = returnKinds_.find(methodResolved);
            if (kindIt != returnKinds_.end()) {
              return kindIt->second;
            }
          }
        }
        return ReturnKind::Unknown;
      }
      return ReturnKind::Int;
    }
    if (!expr.isMethodCall && expr.name == "capacity" && expr.args.size() == 1 &&
        defMap_.find(resolved) == defMap_.end()) {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        return ReturnKind::Int;
      }
    }
    std::string builtinName;
    if (getBuiltinArrayAccessName(expr, builtinName) && expr.args.size() == 2) {
      std::string elemType;
      if (resolveStringTarget(expr.args.front())) {
        return ReturnKind::Int;
      }
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      if (resolveArrayTarget(expr.args.front(), elemType)) {
        ReturnKind kind = returnKindForTypeName(elemType);
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
      if (builtinName == "dereference") {
        ReturnKind targetKind = pointerTargetKind(expr.args.front());
        if (targetKind != ReturnKind::Unknown) {
          return targetKind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinComparisonName(expr, builtinName)) {
      return ReturnKind::Bool;
    }
    if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
      if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
        return ReturnKind::Bool;
      }
      if (builtinName == "lerp" && expr.args.size() == 3) {
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      if (builtinName == "pow" && expr.args.size() == 2) {
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
    if (getBuiltinOperatorName(expr, builtinName)) {
      if (builtinName == "negate") {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
      ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
      return combineNumeric(left, right);
    }
    if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 3) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
      result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
      return result;
    }
    if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
      return result;
    }
    if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (getBuiltinConvertName(expr, builtinName)) {
      if (expr.templateArgs.size() != 1) {
        return ReturnKind::Unknown;
      }
      return returnKindForTypeName(expr.templateArgs.front());
    }
    if (getBuiltinMutationName(expr, builtinName)) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args.front(), params, locals);
    }
    if (isAssignCall(expr)) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args[1], params, locals);
    }
    return ReturnKind::Unknown;
  }
  return ReturnKind::Unknown;
}

bool SemanticsValidator::inferDefinitionReturnKind(const Definition &def) {
  auto kindIt = returnKinds_.find(def.fullPath);
  if (kindIt == returnKinds_.end()) {
    return false;
  }
  if (kindIt->second != ReturnKind::Unknown) {
    return true;
  }
  if (!inferenceStack_.insert(def.fullPath).second) {
    error_ = "return type inference requires explicit annotation on " + def.fullPath;
    return false;
  }
  ReturnKind inferred = ReturnKind::Unknown;
  bool sawReturn = false;
  const auto &defParams = paramsByDef_[def.fullPath];
  std::unordered_map<std::string, BindingInfo> locals;
  std::function<bool(const Expr &, std::unordered_map<std::string, BindingInfo> &)> inferStatement;
  inferStatement = [&](const Expr &stmt, std::unordered_map<std::string, BindingInfo> &activeLocals) -> bool {
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
      sawReturn = true;
      ReturnKind exprKind = ReturnKind::Void;
      if (!stmt.args.empty()) {
        exprKind = inferExprReturnKind(stmt.args.front(), defParams, activeLocals);
      }
      if (exprKind == ReturnKind::Unknown) {
        if (error_.empty()) {
          error_ = "unable to infer return type on " + def.fullPath;
        }
        return false;
      }
      if (inferred == ReturnKind::Unknown) {
        inferred = exprKind;
        return true;
      }
      if (inferred != exprKind) {
        if (error_.empty()) {
          error_ = "conflicting return types on " + def.fullPath;
        }
        return false;
      }
      return true;
    }
    if (isIfCall(stmt) && stmt.args.size() == 3) {
      const Expr &thenBlock = stmt.args[1];
      const Expr &elseBlock = stmt.args[2];
      auto walkBlock = [&](const Expr &block) -> bool {
        std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
        for (const auto &bodyExpr : block.bodyArguments) {
          if (!inferStatement(bodyExpr, blockLocals)) {
            return false;
          }
        }
        return true;
      };
      if (!walkBlock(thenBlock)) {
        return false;
      }
      if (!walkBlock(elseBlock)) {
        return false;
      }
      return true;
    }
    if (isLoopCall(stmt) && stmt.args.size() == 2) {
      const Expr &body = stmt.args[1];
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : body.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isWhileCall(stmt) && stmt.args.size() == 2) {
      const Expr &body = stmt.args[1];
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : body.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isForCall(stmt) && stmt.args.size() == 4) {
      std::unordered_map<std::string, BindingInfo> loopLocals = activeLocals;
      if (!inferStatement(stmt.args[0], loopLocals)) {
        return false;
      }
      if (!inferStatement(stmt.args[2], loopLocals)) {
        return false;
      }
      const Expr &body = stmt.args[3];
      std::unordered_map<std::string, BindingInfo> blockLocals = loopLocals;
      for (const auto &bodyExpr : body.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isRepeatCall(stmt)) {
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : stmt.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (isBlockCall(stmt) && stmt.hasBodyArguments) {
      std::unordered_map<std::string, BindingInfo> blockLocals = activeLocals;
      for (const auto &bodyExpr : stmt.bodyArguments) {
        if (!inferStatement(bodyExpr, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    return true;
  };

  for (const auto &stmt : def.statements) {
    if (!inferStatement(stmt, locals)) {
      return false;
    }
  }
  if (!sawReturn) {
    kindIt->second = ReturnKind::Void;
  } else if (inferred == ReturnKind::Unknown) {
    if (error_.empty()) {
      error_ = "unable to infer return type on " + def.fullPath;
    }
    return false;
  } else {
    kindIt->second = inferred;
  }
  inferenceStack_.erase(def.fullPath);
  return true;
}

} // namespace primec::semantics
