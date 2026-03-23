#include "SemanticsValidator.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprResolvedCallSetup(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    const Definition &resolvedDefinition,
    const std::vector<ParameterInfo> &calleeParams,
    bool hasMethodReceiverIndex,
    size_t methodReceiverIndex,
    ExprResolvedCallSetup &contextOut) {
  contextOut = {};
  contextOut.diagnosticResolved = diagnosticCallTargetPath(resolved);
  contextOut.argumentValidationContext.callExpr = &expr;
  contextOut.argumentValidationContext.resolved = &resolved;
  contextOut.argumentValidationContext.diagnosticResolved =
      &contextOut.diagnosticResolved;
  contextOut.argumentValidationContext.params = &params;
  contextOut.argumentValidationContext.locals = &locals;
  contextOut.argumentValidationContext.dispatchResolvers = &dispatchResolvers;

  const bool isResolvedStructConstructorCall =
      calleeParams.empty() && structNames_.count(resolved) > 0;
  if (isResolvedStructConstructorCall && expr.args.empty() &&
      !hasNamedArguments(expr.argNames) && !expr.hasBodyArguments &&
      expr.bodyArguments.empty()) {
    contextOut.resolvedStructConstructorZeroArgDiagnostic =
        experimentalGfxUnavailableConstructorDiagnostic(expr, resolved);
  }

  contextOut.resolvedStructConstructorContext = {
      .isResolvedStructConstructorCall = isResolvedStructConstructorCall,
      .resolvedDefinition = &resolvedDefinition,
      .argumentValidationContext = &contextOut.argumentValidationContext,
      .diagnosticResolved = &contextOut.diagnosticResolved,
      .zeroArgDiagnostic = &contextOut.resolvedStructConstructorZeroArgDiagnostic,
  };
  contextOut.resolvedCallArgumentContext = {
      .resolvedDefinition = &resolvedDefinition,
      .calleeParams = &calleeParams,
      .argumentValidationContext = &contextOut.argumentValidationContext,
      .diagnosticResolved = &contextOut.diagnosticResolved,
      .hasMethodReceiverIndex = hasMethodReceiverIndex,
      .methodReceiverIndex = methodReceiverIndex,
  };
}

} // namespace primec::semantics
