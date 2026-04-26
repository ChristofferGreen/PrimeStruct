#include "IrLowererResultInternal.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererLowerInferenceBaseKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isResultBuiltinCall(const Expr &expr, const std::string &name, size_t argCount) {
  return expr.kind == Expr::Kind::Call && expr.name == name && expr.args.size() == argCount &&
         !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result";
}

void assignSemanticResultError(std::string *errorOut, const std::string &message) {
  if (errorOut != nullptr && errorOut->empty()) {
    *errorOut = message;
  }
}

std::string describeSemanticResultCall(const Expr &expr) {
  return expr.name.empty() ? "<call>" : expr.name;
}

std::string resolveScopedExprPath(const Expr &expr) {
  if (expr.name.empty()) {
    return {};
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return expr.name;
}

bool isSemanticFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string args;
  return splitTemplateTypeName(trimTemplateTypeText(typeText), base, args) &&
         normalizeCollectionBindingTypeName(base) == "File";
}

bool applySemanticResultValueTypeText(const std::string &valueTypeText, ResultExprInfo &out) {
  const std::string trimmedValueType = trimTemplateTypeText(valueTypeText);
  if (trimmedValueType.empty()) {
    return false;
  }
  out.valueCollectionKind = LocalInfo::Kind::Value;
  out.valueKind = LocalInfo::ValueKind::Unknown;
  out.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
  out.valueIsFileHandle = false;
  out.valueStructType.clear();
  if (resolveSupportedResultCollectionType(
          trimmedValueType, out.valueCollectionKind, out.valueKind, &out.valueMapKeyKind)) {
    return true;
  }
  if (isSemanticFileHandleTypeText(trimmedValueType)) {
    out.valueKind = LocalInfo::ValueKind::Int64;
    out.valueIsFileHandle = true;
    return true;
  }
  out.valueKind = valueKindFromTypeName(trimmedValueType);
  if (out.valueKind != LocalInfo::ValueKind::Unknown) {
    return true;
  }
  out.valueStructType = trimmedValueType;
  if (!out.valueStructType.empty() && out.valueStructType.front() != '/') {
    out.valueStructType.insert(out.valueStructType.begin(), '/');
  }
  return true;
}

bool applySemanticQueryFactResultInfo(const Expr &expr,
                                      const SemanticProgram *semanticProgram,
                                      const SemanticProductIndex *semanticIndex,
                                      ResultExprInfo &out) {
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
  if (queryFact == nullptr || !queryFact->hasResultType) {
    return false;
  }
  out.isResult = true;
  out.hasValue = queryFact->resultTypeHasValue;
  out.errorType = queryFact->resultErrorType;
  if (!out.hasValue) {
    return true;
  }
  return applySemanticResultValueTypeText(queryFact->resultValueType, out);
}

} // namespace

bool validateSemanticProductResultMetadataCompleteness(const SemanticProgram *semanticProgram,
                                                       std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const auto callableSummaries = semanticProgramCallableSummaryView(*semanticProgram);
  for (const auto *summary : callableSummaries) {
    const std::string_view callablePath =
        semanticProgramCallableSummaryFullPath(*semanticProgram, *summary);
    if (summary->fullPathId == InvalidSymbolId || callablePath.empty()) {
      error = "missing semantic-product callable summary path id";
      return false;
    }
    if (summary->hasResultType && summary->resultTypeHasValue) {
      ResultExprInfo resultInfo;
      if (!applySemanticResultValueTypeText(summary->resultValueType, resultInfo)) {
        error = "missing semantic-product callable result metadata: " +
                std::string(callablePath);
        return false;
      }
    }
  }

  const auto returnFacts = semanticProgramReturnFactView(*semanticProgram);
  for (const auto *returnFact : returnFacts) {
    const std::string_view definitionPath =
        semanticProgramReturnFactDefinitionPath(*semanticProgram, *returnFact);
    if (returnFact->definitionPathId == InvalidSymbolId || definitionPath.empty()) {
      error = "missing semantic-product return definition path id";
      return false;
    }
    if (returnFact->bindingTypeText.empty()) {
      error = "missing semantic-product return binding type: " +
              std::string(definitionPath);
      return false;
    }
  }

  const auto queryFacts = semanticProgramQueryFactView(*semanticProgram);
  for (const auto *queryFact : queryFacts) {
    const std::string_view resolvedPath =
        semanticProgramQueryFactResolvedPath(*semanticProgram, *queryFact);
    const std::string_view queryCallNameView =
        queryFact->callNameId != InvalidSymbolId
            ? semanticProgramResolveCallTargetString(*semanticProgram, queryFact->callNameId)
            : std::string_view(queryFact->callName);
    const std::string queryCallName =
        queryCallNameView.empty() ? "<call>" : std::string(queryCallNameView);
    if (queryFact->resolvedPathId == InvalidSymbolId || resolvedPath.empty()) {
      error = "missing semantic-product query resolved path id: " + queryCallName;
      return false;
    }
    if (queryFact->hasResultType && queryFact->resultTypeHasValue) {
      ResultExprInfo resultInfo;
      if (!applySemanticResultValueTypeText(queryFact->resultValueType, resultInfo)) {
        error = "incomplete semantic-product query fact: " + queryCallName;
        return false;
      }
    }
  }

  const auto tryFacts = semanticProgramTryFactView(*semanticProgram);
  for (const auto *tryFact : tryFacts) {
    const std::string_view operandResolvedPath =
        semanticProgramTryFactOperandResolvedPath(*semanticProgram, *tryFact);
    if (tryFact->operandResolvedPathId == InvalidSymbolId || operandResolvedPath.empty()) {
      error = "missing semantic-product try operand resolved path id: try";
      return false;
    }
    if (trimTemplateTypeText(tryFact->valueType).empty()) {
      error = "incomplete semantic-product try fact: try";
      return false;
    }
  }

  return true;
}

bool resolveResultExprInfo(const Expr &expr,
                           const LookupLocalResultInfoFn &lookupLocal,
                           const ResolveCallDefinitionFn &resolveMethodCall,
                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                           const LookupDefinitionResultInfoFn &lookupDefinitionResult,
                           ResultExprInfo &out) {
  out = ResultExprInfo{};
  auto applyDeclaredResultFromDefinition = [&](const Definition &definition) {
    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string declaredType = trimTemplateTypeText(transform.templateArgs.front());
      bool hasValue = false;
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      std::string errorType;
      if (!parseResultTypeName(declaredType, hasValue, valueKind, errorType)) {
        continue;
      }
      out.isResult = true;
      out.hasValue = hasValue;
      out.valueKind = valueKind;
      out.errorType = errorType;
      if (!hasValue || valueKind != LocalInfo::ValueKind::Unknown) {
        return true;
      }

      std::string base;
      std::string argText;
      std::vector<std::string> resultArgs;
      if (!splitTemplateTypeName(declaredType, base, argText) ||
          !splitTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
        return true;
      }

      out.valueCollectionKind = LocalInfo::Kind::Value;
      out.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
      const std::string valueTypeText = trimTemplateTypeText(resultArgs.front());
      if (!resolveSupportedResultCollectionType(
              valueTypeText, out.valueCollectionKind, out.valueKind, &out.valueMapKeyKind)) {
        out.valueStructType = valueTypeText;
      }
      return true;
    }
    return false;
  };
  if (expr.kind == Expr::Kind::Name) {
    const LocalResultInfo local = lookupLocal(expr.name);
    if (local.found && local.isResult) {
      out.isResult = true;
      out.hasValue = local.resultHasValue;
      out.valueKind = local.resultValueKind;
      out.valueCollectionKind = local.resultValueCollectionKind;
      out.valueMapKeyKind = local.resultValueMapKeyKind;
      out.valueIsFileHandle = local.resultValueIsFileHandle;
      out.valueStructType = local.resultValueStructType;
      out.errorType = local.resultErrorType;
      return true;
    }
    return false;
  }

  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  if (!expr.isMethodCall) {
    const std::string directPath = resolveScopedExprPath(expr);
    if (directPath == "File" || directPath == "/std/file/File") {
      out.isResult = true;
      out.hasValue = true;
      out.valueKind = LocalInfo::ValueKind::Int64;
      out.valueIsFileHandle = true;
      out.errorType = "FileError";
      return true;
    }
  }

  if (!expr.args.empty()) {
    if (expr.args.front().kind == Expr::Kind::Name) {
      if (expr.args.front().name == "Result" && expr.name == "ok") {
        out.isResult = true;
        out.hasValue = (expr.args.size() > 1);
        if (out.hasValue && expr.args.size() == 2) {
          const Definition *valueDef = resolveDefinitionCall ? resolveDefinitionCall(expr.args[1]) : nullptr;
          if (valueDef != nullptr && isStructDefinition(*valueDef)) {
            out.valueStructType = valueDef->fullPath;
          }
        }
        return true;
      }
    }
  }

  if (expr.isMethodCall && !expr.args.empty()) {
    if (expr.args.front().kind == Expr::Kind::Name) {
      const LocalResultInfo local = lookupLocal(expr.args.front().name);
      if (local.found && local.isFileHandle) {
        if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
            expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
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
      if (applyDeclaredResultFromDefinition(*callee)) {
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
  return applyDeclaredResultFromDefinition(*callee);
}

bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     const InferExprKindWithLocalsFn &inferExprKind,
                                     ResultExprInfo &out,
                                     const SemanticProgram *semanticProgram,
                                     const SemanticProductIndex *semanticIndex,
                                     std::string *errorOut) {
  out = ResultExprInfo{};
  const ResolveMethodCallWithLocalsFn noopResolveMethodCall =
      [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const ResolveCallDefinitionFn noopResolveDefinitionCall =
      [](const Expr &) -> const Definition * { return nullptr; };
  const LookupReturnInfoFn noopLookupReturnInfo =
      [](const std::string &, ReturnInfo &) { return false; };
  const InferExprKindWithLocalsFn noopInferExprKind =
      [](const Expr &, const LocalMap &) { return LocalInfo::ValueKind::Unknown; };
  const ResolveMethodCallWithLocalsFn &resolveMethodCallFn =
      resolveMethodCall ? resolveMethodCall : noopResolveMethodCall;
  const ResolveCallDefinitionFn &resolveDefinitionCallFn =
      resolveDefinitionCall ? resolveDefinitionCall : noopResolveDefinitionCall;
  const LookupReturnInfoFn &lookupReturnInfoFn =
      lookupReturnInfo ? lookupReturnInfo : noopLookupReturnInfo;
  const InferExprKindWithLocalsFn &inferExprKindFn =
      inferExprKind ? inferExprKind : noopInferExprKind;
  auto isIndexedArgsPackFileHandleReceiver = [&](const Expr &receiverExpr) {
    std::string accessName;
    if (receiverExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(receiverExpr, accessName) ||
        receiverExpr.args.size() != 2 || receiverExpr.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(receiverExpr.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
           (it->second.argsPackElementKind == LocalInfo::Kind::Value ||
            it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
            it->second.argsPackElementKind == LocalInfo::Kind::Pointer);
  };
  auto isIndexedBorrowedArgsPackFileHandleReceiver = [&](const Expr &receiverExpr) {
    if (!(receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
          receiverExpr.args.size() == 1)) {
      return false;
    }
    std::string accessName;
    const Expr &targetExpr = receiverExpr.args.front();
    if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
        targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(targetExpr.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
           it->second.argsPackElementKind == LocalInfo::Kind::Reference;
  };
  auto isIndexedPointerArgsPackFileHandleReceiver = [&](const Expr &receiverExpr) {
    if (!(receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
          receiverExpr.args.size() == 1)) {
      return false;
    }
    std::string accessName;
    const Expr &targetExpr = receiverExpr.args.front();
    if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
        targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(targetExpr.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
           it->second.argsPackElementKind == LocalInfo::Kind::Pointer;
  };

  auto lookupLocal = [&](const std::string &name) -> LocalResultInfo {
    LocalResultInfo local;
    auto it = localsIn.find(name);
    if (it == localsIn.end()) {
      return local;
    }
    local.found = true;
    local.isResult = it->second.isResult;
    local.resultHasValue = it->second.resultHasValue;
    local.resultValueKind = it->second.resultValueKind;
    local.resultValueCollectionKind = it->second.resultValueCollectionKind;
    local.resultValueMapKeyKind = it->second.resultValueMapKeyKind;
    local.resultValueIsFileHandle = it->second.resultValueIsFileHandle;
    local.resultValueStructType = it->second.resultValueStructType;
    local.resultErrorType = it->second.resultErrorType;
    local.isFileHandle = it->second.isFileHandle;
    return local;
  };
  auto resolveMethod = [&](const Expr &callExpr) -> const Definition * {
    return resolveMethodCallFn(callExpr, localsIn);
  };
  auto lookupDefinitionResult = [&](const std::string &path, ResultExprInfo &resultOut) -> bool {
    ReturnInfo info;
    if (!lookupReturnInfoFn(path, info) || !info.isResult) {
      return false;
    }
    resultOut.isResult = true;
    resultOut.hasValue = info.resultHasValue;
    resultOut.valueKind = info.resultValueKind;
    resultOut.valueCollectionKind = info.resultValueCollectionKind;
    resultOut.valueMapKeyKind = info.resultValueMapKeyKind;
    resultOut.valueIsFileHandle = info.resultValueIsFileHandle;
    resultOut.valueStructType = info.resultValueStructType;
    resultOut.errorType = info.resultErrorType;
    return true;
  };
  auto inferCallMapTargetInfo = [&](const Expr &targetExpr, MapAccessTargetInfo &targetInfoOut) {
    targetInfoOut = {};
    const Definition *callee = resolveDefinitionCallFn(targetExpr);
    if (callee == nullptr) {
      return false;
    }
    std::string collectionName;
    std::vector<std::string> collectionArgs;
    if (!inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
      return false;
    }
    if (collectionName != "map" || collectionArgs.size() != 2) {
      return false;
    }
    targetInfoOut.isMapTarget = true;
    targetInfoOut.mapKeyKind = valueKindFromTypeName(collectionArgs[0]);
    targetInfoOut.mapValueKind = valueKindFromTypeName(collectionArgs[1]);
    return true;
  };
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
      isIndexedArgsPackFileHandleReceiver(expr.args.front())) {
    if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
        expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = "FileError";
      return true;
    }
  }
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
      isIndexedBorrowedArgsPackFileHandleReceiver(expr.args.front())) {
    if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
        expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = "FileError";
      return true;
    }
  }
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
      isIndexedPointerArgsPackFileHandleReceiver(expr.args.front())) {
    if (expr.name == "write" || expr.name == "write_line" || expr.name == "write_byte" || expr.name == "read_byte" ||
        expr.name == "write_bytes" || expr.name == "flush" || expr.name == "close") {
      out.isResult = true;
      out.hasValue = false;
      out.errorType = "FileError";
      return true;
    }
  }
  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2 && expr.args.front().kind == Expr::Kind::Name) {
    const LocalResultInfo local = lookupLocal(expr.args.front().name);
    auto localIt = localsIn.find(expr.args.front().name);
    if (local.found && local.isResult && localIt != localsIn.end() && localIt->second.isArgsPack) {
      out.isResult = true;
      out.hasValue = local.resultHasValue;
      out.valueKind = local.resultValueKind;
      out.valueCollectionKind = local.resultValueCollectionKind;
      out.valueMapKeyKind = local.resultValueMapKeyKind;
      out.valueIsFileHandle = local.resultValueIsFileHandle;
      out.valueStructType = local.resultValueStructType;
      out.errorType = local.resultErrorType;
      return true;
    }
  }
  if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
    const Expr &targetExpr = expr.args.front();
    if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, accessName) &&
        targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
      const LocalResultInfo local = lookupLocal(targetExpr.args.front().name);
      auto localIt = localsIn.find(targetExpr.args.front().name);
      if (local.found && local.isResult && localIt != localsIn.end() && localIt->second.isArgsPack &&
          (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference ||
           localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
        out.isResult = true;
        out.hasValue = local.resultHasValue;
        out.valueKind = local.resultValueKind;
        out.valueCollectionKind = local.resultValueCollectionKind;
        out.valueMapKeyKind = local.resultValueMapKeyKind;
        out.valueIsFileHandle = local.resultValueIsFileHandle;
        out.valueStructType = local.resultValueStructType;
        out.errorType = local.resultErrorType;
        return true;
      }
    }
  }
  if (isIfCall(expr) && expr.args.size() == 3) {
    auto resolveBranchResultInfo = [&](const Expr &branchExpr, ResultExprInfo &branchOut) {
      if (isInlineBodyValueEnvelope(branchExpr, resolveDefinitionCallFn)) {
        return resolveBodyResultExprInfo(
            branchExpr.bodyArguments,
            localsIn,
            resolveMethodCallFn,
            resolveDefinitionCallFn,
            lookupReturnInfoFn,
            inferExprKindFn,
            branchOut,
            semanticProgram,
            semanticIndex,
            errorOut);
      }
      return resolveResultExprInfoFromLocals(
          branchExpr,
          localsIn,
          resolveMethodCallFn,
          resolveDefinitionCallFn,
          lookupReturnInfoFn,
          inferExprKindFn,
          branchOut,
          semanticProgram,
          semanticIndex,
          errorOut);
    };

    ResultExprInfo thenResultInfo;
    ResultExprInfo elseResultInfo;
    if (!resolveBranchResultInfo(expr.args[1], thenResultInfo) || !resolveBranchResultInfo(expr.args[2], elseResultInfo)) {
      return false;
    }
    return mergeControlFlowResultInfos(thenResultInfo, elseResultInfo, out);
  }
  if (isInlineBodyBlockEnvelope(expr, resolveDefinitionCallFn)) {
    return resolveBodyResultExprInfo(
        expr.bodyArguments,
        localsIn,
        resolveMethodCallFn,
        resolveDefinitionCallFn,
        lookupReturnInfoFn,
        inferExprKindFn,
        out,
        semanticProgram,
        semanticIndex,
        errorOut);
  }
  if (expr.kind == Expr::Kind::Call && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result" && expr.name == "ok") {
    out.isResult = true;
    out.hasValue = (expr.args.size() > 1);
    if (out.hasValue && expr.args.size() == 2) {
      applyDirectResultValueMetadata(
          expr.args[1], localsIn, resolveDefinitionCallFn, inferExprKindFn, semanticProgram, semanticIndex, out);
    }
    return true;
  }
  if (isResultBuiltinCall(expr, "map", 3)) {
    ResultExprInfo sourceResultInfo;
    if (!resolveResultExprInfoFromLocals(expr.args[1],
                                         localsIn,
                                         resolveMethodCallFn,
                                         resolveDefinitionCallFn,
                                         lookupReturnInfoFn,
                                         inferExprKindFn,
                                         sourceResultInfo,
                                         semanticProgram,
                                         semanticIndex,
                                         errorOut) ||
        !sourceResultInfo.isResult || !sourceResultInfo.hasValue || !expr.args[2].isLambda || expr.args[2].args.size() != 1) {
      return false;
    }

    LocalMap lambdaLocals = localsIn;
    LocalInfo paramInfo;
    applyResultValueInfoToLocal(sourceResultInfo, paramInfo);
    lambdaLocals[expr.args[2].args.front().name] = paramInfo;

    const Expr *mappedValueExpr = nullptr;
    if (!resolveResultLambdaValueExprForMetadata(
            expr.args[2],
            lambdaLocals,
            resolveMethodCallFn,
            resolveDefinitionCallFn,
            lookupReturnInfoFn,
            inferExprKindFn,
            mappedValueExpr,
            semanticProgram,
            semanticIndex,
            errorOut)) {
      return false;
    }

    out.isResult = true;
    out.hasValue = true;
    applyDirectResultValueMetadata(
        *mappedValueExpr, lambdaLocals, resolveDefinitionCallFn, inferExprKindFn, semanticProgram, semanticIndex, out);
    if (out.valueCollectionKind == LocalInfo::Kind::Value &&
        out.valueMapKeyKind == LocalInfo::ValueKind::Unknown &&
        out.valueStructType.empty() &&
        !out.valueIsFileHandle) {
      applySemanticQueryFactResultInfo(expr, semanticProgram, semanticIndex, out);
    }
    out.errorType = sourceResultInfo.errorType;
    return true;
  }
  if (isResultBuiltinCall(expr, "and_then", 3)) {
    ResultExprInfo sourceResultInfo;
    if (!resolveResultExprInfoFromLocals(expr.args[1],
                                         localsIn,
                                         resolveMethodCallFn,
                                         resolveDefinitionCallFn,
                                         lookupReturnInfoFn,
                                         inferExprKindFn,
                                         sourceResultInfo,
                                         semanticProgram,
                                         semanticIndex,
                                         errorOut) ||
        !sourceResultInfo.isResult || !sourceResultInfo.hasValue || !expr.args[2].isLambda || expr.args[2].args.size() != 1) {
      return false;
    }

    LocalMap lambdaLocals = localsIn;
    LocalInfo paramInfo;
    applyResultValueInfoToLocal(sourceResultInfo, paramInfo);
    lambdaLocals[expr.args[2].args.front().name] = paramInfo;

    const Expr *chainedResultExpr = nullptr;
    if (!resolveResultLambdaValueExprForMetadata(
            expr.args[2],
            lambdaLocals,
            resolveMethodCallFn,
            resolveDefinitionCallFn,
            lookupReturnInfoFn,
            inferExprKindFn,
            chainedResultExpr,
            semanticProgram,
            semanticIndex,
            errorOut)) {
      return false;
    }

    ResultExprInfo chainedResultInfo;
    if (!resolveResultExprInfoFromLocals(*chainedResultExpr,
                                         lambdaLocals,
                                         resolveMethodCallFn,
                                         resolveDefinitionCallFn,
                                         lookupReturnInfoFn,
                                         inferExprKindFn,
                                         chainedResultInfo,
                                         semanticProgram,
                                         semanticIndex,
                                         errorOut) ||
        !chainedResultInfo.isResult) {
      return false;
    }
    if (chainedResultInfo.errorType.empty()) {
      chainedResultInfo.errorType = sourceResultInfo.errorType;
    }
    if (chainedResultInfo.hasValue &&
        chainedResultInfo.valueCollectionKind == LocalInfo::Kind::Value &&
        chainedResultInfo.valueMapKeyKind == LocalInfo::ValueKind::Unknown &&
        chainedResultInfo.valueStructType.empty() &&
        !chainedResultInfo.valueIsFileHandle) {
      applySemanticQueryFactResultInfo(expr, semanticProgram, semanticIndex, chainedResultInfo);
      if (chainedResultInfo.errorType.empty()) {
        chainedResultInfo.errorType = sourceResultInfo.errorType;
      }
    }
    out = std::move(chainedResultInfo);
    return true;
  }
  if (isResultBuiltinCall(expr, "map2", 4)) {
    ResultExprInfo leftResultInfo;
    ResultExprInfo rightResultInfo;
    if (!resolveResultExprInfoFromLocals(expr.args[1],
                                         localsIn,
                                         resolveMethodCallFn,
                                         resolveDefinitionCallFn,
                                         lookupReturnInfoFn,
                                         inferExprKindFn,
                                         leftResultInfo,
                                         semanticProgram,
                                         semanticIndex,
                                         errorOut) ||
        !resolveResultExprInfoFromLocals(expr.args[2],
                                         localsIn,
                                         resolveMethodCallFn,
                                         resolveDefinitionCallFn,
                                         lookupReturnInfoFn,
                                         inferExprKindFn,
                                         rightResultInfo,
                                         semanticProgram,
                                         semanticIndex,
                                         errorOut) ||
        !leftResultInfo.isResult || !leftResultInfo.hasValue || !rightResultInfo.isResult || !rightResultInfo.hasValue ||
        !expr.args[3].isLambda || expr.args[3].args.size() != 2) {
      return false;
    }

    if (!leftResultInfo.errorType.empty() && !rightResultInfo.errorType.empty() &&
        leftResultInfo.errorType != rightResultInfo.errorType) {
      return false;
    }

    LocalMap lambdaLocals = localsIn;
    LocalInfo leftParamInfo;
    applyResultValueInfoToLocal(leftResultInfo, leftParamInfo);
    lambdaLocals[expr.args[3].args.front().name] = leftParamInfo;

    LocalInfo rightParamInfo;
    applyResultValueInfoToLocal(rightResultInfo, rightParamInfo);
    lambdaLocals[expr.args[3].args[1].name] = rightParamInfo;

    const Expr *mappedValueExpr = nullptr;
    if (!resolveResultLambdaValueExprForMetadata(
            expr.args[3],
            lambdaLocals,
            resolveMethodCallFn,
            resolveDefinitionCallFn,
            lookupReturnInfoFn,
            inferExprKindFn,
            mappedValueExpr,
            semanticProgram,
            semanticIndex,
            errorOut)) {
      return false;
    }

    out.isResult = true;
    out.hasValue = true;
    applyDirectResultValueMetadata(
        *mappedValueExpr, lambdaLocals, resolveDefinitionCallFn, inferExprKindFn, semanticProgram, semanticIndex, out);
    if (out.valueCollectionKind == LocalInfo::Kind::Value &&
        out.valueMapKeyKind == LocalInfo::ValueKind::Unknown &&
        out.valueStructType.empty() &&
        !out.valueIsFileHandle) {
      applySemanticQueryFactResultInfo(expr, semanticProgram, semanticIndex, out);
    }
    out.errorType = !leftResultInfo.errorType.empty() ? leftResultInfo.errorType : rightResultInfo.errorType;
    return true;
  }
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && expr.name == "tryAt") {
    auto assignTryAtMapResultInfo = [&](LocalInfo::ValueKind valueKind) {
      if (valueKind == LocalInfo::ValueKind::Unknown) {
        return false;
      }
      out.isResult = true;
      out.hasValue = true;
      out.valueKind = valueKind;
      out.errorType = "ContainerError";
      return true;
    };
    const auto methodTargetInfo = resolveMapAccessTargetInfo(expr.args.front(), localsIn, inferCallMapTargetInfo);
    if (methodTargetInfo.isMapTarget && assignTryAtMapResultInfo(methodTargetInfo.mapValueKind)) {
      return true;
    }
    if (expr.args.front().kind == Expr::Kind::Call) {
      const Expr &receiverExpr = expr.args.front();
      const Definition *receiverDef = receiverExpr.isMethodCall ? resolveMethod(receiverExpr)
                                                                : resolveDefinitionCallFn(receiverExpr);
      if (receiverDef != nullptr) {
        std::string collectionName;
        std::vector<std::string> collectionArgs;
        if (inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs) &&
            collectionName == "map" && collectionArgs.size() == 2 &&
            assignTryAtMapResultInfo(valueKindFromTypeName(collectionArgs.back()))) {
          return true;
        }
      }
    }

    LocalInfo::ValueKind builtinTryAtKind = LocalInfo::ValueKind::Unknown;
    bool methodResolved = false;
    if (resolveMethodCallReturnKind(expr,
                                    localsIn,
                                    resolveMethodCallFn,
                                    resolveDefinitionCallFn,
                                    lookupReturnInfoFn,
                                    false,
                                    builtinTryAtKind,
                                    &methodResolved) &&
        methodResolved && assignTryAtMapResultInfo(builtinTryAtKind)) {
      return true;
    }
  }
  if (expr.kind == Expr::Kind::Call && !expr.args.empty() && isMapTryAtCallName(expr)) {
    const auto targetInfo = resolveMapAccessTargetInfo(expr.args.front(), localsIn, inferCallMapTargetInfo);
    if (targetInfo.isMapTarget && targetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
      out.isResult = true;
      out.hasValue = true;
      out.valueKind = targetInfo.mapValueKind;
      out.errorType = "ContainerError";
      return true;
    }
  }
  if (expr.kind == Expr::Kind::Call && semanticProgram != nullptr &&
      semanticIndex != nullptr && expr.semanticNodeId != 0) {
    const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
    if (queryFact == nullptr) {
      assignSemanticResultError(
          errorOut, "missing semantic-product query fact: " + describeSemanticResultCall(expr));
      return false;
    }
    if (!queryFact->hasResultType) {
      return false;
    }
    out.isResult = true;
    out.hasValue = queryFact->resultTypeHasValue;
    out.errorType = queryFact->resultErrorType;
    if (!out.hasValue) {
      return true;
    }
    if (!applySemanticResultValueTypeText(queryFact->resultValueType, out)) {
      assignSemanticResultError(
          errorOut,
          "incomplete semantic-product query fact: " + describeSemanticResultCall(expr));
      return false;
    }
    return true;
  }
  return resolveResultExprInfo(
      expr, lookupLocal, resolveMethod, resolveDefinitionCallFn, lookupDefinitionResult, out);
}

bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     const InferExprKindWithLocalsFn &inferExprKind,
                                     ResultExprInfo &out,
                                     const SemanticProductTargetAdapter *semanticProductTargets,
                                     std::string *errorOut) {
  return resolveResultExprInfoFromLocals(
      expr,
      localsIn,
      resolveMethodCall,
      resolveDefinitionCall,
      lookupReturnInfo,
      inferExprKind,
      out,
      semanticProductTargets == nullptr ? nullptr : semanticProductTargets->semanticProgram,
      semanticProductTargets == nullptr ? nullptr : &semanticProductTargets->semanticIndex,
      errorOut);
}

ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo,
    const InferExprKindWithLocalsFn &inferExprKind,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    std::string *errorOut) {
  return [resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, semanticProgram, semanticIndex, errorOut](
             const Expr &expr, const LocalMap &localsIn, ResultExprInfo &out) {
    return resolveResultExprInfoFromLocals(
        expr,
        localsIn,
        resolveMethodCall,
        resolveDefinitionCall,
        lookupReturnInfo,
        inferExprKind,
        out,
        semanticProgram,
        semanticIndex,
        errorOut);
  };
}

ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo,
    const InferExprKindWithLocalsFn &inferExprKind,
    const SemanticProductTargetAdapter *semanticProductTargets,
    std::string *errorOut) {
  return makeResolveResultExprInfoFromLocals(
      resolveMethodCall,
      resolveDefinitionCall,
      lookupReturnInfo,
      inferExprKind,
      semanticProductTargets == nullptr ? nullptr : semanticProductTargets->semanticProgram,
      semanticProductTargets == nullptr ? nullptr : &semanticProductTargets->semanticIndex,
      errorOut);
}


} // namespace primec::ir_lowerer
