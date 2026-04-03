#include "SemanticsValidator.h"

#include <string>
#include <utility>

namespace primec::semantics {

bool SemanticsValidator::validateExprLateFallbackBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprLateFallbackBuiltinContext &context,
    bool &handledOut) {
  auto publishLateFallbackBuiltinDiagnostic = [&]() -> bool {
    captureExprContext(expr);
    return publishCurrentStructuredDiagnosticNow();
  };
  auto failLateFallbackBuiltinDiagnostic = [&](std::string message) -> bool {
    error_ = std::move(message);
    return publishLateFallbackBuiltinDiagnostic();
  };
  handledOut = false;
  if (context.dispatchResolvers == nullptr) {
    return true;
  }

  bool handledNumericBuiltin = false;
  if (!validateNumericBuiltinExpr(params, locals, expr, handledNumericBuiltin)) {
    return false;
  }
  if (handledNumericBuiltin) {
    handledOut = true;
    return true;
  }

  bool handledCollectionAccessFallback = false;
  if (!validateExprLateCollectionAccessFallbacks(
          params, locals, expr, resolved, resolvedMethod,
          context.collectionAccessFallbackContext,
          handledCollectionAccessFallback)) {
    return false;
  }
  if (handledCollectionAccessFallback) {
    handledOut = true;
    return true;
  }

  std::string builtinName;
  if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
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

  bool handledScalarPointerMemoryBuiltin = false;
  if (!validateExprScalarPointerMemoryBuiltins(
          params, locals, expr, handledScalarPointerMemoryBuiltin)) {
    return false;
  }
  if (handledScalarPointerMemoryBuiltin) {
    handledOut = true;
    return true;
  }

  bool handledCollectionLiteralBuiltin = false;
  if (!validateExprCollectionLiteralBuiltins(
          params, locals, expr, handledCollectionLiteralBuiltin)) {
    return false;
  }
  if (handledCollectionLiteralBuiltin) {
    handledOut = true;
    return true;
  }

  if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinName) &&
      expr.args.size() == 2) {
    const size_t receiverIndex =
        this->mapHelperReceiverIndex(expr, *context.dispatchResolvers);
    if (receiverIndex < expr.args.size()) {
      const Expr &receiverExpr = expr.args[receiverIndex];
      std::string elemType;
      std::string keyType;
      std::string valueType;
      const bool hasCollectionReceiver =
          context.dispatchResolvers->resolveArgsPackAccessTarget(
              receiverExpr, elemType) ||
          context.dispatchResolvers->resolveVectorTarget(receiverExpr,
                                                        elemType) ||
          context.dispatchResolvers->resolveArrayTarget(receiverExpr,
                                                       elemType) ||
          context.dispatchResolvers->resolveStringTarget(receiverExpr) ||
          context.dispatchResolvers->resolveMapTarget(receiverExpr, keyType,
                                                     valueType) ||
          context.dispatchResolvers->resolveExperimentalMapTarget(
              receiverExpr, keyType, valueType);
      if (!hasCollectionReceiver) {
        bool isBuiltinMethod = false;
        std::string methodResolved;
        if (resolveMethodTarget(params, locals, expr.namespacePrefix,
                                receiverExpr, builtinName, methodResolved,
                                isBuiltinMethod) &&
            !methodResolved.empty()) {
          return failLateFallbackBuiltinDiagnostic("unknown method: " +
                                                   methodResolved);
        } else {
          return failLateFallbackBuiltinDiagnostic(
              builtinName + " requires array, vector, map, or string target");
        }
      }
    }
  }

  return true;
}

} // namespace primec::semantics
