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
#include "IrLowererLowerInferenceDispatchSetupFileLocal.h"

namespace primec::ir_lowerer {
using namespace ir_lowerer_lower_inference_dispatch_setup_file_local;

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
      std::string canonicalKeyValueHelperName;
      std::string borrowedKeyValueHelperName;
      const bool hasBorrowedKeyValueHelperAlias =
          resolveBorrowedKeyValueHelperAliasName(expr, borrowedKeyValueHelperName);
      if (resolveKeyValueHelperAliasName(expr, canonicalKeyValueHelperName) &&
          (!stateInOut.resolveDefinitionCall || stateInOut.resolveDefinitionCall(expr) == nullptr) &&
          !hasBorrowedKeyValueHelperAlias &&
          (canonicalKeyValueHelperName == "count" || canonicalKeyValueHelperName == "contains" ||
           canonicalKeyValueHelperName == "tryAt" || canonicalKeyValueHelperName == "at" ||
           canonicalKeyValueHelperName == "at_unsafe" ||
           collection_helpers::isInsertHelperName(canonicalKeyValueHelperName)) &&
          !(isExplicitKeyValueHelperFallbackPath(expr) &&
            (canonicalKeyValueHelperName == "at" || canonicalKeyValueHelperName == "at_unsafe" ||
             canonicalKeyValueHelperName == "tryAt")) &&
          ((expr.name.find('/') != std::string::npos) || !expr.namespacePrefix.empty() ||
           !expr.templateArgs.empty())) {
        Expr rewrittenExpr = expr;
        rewrittenExpr.name = canonicalKeyValueHelperName;
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
          if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa") &&
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
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa") &&
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
        // TODO-5302 round 9: same "structural-only, no semantic
        // corroboration" shape the round 7 fixes above gated - a bare or
        // dereferenced indexed args-pack Result access trusted the
        // receiver's raw `LocalInfo` alone, with no check that a semantic
        // context even exists. Gate on a semantic context existing at all,
        // which every real compiled program has (`IrLowererLower.cpp`
        // hard-errors before lowering when `semanticProgram` is null), so
        // the un-annotated/no-semantics-at-all unit scenarios defer here
        // instead of silently resolving a stale local's claim.
        std::string accessName;
        if (semanticProgram != nullptr && getBuiltinArrayAccessName(resultExpr, accessName) &&
            resultExpr.args.size() == 2 && resultExpr.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(resultExpr.args.front().name);
          if (it != localsIn.end() && it->second.isArgsPack && it->second.isResult) {
            kindOut = it->second.resultHasValue ? it->second.resultValueKind : LocalInfo::ValueKind::Int32;
            return true;
          }
        }
        if (semanticProgram != nullptr && isSimpleCallName(resultExpr, "dereference") &&
            resultExpr.args.size() == 1) {
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
        bool hasSemanticKeyValueReceiver = false;
        if (inferDispatchSetupSemanticKeyValueReceiverKind(resultExpr.args.front(),
                                                           resultIsMapContainsCall,
                                                           semanticProgram,
                                                           semanticIndex,
                                                           kindOut,
                                                           hasSemanticKeyValueReceiver)) {
          return true;
        }
        if (hasSemanticKeyValueReceiver) {
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
      if (inferMapContainsResultKind(
              resultExpr, localsIn, kindOut, semanticProgram, semanticIndex)) {
        return true;
      }
      if (inferMapTryAtResultValueKind(
              resultExpr, localsIn, kindOut, semanticProgram, semanticIndex)) {
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
      // TODO-5302 round 9: same "structural-only, no semantic
      // corroboration" shape as `IrLowererLowerInferenceBaseKindHelpers.cpp`
      // gated in round 7 (`isIndexed[Borrowed/Pointer]ArgsPackFileHandleReceiver`
      // there) - this dispatch-side copy of the same three checks trusted
      // the receiver's raw `LocalInfo` alone with no check that a semantic
      // context even exists. Gate on a semantic context existing at all,
      // which every real compiled program has, matching the same
      // un-annotated/no-semantics-at-all unit scenarios that must defer
      // here instead of silently resolving a stale local's claim.
      if (semanticProgram != nullptr && resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (semanticProgram != nullptr && resultExpr.isMethodCall && !resultExpr.args.empty() &&
          isIndexedBorrowedArgsPackFileHandleReceiver(resultExpr.args.front(), localsIn)) {
        if (resultExpr.name == "write" || resultExpr.name == "write_line" || resultExpr.name == "write_byte" ||
            resultExpr.name == "write_bytes" || resultExpr.name == "flush" || resultExpr.name == "close") {
          kindOut = LocalInfo::ValueKind::Int32;
          return true;
        }
      }
      if (semanticProgram != nullptr && resultExpr.isMethodCall && !resultExpr.args.empty() &&
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
    if (inferDispatchSetupSemanticNameKind(
            expr, semanticProgram, semanticIndex, literalOrNameKind)) {
      return literalOrNameKind;
    }
    if (stateInOut.inferLiteralOrNameExprKind(expr, localsIn, literalOrNameKind)) {
      return literalOrNameKind;
    }
    switch (expr.kind) {
      case Expr::Kind::Call: {
        std::string accessNameForCanonicalKeyValueOverride;
        if (getBuiltinArrayAccessName(expr, accessNameForCanonicalKeyValueOverride) &&
            expr.args.size() == 2 &&
            (accessNameForCanonicalKeyValueOverride == "at" ||
             accessNameForCanonicalKeyValueOverride == "at_unsafe")) {
          LocalInfo::ValueKind keyValueValueKind = LocalInfo::ValueKind::Unknown;
          ReturnInfo accessReturnInfo;
          auto resolveAccessReceiverKeyValueValueKind =
              [&](const Expr &receiverExpr,
                  LocalInfo::ValueKind &receiverKeyValueValueKindOut) {
                receiverKeyValueValueKindOut = LocalInfo::ValueKind::Unknown;
                std::string semanticReceiverTypeText;
                LocalInfo::ValueKind semanticKeyValueKeyKind =
                    LocalInfo::ValueKind::Unknown;
                LocalInfo::ValueKind semanticKeyValueValueKind =
                    LocalInfo::ValueKind::Unknown;
                if (resolveDispatchSetupSemanticReceiverTypeText(
                        receiverExpr,
                        semanticProgram,
                        semanticIndex,
                        semanticReceiverTypeText) &&
                    inferDispatchSetupKeyValueKindsFromTypeText(
                        semanticReceiverTypeText,
                        semanticKeyValueKeyKind,
                        semanticKeyValueValueKind)) {
                  receiverKeyValueValueKindOut = semanticKeyValueValueKind;
                  return receiverKeyValueValueKindOut !=
                         LocalInfo::ValueKind::Unknown;
                }
                if (receiverExpr.kind == Expr::Kind::Name) {
                  auto localIt = localsIn.find(receiverExpr.name);
                  if (localIt != localsIn.end() &&
                      localIt->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
                      localIt->second.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
                    receiverKeyValueValueKindOut = localIt->second.keyValueValueKind;
                    return receiverKeyValueValueKindOut !=
                           LocalInfo::ValueKind::Unknown;
                  }
                }
                if (receiverExpr.kind == Expr::Kind::Call &&
                    stateInOut.getReturnInfo && resolveExprPath) {
                  ReturnInfo receiverReturnInfo;
                  if (stateInOut.getReturnInfo(resolveExprPath(receiverExpr),
                                               receiverReturnInfo) &&
                      receiverReturnInfo.returnsArray) {
                    receiverKeyValueValueKindOut = receiverReturnInfo.kind;
                    return receiverKeyValueValueKindOut !=
                           LocalInfo::ValueKind::Unknown;
                  }
                }
                return false;
              };
          auto semanticCallableReturnsString = [&](const std::string &path) {
            if (semanticProgram == nullptr) {
              return false;
            }
            const auto *summary =
                findSemanticProductCallableSummary(semanticProgram, path);
            if (summary == nullptr) {
              return false;
            }
            std::string returnKind = summary->returnKind;
            if (summary->returnKindId != InvalidSymbolId) {
              const std::string resolvedReturnKind =
                  std::string(semanticProgramResolveCallTargetString(
                      *semanticProgram, summary->returnKindId));
              if (!resolvedReturnKind.empty()) {
                returnKind = resolvedReturnKind;
              }
            }
            return trimTemplateTypeText(returnKind) == "string";
          };
          if (resolveAccessReceiverKeyValueValueKind(expr.args.front(),
                                                     keyValueValueKind) &&
              keyValueValueKind != LocalInfo::ValueKind::String) {
            const std::string canonicalAccessPath =
                canonicalKeyValueHelperPathForDispatchSetup(
                    accessNameForCanonicalKeyValueOverride);
            if (!canonicalAccessPath.empty() &&
                ((stateInOut.getReturnInfo &&
                  stateInOut.getReturnInfo(canonicalAccessPath,
                                           accessReturnInfo) &&
                  !accessReturnInfo.returnsVoid &&
                  !accessReturnInfo.returnsArray &&
                  accessReturnInfo.kind == LocalInfo::ValueKind::String) ||
                 semanticCallableReturnsString(canonicalAccessPath))) {
              return LocalInfo::ValueKind::String;
            }
          }
        }

        std::string semanticExprTypeText;
        if (resolveDispatchSetupSemanticReceiverTypeText(
                expr, semanticProgram, semanticIndex, semanticExprTypeText)) {
          const LocalInfo::ValueKind semanticExprKind =
              valueKindFromTypeName(semanticExprTypeText);
          if (semanticExprKind != LocalInfo::ValueKind::Unknown) {
            return semanticExprKind;
          }
        }

        LocalInfo::ValueKind semanticBindingKind = LocalInfo::ValueKind::Unknown;
        if (inferDispatchSetupSemanticBindingFactKind(
                expr, semanticProgram, semanticIndex, semanticBindingKind)) {
          return semanticBindingKind;
        }

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
        // Deliberately no further inline `count(access(...))` resolution
        // here: `inferCallExprCountAccessGpuFallbackKind` above is the
        // sole authoritative answer for this shape (it already covers the
        // structural Name/Call/templateArgs cases an earlier inline
        // duplicate re-derived here). When it reports "not resolved", the
        // dispatch must return Unknown rather than quietly re-deriving an
        // answer from semantic facts or raw locals - see TODO-5302 round 7
        // (docs/failing_tests.md) for the bug this duplicate caused.
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
