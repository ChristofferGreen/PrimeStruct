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
  const std::string directCallPath = resolveCalleePath(expr);
  const auto inferHelperReturnKind = [&](const std::string &helperName,
                                         const Expr &receiverExpr,
                                         bool allowBuiltinFallback) -> ReturnKind {
    if (context.resolveMethodCallPath == nullptr || context.preferVectorStdlibHelperPath == nullptr) {
      return ReturnKind::Unknown;
    }
    std::string methodResolved;
    const std::string rootedHelperPath = rootedVectorHelperPath(helperName);
    if (directCallPath == rootedHelperPath &&
        (hasImportedDefinitionPath(rootedHelperPath) ||
         defMap_.find(rootedHelperPath) != defMap_.end())) {
      methodResolved = rootedHelperPath;
    } else if (!context.resolveMethodCallPath(helperName, methodResolved)) {
      return ReturnKind::Unknown;
    }
    methodResolved = context.preferVectorStdlibHelperPath(methodResolved);
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
    const ReturnKind helperReturnKind =
        inferHelperReturnKind(context.countHelperName, expr.args.front(),
                              context.isDirectCountSingleArg);
    if (helperReturnKind != ReturnKind::Unknown) {
      return helperReturnKind;
    }

    if (context.isDirectCountSingleArg) {
      std::string elemType;
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
