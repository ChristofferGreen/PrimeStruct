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

bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     ResultExprInfo &out) {
  auto lookupLocal = [&](const std::string &name) -> LocalResultInfo {
    LocalResultInfo local;
    auto it = localsIn.find(name);
    if (it == localsIn.end()) {
      return local;
    }
    local.found = true;
    local.isResult = it->second.isResult;
    local.resultHasValue = it->second.resultHasValue;
    local.resultErrorType = it->second.resultErrorType;
    local.isFileHandle = it->second.isFileHandle;
    return local;
  };
  auto resolveMethod = [&](const Expr &callExpr) -> const Definition * {
    return resolveMethodCall(callExpr, localsIn);
  };
  auto lookupDefinitionResult = [&](const std::string &path, ResultExprInfo &resultOut) -> bool {
    ReturnInfo info;
    if (!lookupReturnInfo(path, info) || !info.isResult) {
      return false;
    }
    resultOut.isResult = true;
    resultOut.hasValue = info.resultHasValue;
    resultOut.errorType = info.resultErrorType;
    return true;
  };
  return resolveResultExprInfo(
      expr, lookupLocal, resolveMethod, resolveDefinitionCall, lookupDefinitionResult, out);
}

} // namespace primec::ir_lowerer
