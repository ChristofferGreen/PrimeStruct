#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::validateExprLateMapSoaBuiltins(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const std::string &resolved,
    bool resolvedMethod,
    const ExprLateMapSoaBuiltinContext &context,
    bool &handledOut) {
  handledOut = false;
  if (context.dispatchResolvers == nullptr ||
      context.resolveVectorTarget == nullptr ||
      context.resolveSoaVectorTarget == nullptr) {
    return true;
  }

  ExprMapSoaBuiltinContext mapSoaBuiltinContext;
  mapSoaBuiltinContext.shouldBuiltinValidateBareMapContainsCall =
      context.shouldBuiltinValidateBareMapContainsCall;
  mapSoaBuiltinContext.resolveMapKeyType =
      [&](const Expr &target, std::string &mapKeyTypeOut) {
        return this->resolveMapKeyType(target, *context.dispatchResolvers,
                                       mapKeyTypeOut);
      };
  mapSoaBuiltinContext.resolveVectorTarget =
      [&](const Expr &target, std::string &elemTypeOut) {
        return context.resolveVectorTarget(target, elemTypeOut);
      };
  mapSoaBuiltinContext.resolveSoaVectorTarget =
      [&](const Expr &target, std::string &elemTypeOut) {
        return context.resolveSoaVectorTarget(target, elemTypeOut);
      };
  mapSoaBuiltinContext.resolveStringTarget = [&](const Expr &target) {
    return this->isStringExprForArgumentValidation(target,
                                                   *context.dispatchResolvers);
  };
  mapSoaBuiltinContext.bareMapHelperOperandIndices =
      [&](const Expr &target, size_t &receiverIndexOut,
          size_t &keyIndexOut) {
        return this->bareMapHelperOperandIndices(target,
                                                 *context.dispatchResolvers,
                                                 receiverIndexOut,
                                                 keyIndexOut);
      };
  mapSoaBuiltinContext.isNamedArgsPackMethodAccessCall =
      [&](const Expr &target) {
        return this->isNamedArgsPackMethodAccessCall(target,
                                                     *context.dispatchResolvers);
      };
  mapSoaBuiltinContext.isNamedArgsPackWrappedFileBuiltinAccessCall =
      [&](const Expr &target) {
        return this->isNamedArgsPackWrappedFileBuiltinAccessCall(
            target, *context.dispatchResolvers);
      };
  return validateExprMapSoaBuiltins(params, locals, expr, resolved,
                                    resolvedMethod, mapSoaBuiltinContext,
                                    handledOut);
}

} // namespace primec::semantics
