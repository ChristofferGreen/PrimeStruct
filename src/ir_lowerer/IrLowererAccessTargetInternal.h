#pragma once

// Helpers shared by the IrLowererAccessTargetResolution*.cpp units (split out of
// IrLowererAccessTargetResolution.cpp without changes, ticket).
#include "IrLowererCallHelpers.h"
#include <string_view>
#include <utility>
#include <vector>
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace access_target_internal {

inline bool isCollectionVectorRecordPath(const std::string &structTypeName) {
  const std::string vectorTypePath = vectorBackingTypePath();
  return structTypeName == vectorTypePath ||
         structTypeName.rfind(vectorTypePath + "__", 0) == 0 ||
         isExperimentalCollectionTypeName(structTypeName, "vector", "Vector");
}

inline std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

inline std::string inferExperimentalSoaVectorStructPathFromTypeName(
    const std::string &typeName) {
  const size_t first = typeName.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = typeName.find_last_not_of(" \t\r\n");
  std::string normalizedArg = typeName.substr(first, last - first + 1);
  if (normalizedArg.empty()) {
    return "";
  }
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedExperimentalSoaVectorStructPathForElementType(normalizedArg);
}

inline std::string inferExperimentalVectorStructPathFromTypeName(
    const std::string &typeName) {
  const size_t first = typeName.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = typeName.find_last_not_of(" \t\r\n");
  std::string normalizedArg = typeName.substr(first, last - first + 1);
  if (normalizedArg.empty()) {
    return "";
  }
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedCollectionVectorRecordPathForElementType(normalizedArg);
}

inline bool hasInferredTypedWrappedKeyValue(const LocalInfo &localInfo, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         localInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
         localInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown;
}

inline bool hasInferredTypedKeyValue(const LocalInfo &localInfo) {
  return localInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
         localInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown;
}

inline const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataForAccessTargets() {
  return keyValueHelperSurfaceMetadata();
}

inline const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadataForAccessTargets() {
  return keyValueConstructorSurfaceMetadata();
}

inline bool isKeyValueAccessHelperName(std::string_view helperName) {
  return collection_helpers::isAtHelperName(helperName) ||
         collection_helpers::isAtUnsafeHelperName(helperName);
}

inline bool resolveKeyValueAccessHelperPathMemberName(std::string_view path,
                                          std::string &helperNameOut) {
  helperNameOut.clear();
  const auto *metadata = keyValueHelperSurfaceMetadataForAccessTargets();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view helperName =
      resolveStdlibSurfaceMemberName(*metadata, path);
  if (helperName.empty()) {
    return false;
  }
  helperNameOut.assign(helperName);
  return true;
}

inline bool isExplicitKeyValueAccessHelperPath(std::string_view path) {
  std::string helperName;
  return resolveKeyValueAccessHelperPathMemberName(path, helperName) &&
         isKeyValueAccessHelperName(helperName);
}

inline bool resolveKeyValueConstructorExprMemberName(const Expr &expr,
                                              std::string &constructorNameOut) {
  constructorNameOut.clear();
  const auto *metadata = keyValueConstructorSurfaceMetadataForAccessTargets();
  return metadata != nullptr &&
         resolvePublishedStdlibSurfaceConstructorExprMemberName(
             expr, metadata->id, constructorNameOut);
}

inline bool resolveKeyValueConstructorPathMemberName(std::string_view path,
                                              std::string &constructorNameOut) {
  constructorNameOut.clear();
  const auto *metadata = keyValueConstructorSurfaceMetadataForAccessTargets();
  return metadata != nullptr &&
         resolvePublishedStdlibSurfaceConstructorMemberName(
             path, metadata->id, constructorNameOut);
}

inline bool isPublishedKeyValueConstructorExpr(const Expr &expr) {
  std::string constructorName;
  return resolveKeyValueConstructorExprMemberName(expr, constructorName);
}

inline std::string forwardedEmptyKeyValueConstructorMemberName() {
  const auto *metadata = keyValueConstructorSurfaceMetadataForAccessTargets();
  if (metadata != nullptr) {
    const std::string_view memberName =
        resolveStdlibSurfaceMemberName(*metadata, metadata->canonicalPath);
    if (!memberName.empty()) {
      return std::string(memberName) + "New";
    }
  }
  return std::string("map") + std::string("New");
}

inline std::string resolveAccessSemanticTypeText(const SemanticProgram *semanticProgram,
                                          const std::string &typeText,
                                          SymbolId typeTextId) {
  if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
    const std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

inline std::string normalizeAccessCollectionFamily(std::string family) {
  family = normalizeCollectionBindingTypeName(trimTemplateTypeText(family));
  if (!family.empty() && family.front() == '/') {
    family.erase(family.begin());
  }
  return family;
}

inline bool classifySemanticArrayVectorAccessTypeText(const std::string &typeText,
                                               ArrayVectorAccessTargetInfo &targetInfoOut) {
  std::string normalizedDirectType = trimTemplateTypeText(typeText);
  if (isCollectionVectorRecordPath(normalizedDirectType)) {
    targetInfoOut = {};
    targetInfoOut.isArrayOrVectorTarget = true;
    targetInfoOut.isVectorTarget = true;
    targetInfoOut.structTypeName = normalizedDirectType;
    return true;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return false;
  }
  base = normalizeAccessCollectionFamily(base);
  std::string elementText = trimTemplateTypeText(argText);
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> wrappedArgs;
    if (!splitTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
      return false;
    }
    if (!splitTemplateTypeName(trimTemplateTypeText(wrappedArgs.front()), base, elementText)) {
      return false;
    }
    base = normalizeAccessCollectionFamily(base);
  }

  if (base != "array" && base != "vector" && base != "Buffer" && base != "soa") {
    return false;
  }
  std::vector<std::string> elementArgs;
  if (!splitTemplateArgs(elementText, elementArgs) || elementArgs.empty()) {
    elementArgs = {elementText};
  }
  if (elementArgs.size() != 1) {
    return false;
  }

  targetInfoOut = {};
  targetInfoOut.isArrayOrVectorTarget = true;
  targetInfoOut.isVectorTarget = (base == "vector");
  targetInfoOut.isSoaVector = (base == "soa");
  targetInfoOut.elemKind = valueKindFromTypeName(trimTemplateTypeText(elementArgs.front()));
  if (targetInfoOut.isSoaVector) {
    targetInfoOut.structTypeName =
        inferExperimentalSoaVectorStructPathFromTypeName(elementArgs.front());
  } else if (base == "vector" &&
             targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown) {
    targetInfoOut.structTypeName =
        inferExperimentalVectorStructPathFromTypeName(elementArgs.front());
  }
  return true;
}

inline bool classifySemanticKeyValueAccessTypeText(const std::string &typeText,
                                       CollectionPairTypeInfo &targetInfoOut) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return false;
  }
  base = normalizeAccessCollectionFamily(base);
  bool isWrappedKeyValue = false;
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> wrappedArgs;
    if (!splitTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
      return false;
    }
    if (!splitTemplateTypeName(trimTemplateTypeText(wrappedArgs.front()), base, argText)) {
      return false;
    }
    base = normalizeAccessCollectionFamily(base);
    isWrappedKeyValue = true;
  }
  if (base != "map") {
    return false;
  }

  std::vector<std::string> keyValueArgs;
  if (!splitTemplateArgs(argText, keyValueArgs) || keyValueArgs.size() != 2) {
    return false;
  }

  targetInfoOut = {};
  targetInfoOut.isKeyValueTarget = true;
  targetInfoOut.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(keyValueArgs.front()));
  targetInfoOut.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(keyValueArgs.back()));
  targetInfoOut.isWrappedKeyValueTarget = isWrappedKeyValue;
  return true;
}

inline bool resolveSemanticArrayVectorAccessTargetInfo(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    ArrayVectorAccessTargetInfo &targetInfoOut,
    bool &hasSemanticFactOut) {
  hasSemanticFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || targetExpr.semanticNodeId == 0) {
    return false;
  }

  auto tryClassifyType = [&](const std::string &typeText, SymbolId typeTextId) {
    const std::string resolvedTypeText =
        resolveAccessSemanticTypeText(semanticProgram, typeText, typeTextId);
    return classifySemanticArrayVectorAccessTypeText(resolvedTypeText, targetInfoOut);
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, targetExpr);
      collectionFact != nullptr) {
    hasSemanticFactOut = true;
    const std::string rawFamily = resolveAccessSemanticTypeText(
        semanticProgram,
        collectionFact->collectionFamily,
        collectionFact->collectionFamilyId);
    const std::string family = normalizeAccessCollectionFamily(rawFamily);
    if (family != "array" && family != "vector" && family != "Buffer" &&
        family != "soa") {
      return false;
    }
    targetInfoOut = {};
    targetInfoOut.isArrayOrVectorTarget = true;
    targetInfoOut.isVectorTarget = (family == "vector");
    targetInfoOut.isSoaVector = (family == "soa");
    const std::string elementTypeText = resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->elementTypeText, collectionFact->elementTypeTextId);
    targetInfoOut.elemKind = valueKindFromTypeName(elementTypeText);
    if (targetInfoOut.isSoaVector) {
      targetInfoOut.structTypeName =
          inferExperimentalSoaVectorStructPathFromTypeName(elementTypeText);
    } else if (family == "vector" &&
               targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown) {
      targetInfoOut.structTypeName = elementTypeText.empty() &&
                                             isCollectionVectorRecordPath(rawFamily)
                                         ? rawFamily
                                         : inferExperimentalVectorStructPathFromTypeName(elementTypeText);
    }
    if (targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown &&
        targetInfoOut.structTypeName.empty()) {
      return false;
    }
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, targetExpr);
      queryFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(queryFact->queryTypeText, queryFact->queryTypeTextId) ||
           tryClassifyType(queryFact->bindingTypeText, queryFact->bindingTypeTextId) ||
           tryClassifyType(queryFact->receiverBindingTypeText,
                           queryFact->receiverBindingTypeTextId);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, targetExpr);
      bindingFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, targetExpr);
      localAutoFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
  }
  return false;
}

inline bool resolveSemanticCollectionPairTypeInfo(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    CollectionPairTypeInfo &targetInfoOut,
    bool &hasSemanticFactOut) {
  hasSemanticFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || targetExpr.semanticNodeId == 0) {
    return false;
  }

  auto tryClassifyType = [&](const std::string &typeText, SymbolId typeTextId) {
    const std::string resolvedTypeText =
        resolveAccessSemanticTypeText(semanticProgram, typeText, typeTextId);
    return classifySemanticKeyValueAccessTypeText(resolvedTypeText, targetInfoOut);
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, targetExpr);
      collectionFact != nullptr) {
    hasSemanticFactOut = true;
    const std::string family = normalizeAccessCollectionFamily(
        resolveAccessSemanticTypeText(
            semanticProgram,
            collectionFact->collectionFamily,
            collectionFact->collectionFamilyId));
    if (family != "map") {
      return false;
    }
    targetInfoOut = {};
    targetInfoOut.isKeyValueTarget = true;
    targetInfoOut.keyValueKeyKind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->keyTypeText, collectionFact->keyTypeTextId));
    targetInfoOut.keyValueValueKind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->valueTypeText, collectionFact->valueTypeTextId));
    targetInfoOut.isWrappedKeyValueTarget = collectionFact->isReference || collectionFact->isPointer;
    targetInfoOut.structTypeName = resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->structPath, collectionFact->structPathId);
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, targetExpr);
      queryFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(queryFact->queryTypeText, queryFact->queryTypeTextId) ||
           tryClassifyType(queryFact->bindingTypeText, queryFact->bindingTypeTextId) ||
           tryClassifyType(queryFact->receiverBindingTypeText,
                           queryFact->receiverBindingTypeTextId);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, targetExpr);
      bindingFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, targetExpr);
      localAutoFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
  }
  return false;
}

inline LocalInfo::ValueKind resolveAccessIndexSemanticKind(
    const Expr &indexExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr || indexExpr.semanticNodeId == 0) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, indexExpr);
      queryFact != nullptr) {
    LocalInfo::ValueKind kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
    kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, indexExpr);
      bindingFact != nullptr) {
    const LocalInfo::ValueKind kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, indexExpr);
      localAutoFact != nullptr) {
    const LocalInfo::ValueKind kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
  }
  return LocalInfo::ValueKind::Unknown;
}

inline const Expr *findForwardedReturnValueExpr(const Definition &definition) {
  if (definition.returnExpr.has_value()) {
    return &*definition.returnExpr;
  }
  const Expr *valueExpr = nullptr;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
      continue;
    }
    valueExpr = &stmt;
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      return &stmt.args.front();
    }
  }
  return valueExpr;
}

inline bool resolveForwardedReturnParameterName(const Definition &definition,
                                         std::string &parameterNameOut) {
  parameterNameOut.clear();
  const Expr *valueExpr = findForwardedReturnValueExpr(definition);
  if (valueExpr == nullptr) {
    return false;
  }
  if (isReturnCall(*valueExpr) && valueExpr->args.size() == 1) {
    valueExpr = &valueExpr->args.front();
  }
  if (valueExpr->kind != Expr::Kind::Name) {
    return false;
  }
  for (const auto &parameter : definition.parameters) {
    if (parameter.name == valueExpr->name) {
      parameterNameOut = valueExpr->name;
      return true;
    }
  }
  return false;
}

inline const Expr *resolveCallArgumentForParameter(const Expr &target,
                                            const Definition &callee,
                                            const std::string &parameterName) {
  for (size_t index = 0; index < target.args.size(); ++index) {
    if (index < target.argNames.size() &&
        target.argNames[index].has_value() &&
        *target.argNames[index] == parameterName) {
      return &target.args[index];
    }
  }
  for (size_t index = 0; index < callee.parameters.size(); ++index) {
    if (callee.parameters[index].name != parameterName) {
      continue;
    }
    return index < target.args.size() ? &target.args[index] : nullptr;
  }
  return nullptr;
}

inline bool isForwardedKeyValueNewConstructor(const Expr &expr) {
  std::string constructorName;
  return resolveKeyValueConstructorExprMemberName(expr, constructorName) &&
         constructorName == forwardedEmptyKeyValueConstructorMemberName();
}

inline bool inferDirectKeyValueConstructorTargetInfo(const Expr &target, CollectionPairTypeInfo &info) {
  info = {};
  if (target.kind != Expr::Kind::Call || target.isBinding || target.isMethodCall) {
    return false;
  }

  std::string normalizedName = target.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  auto isDirectKeyValueConstructor = [&]() {
    std::string constructorName;
    return resolveKeyValueConstructorPathMemberName(normalizedName, constructorName) ||
           isPublishedKeyValueConstructorExpr(target);
  };
  auto inferLiteralKind = [&](const Expr &valueExpr, LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    if (valueExpr.kind == Expr::Kind::Literal) {
      kindOut = valueExpr.isUnsigned ? LocalInfo::ValueKind::UInt64
                                     : (valueExpr.intWidth == 64 ? LocalInfo::ValueKind::Int64
                                                                 : LocalInfo::ValueKind::Int32);
      return true;
    }
    if (valueExpr.kind == Expr::Kind::BoolLiteral) {
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    }
    if (valueExpr.kind == Expr::Kind::FloatLiteral) {
      kindOut = valueExpr.floatWidth == 64 ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      return true;
    }
    if (valueExpr.kind == Expr::Kind::StringLiteral) {
      kindOut = LocalInfo::ValueKind::String;
      return true;
    }
    return false;
  };

  if (!isDirectKeyValueConstructor()) {
    return false;
  }

  if (target.templateArgs.size() == 2) {
    info.isKeyValueTarget = true;
    info.keyValueKeyKind = valueKindFromTypeName(target.templateArgs[0]);
    info.keyValueValueKind = valueKindFromTypeName(target.templateArgs[1]);
    return true;
  }

  if (target.args.empty() || (target.args.size() % 2) != 0) {
    return false;
  }

  info.isKeyValueTarget = true;
  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  for (size_t i = 0; i < target.args.size(); i += 2) {
    LocalInfo::ValueKind currentKeyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind currentValueKind = LocalInfo::ValueKind::Unknown;
    if (!inferLiteralKind(target.args[i], currentKeyKind) ||
        !inferLiteralKind(target.args[i + 1], currentValueKind)) {
      return true;
    }
    if (keyKind == LocalInfo::ValueKind::Unknown) {
      keyKind = currentKeyKind;
    } else if (keyKind != currentKeyKind) {
      return false;
    }
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      valueKind = currentValueKind;
    } else if (valueKind != currentValueKind) {
      return false;
    }
  }

  info.keyValueKeyKind = keyKind;
  info.keyValueValueKind = valueKind;
  return true;
}

} // namespace access_target_internal
} // namespace primec::ir_lowerer
