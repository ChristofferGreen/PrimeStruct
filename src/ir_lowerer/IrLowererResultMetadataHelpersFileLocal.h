#pragma once

// Helpers shared by the IrLowererResultMetadataHelpers*.cpp units (split out of
// IrLowererResultMetadataHelpers.cpp without changes).
#include "IrLowererResultInternal.h"
#include <utility>
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererRuntimeErrorHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStatementBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace ir_lowerer_result_metadata_helpers_file_local {

inline std::string resolveScopedCallPath(const Expr &expr) {
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

inline const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadataForResultMetadata() {
  return keyValueConstructorSurfaceMetadata();
}

inline bool isFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string arg;
  return splitTemplateTypeName(trimTemplateTypeText(typeText), base, arg) &&
         normalizeCollectionBindingTypeName(base) == "File";
}

inline bool isBufferHandleCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.isBinding) {
    return false;
  }
  const std::string scopedName = resolveScopedCallPath(expr);
  return scopedName == "Buffer" || scopedName == "/std/gfx/Buffer" ||
         scopedName == "/std/gfx/experimental/Buffer" ||
         scopedName.rfind("/std/gfx/Buffer__t", 0) == 0 ||
         scopedName.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
}

inline bool extractResultValueTypeText(const std::string &typeText, std::string &valueTypeOut) {
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

inline bool isSpecializedExperimentalKeyValueTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind(keyValueStorageStructRootPath() + "__", 0) == 0;
}

inline bool isSpecializedExperimentalVectorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return isExperimentalCollectionTypeName(normalized, "vector", "Vector") &&
         normalized.find("__") != std::string::npos;
}

inline bool resolveSpecializedExperimentalVectorElementKind(const std::string &typeText,
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

inline bool resolveSpecializedExperimentalKeyValueFieldKinds(const Definition &structDef,
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

inline bool resolveSpecializedExperimentalKeyValueTypeKinds(const std::string &typeText,
                                                const ResolveCallDefinitionFn &resolveDefinitionCall,
                                                LocalInfo::ValueKind &keyKindOut,
                                                LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  if (!isSpecializedExperimentalKeyValueTypeText(typeText) || !resolveDefinitionCall) {
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
  return resolveSpecializedExperimentalKeyValueFieldKinds(*structDef, resolveDefinitionCall, keyKindOut, valueKindOut);
}

inline void assignDeclaredResultCollectionInfo(const std::string &typeText,
                                        LocalInfo::Kind &collectionKindOut,
                                        LocalInfo::ValueKind &valueKindOut,
                                        LocalInfo::ValueKind &keyValueKeyKindOut) {
  collectionKindOut = LocalInfo::Kind::Value;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  keyValueKeyKindOut = LocalInfo::ValueKind::Unknown;
  if (!resolveSupportedResultCollectionType(typeText, collectionKindOut, valueKindOut, &keyValueKeyKindOut)) {
    collectionKindOut = LocalInfo::Kind::Value;
    valueKindOut = LocalInfo::ValueKind::Unknown;
    keyValueKeyKindOut = LocalInfo::ValueKind::Unknown;
  }
}

inline void applyDeclaredResultBindingMetadata(const Expr &bindingExpr, LocalInfo &bindingInfo) {
  for (const auto &transform : bindingExpr.transforms) {
    if (isResultTemplateTypeBaseName(transform.name)) {
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

} // namespace ir_lowerer_result_metadata_helpers_file_local
} // namespace primec::ir_lowerer
