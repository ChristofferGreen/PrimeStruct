#pragma once

// Helpers shared by the IrLowererOperatorCollectionMutationHelpers*.cpp units (split out of
// IrLowererOperatorCollectionMutationHelpers.cpp without changes).
#include "IrLowererOperatorConversionsAndCallsInternal.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererStructFieldBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererVectorRecordLayoutHelpers.h"
#include "primec/ir/SoaPathHelpers.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {
namespace ir_lowerer_operator_collection_mutation_helpers_file_local {

inline std::string stdCollectionsRoot() {
  return "/std/collections";
}

inline std::string collectionTypePath(std::string_view collectionName) {
  return stdCollectionsRoot() + "/" + std::string(collectionName);
}

inline std::string localExperimentalCollectionTypePath(std::string_view collectionName,
                                                std::string_view typeName) {
  return stdCollectionsRoot() + "/" + collection_paths::typeIdentityFolder(collectionName) +
         "/" + std::string(typeName);
}

inline bool matchesGeneratedSpecializedPath(std::string_view text,
                                     const std::string &basePath) {
  return text.rfind(basePath + "__", 0) == 0;
}

inline bool isVectorStructPath(const std::string &structPath) {
  const std::string vectorTypePath = localExperimentalCollectionTypePath("vector", "Vector");
  return collection_helpers::isCollectionFamilyRoot(structPath, collection_helpers::CollectionFamily::Vector) || structPath == vectorTypePath ||
         matchesGeneratedSpecializedPath(structPath, vectorTypePath);
}

inline bool isSpecializedExperimentalSoaVectorStructPath(const std::string &structPath) {
  return soa_paths::isExperimentalColumnarVectorSpecializedTypePath(structPath);
}

inline std::string defaultVectorRecordStructPath(std::string_view builtin) {
  if (builtin == "soa") {
    return collection_paths::memberPath(collection_paths::kSoaFolder,
                                        collection_paths::kSoaVectorTypeName);
  }
  return collectionTypePath("vector");
}

inline std::string stripGeneratedStructSuffix(std::string structPath) {
  const size_t leafStart = structPath.find_last_of('/');
  const size_t suffixStart =
      structPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (suffixStart != std::string::npos) {
    structPath.erase(suffixStart);
  }
  return structPath;
}

inline std::string stripTemplateArguments(std::string structPath) {
  const size_t templateStart = structPath.find('<');
  if (templateStart != std::string::npos) {
    structPath.erase(templateStart);
  }
  return structPath;
}

inline std::string normalizedInternalSoaStorageLeaf(std::string structPath) {
  structPath = stripTemplateArguments(std::move(structPath));
  structPath = stripGeneratedStructSuffix(std::move(structPath));
  if (!structPath.empty() && structPath.front() == '/') {
    structPath.erase(structPath.begin());
  }
  const std::string Prefix = collection_paths::modulePrefixBare(collection_paths::kInternalSoaStorageFolder);
  if (structPath.rfind(Prefix, 0) == 0) {
    structPath.erase(0, Prefix.size());
  }
  if (structPath == "SoaColumn" || structPath == "SoaFieldView" ||
      structPath.rfind("SoaColumns", 0) == 0) {
    return structPath;
  }
  return {};
}

inline bool areCompatibleInternalSoaStoragePaths(const std::string &lhs, const std::string &rhs) {
  const std::string lhsLeaf = normalizedInternalSoaStorageLeaf(lhs);
  if (lhsLeaf.empty()) {
    return false;
  }
  return lhsLeaf == normalizedInternalSoaStorageLeaf(rhs);
}

inline bool areCompatibleStructPaths(const std::string &lhs, const std::string &rhs) {
  return lhs == rhs || (isVectorStructPath(lhs) && isVectorStructPath(rhs)) ||
         areCompatibleInternalSoaStoragePaths(lhs, rhs);
}

inline bool classifyArrayVectorTypeText(
    const std::string &typeText,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    ArrayVectorAccessTargetInfo &targetInfoOut) {
  std::string collectionType;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), collectionType, argText)) {
    return false;
  }
  collectionType = normalizeCollectionBindingTypeName(trimTemplateTypeText(collectionType));
  std::string elementType = trimTemplateTypeText(argText);
  if (collectionType == "Reference" || collectionType == "Pointer") {
    std::string wrappedBase;
    std::string wrappedArgs;
    if (!splitTemplateTypeName(elementType, wrappedBase, wrappedArgs)) {
      return false;
    }
    collectionType = normalizeCollectionBindingTypeName(trimTemplateTypeText(wrappedBase));
    elementType = trimTemplateTypeText(wrappedArgs);
  }
  if (collectionType != "array" && collectionType != "vector") {
    return false;
  }
  elementType = trimTemplateTypeText(elementType);
  if (elementType.empty()) {
    return false;
  }
  targetInfoOut = {};
  targetInfoOut.isArrayOrVectorTarget = true;
  targetInfoOut.isVectorTarget = (collectionType == "vector");
  targetInfoOut.elemKind = valueKindFromTypeName(elementType);
  return true;
}

inline bool classifyArrayVectorFieldBinding(
    const LayoutFieldBinding &binding,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    ArrayVectorAccessTargetInfo &targetInfoOut) {
  return classifyArrayVectorTypeText(
      binding.typeTemplateArg.empty()
          ? binding.typeName
          : binding.typeName + "<" + binding.typeTemplateArg + ">",
      valueKindFromTypeName,
      targetInfoOut);
}

inline bool resolveArrayVectorAssignmentAccessName(const Expr &expr,
                                            std::string &accessNameOut) {
  if (getBuiltinArrayAccessName(expr, accessNameOut)) {
    return true;
  }
  std::string vectorHelperName;
  if (!resolveVectorHelperAliasName(expr, vectorHelperName) ||
      (vectorHelperName != "at" && vectorHelperName != "at_unsafe")) {
    accessNameOut.clear();
    return false;
  }
  accessNameOut = vectorHelperName;
  return true;
}

inline bool resolveSemanticBareArrayVectorAssignmentAccessName(
    const Expr &expr,
    std::string &accessNameOut) {
  if (expr.kind != Expr::Kind::Call || !expr.namespacePrefix.empty() ||
      expr.name.find('/') != std::string::npos) {
    return false;
  }
  if (expr.name != "at" && expr.name != "at_unsafe") {
    return false;
  }
  accessNameOut = expr.name;
  return true;
}

inline bool resolveSemanticArrayVectorTargetInfo(
    const Expr &expr,
    const SemanticProductTargetAdapter *semanticProductTargets,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    ArrayVectorAccessTargetInfo &targetInfoOut,
    bool &hasSemanticFactOut) {
  hasSemanticFactOut = false;
  if (semanticProductTargets == nullptr || !semanticProductTargets->hasSemanticProduct ||
      semanticProductTargets->semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const SemanticProgram *semanticProgram = semanticProductTargets->semanticProgram;
  auto tryClassifyType = [&](SymbolId typeTextId, const std::string &typeText) {
    const std::string resolvedTypeText =
        resolveSemanticProductTypeText(semanticProgram, typeText, typeTextId);
    return classifyArrayVectorTypeText(resolvedTypeText, valueKindFromTypeName, targetInfoOut);
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticProductTargets, expr)) {
    hasSemanticFactOut = true;
    const std::string family = resolveSemanticProductTypeText(
        semanticProgram, collectionFact->collectionFamily, collectionFact->collectionFamilyId);
    const std::string elementType = resolveSemanticProductTypeText(
        semanticProgram, collectionFact->elementTypeText, collectionFact->elementTypeTextId);
    const std::string normalizedFamily = normalizeCollectionBindingTypeName(family);
    if (normalizedFamily != "array" && normalizedFamily != "vector") {
      return false;
    }
    targetInfoOut = {};
    targetInfoOut.isArrayOrVectorTarget = true;
    targetInfoOut.isVectorTarget = (normalizedFamily == "vector");
    targetInfoOut.elemKind = valueKindFromTypeName(trimTemplateTypeText(elementType));
    return true;
  }
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticProductTargets, expr)) {
    hasSemanticFactOut = true;
    return tryClassifyType(bindingFact->bindingTypeTextId, bindingFact->bindingTypeText);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticProductTargets, expr)) {
    hasSemanticFactOut = true;
    return tryClassifyType(localAutoFact->bindingTypeTextId, localAutoFact->bindingTypeText);
  }
  if (const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticProductTargets, expr)) {
    hasSemanticFactOut = true;
    return tryClassifyType(queryFact->bindingTypeTextId, queryFact->bindingTypeText) ||
           tryClassifyType(queryFact->queryTypeTextId, queryFact->queryTypeText) ||
           tryClassifyType(queryFact->receiverBindingTypeTextId,
                           queryFact->receiverBindingTypeText);
  }
  return false;
}

inline LocalInfo::ValueKind scalarMutationValueKindFromTypeText(
    const std::string &typeText,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName) {
  const std::string normalizedType = trimTemplateTypeText(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if (base == "Reference" || base == "Pointer") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(argText, args) || args.size() != 1) {
        return LocalInfo::ValueKind::Unknown;
      }
      return scalarMutationValueKindFromTypeText(args.front(), valueKindFromTypeName);
    }
    return LocalInfo::ValueKind::Unknown;
  }
  return valueKindFromTypeName(normalizedType);
}

inline bool inferSemanticMutationTargetValueKind(
    const Expr &expr,
    const SemanticProductTargetAdapter *semanticProductTargets,
    const ConversionsAndCallsValueKindFromTypeNameFn &valueKindFromTypeName,
    LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Name || semanticProductTargets == nullptr ||
      !semanticProductTargets->hasSemanticProduct ||
      semanticProductTargets->semanticProgram == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const SemanticProgram *semanticProgram = semanticProductTargets->semanticProgram;
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticProductTargets, expr)) {
    kindOut = scalarMutationValueKindFromTypeText(
        resolveSemanticProductTypeText(
            semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId),
        valueKindFromTypeName);
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticProductTargets, expr)) {
    kindOut = scalarMutationValueKindFromTypeText(
        resolveSemanticProductTypeText(
            semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId),
        valueKindFromTypeName);
    return true;
  }
  if (const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticProductTargets, expr)) {
    std::string typeText = resolveSemanticProductTypeText(
        semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (typeText.empty()) {
      typeText = resolveSemanticProductTypeText(
          semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
    }
    kindOut = scalarMutationValueKindFromTypeText(typeText, valueKindFromTypeName);
    return true;
  }
  return false;
}

} // namespace ir_lowerer_operator_collection_mutation_helpers_file_local
} // namespace primec::ir_lowerer
