#pragma once

// Helpers shared by the IrLowererResultHelpers*.cpp units (split out of
// IrLowererResultHelpers.cpp without changes, ticket).
#include "IrLowererResultInternal.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererLowerInferenceBaseKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace result_helpers_file_local {

inline bool isResultBuiltinCall(const Expr &expr, const std::string &name, size_t argCount) {
  return expr.kind == Expr::Kind::Call && expr.name == name && expr.args.size() == argCount &&
         !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result";
}

inline void assignSemanticResultError(std::string *errorOut, const std::string &message) {
  if (errorOut != nullptr && errorOut->empty()) {
    *errorOut = message;
  }
}

inline std::string describeSemanticResultCall(const Expr &expr) {
  return expr.name.empty() ? "<call>" : expr.name;
}

inline std::string resolveScopedExprPath(const Expr &expr) {
  if (expr.name.empty()) {
    return {};
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return expr.name;
}

inline bool isSemanticFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string args;
  return splitTemplateTypeName(trimTemplateTypeText(typeText), base, args) &&
         normalizeCollectionBindingTypeName(base) == "File";
}

inline bool applySemanticResultValueTypeText(const std::string &valueTypeText, ResultExprInfo &out) {
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
  const auto *containerErrorMetadata =
      findStdlibSurfaceMetadata(StdlibSurfaceId::CollectionsContainerErrorHelpers);
  if (containerErrorMetadata != nullptr &&
      stdlibSurfaceMatchesSpelling(*containerErrorMetadata, trimmedValueType)) {
    out.valueStructType = std::string(containerErrorMetadata->canonicalPath);
    return true;
  }
  if (trimmedValueType == "ImageError" ||
      trimmedValueType == "/std/image/ImageError") {
    out.valueStructType = "/std/image/ImageError";
    return true;
  }
  if (trimmedValueType == "GfxError" ||
      trimmedValueType == "/std/gfx/GfxError" ||
      trimmedValueType == "/std/gfx/experimental/GfxError") {
    out.valueStructType = trimmedValueType == "/std/gfx/experimental/GfxError"
                              ? "/std/gfx/experimental/GfxError"
                              : "/std/gfx/GfxError";
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

inline bool applySemanticPublishedResultSumTypeText(const SemanticProgram *semanticProgram,
                                             const std::string &typeText,
                                             ResultExprInfo &out) {
  if (semanticProgram == nullptr) {
    return false;
  }
  std::string normalizedType = trimTemplateTypeText(typeText);
  if (!normalizedType.empty() && normalizedType.front() != '/') {
    normalizedType.insert(normalizedType.begin(), '/');
  }
  if (normalizedType.rfind("/std/result/Result__", 0) != 0) {
    return false;
  }

  const SemanticProgramSumVariantMetadata *okVariant = nullptr;
  const SemanticProgramSumVariantMetadata *errorVariant = nullptr;
  for (const auto &variant : semanticProgram->sumVariantMetadata) {
    if (variant.sumPath != normalizedType) {
      continue;
    }
    if (variant.variantName == "ok") {
      okVariant = &variant;
    } else if (variant.variantName == "error") {
      errorVariant = &variant;
    }
  }
  if (errorVariant == nullptr || !errorVariant->hasPayload) {
    return false;
  }

  out = ResultExprInfo{};
  out.isResult = true;
  out.hasValue = okVariant != nullptr && okVariant->hasPayload;
  out.errorType = trimTemplateTypeText(errorVariant->payloadTypeText);
  if (!out.hasValue) {
    return true;
  }
  return applySemanticResultValueTypeText(okVariant->payloadTypeText, out);
}

inline bool applySemanticResultTypeText(const std::string &typeText,
                                 ResultExprInfo &out,
                                 const SemanticProgram *semanticProgram = nullptr,
                                 bool allowPointerLikeResultTarget = false) {
  const std::string trimmedType = trimTemplateTypeText(typeText);
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  if (!parseResultTypeName(trimmedType, resultHasValue, resultValueKind, resultErrorType)) {
    if (applySemanticPublishedResultSumTypeText(semanticProgram, trimmedType, out)) {
      return true;
    }
    if (!allowPointerLikeResultTarget) {
      return false;
    }
    std::string pointerBase;
    std::string pointerArgText;
    if (!splitTemplateTypeName(trimmedType, pointerBase, pointerArgText)) {
      return false;
    }
    pointerBase = trimTemplateTypeText(pointerBase);
    if (pointerBase != "Reference" && pointerBase != "/Reference" &&
        pointerBase != "Pointer" && pointerBase != "/Pointer") {
      return false;
    }
    return applySemanticResultTypeText(pointerArgText, out, semanticProgram, false);
  }
  out = ResultExprInfo{};
  out.isResult = true;
  out.hasValue = resultHasValue;
  out.valueKind = resultValueKind;
  out.errorType = resultErrorType;
  if (!resultHasValue || resultValueKind != LocalInfo::ValueKind::Unknown) {
    return true;
  }

  std::string base;
  std::string argText;
  std::vector<std::string> resultArgs;
  if (!splitTemplateTypeName(trimmedType, base, argText) ||
      !splitTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
    return true;
  }
  return applySemanticResultValueTypeText(resultArgs.front(), out);
}

inline std::string resolveSemanticResultFactText(const SemanticProgram &semanticProgram,
                                          const std::string &text,
                                          SymbolId textId) {
  if (textId != InvalidSymbolId) {
    std::string resolvedText = std::string(
        semanticProgramResolveCallTargetString(semanticProgram, textId));
    if (!resolvedText.empty()) {
      return trimTemplateTypeText(resolvedText);
    }
  }
  return trimTemplateTypeText(text);
}

inline std::string resolveSemanticQueryResultValueTypeText(
    const SemanticProgram &semanticProgram,
    const SemanticProgramQueryFact &queryFact) {
  return resolveSemanticResultFactText(
      semanticProgram,
      queryFact.resultValueType,
      queryFact.resultValueTypeId);
}

inline std::string resolveSemanticQueryResultErrorTypeText(
    const SemanticProgram &semanticProgram,
    const SemanticProgramQueryFact &queryFact) {
  return resolveSemanticResultFactText(
      semanticProgram,
      queryFact.resultErrorType,
      queryFact.resultErrorTypeId);
}

inline bool applySemanticQueryFactResultInfo(const Expr &expr,
                                      const SemanticProgram *semanticProgram,
                                      const SemanticProductIndex *semanticIndex,
                                      std::string *errorOut,
                                      ResultExprInfo &out) {
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
  if (queryFact == nullptr) {
    assignSemanticResultError(
        errorOut, "missing semantic-product query fact: " + describeSemanticResultCall(expr));
    return false;
  }
  if (!queryFact->hasResultType) {
    assignSemanticResultError(
        errorOut, "incomplete semantic-product query fact: " + describeSemanticResultCall(expr));
    return false;
  }
  out.isResult = true;
  out.hasValue = queryFact->resultTypeHasValue;
  out.errorType = resolveSemanticQueryResultErrorTypeText(*semanticProgram, *queryFact);
  if (!out.hasValue) {
    return true;
  }
  if (!applySemanticResultValueTypeText(
          resolveSemanticQueryResultValueTypeText(*semanticProgram, *queryFact), out)) {
    assignSemanticResultError(
        errorOut, "incomplete semantic-product query fact: " + describeSemanticResultCall(expr));
    return false;
  }
  return true;
}

inline bool resolveSemanticQueryResultInfoWithPresence(const Expr &expr,
                                                const SemanticProgram *semanticProgram,
                                                const SemanticProductIndex *semanticIndex,
                                                std::string *errorOut,
                                                ResultExprInfo &out,
                                                bool &hasSemanticQueryOut,
                                                bool allowPointerLikeResultTarget = false) {
  hasSemanticQueryOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
  if (queryFact == nullptr) {
    hasSemanticQueryOut = true;
    assignSemanticResultError(
        errorOut, "missing semantic-product query fact: " + describeSemanticResultCall(expr));
    return false;
  }
  hasSemanticQueryOut = true;
  if (!queryFact->hasResultType) {
    if (allowPointerLikeResultTarget) {
      const std::string bindingTypeText = resolveSemanticResultFactText(
          *semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
      if (!bindingTypeText.empty() &&
          applySemanticResultTypeText(bindingTypeText, out, semanticProgram, true)) {
        return true;
      }
      const std::string queryTypeText = resolveSemanticResultFactText(
          *semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
      if (!queryTypeText.empty() &&
          applySemanticResultTypeText(queryTypeText, out, semanticProgram, true)) {
        return true;
      }
    }
    return false;
  }
  out.isResult = true;
  out.hasValue = queryFact->resultTypeHasValue;
  out.errorType = resolveSemanticQueryResultErrorTypeText(*semanticProgram, *queryFact);
  if (!out.hasValue) {
    return true;
  }
  if (!applySemanticResultValueTypeText(
          resolveSemanticQueryResultValueTypeText(*semanticProgram, *queryFact), out)) {
    assignSemanticResultError(
        errorOut, "incomplete semantic-product query fact: " + describeSemanticResultCall(expr));
    return false;
  }
  return true;
}

inline bool resolveSemanticBindingResultInfoWithPresence(const Expr &expr,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  ResultExprInfo &out,
                                                  bool &hasSemanticBindingOut,
                                                  bool allowPointerLikeResultTarget = false) {
  hasSemanticBindingOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, expr)) {
    hasSemanticBindingOut = true;
    return applySemanticResultTypeText(
        resolveSemanticResultFactText(
            *semanticProgram,
            bindingFact->bindingTypeText,
            bindingFact->bindingTypeTextId),
        out,
        semanticProgram,
        allowPointerLikeResultTarget);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, expr)) {
    hasSemanticBindingOut = true;
    return applySemanticResultTypeText(
        resolveSemanticResultFactText(
            *semanticProgram,
            localAutoFact->bindingTypeText,
            localAutoFact->bindingTypeTextId),
        out,
        semanticProgram,
        allowPointerLikeResultTarget);
  }
  return false;
}

inline bool needsSemanticQueryResultValueMetadata(const Expr &expr,
                                           const SemanticProgram *semanticProgram,
                                           const SemanticProductIndex *semanticIndex,
                                           const ResultExprInfo &out) {
  return semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0 &&
         out.valueKind == LocalInfo::ValueKind::Unknown &&
         out.valueCollectionKind == LocalInfo::Kind::Value &&
         out.valueMapKeyKind == LocalInfo::ValueKind::Unknown &&
         out.valueStructType.empty() &&
         !out.valueIsFileHandle;
}

inline bool validateInternedSemanticTextMetadata(const SemanticProgram &semanticProgram,
                                          SymbolId textId,
                                          std::string_view expectedText,
                                          std::string_view factLabel,
                                          std::string_view fieldLabel,
                                          const std::string &displayName,
                                          std::string &error) {
  if (textId == InvalidSymbolId) {
    return true;
  }
  const std::string_view resolvedText =
      semanticProgramResolveCallTargetString(semanticProgram, textId);
  if (resolvedText.empty()) {
    error = "missing semantic-product " + std::string(factLabel) + " " +
            std::string(fieldLabel) + " id: " + displayName;
    return false;
  }
  if (!expectedText.empty() && resolvedText != expectedText) {
    error = "stale semantic-product " + std::string(factLabel) + " " +
            std::string(fieldLabel) + " metadata: " + displayName;
    return false;
  }
  return true;
}

inline bool isRootBuiltinCountQueryPath(std::string_view path) {
  return path == "count" || path == "/count";
}

inline bool isCoreBuiltinCountTargetPath(std::string_view path) {
  return path == collection_helpers::kRootedArrayCount || path == collection_helpers::kRootedStringCount;
}

inline bool isQueryOwnedBuiltinCountTargetMatch(std::string_view queryCallName,
                                         std::string_view queryResolvedPath,
                                         std::string_view publishedTargetPath) {
  return queryCallName == "count" &&
         ((isRootBuiltinCountQueryPath(queryResolvedPath) &&
           isCoreBuiltinCountTargetPath(publishedTargetPath)) ||
          (isCoreBuiltinCountTargetPath(queryResolvedPath) &&
           isRootBuiltinCountQueryPath(publishedTargetPath)));
}

inline bool isInternalSoaStorageQueryTargetMatch(std::string_view queryCallName,
                                          std::string_view queryResolvedPath,
                                          std::string_view publishedTargetPath) {
  const std::string internalSoaStoragePrefix =
      collection_paths::modulePrefix(collection_paths::kInternalSoaStorageFolder);
  return (queryCallName == "storage" || queryCallName == "field_count" ||
          queryCallName == "field_capacity" || queryCallName == "set_field_count" ||
          queryCallName == "set_field_capacity") &&
         queryResolvedPath.rfind(internalSoaStoragePrefix, 0) == 0 &&
         publishedTargetPath.rfind(internalSoaStoragePrefix, 0) == 0;
}

std::vector<const SemanticProgramReturnFact *>
publishedReturnFactsByDefinitionMap(const SemanticProgram &semanticProgram) {
  std::vector<std::size_t> factIndices;
  factIndices.reserve(
      semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId.size());
  for (const auto &[definitionSemanticId, factIndex] :
       semanticProgram.publishedRoutingLookups.returnFactIndicesByDefinitionId) {
    (void)definitionSemanticId;
    if (factIndex < semanticProgram.returnFacts.size()) {
      factIndices.push_back(factIndex);
    }
  }
  std::sort(factIndices.begin(), factIndices.end());
  factIndices.erase(std::unique(factIndices.begin(), factIndices.end()), factIndices.end());

  std::vector<const SemanticProgramReturnFact *> facts;
  facts.reserve(factIndices.size());
  for (const std::size_t factIndex : factIndices) {
    facts.push_back(&semanticProgram.returnFacts[factIndex]);
  }
  return facts;
}

} // namespace result_helpers_file_local
} // namespace primec::ir_lowerer
