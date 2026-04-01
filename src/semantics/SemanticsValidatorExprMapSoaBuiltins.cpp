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
    if (arg.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (context.resolveStringTarget != nullptr && context.resolveStringTarget(arg)) {
      return true;
    }
    return inferExprReturnKind(arg, params, locals) == ReturnKind::String;
  };
  auto findNamedBinding = [&](const Expr &target) -> const BindingInfo * {
    if (target.kind != Expr::Kind::Name) {
      return nullptr;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
      return paramBinding;
    }
    auto it = locals.find(target.name);
    if (it != locals.end()) {
      return &it->second;
    }
    return nullptr;
  };
  auto resolveSoaVectorOrExperimentalBorrowedTarget = [&](const Expr &target,
                                                           std::string &elemTypeOut) -> bool {
    if (context.resolveSoaVectorTarget != nullptr &&
        context.resolveSoaVectorTarget(target, elemTypeOut)) {
      return true;
    }
    const BindingInfo *binding = findNamedBinding(target);
    if (binding == nullptr) {
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(binding->typeName);
    if (normalizedType != "Reference" && normalizedType != "Pointer") {
      return false;
    }
    return extractExperimentalSoaVectorElementType(*binding, elemTypeOut);
  };

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
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareMapOperands =
        context.bareMapHelperOperandIndices != nullptr &&
        context.bareMapHelperOperandIndices(expr, receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
    std::string mapKeyType;
    if (!(context.resolveMapKeyType != nullptr &&
          context.resolveMapKeyType(receiverExpr, mapKeyType))) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      error_ = helperName + " requires map target";
      return false;
    }
    if (!validateMapContainsKeyExpr(keyExpr, mapKeyType)) {
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
    if (!hasImportedDefinitionPath("/std/collections/map/contains") &&
        !hasDeclaredDefinitionPath("/std/collections/map/contains") &&
        !hasImportedDefinitionPath("/contains") &&
        !hasDeclaredDefinitionPath("/contains")) {
      error_ = "unknown call target: /std/collections/map/contains";
      return false;
    }
    return validateContainsBuiltin("contains");
  }

  if (resolvedMethod && resolved == "/std/collections/map/contains") {
    handledOut = true;
    return validateContainsBuiltin("contains");
  }

  const bool isCanonicalSoaToAosResolved =
      resolved.rfind("/std/collections/soa_vector/to_aos", 0) == 0;

  if ((!resolvedMethod &&
       (isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos")) &&
       resolvedMissing) ||
      (resolvedMethod &&
       (resolved == "/to_soa" || resolved == "/to_aos" ||
        isCanonicalSoaToAosResolved)) ||
      (!resolvedMethod && isCanonicalSoaToAosResolved)) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    const std::string helperName =
        (isSimpleCallName(expr, "to_soa") || resolved == "/to_soa") ? "to_soa" : "to_aos";
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
            : resolveSoaVectorOrExperimentalBorrowedTarget(expr.args.front(), elemType);
    if (!targetValid) {
      if (helperName == "to_aos" && isCanonicalSoaToAosResolved) {
        error_ = "argument type mismatch for /std/collections/soa_vector/to_aos parameter values";
      } else {
        error_ = helperName == "to_soa" ? "to_soa requires vector target"
                                        : "to_aos requires soa_vector target";
      }
      return false;
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    return true;
  }

  if ((resolvedMethod || resolvedMissing) &&
      (resolved == "/soa_vector/get" ||
       resolved == "/soa_vector/ref" ||
       (resolvedMethod && resolved == "/std/collections/soa_vector/get") ||
       (resolvedMethod && resolved == "/std/collections/soa_vector/ref"))) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
    const std::string helperName =
        (resolved == "/soa_vector/ref" || resolved == "/std/collections/soa_vector/ref") ? "ref"
                                                                                          : "get";
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
    if (!resolveSoaVectorOrExperimentalBorrowedTarget(expr.args.front(), elemType)) {
      error_ = helperName + " requires soa_vector target";
      return false;
    }
    if (!isIntegerExpr(expr.args[1])) {
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

  std::string soaFieldViewName;
  if (resolvedMethod && splitSoaFieldViewHelperPath(resolved, &soaFieldViewName)) {
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
      error_ = "soa_vector field views require value.<field>()[index] syntax: " + expr.name;
      return false;
    }
    std::string elemType;
    if (!resolveSoaVectorOrExperimentalBorrowedTarget(expr.args.front(), elemType)) {
      error_ = "soa_vector field view requires soa_vector target";
      return false;
    }
    error_ = soaFieldViewPendingDiagnostic(soaFieldViewName);
    return false;
  }

  if (!resolvedMethod && (expr.name == "get" || expr.name == "ref") && resolvedMissing) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
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
    if (!isIntegerExpr(expr.args[1])) {
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

  if (!resolvedMethod && !expr.isMethodCall && resolvedMissing && !expr.args.empty()) {
    const bool handledBuiltinName = isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref") ||
                                    isSimpleCallName(expr, "to_soa") ||
                                    isSimpleCallName(expr, "to_aos") ||
                                    isSimpleCallName(expr, "contains");
    if (!handledBuiltinName) {
      std::string elemType;
      if (resolveSoaVectorOrExperimentalBorrowedTarget(expr.args.front(), elemType)) {
        handledOut = true;
        if (expr.args.size() != 1) {
          error_ = "soa_vector field views require value.<field>()[index] syntax: " + expr.name;
          return false;
        }
      }
    }
  }

  return true;
}

} // namespace primec::semantics
