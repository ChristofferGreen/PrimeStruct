#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::resolveBuiltinCollectionCountCapacityReturnKind(
    const Expr &callExpr,
    const BuiltinCollectionCountCapacityDispatchContext &context,
    ReturnKind &kindOut) {
  kindOut = ReturnKind::Unknown;
  if (callExpr.isMethodCall || callExpr.args.empty()) {
    return false;
  }
  if (!context.isCountLike && !context.isCapacityLike) {
    return false;
  }

  std::string mapKeyType;
  std::string mapValueType;
  if (context.isUnnamespacedMapCountFallbackCall &&
      context.hasDeclaredDefinitionPath != nullptr &&
      !context.hasDeclaredDefinitionPath("/std/collections/map/count") &&
      !context.hasDeclaredDefinitionPath("/map/count") &&
      !hasImportedDefinitionPath("/std/collections/map/count") &&
      context.resolveMapTarget != nullptr &&
      context.resolveMapTarget(callExpr.args.front(), mapKeyType, mapValueType)) {
    kindOut = ReturnKind::Int;
    return true;
  }

  if (context.resolveMethodCallPath == nullptr || context.preferVectorStdlibHelperPath == nullptr) {
    return false;
  }

  std::string methodResolved;
  if (!context.resolveMethodCallPath(context.isCountLike ? "count" : "capacity", methodResolved)) {
    return false;
  }
  methodResolved = context.preferVectorStdlibHelperPath(methodResolved);
  if (context.isCountLike && methodResolved == "/std/collections/map/count" &&
      context.hasDeclaredDefinitionPath != nullptr &&
      !context.hasDeclaredDefinitionPath("/map/count") &&
      !hasImportedDefinitionPath("/std/collections/map/count") &&
      !context.hasDeclaredDefinitionPath("/std/collections/map/count") &&
      !context.shouldInferBuiltinBareMapCountCall) {
    error_ = "unknown call target: /std/collections/map/count";
    return false;
  }

  auto methodIt = defMap_.find(methodResolved);
  if (methodIt != defMap_.end()) {
    if (!inferDefinitionReturnKind(*methodIt->second)) {
      return false;
    }
    auto kindIt = returnKinds_.find(methodResolved);
    if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
      kindOut = kindIt->second;
      return true;
    }
  }

  if (context.dispatchResolvers == nullptr) {
    return false;
  }
  return resolveBuiltinCollectionMethodReturnKind(
      methodResolved, callExpr.args.front(), *context.dispatchResolvers, kindOut);
}

ReturnKind SemanticsValidator::inferBuiltinCollectionDirectCountCapacityReturnKind(
    const Expr &expr,
    const std::string &resolved,
    const BuiltinCollectionDirectCountCapacityContext &context,
    bool &handled) {
  handled = false;
  if (expr.isMethodCall || expr.args.empty()) {
    return ReturnKind::Unknown;
  }
  if (!context.isDirectCountCall && !context.isDirectCapacityCall) {
    return ReturnKind::Unknown;
  }

  handled = true;
  const auto inferHelperReturnKind = [&](const std::string &helperName,
                                         const Expr &receiverExpr,
                                         bool allowBuiltinFallback) -> ReturnKind {
    if (context.resolveMethodCallPath == nullptr || context.preferVectorStdlibHelperPath == nullptr) {
      return ReturnKind::Unknown;
    }
    std::string methodResolved;
    if (!context.resolveMethodCallPath(helperName, methodResolved)) {
      return ReturnKind::Unknown;
    }
    methodResolved = context.preferVectorStdlibHelperPath(methodResolved);
    if (helperName == "count" && methodResolved == "/std/collections/map/count" &&
        context.hasDeclaredDefinitionPath != nullptr &&
        !context.hasDeclaredDefinitionPath("/map/count") &&
        !hasImportedDefinitionPath("/std/collections/map/count") &&
        !context.hasDeclaredDefinitionPath("/std/collections/map/count") &&
        !context.shouldInferBuiltinBareMapCountCall) {
      error_ = "unknown call target: /std/collections/map/count";
      return ReturnKind::Unknown;
    }
    if (allowBuiltinFallback && context.dispatchResolvers != nullptr &&
        defMap_.find(methodResolved) == defMap_.end()) {
      ReturnKind builtinMethodKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionMethodReturnKind(
              methodResolved, receiverExpr, *context.dispatchResolvers, builtinMethodKind)) {
        return builtinMethodKind;
      }
    }
    if (context.inferResolvedPathReturnKind != nullptr) {
      ReturnKind helperReturnKind = ReturnKind::Unknown;
      if (context.inferResolvedPathReturnKind(methodResolved, helperReturnKind)) {
        return helperReturnKind;
      }
    }
    return ReturnKind::Unknown;
  };

  if (context.isDirectCountCall) {
    if (!context.shouldInferBuiltinBareMapCountCall &&
        context.tryRewriteBareMapHelperCall != nullptr &&
        context.inferRewrittenExprReturnKind != nullptr) {
      Expr rewrittenMapHelperCall;
      if (context.tryRewriteBareMapHelperCall(expr, rewrittenMapHelperCall)) {
        return context.inferRewrittenExprReturnKind(rewrittenMapHelperCall);
      }
    }

    const ReturnKind helperReturnKind =
        inferHelperReturnKind("count", expr.args.front(), context.isDirectCountSingleArg);
    if (helperReturnKind != ReturnKind::Unknown) {
      return helperReturnKind;
    }

    if (context.isDirectCountSingleArg) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if ((context.resolveArgsPackCountTarget != nullptr &&
           context.resolveArgsPackCountTarget(expr.args.front(), elemType)) ||
          (context.resolveVectorTarget != nullptr &&
           context.resolveVectorTarget(expr.args.front(), elemType)) ||
          (context.resolveArrayTarget != nullptr &&
           context.resolveArrayTarget(expr.args.front(), elemType)) ||
          (context.resolveStringTarget != nullptr && context.resolveStringTarget(expr.args.front())) ||
          (context.resolveMapTarget != nullptr &&
           context.resolveMapTarget(expr.args.front(), keyType, valueType))) {
        return ReturnKind::Int;
      }
    }

    return ReturnKind::Unknown;
  }

  const ReturnKind helperReturnKind =
      inferHelperReturnKind("capacity", expr.args.front(), context.isDirectCapacitySingleArg);
  if (helperReturnKind != ReturnKind::Unknown) {
    return helperReturnKind;
  }

  if (context.isDirectCapacitySingleArg && context.resolveVectorTarget != nullptr) {
    std::string elemType;
    if (context.resolveVectorTarget(expr.args.front(), elemType)) {
      return ReturnKind::Int;
    }
  }

  return ReturnKind::Unknown;
}

} // namespace primec::semantics
