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
  auto failMapSoaBuiltinDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
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
  auto validateSoaHelperReturnTemplateArgs =
      [&](const Expr &receiverExpr, const std::string &elemType, const std::string &helperName) {
    if (expr.templateArgs.empty()) {
      return true;
    }
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding ||
        expr.templateArgs.size() != 1 ||
        normalizeBindingTypeName(expr.templateArgs.front()) !=
            normalizeBindingTypeName(elemType)) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " does not accept template arguments");
    }
    return true;
  };
  auto validateMapContainsKeyExpr = [&](const Expr &keyExpr,
                                        const std::string &mapKeyType) -> bool {
    if (mapKeyType.empty()) {
      return true;
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      if (!isStringExpr(keyExpr)) {
        return failMapSoaBuiltinDiagnostic("contains requires string map key");
      }
      return true;
    }

    ReturnKind keyKind = returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
    if (keyKind == ReturnKind::Unknown) {
      return true;
    }
    if (context.resolveStringTarget != nullptr &&
        context.resolveStringTarget(keyExpr)) {
      return failMapSoaBuiltinDiagnostic("contains requires map key type " +
                                        mapKeyType);
    }
    ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
    if (candidateKind != ReturnKind::Unknown && candidateKind != keyKind) {
      return failMapSoaBuiltinDiagnostic("contains requires map key type " +
                                        mapKeyType);
    }
    return true;
  };

  auto validateContainsBuiltin = [&](const std::string &helperName) -> bool {
    if (!expr.templateArgs.empty()) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failMapSoaBuiltinDiagnostic("argument count mismatch for builtin " +
                                        helperName);
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
      return failMapSoaBuiltinDiagnostic(helperName + " requires map target");
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
      return failMapSoaBuiltinDiagnostic(
          "unknown call target: /std/collections/map/contains");
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
      return failMapSoaBuiltinDiagnostic(
          "named arguments not supported for builtin calls");
    }
    const std::string helperName =
        (isSimpleCallName(expr, "to_soa") || resolved == "/to_soa") ? "to_soa" : "to_aos";
    if (!expr.templateArgs.empty()) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " does not accept template arguments");
    }
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " does not accept block arguments");
    }
    if (expr.args.size() != 1) {
      return failMapSoaBuiltinDiagnostic("argument count mismatch for builtin " +
                                        helperName);
    }
    std::string elemType;
    const bool targetValid =
        helperName == "to_soa"
            ? (context.resolveVectorTarget != nullptr &&
               context.resolveVectorTarget(expr.args.front(), elemType))
            : this->resolveSoaVectorOrExperimentalBorrowedReceiver(
                  expr.args.front(),
                  params,
                  locals,
                  context.resolveSoaVectorTarget,
                  elemType);
    if (!targetValid) {
      if (helperName == "to_aos" && isCanonicalSoaToAosResolved) {
        return failMapSoaBuiltinDiagnostic(
            "argument type mismatch for /std/collections/soa_vector/to_aos parameter values");
      } else {
        return failMapSoaBuiltinDiagnostic(
            helperName == "to_soa" ? "to_soa requires vector target"
                                   : "to_aos requires soa_vector target");
      }
    }
    if (!validateExpr(params, locals, expr.args.front())) {
      return false;
    }
    return true;
  }

  const auto soaAccessHelperName =
      builtinSoaAccessHelperName(expr, params, locals);
  if ((resolvedMethod || resolvedMissing ||
       resolved == "/std/collections/soa_vector/get" ||
       resolved == "/std/collections/soa_vector/ref") &&
      soaAccessHelperName.has_value()) {
    handledOut = true;
    if (hasNamedArguments(expr.argNames) &&
        !(context.isNamedArgsPackMethodAccessCall != nullptr &&
          context.isNamedArgsPackMethodAccessCall(expr)) &&
        !(context.isNamedArgsPackWrappedFileBuiltinAccessCall != nullptr &&
          context.isNamedArgsPackWrappedFileBuiltinAccessCall(expr))) {
      return failMapSoaBuiltinDiagnostic(
          "named arguments not supported for builtin calls");
    }
    const std::string &helperName = *soaAccessHelperName;
    if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " does not accept block arguments");
    }
    if (expr.args.size() != 2) {
      return failMapSoaBuiltinDiagnostic("argument count mismatch for builtin " +
                                        helperName);
    }
    std::string elemType;
    if (!this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            expr.args.front(),
            params,
            locals,
            context.resolveSoaVectorTarget,
            elemType)) {
      if (resolved == "/soa_vector/" + helperName &&
          hasVisibleDefinitionPathForCurrentImports(
              "/soa_vector/" + helperName)) {
        handledOut = false;
        return true;
      }
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " requires soa_vector target");
    }
    if (!validateSoaHelperReturnTemplateArgs(expr.args.front(), elemType, helperName)) {
      return false;
    }
    if (!isIntegerExpr(expr.args[1])) {
      return failMapSoaBuiltinDiagnostic(helperName +
                                        " requires integer index");
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
