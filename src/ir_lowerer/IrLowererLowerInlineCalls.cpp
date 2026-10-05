#include "IrLowererLowerInlineCalls.h"

#include "IrLowererCountAccessClassifiers.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineCallContextHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineParamHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineStructArgHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallActiveContextStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallCleanupStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallContextSetupStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallGpuLocalsStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallReturnValueStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallStatementStep.h"
#include "IrLowererRecursionAnalysis.h"
#include "IrLowererRequirementContractHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructLayoutHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererVectorRecordLayoutHelpers.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/semantics/HostDefinitions.h"
#include "primec/support/SourceLocationMapper.h"

#include <limits>

namespace primec::ir_lowerer {

bool emitInlineDefinitionCallImpl(
    LowerSetupStageState &setupStage,
    LowerReturnEmitStageState &stateOut,
    const CallResolutionAdapters &callResolutionAdapters,
    DiagnosticSinkReport *diagnosticInfo,
    OnErrorByDefinition *onErrorByDefPtr,
    const Expr &callExpr,
    const Definition &callee,
    const LocalMap &callerLocals,
    bool requireValue,
    std::string &error) {
    using InlineContext = LowerReturnEmitInlineContext;
    DiagnosticSink diagnosticSink(diagnosticInfo);
    auto captureLoweringDiagnosticPrimarySpan = [&](const Expr &expr) {
      if (expr.sourceLine <= 0 || expr.sourceColumn <= 0) {
        return;
      }
      DiagnosticSpan span;
      span.line = expr.sourceLine;
      span.column = expr.sourceColumn;
      span.endLine = expr.sourceLine;
      span.endColumn = expr.sourceColumn;
      if (setupStage.expandedSource != nullptr) {
        span = mapDiagnosticSpanToSourceUnit(*setupStage.expandedSource, span);
      }
      diagnosticSink.capturePrimarySpanIfUnset(span);
    };

    OnErrorByDefinition &onErrorByDef = *onErrorByDefPtr;
    auto *&activeInlineContext = stateOut.activeInlineContext;
    auto &inlineStack = stateOut.inlineStack;
    auto &function = setupStage.function;
    auto &nextLocal = setupStage.nextLocal;
    auto &structNames = setupStage.structNames;
    auto &loweredCallTargets = setupStage.loweredCallTargets;
    auto &fileScopeStack = setupStage.fileScopeStack;
    auto &currentOnError = setupStage.currentOnError;
    auto &currentReturnResult = setupStage.currentReturnResult;
    auto &realCallReservationIndex = setupStage.realCallReservationIndex;
    auto &setupLocalsOrchestration = setupStage.setupLocalsOrchestration;
    const auto &runtimeErrorAndStringLiteralSetup =
        setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup;
    const auto &stringLiteralHelpers = runtimeErrorAndStringLiteralSetup.stringLiteralHelpers;
    auto &internString = stringLiteralHelpers.internString;
    auto &resolveStringTableTarget = stringLiteralHelpers.resolveStringTableTarget;
    const auto &runtimeErrorEmitters = runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters;
    auto &emitArrayIndexOutOfBounds = runtimeErrorEmitters.emitArrayIndexOutOfBounds;
    const auto &bindingTypeAdapters = setupLocalsOrchestration.bindingTypeAdapters;
    auto &isBindingMutable = bindingTypeAdapters.isBindingMutable;
    auto &setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo;
    auto &bindingKind = bindingTypeAdapters.bindingKind;
    auto &hasExplicitBindingTypeTransform = stateOut.hasExplicitBindingTypeTransform;
    auto &isStringBinding = bindingTypeAdapters.isStringBinding;
    auto &isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding;
    auto &bindingValueKind = bindingTypeAdapters.bindingValueKind;
    const auto &structArrayInfoAdapters = setupLocalsOrchestration.structArrayInfoAdapters;
    auto &applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo;
    auto &applyStructValueInfo = setupLocalsOrchestration.applyStructValueInfo;
    auto &getReturnInfo = setupStage.inferenceSetupBootstrap.getReturnInfo;
    auto &resolveMethodCallDefinition = setupStage.inferenceSetupBootstrap.resolveMethodCallDefinition;
    auto &inferExprKind = setupStage.inferenceSetupBootstrap.inferExprKind;
    auto &appendInstructionSourceRange = stateOut.appendInstructionSourceRange;
    auto &pushFileScope = stateOut.pushFileScope;
    auto &popFileScope = stateOut.popFileScope;
    const auto &structSlotResolutionAdapters = setupLocalsOrchestration.structSlotResolutionAdapters;
    auto &resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;
    const auto &uninitializedResolutionAdapters = setupLocalsOrchestration.uninitializedResolutionAdapters;
    auto &inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;
    auto &emitExpr = stateOut.emitExpr;
    auto &emitStatement = stateOut.emitStatement;
    auto &emitStringValueForCall = stateOut.emitStringValueForCall;
    auto &allocTempLocal = stateOut.allocTempLocal;
    auto &resolveDefinitionCall = stateOut.resolveDefinitionCall;
    auto &emitStructCopySlots = stateOut.emitStructCopySlots;
    auto &emitFileScopeCleanup = stateOut.emitFileScopeCleanup;

    // TODO-4747 Phase 1: definitions statically identified as safe for real
    // (non-inlined) Call/CallVoid emission - see
    // computeRealCallEligibleDefinitionPaths - are redirected here instead of
    // falling through to the inline path below. `imm` is a placeholder
    // ((1<<32)|reservationIndex, unambiguous since real function counts never
    // approach 2^32) rewritten to the callee's final module.functions index by
    // a fixup pass once every reserved function has been appended (mutual
    // recursion means the callee's final index isn't known yet here, and may
    // not even be known when the *caller* is fully lowered, if the caller
    // comes first in reservation order). Arguments are resolved to callee
    // parameter order via the same buildInlineCallOrderedArguments the inline
    // path below uses (so named arguments and callee-side defaults work
    // identically here - a naive left-to-right walk of callExpr.args would
    // silently mis-order or drop them), then evaluated via the ordinary
    // emitExpr path and left on the shared operand stack - the callee's own
    // prologue is responsible for popping them into its locals, matching the
    // calling convention already used by every existing Call/CallVoid site.
    // Host definitions ([host] f(...) {}) lower to CallHost: arguments on the
    // stack, import index in imm. The import table lives on the module.
    if (isHostDefinition(callee)) {
      const auto hostKind = [](std::string_view typeName) -> std::optional<IrHostValueKind> {
        const auto canonical = canonicalHostTypeName(typeName);
        if (!canonical.has_value()) {
          return std::nullopt;
        }
        if (*canonical == "i32") return IrHostValueKind::I32;
        if (*canonical == "i64") return IrHostValueKind::I64;
        if (*canonical == "u64") return IrHostValueKind::U64;
        if (*canonical == "f32") return IrHostValueKind::F32;
        if (*canonical == "f64") return IrHostValueKind::F64;
        if (*canonical == "string") return IrHostValueKind::String;
        return IrHostValueKind::Bool;
      };
      if (setupStage.outModule == nullptr) {
        error = "internal error: host call lowering has no module for " + callee.fullPath;
        return false;
      }
      IrHostImport import;
      import.name = callee.fullPath.size() > 1 && callee.fullPath.front() == '/' ? callee.fullPath.substr(1)
                                                                              : callee.fullPath;
      for (const Expr &param : callee.parameters) {
        const auto kind = hostKind(extractParameterTypeNameStatic(param));
        if (!kind.has_value()) {
          error = "host definition parameter is not a primitive: " + callee.fullPath;
          return false;
        }
        import.parameters.push_back(*kind);
      }
      ReturnInfo hostReturnInfo;
      if (!getReturnInfo(callee.fullPath, hostReturnInfo)) {
        error = "internal error: missing return info for host definition " + callee.fullPath;
        return false;
      }
      import.returnKind = IrHostValueKind::Void;
      if (!hostReturnInfo.returnsVoid) {
        std::optional<IrHostValueKind> returnKind;
        for (const auto &transform : callee.transforms) {
          if (transform.name == "return" && transform.templateArgs.size() == 1) {
            returnKind = hostKind(transform.templateArgs.front());
          }
        }
        if (!returnKind.has_value()) {
          error = "host definition return type is not a primitive: " + callee.fullPath;
          return false;
        }
        import.returnKind = *returnKind;
      }
      std::vector<Expr> hostParams;
      std::vector<const Expr *> hostOrderedArgs;
      std::vector<const Expr *> hostPackedArgs;
      size_t hostPackedParamIndex = 0;
      if (!ir_lowerer::buildInlineCallOrderedArguments(callExpr,
                                                       callee,
                                                       structNames,
                                                       callerLocals,
                                                       hostParams,
                                                       hostOrderedArgs,
                                                       hostPackedArgs,
                                                       hostPackedParamIndex,
                                                       error)) {
        return false;
      }
      if (!hostPackedArgs.empty() || hostOrderedArgs.size() != import.parameters.size()) {
        error = "host call argument mismatch for " + callee.fullPath;
        return false;
      }
      for (const Expr *argExpr : hostOrderedArgs) {
        if (argExpr == nullptr) {
          error = "internal error: missing argument for host call " + callee.fullPath;
          return false;
        }
        if (!emitExpr(*argExpr, callerLocals)) {
          return false;
        }
      }
      auto &imports = setupStage.outModule->hostImports;
      uint64_t importIndex = imports.size();
      for (size_t i = 0; i < imports.size(); ++i) {
        if (imports[i].name == import.name) {
          importIndex = i;
          break;
        }
      }
      if (importIndex == imports.size()) {
        imports.push_back(std::move(import));
      }
      function.instructions.push_back({IrOpcode::CallHost, importIndex});
      if (imports[importIndex].returnKind != IrHostValueKind::Void && !requireValue) {
        function.instructions.push_back({IrOpcode::Pop, 0});
      }
      return true;
    }
    if (const auto realCallIt = realCallReservationIndex.find(callee.fullPath);
        realCallIt != realCallReservationIndex.end()) {
      ReturnInfo calleeReturnInfo;
      if (!getReturnInfo(callee.fullPath, calleeReturnInfo)) {
        error = "internal error: missing return info for real-call target " + callee.fullPath;
        return false;
      }
      std::vector<Expr> realCallParams;
      std::vector<const Expr *> realCallOrderedArgs;
      std::vector<const Expr *> realCallPackedArgs;
      size_t realCallPackedParamIndex = 0;
      if (!ir_lowerer::buildInlineCallOrderedArguments(callExpr,
                                                       callee,
                                                       structNames,
                                                       callerLocals,
                                                       realCallParams,
                                                       realCallOrderedArgs,
                                                       realCallPackedArgs,
                                                       realCallPackedParamIndex,
                                                       error)) {
        return false;
      }
      if (!realCallPackedArgs.empty()) {
        // Eligibility excludes args-pack parameters, so a real-call target
        // should never produce packed arguments here - fail loudly instead
        // of silently dropping them.
        error = "internal error: unexpected packed arguments for real-call target " + callee.fullPath;
        return false;
      }
      for (const Expr *argExpr : realCallOrderedArgs) {
        if (argExpr == nullptr) {
          error = "internal error: missing argument for real-call target " + callee.fullPath;
          return false;
        }
        if (!emitExpr(*argExpr, callerLocals)) {
          return false;
        }
      }
      const uint64_t placeholderImm = (uint64_t(1) << 32) | realCallIt->second;
      function.instructions.push_back(
          {calleeReturnInfo.returnsVoid ? IrOpcode::CallVoid : IrOpcode::Call, placeholderImm});
      if (!calleeReturnInfo.returnsVoid && !requireValue) {
        function.instructions.push_back({IrOpcode::Pop, 0});
      }
      return true;
    }
    const auto isInternalSoaMetadataInlineHelper =
        [](std::string_view path, std::string_view fieldName) {
          if (path.rfind(collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) != 0 &&
              path.rfind(collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView"), 0) != 0) {
            return false;
          }
          std::string leaf(path.substr(path.find_last_of('/') == std::string_view::npos
                                           ? 0
                                           : path.find_last_of('/') + 1));
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == fieldName;
        };
    const auto isInternalSoaMetadataOwner =
        [](std::string_view path) {
          if (path.rfind(collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) != 0 &&
              path.rfind(collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView"), 0) != 0) {
            return false;
          }
          const size_t leafStart = path.find_last_of('/');
          std::string leaf(path.substr(leafStart == std::string_view::npos
                                           ? 0
                                           : leafStart + 1));
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == "SoaColumn" || leaf == "SoaFieldView";
        };
    const auto callLeafName = [](const Expr &expr) {
      std::string path = expr.name;
      if (path.find('/') == std::string::npos && !expr.namespacePrefix.empty()) {
        path = expr.namespacePrefix == "/" ? "/" + path
                                           : expr.namespacePrefix + "/" + path;
      }
      const size_t leafStart = path.find_last_of('/');
      std::string leaf =
          leafStart == std::string::npos ? path : path.substr(leafStart + 1);
      const size_t generatedSuffix = leaf.find("__");
      if (generatedSuffix != std::string::npos) {
        leaf.erase(generatedSuffix);
      }
      return leaf;
    };
    const auto stdCollectionsRoot = []() {
      return std::string("/std/collections");
    };
    const auto collectionMemberRoot = [&](std::string_view collectionName) {
      return stdCollectionsRoot() + "/" + std::string(collectionName) + "/";
    };
    const auto collectionWrapperAlias =
        [](std::string_view collectionName, std::string_view suffix) {
          return std::string(collectionName) + std::string(suffix);
        };
    const auto isCollectionVectorConstructor =
        [&](std::string_view path) {
          std::string leaf;
          const std::string canonicalVectorRoot = collectionMemberRoot("vector");
          const std::string experimentalVectorRoot =
              vectorBackingMemberRoot();
          for (const std::string &prefix :
               {canonicalVectorRoot,
                std::string(collection_paths::modulePrefix(collection_paths::kVectorFolder)),
                experimentalVectorRoot}) {
            if (path.rfind(prefix, 0) != 0) {
              continue;
            }
            leaf = std::string(path.substr(prefix.size()));
            break;
          }
          if (leaf.empty()) {
            return false;
          }
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == "vector" ||
                 leaf == collectionWrapperAlias("vector", "New") ||
                 leaf == collectionWrapperAlias("vector", "Single") ||
                 leaf == collectionWrapperAlias("vector", "Pair") ||
                 leaf == collectionWrapperAlias("vector", "Triple") ||
                 leaf == collectionWrapperAlias("vector", "Quad") ||
                 leaf == collectionWrapperAlias("vector", "Quint") ||
                 leaf == collectionWrapperAlias("vector", "Sext") ||
                 leaf == collectionWrapperAlias("vector", "Sept") ||
                 leaf == collectionWrapperAlias("vector", "Oct");
        };
#include "IrLowererLowerInlineCallsBody.h"
    if (!ir_lowerer::runLowerInlineCallGpuLocalsStep(
            {
                .callerLocals = &callerLocals,
                .calleeLocals = &calleeLocals,
            },
            error)) {
      popInlineStack();
      return false;
    }

    if (!ir_lowerer::emitRequirementContractChecks(
            callee,
            function,
            [&](const Expr &predicateExpr) {
              return emitExpr(predicateExpr, calleeLocals);
            },
            internString,
            error)) {
      popInlineStack();
      return false;
    }

    const bool isGeneratedMapInsertHelper =
        callee.fullPath == collection_paths::memberPath(collection_paths::kMapFolder, "insertImpl") ||
        callee.fullPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kMapFolder, "insertImpl"), 0) == 0 ||
        callee.fullPath == collection_paths::memberPath(collection_paths::kMapFolder, "insertRefImpl") ||
        callee.fullPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kMapFolder, "insertRefImpl"), 0) == 0;
    if (isGeneratedMapInsertHelper) {
      auto extractParameterTypeName = [](const Expr &paramExpr) {
        for (const auto &transform : paramExpr.transforms) {
          if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
              transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
              transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
              transform.name == "effects" || transform.name == "capabilities") {
            continue;
          }
          if (!transform.arguments.empty()) {
            continue;
          }
          std::string typeName = transform.name;
          if (!transform.templateArgs.empty()) {
            typeName += "<";
            for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
              if (index != 0) {
                typeName += ", ";
              }
              typeName += trimTemplateTypeText(transform.templateArgs[index]);
            }
            typeName += ">";
          }
          return typeName;
        }
        return std::string{};
      };
      auto inferValueKindFromTypeText = [&](std::string typeText,
                                            LocalInfo::ValueKind &kindOut) {
        kindOut = LocalInfo::ValueKind::Unknown;
        typeText = trimTemplateTypeText(typeText);
        while (!typeText.empty()) {
          kindOut = valueKindFromTypeName(typeText);
          if (kindOut != LocalInfo::ValueKind::Unknown) {
            return true;
          }

          std::string base;
          std::string argText;
          if (!splitTemplateTypeName(typeText, base, argText)) {
            return false;
          }

          const std::string normalizedBase = trimTemplateTypeText(base);
          if ((normalizedBase == "Reference" || normalizedBase == "Pointer") && !argText.empty()) {
            typeText = trimTemplateTypeText(argText);
            continue;
          }
          return false;
        }
        return false;
      };
      std::function<bool(const std::string &,
                         LocalInfo::ValueKind &,
                         LocalInfo::ValueKind &)> inferMapKindsFromTypeText;
      inferMapKindsFromTypeText = [&](const std::string &typeText,
                                      LocalInfo::ValueKind &keyKindOut,
                                      LocalInfo::ValueKind &valueKindOut) {
        keyKindOut = LocalInfo::ValueKind::Unknown;
        valueKindOut = LocalInfo::ValueKind::Unknown;

        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
          return false;
        }

        const std::string normalizedBase = trimTemplateTypeText(base);
        if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
          return inferMapKindsFromTypeText(argText, keyKindOut, valueKindOut);
        }
        const bool isMapBase =
            isBuiltinCollectionTypeName(normalizedBase, "map") ||
            isExperimentalCollectionTypeName(normalizedBase, "map", "Map");
        if (!isMapBase) {
          return false;
        }

        std::vector<std::string> mapArgs;
        if (!splitTemplateArgs(argText, mapArgs) || mapArgs.size() != 2) {
          return false;
        }
        keyKindOut = valueKindFromTypeName(trimTemplateTypeText(mapArgs.front()));
        valueKindOut = valueKindFromTypeName(trimTemplateTypeText(mapArgs.back()));
        return keyKindOut != LocalInfo::ValueKind::Unknown &&
               valueKindOut != LocalInfo::ValueKind::Unknown;
      };
      auto resolveSemanticTypeText = [&](SymbolId typeTextId,
                                         const std::string &typeText) {
        if (callResolutionAdapters.semanticProgram != nullptr &&
            typeTextId != InvalidSymbolId) {
          const std::string resolvedText =
              std::string(semanticProgramResolveCallTargetString(
                  *callResolutionAdapters.semanticProgram,
                  typeTextId));
          if (!resolvedText.empty()) {
            return trimTemplateTypeText(resolvedText);
          }
        }
        return trimTemplateTypeText(typeText);
      };
      auto tryApplySemanticMapTypeText = [&](SymbolId typeTextId,
                                             const std::string &typeText,
                                             LocalInfo &infoOut) {
        LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
        LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
        if (!inferMapKindsFromTypeText(resolveSemanticTypeText(typeTextId, typeText),
                                       keyKind,
                                       valueKind)) {
          return false;
        }
        infoOut.keyValueKeyKind = keyKind;
        infoOut.keyValueValueKind = valueKind;
        return true;
      };
      auto tryApplySemanticCollectionSpecialization = [&](const Expr &receiverExpr,
                                                         LocalInfo &infoOut) {
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (!semanticTargets.hasSemanticProduct || receiverExpr.semanticNodeId == 0) {
          return false;
        }
        const auto *collectionFact =
            findSemanticProductCollectionSpecialization(semanticTargets.semanticIndex,
                                                        receiverExpr);
        if (collectionFact == nullptr) {
          return false;
        }
        const std::string collectionFamily =
            resolveSemanticTypeText(collectionFact->collectionFamilyId,
                                    collectionFact->collectionFamily);
        if (!isBuiltinCollectionTypeName(collectionFamily, "map")) {
          return false;
        }
        const LocalInfo::ValueKind keyKind = valueKindFromTypeName(
            resolveSemanticTypeText(collectionFact->keyTypeTextId,
                                    collectionFact->keyTypeText));
        const LocalInfo::ValueKind valueKind = valueKindFromTypeName(
            resolveSemanticTypeText(collectionFact->valueTypeTextId,
                                    collectionFact->valueTypeText));
        if (keyKind == LocalInfo::ValueKind::Unknown ||
            valueKind == LocalInfo::ValueKind::Unknown) {
          return false;
        }
        infoOut.keyValueKeyKind = keyKind;
        infoOut.keyValueValueKind = valueKind;
        return true;
      };
      auto tryPopulateMapKindsFromSemanticReceiver = [&](const Expr &receiverExpr,
                                                        LocalInfo &infoOut) {
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (!semanticTargets.hasSemanticProduct ||
            callResolutionAdapters.semanticProgram == nullptr ||
            receiverExpr.semanticNodeId == 0) {
          return false;
        }
        if (tryApplySemanticCollectionSpecialization(receiverExpr, infoOut)) {
          return true;
        }
        if (const auto *queryFact =
                findSemanticProductQueryFact(callResolutionAdapters.semanticProgram,
                                             semanticTargets.semanticIndex,
                                             receiverExpr);
            queryFact != nullptr) {
          if (tryApplySemanticMapTypeText(queryFact->bindingTypeTextId,
                                          queryFact->bindingTypeText,
                                          infoOut) ||
              tryApplySemanticMapTypeText(queryFact->queryTypeTextId,
                                          queryFact->queryTypeText,
                                          infoOut) ||
              tryApplySemanticMapTypeText(queryFact->receiverBindingTypeTextId,
                                          queryFact->receiverBindingTypeText,
                                          infoOut)) {
            return true;
          }
        }
        if (const auto *bindingFact =
                findSemanticProductBindingFact(semanticTargets.semanticIndex,
                                               receiverExpr);
            bindingFact != nullptr &&
            tryApplySemanticMapTypeText(bindingFact->bindingTypeTextId,
                                        bindingFact->bindingTypeText,
                                        infoOut)) {
          return true;
        }
        if (const auto *localAutoFact =
                findSemanticProductLocalAutoFact(callResolutionAdapters.semanticProgram,
                                                 semanticTargets.semanticIndex,
                                                 receiverExpr);
            localAutoFact != nullptr &&
            tryApplySemanticMapTypeText(localAutoFact->bindingTypeTextId,
                                        localAutoFact->bindingTypeText,
                                        infoOut)) {
          return true;
        }
        return false;
      };
      auto valuesIt = calleeLocals.find("values");
      if (valuesIt == calleeLocals.end()) {
        valuesIt = calleeLocals.find("entries");
      }
      auto keyIt = calleeLocals.find("key");
      auto valueIt = calleeLocals.find("value");
      if (valuesIt == calleeLocals.end() || keyIt == calleeLocals.end() || valueIt == calleeLocals.end()) {
        error = "builtin canonical map insert lowering requires values or entries plus key/value locals";
        popInlineStack();
        return false;
      }
      const Expr *originalValuesArg = nullptr;
      if (!orderedArgs.empty() && orderedArgs.front() != nullptr) {
        originalValuesArg = orderedArgs.front();
        if (originalValuesArg->kind == Expr::Kind::Call &&
            isSimpleCallName(*originalValuesArg, "dereference") &&
            originalValuesArg->args.size() == 1 &&
            originalValuesArg->args.front().kind == Expr::Kind::Name) {
          originalValuesArg = &originalValuesArg->args.front();
        }
        tryPopulateMapKindsFromSemanticReceiver(*originalValuesArg, valuesIt->second);
        if (originalValuesArg->kind == Expr::Kind::Name) {
          auto callerValuesIt = callerLocals.find(originalValuesArg->name);
          if (callerValuesIt != callerLocals.end() &&
              valuesIt->second.keyValueKeyKind == LocalInfo::ValueKind::Unknown &&
              callerValuesIt->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
              callerValuesIt->second.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
            valuesIt->second.keyValueKeyKind = callerValuesIt->second.keyValueKeyKind;
            valuesIt->second.keyValueValueKind = callerValuesIt->second.keyValueValueKind;
          }
        }
      }
      auto isExperimentalMapStructPath = [&](const std::string &structPath) {
        const std::string experimentalMapType =
            keyValueStorageStructRootPath();
        return structPath == experimentalMapType ||
               structPath.rfind(experimentalMapType + "__", 0) == 0;
      };
      bool receiverUsesExperimentalMapStruct =
          isExperimentalMapStructPath(valuesIt->second.structTypeName);
      if (!receiverUsesExperimentalMapStruct && originalValuesArg != nullptr &&
          originalValuesArg->kind == Expr::Kind::Name) {
        auto callerValuesIt = callerLocals.find(originalValuesArg->name);
        if (callerValuesIt != callerLocals.end()) {
          receiverUsesExperimentalMapStruct =
              isExperimentalMapStructPath(callerValuesIt->second.structTypeName);
        }
      }
      if (receiverUsesExperimentalMapStruct) {
        // Experimental-map receivers still need the real stdlib helper body,
        // because this helper path mutates the flat map storage layout, not
        // the struct-backed experimental map layout.
      } else {
        if (valuesIt->second.keyValueKeyKind == LocalInfo::ValueKind::Unknown &&
            callee.parameters.size() >= 3) {
          LocalInfo::ValueKind inferredKeyKind = LocalInfo::ValueKind::Unknown;
          LocalInfo::ValueKind inferredValueKind = LocalInfo::ValueKind::Unknown;
          if (inferValueKindFromTypeText(extractParameterTypeName(callee.parameters[1]), inferredKeyKind) &&
              inferValueKindFromTypeText(extractParameterTypeName(callee.parameters[2]), inferredValueKind)) {
            valuesIt->second.keyValueKeyKind = inferredKeyKind;
            valuesIt->second.keyValueValueKind = inferredValueKind;
          }
        }
        if (valuesIt->second.keyValueKeyKind == LocalInfo::ValueKind::Unknown) {
          error = "builtin canonical map insert lowering requires typed map bindings";
          popInlineStack();
          return false;
        }
        auto isDirectMapStorageLocal = [](const LocalInfo &info) {
          return info.kind == LocalInfo::Kind::Value && hasKeyValueKinds(info);
        };
        int32_t valuesLocal =
            isDirectMapStorageLocal(valuesIt->second) ? valuesIt->second.index : -1;
        int32_t valuesWrapperLocal = -1;
        int32_t ptrLocal = valuesIt->second.index;
        if (originalValuesArg != nullptr) {
          if (originalValuesArg->kind == Expr::Kind::Name) {
            auto callerValuesIt = callerLocals.find(originalValuesArg->name);
            if (callerValuesIt != callerLocals.end()) {
              if (isDirectMapStorageLocal(callerValuesIt->second)) {
                valuesLocal = callerValuesIt->second.index;
              } else if (hasWrappedKeyValueKinds(callerValuesIt->second, callerValuesIt->second.kind)) {
                valuesWrapperLocal = callerValuesIt->second.index;
              }
            }
          }
        }
        if (valuesIt->second.kind == LocalInfo::Kind::Reference ||
            valuesIt->second.kind == LocalInfo::Kind::Pointer) {
          if (hasKeyValueKinds(valuesIt->second)) {
            valuesWrapperLocal = valuesIt->second.index;
            ptrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesIt->second.index)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          } else {
            ptrLocal = valuesIt->second.index;
          }
        }
        if (!ir_lowerer::emitBuiltinCanonicalMapInsertOverwriteOrGrow(
                valuesLocal,
                valuesWrapperLocal,
                ptrLocal,
                keyIt->second.index,
                valueIt->second.index,
                valuesIt->second.keyValueKeyKind,
                [&]() { return allocTempLocal(); },
                [&]() { return function.instructions.size(); },
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                [&](size_t indexToPatch, uint64_t target) { function.instructions[indexToPatch].imm = target; })) {
          error = "failed to lower builtin canonical map insert helper";
          popInlineStack();
          return false;
        }
        if (requireValue) {
          function.instructions.push_back({IrOpcode::PushI32, 0});
        }
        emitFileScopeCleanup(fileScopeStack.back());
        popFileScope();
        popInlineStack();
        return true;
      }
    }

    InlineContext context;
    context.defPath = callee.fullPath;
    ir_lowerer::LowerInlineCallContextSetupStepOutput contextSetup;
    if (!ir_lowerer::runLowerInlineCallContextSetupStep(
            {
                .function = &function,
                .returnInfo = &returnInfo,
                .allocTempLocal = [&]() { return allocTempLocal(); },
            },
            contextSetup,
            error)) {
      popInlineStack();
      return false;
    }
    context.returnsVoid = contextSetup.returnsVoid;
    context.returnsArray = contextSetup.returnsArray;
    context.returnKind = contextSetup.returnKind;
    context.returnLocal = contextSetup.returnLocal;
    context.bodyScopeDepth = fileScopeStack.size();

    InlineContext *prevContext = activeInlineContext;
    if (!ir_lowerer::runLowerInlineCallActiveContextStep(
            {
                .callee = &callee,
                .structDefinition = structDef,
                .definitionReturnsVoid = context.returnsVoid,
                .activateInlineContext = [&]() { activeInlineContext = &context; },
                .restoreInlineContext = [&]() { activeInlineContext = prevContext; },
                .emitInlineStatement = [&](const Expr &stmt) {
                  return ir_lowerer::runLowerInlineCallStatementStep(
                      {
                          .function = &function,
                          .emitStatement = [&](const Expr &inlineStmt) { return emitStatement(inlineStmt, calleeLocals); },
                          .appendInstructionSourceRange = [&](const std::string &functionName,
                                                              const Expr &inlineStmt,
                                                              size_t beginIndex,
                                                              size_t endIndex) {
                            appendInstructionSourceRange(functionName, inlineStmt, beginIndex, endIndex);
                          },
                      },
                      stmt,
                      error);
                },
                .runInlineCleanup = [&]() {
                  return ir_lowerer::runLowerInlineCallCleanupStep(
                      {
                          .function = &function,
                          .returnJumps = &context.returnJumps,
                          .emitCurrentFileScopeCleanup = [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
                          .popFileScope = [&]() { popFileScope(); },
                      },
                      error);
                },
            },
            error)) {
      popInlineStack();
      return false;
    }

    if (!ir_lowerer::runLowerInlineCallReturnValueStep(
            {
                .function = &function,
                .returnsVoid = context.returnsVoid,
                .returnLocal = context.returnLocal,
                .structDefinition = structDef,
                .requireValue = requireValue,
            },
            error)) {
      popInlineStack();
      return false;
    }
    if (requireValue && context.returnsVoid && !structDef && isGeneratedMapInsertHelper) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
    }

    popInlineStack();
    return true;
}

} // namespace primec::ir_lowerer
