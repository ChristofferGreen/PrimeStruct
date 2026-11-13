#pragma once

#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include <vector>

namespace primec::ir_lowerer {

inline const SemanticProgramBindingFact *
findInferenceSemanticBindingFactForExpr(const Expr &expr,
                                        const SemanticProgram *semanticProgram,
                                        const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return nullptr;
  }
  if (expr.semanticNodeId != 0) {
    if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, expr)) {
      return bindingFact;
    }
  }
  if (expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return nullptr;
  }
  for (const SemanticProgramBindingFact &candidate : semanticProgram->bindingFacts) {
    if (candidate.name == expr.name && candidate.sourceLine == expr.sourceLine &&
        candidate.sourceColumn == expr.sourceColumn) {
      return &candidate;
    }
  }
  return nullptr;
}

inline bool isInferenceFileErrorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "FileError" || normalized == "std/file/FileError";
}

inline bool isInferenceFileHandleTypeText(const std::string &typeText) {
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

inline std::string resolveInferenceSemanticFactTypeText(const SemanticProgram &semanticProgram,
                                                        const std::string &typeText,
                                                        SymbolId typeTextId) {
  if (typeTextId != InvalidSymbolId) {
    std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

inline bool isInferenceFileHandleMethodName(const std::string &methodName) {
  return methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
         methodName == "read_byte" || methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

inline bool resolveInferenceSemanticReceiverTypeText(const Expr &receiver,
                                                     const SemanticProgram *semanticProgram,
                                                     const SemanticProductIndex *semanticIndex,
                                                     std::string &typeTextOut) {
  typeTextOut.clear();
  if (semanticProgram == nullptr || semanticIndex == nullptr || receiver.semanticNodeId == 0) {
    return false;
  }
  if (const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, receiver)) {
    typeTextOut = resolveInferenceSemanticFactTypeText(
        *semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (typeTextOut.empty()) {
      typeTextOut = resolveInferenceSemanticFactTypeText(
          *semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
    }
    return true;
  }
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, receiver)) {
    typeTextOut = resolveInferenceSemanticFactTypeText(
        *semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, receiver)) {
    typeTextOut = resolveInferenceSemanticFactTypeText(
        *semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
    return true;
  }
  return false;
}

inline bool inferInferenceResultValueKindFromValueTypeText(const std::string &valueTypeText,
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

inline bool inferInferenceResultValueKindFromResultTypeText(const std::string &typeText,
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
  return inferInferenceResultValueKindFromValueTypeText(resultArgs.front(), kindOut);
}

inline bool inferInferenceSemanticFileErrorWhyKind(const Expr &receiver,
                                                   const SemanticProgram *semanticProgram,
                                                   const SemanticProductIndex *semanticIndex,
                                                   LocalInfo::ValueKind &kindOut,
                                                   bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  std::string receiverType;
  if (!resolveInferenceSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  if (!isInferenceFileErrorTypeText(receiverType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::String;
  return true;
}

inline bool inferInferenceSemanticFileHandleCallKind(const Expr &expr,
                                                     const SemanticProgram *semanticProgram,
                                                     const SemanticProductIndex *semanticIndex,
                                                     LocalInfo::ValueKind &kindOut,
                                                     bool &hasSemanticCallOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticCallOut = false;
  if (!isFileHandleCall(expr) || semanticProgram == nullptr || semanticIndex == nullptr ||
      expr.semanticNodeId == 0) {
    return false;
  }
  const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
  if (queryFact == nullptr) {
    return false;
  }
  hasSemanticCallOut = true;
  std::string callType = resolveInferenceSemanticFactTypeText(
      *semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
  if (callType.empty()) {
    callType = resolveInferenceSemanticFactTypeText(
        *semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
  }
  if (!isInferenceFileHandleTypeText(callType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Int64;
  return true;
}

inline bool inferInferenceSemanticFileHandleMethodKind(const Expr &receiver,
                                                       const std::string &methodName,
                                                       const SemanticProgram *semanticProgram,
                                                       const SemanticProductIndex *semanticIndex,
                                                       LocalInfo::ValueKind &kindOut,
                                                       bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  if (!isInferenceFileHandleMethodName(methodName)) {
    return false;
  }
  std::string receiverType;
  if (!resolveInferenceSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  if (!isInferenceFileHandleTypeText(receiverType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Int32;
  return true;
}

inline bool inferInferenceSemanticTryOperandResultKind(const Expr &operand,
                                                       const SemanticProgram *semanticProgram,
                                                       const SemanticProductIndex *semanticIndex,
                                                       LocalInfo::ValueKind &kindOut,
                                                       bool &hasSemanticOperandOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticOperandOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || operand.semanticNodeId == 0) {
    return false;
  }
  if (const auto *queryFact = findSemanticProductQueryFactBySemanticId(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    if (queryFact->hasResultType) {
      if (!queryFact->resultTypeHasValue) {
        kindOut = LocalInfo::ValueKind::Int32;
        return true;
      }
      return inferInferenceResultValueKindFromValueTypeText(
          resolveInferenceSemanticFactTypeText(
              *semanticProgram, queryFact->resultValueType, queryFact->resultValueTypeId),
          kindOut);
    }
    std::string queryType = resolveInferenceSemanticFactTypeText(
        *semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (queryType.empty()) {
      queryType = resolveInferenceSemanticFactTypeText(
          *semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
    }
    return inferInferenceResultValueKindFromResultTypeText(queryType, kindOut);
  }
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    return inferInferenceResultValueKindFromResultTypeText(
        resolveInferenceSemanticFactTypeText(
            *semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId),
        kindOut);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    return inferInferenceResultValueKindFromResultTypeText(
        resolveInferenceSemanticFactTypeText(
            *semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId),
        kindOut);
  }
  return false;
}

inline bool
inferInferenceSemanticDereferencedFileErrorWhyKind(const Expr &receiver,
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
  if (inferInferenceSemanticFileErrorWhyKind(
          receiver.args.front(), semanticProgram, semanticIndex, kindOut, hasSemanticTarget)) {
    hasSemanticTargetOut = true;
    return true;
  }
  hasSemanticTargetOut = hasSemanticTarget;
  return false;
}

inline bool
inferInferenceSemanticDereferencedFileHandleMethodKind(const Expr &receiver,
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
  if (inferInferenceSemanticFileHandleMethodKind(receiver.args.front(),
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

inline bool
inferInferenceSemanticDereferencedTryOperandResultKind(const Expr &operand,
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
  if (inferInferenceSemanticTryOperandResultKind(
          operand.args.front(), semanticProgram, semanticIndex, kindOut, hasSemanticTarget)) {
    hasSemanticTargetOut = true;
    return true;
  }
  hasSemanticTargetOut = hasSemanticTarget;
  return false;
}

} // namespace primec::ir_lowerer
