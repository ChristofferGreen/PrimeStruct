#pragma once

// Helpers shared by the IrLowererStatementBindingHelpers*.cpp units (split out of
// IrLowererStatementBindingHelpers.cpp without changes).
#include "primec/ir_lowerer/IrLowererStatementBindingHelpers.h"
#include <algorithm>
#include <cctype>
#include "IrLowererStatementBindingInternal.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace ir_lowerer_statement_binding_helpers_file_local {

inline void mergeStatementBindingAuxTypeInfo(const StatementBindingTypeInfo &source,
                                      StatementBindingTypeInfo &dest) {
  dest.referenceToArray = dest.referenceToArray || source.referenceToArray;
  dest.pointerToArray = dest.pointerToArray || source.pointerToArray;
  dest.referenceToVector = dest.referenceToVector || source.referenceToVector;
  dest.pointerToVector = dest.pointerToVector || source.pointerToVector;
  dest.referenceToBuffer = dest.referenceToBuffer || source.referenceToBuffer;
  dest.pointerToBuffer = dest.pointerToBuffer || source.pointerToBuffer;
  dest.isSoaVector = dest.isSoaVector || source.isSoaVector;
  dest.usesBuiltinCollectionLayout =
      dest.usesBuiltinCollectionLayout || source.usesBuiltinCollectionLayout;
}

inline bool isSpecializedExperimentalMapTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind(keyValueStorageStructRootPath() + "__", 0) == 0;
}

inline std::string resolveSemanticBindingTypeText(const SemanticProgram *semanticProgram,
                                           const SemanticProgramBindingFact &bindingFact) {
  if (semanticProgram != nullptr && bindingFact.bindingTypeTextId != InvalidSymbolId) {
    std::string resolvedTypeText = std::string(
        semanticProgramResolveCallTargetString(*semanticProgram,
                                               bindingFact.bindingTypeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(bindingFact.bindingTypeText);
}

inline std::string resolveSemanticTryValueTypeText(const SemanticProgram *semanticProgram,
                                            const SemanticProgramTryFact &tryFact) {
  if (semanticProgram != nullptr && tryFact.valueTypeId != InvalidSymbolId) {
    std::string resolvedTypeText = std::string(
        semanticProgramResolveCallTargetString(*semanticProgram,
                                               tryFact.valueTypeId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(tryFact.valueType);
}

inline std::string resolveSemanticCollectionSpecializationText(
    const SemanticProgram *semanticProgram,
    const std::string &fallback,
    SymbolId textId) {
  if (semanticProgram != nullptr && textId != InvalidSymbolId) {
    std::string resolvedText = std::string(
        semanticProgramResolveCallTargetString(*semanticProgram, textId));
    if (!resolvedText.empty()) {
      return trimTemplateTypeText(resolvedText);
    }
  }
  return trimTemplateTypeText(fallback);
}

inline bool isSpecializedExperimentalVectorTypeText(const std::string &typeText) {
  const std::string normalized = trimTemplateTypeText(typeText);
  return isExperimentalCollectionTypeName(normalized, "vector", "Vector") &&
         normalized.find("__") != std::string::npos;
}

inline bool isCollectionVectorSurfaceBase(const std::string &typeText) {
  const std::string normalized = trimTemplateTypeText(typeText);
  return normalized == "Vector" ||
         normalized == vectorBackingTypePath(false) ||
         normalized == vectorBackingTypePath();
}

inline bool isSpecializedExperimentalSoaVectorStructPathText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  return normalized.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0;
}

inline bool resolveSpecializedExperimentalVectorElementKind(const std::string &typeText,
                                                     const std::function<const Definition *(const Expr &)>
                                                         &resolveDefinitionCall,
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

inline bool resolveSpecializedExperimentalMapTypeKinds(const std::string &typeText,
                                                const std::function<const Definition *(const Expr &)>
                                                    &resolveDefinitionCall,
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
  for (const auto &fieldExpr : structDef->statements) {
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

inline bool resolveSpecializedKeyValueStorageStructPathFromTypeText(const std::string &typeText,
                                                             std::string &structPathOut) {
  structPathOut.clear();
  std::string normalizedType = trimTemplateTypeText(typeText);
  if (!normalizedType.empty() && normalizedType.front() != '/') {
    normalizedType.insert(normalizedType.begin(), '/');
  }
  if (isKeyValueStorageStructPath(normalizedType) &&
      normalizedType.find("__") != std::string::npos) {
    structPathOut = normalizedType;
    return true;
  }
  return false;
}

inline bool resolveSpecializedExperimentalSoaVectorStructPath(const std::string &typeText,
                                                       std::string &structPathOut) {
  structPathOut.clear();
  std::string normalizedType = trimTemplateTypeText(typeText);
  while (true) {
    if (!normalizedType.empty() && normalizedType.front() != '/') {
      normalizedType.insert(normalizedType.begin(), '/');
    }
    if (isSpecializedExperimentalSoaVectorStructPathText(normalizedType)) {
      structPathOut = normalizedType;
      return true;
    }

    std::string base;
    std::string argList;
    if (!splitTemplateTypeName(normalizedType, base, argList)) {
      return false;
    }

    const std::string normalizedBase =
        normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
        !argList.empty()) {
      std::vector<std::string> wrappedArgs;
      if (splitTemplateArgs(argList, wrappedArgs) && wrappedArgs.size() == 1) {
        normalizedType = unwrapTopLevelUninitializedTypeText(
            trimTemplateTypeText(wrappedArgs.front()));
      } else {
        normalizedType = unwrapTopLevelUninitializedTypeText(
            trimTemplateTypeText(argList));
      }
      continue;
    }

    if (normalizedBase != "soa" || argList.empty()) {
      return false;
    }

    std::vector<std::string> templateArgs;
    if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 1) {
      return false;
    }

    std::string normalizedArg = trimTemplateTypeText(templateArgs.front());
    if (!normalizedArg.empty() && normalizedArg.front() == '/') {
      normalizedArg.erase(normalizedArg.begin());
    }
    structPathOut =
        specializedExperimentalSoaVectorStructPathForElementType(normalizedArg);
    return true;
  }
}

inline bool isIfBlockEnvelopeForBindingTypeInfo(const Expr &candidate) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
    return false;
  }
  if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
    return false;
  }
  return candidate.hasBodyArguments || !candidate.bodyArguments.empty();
}

inline bool tryResolveCanonicalStructTypePath(
    const std::string &typeText,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    std::string &structPathOut) {
  structPathOut.clear();
  if (!resolveDefinitionCall) {
    return false;
  }

  Expr syntheticExpr;
  syntheticExpr.kind = Expr::Kind::Call;
  syntheticExpr.name = trimTemplateTypeText(typeText);
  if (syntheticExpr.name.empty()) {
    return false;
  }

  const Definition *definition = resolveDefinitionCall(syntheticExpr);
  if (definition == nullptr || !isStructDefinition(*definition)) {
    return false;
  }

  structPathOut = definition->fullPath;
  return !structPathOut.empty();
}

inline bool shouldDeferSurfaceStructTypeName(const StatementBindingTypeInfo &info) {
  return info.kind == LocalInfo::Kind::Value &&
         info.valueKind == LocalInfo::ValueKind::Unknown &&
         !info.structTypeName.empty() &&
         info.structTypeName.front() != '/';
}

inline void deferSurfaceStructTypeName(StatementBindingTypeInfo &info) {
  if (shouldDeferSurfaceStructTypeName(info)) {
    info.structTypeName.clear();
  }
}

inline const Expr *findIfBranchValueExprForBindingTypeInfo(const Expr &candidate) {
  if (!isIfBlockEnvelopeForBindingTypeInfo(candidate)) {
    return &candidate;
  }
  const Expr *valueExpr = nullptr;
  for (const auto &bodyExpr : candidate.bodyArguments) {
    if (bodyExpr.isBinding) {
      continue;
    }
    if (isReturnCall(bodyExpr)) {
      if (bodyExpr.args.size() != 1) {
        return nullptr;
      }
      return &bodyExpr.args.front();
    }
    valueExpr = &bodyExpr;
  }
  return valueExpr;
}

inline bool populateBindingTypeInfoFromTypeText(
    const std::string &typeText,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    StatementBindingTypeInfo &infoOut) {
  const std::string normalizedTypeText = trimTemplateTypeText(typeText);
  if (normalizedTypeText.empty()) {
    return false;
  }

  if (resolveSpecializedExperimentalSoaVectorStructPath(normalizedTypeText,
                                                        infoOut.structTypeName)) {
    infoOut.kind = LocalInfo::Kind::Vector;
    infoOut.valueKind = LocalInfo::ValueKind::Unknown;
    infoOut.isSoaVector = true;
    return true;
  }
  if (isSpecializedExperimentalVectorTypeText(normalizedTypeText)) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.structTypeName = normalizedTypeText;
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
    return true;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedTypeText, base, argText)) {
    if (normalizedTypeText == "ContainerError" ||
        normalizedTypeText == collection_helpers::kCanonicalContainerErrorType) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      infoOut.structTypeName = collection_helpers::kCanonicalContainerErrorType;
      return true;
    }
    if (normalizedTypeText == "ImageError" ||
        normalizedTypeText == "/std/image/ImageError") {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      infoOut.structTypeName = "/std/image/ImageError";
      return true;
    }
    if (normalizedTypeText == "GfxError" ||
        normalizedTypeText == "/std/gfx/GfxError" ||
        normalizedTypeText == "/std/gfx/experimental/GfxError") {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      infoOut.structTypeName = normalizedTypeText == "/std/gfx/experimental/GfxError"
                                   ? "/std/gfx/experimental/GfxError"
                                   : "/std/gfx/GfxError";
      return true;
    }
    const LocalInfo::ValueKind scalarKind =
        valueKindFromTypeName(normalizeCollectionBindingTypeName(normalizedTypeText));
    if (scalarKind != LocalInfo::ValueKind::Unknown) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = scalarKind;
      infoOut.structTypeName.clear();
      return true;
    }
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Unknown;
    if (!tryResolveCanonicalStructTypePath(normalizedTypeText,
                                           resolveDefinitionCall,
                                           infoOut.structTypeName)) {
      infoOut.structTypeName = normalizedTypeText;
    }
    return true;
  }

  const std::string normalizedBase =
      normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (trimTemplateTypeText(base) == "args") {
    infoOut.kind = LocalInfo::Kind::Array;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(argText));
    infoOut.structTypeName.clear();
    return true;
  }
  if (normalizedBase == "array" || normalizedBase == "vector" || normalizedBase == "soa") {
    const std::string elementType = trimTemplateTypeText(argText);
    if (normalizedBase == "vector" && isCollectionVectorSurfaceBase(base)) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.valueKind = LocalInfo::ValueKind::Int64;
      infoOut.structTypeName =
          specializedCollectionVectorRecordPathForElementType(elementType);
      return true;
    }
    infoOut.kind = normalizedBase == "array" ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
    infoOut.valueKind = valueKindFromTypeName(elementType);
    if (normalizedBase == "soa") {
      infoOut.isSoaVector = true;
      resolveSpecializedExperimentalSoaVectorStructPath(normalizedTypeText,
                                                        infoOut.structTypeName);
    } else {
      infoOut.structTypeName =
          infoOut.valueKind == LocalInfo::ValueKind::Unknown
              ? specializedCollectionVectorRecordPathForElementType(
                    elementType)
              : std::string{};
    }
    return true;
  }
  if (normalizedBase == "map") {
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args) || args.size() != 2) {
      return false;
    }
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
    infoOut.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
    infoOut.valueKind = infoOut.keyValueValueKind;
    resolveSpecializedKeyValueStorageStructPathFromTypeText(normalizedTypeText, infoOut.structTypeName);
    return true;
  }
  if (normalizedBase == "Pointer" || normalizedBase == "Reference") {
    infoOut.kind =
        normalizedBase == "Pointer" ? LocalInfo::Kind::Pointer : LocalInfo::Kind::Reference;
    const std::string targetType =
        unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(argText));
    if (isSpecializedExperimentalVectorTypeText(targetType)) {
      if (infoOut.kind == LocalInfo::Kind::Reference) {
        infoOut.referenceToVector = true;
      } else {
        infoOut.pointerToVector = true;
      }
      infoOut.structTypeName = trimTemplateTypeText(targetType);
      LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
      infoOut.valueKind = LocalInfo::ValueKind::Unknown;
      if (resolveSpecializedExperimentalVectorElementKind(targetType,
                                                          resolveDefinitionCall,
                                                          elementKind)) {
        infoOut.valueKind = elementKind;
      }
      return true;
    }
    if (isSpecializedExperimentalSoaVectorStructPathText(targetType)) {
      if (infoOut.kind == LocalInfo::Kind::Reference) {
        infoOut.referenceToVector = true;
      } else {
        infoOut.pointerToVector = true;
      }
      infoOut.isSoaVector = true;
      infoOut.valueKind = LocalInfo::ValueKind::Unknown;
      infoOut.structTypeName = trimTemplateTypeText(targetType);
      if (!infoOut.structTypeName.empty() && infoOut.structTypeName.front() != '/') {
        infoOut.structTypeName.insert(infoOut.structTypeName.begin(), '/');
      }
      return true;
    }
    std::string targetBase;
    std::string targetArgText;
    if (splitTemplateTypeName(targetType, targetBase, targetArgText)) {
      const std::string normalizedTargetBase =
          normalizeCollectionBindingTypeName(trimTemplateTypeText(targetBase));
      if (normalizedTargetBase == "array") {
        if (infoOut.kind == LocalInfo::Kind::Reference) {
          infoOut.referenceToArray = true;
        } else {
          infoOut.pointerToArray = true;
        }
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(targetArgText));
        return true;
      }
      if (normalizedTargetBase == "vector") {
        if (infoOut.kind == LocalInfo::Kind::Reference) {
          infoOut.referenceToVector = true;
        } else {
          infoOut.pointerToVector = true;
        }
        const std::string elementType = trimTemplateTypeText(targetArgText);
        infoOut.valueKind = valueKindFromTypeName(elementType);
        if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
          infoOut.structTypeName =
              specializedCollectionVectorRecordPathForElementType(elementType);
        }
        return true;
      }
      if (normalizedTargetBase == "soa") {
        if (infoOut.kind == LocalInfo::Kind::Reference) {
          infoOut.referenceToVector = true;
        } else {
          infoOut.pointerToVector = true;
        }
        infoOut.isSoaVector = true;
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(targetArgText));
        resolveSpecializedExperimentalSoaVectorStructPath(targetType,
                                                          infoOut.structTypeName);
        return true;
      }
      if (normalizedTargetBase == "map") {
        std::vector<std::string> args;
        if (!splitTemplateArgs(targetArgText, args) || args.size() != 2) {
          return false;
        }
        infoOut.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
        infoOut.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
        infoOut.valueKind = infoOut.keyValueValueKind;
        resolveSpecializedKeyValueStorageStructPathFromTypeText(targetType,
                                                                infoOut.structTypeName);
        return true;
      }
      if (normalizedTargetBase == "Buffer") {
        if (infoOut.kind == LocalInfo::Kind::Reference) {
          infoOut.referenceToBuffer = true;
        } else {
          infoOut.pointerToBuffer = true;
        }
        infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(targetArgText));
        return true;
      }
      if (normalizedTargetBase == "Result") {
        bool resultHasValue = false;
        LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
        std::string resultErrorType;
        if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
          infoOut.valueKind =
              resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          infoOut.structTypeName.clear();
          return true;
        }
      }
    }
    infoOut.valueKind = valueKindFromTypeName(targetType);
    if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
      if (!tryResolveCanonicalStructTypePath(targetType,
                                             resolveDefinitionCall,
                                             infoOut.structTypeName)) {
        infoOut.structTypeName = targetType;
      }
    } else {
      infoOut.structTypeName.clear();
    }
    std::string columnarVectorStructPath;
    if (resolveSpecializedExperimentalSoaVectorStructPath(targetType,
                                                          columnarVectorStructPath)) {
      infoOut.structTypeName = std::move(columnarVectorStructPath);
      infoOut.valueKind = LocalInfo::ValueKind::Unknown;
    }
    return true;
  }
  if (normalizedBase == "Buffer") {
    infoOut.kind = LocalInfo::Kind::Buffer;
    infoOut.valueKind = valueKindFromTypeName(trimTemplateTypeText(argText));
    return true;
  }
  if (normalizedBase == "File") {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
    infoOut.structTypeName.clear();
    return true;
  }
  if (normalizedBase == "Result") {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Int64;
    return true;
  }
  if (resolveDefinitionCall) {
    LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
    if (resolveSpecializedExperimentalVectorElementKind(normalizedTypeText, resolveDefinitionCall, elemKind)) {
      infoOut.kind = LocalInfo::Kind::Vector;
      infoOut.valueKind = elemKind;
      infoOut.structTypeName.clear();
      return true;
    }
    LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
    if (resolveSpecializedExperimentalMapTypeKinds(normalizedTypeText, resolveDefinitionCall, keyKind, valueKind)) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.keyValueKeyKind = keyKind;
      infoOut.keyValueValueKind = valueKind;
      infoOut.valueKind = valueKind;
      infoOut.structTypeName = normalizedTypeText;
      return true;
    }
  }

  infoOut.kind = LocalInfo::Kind::Value;
  infoOut.valueKind = LocalInfo::ValueKind::Unknown;
  if (!tryResolveCanonicalStructTypePath(normalizedTypeText,
                                         resolveDefinitionCall,
                                         infoOut.structTypeName)) {
    infoOut.structTypeName = normalizedTypeText;
  }
  return true;
}

inline bool populateBindingTypeInfoFromSemanticBindingFact(
    const Expr &expr,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    StatementBindingTypeInfo &infoOut) {
  if (semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, expr);
  const std::string bindingTypeText =
      bindingFact != nullptr
          ? resolveSemanticBindingTypeText(semanticProgram, *bindingFact)
          : std::string{};
  if (bindingFact == nullptr || bindingTypeText.empty() ||
      !populateBindingTypeInfoFromTypeText(
          bindingTypeText, resolveDefinitionCall, infoOut)) {
    return false;
  }

  const auto *collectionFact =
      findSemanticProductCollectionSpecialization(*semanticIndex, expr);
  if (collectionFact == nullptr) {
    return true;
  }
  const std::string collectionFamily = resolveSemanticCollectionSpecializationText(
      semanticProgram, collectionFact->collectionFamily, collectionFact->collectionFamilyId);
  if (collectionFamily != "map") {
    return true;
  }
  infoOut.keyValueKeyKind = valueKindFromTypeName(
      resolveSemanticCollectionSpecializationText(
          semanticProgram, collectionFact->keyTypeText, collectionFact->keyTypeTextId));
  infoOut.keyValueValueKind = valueKindFromTypeName(
      resolveSemanticCollectionSpecializationText(
          semanticProgram, collectionFact->valueTypeText, collectionFact->valueTypeTextId));
  if (infoOut.valueKind == LocalInfo::ValueKind::Unknown) {
    infoOut.valueKind = infoOut.keyValueValueKind;
  }
  if (infoOut.structTypeName.empty()) {
    infoOut.structTypeName = resolveSemanticCollectionSpecializationText(
        semanticProgram, collectionFact->structPath, collectionFact->structPathId);
  }
  return true;
}

inline bool populateBindingTypeInfoFromSemanticTryFact(
    const Expr &expr,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    StatementBindingTypeInfo &infoOut) {
  if (semanticIndex == nullptr || expr.semanticNodeId == 0) {
    if (semanticProgram == nullptr || expr.sourceLine == 0 ||
        expr.sourceColumn == 0) {
      return false;
    }
  }
  const auto *tryFact = expr.semanticNodeId != 0
                            ? findSemanticProductTryFact(
                                  semanticProgram, *semanticIndex, expr)
                            : nullptr;
  if (tryFact == nullptr && semanticProgram != nullptr &&
      expr.sourceLine != 0 && expr.sourceColumn != 0) {
    for (const auto &candidate : semanticProgram->tryFacts) {
      if (candidate.sourceLine == expr.sourceLine &&
          candidate.sourceColumn == expr.sourceColumn) {
        tryFact = &candidate;
        break;
      }
    }
  }
  const std::string valueTypeText =
      tryFact != nullptr
          ? resolveSemanticTryValueTypeText(semanticProgram, *tryFact)
          : std::string{};
  return tryFact != nullptr && !valueTypeText.empty() &&
         populateBindingTypeInfoFromTypeText(
             valueTypeText, resolveDefinitionCall, infoOut);
}

inline bool inferExprBindingTypeInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const ExprLocalsValueKindFn &inferExprKind,
                              const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
                              const SemanticProgram *semanticProgram,
                              const SemanticProductIndex *semanticIndex,
                              StatementBindingTypeInfo &infoOut) {
  infoOut = {};
  if (populateBindingTypeInfoFromSemanticBindingFact(
          expr, resolveDefinitionCall, semanticProgram, semanticIndex, infoOut)) {
    return true;
  }
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    if (it == localsIn.end()) {
      return false;
    }
    infoOut.kind = it->second.kind;
    infoOut.valueKind = it->second.valueKind;
    infoOut.keyValueKeyKind = it->second.keyValueKeyKind;
    infoOut.keyValueValueKind = it->second.keyValueValueKind;
    infoOut.structTypeName = it->second.structTypeName;
    infoOut.referenceToArray = it->second.referenceToArray;
    infoOut.pointerToArray = it->second.pointerToArray;
    infoOut.referenceToVector = it->second.referenceToVector;
    infoOut.pointerToVector = it->second.pointerToVector;
    infoOut.referenceToBuffer = it->second.referenceToBuffer;
    infoOut.pointerToBuffer = it->second.pointerToBuffer;
    infoOut.isSoaVector = it->second.isSoaVector;
    infoOut.usesBuiltinCollectionLayout = it->second.usesBuiltinCollectionLayout;
    return true;
  }
  if (expr.kind == Expr::Kind::Literal) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = expr.isUnsigned ? LocalInfo::ValueKind::UInt64
                                        : (expr.intWidth == 64 ? LocalInfo::ValueKind::Int64
                                                               : LocalInfo::ValueKind::Int32);
    return true;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::Bool;
    return true;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind =
        expr.floatWidth == 64 ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
    return true;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind = LocalInfo::ValueKind::String;
    return true;
  }
  if (isIfCall(expr) && expr.args.size() == 3) {
    const Expr *thenValue = findIfBranchValueExprForBindingTypeInfo(expr.args[1]);
    const Expr *elseValue = findIfBranchValueExprForBindingTypeInfo(expr.args[2]);
    if (thenValue == nullptr || elseValue == nullptr) {
      return false;
    }
    StatementBindingTypeInfo thenInfo;
    StatementBindingTypeInfo elseInfo;
    if (!inferExprBindingTypeInfo(
            *thenValue, localsIn, inferExprKind, resolveDefinitionCall, semanticProgram, semanticIndex, thenInfo) ||
        !inferExprBindingTypeInfo(
            *elseValue, localsIn, inferExprKind, resolveDefinitionCall, semanticProgram, semanticIndex, elseInfo)) {
      return false;
    }
    if (thenInfo.kind != elseInfo.kind) {
      return false;
    }
    if (thenInfo.kind == LocalInfo::Kind::Value &&
        thenInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown) {
      if (thenInfo.keyValueKeyKind != elseInfo.keyValueKeyKind || thenInfo.keyValueValueKind != elseInfo.keyValueValueKind) {
        return false;
      }
    } else if (thenInfo.kind == LocalInfo::Kind::Value) {
      if (!thenInfo.structTypeName.empty() || !elseInfo.structTypeName.empty()) {
        if (thenInfo.structTypeName.empty() || thenInfo.structTypeName != elseInfo.structTypeName) {
          return false;
        }
      } else if (thenInfo.valueKind == LocalInfo::ValueKind::Unknown ||
                 thenInfo.valueKind != elseInfo.valueKind) {
        return false;
      }
    } else if (thenInfo.valueKind != elseInfo.valueKind ||
               thenInfo.structTypeName != elseInfo.structTypeName) {
      return false;
    }
    infoOut = thenInfo;
    return true;
  }
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }

  if (populateBindingTypeInfoFromSemanticTryFact(
          expr, resolveDefinitionCall, semanticProgram, semanticIndex, infoOut)) {
    return true;
  }

  std::string collection;
  if (getBuiltinCollectionName(expr, collection)) {
    if ((collection == "array" || collection == "vector" || collection == "soa") &&
        expr.templateArgs.size() == 1) {
      infoOut.kind = collection == "array" ? LocalInfo::Kind::Array : LocalInfo::Kind::Vector;
      infoOut.usesBuiltinCollectionLayout = (collection == "soa");
      const std::string elementType = trimTemplateTypeText(expr.templateArgs.front());
      infoOut.valueKind = valueKindFromTypeName(elementType);
      if (collection == "soa") {
        resolveSpecializedExperimentalSoaVectorStructPath(
            expr.name + "<" + elementType + ">", infoOut.structTypeName);
      } else {
        infoOut.structTypeName =
            infoOut.valueKind == LocalInfo::ValueKind::Unknown
                ? specializedCollectionVectorRecordPathForElementType(
                      elementType)
                : std::string{};
      }
      return true;
    }
    if (collection == "map" && expr.templateArgs.size() == 2) {
      infoOut.kind = LocalInfo::Kind::Value;
      infoOut.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs[0]));
      infoOut.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(expr.templateArgs[1]));
      infoOut.valueKind = infoOut.keyValueValueKind;
      return true;
    }
  }

  if (resolveDefinitionCall) {
    if (const Definition *callee = resolveDefinitionCall(expr); callee != nullptr) {
      for (const auto &transform : callee->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1 ||
            transform.templateArgs.front() == "auto") {
          continue;
        }
        return populateBindingTypeInfoFromTypeText(
            transform.templateArgs.front(), resolveDefinitionCall, infoOut);
      }
    }
  }

  const ResolveMethodCallWithLocalsFn noopResolveMethodCall =
      [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const LookupReturnInfoFn noopLookupReturnInfo =
      [](const std::string &, ReturnInfo &) { return false; };
  ResultExprInfo resultInfo;
  if (resolveResultExprInfoFromLocals(expr,
                                      localsIn,
                                      noopResolveMethodCall,
                                      resolveDefinitionCall,
                                      noopLookupReturnInfo,
                                      inferExprKind,
                                      resultInfo,
                                      semanticProgram,
                                      semanticIndex) &&
      resultInfo.isResult) {
    infoOut.kind = LocalInfo::Kind::Value;
    infoOut.valueKind =
        resultInfo.hasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    infoOut.structTypeName.clear();
    return true;
  }

  const LocalInfo::ValueKind scalarKind = inferExprKind(expr, localsIn);
  if (scalarKind == LocalInfo::ValueKind::Unknown) {
    return false;
  }
  infoOut.kind = LocalInfo::Kind::Value;
  infoOut.valueKind = scalarKind;
  return true;
}

} // namespace ir_lowerer_statement_binding_helpers_file_local
} // namespace primec::ir_lowerer
