#include "IrLowererResultHelpers.h"

#include <utility>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererRuntimeErrorHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isMapTryAtCallName(const Expr &expr) {
  if (expr.isMethodCall && expr.name == "tryAt") {
    return true;
  }
  if (isSimpleCallName(expr, "tryAt")) {
    return true;
  }
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "map/tryAt" || normalized == "std/collections/map/tryAt" ||
         normalized == "std/collections/mapTryAt" ||
         normalized == "std/collections/experimental_map/mapTryAt" ||
         normalized.rfind("std/collections/mapTryAt__", 0) == 0 ||
         normalized.rfind("std/collections/experimental_map/mapTryAt__", 0) == 0;
}

bool isResultBuiltinCall(const Expr &expr, const std::string &name, size_t argCount) {
  return expr.kind == Expr::Kind::Call && expr.isMethodCall && expr.name == name && expr.args.size() == argCount &&
         !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result";
}

bool isFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string arg;
  return splitTemplateTypeName(trimTemplateTypeText(typeText), base, arg) &&
         normalizeCollectionBindingTypeName(base) == "File";
}

bool isBufferHandleCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding) {
    return false;
  }
  return expr.name == "Buffer" || expr.name == "/std/gfx/Buffer" ||
         expr.name == "/std/gfx/experimental/Buffer" ||
         expr.name.rfind("/std/gfx/Buffer__t", 0) == 0 ||
         expr.name.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
}

bool extractResultValueTypeText(const std::string &typeText, std::string &valueTypeOut) {
  valueTypeOut.clear();
  std::string base;
  std::string argList;
  std::vector<std::string> args;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argList) ||
      normalizeCollectionBindingTypeName(base) != "Result" ||
      !splitTemplateArgs(argList, args) || args.size() != 2) {
    return false;
  }
  valueTypeOut = trimTemplateTypeText(args.front());
  return true;
}

void assignDeclaredResultCollectionInfo(const std::string &typeText,
                                        LocalInfo::Kind &collectionKindOut,
                                        LocalInfo::ValueKind &valueKindOut,
                                        LocalInfo::ValueKind &mapKeyKindOut) {
  collectionKindOut = LocalInfo::Kind::Value;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  mapKeyKindOut = LocalInfo::ValueKind::Unknown;
  if (!resolveSupportedResultCollectionType(typeText, collectionKindOut, valueKindOut, &mapKeyKindOut)) {
    collectionKindOut = LocalInfo::Kind::Value;
    valueKindOut = LocalInfo::ValueKind::Unknown;
    mapKeyKindOut = LocalInfo::ValueKind::Unknown;
  }
}

void applyDeclaredResultBindingMetadata(const Expr &bindingExpr, LocalInfo &bindingInfo) {
  for (const auto &transform : bindingExpr.transforms) {
    if (transform.name == "Result") {
      bindingInfo.isResult = true;
      bindingInfo.resultHasValue = (transform.templateArgs.size() == 2);
      bindingInfo.resultValueKind = LocalInfo::ValueKind::Unknown;
      bindingInfo.resultValueCollectionKind = LocalInfo::Kind::Value;
      bindingInfo.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
      if (bindingInfo.resultHasValue && !transform.templateArgs.empty()) {
        assignDeclaredResultCollectionInfo(
            transform.templateArgs.front(),
            bindingInfo.resultValueCollectionKind,
            bindingInfo.resultValueKind,
            bindingInfo.resultValueMapKeyKind);
        if (bindingInfo.resultValueCollectionKind == LocalInfo::Kind::Value) {
          bindingInfo.resultValueKind = valueKindFromTypeName(transform.templateArgs.front());
        }
      }
      bindingInfo.resultValueIsFileHandle =
          bindingInfo.resultHasValue && !transform.templateArgs.empty() &&
          isFileHandleTypeText(transform.templateArgs.front());
      if (bindingInfo.resultValueIsFileHandle) {
        bindingInfo.resultValueKind = LocalInfo::ValueKind::Int64;
      }
      bindingInfo.resultErrorType = transform.templateArgs.empty() ? std::string{} : transform.templateArgs.back();
      bindingInfo.valueKind = bindingInfo.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      continue;
    }
    if ((transform.name != "Reference" && transform.name != "Pointer") || transform.templateArgs.size() != 1) {
      continue;
    }

    bool resultHasValue = false;
    LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
    std::string resultErrorType;
    if (!parseResultTypeName(trimTemplateTypeText(transform.templateArgs.front()),
                             resultHasValue,
                             resultValueKind,
                             resultErrorType)) {
      continue;
    }

    bindingInfo.isResult = true;
    bindingInfo.resultHasValue = resultHasValue;
    bindingInfo.resultValueKind = resultValueKind;
    bindingInfo.resultValueCollectionKind = LocalInfo::Kind::Value;
    bindingInfo.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
    std::string resultValueType;
    if (resultHasValue) {
      assignDeclaredResultCollectionInfo(
          extractResultValueTypeText(transform.templateArgs.front(), resultValueType) ? resultValueType
                                                                                      : std::string{},
          bindingInfo.resultValueCollectionKind,
          bindingInfo.resultValueKind,
          bindingInfo.resultValueMapKeyKind);
    }
    bindingInfo.resultValueIsFileHandle =
        resultHasValue && extractResultValueTypeText(transform.templateArgs.front(), resultValueType) &&
        isFileHandleTypeText(resultValueType);
    if (bindingInfo.resultValueIsFileHandle) {
      bindingInfo.resultValueKind = LocalInfo::ValueKind::Int64;
    }
    bindingInfo.resultErrorType = resultErrorType;
    bindingInfo.valueKind = bindingInfo.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
  }
}

bool populateMetadataBindingInfo(const Expr &bindingExpr,
                                 LocalMap &localsIn,
                                 const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                 const ResolveCallDefinitionFn &resolveDefinitionCall,
                                 const LookupReturnInfoFn &lookupReturnInfo,
                                 const InferExprKindWithLocalsFn &inferExprKind) {
  if (bindingExpr.name.empty() || bindingExpr.args.empty()) {
    return false;
  }

  LocalInfo bindingInfo;
  bindingInfo.kind = LocalInfo::Kind::Value;
  if (inferExprKind) {
    bindingInfo.valueKind = inferExprKind(bindingExpr.args.front(), localsIn);
    if (bindingInfo.valueKind == LocalInfo::ValueKind::String) {
      bindingInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
    }
  }

  applyDeclaredResultBindingMetadata(bindingExpr, bindingInfo);

  ResultExprInfo bindingResultInfo;
  if (resolveResultExprInfoFromLocals(bindingExpr.args.front(),
                                      localsIn,
                                      resolveMethodCall,
                                      resolveDefinitionCall,
                                      lookupReturnInfo,
                                      inferExprKind,
                                      bindingResultInfo) &&
      bindingResultInfo.isResult) {
    const std::string declaredErrorType = bindingInfo.resultErrorType;
    bindingInfo.isResult = true;
    bindingInfo.resultHasValue = bindingResultInfo.hasValue;
    bindingInfo.resultValueKind = bindingResultInfo.valueKind;
    bindingInfo.resultValueCollectionKind = bindingResultInfo.valueCollectionKind;
    bindingInfo.resultValueMapKeyKind = bindingResultInfo.valueMapKeyKind;
    bindingInfo.resultValueIsFileHandle = bindingResultInfo.valueIsFileHandle;
    bindingInfo.resultValueStructType = bindingResultInfo.valueStructType;
    bindingInfo.resultErrorType =
        !bindingResultInfo.errorType.empty() ? bindingResultInfo.errorType : declaredErrorType;
    bindingInfo.valueKind = bindingInfo.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
  }

  localsIn[bindingExpr.name] = bindingInfo;
  return true;
}

bool isInlineBodyBlockEnvelope(const Expr &candidate, const ResolveCallDefinitionFn &resolveDefinitionCall) {
  if (!isBlockCall(candidate) || candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
    return false;
  }
  if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
    return false;
  }
  return !resolveDefinitionCall || resolveDefinitionCall(candidate) == nullptr;
}

bool mergeControlFlowResultInfos(const ResultExprInfo &first,
                                 const ResultExprInfo &second,
                                 ResultExprInfo &out) {
  out = ResultExprInfo{};
  if (!first.isResult || !second.isResult || first.hasValue != second.hasValue) {
    return false;
  }

  out.isResult = true;
  out.hasValue = first.hasValue;
  if (out.hasValue) {
    if (first.valueKind != LocalInfo::ValueKind::Unknown && second.valueKind != LocalInfo::ValueKind::Unknown &&
        first.valueKind != second.valueKind) {
      return false;
    }
    if (first.valueIsFileHandle != second.valueIsFileHandle &&
        ((first.valueKind != LocalInfo::ValueKind::Unknown && second.valueKind != LocalInfo::ValueKind::Unknown) ||
         !first.valueStructType.empty() || !second.valueStructType.empty())) {
      return false;
    }
    out.valueKind = first.valueKind != LocalInfo::ValueKind::Unknown ? first.valueKind : second.valueKind;
    out.valueIsFileHandle = first.valueIsFileHandle || second.valueIsFileHandle;
    if (first.valueCollectionKind != second.valueCollectionKind) {
      return false;
    }
    out.valueCollectionKind = first.valueCollectionKind;
    if (first.valueMapKeyKind != second.valueMapKeyKind) {
      return false;
    }
    out.valueMapKeyKind = first.valueMapKeyKind;
    if (!first.valueStructType.empty() && !second.valueStructType.empty() &&
        first.valueStructType != second.valueStructType) {
      return false;
    }
    out.valueStructType =
        !first.valueStructType.empty() ? first.valueStructType : second.valueStructType;
  }

  if (!first.errorType.empty() && !second.errorType.empty() && first.errorType != second.errorType) {
    return false;
  }
  out.errorType = !first.errorType.empty() ? first.errorType : second.errorType;
  return true;
}

bool inferDirectResultValueStructType(const Expr &expr,
                                      const LocalMap &localsIn,
                                      const ResolveCallDefinitionFn &resolveDefinitionCall,
                                      std::string &structTypeOut) {
  structTypeOut.clear();
  if (expr.kind == Expr::Kind::Name) {
    auto localIt = localsIn.find(expr.name);
    if (localIt != localsIn.end()) {
      if (!localIt->second.structTypeName.empty()) {
        structTypeOut = localIt->second.structTypeName;
        return true;
      }
      if (localIt->second.isFileHandle) {
        return false;
      }
    }
    return false;
  }

  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }

  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(expr) : nullptr;
  if (callee != nullptr && isStructDefinition(*callee)) {
    structTypeOut = callee->fullPath;
    return true;
  }
  return false;
}

bool inferDirectResultValueCollectionInfo(const Expr &expr,
                                          const LocalMap &localsIn,
                                          const ResolveCallDefinitionFn &resolveDefinitionCall,
                                          LocalInfo::Kind &collectionKindOut,
                                          LocalInfo::ValueKind &valueKindOut,
                                          LocalInfo::ValueKind &mapKeyKindOut) {
  collectionKindOut = LocalInfo::Kind::Value;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  mapKeyKindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind == Expr::Kind::Name) {
    auto localIt = localsIn.find(expr.name);
    if (localIt == localsIn.end()) {
      return false;
    }
    if (!isSupportedPackedResultCollectionKind(localIt->second.kind)) {
      return false;
    }
    collectionKindOut = localIt->second.kind;
    valueKindOut = localIt->second.valueKind;
    mapKeyKindOut = localIt->second.mapKeyKind;
    return valueKindOut != LocalInfo::ValueKind::Unknown;
  }

  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  std::string collectionName;
  if (!expr.isMethodCall && getBuiltinCollectionName(expr, collectionName)) {
    const auto assignCollection = [&](LocalInfo::Kind kind,
                                      const std::string &elemType,
                                      LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown) {
      collectionKindOut = kind;
      valueKindOut = valueKindFromTypeName(trimTemplateTypeText(elemType));
      mapKeyKindOut = mapKeyKind;
      return valueKindOut != LocalInfo::ValueKind::Unknown &&
             (kind != LocalInfo::Kind::Map || mapKeyKindOut != LocalInfo::ValueKind::Unknown);
    };
    if (collectionName == "array" && expr.templateArgs.size() == 1) {
      return assignCollection(LocalInfo::Kind::Array, expr.templateArgs.front());
    }
    if (collectionName == "vector" && expr.templateArgs.size() == 1) {
      return assignCollection(LocalInfo::Kind::Vector, expr.templateArgs.front());
    }
    if (collectionName == "map" && expr.templateArgs.size() == 2) {
      return assignCollection(
          LocalInfo::Kind::Map,
          expr.templateArgs.back(),
          valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs.front())));
    }
  }
  if (!expr.isMethodCall && isBufferHandleCall(expr) && expr.templateArgs.size() == 1) {
    return assignCollection(LocalInfo::Kind::Buffer, expr.templateArgs.front());
  }

  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(expr) : nullptr;
  if (callee == nullptr) {
    return false;
  }
  std::string declaredCollection;
  std::vector<std::string> declaredCollectionArgs;
  if (!inferDeclaredReturnCollection(*callee, declaredCollection, declaredCollectionArgs)) {
    return false;
  }
  if (declaredCollection == "array") {
    if (declaredCollectionArgs.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Array;
  } else if (declaredCollection == "vector") {
    if (declaredCollectionArgs.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Vector;
  } else if (declaredCollection == "map") {
    if (declaredCollectionArgs.size() != 2) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Map;
    mapKeyKindOut = valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.front()));
  } else if (declaredCollection == "Buffer") {
    if (declaredCollectionArgs.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Buffer;
  } else {
    return false;
  }
  valueKindOut = valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.front()));
  if (collectionKindOut == LocalInfo::Kind::Map) {
    valueKindOut = valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.back()));
  }
  return valueKindOut != LocalInfo::ValueKind::Unknown &&
         (collectionKindOut != LocalInfo::Kind::Map || mapKeyKindOut != LocalInfo::ValueKind::Unknown);
}

void applyResultValueInfoToLocal(const ResultExprInfo &resultInfo, LocalInfo &paramInfo) {
  paramInfo.kind = resultInfo.valueCollectionKind;
  if (isSupportedPackedResultCollectionKind(resultInfo.valueCollectionKind)) {
    paramInfo.valueKind = resultInfo.valueKind;
    if (resultInfo.valueCollectionKind == LocalInfo::Kind::Map) {
      paramInfo.mapKeyKind = resultInfo.valueMapKeyKind;
      paramInfo.mapValueKind = resultInfo.valueKind;
    }
    return;
  }
  paramInfo.kind = LocalInfo::Kind::Value;
  if (resultInfo.valueIsFileHandle) {
    paramInfo.valueKind = LocalInfo::ValueKind::Int64;
    paramInfo.isFileHandle = true;
    return;
  }
  if (!resultInfo.valueStructType.empty()) {
    paramInfo.valueKind = LocalInfo::ValueKind::Int64;
    paramInfo.structTypeName = resultInfo.valueStructType;
    return;
  }
  paramInfo.valueKind = resultInfo.valueKind;
  if (resultInfo.valueKind == LocalInfo::ValueKind::String) {
    paramInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
  }
}

void applyDirectResultValueMetadata(const Expr &valueExpr,
                                    const LocalMap &localsIn,
                                    const ResolveCallDefinitionFn &resolveDefinitionCall,
                                    const InferExprKindWithLocalsFn &inferExprKind,
                                    ResultExprInfo &out) {
  if (valueExpr.kind == Expr::Kind::Name) {
    auto localIt = localsIn.find(valueExpr.name);
    if (localIt != localsIn.end() && localIt->second.isFileHandle) {
      out.valueKind = LocalInfo::ValueKind::Int64;
      out.valueIsFileHandle = true;
      out.valueStructType.clear();
      return;
    }
  }
  LocalInfo::Kind valueCollectionKind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind collectionValueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind collectionMapKeyKind = LocalInfo::ValueKind::Unknown;
  if (inferDirectResultValueCollectionInfo(
          valueExpr,
          localsIn,
          resolveDefinitionCall,
          valueCollectionKind,
          collectionValueKind,
          collectionMapKeyKind)) {
    out.valueCollectionKind = valueCollectionKind;
    out.valueKind = collectionValueKind;
    out.valueMapKeyKind = collectionMapKeyKind;
    out.valueStructType.clear();
    out.valueIsFileHandle = false;
    return;
  }
  std::string valueStructType;
  if (inferDirectResultValueStructType(valueExpr, localsIn, resolveDefinitionCall, valueStructType)) {
    out.valueStructType = std::move(valueStructType);
    out.valueKind = LocalInfo::ValueKind::Unknown;
    out.valueIsFileHandle = false;
    return;
  }
  if (inferExprKind) {
    out.valueKind = inferExprKind(valueExpr, localsIn);
  }
}

bool resolveBodyResultExprInfo(const std::vector<Expr> &bodyExprs,
                               const LocalMap &localsIn,
                               const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                               const ResolveCallDefinitionFn &resolveDefinitionCall,
                               const LookupReturnInfoFn &lookupReturnInfo,
                               const InferExprKindWithLocalsFn &inferExprKind,
                               ResultExprInfo &out) {
  out = ResultExprInfo{};
  LocalMap bodyLocals = localsIn;

  for (size_t i = 0; i < bodyExprs.size(); ++i) {
    const Expr &bodyExpr = bodyExprs[i];
    const bool isLast = (i + 1 == bodyExprs.size());
    if (bodyExpr.isBinding) {
      if (!populateMetadataBindingInfo(
              bodyExpr, bodyLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind)) {
        return false;
      }
      continue;
    }

    const Expr *valueExpr = nullptr;
    if (isSimpleCallName(bodyExpr, "return")) {
      if (bodyExpr.args.size() != 1 || !isLast) {
        return false;
      }
      valueExpr = &bodyExpr.args.front();
    } else if (isLast) {
      valueExpr = &bodyExpr;
    } else {
      continue;
    }

    return resolveResultExprInfoFromLocals(
        *valueExpr, bodyLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out);
  }

  return false;
}

bool resolveResultLambdaValueExprForMetadata(const Expr &lambdaExpr,
                                             LocalMap &lambdaLocals,
                                             const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                             const ResolveCallDefinitionFn &resolveDefinitionCall,
                                             const LookupReturnInfoFn &lookupReturnInfo,
                                             const InferExprKindWithLocalsFn &inferExprKind,
                                             const Expr *&valueExprOut) {
  valueExprOut = nullptr;
  if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
    return false;
  }

  for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
    const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
    const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
    if (bodyExpr.isBinding) {
      if (!populateMetadataBindingInfo(
              bodyExpr, lambdaLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind)) {
        return false;
      }
      continue;
    }

    if (isSimpleCallName(bodyExpr, "return")) {
      if (bodyExpr.args.size() != 1 || !isLast) {
        return false;
      }
      valueExprOut = &bodyExpr.args.front();
      break;
    }

    if (!isLast) {
      continue;
    }

    valueExprOut = &bodyExpr;
  }

  return valueExprOut != nullptr;
}

} // namespace

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

  if (!expr.isMethodCall && expr.name == "File") {
    out.isResult = true;
    out.hasValue = true;
    out.valueKind = LocalInfo::ValueKind::Int64;
    out.valueIsFileHandle = true;
    out.errorType = "FileError";
    return true;
  }

  if (expr.isMethodCall && !expr.args.empty()) {
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
                                     const InferExprKindWithLocalsFn &inferExprKind,
                                     ResultExprInfo &out) {
  out = ResultExprInfo{};
  auto isIndexedArgsPackFileHandleReceiver = [&](const Expr &receiverExpr) {
    std::string accessName;
    if (receiverExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(receiverExpr, accessName) ||
        receiverExpr.args.size() != 2 || receiverExpr.args.front().kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(receiverExpr.args.front().name);
    return it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
           it->second.argsPackElementKind == LocalInfo::Kind::Value;
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
    return resolveMethodCall(callExpr, localsIn);
  };
  auto lookupDefinitionResult = [&](const std::string &path, ResultExprInfo &resultOut) -> bool {
    ReturnInfo info;
    if (!lookupReturnInfo(path, info) || !info.isResult) {
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
    const Definition *callee = resolveDefinitionCall(targetExpr);
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
      if (isInlineBodyBlockEnvelope(branchExpr, resolveDefinitionCall)) {
        return resolveBodyResultExprInfo(
            branchExpr.bodyArguments,
            localsIn,
            resolveMethodCall,
            resolveDefinitionCall,
            lookupReturnInfo,
            inferExprKind,
            branchOut);
      }
      return resolveResultExprInfoFromLocals(
          branchExpr, localsIn, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, branchOut);
    };

    ResultExprInfo thenResultInfo;
    ResultExprInfo elseResultInfo;
    if (!resolveBranchResultInfo(expr.args[1], thenResultInfo) || !resolveBranchResultInfo(expr.args[2], elseResultInfo)) {
      return false;
    }
    return mergeControlFlowResultInfos(thenResultInfo, elseResultInfo, out);
  }
  if (isInlineBodyBlockEnvelope(expr, resolveDefinitionCall)) {
    return resolveBodyResultExprInfo(
        expr.bodyArguments, localsIn, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out);
  }
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result" && expr.name == "ok") {
    out.isResult = true;
    out.hasValue = (expr.args.size() > 1);
    if (out.hasValue && expr.args.size() == 2) {
      applyDirectResultValueMetadata(expr.args[1], localsIn, resolveDefinitionCall, inferExprKind, out);
    }
    return true;
  }
  if (isResultBuiltinCall(expr, "map", 3)) {
    ResultExprInfo sourceResultInfo;
    if (!resolveResultExprInfoFromLocals(expr.args[1],
                                         localsIn,
                                         resolveMethodCall,
                                         resolveDefinitionCall,
                                         lookupReturnInfo,
                                         inferExprKind,
                                         sourceResultInfo) ||
        !sourceResultInfo.isResult || !sourceResultInfo.hasValue || !expr.args[2].isLambda || expr.args[2].args.size() != 1) {
      return false;
    }

    LocalMap lambdaLocals = localsIn;
    LocalInfo paramInfo;
    applyResultValueInfoToLocal(sourceResultInfo, paramInfo);
    lambdaLocals[expr.args[2].args.front().name] = paramInfo;

    const Expr *mappedValueExpr = nullptr;
    if (!resolveResultLambdaValueExprForMetadata(
            expr.args[2], lambdaLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, mappedValueExpr)) {
      return false;
    }

    out.isResult = true;
    out.hasValue = true;
    applyDirectResultValueMetadata(*mappedValueExpr, lambdaLocals, resolveDefinitionCall, inferExprKind, out);
    out.errorType = sourceResultInfo.errorType;
    return true;
  }
  if (isResultBuiltinCall(expr, "and_then", 3)) {
    ResultExprInfo sourceResultInfo;
    if (!resolveResultExprInfoFromLocals(expr.args[1],
                                         localsIn,
                                         resolveMethodCall,
                                         resolveDefinitionCall,
                                         lookupReturnInfo,
                                         inferExprKind,
                                         sourceResultInfo) ||
        !sourceResultInfo.isResult || !sourceResultInfo.hasValue || !expr.args[2].isLambda || expr.args[2].args.size() != 1) {
      return false;
    }

    LocalMap lambdaLocals = localsIn;
    LocalInfo paramInfo;
    applyResultValueInfoToLocal(sourceResultInfo, paramInfo);
    lambdaLocals[expr.args[2].args.front().name] = paramInfo;

    const Expr *chainedResultExpr = nullptr;
    if (!resolveResultLambdaValueExprForMetadata(
            expr.args[2], lambdaLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, chainedResultExpr)) {
      return false;
    }

    ResultExprInfo chainedResultInfo;
    if (!resolveResultExprInfoFromLocals(*chainedResultExpr,
                                         lambdaLocals,
                                         resolveMethodCall,
                                         resolveDefinitionCall,
                                         lookupReturnInfo,
                                         inferExprKind,
                                         chainedResultInfo) ||
        !chainedResultInfo.isResult) {
      return false;
    }
    if (chainedResultInfo.errorType.empty()) {
      chainedResultInfo.errorType = sourceResultInfo.errorType;
    }
    out = std::move(chainedResultInfo);
    return true;
  }
  if (isResultBuiltinCall(expr, "map2", 4)) {
    ResultExprInfo leftResultInfo;
    ResultExprInfo rightResultInfo;
    if (!resolveResultExprInfoFromLocals(expr.args[1],
                                         localsIn,
                                         resolveMethodCall,
                                         resolveDefinitionCall,
                                         lookupReturnInfo,
                                         inferExprKind,
                                         leftResultInfo) ||
        !resolveResultExprInfoFromLocals(expr.args[2],
                                         localsIn,
                                         resolveMethodCall,
                                         resolveDefinitionCall,
                                         lookupReturnInfo,
                                         inferExprKind,
                                         rightResultInfo) ||
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
            expr.args[3], lambdaLocals, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, mappedValueExpr)) {
      return false;
    }

    out.isResult = true;
    out.hasValue = true;
    applyDirectResultValueMetadata(*mappedValueExpr, lambdaLocals, resolveDefinitionCall, inferExprKind, out);
    out.errorType = !leftResultInfo.errorType.empty() ? leftResultInfo.errorType : rightResultInfo.errorType;
    return true;
  }
  if (expr.kind == Expr::Kind::Call && expr.isMethodCall && expr.name == "tryAt") {
    const Definition *methodCallee = resolveMethod(expr);
    if (methodCallee == nullptr) {
      LocalInfo::ValueKind builtinTryAtKind = LocalInfo::ValueKind::Unknown;
      bool methodResolved = false;
      if (resolveMethodCallReturnKind(expr,
                                      localsIn,
                                      resolveMethodCall,
                                      resolveDefinitionCall,
                                      lookupReturnInfo,
                                      false,
                                      builtinTryAtKind,
                                      &methodResolved) &&
          methodResolved && builtinTryAtKind != LocalInfo::ValueKind::Unknown) {
        out.isResult = true;
        out.hasValue = true;
        out.valueKind = builtinTryAtKind;
        out.errorType = "ContainerError";
        return true;
      }
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
  return resolveResultExprInfo(
      expr, lookupLocal, resolveMethod, resolveDefinitionCall, lookupDefinitionResult, out);
}

ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo,
    const InferExprKindWithLocalsFn &inferExprKind) {
  return [resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind](
             const Expr &expr, const LocalMap &localsIn, ResultExprInfo &out) {
    return resolveResultExprInfoFromLocals(
        expr, localsIn, resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, inferExprKind, out);
  };
}

bool isSupportedPackedResultValueKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool ||
         kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::String;
}

bool isSupportedPackedResultCollectionKind(LocalInfo::Kind kind) {
  return kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector ||
         kind == LocalInfo::Kind::Map || kind == LocalInfo::Kind::Buffer;
}

bool resolveSupportedResultCollectionType(const std::string &typeText,
                                          LocalInfo::Kind &collectionKindOut,
                                          LocalInfo::ValueKind &valueKindOut,
                                          LocalInfo::ValueKind *mapKeyKindOut) {
  collectionKindOut = LocalInfo::Kind::Value;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  if (mapKeyKindOut != nullptr) {
    *mapKeyKindOut = LocalInfo::ValueKind::Unknown;
  }
  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argList)) {
    return false;
  }
  base = normalizeCollectionBindingTypeName(base);
  std::vector<std::string> args;
  if (!splitTemplateArgs(argList, args)) {
    return false;
  }
  if (base == "array") {
    if (args.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Array;
  } else if (base == "vector") {
    if (args.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Vector;
  } else if (base == "map") {
    if (args.size() != 2) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Map;
    const LocalInfo::ValueKind mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args.front()));
    if (mapKeyKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    if (mapKeyKindOut != nullptr) {
      *mapKeyKindOut = mapKeyKind;
    }
  } else if (base == "Buffer") {
    if (args.size() != 1) {
      return false;
    }
    collectionKindOut = LocalInfo::Kind::Buffer;
  } else {
    return false;
  }
  valueKindOut =
      valueKindFromTypeName(trimTemplateTypeText(collectionKindOut == LocalInfo::Kind::Map ? args.back() : args.front()));
  return valueKindOut != LocalInfo::ValueKind::Unknown;
}

bool resolveSupportedResultStructPayloadInfo(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    bool &isPackedSingleSlotOut,
    LocalInfo::ValueKind &packedKindOut,
    int32_t &slotCountOut) {
  isPackedSingleSlotOut = false;
  packedKindOut = LocalInfo::ValueKind::Unknown;
  slotCountOut = 0;
  if (structType.empty() || !resolveStructSlotLayout) {
    return false;
  }

  const std::string trimmedType = trimTemplateTypeText(structType);
  std::string baseType = trimmedType;
  std::string templateArgs;
  std::string parsedBaseType;
  if (splitTemplateTypeName(trimmedType, parsedBaseType, templateArgs)) {
    baseType = trimTemplateTypeText(parsedBaseType);
  } else {
    baseType = trimTemplateTypeText(baseType);
  }
  const std::string normalizedBase = normalizeCollectionBindingTypeName(baseType);
  if (normalizedBase == "File" || normalizedBase == "array" || normalizedBase == "vector" ||
      normalizedBase == "map" || normalizedBase == "Buffer") {
    return false;
  }

  StructSlotLayoutInfo layout;
  if (!resolveStructSlotLayout(structType, layout) || layout.totalSlots <= 0) {
    return false;
  }
  slotCountOut = layout.totalSlots;

  if (layout.totalSlots != 1 || layout.fields.size() != 1) {
    return true;
  }

  const StructSlotFieldInfo &field = layout.fields.front();
  if (field.slotOffset != 0 || field.slotCount != 1 || !field.structPath.empty() ||
      !isSupportedPackedResultValueKind(field.valueKind)) {
    return true;
  }

  isPackedSingleSlotOut = true;
  packedKindOut = field.valueKind;
  return true;
}

bool isSupportedPackedResultValueInfo(
    const ResultExprInfo &info,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout) {
  if (isSupportedPackedResultCollectionKind(info.valueCollectionKind)) {
    return info.valueKind != LocalInfo::ValueKind::Unknown;
  }
  if (info.valueIsFileHandle && info.valueKind == LocalInfo::ValueKind::Int64) {
    return true;
  }
  if (!info.valueStructType.empty()) {
    bool isPackedSingleSlot = false;
    LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
    int32_t slotCount = 0;
    return resolveSupportedResultStructPayloadInfo(
        info.valueStructType, resolveStructSlotLayout, isPackedSingleSlot, packedStructKind, slotCount);
  }
  return isSupportedPackedResultValueKind(info.valueKind);
}

bool resolveSupportedPackedResultStructValueKind(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    LocalInfo::ValueKind &out) {
  out = LocalInfo::ValueKind::Unknown;
  bool isPackedSingleSlot = false;
  int32_t slotCount = 0;
  if (!resolveSupportedResultStructPayloadInfo(
          structType, resolveStructSlotLayout, isPackedSingleSlot, out, slotCount)) {
    return false;
  }
  return isPackedSingleSlot;
}

std::string unsupportedPackedResultValueKindError(const std::string &builtinName) {
  return "IR backends only support " + builtinName + " with supported payload values";
}

ResultOkMethodCallEmitResult tryEmitResultOkCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isFileHandleExpr,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  if (!(expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result" && expr.name == "ok")) {
    return ResultOkMethodCallEmitResult::NotHandled;
  }
  if (expr.args.size() == 1) {
    emitInstruction(IrOpcode::PushI32, 0);
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (expr.args.size() != 2) {
    error = "Result.ok accepts at most one argument";
    return ResultOkMethodCallEmitResult::Error;
  }
  const LocalInfo::ValueKind argKind = inferExprKind(expr.args[1], localsIn);
  if (isFileHandleExpr && argKind == LocalInfo::ValueKind::Int64 &&
      isFileHandleExpr(expr.args[1], localsIn)) {
    if (!emitExpr(expr.args[1], localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (isSupportedPackedResultValueKind(argKind)) {
    if (!emitExpr(expr.args[1], localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }
  LocalInfo::Kind collectionKind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind collectionValueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind collectionMapKeyKind = LocalInfo::ValueKind::Unknown;
  if (inferDirectResultValueCollectionInfo(
          expr.args[1], localsIn, resolveDefinitionCall, collectionKind, collectionValueKind, collectionMapKeyKind) &&
      isSupportedPackedResultCollectionKind(collectionKind)) {
    if (!emitExpr(expr.args[1], localsIn)) {
      return ResultOkMethodCallEmitResult::Error;
    }
    return ResultOkMethodCallEmitResult::Emitted;
  }

  const std::string structType = inferStructExprPath ? inferStructExprPath(expr.args[1], localsIn) : std::string{};
  bool isPackedSingleSlot = false;
  LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
  int32_t slotCount = 0;
  if (!resolveSupportedResultStructPayloadInfo(
          structType, resolveStructSlotLayout, isPackedSingleSlot, packedStructKind, slotCount)) {
    error = unsupportedPackedResultValueKindError("Result.ok");
    return ResultOkMethodCallEmitResult::Error;
  }
  (void)packedStructKind;
  (void)slotCount;
  if (!emitExpr(expr.args[1], localsIn)) {
    return ResultOkMethodCallEmitResult::Error;
  }
  if (!isPackedSingleSlot) {
    return ResultOkMethodCallEmitResult::Emitted;
  }
  if (!allocTempLocal) {
    error = "native backend missing temporary local for Result.ok";
    return ResultOkMethodCallEmitResult::Error;
  }
  const int32_t ptrLocal = allocTempLocal();
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadIndirect, 0);
  return ResultOkMethodCallEmitResult::Emitted;
}

bool resolveResultWhyCallInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                              ResultExprInfo &resultInfo,
                              std::string &error) {
  if (expr.args.size() != 2) {
    error = "Result.why requires exactly one argument";
    return false;
  }
  if (!resolveResultExprInfo ||
      !resolveResultExprInfo(expr.args[1], localsIn, resultInfo) ||
      !resultInfo.isResult) {
    error = "Result.why requires Result argument";
    return false;
  }
  return true;
}

bool resolveResultErrorCallInfo(const Expr &expr,
                                const LocalMap &localsIn,
                                const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                                ResultExprInfo &resultInfo,
                                std::string &error) {
  if (expr.args.size() != 2) {
    error = "Result.error requires exactly one argument";
    return false;
  }
  if (!resolveResultExprInfo ||
      !resolveResultExprInfo(expr.args[1], localsIn, resultInfo) ||
      !resultInfo.isResult) {
    error = "Result.error requires Result argument";
    return false;
  }
  return true;
}

ResultErrorMethodCallEmitResult tryEmitResultErrorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  if (!(expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result" && expr.name == "error")) {
    return ResultErrorMethodCallEmitResult::NotHandled;
  }

  ResultExprInfo resultInfo;
  if (!resolveResultErrorCallInfo(
          expr, localsIn, resolveResultExprInfo, resultInfo, error)) {
    return ResultErrorMethodCallEmitResult::Error;
  }

  int32_t errorLocal = 0;
  if (!emitResultWhyLocalsFromValueExpr(
          expr.args[1],
          localsIn,
          resultInfo.hasValue,
          emitExpr,
          allocTempLocal,
          emitInstruction,
          errorLocal)) {
    return ResultErrorMethodCallEmitResult::Error;
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  return ResultErrorMethodCallEmitResult::Emitted;
}

ResultWhyMethodCallEmitResult tryEmitResultWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error) {
  if (!(expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result" && expr.name == "why")) {
    return ResultWhyMethodCallEmitResult::NotHandled;
  }

  ResultExprInfo resultInfo;
  if (!resolveResultWhyCallInfo(
          expr, localsIn, resolveResultExprInfo, resultInfo, error)) {
    return ResultWhyMethodCallEmitResult::Error;
  }

  int32_t errorLocal = 0;
  if (!emitResultWhyLocalsFromValueExpr(
          expr.args[1],
          localsIn,
          resultInfo.hasValue,
          emitExpr,
          allocTempLocal,
          emitInstruction,
          errorLocal)) {
    return ResultWhyMethodCallEmitResult::Error;
  }

  const ResultWhyExprOps resultWhyExprOps = makeResultWhyExprOps(
      errorLocal,
      expr.namespacePrefix,
      onErrorTempCounter,
      internString,
      allocTempLocal,
      emitInstruction);

  return emitResultWhyCallWithComposedOps(
             expr,
             resultInfo,
             localsIn,
             errorLocal,
             defMap,
             resultWhyExprOps,
             resolveStructTypeName,
             getReturnInfo,
             bindingKind,
             resolveStructSlotLayout,
             valueKindFromTypeName,
             emitInlineDefinitionCall,
             emitFileErrorWhy,
             error)
             ? ResultWhyMethodCallEmitResult::Emitted
             : ResultWhyMethodCallEmitResult::Error;
}

ResultWhyDispatchEmitResult tryEmitResultWhyDispatchCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error) {
  const ResultWhyMethodCallEmitResult resultWhyCallResult = tryEmitResultWhyCall(
      expr,
      localsIn,
      defMap,
      onErrorTempCounter,
      resolveResultExprInfo,
      emitExpr,
      allocTempLocal,
      emitInstruction,
      internString,
      resolveStructTypeName,
      getReturnInfo,
      bindingKind,
      resolveStructSlotLayout,
      valueKindFromTypeName,
      emitInlineDefinitionCall,
      emitFileErrorWhy,
      error);
  if (resultWhyCallResult == ResultWhyMethodCallEmitResult::Emitted) {
    return ResultWhyDispatchEmitResult::Emitted;
  }
  if (resultWhyCallResult == ResultWhyMethodCallEmitResult::Error) {
    return ResultWhyDispatchEmitResult::Error;
  }

  const FileErrorWhyCallEmitResult fileErrorWhyResult = tryEmitFileErrorWhyCall(
      expr,
      localsIn,
      emitExpr,
      allocTempLocal,
      emitInstruction,
      [&](int32_t errorLocal) {
        (void)emitFileErrorWhy(errorLocal);
      },
      error);
  if (fileErrorWhyResult == FileErrorWhyCallEmitResult::Emitted) {
    return ResultWhyDispatchEmitResult::Emitted;
  }
  if (fileErrorWhyResult == FileErrorWhyCallEmitResult::Error) {
    return ResultWhyDispatchEmitResult::Error;
  }
  return ResultWhyDispatchEmitResult::NotHandled;
}

bool emitResultWhyLocalsFromValueExpr(
    const Expr &valueExpr,
    const LocalMap &localsIn,
    bool resultHasValue,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    int32_t &errorLocalOut) {
  if (!emitExpr || !emitExpr(valueExpr, localsIn)) {
    return false;
  }

  const int32_t resultLocal = allocTempLocal();
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal));
  errorLocalOut = allocTempLocal();
  emitResultWhyErrorLocalFromResult(
      resultLocal, resultHasValue, errorLocalOut, emitInstruction);
  return true;
}

ResultWhyExprOps makeResultWhyExprOps(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t &onErrorTempCounter,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  ResultWhyExprOps ops;
  int32_t *tempCounter = &onErrorTempCounter;
  ops.emitEmptyString = [internString, emitInstruction]() {
    return emitResultWhyEmptyString(internString, emitInstruction);
  };
  ops.makeErrorValueExpr =
      [errorLocal, namespacePrefix, tempCounter](LocalMap &callLocals,
                                                 LocalInfo::ValueKind valueKind) {
        const int32_t tempOrdinal = (*tempCounter)++;
        return makeResultWhyErrorValueExpr(
            errorLocal, valueKind, namespacePrefix, tempOrdinal, callLocals);
      };
  ops.makeBoolErrorExpr =
      [errorLocal, namespacePrefix, tempCounter, allocTempLocal, emitInstruction](
          LocalMap &callLocals) {
        const int32_t tempOrdinal = (*tempCounter)++;
        return makeResultWhyBoolErrorExpr(
            errorLocal,
            namespacePrefix,
            tempOrdinal,
            callLocals,
            allocTempLocal,
            emitInstruction);
      };
  return ops;
}

ResultWhyCallOps makeResultWhyCallOps(
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<Expr(LocalMap &, LocalInfo::ValueKind)> &makeErrorValueExpr,
    const std::function<Expr(LocalMap &)> &makeBoolErrorExpr,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    const std::function<bool()> &emitEmptyString) {
  ResultWhyCallOps ops;
  ops.resolveStructTypeName = resolveStructTypeName;
  ops.getReturnInfo = getReturnInfo;
  ops.bindingKind = bindingKind;
  ops.extractFirstBindingTypeTransform =
      [](const Expr &bindingExpr, std::string &typeNameOut, std::vector<std::string> &templateArgsOut) {
        return extractFirstBindingTypeTransform(bindingExpr, typeNameOut, templateArgsOut);
      };
  ops.resolveStructSlotLayout = resolveStructSlotLayout;
  ops.valueKindFromTypeName = valueKindFromTypeName;
  ops.makeErrorValueExpr = makeErrorValueExpr;
  ops.makeBoolErrorExpr = makeBoolErrorExpr;
  ops.emitInlineDefinitionCall = emitInlineDefinitionCall;
  ops.emitFileErrorWhy = emitFileErrorWhy;
  ops.emitEmptyString = emitEmptyString;
  return ops;
}

bool emitResultWhyCallWithComposedOps(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyExprOps &exprOps,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error) {
  const ResultWhyCallOps ops = makeResultWhyCallOps(
      resolveStructTypeName,
      getReturnInfo,
      bindingKind,
      resolveStructSlotLayout,
      valueKindFromTypeName,
      [&](LocalMap &callLocals, LocalInfo::ValueKind valueKind) {
        return exprOps.makeErrorValueExpr(callLocals, valueKind);
      },
      [&](LocalMap &callLocals) { return exprOps.makeBoolErrorExpr(callLocals); },
      emitInlineDefinitionCall,
      emitFileErrorWhy,
      [&]() { return exprOps.emitEmptyString(); });
  return emitResolvedResultWhyCall(
             expr, resultInfo, localsIn, errorLocal, defMap, ops, error) ==
         ResultWhyCallEmitResult::Emitted;
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
