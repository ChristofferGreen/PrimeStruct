#include "IrLowererResultInternal.h"

#include <utility>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererRuntimeErrorHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  if (!expr.namespacePrefix.empty() && expr.namespacePrefix.back() == '/') {
    return expr.namespacePrefix + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
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
  const std::string scopedName = resolveScopedCallPath(expr);
  return scopedName == "Buffer" || scopedName == "/std/gfx/Buffer" ||
         scopedName == "/std/gfx/experimental/Buffer" ||
         scopedName.rfind("/std/gfx/Buffer__t", 0) == 0 ||
         scopedName.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
}

bool isFileHandleCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding) {
    return false;
  }
  const std::string scopedName = resolveScopedCallPath(expr);
  return scopedName == "File" || scopedName == "/std/file/File";
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

bool isSpecializedExperimentalMapTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind("/std/collections/experimental_map/Map__", 0) == 0;
}

bool isSpecializedExperimentalVectorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
}

bool resolveSpecializedExperimentalVectorElementKind(const std::string &typeText,
                                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                                     LocalInfo::ValueKind &elemKindOut) {
  elemKindOut = LocalInfo::ValueKind::Unknown;
  if (!isSpecializedExperimentalVectorTypeText(typeText) || !resolveDefinitionCall) {
    return false;
  }
  Expr syntheticExpr;
  syntheticExpr.kind = Expr::Kind::Call;
  syntheticExpr.name = trimTemplateTypeText(typeText);
  if (!syntheticExpr.name.empty() && syntheticExpr.name.front() != '/') {
    syntheticExpr.name.insert(syntheticExpr.name.begin(), '/');
  }
  const Definition *structDef = resolveDefinitionCall(syntheticExpr);
  if (structDef == nullptr || !isStructDefinition(*structDef)) {
    return false;
  }
  for (const auto &fieldExpr : structDef->statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    std::string typeName;
    std::vector<std::string> templateArgs;
    if (!extractFirstBindingTypeTransform(fieldExpr, typeName, templateArgs) ||
        normalizeCollectionBindingTypeName(typeName) != "Pointer" || templateArgs.size() != 1) {
      continue;
    }
    std::string elementType = trimTemplateTypeText(templateArgs.front());
    if (!extractTopLevelUninitializedTypeText(elementType, elementType)) {
      continue;
    }
    elemKindOut = valueKindFromTypeName(elementType);
    return elemKindOut != LocalInfo::ValueKind::Unknown;
  }
  return false;
}

bool resolveSpecializedExperimentalMapFieldKinds(const Definition &structDef,
                                                 const ResolveCallDefinitionFn &resolveDefinitionCall,
                                                 LocalInfo::ValueKind &keyKindOut,
                                                 LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  for (const auto &fieldExpr : structDef.statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    std::string typeName;
    std::vector<std::string> templateArgs;
    if (!extractFirstBindingTypeTransform(fieldExpr, typeName, templateArgs) ||
        normalizeCollectionBindingTypeName(typeName) != "vector") {
      continue;
    }
    LocalInfo::ValueKind fieldKind = LocalInfo::ValueKind::Unknown;
    if (templateArgs.size() == 1) {
      fieldKind = valueKindFromTypeName(trimTemplateTypeText(templateArgs.front()));
    } else if (!resolveSpecializedExperimentalVectorElementKind(typeName, resolveDefinitionCall, fieldKind)) {
      continue;
    }
    if (fieldKind == LocalInfo::ValueKind::Unknown) {
      continue;
    }
    if (fieldExpr.name == "keys") {
      keyKindOut = fieldKind;
    } else if (fieldExpr.name == "payloads") {
      valueKindOut = fieldKind;
    }
  }
  return keyKindOut != LocalInfo::ValueKind::Unknown &&
         valueKindOut != LocalInfo::ValueKind::Unknown;
}

bool resolveSpecializedExperimentalMapTypeKinds(const std::string &typeText,
                                                const ResolveCallDefinitionFn &resolveDefinitionCall,
                                                LocalInfo::ValueKind &keyKindOut,
                                                LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  if (!isSpecializedExperimentalMapTypeText(typeText) || !resolveDefinitionCall) {
    return false;
  }
  Expr syntheticExpr;
  syntheticExpr.kind = Expr::Kind::Call;
  syntheticExpr.name = trimTemplateTypeText(typeText);
  if (!syntheticExpr.name.empty() && syntheticExpr.name.front() != '/') {
    syntheticExpr.name.insert(syntheticExpr.name.begin(), '/');
  }
  const Definition *structDef = resolveDefinitionCall(syntheticExpr);
  if (structDef == nullptr || !isStructDefinition(*structDef)) {
    return false;
  }
  return resolveSpecializedExperimentalMapFieldKinds(*structDef, resolveDefinitionCall, keyKindOut, valueKindOut);
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

} // namespace

bool usesInlineBufferResultErrorDiscriminator(const ResultExprInfo &resultInfo) {
  return resultInfo.hasValue &&
         (resultInfo.valueCollectionKind == LocalInfo::Kind::Array ||
          resultInfo.valueCollectionKind == LocalInfo::Kind::Vector ||
          resultInfo.valueCollectionKind == LocalInfo::Kind::Map ||
          resultInfo.valueCollectionKind == LocalInfo::Kind::Buffer);
}

bool populateMetadataBindingInfo(const Expr &bindingExpr,
                                 LocalMap &localsIn,
                                 const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                 const ResolveCallDefinitionFn &resolveDefinitionCall,
                                 const LookupReturnInfoFn &lookupReturnInfo,
                                 const InferExprKindWithLocalsFn &inferExprKind,
                                 const SemanticProgram *semanticProgram,
                                 const SemanticProductIndex *semanticIndex,
                                 std::string *errorOut) {
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

  std::string bindingTypeName;
  std::vector<std::string> bindingTemplateArgs;
  if (extractFirstBindingTypeTransform(bindingExpr, bindingTypeName, bindingTemplateArgs) &&
      normalizeCollectionBindingTypeName(bindingTypeName) == "File" && bindingTemplateArgs.size() == 1) {
    bindingInfo.isFileHandle = true;
    bindingInfo.valueKind = LocalInfo::ValueKind::Int64;
  }
  if (!bindingInfo.isFileHandle && bindingExpr.args.front().kind == Expr::Kind::Name) {
    auto existing = localsIn.find(bindingExpr.args.front().name);
    if (existing != localsIn.end() && existing->second.isFileHandle) {
      bindingInfo.isFileHandle = true;
      bindingInfo.valueKind = LocalInfo::ValueKind::Int64;
    }
  }
  if (!bindingInfo.isFileHandle && bindingExpr.args.front().kind == Expr::Kind::Call &&
      !bindingExpr.args.front().isMethodCall && isSimpleCallName(bindingExpr.args.front(), "File") &&
      bindingExpr.args.front().templateArgs.size() == 1) {
    bindingInfo.isFileHandle = true;
    bindingInfo.valueKind = LocalInfo::ValueKind::Int64;
  }
  if (!bindingInfo.isFileHandle && bindingExpr.args.front().kind == Expr::Kind::Call &&
      !bindingExpr.args.front().isMethodCall && isSimpleCallName(bindingExpr.args.front(), "try") &&
      bindingExpr.args.front().args.size() == 1) {
    ResultExprInfo tryResultInfo;
    if (resolveResultExprInfoFromLocals(bindingExpr.args.front().args.front(),
                                        localsIn,
                                        resolveMethodCall,
                                        resolveDefinitionCall,
                                        lookupReturnInfo,
                                        inferExprKind,
                                        tryResultInfo,
                                        semanticProgram,
                                        semanticIndex,
                                        errorOut) &&
        tryResultInfo.isResult && tryResultInfo.hasValue && tryResultInfo.valueIsFileHandle) {
      bindingInfo.isFileHandle = true;
      bindingInfo.valueKind = LocalInfo::ValueKind::Int64;
    } else if (errorOut != nullptr && !errorOut->empty()) {
      return false;
    }
  }

  ResultExprInfo bindingResultInfo;
  if (resolveResultExprInfoFromLocals(bindingExpr.args.front(),
                                      localsIn,
                                      resolveMethodCall,
                                      resolveDefinitionCall,
                                      lookupReturnInfo,
                                      inferExprKind,
                                      bindingResultInfo,
                                      semanticProgram,
                                      semanticIndex,
                                      errorOut) &&
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
  } else if (errorOut != nullptr && !errorOut->empty()) {
    return false;
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

bool isInlineBodyValueEnvelope(const Expr &candidate, const ResolveCallDefinitionFn &resolveDefinitionCall) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
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

namespace {

bool inferPackedErrorStructPathText(const std::string &callNameText, std::string &pathOut) {
  pathOut.clear();
  if (callNameText.empty()) {
    return false;
  }
  std::string normalized = trimTemplateTypeText(callNameText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  std::string baseName = normalized;
  const size_t lastSlash = baseName.rfind('/');
  if (lastSlash != std::string::npos) {
    baseName.erase(0, lastSlash + 1);
  }
  const size_t generatedSuffix = baseName.find("__");
  if (generatedSuffix != std::string::npos) {
    baseName.erase(generatedSuffix);
  }
  if (normalized == "FileError" || normalized == "std/file/FileError" || baseName == "FileError") {
    pathOut = "/std/file/FileError";
    return true;
  }
  if (normalized == "ImageError" || normalized == "std/image/ImageError" || baseName == "ImageError") {
    pathOut = "/std/image/ImageError";
    return true;
  }
  if (normalized == "ContainerError" || normalized == "std/collections/ContainerError" ||
      baseName == "ContainerError") {
    pathOut = "/std/collections/ContainerError";
    return true;
  }
  if (normalized == "GfxError" || normalized == "std/gfx/GfxError" ||
      normalized == "std/gfx/experimental/GfxError" || baseName == "GfxError") {
    pathOut = normalized.find("experimental/GfxError") != std::string::npos
                  ? "/std/gfx/experimental/GfxError"
                  : "/std/gfx/GfxError";
    return true;
  }
  return false;
}

bool inferPackedErrorStructPath(const Expr &expr, std::string &pathOut) {
  pathOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }

  if (inferPackedErrorStructPathText(expr.name, pathOut)) {
    return true;
  }

  const std::string scopedName = resolveScopedCallPath(expr);
  if (scopedName != expr.name && inferPackedErrorStructPathText(scopedName, pathOut)) {
    return true;
  }

  return false;
}

bool applySemanticDirectValueTypeText(const std::string &typeText, ResultExprInfo &out) {
  const std::string trimmedType = trimTemplateTypeText(typeText);
  if (trimmedType.empty()) {
    return false;
  }
  out.valueCollectionKind = LocalInfo::Kind::Value;
  out.valueKind = LocalInfo::ValueKind::Unknown;
  out.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
  out.valueIsFileHandle = false;
  out.valueStructType.clear();
  if (resolveSupportedResultCollectionType(
          trimmedType, out.valueCollectionKind, out.valueKind, &out.valueMapKeyKind)) {
    return true;
  }
  if (isFileHandleTypeText(trimmedType)) {
    out.valueKind = LocalInfo::ValueKind::Int64;
    out.valueIsFileHandle = true;
    return true;
  }
  out.valueKind = valueKindFromTypeName(trimmedType);
  if (out.valueKind != LocalInfo::ValueKind::Unknown) {
    return true;
  }
  out.valueStructType = trimmedType;
  if (!out.valueStructType.empty() && out.valueStructType.front() != '/') {
    out.valueStructType.insert(out.valueStructType.begin(), '/');
  }
  return true;
}

} // namespace

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

  if (inferPackedErrorStructPath(expr, structTypeOut)) {
    return true;
  }

  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(expr) : nullptr;
  if (callee != nullptr && isStructDefinition(*callee)) {
    structTypeOut = callee->fullPath;
    return true;
  }
  return inferPackedErrorStructPathText(expr.name, structTypeOut);
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
    if (!isSupportedPackedResultCollectionKind(localIt->second.kind) &&
        resolveSpecializedExperimentalMapTypeKinds(
            localIt->second.structTypeName, resolveDefinitionCall, mapKeyKindOut, valueKindOut)) {
      collectionKindOut = LocalInfo::Kind::Map;
      return true;
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

  const auto assignCollection = [&](LocalInfo::Kind kind,
                                    const std::string &elemType,
                                    LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown) {
    collectionKindOut = kind;
    valueKindOut = valueKindFromTypeName(trimTemplateTypeText(elemType));
    mapKeyKindOut = mapKeyKind;
    return valueKindOut != LocalInfo::ValueKind::Unknown &&
           (kind != LocalInfo::Kind::Map || mapKeyKindOut != LocalInfo::ValueKind::Unknown);
  };

  std::string collectionName;
  if (!expr.isMethodCall && getBuiltinCollectionName(expr, collectionName)) {
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
  if (!expr.isMethodCall) {
    auto matchesDirectMapConstructor = [&](const Expr &candidate) {
      const std::string scopedPath = resolveScopedCallPath(candidate);
      return isPublishedStdlibSurfaceConstructorExpr(
                 candidate,
                 primec::StdlibSurfaceId::CollectionsMapConstructors) ||
             scopedPath == "/std/collections/map/map" ||
             scopedPath.rfind("/std/collections/map/map__", 0) == 0 ||
             scopedPath == "std/collections/map/map" ||
             scopedPath.rfind("std/collections/map/map__", 0) == 0;
    };
    if (matchesDirectMapConstructor(expr) && expr.templateArgs.size() == 2) {
      return assignCollection(
          LocalInfo::Kind::Map,
          expr.templateArgs.back(),
          valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs.front())));
    }
    if (matchesDirectMapConstructor(expr) && expr.args.size() % 2 == 0) {
      auto inferLiteralType = [&](const Expr &value, std::string &typeOut) {
        typeOut.clear();
        if (value.kind == Expr::Kind::Literal) {
          typeOut = value.isUnsigned ? "u64" : (value.intWidth == 64 ? "i64" : "i32");
          return true;
        }
        if (value.kind == Expr::Kind::BoolLiteral) {
          typeOut = "bool";
          return true;
        }
        if (value.kind == Expr::Kind::FloatLiteral) {
          typeOut = value.floatWidth == 64 ? "f64" : "f32";
          return true;
        }
        if (value.kind == Expr::Kind::StringLiteral) {
          typeOut = "string";
          return true;
        }
        return false;
      };
      std::string keyType;
      std::string mappedValueType;
      for (size_t i = 0; i < expr.args.size(); i += 2) {
        std::string currentKeyType;
        std::string currentValueType;
        if (!inferLiteralType(expr.args[i], currentKeyType) ||
            !inferLiteralType(expr.args[i + 1], currentValueType)) {
          keyType.clear();
          mappedValueType.clear();
          break;
        }
        if (keyType.empty()) {
          keyType = currentKeyType;
        } else if (keyType != currentKeyType) {
          keyType.clear();
          mappedValueType.clear();
          break;
        }
        if (mappedValueType.empty()) {
          mappedValueType = currentValueType;
        } else if (mappedValueType != currentValueType) {
          keyType.clear();
          mappedValueType.clear();
          break;
        }
      }
      if (!keyType.empty() && !mappedValueType.empty()) {
        return assignCollection(
            LocalInfo::Kind::Map,
            mappedValueType,
            valueKindFromTypeName(trimTemplateTypeText(keyType)));
      }
    }
    std::string normalized = expr.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    const bool isBufferAllocateCall =
        normalized == "allocate" ||
        normalized.rfind("allocate__t", 0) == 0 ||
        normalized == "std/gfx/Buffer/allocate" ||
        normalized == "std/gfx/experimental/Buffer/allocate" ||
        normalized.rfind("std/gfx/Buffer/allocate__t", 0) == 0 ||
        normalized.rfind("std/gfx/experimental/Buffer/allocate__t", 0) == 0;
    if (isBufferAllocateCall && expr.templateArgs.size() == 1) {
      return assignCollection(LocalInfo::Kind::Buffer, expr.templateArgs.front());
    }
    const bool isBufferUploadCall =
        normalized == "upload" ||
        normalized.rfind("upload__t", 0) == 0 ||
        normalized == "std/gfx/Buffer/upload" ||
        normalized == "std/gfx/experimental/Buffer/upload" ||
        normalized.rfind("std/gfx/Buffer/upload__t", 0) == 0 ||
        normalized.rfind("std/gfx/experimental/Buffer/upload__t", 0) == 0;
    if (isBufferUploadCall) {
      if (expr.templateArgs.size() == 1) {
        return assignCollection(LocalInfo::Kind::Buffer, expr.templateArgs.front());
      }
      if (expr.args.size() == 1) {
        LocalInfo::Kind sourceCollectionKind = LocalInfo::Kind::Value;
        LocalInfo::ValueKind sourceValueKind = LocalInfo::ValueKind::Unknown;
        LocalInfo::ValueKind sourceMapKeyKind = LocalInfo::ValueKind::Unknown;
        if (inferDirectResultValueCollectionInfo(expr.args.front(),
                                                 localsIn,
                                                 resolveDefinitionCall,
                                                 sourceCollectionKind,
                                                 sourceValueKind,
                                                 sourceMapKeyKind)) {
          collectionKindOut = LocalInfo::Kind::Buffer;
          valueKindOut = sourceValueKind;
          mapKeyKindOut = LocalInfo::ValueKind::Unknown;
          return valueKindOut != LocalInfo::ValueKind::Unknown;
        }
      }
    }
  }

  const Definition *callee = resolveDefinitionCall ? resolveDefinitionCall(expr) : nullptr;
  if (callee == nullptr) {
    return false;
  }
  for (const auto &transform : callee->transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    if (resolveSpecializedExperimentalMapTypeKinds(
            transform.templateArgs.front(), resolveDefinitionCall, mapKeyKindOut, valueKindOut)) {
      collectionKindOut = LocalInfo::Kind::Map;
      return true;
    }
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
    mapKeyKindOut = valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.front()));
    valueKindOut = valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.back()));
    if ((mapKeyKindOut == LocalInfo::ValueKind::Unknown ||
         valueKindOut == LocalInfo::ValueKind::Unknown) &&
        expr.templateArgs.size() == 2) {
      mapKeyKindOut = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs.front()));
      valueKindOut = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs.back()));
    }
  } else if (valueKindOut == LocalInfo::ValueKind::Unknown && expr.templateArgs.size() == 1) {
    valueKindOut = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs.front()));
  }
  if (collectionKindOut == LocalInfo::Kind::Buffer &&
      valueKindOut == LocalInfo::ValueKind::Unknown &&
      expr.args.size() == 1) {
    LocalInfo::Kind sourceCollectionKind = LocalInfo::Kind::Value;
    LocalInfo::ValueKind sourceValueKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind sourceMapKeyKind = LocalInfo::ValueKind::Unknown;
    if (inferDirectResultValueCollectionInfo(expr.args.front(),
                                             localsIn,
                                             resolveDefinitionCall,
                                             sourceCollectionKind,
                                             sourceValueKind,
                                             sourceMapKeyKind)) {
      valueKindOut = sourceValueKind;
    }
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
                                    const SemanticProgram *semanticProgram,
                                    const SemanticProductIndex *semanticIndex,
                                    ResultExprInfo &out) {
  (void)semanticProgram;

  if (valueExpr.kind == Expr::Kind::Name) {
    auto localIt = localsIn.find(valueExpr.name);
    if (localIt != localsIn.end() && localIt->second.isFileHandle) {
      out.valueKind = LocalInfo::ValueKind::Int64;
      out.valueIsFileHandle = true;
      out.valueStructType.clear();
      return;
    }
  }
  if (isFileHandleCall(valueExpr)) {
    out.valueKind = LocalInfo::ValueKind::Int64;
    out.valueIsFileHandle = true;
    out.valueStructType.clear();
    return;
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
  if (valueExpr.semanticNodeId != 0) {
    if (semanticIndex != nullptr) {
      if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, valueExpr);
          bindingFact != nullptr && !bindingFact->bindingTypeText.empty() &&
          applySemanticDirectValueTypeText(bindingFact->bindingTypeText, out)) {
        return;
      }
    }
    if (semanticIndex != nullptr) {
      if (const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, valueExpr);
          queryFact != nullptr && !queryFact->bindingTypeText.empty() &&
          applySemanticDirectValueTypeText(queryFact->bindingTypeText, out)) {
        return;
      }
    }
  }
  if (inferExprKind) {
    out.valueKind = inferExprKind(valueExpr, localsIn);
    if (out.valueKind == LocalInfo::ValueKind::Unknown) {
      std::string builtinComparison;
      if (getBuiltinComparisonName(valueExpr, builtinComparison)) {
        out.valueKind = LocalInfo::ValueKind::Bool;
      }
    }
  }
}

bool resolveBodyResultExprInfo(const std::vector<Expr> &bodyExprs,
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
  LocalMap bodyLocals = localsIn;

  for (size_t i = 0; i < bodyExprs.size(); ++i) {
    const Expr &bodyExpr = bodyExprs[i];
    const bool isLast = (i + 1 == bodyExprs.size());
    if (bodyExpr.isBinding) {
      if (!populateMetadataBindingInfo(
              bodyExpr,
              bodyLocals,
              resolveMethodCall,
              resolveDefinitionCall,
              lookupReturnInfo,
              inferExprKind,
              semanticProgram,
              semanticIndex,
              errorOut)) {
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
        *valueExpr,
        bodyLocals,
        resolveMethodCall,
        resolveDefinitionCall,
        lookupReturnInfo,
        inferExprKind,
        out,
        semanticProgram,
        semanticIndex,
        errorOut);
  }

  return false;
}

bool resolveResultLambdaValueExprForMetadata(const Expr &lambdaExpr,
                                             LocalMap &lambdaLocals,
                                             const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                             const ResolveCallDefinitionFn &resolveDefinitionCall,
                                             const LookupReturnInfoFn &lookupReturnInfo,
                                             const InferExprKindWithLocalsFn &inferExprKind,
                                             const Expr *&valueExprOut,
                                             const SemanticProgram *semanticProgram,
                                             const SemanticProductIndex *semanticIndex,
                                             std::string *errorOut) {
  valueExprOut = nullptr;
  if (!lambdaExpr.isLambda || (!lambdaExpr.hasBodyArguments && lambdaExpr.bodyArguments.empty())) {
    return false;
  }

  for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
    const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
    const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
    if (bodyExpr.isBinding) {
      if (!populateMetadataBindingInfo(
              bodyExpr,
              lambdaLocals,
              resolveMethodCall,
              resolveDefinitionCall,
              lookupReturnInfo,
              inferExprKind,
              semanticProgram,
              semanticIndex,
              errorOut)) {
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

} // namespace primec::ir_lowerer
