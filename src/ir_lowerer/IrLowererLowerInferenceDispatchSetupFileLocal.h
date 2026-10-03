#pragma once

// Helpers shared by the IrLowererLowerInferenceDispatchSetup*.cpp units (split out of
// IrLowererLowerInferenceDispatchSetup.cpp without changes).
#include "primec/ir_lowerer/IrLowererLowerInferenceSetup.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"
#include <vector>

namespace primec::ir_lowerer {

namespace ir_lowerer_lower_inference_dispatch_setup_file_local {

inline std::string resolveDispatchSetupSemanticFactTypeText(
    const SemanticProgram &semanticProgram,
    const std::string &typeText,
    SymbolId typeTextId) {
  if (typeTextId != InvalidSymbolId) {
    std::string resolvedTypeText = std::string(
        semanticProgramResolveCallTargetString(semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

inline bool inferDispatchSetupResultValueKindFromValueTypeText(const std::string &valueTypeText,
                                                        LocalInfo::ValueKind &kindOut) {
  const std::string trimmedValueType = trimTemplateTypeText(valueTypeText);
  if (trimmedValueType.empty()) {
    return false;
  }
  kindOut = valueKindFromTypeName(trimmedValueType);
  if (kindOut == LocalInfo::ValueKind::Unknown) {
    kindOut = LocalInfo::ValueKind::Int64;
  }
  return true;
}

inline bool inferDispatchSetupResultValueKindFromResultTypeText(const std::string &typeText,
                                                         LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  const std::string trimmedType = trimTemplateTypeText(typeText);
  if (!parseResultTypeName(trimmedType, resultHasValue, resultValueKind, resultErrorType)) {
    return false;
  }
  if (!resultHasValue) {
    kindOut = LocalInfo::ValueKind::Int32;
    return true;
  }
  if (resultValueKind != LocalInfo::ValueKind::Unknown) {
    kindOut = resultValueKind;
    return true;
  }
  std::string base;
  std::string argText;
  std::vector<std::string> resultArgs;
  if (!splitTemplateTypeName(trimmedType, base, argText) ||
      !splitTemplateArgs(argText, resultArgs) || resultArgs.size() != 2) {
    return false;
  }
  return inferDispatchSetupResultValueKindFromValueTypeText(resultArgs.front(), kindOut);
}

inline LocalInfo::ValueKind inferDispatchSetupNameValueKindFromTypeText(
    const std::string &typeText) {
  std::string normalizedType = trimTemplateTypeText(typeText);
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  std::string resultErrorType;
  if (parseResultTypeName(
          normalizedType, resultHasValue, resultValueKind, resultErrorType)) {
    return resultHasValue ? LocalInfo::ValueKind::Int64
                          : LocalInfo::ValueKind::Int32;
  }
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    std::vector<std::string> args;
    if ((base == "Reference" || base == "Pointer") &&
        splitTemplateArgs(argText, args) && args.size() == 1) {
      return valueKindFromTypeName(trimTemplateTypeText(args.front()));
    }
    if (base == "Result") {
      return splitTemplateArgs(argText, args) && args.size() == 1
                 ? LocalInfo::ValueKind::Int32
                 : LocalInfo::ValueKind::Int64;
    }
    if (base == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    return valueKindFromTypeName(base);
  }
  return valueKindFromTypeName(normalizeCollectionBindingTypeName(normalizedType));
}

inline bool isDispatchSetupResultTypeMethodCall(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
         expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result" &&
         (expr.name == "ok" || expr.name == "error" || expr.name == "why");
}

inline bool hasSemanticProductDispatchResultMethodFactContext(const Expr &expr,
                                                       const SemanticProgram *semanticProgram,
                                                       const SemanticProductIndex *semanticIndex) {
  if (semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0) {
    return true;
  }
  return semanticProgram != nullptr && semanticIndex != nullptr && expr.name == "ok" &&
         expr.args.size() > 1 && expr.args[1].semanticNodeId != 0;
}

inline bool isDispatchSetupFileErrorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "FileError" || normalized == "std/file/FileError";
}

inline bool isDispatchSetupFileHandleMethodName(const std::string &methodName) {
  return methodName == "write" || methodName == "write_line" ||
         methodName == "write_byte" || methodName == "write_bytes" ||
         methodName == "flush" || methodName == "close";
}

inline bool isDispatchSetupFileHandleTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return false;
  }
  base = trimTemplateTypeText(base);
  if (!base.empty() && base.front() == '/') {
    base.erase(base.begin());
  }
  return base == "File" || base == "std/file/File";
}

inline bool resolveDispatchSetupSemanticReceiverTypeText(const Expr &receiver,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  std::string &typeTextOut);

inline const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataForDispatchSetup() {
  return keyValueHelperSurfaceMetadata();
}

inline std::string canonicalKeyValueHelperPathForDispatchSetup(std::string_view helperName) {
  const StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataForDispatchSetup();
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
}

inline bool isDispatchSetupKeyValueFamilyText(const std::string &familyText) {
  std::string normalized = trimTemplateTypeText(familyText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return isBuiltinCollectionTypeName(normalized, "map") ||
         isExperimentalCollectionTypeName(normalized, "map", "Map");
}

inline bool inferDispatchSetupKeyValueKindsFromTypeText(const std::string &typeText,
                                                 LocalInfo::ValueKind &keyKindOut,
                                                 LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText) ||
      !isDispatchSetupKeyValueFamilyText(base)) {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 2) {
    return false;
  }
  keyKindOut = valueKindFromTypeName(trimTemplateTypeText(args.front()));
  valueKindOut = valueKindFromTypeName(trimTemplateTypeText(args.back()));
  return keyKindOut != LocalInfo::ValueKind::Unknown &&
         valueKindOut != LocalInfo::ValueKind::Unknown;
}

inline bool inferDispatchSetupSemanticKeyValueReceiverKind(const Expr &receiver,
                                                    bool containsResult,
                                                    const SemanticProgram *semanticProgram,
                                                    const SemanticProductIndex *semanticIndex,
                                                    LocalInfo::ValueKind &kindOut,
                                                    bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      receiver.semanticNodeId == 0) {
    return false;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, receiver)) {
    hasSemanticReceiverOut = true;
    const std::string familyText = resolveDispatchSetupSemanticFactTypeText(
        *semanticProgram,
        collectionFact->collectionFamily,
        collectionFact->collectionFamilyId);
    if (!isDispatchSetupKeyValueFamilyText(familyText)) {
      return false;
    }
    const LocalInfo::ValueKind keyKind = valueKindFromTypeName(
        resolveDispatchSetupSemanticFactTypeText(
            *semanticProgram,
            collectionFact->keyTypeText,
            collectionFact->keyTypeTextId));
    const LocalInfo::ValueKind valueKind = valueKindFromTypeName(
        resolveDispatchSetupSemanticFactTypeText(
            *semanticProgram,
            collectionFact->valueTypeText,
            collectionFact->valueTypeTextId));
    if (keyKind == LocalInfo::ValueKind::Unknown ||
        valueKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    kindOut = containsResult ? LocalInfo::ValueKind::Bool : valueKind;
    return true;
  }
  std::string receiverType;
  if (!resolveDispatchSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  if (!inferDispatchSetupKeyValueKindsFromTypeText(receiverType, keyKind, valueKind)) {
    return false;
  }
  kindOut = containsResult ? LocalInfo::ValueKind::Bool : valueKind;
  return true;
}

inline bool inferDispatchSetupSemanticFileHandleCallKind(const Expr &expr,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  LocalInfo::ValueKind &kindOut,
                                                  bool &hasSemanticCallOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticCallOut = false;
  if (!isFileHandleCall(expr) || semanticProgram == nullptr ||
      semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  const auto *queryFact =
      findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
  if (queryFact == nullptr) {
    return false;
  }
  hasSemanticCallOut = true;
  std::string callType = resolveDispatchSetupSemanticFactTypeText(
      *semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
  if (callType.empty()) {
    callType = resolveDispatchSetupSemanticFactTypeText(
        *semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
  }
  if (!isDispatchSetupFileHandleTypeText(callType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Int64;
  return true;
}

inline bool resolveDispatchSetupSemanticReceiverTypeText(const Expr &receiver,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  std::string &typeTextOut) {
  typeTextOut.clear();
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      receiver.semanticNodeId == 0) {
    return false;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFactBySemanticId(*semanticIndex, receiver)) {
    typeTextOut = resolveDispatchSetupSemanticFactTypeText(
        *semanticProgram,
        queryFact->queryTypeText,
        queryFact->queryTypeTextId);
    if (typeTextOut.empty()) {
      typeTextOut = resolveDispatchSetupSemanticFactTypeText(
          *semanticProgram,
          queryFact->bindingTypeText,
          queryFact->bindingTypeTextId);
    }
    return true;
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, receiver)) {
    typeTextOut = resolveDispatchSetupSemanticFactTypeText(
        *semanticProgram,
        bindingFact->bindingTypeText,
        bindingFact->bindingTypeTextId);
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, receiver)) {
    typeTextOut = resolveDispatchSetupSemanticFactTypeText(
        *semanticProgram,
        localAutoFact->bindingTypeText,
        localAutoFact->bindingTypeTextId);
    return true;
  }
  return false;
}

inline const SemanticProgramBindingFact *findDispatchSetupSemanticBindingFactForExpr(
    const Expr &expr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return nullptr;
  }
  if (expr.semanticNodeId != 0) {
    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, expr)) {
      return bindingFact;
    }
  }
  if (expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return nullptr;
  }
  for (const SemanticProgramBindingFact &candidate : semanticProgram->bindingFacts) {
    if (candidate.name == expr.name &&
        candidate.sourceLine == expr.sourceLine &&
        candidate.sourceColumn == expr.sourceColumn) {
      return &candidate;
    }
  }
  return nullptr;
}

inline bool inferDispatchSetupSemanticBindingFactKind(const Expr &expr,
                                               const SemanticProgram *semanticProgram,
                                               const SemanticProductIndex *semanticIndex,
                                               LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  const auto *bindingFact =
      findDispatchSetupSemanticBindingFactForExpr(expr, semanticProgram, semanticIndex);
  if (bindingFact == nullptr || semanticProgram == nullptr) {
    return false;
  }
  kindOut = valueKindFromTypeName(
      resolveDispatchSetupSemanticFactTypeText(
          *semanticProgram,
          bindingFact->bindingTypeText,
          bindingFact->bindingTypeTextId));
  return kindOut != LocalInfo::ValueKind::Unknown;
}

inline bool inferDispatchSetupSemanticNameKind(const Expr &expr,
                                        const SemanticProgram *semanticProgram,
                                        const SemanticProductIndex *semanticIndex,
                                        LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  std::string typeText;
  if (!resolveDispatchSetupSemanticReceiverTypeText(
          expr, semanticProgram, semanticIndex, typeText)) {
    return false;
  }
  kindOut = inferDispatchSetupNameValueKindFromTypeText(typeText);
  return true;
}

inline bool inferDispatchSetupSemanticFileErrorWhyKind(const Expr &receiver,
                                                const SemanticProgram *semanticProgram,
                                                const SemanticProductIndex *semanticIndex,
                                                LocalInfo::ValueKind &kindOut,
                                                bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  std::string receiverType;
  if (!resolveDispatchSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  if (!isDispatchSetupFileErrorTypeText(receiverType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::String;
  return true;
}

inline bool inferDispatchSetupSemanticDereferencedFileErrorWhyKind(
    const Expr &receiver,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    LocalInfo::ValueKind &kindOut,
    bool &hasSemanticTargetOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticTargetOut = false;
  if (!isSimpleCallName(receiver, "dereference") || receiver.args.size() != 1) {
    return false;
  }

  bool hasSemanticTarget = false;
  if (inferDispatchSetupSemanticFileErrorWhyKind(receiver.args.front(),
                                                semanticProgram,
                                                semanticIndex,
                                                kindOut,
                                                hasSemanticTarget)) {
    hasSemanticTargetOut = true;
    return true;
  }
  hasSemanticTargetOut = hasSemanticTarget;
  return false;
}

inline bool inferDispatchSetupSemanticFileHandleMethodKind(const Expr &receiver,
                                                    const std::string &methodName,
                                                    const SemanticProgram *semanticProgram,
                                                    const SemanticProductIndex *semanticIndex,
                                                    LocalInfo::ValueKind &kindOut,
                                                    bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  if (!isDispatchSetupFileHandleMethodName(methodName)) {
    return false;
  }
  std::string receiverType;
  if (!resolveDispatchSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  if (!isDispatchSetupFileHandleTypeText(receiverType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Int32;
  return true;
}

inline bool inferDispatchSetupSemanticDereferencedFileHandleMethodKind(
    const Expr &receiver,
    const std::string &methodName,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    LocalInfo::ValueKind &kindOut,
    bool &hasSemanticTargetOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticTargetOut = false;
  if (!isSimpleCallName(receiver, "dereference") || receiver.args.size() != 1) {
    return false;
  }

  bool hasSemanticTarget = false;
  if (inferDispatchSetupSemanticFileHandleMethodKind(receiver.args.front(),
                                                    methodName,
                                                    semanticProgram,
                                                    semanticIndex,
                                                    kindOut,
                                                    hasSemanticTarget)) {
    hasSemanticTargetOut = true;
    return true;
  }
  hasSemanticTargetOut = hasSemanticTarget;
  return false;
}

inline bool inferDispatchSetupSemanticTryOperandResultKind(const Expr &operand,
                                                    const SemanticProgram *semanticProgram,
                                                    const SemanticProductIndex *semanticIndex,
                                                    LocalInfo::ValueKind &kindOut,
                                                    bool &hasSemanticOperandOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticOperandOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || operand.semanticNodeId == 0) {
    return false;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFactBySemanticId(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    if (queryFact->hasResultType) {
      if (!queryFact->resultTypeHasValue) {
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
      return inferDispatchSetupResultValueKindFromValueTypeText(
          resolveDispatchSetupSemanticFactTypeText(
              *semanticProgram,
              queryFact->resultValueType,
              queryFact->resultValueTypeId),
          kindOut);
    }
    std::string queryType = resolveDispatchSetupSemanticFactTypeText(
        *semanticProgram,
        queryFact->queryTypeText,
        queryFact->queryTypeTextId);
    if (queryType.empty()) {
      queryType = resolveDispatchSetupSemanticFactTypeText(
          *semanticProgram,
          queryFact->bindingTypeText,
          queryFact->bindingTypeTextId);
    }
    return inferDispatchSetupResultValueKindFromResultTypeText(queryType, kindOut);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    return inferDispatchSetupResultValueKindFromResultTypeText(
        resolveDispatchSetupSemanticFactTypeText(
            *semanticProgram,
            bindingFact->bindingTypeText,
            bindingFact->bindingTypeTextId),
        kindOut);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    return inferDispatchSetupResultValueKindFromResultTypeText(
        resolveDispatchSetupSemanticFactTypeText(
            *semanticProgram,
            localAutoFact->bindingTypeText,
            localAutoFact->bindingTypeTextId),
        kindOut);
  }
  return false;
}

inline bool inferDispatchSetupSemanticDereferencedTryOperandResultKind(
    const Expr &operand,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    LocalInfo::ValueKind &kindOut,
    bool &hasSemanticTargetOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticTargetOut = false;
  if (!isSimpleCallName(operand, "dereference") || operand.args.size() != 1) {
    return false;
  }

  bool hasSemanticTarget = false;
  if (inferDispatchSetupSemanticTryOperandResultKind(operand.args.front(),
                                                    semanticProgram,
                                                    semanticIndex,
                                                    kindOut,
                                                    hasSemanticTarget)) {
    hasSemanticTargetOut = true;
    return true;
  }
  hasSemanticTargetOut = hasSemanticTarget;
  return false;
}

} // namespace ir_lowerer_lower_inference_dispatch_setup_file_local
} // namespace primec::ir_lowerer
