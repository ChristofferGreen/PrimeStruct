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
    if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
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

} // namespace primec::semantics
