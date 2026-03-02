#include "IrLowererResultHelpers.h"

#include <utility>

namespace primec::ir_lowerer {

bool resolveResultExprInfo(const Expr &expr,
                           const LookupLocalResultInfoFn &lookupLocal,
                           const ResolveCallDefinitionFn &resolveMethodCall,
                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                           const LookupDefinitionResultInfoFn &lookupDefinitionResult,
                           ResultExprInfo &out) {
  out = ResultExprInfo{};
  if (expr.kind == Expr::Kind::Name) {
    const LocalResultInfo local = lookupLocal(expr.name);
    if (local.found && local.isResult) {
      out.isResult = true;
      out.hasValue = local.resultHasValue;
      out.errorType = local.resultErrorType;
      return true;
    }
    return false;
  }

  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  if (!expr.isMethodCall && expr.name == "File") {
    out.isResult = true;
    out.hasValue = true;
    out.errorType = "FileError";
    return true;
  }

  if (expr.isMethodCall && !expr.args.empty()) {
    if (expr.args.front().kind == Expr::Kind::Name) {
      if (expr.args.front().name == "Result" && expr.name == "ok") {
        out.isResult = true;
        out.hasValue = (expr.args.size() > 1);
        return true;
      }
      const LocalResultInfo local = lookupLocal(expr.args.front().name);
      if (local.found && local.isFileHandle) {
        if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "write_bytes" ||
            expr.name == "flush" || expr.name == "close") {
          out.isResult = true;
          out.hasValue = false;
          out.errorType = "FileError";
          return true;
        }
      }
    }
    const Definition *callee = resolveMethodCall(expr);
    if (callee) {
      ResultExprInfo calleeInfo;
      if (lookupDefinitionResult(callee->fullPath, calleeInfo) && calleeInfo.isResult) {
        out = std::move(calleeInfo);
        return true;
      }
    }
    return false;
  }

  const Definition *callee = resolveDefinitionCall(expr);
  if (!callee) {
    return false;
  }

  ResultExprInfo calleeInfo;
  if (lookupDefinitionResult(callee->fullPath, calleeInfo) && calleeInfo.isResult) {
    out = std::move(calleeInfo);
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
