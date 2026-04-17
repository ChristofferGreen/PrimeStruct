#include "SemanticsValidator.h"

namespace primec::semantics {

ReturnKind SemanticsValidator::inferBuiltinCollectionDirectCountCapacityReturnKind(
    const Expr &expr,
    const std::string &resolved,
    const BuiltinCollectionDirectCountCapacityContext &context,
    bool &handled) {
  (void)resolved;
  handled = false;
  if (expr.isMethodCall || expr.args.empty()) {
    return ReturnKind::Unknown;
  }
  if (!context.isDirectCountCall && !context.isDirectCapacityCall) {
    return ReturnKind::Unknown;
  }

  handled = true;
  auto failInferDirectCountCapacityDiagnostic =
      [&](std::string message) -> ReturnKind {
    (void)failExprDiagnostic(expr, std::move(message));
    return ReturnKind::Unknown;
  };
  const std::string directCallPath = resolveCalleePath(expr);
  const auto inferHelperReturnKind = [&](const std::string &helperName,
                                         const Expr &receiverExpr,
                                         bool allowBuiltinFallback) -> ReturnKind {
    if (context.resolveMethodCallPath == nullptr || context.preferVectorStdlibHelperPath == nullptr) {
      return ReturnKind::Unknown;
    }
    auto hasVisibleStdlibMapCountDefinition = [&]() {
      return hasImportedDefinitionPath("/std/collections/map/count") ||
             (context.hasDeclaredDefinitionPath != nullptr &&
              context.hasDeclaredDefinitionPath("/std/collections/map/count"));
    };
    auto rejectsRootedVectorBuiltinAlias = [&](const std::string &methodPath) {
      if (context.resolveVectorTarget == nullptr ||
          methodPath != "/vector/" + helperName ||
          hasImportedDefinitionPath(methodPath) ||
          defMap_.find(methodPath) != defMap_.end()) {
        return false;
      }
      std::string elemType;
      return context.resolveVectorTarget(receiverExpr, elemType);
    };
    auto rejectsRootedVectorCountNonVectorAlias = [&](const std::string &methodPath) {
      if (helperName != "count" ||
          directCallPath != "/vector/count" ||
          hasImportedDefinitionPath("/vector/count") ||
          defMap_.find("/vector/count") != defMap_.end()) {
        return false;
      }
      return methodPath != "/vector/count";
    };
    std::string methodResolved;
    const std::string rootedHelperPath = "/vector/" + helperName;
    if (directCallPath == rootedHelperPath &&
        (hasImportedDefinitionPath(rootedHelperPath) ||
         defMap_.find(rootedHelperPath) != defMap_.end())) {
      methodResolved = rootedHelperPath;
    } else if (!context.resolveMethodCallPath(helperName, methodResolved)) {
      return ReturnKind::Unknown;
    }
    methodResolved = context.preferVectorStdlibHelperPath(methodResolved);
    if (helperName == "count" && methodResolved == "/std/collections/map/count" &&
        !hasVisibleStdlibMapCountDefinition() &&
        !context.shouldInferBuiltinBareMapCountCall) {
      return failInferDirectCountCapacityDiagnostic(
          "unknown call target: /std/collections/map/count");
    }
    if (rejectsRootedVectorBuiltinAlias(methodResolved)) {
      return failInferDirectCountCapacityDiagnostic(
          "unknown call target: " + methodResolved);
    }
    if (rejectsRootedVectorCountNonVectorAlias(methodResolved)) {
      return failInferDirectCountCapacityDiagnostic(
          "unknown call target: /vector/count");
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
          (context.resolveStringTarget != nullptr &&
           context.resolveStringTarget(expr.args.front()))) {
        return ReturnKind::Int;
      }
      if (context.shouldInferBuiltinBareMapCountCall &&
          context.resolveMapTarget != nullptr &&
          context.resolveMapTarget(expr.args.front(), keyType, valueType)) {
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
