#pragma once

// Helpers shared by the IrLowererSetupTypeReturnKindHelpers*.cpp units (split out of
// IrLowererSetupTypeReturnKindHelpers.cpp without changes).
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include <algorithm>
#include <vector>
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeReceiverTargetHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace ir_lowerer_setup_type_return_kind_helpers_file_local {

inline std::string resolveScopedCallPath(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return expr.name;
}

inline bool prefersExactDirectMapCountLikeReturnPath(const Expr &callExpr) {
  const std::string scopedCallPath = resolveScopedCallPath(callExpr);
  if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
      scopedCallPath.empty() || scopedCallPath.front() != '/') {
    return false;
  }
  std::string helperName;
  return resolveKeyValueHelperAliasName(callExpr, helperName) &&
         (collection_helpers::isCountHelperName(helperName) || helperName == "contains" ||
          helperName == "tryAt");
}

inline std::string canonicalKeyValueHelperPathForSetupReturnKind(
    std::string_view helperName) {
  const auto *metadata = keyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
}

struct SemanticReturnKindTargetInfo {
  ArrayVectorAccessTargetInfo arrayVectorInfo{};
  CollectionPairTypeInfo keyValueInfo{};
  LocalInfo::ValueKind valueKind{LocalInfo::ValueKind::Unknown};
};

inline bool classifySemanticReturnKindCollectionTypeText(
    const std::string &typeText,
    SemanticReturnKindTargetInfo &infoOut) {
  const std::string normalizedType = trimTemplateTypeText(typeText);
  if (normalizedType.empty()) {
    return false;
  }
  infoOut.valueKind = valueKindFromTypeName(normalizedType);

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(normalizedType, base, argText)) {
    return true;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args)) {
    return true;
  }
  if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
    return classifySemanticReturnKindCollectionTypeText(args.front(), infoOut);
  }
  if ((base == "array" || base == "vector" || base == "Buffer" ||
       base == "soa") &&
      args.size() == 1) {
    infoOut.arrayVectorInfo.isArrayOrVectorTarget = true;
    infoOut.arrayVectorInfo.isVectorTarget = base == "vector";
    infoOut.arrayVectorInfo.isSoaVector = base == "soa";
    infoOut.arrayVectorInfo.elemKind = valueKindFromTypeName(args.front());
    return true;
  }
  if (base == "map" && args.size() == 2) {
    infoOut.keyValueInfo.isKeyValueTarget = true;
    infoOut.keyValueInfo.keyValueKeyKind = valueKindFromTypeName(args.front());
    infoOut.keyValueInfo.keyValueValueKind = valueKindFromTypeName(args.back());
    return true;
  }
  return true;
}

inline bool classifySemanticReturnKindCollectionSpecialization(
    const SemanticProgram *semanticProgram,
    const SemanticProgramCollectionSpecialization &fact,
    SemanticReturnKindTargetInfo &infoOut) {
  const std::string family = normalizeCollectionBindingTypeName(
      resolveSemanticProductTypeText(semanticProgram,
                                     fact.collectionFamily,
                                     fact.collectionFamilyId));
  if (family == "array" || family == "vector" || family == "Buffer" ||
      family == "soa") {
    infoOut.arrayVectorInfo.isArrayOrVectorTarget = true;
    infoOut.arrayVectorInfo.isVectorTarget = family == "vector";
    infoOut.arrayVectorInfo.isSoaVector = family == "soa";
    infoOut.arrayVectorInfo.elemKind =
        valueKindFromTypeName(resolveSemanticProductTypeText(
            semanticProgram, fact.elementTypeText, fact.elementTypeTextId));
    return true;
  }
  if (family == "map") {
    infoOut.keyValueInfo.isKeyValueTarget = true;
    infoOut.keyValueInfo.keyValueKeyKind =
        valueKindFromTypeName(resolveSemanticProductTypeText(
            semanticProgram, fact.keyTypeText, fact.keyTypeTextId));
    infoOut.keyValueInfo.keyValueValueKind =
        valueKindFromTypeName(resolveSemanticProductTypeText(
            semanticProgram, fact.valueTypeText, fact.valueTypeTextId));
    return true;
  }
  return true;
}

inline bool resolveSemanticReturnKindTargetInfo(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    SemanticReturnKindTargetInfo &infoOut) {
  infoOut = {};
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      target.semanticNodeId == 0) {
    return false;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, target);
      collectionFact != nullptr) {
    return classifySemanticReturnKindCollectionSpecialization(
        semanticProgram, *collectionFact, infoOut);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, target);
      bindingFact != nullptr) {
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       bindingFact->bindingTypeText,
                                       bindingFact->bindingTypeTextId),
        infoOut);
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, target);
      localAutoFact != nullptr) {
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       localAutoFact->bindingTypeText,
                                       localAutoFact->bindingTypeTextId),
        infoOut);
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFactBySemanticId(*semanticIndex, target);
      queryFact != nullptr) {
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       queryFact->queryTypeText,
                                       queryFact->queryTypeTextId),
        infoOut);
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       queryFact->bindingTypeText,
                                       queryFact->bindingTypeTextId),
        infoOut);
    classifySemanticReturnKindCollectionTypeText(
        resolveSemanticProductTypeText(semanticProgram,
                                       queryFact->receiverBindingTypeText,
                                       queryFact->receiverBindingTypeTextId),
        infoOut);
    return true;
  }
  return false;
}

// Coarse shape category used to cross-check a semantic-fact-resolved
// receiver type against the receiver's own *structural* `LocalInfo` (see
// TODO-5302 round 6/7: a real semantic product can still legitimately carry
// a decoy/mismatched raw type-text field, but its *resolved shape* - map vs.
// array/vector vs. string vs. plain scalar - should never disagree with the
// receiver's independently-populated structural `LocalInfo` for a genuine
// compiled program; when it does, the fact is untrustworthy for classifying
// that receiver).
enum class ReceiverShapeCategory { KeyValue, ArrayVector, String, Other };

inline ReceiverShapeCategory classifySemanticReturnKindShapeCategory(
    const SemanticReturnKindTargetInfo &info) {
  if (info.keyValueInfo.isKeyValueTarget) {
    return ReceiverShapeCategory::KeyValue;
  }
  if (info.arrayVectorInfo.isArrayOrVectorTarget) {
    return ReceiverShapeCategory::ArrayVector;
  }
  if (info.valueKind == LocalInfo::ValueKind::String) {
    return ReceiverShapeCategory::String;
  }
  return ReceiverShapeCategory::Other;
}

inline ReceiverShapeCategory classifyLocalInfoShapeCategory(const LocalInfo &info) {
  if (info.isArgsPack) {
    if (hasKeyValueKinds(info)) {
      return ReceiverShapeCategory::KeyValue;
    }
    if (info.argsPackElementKind == LocalInfo::Kind::Array ||
        info.argsPackElementKind == LocalInfo::Kind::Vector ||
        info.argsPackElementKind == LocalInfo::Kind::Buffer) {
      return ReceiverShapeCategory::ArrayVector;
    }
    return ReceiverShapeCategory::Other;
  }
  if (hasKeyValueKinds(info)) {
    return ReceiverShapeCategory::KeyValue;
  }
  if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
      info.kind == LocalInfo::Kind::Buffer || info.isSoaVector ||
      info.referenceToArray || info.pointerToArray || info.referenceToVector ||
      info.pointerToVector || info.referenceToBuffer || info.pointerToBuffer) {
    return ReceiverShapeCategory::ArrayVector;
  }
  if (info.kind == LocalInfo::Kind::Value && info.valueKind == LocalInfo::ValueKind::String) {
    return ReceiverShapeCategory::String;
  }
  return ReceiverShapeCategory::Other;
}

} // namespace ir_lowerer_setup_type_return_kind_helpers_file_local
} // namespace primec::ir_lowerer
