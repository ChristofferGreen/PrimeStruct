#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprLateBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprLateBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  if (defMap_.find(resolved) != defMap_.end() && !resolvedMethod) {
    return true;
  }
  bool handledTryBuiltin = false;
  if (!validateExprTryBuiltin(params, locals, expr, context.tryBuiltinContext,
                              handledTryBuiltin)) {
    return false;
  }
  if (handledTryBuiltin) {
    handledOut = true;
    return true;
  }

  bool handledResultFileBuiltin = false;
  if (!validateExprResultFileBuiltins(params, locals, expr, resolved,
                                      resolvedMethod,
                                      context.resultFileBuiltinContext,
                                      handledResultFileBuiltin)) {
    return false;
  }
  if (handledResultFileBuiltin) {
    handledOut = true;
    return true;
  }

  return validateExprGpuBufferBuiltins(params, locals, expr, handledOut);
}

} // namespace primec::semantics
