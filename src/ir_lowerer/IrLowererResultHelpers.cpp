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

ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo) {
  return [resolveMethodCall, resolveDefinitionCall, lookupReturnInfo](
             const Expr &expr, const LocalMap &localsIn, ResultExprInfo &out) {
    return resolveResultExprInfoFromLocals(
        expr, localsIn, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, out);
  };
}

bool isSupportedResultWhyErrorKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
         kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::Bool;
}

std::string normalizeResultWhyErrorName(const std::string &errorType, LocalInfo::ValueKind errorKind) {
  if (errorType == "FileError") {
    return "FileError";
  }
  switch (errorKind) {
    case LocalInfo::ValueKind::Int32:
      return "i32";
    case LocalInfo::ValueKind::Int64:
      return "i64";
    case LocalInfo::ValueKind::UInt64:
      return "u64";
    case LocalInfo::ValueKind::Bool:
      return "bool";
    default:
      return errorType;
  }
}

void emitResultWhyErrorLocalFromResult(
    int32_t resultLocal,
    bool resultHasValue,
    int32_t errorLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal));
  if (resultHasValue) {
    emitInstruction(IrOpcode::PushI64, 4294967296ull);
    emitInstruction(IrOpcode::DivI64, 0);
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
}

bool emitResultWhyEmptyString(
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const int32_t index = internString("");
  emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(index));
  return true;
}

Expr makeResultWhyErrorValueExpr(int32_t errorLocal,
                                 LocalInfo::ValueKind valueKind,
                                 const std::string &namespacePrefix,
                                 int32_t tempOrdinal,
                                 LocalMap &callLocals) {
  std::string localName = "__result_why_err_" + std::to_string(tempOrdinal);
  LocalInfo info;
  info.index = errorLocal;
  info.isMutable = false;
  info.kind = LocalInfo::Kind::Value;
  info.valueKind = valueKind;
  callLocals.emplace(localName, info);

  Expr errorExpr;
  errorExpr.kind = Expr::Kind::Name;
  errorExpr.name = std::move(localName);
  errorExpr.namespacePrefix = namespacePrefix;
  return errorExpr;
}

Expr makeResultWhyBoolErrorExpr(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t tempOrdinal,
    LocalMap &callLocals,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const int32_t boolLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(boolLocal));

  std::string localName = "__result_why_bool_" + std::to_string(tempOrdinal);
  LocalInfo info;
  info.index = boolLocal;
  info.isMutable = false;
  info.kind = LocalInfo::Kind::Value;
  info.valueKind = LocalInfo::ValueKind::Bool;
  callLocals.emplace(localName, info);

  Expr errorExpr;
  errorExpr.kind = Expr::Kind::Name;
  errorExpr.name = std::move(localName);
  errorExpr.namespacePrefix = namespacePrefix;
  return errorExpr;
}

ResultWhyCallEmitResult emitResolvedResultWhyCall(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyCallOps &ops,
    std::string &error) {
  std::string errorStructPath;
  if (ops.resolveStructTypeName(resultInfo.errorType, expr.namespacePrefix, errorStructPath)) {
    const std::string whyPath = errorStructPath + "/why";
    auto whyIt = defMap.find(whyPath);
    if (whyIt != defMap.end() && whyIt->second && whyIt->second->parameters.size() == 1) {
      ReturnInfo whyReturn;
      if (!ops.getReturnInfo || !ops.getReturnInfo(whyIt->second->fullPath, whyReturn)) {
        return ResultWhyCallEmitResult::Error;
      }
      if (whyReturn.returnsVoid || whyReturn.returnsArray ||
          whyReturn.kind != LocalInfo::ValueKind::String) {
        error = "Result.why requires a string-returning why() for " + resultInfo.errorType;
        return ResultWhyCallEmitResult::Error;
      }
      const Expr &param = whyIt->second->parameters.front();
      LocalInfo::Kind paramKind = ops.bindingKind(param);
      std::string paramTypeName;
      std::vector<std::string> paramTemplateArgs;
      if (paramKind == LocalInfo::Kind::Value &&
          ops.extractFirstBindingTypeTransform(param, paramTypeName, paramTemplateArgs) &&
          paramTemplateArgs.empty()) {
        std::string paramStructPath;
        LocalMap callLocals = localsIn;
        if (ops.resolveStructTypeName(paramTypeName, param.namespacePrefix, paramStructPath) &&
            paramStructPath == errorStructPath) {
          StructSlotLayoutInfo layout;
          if (!ops.resolveStructSlotLayout(errorStructPath, layout)) {
            return ResultWhyCallEmitResult::Error;
          }
          if (layout.fields.size() != 1 || !layout.fields.front().structPath.empty() ||
              !isSupportedResultWhyErrorKind(layout.fields.front().valueKind)) {
            return ops.emitEmptyString() ? ResultWhyCallEmitResult::Emitted
                                         : ResultWhyCallEmitResult::Error;
          }
          Expr errorValueExpr = layout.fields.front().valueKind == LocalInfo::ValueKind::Bool
                                    ? ops.makeBoolErrorExpr(callLocals)
                                    : ops.makeErrorValueExpr(callLocals, layout.fields.front().valueKind);
          Expr ctorExpr;
          ctorExpr.kind = Expr::Kind::Call;
          ctorExpr.name = errorStructPath;
          ctorExpr.namespacePrefix = expr.namespacePrefix;
          ctorExpr.isMethodCall = false;
          ctorExpr.isBinding = false;
          ctorExpr.args.push_back(errorValueExpr);
          ctorExpr.argNames.push_back(std::nullopt);
          Expr callExpr;
          callExpr.kind = Expr::Kind::Call;
          callExpr.name = whyPath;
          callExpr.namespacePrefix = expr.namespacePrefix;
          callExpr.isMethodCall = false;
          callExpr.isBinding = false;
          callExpr.args.push_back(ctorExpr);
          callExpr.argNames.push_back(std::nullopt);
          return ops.emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals)
                     ? ResultWhyCallEmitResult::Emitted
                     : ResultWhyCallEmitResult::Error;
        }
        LocalInfo::ValueKind paramKindValue = ops.valueKindFromTypeName(paramTypeName);
        if (isSupportedResultWhyErrorKind(paramKindValue)) {
          Expr errorValueExpr = paramKindValue == LocalInfo::ValueKind::Bool
                                    ? ops.makeBoolErrorExpr(callLocals)
                                    : ops.makeErrorValueExpr(callLocals, paramKindValue);
          Expr callExpr;
          callExpr.kind = Expr::Kind::Call;
          callExpr.name = whyPath;
          callExpr.namespacePrefix = expr.namespacePrefix;
          callExpr.isMethodCall = false;
          callExpr.isBinding = false;
          callExpr.args.push_back(errorValueExpr);
          callExpr.argNames.push_back(std::nullopt);
          return ops.emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals)
                     ? ResultWhyCallEmitResult::Emitted
                     : ResultWhyCallEmitResult::Error;
        }
      }
    }
  }

  LocalInfo::ValueKind errorKind = ops.valueKindFromTypeName(resultInfo.errorType);
  if (isSupportedResultWhyErrorKind(errorKind)) {
    const std::string whyPath =
        "/" + normalizeResultWhyErrorName(resultInfo.errorType, errorKind) + "/why";
    auto whyIt = defMap.find(whyPath);
    if (whyIt != defMap.end() && whyIt->second && whyIt->second->parameters.size() == 1) {
      ReturnInfo whyReturn;
      if (!ops.getReturnInfo || !ops.getReturnInfo(whyIt->second->fullPath, whyReturn)) {
        return ResultWhyCallEmitResult::Error;
      }
      if (whyReturn.returnsVoid || whyReturn.returnsArray ||
          whyReturn.kind != LocalInfo::ValueKind::String) {
        error = "Result.why requires a string-returning why() for " + resultInfo.errorType;
        return ResultWhyCallEmitResult::Error;
      }
      LocalMap callLocals = localsIn;
      Expr errorValueExpr = errorKind == LocalInfo::ValueKind::Bool
                                ? ops.makeBoolErrorExpr(callLocals)
                                : ops.makeErrorValueExpr(callLocals, errorKind);
      Expr callExpr;
      callExpr.kind = Expr::Kind::Call;
      callExpr.name = whyPath;
      callExpr.namespacePrefix = expr.namespacePrefix;
      callExpr.isMethodCall = false;
      callExpr.isBinding = false;
      callExpr.args.push_back(errorValueExpr);
      callExpr.argNames.push_back(std::nullopt);
      return ops.emitInlineDefinitionCall(callExpr, *whyIt->second, callLocals)
                 ? ResultWhyCallEmitResult::Emitted
                 : ResultWhyCallEmitResult::Error;
    }
  }

  if (resultInfo.errorType == "FileError") {
    return ops.emitFileErrorWhy(errorLocal) ? ResultWhyCallEmitResult::Emitted
                                            : ResultWhyCallEmitResult::Error;
  }
  return ops.emitEmptyString() ? ResultWhyCallEmitResult::Emitted
                               : ResultWhyCallEmitResult::Error;
}

} // namespace primec::ir_lowerer
