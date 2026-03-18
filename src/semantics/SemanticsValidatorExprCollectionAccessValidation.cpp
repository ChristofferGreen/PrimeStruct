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
    handledOut = true;
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

    std::string elemType;
    const bool isArrayOrString =
        (context.resolveArgsPackAccessTarget != nullptr &&
         context.resolveArgsPackAccessTarget(expr.args.front(), elemType)) ||
        (context.resolveArrayTarget != nullptr &&
         context.resolveArrayTarget(expr.args.front(), elemType)) ||
        (context.resolveStringTarget != nullptr &&
         context.resolveStringTarget(expr.args.front()));
    std::string mapKeyType;
    const bool isMap = context.resolveMapKeyType != nullptr &&
                       context.resolveMapKeyType(expr.args.front(), mapKeyType);
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
      if (!isIntegerExpr(expr.args[1], params, locals)) {
        error_ = builtinName + " requires integer index";
        return false;
      }
    } else if (!validateMapKeyExpr(builtinName, expr.args[1], mapKeyType)) {
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
