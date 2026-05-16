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
  if ((!context.isCountLike && !context.isCapacityLike) ||
      context.resolveMethodCallPath == nullptr ||
      context.preferVectorStdlibHelperPath == nullptr) {
    return false;
  }

  std::string methodResolved;
  if (!context.resolveMethodCallPath(context.isCountLike ? context.countHelperName : "capacity",
                                     methodResolved)) {
    return false;
  }
  methodResolved = context.preferVectorStdlibHelperPath(methodResolved);

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
