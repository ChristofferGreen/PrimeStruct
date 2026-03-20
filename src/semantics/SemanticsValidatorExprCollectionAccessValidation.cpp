#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {
namespace {

bool getRemovedVectorAccessBuiltinName(const Expr &candidate, std::string &helperOut) {
  helperOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty()) {
    return false;
  }
  std::string normalized = candidate.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  if (normalized == "vector/at") {
    helperOut = "at";
    return true;
  }
  if (normalized == "vector/at_unsafe") {
    helperOut = "at_unsafe";
    return true;
  }
  return false;
}

} // namespace

bool SemanticsValidator::validateExprCollectionAccessFallbacks(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprCollectionAccessValidationContext &context,
    bool &handledOut) {
  handledOut = false;
  const bool resolvedMissing = defMap_.find(resolved) == defMap_.end();

  auto returnKindForBinding = [](const BindingInfo &binding) -> ReturnKind {
    if (binding.typeName == "Reference") {
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          return ReturnKind::Array;
        }
      }
      return returnKindForTypeName(binding.typeTemplateArg);
    }
    return returnKindForTypeName(binding.typeName);
  };
  auto isIntegerExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
        kind == ReturnKind::String || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral ||
          arg.kind == Expr::Kind::BoolLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 ||
                 paramKind == ReturnKind::UInt64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 ||
                 localKind == ReturnKind::UInt64;
        }
      }
      return true;
    }
    return false;
  };
  auto isStringExpr = [&](const Expr &arg) -> bool {
    if (context.resolveStringTarget != nullptr && context.resolveStringTarget(arg)) {
      return true;
    }
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    return kind == ReturnKind::String || arg.kind == Expr::Kind::StringLiteral;
  };

  auto validateMapKeyExpr = [&](const std::string &helperName,
                                const Expr &keyExpr,
                                const std::string &mapKeyType) -> bool {
    if (mapKeyType.empty()) {
      return true;
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      if (!isStringExpr(keyExpr)) {
        error_ = helperName + " requires string map key";
        return false;
      }
      return true;
    }
    ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
    if (keyKind == ReturnKind::Unknown) {
      return true;
    }
    if (context.resolveStringTarget != nullptr &&
        context.resolveStringTarget(keyExpr)) {
      error_ = helperName + " requires map key type " + mapKeyType;
      return false;
    }
    ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
    if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
      error_ = helperName + " requires map key type " + mapKeyType;
      return false;
    }
    return true;
  };

  if (!resolvedMethod && resolvedMissing &&
      !(context.isStdNamespacedVectorAccessCall &&
        hasNamedArguments(expr.argNames))) {
    std::string removedVectorAccessBuiltinName;
    if (getRemovedVectorAccessBuiltinName(expr, removedVectorAccessBuiltinName)) {
      if (hasNamedArguments(expr.argNames)) {
        error_ = "named arguments not supported for builtin calls";
        return false;
      }
      if (!expr.templateArgs.empty()) {
        error_ = removedVectorAccessBuiltinName +
                 " does not accept template arguments";
        return false;
      }
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error_ =
            removedVectorAccessBuiltinName + " does not accept block arguments";
        return false;
      }
      if (expr.args.size() != 2) {
        error_ = "argument count mismatch for builtin " +
                 removedVectorAccessBuiltinName;
        return false;
      }
    }
  }

  std::string builtinName;
  if (getBuiltinArrayAccessName(expr, builtinName) &&
      (!context.isStdNamespacedVectorAccessCall ||
       context.shouldAllowStdAccessCompatibilityFallback ||
       context.hasStdNamespacedVectorAccessDefinition) &&
      (!context.isStdNamespacedMapAccessCall ||
       context.hasStdNamespacedMapAccessDefinition) &&
      !(context.isStdNamespacedVectorAccessCall &&
        hasNamedArguments(expr.argNames))) {
    if (!expr.isMethodCall &&
        context.shouldBuiltinValidateBareMapAccessCall &&
        context.isMapLikeBareAccessReceiverTarget != nullptr &&
        expr.args.size() == 2 &&
        (context.isMapLikeBareAccessReceiverTarget(expr.args.front()) ||
         context.isMapLikeBareAccessReceiverTarget(expr.args[1]))) {
      return true;
    }
    if (!context.shouldBuiltinValidateBareMapAccessCall) {
      Expr rewrittenMapHelperCall;
      if (context.tryRewriteBareMapHelperCall != nullptr &&
          context.tryRewriteBareMapHelperCall(expr, builtinName,
                                             rewrittenMapHelperCall)) {
        return validateExpr(params, locals, rewrittenMapHelperCall);
      }
    }
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = builtinName + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = builtinName + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 2) {
      error_ = "argument count mismatch for builtin " + builtinName;
      return false;
    }

    size_t indexArgIndex = 1;
    std::string elemType;
    auto isArrayOrStringTarget = [&](const Expr &candidate, std::string &elemTypeOut) {
      return (context.resolveArgsPackAccessTarget != nullptr &&
              context.resolveArgsPackAccessTarget(candidate, elemTypeOut)) ||
             (context.resolveArrayTarget != nullptr &&
              context.resolveArrayTarget(candidate, elemTypeOut)) ||
             (context.resolveStringTarget != nullptr &&
              context.resolveStringTarget(candidate));
    };
    std::string mapKeyType;
    std::string mapValueType;
    auto isMapTarget = [&](const Expr &candidate, std::string &mapKeyTypeOut) {
      return context.resolveMapKeyType != nullptr &&
             context.resolveMapKeyType(candidate, mapKeyTypeOut);
    };
    bool isArrayOrString = isArrayOrStringTarget(expr.args.front(), elemType);
    bool isMap = isMapTarget(expr.args.front(), mapKeyType);
    bool isExperimentalMap =
        context.resolveExperimentalMapTarget != nullptr &&
        context.resolveExperimentalMapTarget(expr.args.front(), mapKeyType,
                                             mapValueType);
    const bool shouldProbeReorderedReceiver =
        expr.args.size() == 2 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         (expr.args.front().kind == Expr::Kind::Name && !isArrayOrString &&
          !isMap && !isExperimentalMap));
    if (!isArrayOrString && !isMap && !isExperimentalMap &&
        shouldProbeReorderedReceiver) {
      std::string reorderedElemType;
      std::string reorderedMapKeyType;
      std::string reorderedMapValueType;
      const bool reorderedArrayOrString = isArrayOrStringTarget(expr.args[1], reorderedElemType);
      const bool reorderedMap = isMapTarget(expr.args[1], reorderedMapKeyType);
      const bool reorderedExperimentalMap =
          context.resolveExperimentalMapTarget != nullptr &&
          context.resolveExperimentalMapTarget(expr.args[1], reorderedMapKeyType,
                                               reorderedMapValueType);
      if (reorderedArrayOrString || reorderedMap || reorderedExperimentalMap) {
        indexArgIndex = 0;
        elemType = reorderedElemType;
        mapKeyType = reorderedMapKeyType;
        mapValueType = reorderedMapValueType;
        isArrayOrString = reorderedArrayOrString;
        isMap = reorderedMap;
        isExperimentalMap = reorderedExperimentalMap;
      }
    }
    const bool isExplicitMapAccessHelper =
        resolved == "/map/at" || resolved == "/map/at_unsafe" ||
        resolved == "/std/collections/map/at" ||
        resolved == "/std/collections/map/at_unsafe";
    if (!expr.templateArgs.empty() &&
        (isMap || isExperimentalMap || isExplicitMapAccessHelper)) {
      return true;
    }
    handledOut = true;
    if (isExperimentalMap) {
      error_ = builtinName + " requires integer index";
      return false;
    }
    if (!isArrayOrString && !isMap) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      bool isBuiltinMethod = false;
      std::string methodResolved;
      if (resolveMethodTarget(params, locals, expr.namespacePrefix,
                              expr.args.front(), builtinName, methodResolved,
                              isBuiltinMethod) &&
          !isBuiltinMethod && defMap_.find(methodResolved) == defMap_.end() &&
          context.isNonCollectionStructAccessTarget != nullptr &&
          context.isNonCollectionStructAccessTarget(methodResolved)) {
        error_ = "unknown method: " + methodResolved;
        return false;
      }
      if (expr.args.front().kind == Expr::Kind::Call || expr.args.front().kind == Expr::Kind::Name) {
        std::string receiverStructPath;
        if (expr.args.front().kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, expr.args.front().name)) {
            receiverStructPath = resolveTypePath(paramBinding->typeName, expr.args.front().namespacePrefix);
          } else if (auto it = locals.find(expr.args.front().name); it != locals.end()) {
            receiverStructPath = resolveTypePath(it->second.typeName, expr.args.front().namespacePrefix);
          }
        } else {
          receiverStructPath = inferStructReturnPath(expr.args.front(), params, locals);
          if (receiverStructPath.empty()) {
            auto defIt = defMap_.find(resolveCalleePath(expr.args.front()));
            if (defIt != defMap_.end() && defIt->second != nullptr) {
              BindingInfo inferredReturn;
              if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
                receiverStructPath = resolveTypePath(inferredReturn.typeName, expr.args.front().namespacePrefix);
              }
            }
          }
        }
        if (!receiverStructPath.empty() && receiverStructPath.front() != '/') {
          receiverStructPath.insert(receiverStructPath.begin(), '/');
        }
        if (!receiverStructPath.empty() && context.isNonCollectionStructAccessTarget != nullptr &&
            context.isNonCollectionStructAccessTarget(receiverStructPath + "/" + builtinName)) {
          error_ = "unknown method: " + receiverStructPath + "/" + builtinName;
          return false;
        }
      }
      error_ = builtinName +
               " requires array, vector, map, or string target";
      return false;
    }
    if (isMap && !context.shouldBuiltinValidateBareMapAccessCall &&
        !(context.isIndexedArgsPackMapReceiverTarget != nullptr &&
          context.isIndexedArgsPackMapReceiverTarget(expr.args.front())) &&
        !hasDeclaredDefinitionPath("/map/" + builtinName) &&
        !hasImportedDefinitionPath("/std/collections/map/" + builtinName) &&
        !hasDeclaredDefinitionPath("/std/collections/map/" + builtinName)) {
      error_ = "unknown call target: /std/collections/map/" + builtinName;
      return false;
    }
    if (!isMap) {
      if (!isIntegerExpr(expr.args[indexArgIndex])) {
        error_ = builtinName + " requires integer index";
        return false;
      }
    } else if (!validateMapKeyExpr(builtinName, expr.args[indexArgIndex], mapKeyType)) {
      return false;
    }

    for (const auto &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }

  return true;
}

} // namespace primec::semantics
