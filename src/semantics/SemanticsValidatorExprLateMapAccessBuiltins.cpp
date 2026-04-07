#include "SemanticsValidator.h"

#include <string>
#include <utility>

namespace primec::semantics {

namespace {

bool getCanonicalMapAccessBuiltinName(const Expr &candidate,
                                      std::string &helperOut) {
  helperOut.clear();
  if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall ||
      candidate.name.empty() || candidate.args.size() != 2) {
    return false;
  }
  if (getBuiltinArrayAccessName(candidate, helperOut)) {
    return true;
  }
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  if (normalizedName == "map/at_ref" ||
      normalizedName == "std/collections/map/at_ref") {
    helperOut = "at_ref";
    return true;
  }
  if (normalizedName == "map/at_unsafe_ref" ||
      normalizedName == "std/collections/map/at_unsafe_ref") {
    helperOut = "at_unsafe_ref";
    return true;
  }
  std::string namespacePrefix = candidate.namespacePrefix;
  if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
    namespacePrefix.erase(namespacePrefix.begin());
  }
  if ((namespacePrefix == "map" ||
       namespacePrefix == "std/collections/map") &&
      (candidate.name == "at_ref" || candidate.name == "at_unsafe_ref")) {
    helperOut = candidate.name;
    return true;
  }
  if (candidate.name.find('/') == std::string::npos &&
      (candidate.name == "at_ref" || candidate.name == "at_unsafe_ref")) {
    helperOut = candidate.name;
    return true;
  }
  return false;
}

} // namespace

bool SemanticsValidator::validateExprLateMapAccessBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const ExprLateMapAccessBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.dispatchResolvers == nullptr) {
    return true;
  }

  auto failLateMapAccessBuiltinDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  auto failLateMapAccessKeyMismatch = [&](const std::string &helperName,
                                          const std::string &mapKeyType) {
    if (expr.name.rfind("/std/collections/map/", 0) == 0 ||
        expr.namespacePrefix == "/std/collections/map" ||
        expr.namespacePrefix == "std/collections/map") {
      return failLateMapAccessBuiltinDiagnostic(
          "argument type mismatch for /std/collections/map/" + helperName +
          " parameter key");
    }
    if (normalizeBindingTypeName(mapKeyType) == "string") {
      return failLateMapAccessBuiltinDiagnostic(helperName +
                                                " requires string map key");
    }
    return failLateMapAccessBuiltinDiagnostic(helperName +
                                              " requires map key type " +
                                              mapKeyType);
  };
  auto hasBareMapContainsBuiltinDefinition = [&]() {
    return hasImportedDefinitionPath("/std/collections/map/contains") ||
           hasDeclaredDefinitionPath("/std/collections/map/contains") ||
           hasImportedDefinitionPath("/std/collections/map/contains_ref") ||
           hasDeclaredDefinitionPath("/std/collections/map/contains_ref") ||
           hasImportedDefinitionPath("/contains") ||
           hasDeclaredDefinitionPath("/contains");
  };

  std::string builtinName;
  if (!expr.isMethodCall &&
      getCanonicalMapAccessBuiltinName(expr, builtinName) &&
      expr.args.size() == 2 && !hasNamedArguments(expr.argNames)) {
    if (!this->isMapLikeBareAccessReceiver(expr.args.front(), params, locals,
                                           *context.dispatchResolvers) &&
        this->isMapLikeBareAccessReceiver(expr.args[1], params, locals,
                                          *context.dispatchResolvers)) {
      Expr rewrittenMapAccessCall = expr;
      std::swap(rewrittenMapAccessCall.args[0], rewrittenMapAccessCall.args[1]);
      handledOut = true;
      return validateExpr(params, locals, rewrittenMapAccessCall);
    }
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "contains") &&
      !context.shouldBuiltinValidateBareMapContainsCall) {
    Expr rewrittenMapHelperCall;
    if (this->tryRewriteBareMapHelperCall(expr, "contains",
                                          *context.dispatchResolvers,
                                          rewrittenMapHelperCall)) {
      handledOut = true;
      return validateExpr(params, locals, rewrittenMapHelperCall);
    }
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") &&
      !context.shouldBuiltinValidateBareMapTryAtCall) {
    Expr rewrittenMapHelperCall;
    if (this->tryRewriteBareMapHelperCall(expr, "tryAt",
                                          *context.dispatchResolvers,
                                          rewrittenMapHelperCall)) {
      if (rewrittenMapHelperCall.name == "/std/collections/map/tryAt" &&
          !hasImportedDefinitionPath("/std/collections/map/tryAt") &&
          !hasDeclaredDefinitionPath("/tryAt")) {
        return failLateMapAccessBuiltinDiagnostic(
            "unknown call target: /std/collections/map/tryAt");
      }
      handledOut = true;
      return validateExpr(params, locals, rewrittenMapHelperCall);
    }
  }

  if (!expr.isMethodCall &&
      getCanonicalMapAccessBuiltinName(expr, builtinName) &&
      !context.shouldBuiltinValidateBareMapAccessCall) {
    Expr rewrittenMapHelperCall;
    if (this->tryRewriteBareMapHelperCall(expr, builtinName,
                                          *context.dispatchResolvers,
                                          rewrittenMapHelperCall)) {
      handledOut = true;
      return validateExpr(params, locals, rewrittenMapHelperCall);
    }
  }

  auto resolveMapKeyTypeWithInference =
      [&](const Expr &receiverExpr, std::string &mapKeyTypeOut) {
        if (this->resolveMapKeyType(receiverExpr, *context.dispatchResolvers,
                                    mapKeyTypeOut)) {
          return true;
        }
        std::string receiverTypeText;
        std::string mapValueType;
        return inferQueryExprTypeText(receiverExpr, params, locals,
                                      receiverTypeText) &&
               extractMapKeyValueTypesFromTypeText(receiverTypeText,
                                                  mapKeyTypeOut, mapValueType);
      };

  if (!expr.isMethodCall && isSimpleCallName(expr, "contains") &&
      expr.args.size() == 2 &&
      (context.shouldBuiltinValidateBareMapContainsCall ||
       this->isMapLikeBareAccessReceiver(
           expr.args[this->mapHelperReceiverIndex(expr, *context.dispatchResolvers)],
           params, locals, *context.dispatchResolvers) ||
       this->isIndexedArgsPackMapReceiverTarget(
           expr.args.front(), *context.dispatchResolvers))) {
    if (!hasBareMapContainsBuiltinDefinition()) {
      return failLateMapAccessBuiltinDiagnostic(
          "unknown call target: /std/collections/map/contains");
    }
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareMapOperands =
        this->bareMapHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
    std::string mapKeyType;
    if (!resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      return failLateMapAccessBuiltinDiagnostic("contains requires map target");
    }
    if (!mapKeyType.empty()) {
      if (normalizeBindingTypeName(mapKeyType) == "string") {
        if (!this->isStringExprForArgumentValidation(keyExpr,
                                                     *context.dispatchResolvers)) {
          return failLateMapAccessBuiltinDiagnostic(
              "contains requires string map key");
        }
      } else {
        ReturnKind keyKind =
            returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
        if (keyKind != ReturnKind::Unknown) {
          if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
            return failLateMapAccessBuiltinDiagnostic(
                "contains requires map key type " + mapKeyType);
          }
          ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
          if (candidateKind != ReturnKind::Unknown &&
              candidateKind != keyKind) {
            return failLateMapAccessBuiltinDiagnostic(
                "contains requires map key type " + mapKeyType);
          }
        }
      }
    }
    if (!validateExpr(params, locals, expr.args.front()) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    handledOut = true;
    return true;
  }

  if (((expr.isMethodCall &&
        (resolved == "/std/collections/map/tryAt" ||
         resolved == "/std/collections/map/tryAt_ref")) ||
       (!expr.isMethodCall &&
        (isSimpleCallName(expr, "tryAt") ||
         resolved == "/std/collections/map/tryAt"))) &&
      expr.args.size() == 2 &&
      (context.shouldBuiltinValidateBareMapTryAtCall ||
       this->isMapLikeBareAccessReceiver(
           expr.args[this->mapHelperReceiverIndex(expr, *context.dispatchResolvers)],
           params, locals, *context.dispatchResolvers) ||
       this->isIndexedArgsPackMapReceiverTarget(
           expr.args[this->mapHelperReceiverIndex(expr, *context.dispatchResolvers)],
           *context.dispatchResolvers))) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareMapOperands =
        this->bareMapHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
    if (((!expr.isMethodCall && resolved == "/std/collections/map/tryAt") ||
         resolved == "/std/collections/map/tryAt_ref") &&
        !hasImportedDefinitionPath(resolved == "/std/collections/map/tryAt_ref"
                                       ? "/std/collections/map/tryAt_ref"
                                       : "/std/collections/map/tryAt") &&
        !hasDeclaredDefinitionPath(resolved == "/std/collections/map/tryAt_ref"
                                       ? "/std/collections/map/tryAt_ref"
                                       : "/std/collections/map/tryAt")) {
      return failLateMapAccessBuiltinDiagnostic(
          "unknown call target: " +
          std::string(resolved == "/std/collections/map/tryAt_ref"
                          ? "/std/collections/map/tryAt_ref"
                          : "/std/collections/map/tryAt"));
    }
    std::string mapKeyType;
    if (!resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
      if (!validateExpr(params, locals, receiverExpr)) {
        return false;
      }
      return failLateMapAccessBuiltinDiagnostic("tryAt requires map target");
    }
    if (!mapKeyType.empty()) {
      if (normalizeBindingTypeName(mapKeyType) == "string") {
        if (!this->isStringExprForArgumentValidation(keyExpr,
                                                     *context.dispatchResolvers)) {
          return failLateMapAccessBuiltinDiagnostic(
              "tryAt requires string map key");
        }
      } else {
        ReturnKind keyKind =
            returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
        if (keyKind != ReturnKind::Unknown) {
          if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
            return failLateMapAccessBuiltinDiagnostic(
                "tryAt requires map key type " + mapKeyType);
          }
          ReturnKind candidateKind = inferExprReturnKind(keyExpr, params, locals);
          if (candidateKind != ReturnKind::Unknown &&
              candidateKind != keyKind) {
            return failLateMapAccessBuiltinDiagnostic(
                "tryAt requires map key type " + mapKeyType);
          }
        }
      }
    }
    if (!validateExpr(params, locals, expr.args.front()) ||
        !validateExpr(params, locals, expr.args[1])) {
      return false;
    }
    handledOut = true;
    return true;
  }

  if (!expr.isMethodCall &&
      getCanonicalMapAccessBuiltinName(expr, builtinName) &&
      expr.args.size() == 2 &&
      (context.shouldBuiltinValidateBareMapAccessCall ||
       this->isMapLikeBareAccessReceiver(
           expr.args[this->mapHelperReceiverIndex(expr, *context.dispatchResolvers)],
           params, locals, *context.dispatchResolvers) ||
       this->isIndexedArgsPackMapReceiverTarget(
           expr.args[this->mapHelperReceiverIndex(expr, *context.dispatchResolvers)],
           *context.dispatchResolvers))) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareMapOperands =
        this->bareMapHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
    if (expr.name.rfind("/std/collections/map/", 0) == 0 &&
        !hasImportedDefinitionPath("/std/collections/map/" + builtinName) &&
        !hasDeclaredDefinitionPath("/std/collections/map/" + builtinName)) {
      return failLateMapAccessBuiltinDiagnostic(
          "unknown call target: /std/collections/map/" + builtinName);
    }
    std::string mapKeyType;
    if (resolveMapKeyTypeWithInference(receiverExpr, mapKeyType)) {
      if (!mapKeyType.empty()) {
        if (normalizeBindingTypeName(mapKeyType) == "string") {
          if (!this->isStringExprForArgumentValidation(keyExpr,
                                                       *context.dispatchResolvers)) {
            return failLateMapAccessKeyMismatch(builtinName, mapKeyType);
          }
        } else {
          ReturnKind keyKind =
              returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
          if (keyKind != ReturnKind::Unknown) {
            if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
              return failLateMapAccessKeyMismatch(builtinName, mapKeyType);
            }
            ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
            if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
              return failLateMapAccessKeyMismatch(builtinName, mapKeyType);
            }
          }
        }
      }
      if (!validateExpr(params, locals, expr.args.front()) ||
          !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      handledOut = true;
      return true;
    }
  }

  if (!expr.isMethodCall &&
      getCanonicalMapAccessBuiltinName(expr, builtinName) &&
      expr.args.size() == 2 && defMap_.find(resolved) == defMap_.end() &&
      hasImportedDefinitionPath("/std/collections/map/" + builtinName)) {
    size_t receiverIndex = 0;
    size_t keyIndex = 1;
    const bool hasBareMapOperands =
        this->bareMapHelperOperandIndices(expr, *context.dispatchResolvers,
                                          receiverIndex, keyIndex);
    const Expr &receiverExpr =
        hasBareMapOperands ? expr.args[receiverIndex] : expr.args.front();
    const Expr &keyExpr =
        hasBareMapOperands ? expr.args[keyIndex] : expr.args[1];
    std::string receiverTypeText;
    std::string mapKeyType;
    std::string mapValueType;
    if (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText) &&
        extractMapKeyValueTypesFromTypeText(receiverTypeText, mapKeyType,
                                           mapValueType)) {
      if (!mapKeyType.empty()) {
        if (normalizeBindingTypeName(mapKeyType) == "string") {
          if (!this->isStringExprForArgumentValidation(keyExpr,
                                                       *context.dispatchResolvers)) {
            return failLateMapAccessKeyMismatch(builtinName, mapKeyType);
          }
        } else {
          ReturnKind keyKind =
              returnKindForTypeName(normalizeBindingTypeName(mapKeyType));
          if (keyKind != ReturnKind::Unknown) {
            if (context.dispatchResolvers->resolveStringTarget(keyExpr)) {
              return failLateMapAccessKeyMismatch(builtinName, mapKeyType);
            }
            ReturnKind indexKind = inferExprReturnKind(keyExpr, params, locals);
            if (indexKind != ReturnKind::Unknown && indexKind != keyKind) {
              return failLateMapAccessKeyMismatch(builtinName, mapKeyType);
            }
          }
        }
      }
      if (!validateExpr(params, locals, expr.args.front()) ||
          !validateExpr(params, locals, expr.args[1])) {
        return false;
      }
      handledOut = true;
      return true;
    }
  }

  return true;
}

} // namespace primec::semantics
