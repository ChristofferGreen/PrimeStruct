#pragma once

// Helpers shared by the IrLowererLowerInferenceBaseKindHelpers*.cpp units (split out of
// IrLowererLowerInferenceBaseKindHelpers.cpp without changes, ticket).
#include "primec/ir_lowerer/IrLowererLowerInferenceBaseKindHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererResultHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include <vector>

namespace primec::ir_lowerer {

namespace base_kind_internal {

inline std::string resolveScopedExprPath(const Expr &expr) {
  if (expr.name.empty()) {
    return {};
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return expr.name;
}

inline bool isBaseSetupResultTypeCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call ||
      (expr.name != "ok" && expr.name != "error" && expr.name != "why")) {
    return false;
  }
  if (!expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result") {
    return true;
  }
  std::string normalizedPrefix = expr.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  return normalizedPrefix == "Result";
}

inline bool isBaseSetupResultOrTryCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call) {
    return false;
  }
  if (isSimpleCallName(expr, "try")) {
    return true;
  }
  return isBaseSetupResultTypeCall(expr);
}

inline bool isBaseSetupResultTypeMethodCall(const Expr &expr) {
  return isBaseSetupResultTypeCall(expr);
}

inline LocalInfo::ValueKind inferBaseSetupSimpleExprKind(const Expr &expr,
                                                  const LocalMap &localsIn,
                                                  const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                                  const ResolveCallDefinitionFn *resolveDefinitionCall,
                                                  const LookupReturnInfoFn *lookupReturnInfo,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  const ExprLocalsValueKindFn *fallbackInferExprKind);

inline bool resolveBaseSetupResultExprInfo(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                    const ResolveCallDefinitionFn *resolveDefinitionCall,
                                    const LookupReturnInfoFn *lookupReturnInfo,
                                    const SemanticProgram *semanticProgram,
                                    const SemanticProductIndex *semanticIndex,
                                    const ExprLocalsValueKindFn *fallbackInferExprKind,
                                    ResultExprInfo &out);

inline bool hasSemanticProductResultMethodFactContext(const Expr &expr,
                                               const SemanticProgram *semanticProgram,
                                               const SemanticProductIndex *semanticIndex);

inline bool inferBaseSetupResultTypeCallKind(const Expr &expr,
                                      const LocalMap &localsIn,
                                      const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                      const ResolveCallDefinitionFn *resolveDefinitionCall,
                                      const LookupReturnInfoFn *lookupReturnInfo,
                                      const SemanticProgram *semanticProgram,
                                      const SemanticProductIndex *semanticIndex,
                                      const ExprLocalsValueKindFn *fallbackInferExprKind,
                                      LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (!isBaseSetupResultTypeMethodCall(expr)) {
    return false;
  }
  const bool hasSemanticResultContext =
      hasSemanticProductResultMethodFactContext(expr, semanticProgram, semanticIndex);
  if (hasSemanticResultContext && (expr.name == "error" || expr.name == "why")) {
    return true;
  }
  if (expr.name == "error") {
    kindOut = LocalInfo::ValueKind::Bool;
    return true;
  }
  if (expr.name == "why") {
    kindOut = LocalInfo::ValueKind::String;
    return true;
  }
  const bool hasExplicitResultReceiverArg =
      !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
      expr.args.front().name == "Result";
  const size_t payloadArgCount =
      hasExplicitResultReceiverArg ? expr.args.size() - 1 : expr.args.size();
  ResultExprInfo resultInfo;
  if (expr.name == "ok" && hasSemanticResultContext &&
      resolveBaseSetupResultExprInfo(expr,
                                     localsIn,
                                     resolveMethodCall,
                                     resolveDefinitionCall,
                                     lookupReturnInfo,
                                     semanticProgram,
                                     semanticIndex,
                                     fallbackInferExprKind,
                                     resultInfo) &&
      resultInfo.isResult) {
    if (!resultInfo.hasValue) {
      kindOut = LocalInfo::ValueKind::Int32;
      return true;
    }
    if (resultInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      kindOut = resultInfo.valueKind;
    }
    return true;
  }
  if (expr.name == "ok" && hasSemanticResultContext) {
    return true;
  }
  if (expr.name == "ok") {
    kindOut = payloadArgCount > 0 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    return true;
  }
  return false;
}

inline bool hasSemanticProductBaseSetupSite(const Expr &expr,
                                     const SemanticProgram *semanticProgram,
                                     const SemanticProductIndex *semanticIndex) {
  return semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0;
}

inline bool hasSemanticProductResultMethodFactContext(const Expr &expr,
                                               const SemanticProgram *semanticProgram,
                                               const SemanticProductIndex *semanticIndex) {
  if (hasSemanticProductBaseSetupSite(expr, semanticProgram, semanticIndex)) {
    return true;
  }
  return semanticProgram != nullptr && semanticIndex != nullptr && expr.name == "ok" &&
         expr.args.size() > 1 && expr.args[1].semanticNodeId != 0;
}

inline std::string resolveBaseSetupSemanticFactTypeText(
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

inline bool isBaseSetupFileErrorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "FileError" || normalized == "std/file/FileError";
}

inline bool isBaseSetupFileHandleMethodName(const std::string &methodName) {
  return methodName == "write" || methodName == "write_line" ||
         methodName == "write_byte" || methodName == "read_byte" ||
         methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

inline bool isBaseSetupFileHandleTypeText(const std::string &typeText) {
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

inline bool resolveBaseSetupSemanticReceiverTypeText(
    const Expr &receiver,
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
    typeTextOut = resolveBaseSetupSemanticFactTypeText(
        *semanticProgram,
        queryFact->queryTypeText,
        queryFact->queryTypeTextId);
    if (typeTextOut.empty()) {
      typeTextOut = resolveBaseSetupSemanticFactTypeText(
          *semanticProgram,
          queryFact->bindingTypeText,
          queryFact->bindingTypeTextId);
    }
    return true;
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, receiver)) {
    typeTextOut = resolveBaseSetupSemanticFactTypeText(
        *semanticProgram,
        bindingFact->bindingTypeText,
        bindingFact->bindingTypeTextId);
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, receiver)) {
    typeTextOut = resolveBaseSetupSemanticFactTypeText(
        *semanticProgram,
        localAutoFact->bindingTypeText,
        localAutoFact->bindingTypeTextId);
    return true;
  }
  return false;
}

inline const SemanticProgramQueryFact *findBaseSetupSemanticQueryFactForExpr(
    const Expr &expr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return nullptr;
  }
  if (expr.semanticNodeId != 0) {
    if (const auto *queryFact =
            findSemanticProductQueryFactBySemanticId(*semanticIndex, expr)) {
      return queryFact;
    }
  }
  if (expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
    return nullptr;
  }
  for (const SemanticProgramQueryFact &candidate : semanticProgram->queryFacts) {
    if (candidate.callName == expr.name &&
        candidate.sourceLine == expr.sourceLine &&
        candidate.sourceColumn == expr.sourceColumn) {
      return &candidate;
    }
  }
  return nullptr;
}

inline const SemanticProgramBindingFact *findBaseSetupSemanticBindingFactForExpr(
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

inline bool inferBaseSetupSemanticFileErrorWhyKind(const Expr &receiver,
                                            const SemanticProgram *semanticProgram,
                                            const SemanticProductIndex *semanticIndex,
                                            LocalInfo::ValueKind &kindOut,
                                            bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  std::string receiverType;
  if (!resolveBaseSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  if (!isBaseSetupFileErrorTypeText(receiverType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::String;
  return true;
}

inline bool inferBaseSetupSemanticDereferencedFileErrorWhyKind(
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
  if (inferBaseSetupSemanticFileErrorWhyKind(receiver.args.front(),
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

inline bool inferBaseSetupSemanticFileHandleMethodKind(const Expr &receiver,
                                                const std::string &methodName,
                                                const SemanticProgram *semanticProgram,
                                                const SemanticProductIndex *semanticIndex,
                                                LocalInfo::ValueKind &kindOut,
                                                bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  if (!isBaseSetupFileHandleMethodName(methodName)) {
    return false;
  }
  std::string receiverType;
  if (!resolveBaseSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  if (!isBaseSetupFileHandleTypeText(receiverType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Int32;
  return true;
}

inline bool inferBaseSetupSemanticDereferencedFileHandleMethodKind(
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
  if (inferBaseSetupSemanticFileHandleMethodKind(receiver.args.front(),
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

inline bool inferBaseSetupSemanticFileHandleCallKind(const Expr &expr,
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
  std::string callType = resolveBaseSetupSemanticFactTypeText(
      *semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
  if (callType.empty()) {
    callType = resolveBaseSetupSemanticFactTypeText(
        *semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
  }
  if (!isBaseSetupFileHandleTypeText(callType)) {
    return false;
  }
  kindOut = LocalInfo::ValueKind::Int64;
  return true;
}

inline bool isBaseSetupMapFamilyText(const std::string &familyText) {
  std::string normalized = trimTemplateTypeText(familyText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return isBuiltinCollectionTypeName(normalized, "map") ||
         isExperimentalCollectionTypeName(normalized, "map", "Map");
}

inline bool inferBaseSetupMapKindsFromTypeText(const std::string &typeText,
                                        LocalInfo::ValueKind &keyKindOut,
                                        LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText) ||
      !isBaseSetupMapFamilyText(base)) {
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

inline bool inferBaseSetupSemanticMapTryAtReceiverKind(
    const Expr &receiver,
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
    const std::string familyText = resolveBaseSetupSemanticFactTypeText(
        *semanticProgram,
        collectionFact->collectionFamily,
        collectionFact->collectionFamilyId);
    if (!isBaseSetupMapFamilyText(familyText)) {
      return false;
    }
    const LocalInfo::ValueKind keyKind = valueKindFromTypeName(
        resolveBaseSetupSemanticFactTypeText(
            *semanticProgram,
            collectionFact->keyTypeText,
            collectionFact->keyTypeTextId));
    const LocalInfo::ValueKind valueKind = valueKindFromTypeName(
        resolveBaseSetupSemanticFactTypeText(
            *semanticProgram,
            collectionFact->valueTypeText,
            collectionFact->valueTypeTextId));
    if (keyKind == LocalInfo::ValueKind::Unknown ||
        valueKind == LocalInfo::ValueKind::Unknown) {
      return false;
    }
    kindOut = valueKind;
    return true;
  }
  std::string receiverType;
  if (!resolveBaseSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  if (!inferBaseSetupMapKindsFromTypeText(receiverType, keyKind, valueKind)) {
    return false;
  }
  kindOut = valueKind;
  return true;
}

inline bool inferBaseSetupSemanticFieldAccessKind(const Expr &receiver,
                                           const std::string &fieldName,
                                           const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                                           const SemanticProgram *semanticProgram,
                                           const SemanticProductIndex *semanticIndex,
                                           LocalInfo::ValueKind &kindOut,
                                           bool &hasSemanticReceiverOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticReceiverOut = false;
  std::string receiverType;
  if (!resolveBaseSetupSemanticReceiverTypeText(
          receiver, semanticProgram, semanticIndex, receiverType)) {
    return false;
  }
  hasSemanticReceiverOut = true;
  std::vector<std::string> candidateStructPaths;
  const std::string trimmedReceiverType = trimTemplateTypeText(receiverType);
  if (!trimmedReceiverType.empty()) {
    candidateStructPaths.push_back(trimmedReceiverType);
    if (trimmedReceiverType.front() != '/') {
      candidateStructPaths.push_back("/" + trimmedReceiverType);
    }
  }
  for (const std::string &structPath : candidateStructPaths) {
    StructSlotFieldInfo fieldInfo;
    if (!resolveStructFieldSlot(structPath, fieldName, fieldInfo)) {
      continue;
    }
    kindOut = fieldInfo.structPath.empty() ? fieldInfo.valueKind : LocalInfo::ValueKind::Unknown;
    return true;
  }
  return false;
}

inline bool inferBaseSetupResultValueKindFromValueTypeText(const std::string &valueTypeText,
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

inline bool inferBaseSetupResultValueKindFromResultTypeText(const std::string &typeText,
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
  return inferBaseSetupResultValueKindFromValueTypeText(resultArgs.front(), kindOut);
}

inline bool inferBaseSetupSemanticTryOperandResultKind(const Expr &operand,
                                                const SemanticProgram *semanticProgram,
                                                const SemanticProductIndex *semanticIndex,
                                                LocalInfo::ValueKind &kindOut,
                                                bool &hasSemanticOperandOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticOperandOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      operand.semanticNodeId == 0) {
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
      return inferBaseSetupResultValueKindFromValueTypeText(
          resolveBaseSetupSemanticFactTypeText(
              *semanticProgram,
              queryFact->resultValueType,
              queryFact->resultValueTypeId),
          kindOut);
    }
    std::string queryType = resolveBaseSetupSemanticFactTypeText(
        *semanticProgram,
        queryFact->queryTypeText,
        queryFact->queryTypeTextId);
    if (queryType.empty()) {
      queryType = resolveBaseSetupSemanticFactTypeText(
          *semanticProgram,
          queryFact->bindingTypeText,
          queryFact->bindingTypeTextId);
    }
    return inferBaseSetupResultValueKindFromResultTypeText(queryType, kindOut);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    return inferBaseSetupResultValueKindFromResultTypeText(
        resolveBaseSetupSemanticFactTypeText(
            *semanticProgram,
            bindingFact->bindingTypeText,
            bindingFact->bindingTypeTextId),
        kindOut);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, operand)) {
    hasSemanticOperandOut = true;
    return inferBaseSetupResultValueKindFromResultTypeText(
        resolveBaseSetupSemanticFactTypeText(
            *semanticProgram,
            localAutoFact->bindingTypeText,
            localAutoFact->bindingTypeTextId),
        kindOut);
  }
  return false;
}

inline bool inferBaseSetupSemanticDereferencedTryOperandResultKind(
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
  if (inferBaseSetupSemanticTryOperandResultKind(operand.args.front(),
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

inline bool inferBaseSetupSemanticQueryFactValueKindWithPresence(const Expr &expr,
                                                          const SemanticProgram *semanticProgram,
                                                          const SemanticProductIndex *semanticIndex,
                                                          LocalInfo::ValueKind &kindOut,
                                                          bool &hasSemanticQueryOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticQueryOut = false;
  const auto *queryFact =
      findBaseSetupSemanticQueryFactForExpr(expr, semanticProgram, semanticIndex);
  if (queryFact == nullptr) {
    if (const auto *bindingFact =
            findBaseSetupSemanticBindingFactForExpr(expr, semanticProgram, semanticIndex)) {
      hasSemanticQueryOut = true;
      const LocalInfo::ValueKind semanticBindingKind =
          valueKindFromTypeName(
              resolveBaseSetupSemanticFactTypeText(
                  *semanticProgram,
                  bindingFact->bindingTypeText,
                  bindingFact->bindingTypeTextId));
      if (semanticBindingKind != LocalInfo::ValueKind::Unknown) {
        kindOut = semanticBindingKind;
        return true;
      }
    }
    return false;
  }
  hasSemanticQueryOut = true;
  const LocalInfo::ValueKind queryKind =
      valueKindFromTypeName(
          resolveBaseSetupSemanticFactTypeText(
              *semanticProgram,
              queryFact->queryTypeText,
              queryFact->queryTypeTextId));
  if (queryKind != LocalInfo::ValueKind::Unknown) {
    kindOut = queryKind;
    return true;
  }
  const LocalInfo::ValueKind bindingKind =
      valueKindFromTypeName(
          resolveBaseSetupSemanticFactTypeText(
              *semanticProgram,
              queryFact->bindingTypeText,
              queryFact->bindingTypeTextId));
  if (bindingKind != LocalInfo::ValueKind::Unknown) {
    kindOut = bindingKind;
    return true;
  }
  if (const auto *bindingFact =
          findBaseSetupSemanticBindingFactForExpr(expr, semanticProgram, semanticIndex)) {
    const LocalInfo::ValueKind semanticBindingKind =
        valueKindFromTypeName(
            resolveBaseSetupSemanticFactTypeText(
                *semanticProgram,
                bindingFact->bindingTypeText,
                bindingFact->bindingTypeTextId));
    if (semanticBindingKind != LocalInfo::ValueKind::Unknown) {
      kindOut = semanticBindingKind;
      return true;
    }
  }
  return false;
}

inline bool inferBaseSetupSemanticQueryFactValueKind(const Expr &expr,
                                              const SemanticProgram *semanticProgram,
                                              const SemanticProductIndex *semanticIndex,
                                              LocalInfo::ValueKind &kindOut) {
  bool hasSemanticQuery = false;
  return inferBaseSetupSemanticQueryFactValueKindWithPresence(
      expr, semanticProgram, semanticIndex, kindOut, hasSemanticQuery);
}

inline std::string resolveBaseSetupSemanticTryValueTypeText(
    const SemanticProgram &semanticProgram,
    const SemanticProgramTryFact &tryFact) {
  if (tryFact.valueTypeId != InvalidSymbolId) {
    const std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(
            semanticProgram,
            tryFact.valueTypeId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(tryFact.valueType);
}

inline bool inferBaseSetupSemanticTryFactValueKind(const Expr &expr,
                                            const SemanticProgram *semanticProgram,
                                            const SemanticProductIndex *semanticIndex,
                                            LocalInfo::ValueKind &kindOut,
                                            bool &hasSemanticTrySiteOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  hasSemanticTrySiteOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0 ||
      expr.kind != Expr::Kind::Call || !isSimpleCallName(expr, "try")) {
    return false;
  }
  hasSemanticTrySiteOut = true;
  const auto *tryFact = findSemanticProductTryFactBySemanticId(*semanticIndex, expr);
  if (tryFact == nullptr) {
    return false;
  }
  const std::string valueTypeText =
      resolveBaseSetupSemanticTryValueTypeText(*semanticProgram, *tryFact);
  kindOut = valueKindFromTypeName(valueTypeText);
  if (kindOut == LocalInfo::ValueKind::Unknown && !valueTypeText.empty()) {
    kindOut = LocalInfo::ValueKind::Int64;
  }
  return kindOut != LocalInfo::ValueKind::Unknown;
}

inline bool resolveBaseSetupResultExprInfo(const Expr &expr,
                                    const LocalMap &localsIn,
                                    const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                    const ResolveCallDefinitionFn *resolveDefinitionCall,
                                    const LookupReturnInfoFn *lookupReturnInfo,
                                    const SemanticProgram *semanticProgram,
                                    const SemanticProductIndex *semanticIndex,
                                    const ExprLocalsValueKindFn *fallbackInferExprKind,
                                    ResultExprInfo &out) {
  const ResolveMethodCallWithLocalsFn noopResolveMethodCall =
      [](const Expr &, const LocalMap &) -> const Definition * { return nullptr; };
  const ResolveCallDefinitionFn noopResolveDefinitionCall = [](const Expr &) -> const Definition * { return nullptr; };
  const LookupReturnInfoFn noopLookupReturnInfo = [](const std::string &, ReturnInfo &) { return false; };
  const ResolveMethodCallWithLocalsFn &resolveMethodCallFn =
      (resolveMethodCall != nullptr && *resolveMethodCall) ? *resolveMethodCall : noopResolveMethodCall;
  const ResolveCallDefinitionFn &resolveDefinitionCallFn =
      (resolveDefinitionCall != nullptr && *resolveDefinitionCall) ? *resolveDefinitionCall : noopResolveDefinitionCall;
  const LookupReturnInfoFn &lookupReturnInfoFn =
      (lookupReturnInfo != nullptr && *lookupReturnInfo) ? *lookupReturnInfo : noopLookupReturnInfo;
  return resolveResultExprInfoFromLocals(
      expr,
      localsIn,
      resolveMethodCallFn,
      resolveDefinitionCallFn,
      lookupReturnInfoFn,
      [resolveMethodCall, resolveDefinitionCall, lookupReturnInfo, semanticProgram, semanticIndex, fallbackInferExprKind](
          const Expr &candidate, const LocalMap &candidateLocals) {
        return inferBaseSetupSimpleExprKind(
            candidate,
            candidateLocals,
            resolveMethodCall,
            resolveDefinitionCall,
            lookupReturnInfo,
            semanticProgram,
            semanticIndex,
            fallbackInferExprKind);
      },
      out,
      semanticProgram,
      semanticIndex);
}

inline LocalInfo::ValueKind inferBaseSetupSimpleExprKind(const Expr &expr,
                                                  const LocalMap &localsIn,
                                                  const ResolveMethodCallWithLocalsFn *resolveMethodCall,
                                                  const ResolveCallDefinitionFn *resolveDefinitionCall,
                                                  const LookupReturnInfoFn *lookupReturnInfo,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  const ExprLocalsValueKindFn *fallbackInferExprKind) {
  switch (expr.kind) {
    case Expr::Kind::Literal:
      if (expr.isUnsigned) {
        return LocalInfo::ValueKind::UInt64;
      }
      return (expr.intWidth == 64) ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
    case Expr::Kind::FloatLiteral:
      return (expr.floatWidth == 64) ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
    case Expr::Kind::BoolLiteral:
      return LocalInfo::ValueKind::Bool;
    case Expr::Kind::StringLiteral:
      return LocalInfo::ValueKind::String;
    case Expr::Kind::Name: {
      auto it = localsIn.find(expr.name);
      if (it == localsIn.end()) {
        return LocalInfo::ValueKind::Unknown;
      }
      if (it->second.kind == LocalInfo::Kind::Value) {
        return it->second.valueKind;
      }
      if (it->second.kind == LocalInfo::Kind::Reference && !it->second.referenceToArray &&
          !it->second.referenceToVector && it->second.structTypeName.empty()) {
        return it->second.valueKind;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    case Expr::Kind::Call: {
      if (!expr.isMethodCall && expr.args.empty() && expr.templateArgs.empty() &&
          !expr.hasBodyArguments && expr.bodyArguments.empty()) {
        const std::string resolvedPrimitivePath = resolveScopedExprPath(expr);
        const size_t slash = resolvedPrimitivePath.find_last_of('/');
        const std::string leaf = slash == std::string::npos
                                     ? resolvedPrimitivePath
                                     : resolvedPrimitivePath.substr(slash + 1);
        if (leaf == "int" || leaf == "i32" || leaf == "i64" ||
            leaf == "u64" || leaf == "float" || leaf == "f32" ||
            leaf == "f64" || leaf == "bool") {
          return valueKindFromTypeName(leaf);
        }
      }
      LocalInfo::ValueKind kindOut = LocalInfo::ValueKind::Unknown;
      if (inferBaseSetupSemanticQueryFactValueKind(expr, semanticProgram, semanticIndex, kindOut)) {
        return kindOut;
      }
      if (inferBaseSetupResultTypeCallKind(expr,
                                           localsIn,
                                           resolveMethodCall,
                                           resolveDefinitionCall,
                                           lookupReturnInfo,
                                           semanticProgram,
                                           semanticIndex,
                                           fallbackInferExprKind,
                                           kindOut)) {
        return kindOut;
      }
      bool hasSemanticTrySite = false;
      if (inferBaseSetupSemanticTryFactValueKind(
              expr, semanticProgram, semanticIndex, kindOut, hasSemanticTrySite)) {
        return kindOut;
      }
      if (hasSemanticTrySite) {
        return LocalInfo::ValueKind::Unknown;
      }
      ResultExprInfo resultInfo;
      if (resolveBaseSetupResultExprInfo(
              expr,
              localsIn,
              resolveMethodCall,
              resolveDefinitionCall,
              lookupReturnInfo,
              semanticProgram,
              semanticIndex,
              fallbackInferExprKind,
              resultInfo) &&
          resultInfo.isResult &&
          resultInfo.hasValue) {
        return resultInfo.valueKind;
      }
      const bool hasSemanticProductQuerySite =
          semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0;
      if (!hasSemanticProductQuerySite && fallbackInferExprKind != nullptr && *fallbackInferExprKind &&
          !isBaseSetupResultOrTryCall(expr)) {
        kindOut = (*fallbackInferExprKind)(expr, localsIn);
        if (kindOut != LocalInfo::ValueKind::Unknown) {
          return kindOut;
        }
      }
      std::string builtinComparison;
      if (getBuiltinComparisonName(expr, builtinComparison)) {
        return LocalInfo::ValueKind::Bool;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    default:
      return LocalInfo::ValueKind::Unknown;
  }
}

} // namespace base_kind_internal
} // namespace primec::ir_lowerer
