#include "EmitterExprControlBodyWrapperStep.h"

#include <sstream>

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlBodyWrapperStep(
    const Expr &expr,
    const std::unordered_map<std::string, std::string> &nameMap,
    const EmitterExprControlBodyWrapperIsBuiltinBlockFn &isBuiltinBlock,
    const EmitterExprControlBodyWrapperEmitExprFn &emitExpr) {
  if (!(expr.hasBodyArguments || !expr.bodyArguments.empty())) {
    return std::nullopt;
  }
  if (!isBuiltinBlock || isBuiltinBlock(expr, nameMap)) {
    return std::nullopt;
  }
  if (!emitExpr) {
    return std::nullopt;
  }
  Expr callExpr = expr;
  callExpr.bodyArguments.clear();
  callExpr.hasBodyArguments = false;

  Expr blockExpr = expr;
  blockExpr.name = "block";
  blockExpr.args.clear();
  blockExpr.templateArgs.clear();
  blockExpr.argNames.clear();
  blockExpr.isMethodCall = false;
  blockExpr.isBinding = false;
  blockExpr.isLambda = false;
  blockExpr.hasBodyArguments = true;

  std::ostringstream out;
  out << "([&]() { ";
  out << "auto ps_call_value = " << emitExpr(callExpr) << "; ";
  out << "(void)" << emitExpr(blockExpr) << "; ";
  out << "return ps_call_value; }())";
  return out.str();
}

} // namespace primec::emitter
