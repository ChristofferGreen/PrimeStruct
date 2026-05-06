#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererHelpers.h"
#include "IrLowererLowerInferenceBaseKindHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <vector>

namespace primec::ir_lowerer {

namespace {

std::string resolveDispatchSetupSemanticFactTypeText(
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

bool inferDispatchSetupResultValueKindFromValueTypeText(const std::string &valueTypeText,
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

bool inferDispatchSetupResultValueKindFromResultTypeText(const std::string &typeText,
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

bool isDispatchSetupResultTypeMethodCall(const Expr &expr) {
  return expr.kind == Expr::Kind::Call && expr.isMethodCall && !expr.args.empty() &&
         expr.args.front().kind == Expr::Kind::Name && expr.args.front().name == "Result" &&
         (expr.name == "ok" || expr.name == "error" || expr.name == "why");
}

bool hasSemanticProductDispatchResultMethodFactContext(const Expr &expr,
                                                       const SemanticProgram *semanticProgram,
                                                       const SemanticProductIndex *semanticIndex) {
  if (semanticProgram != nullptr && semanticIndex != nullptr && expr.semanticNodeId != 0) {
    return true;
  }
  return semanticProgram != nullptr && semanticIndex != nullptr && expr.name == "ok" &&
         expr.args.size() > 1 && expr.args[1].semanticNodeId != 0;
}

bool isDispatchSetupFileErrorTypeText(const std::string &typeText) {
  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "FileError" || normalized == "std/file/FileError";
}

bool isDispatchSetupFileHandleMethodName(const std::string &methodName) {
  return methodName == "write" || methodName == "write_line" ||
         methodName == "write_byte" || methodName == "write_bytes" ||
         methodName == "flush" || methodName == "close";
}

bool isDispatchSetupFileHandleTypeText(const std::string &typeText) {
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

bool resolveDispatchSetupSemanticReceiverTypeText(const Expr &receiver,
                                                  const SemanticProgram *semanticProgram,
                                                  const SemanticProductIndex *semanticIndex,
                                                  std::string &typeTextOut);

bool isDispatchSetupMapFamilyText(const std::string &familyText) {
  std::string normalized = trimTemplateTypeText(familyText);
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized == "map" || normalized == "std/collections/map" ||
         normalized == "Map" ||
         normalized == "std/collections/experimental_map/Map" ||
         normalized.rfind("std/collections/experimental_map/Map__", 0) == 0;
}

bool inferDispatchSetupMapKindsFromTypeText(const std::string &typeText,
                                            LocalInfo::ValueKind &keyKindOut,
                                            LocalInfo::ValueKind &valueKindOut) {
  keyKindOut = LocalInfo::ValueKind::Unknown;
  valueKindOut = LocalInfo::ValueKind::Unknown;
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText) ||
      !isDispatchSetupMapFamilyText(base)) {
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

bool inferDispatchSetupSemanticMapReceiverKind(const Expr &receiver,
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
    if (!isDispatchSetupMapFamilyText(familyText)) {
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
  if (!inferDispatchSetupMapKindsFromTypeText(receiverType, keyKind, valueKind)) {
    return false;
  }
  kindOut = containsResult ? LocalInfo::ValueKind::Bool : valueKind;
  return true;
}

bool inferDispatchSetupSemanticFileHandleCallKind(const Expr &expr,
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

bool resolveDispatchSetupSemanticReceiverTypeText(const Expr &receiver,
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

bool inferDispatchSetupSemanticFileErrorWhyKind(const Expr &receiver,
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

bool inferDispatchSetupSemanticDereferencedFileErrorWhyKind(
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

bool inferDispatchSetupSemanticFileHandleMethodKind(const Expr &receiver,
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

bool inferDispatchSetupSemanticDereferencedFileHandleMethodKind(
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

bool inferDispatchSetupSemanticTryOperandResultKind(const Expr &operand,
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

bool inferDispatchSetupSemanticDereferencedTryOperandResultKind(
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

} // namespace

bool runLowerInferenceExprKindDispatchSetup(const LowerInferenceExprKindDispatchSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut) {
  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: defMap";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: resolveExprPath";
    return false;
  }
  if (input.error == nullptr) {
    errorOut = "native backend missing inference expr-kind dispatch setup dependency: error";
    return false;
  }
  if (!stateInOut.inferLiteralOrNameExprKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferLiteralOrNameExprKind";
    return false;
  }
  if (!stateInOut.inferCallExprBaseKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprBaseKind";
    return false;
  }
  if (!stateInOut.inferCallExprDirectReturnKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprDirectReturnKind";
    return false;
  }
  if (!stateInOut.inferCallExprCountAccessGpuFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprCountAccessGpuFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprOperatorFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprOperatorFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprControlFlowFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprControlFlowFallbackKind";
    return false;
  }
  if (!stateInOut.inferCallExprPointerFallbackKind) {
    errorOut = "native backend missing inference expr-kind dispatch setup state: inferCallExprPointerFallbackKind";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto resolveExprPath = input.resolveExprPath;
  std::string *const inferenceError = input.error;
  const auto *semanticProgram = stateInOut.semanticProgram;
  const auto *semanticIndex = stateInOut.semanticIndex;
  stateInOut.inferExprKind = [defMap, resolveExprPath, inferenceError, semanticProgram, semanticIndex, &stateInOut](
                                 const Expr &expr, const LocalMap &localsIn) -> LocalInfo::ValueKind {
    if (!expr.isMethodCall && expr.kind == Expr::Kind::Call && !expr.args.empty()) {
      std::string canonicalMapHelperName;
      std::string borrowedMapHelperName;
      const bool hasBorrowedMapHelperAlias =
          resolveBorrowedMapHelperAliasName(expr, borrowedMapHelperName);
      if (resolveMapHelperAliasName(expr, canonicalMapHelperName) &&
          (!stateInOut.resolveDefinitionCall || stateInOut.resolveDefinitionCall(expr) == nullptr) &&
          !hasBorrowedMapHelperAlias &&
          (canonicalMapHelperName == "count" || canonicalMapHelperName == "contains" ||
           canonicalMapHelperName == "tryAt" || canonicalMapHelperName == "at" ||
           canonicalMapHelperName == "at_unsafe" ||
           canonicalMapHelperName == "insert" ||
           canonicalMapHelperName == "insert_ref") &&
          !(isExplicitMapHelperFallbackPath(expr) &&
            (canonicalMapHelperName == "at" || canonicalMapHelperName == "at_unsafe" ||
             canonicalMapHelperName == "tryAt")) &&
          ((expr.name.find('/') != std::string::npos) || !expr.namespacePrefix.empty() ||
           !expr.templateArgs.empty())) {
        Expr rewrittenExpr = expr;
        rewrittenExpr.name = canonicalMapHelperName;
        rewrittenExpr.namespacePrefix.clear();
        rewrittenExpr.semanticNodeId = 0;
        rewrittenExpr.templateArgs.clear();
        return stateInOut.inferExprKind(rewrittenExpr, localsIn);
      }
    }
    auto resolveDefinitionCallForResultInfo = [&](const Expr &candidate) -> const Definition * {
      if (candidate.isMethodCall || defMap == nullptr || !resolveExprPath) {
        return nullptr;
      }
      std::string path = resolveExprPath(candidate);
      if (path.empty() && !candidate.name.empty()) {
        path = candidate.name;
        if (!path.empty() && path.front() != '/') {
          path.insert(path.begin(), '/');
        }
      }
      if (path.empty()) {
        return nullptr;
      }
      auto defIt = defMap->find(path);
      return defIt != defMap->end() ? defIt->second : nullptr;
    };
    auto resolveMethodCallDefinitionForResultInfo =
        [&](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
      if (!stateInOut.resolveMethodCallDefinition) {
        return nullptr;
      }
      return stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
    };
    auto inferSemanticResultMethodKind = [&](const Expr &candidate,
                                             LocalInfo::ValueKind &kindOut) {
      kindOut = LocalInfo::ValueKind::Unknown;
      if (!isDispatchSetupResultTypeMethodCall(candidate) ||
          !hasSemanticProductDispatchResultMethodFactContext(
              candidate, semanticProgram, semanticIndex)) {
        return false;
      }
      if (candidate.name != "ok") {
        return true;
      }
      ResultExprInfo resultInfo;
      if (resolveResultExprInfoFromLocals(candidate,
                                          localsIn,
                                          resolveMethodCallDefinitionForResultInfo,
                                          resolveDefinitionCallForResultInfo,
                                          stateInOut.getReturnInfo,
                                          stateInOut.inferExprKind,
                                          resultInfo,
                                          semanticProgram,
                                          semanticIndex) &&
          resultInfo.isResult) {
        kindOut = resultInfo.hasValue ? resultInfo.valueKind : LocalInfo::ValueKind::Int32;
      }
      return true;
    };
    auto resolveTryValueKind = [&](const Expr &tryExpr, LocalInfo::ValueKind &kindOut) -> bool {
      kindOut = LocalInfo::ValueKind::Unknown;
      std::string semanticTryFactError;
      auto resolveTryFactValueTypeText = [&](const SemanticProgramTryFact &tryFact) {
        if (semanticProgram != nullptr && tryFact.valueTypeId != InvalidSymbolId) {
          std::string resolvedTypeText = std::string(
              semanticProgramResolveCallTargetString(*semanticProgram, tryFact.valueTypeId));
          if (!resolvedTypeText.empty()) {
            return trimTemplateTypeText(resolvedTypeText);
          }
        }
        return trimTemplateTypeText(tryFact.valueType);
      };
      if (semanticProgram != nullptr && semanticIndex != nullptr && tryExpr.semanticNodeId != 0) {
        const auto *tryFact =
            findSemanticProductTryFactBySemanticId(*semanticIndex, tryExpr);
        if (tryFact != nullptr) {
          const std::string valueTypeText = resolveTryFactValueTypeText(*tryFact);
          kindOut = valueKindFromTypeName(valueTypeText);
          if (kindOut == LocalInfo::ValueKind::Unknown && !valueTypeText.empty()) {
            kindOut = LocalInfo::ValueKind::Int64;
          }
          if (kindOut != LocalInfo::ValueKind::Unknown) {
            return true;
          }
          semanticTryFactError = "incomplete semantic-product try fact: try";
        } else {
          semanticTryFactError = "missing semantic-product try fact: try";
        }
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
          return false;
        }
      }
      if (tryExpr.kind != Expr::Kind::Call || tryExpr.args.size() != 1) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      const Expr &resultExpr = tryExpr.args.front();
      bool hasSemanticFileHandleCall = false;
      if (inferDispatchSetupSemanticFileHandleCallKind(resultExpr,
                                                       semanticProgram,
                                                       semanticIndex,
                                                       kindOut,
                                                       hasSemanticFileHandleCall)) {
        return true;
      }
      if (hasSemanticFileHandleCall) {
        return true;
      }
      bool hasSemanticResultOperand = false;
      if (inferDispatchSetupSemanticTryOperandResultKind(resultExpr,
                                                        semanticProgram,
                                                        semanticIndex,
                                                        kindOut,
                                                        hasSemanticResultOperand)) {
        return true;
      }
      if (hasSemanticResultOperand) {
        return true;
      }
      bool hasSemanticDereferencedResultOperand = false;
      if (inferDispatchSetupSemanticDereferencedTryOperandResultKind(
              resultExpr,
              semanticProgram,
              semanticIndex,
              kindOut,
              hasSemanticDereferencedResultOperand)) {
        return true;
      }
      if (hasSemanticDereferencedResultOperand) {
        return true;
      }
      auto resolveCallReceiverCollectionValueKind = [&](const Expr &receiverExpr,
                                                        LocalInfo::ValueKind &receiverKindOut) -> bool {
        receiverKindOut = LocalInfo::ValueKind::Unknown;
        if (receiverExpr.kind != Expr::Kind::Call) {
          return false;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(receiverExpr, collectionName)) {
          if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
              receiverExpr.templateArgs.size() == 1) {
            receiverKindOut = valueKindFromTypeName(receiverExpr.templateArgs.front());
            return receiverKindOut != LocalInfo::ValueKind::Unknown;
          }
          if (collectionName == "map" && receiverExpr.templateArgs.size() == 2) {
            receiverKindOut = valueKindFromTypeName(receiverExpr.templateArgs.back());
            return receiverKindOut != LocalInfo::ValueKind::Unknown;
          }
        }
        if (defMap == nullptr || !resolveExprPath) {
          return false;
        }
        std::string receiverPath = resolveExprPath(receiverExpr);
        if (receiverPath.empty() && !receiverExpr.name.empty()) {
          receiverPath = receiverExpr.name;
          if (!receiverPath.empty() && receiverPath.front() != '/') {
            receiverPath.insert(receiverPath.begin(), '/');
          }
        }
        if (receiverPath.empty()) {
          return false;
        }
        auto defIt = defMap->find(receiverPath);
        if (defIt == defMap->end() || defIt->second == nullptr) {
          return false;
        }
        std::vector<std::string> collectionArgs;
        if (!inferDeclaredReturnCollection(*defIt->second, collectionName, collectionArgs)) {
          return false;
        }
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
            collectionArgs.size() == 1) {
          receiverKindOut = valueKindFromTypeName(collectionArgs.front());
          return receiverKindOut != LocalInfo::ValueKind::Unknown;
        }
        if (collectionName == "map" && collectionArgs.size() == 2) {
          receiverKindOut = valueKindFromTypeName(collectionArgs.back());
          return receiverKindOut != LocalInfo::ValueKind::Unknown;
        }
        return false;
      };
      if (resultExpr.kind == Expr::Kind::Name) {
        auto it = localsIn.find(resultExpr.name);
        if (it != localsIn.end() && it->second.isResult) {
          kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
          return true;
        }
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      if (resultExpr.kind == Expr::Kind::Call) {
        if (inferSemanticResultMethodKind(resultExpr, kindOut)) {
          return true;
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(resultExpr, accessName) && resultExpr.args.size() == 2 &&
            resultExpr.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(resultExpr.args.front().name);
          if (it != localsIn.end() && it->second.isArgsPack && it->second.isResult) {
            kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        if (isSimpleCallName(resultExpr, "dereference") && resultExpr.args.size() == 1) {
          const Expr &targetExpr = resultExpr.args.front();
          if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, accessName) &&
              targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
            auto it = localsIn.find(targetExpr.args.front().name);
            if (it != localsIn.end() && it->second.isArgsPack && it->second.isResult &&
                (it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
                 it->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
              kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
              return true;
            }
          }
        }
        if (stateInOut.getReturnInfo && stateInOut.inferExprKind) {
          auto resolveMethodCallDefinitionNoProbeError =
              [&](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
            if (!stateInOut.resolveMethodCallDefinition) {
              return nullptr;
            }
            return stateInOut.resolveMethodCallDefinition(candidate, candidateLocals);
          };
          auto resolveDefinitionCall = [&](const Expr &candidate) -> const Definition * {
            if (candidate.isMethodCall || defMap == nullptr || !resolveExprPath) {
              return nullptr;
            }
            std::string path = resolveExprPath(candidate);
            if (path.empty() && !candidate.name.empty()) {
              path = candidate.name;
              if (!path.empty() && path.front() != '/') {
                path.insert(path.begin(), '/');
              }
            }
            if (path.empty()) {
              return nullptr;
            }
            auto defIt = defMap->find(path);
            return defIt != defMap->end() ? defIt->second : nullptr;
          };

          ResultExprInfo resultInfo;
          if (resolveResultExprInfoFromLocals(resultExpr,
                                             localsIn,
                                             resolveMethodCallDefinitionNoProbeError,
                                             resolveDefinitionCall,
                                             stateInOut.getReturnInfo,
                                             stateInOut.inferExprKind,
                                             resultInfo) &&
              resultInfo.isResult) {
            kindOut = resultInfo.hasValue ? resultInfo.valueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
      }
      if (resultExpr.kind != Expr::Kind::Call) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      const bool resultIsMapContainsCall = isMapContainsCallName(resultExpr);
      const bool resultIsMapTryAtCall = isMapTryAtCallName(resultExpr);
      if ((resultIsMapContainsCall || resultIsMapTryAtCall) &&
          !resultExpr.args.empty()) {
        bool hasSemanticMapReceiver = false;
        if (inferDispatchSetupSemanticMapReceiverKind(resultExpr.args.front(),
                                                     resultIsMapContainsCall,
                                                     semanticProgram,
                                                     semanticIndex,
                                                     kindOut,
                                                     hasSemanticMapReceiver)) {
          return true;
        }
        if (hasSemanticMapReceiver) {
          return true;
        }
      }
      if (isMapContainsCallName(resultExpr) && !resultExpr.args.empty()) {
        LocalInfo::ValueKind receiverCollectionKind = LocalInfo::ValueKind::Unknown;
        if (resolveCallReceiverCollectionValueKind(resultExpr.args.front(), receiverCollectionKind) &&
            receiverCollectionKind != LocalInfo::ValueKind::Unknown) {
          kindOut = LocalInfo::ValueKind::Bool;
          return true;
        }
      }
      if (isMapTryAtCallName(resultExpr) && !resultExpr.args.empty()) {
        LocalInfo::ValueKind receiverCollectionKind = LocalInfo::ValueKind::Unknown;
        if (resolveCallReceiverCollectionValueKind(resultExpr.args.front(), receiverCollectionKind) &&
            receiverCollectionKind != LocalInfo::ValueKind::Unknown) {
          kindOut = receiverCollectionKind;
          return true;
        }
      }
      if (inferMapContainsResultKind(resultExpr, localsIn, kindOut)) {
        return true;
      }
      if (inferMapTryAtResultValueKind(resultExpr, localsIn, kindOut)) {
        return true;
      }
      if (isFileHandleCall(resultExpr)) {
        kindOut = LocalInfo::ValueKind::Int64;
        return true;
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty()) {
        const Expr &receiverExpr = resultExpr.args.front();
        bool hasSemanticFileHandleReceiver = false;
        if (inferDispatchSetupSemanticFileHandleMethodKind(receiverExpr,
                                                          resultExpr.name,
                                                          semanticProgram,
                                                          semanticIndex,
                                                          kindOut,
                                                          hasSemanticFileHandleReceiver)) {
          return true;
        }
        bool hasSemanticFileHandleTarget = false;
        if (inferDispatchSetupSemanticDereferencedFileHandleMethodKind(
                receiverExpr,
                resultExpr.name,
                semanticProgram,
                semanticIndex,
                kindOut,
                hasSemanticFileHandleTarget)) {
          return true;
        }
        bool hasSemanticFileErrorReceiver = false;
        if (resultExpr.name == "why" &&
            inferDispatchSetupSemanticFileErrorWhyKind(receiverExpr,
                                                       semanticProgram,
                                                       semanticIndex,
                                                       kindOut,
                                                       hasSemanticFileErrorReceiver)) {
          return true;
        }
        bool hasSemanticFileErrorTarget = false;
        if (resultExpr.name == "why" &&
            inferDispatchSetupSemanticDereferencedFileErrorWhyKind(
                receiverExpr,
                semanticProgram,
                semanticIndex,
                kindOut,
                hasSemanticFileErrorTarget)) {
          return true;
        }
        if (hasSemanticFileHandleReceiver || hasSemanticFileHandleTarget ||
            hasSemanticFileErrorReceiver || hasSemanticFileErrorTarget) {
          return true;
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() && resultExpr.args.front().kind == Expr::Kind::Name) {
        const Expr &receiverExpr = resultExpr.args.front();
        auto it = localsIn.find(receiverExpr.name);
        if (it != localsIn.end() && it->second.isFileHandle) {
          if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
              resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
            kindOut = LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        std::string receiverPath = receiverExpr.name;
        if (!receiverExpr.namespacePrefix.empty()) {
          receiverPath = receiverExpr.namespacePrefix + "/" + receiverExpr.name;
        }
        if ((receiverExpr.name == "FileError" || receiverPath == "/std/file/FileError") &&
            resultExpr.name == "why") {
          kindOut = LocalInfo::ValueKind::String;
          return true;
        }
        if (receiverExpr.name == "Result") {
          if (resultExpr.name == "ok") {
            kindOut = resultExpr.args.size() > 1 ? stateInOut.inferExprKind(resultExpr.args[1], localsIn)
                                                 : LocalInfo::ValueKind::Int32;
            return true;
          }
          if (resultExpr.name == "error") {
            kindOut = LocalInfo::ValueKind::Bool;
            return true;
          }
          if (resultExpr.name == "why") {
            kindOut = LocalInfo::ValueKind::String;
            return true;
          }
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedBorrowedArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedPointerArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }

      const Definition *callee = nullptr;
      if (resultExpr.isMethodCall) {
        callee = stateInOut.resolveMethodCallDefinition(resultExpr, localsIn);
      } else {
        const std::string path = resolveExprPath(resultExpr);
        auto defIt = defMap->find(path);
        if (defIt != defMap->end()) {
          callee = defIt->second;
        }
      }
      if (callee == nullptr) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }

      ReturnInfo returnInfo;
      if (!stateInOut.getReturnInfo(callee->fullPath, returnInfo) || !returnInfo.isResult) {
        if (!semanticTryFactError.empty()) {
          *inferenceError = semanticTryFactError;
        }
        return false;
      }
      kindOut = returnInfo.resultHasValue ? returnInfo.resultValueKind : LocalInfo::ValueKind::Int32;
      return true;
    };

    LocalInfo::ValueKind literalOrNameKind = LocalInfo::ValueKind::Unknown;
    if (stateInOut.inferLiteralOrNameExprKind(expr, localsIn, literalOrNameKind)) {
      return literalOrNameKind;
    }
    switch (expr.kind) {
      case Expr::Kind::Call: {
        auto isBuiltinCountLikeCall = [](const Expr &candidate) {
          if (candidate.kind != Expr::Kind::Call) {
            return false;
          }
          if (isSimpleCallName(candidate, "count")) {
            return true;
          }
          std::string collectionName;
          std::string helperName;
          return getNamespacedCollectionHelperName(candidate, collectionName, helperName) && helperName == "count";
        };

        if (isSimpleCallName(expr, "try") && expr.args.size() == 1) {
          LocalInfo::ValueKind tryValueKind = LocalInfo::ValueKind::Unknown;
          if (resolveTryValueKind(expr, tryValueKind)) {
            return tryValueKind;
          }
          if (!inferenceError->empty()) {
            return LocalInfo::ValueKind::Unknown;
          }
        }

        if (expr.isMethodCall && !expr.args.empty() && expr.name == "why") {
          bool hasSemanticFileErrorReceiver = false;
          LocalInfo::ValueKind semanticFileErrorWhyKind = LocalInfo::ValueKind::Unknown;
          if (inferDispatchSetupSemanticFileErrorWhyKind(expr.args.front(),
                                                        semanticProgram,
                                                        semanticIndex,
                                                        semanticFileErrorWhyKind,
                                                        hasSemanticFileErrorReceiver)) {
            return semanticFileErrorWhyKind;
          }
          bool hasSemanticFileErrorTarget = false;
          if (inferDispatchSetupSemanticDereferencedFileErrorWhyKind(
                  expr.args.front(),
                  semanticProgram,
                  semanticIndex,
                  semanticFileErrorWhyKind,
                  hasSemanticFileErrorTarget)) {
            return semanticFileErrorWhyKind;
          }
          if (hasSemanticFileErrorReceiver) {
            return LocalInfo::ValueKind::Unknown;
          }
          if (hasSemanticFileErrorTarget) {
            return LocalInfo::ValueKind::Unknown;
          }
        }

        if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
          const Expr &receiverExpr = expr.args.front();
          std::string receiverPath = receiverExpr.name;
          if (!receiverExpr.namespacePrefix.empty()) {
            receiverPath = receiverExpr.namespacePrefix + "/" + receiverExpr.name;
          }
          if ((receiverExpr.name == "FileError" || receiverPath == "/std/file/FileError") &&
              expr.name == "why") {
            return LocalInfo::ValueKind::String;
          }
          if (receiverExpr.name == "Result") {
            LocalInfo::ValueKind semanticResultMethodKind = LocalInfo::ValueKind::Unknown;
            if (inferSemanticResultMethodKind(expr, semanticResultMethodKind)) {
              return semanticResultMethodKind;
            }
            if (expr.name == "ok") {
              return expr.args.size() > 1 ? stateInOut.inferExprKind(expr.args[1], localsIn)
                                          : LocalInfo::ValueKind::Int32;
            }
            if (expr.name == "error") {
              return LocalInfo::ValueKind::Bool;
            }
            if (expr.name == "why") {
              return LocalInfo::ValueKind::String;
            }
          }
        }

        LocalInfo::ValueKind callBaseKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprBaseKind(expr, localsIn, callBaseKind)) {
          return callBaseKind;
        }
        std::string builtinOperatorName;
        if ((getBuiltinComparisonName(expr, builtinOperatorName) ||
             getBuiltinOperatorName(expr, builtinOperatorName))) {
          LocalInfo::ValueKind operatorFallbackKind = LocalInfo::ValueKind::Unknown;
          if (stateInOut.inferCallExprOperatorFallbackKind(expr, localsIn, operatorFallbackKind)) {
            return operatorFallbackKind;
          }
        }
        LocalInfo::ValueKind callReturnKind = LocalInfo::ValueKind::Unknown;
        const auto callReturnResolution = stateInOut.inferCallExprDirectReturnKind(expr, localsIn, callReturnKind);
        if (callReturnResolution == CallExpressionReturnKindResolution::Resolved) {
          return callReturnKind;
        }
        LocalInfo::ValueKind callFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprCountAccessGpuFallbackKind(expr, localsIn, callFallbackKind)) {
          return callFallbackKind;
        }
        if (isBuiltinCountLikeCall(expr) && expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call) {
          std::string accessName;
          const Expr &accessExpr = expr.args.front();
          if (getBuiltinArrayAccessName(accessExpr, accessName) && accessExpr.args.size() == 2) {
            if (stateInOut.inferExprKind &&
                stateInOut.inferExprKind(accessExpr, localsIn) == LocalInfo::ValueKind::String) {
              return LocalInfo::ValueKind::Int32;
            }
            std::string semanticAccessType;
            if (resolveDispatchSetupSemanticReceiverTypeText(
                    accessExpr, semanticProgram, semanticIndex, semanticAccessType)) {
              return LocalInfo::ValueKind::Unknown;
            }
            const Expr &accessTarget = accessExpr.args.front();
            if (accessTarget.kind == Expr::Kind::Name) {
              auto it = localsIn.find(accessTarget.name);
              if (it != localsIn.end() &&
                  ((it->second.kind == LocalInfo::Kind::Map) ||
                   (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToMap)) &&
                  it->second.mapValueKind == LocalInfo::ValueKind::String) {
                return LocalInfo::ValueKind::Int32;
              }
            } else if (accessTarget.kind == Expr::Kind::Call) {
              std::string collectionName;
              if (getBuiltinCollectionName(accessTarget, collectionName) && collectionName == "map" &&
                  accessTarget.templateArgs.size() == 2) {
                const std::string &valueType = accessTarget.templateArgs[1];
                if (valueType == "string" || valueType == "/string") {
                  return LocalInfo::ValueKind::Int32;
                }
              }
            }
          }
        }
        if (callReturnResolution == CallExpressionReturnKindResolution::MatchedButUnsupported) {
          return LocalInfo::ValueKind::Unknown;
        }
        LocalInfo::ValueKind operatorFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprOperatorFallbackKind(expr, localsIn, operatorFallbackKind)) {
          return operatorFallbackKind;
        }
        LocalInfo::ValueKind controlFlowKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprControlFlowFallbackKind(expr, localsIn, *inferenceError, controlFlowKind)) {
          return controlFlowKind;
        }
        LocalInfo::ValueKind pointerFallbackKind = LocalInfo::ValueKind::Unknown;
        if (stateInOut.inferCallExprPointerFallbackKind(expr, localsIn, pointerFallbackKind)) {
          return pointerFallbackKind;
        }
        return LocalInfo::ValueKind::Unknown;
      }
      default:
        return LocalInfo::ValueKind::Unknown;
    }
  };
  return true;
}

} // namespace primec::ir_lowerer
