#include "SemanticsValidator.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace primec::semantics {

bool SemanticsValidator::validateExprMapSoaBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprMapSoaBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  const bool resolvedMissing = defMap_.find(resolved) == defMap_.end();

  auto validateMapContainsKeyExpr = [&](const Expr &keyExpr,
                                        const std::string &mapKeyType) -> bool {
    if (mapKeyType.empty()) {
      return true;
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      if (!isStringExpr(keyExpr)) {
        error_ = "contains requires string map key";
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
      error_ = "contains requires map key type " + mapKeyType;
      return false;
    }
    ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
    if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
      error_ = "contains requires map key type " + mapKeyType;
      return false;
    }
    return true;
  };

  auto validateContainsBuiltin = [&](const std::string &helperName) -> bool {
    if (!expr.templateArgs.empty()) {
      error_ = helperName + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = helperName + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 2) {
      error_ = "argument count mismatch for builtin " + helperName;
      return false;
    }
    std::string mapKeyType;
    if (!(context.resolveMapKeyType != nullptr &&
          context.resolveMapKeyType(expr.args.front(), mapKeyType))) {
      if (!validateExpr(params, locals, expr.args.front())) {
        return false;
      }
      error_ = helperName + " requires map target";
      return false;
    }
    if (!validateMapContainsKeyExpr(expr.args[1], mapKeyType)) {
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front()) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    return true;
  };

  if (!resolvedMethod && !expr.isMethodCall && isSimpleCallName(expr, "contains") &&
      context.shouldBuiltinValidateBareMapContainsCall && resolvedMissing) {
    handledOut = true;
    return validateContainsBuiltin("contains");
  }

  if (resolvedMethod && resolved == "/std/collections/map/contains") {
    handledOut = true;
    return validateContainsBuiltin("contains");
  }

  if (!resolvedMethod &&
      (isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos")) &&
      resolvedMissing) {
    handledOut = true;
    const std::string helperName =
        isSimpleCallName(expr, "to_soa") ? "to_soa" : "to_aos";
    if (!expr.templateArgs.empty()) {
      error_ = helperName + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = helperName + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin " + helperName;
      return false;
    }
    std::string elemType;
    const bool targetValid =
        helperName == "to_soa"
            ? (context.resolveVectorTarget != nullptr &&
               context.resolveVectorTarget(expr.args.front(), elemType))
            : (context.resolveSoaVectorTarget != nullptr &&
               context.resolveSoaVectorTarget(expr.args.front(), elemType));
    if (!targetValid) {
      error_ = helperName == "to_soa" ? "to_soa requires vector target"
                                      : "to_aos requires soa_vector target";
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    return true;
  }

  if (resolvedMethod && (resolved == "/soa_vector/get" || resolved == "/soa_vector/ref")) {
    handledOut = true;
    const std::string helperName = resolved == "/soa_vector/ref" ? "ref" : "get";
    if (!expr.templateArgs.empty()) {
      error_ = helperName + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = helperName + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 2) {
      error_ = "argument count mismatch for builtin " + helperName;
      return false;
    }
    std::string elemType;
    if (!(context.resolveSoaVectorTarget != nullptr &&
          context.resolveSoaVectorTarget(expr.args.front(), elemType))) {
      error_ = helperName + " requires soa_vector target";
      return false;
    }
    if (!isIntegerExpr(expr.args[1], params, locals)) {
      error_ = helperName + " requires integer index";
      return false;
    }
    for (const auto &arg : expr.args) {
      if (!validateExpr(params, locals, arg)) {
        return false;
      }
    }
    return true;
  }

  if (resolvedMethod && resolved.rfind("/soa_vector/field_view/", 0) == 0) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    if (!expr.templateArgs.empty()) {
      error_ = expr.name + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = expr.name + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 1) {
      error_ = expr.name + " does not accept arguments";
      return false;
    }
    std::string elemType;
    if (!(context.resolveSoaVectorTarget != nullptr &&
          context.resolveSoaVectorTarget(expr.args.front(), elemType))) {
      error_ = "soa_vector field view requires soa_vector target";
      return false;
    }
    error_ = "soa_vector field views are not implemented yet: " + expr.name;
    return false;
  }

  if (!resolvedMethod && (expr.name == "get" || expr.name == "ref") && resolvedMissing) {
    handledOut = true;
    const std::string helperName = expr.name;
    if (!expr.templateArgs.empty()) {
      error_ = helperName + " does not accept template arguments";
      return false;
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error_ = helperName + " does not accept block arguments";
      return false;
    }
    if (expr.args.size() != 2) {
      error_ = "argument count mismatch for builtin " + helperName;
      return false;
    }
    std::string elemType;
    if (!(context.resolveSoaVectorTarget != nullptr &&
          context.resolveSoaVectorTarget(expr.args.front(), elemType))) {
      error_ = helperName + " requires soa_vector target";
      return false;
    }
    if (!isIntegerExpr(expr.args[1], params, locals)) {
      error_ = helperName + " requires integer index";
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
