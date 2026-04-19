#include "primec/IrLowerer.h"
#include "primec/Lexer.h"
#include "primec/Parser.h"
#include "primec/StringLiteral.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererFileWriteHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererInlineCallContextHelpers.h"
#include "IrLowererInlineParamHelpers.h"
#include "IrLowererInlineStructArgHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererLowerReturnEmitStage.h"
#include "IrLowererLowerSetupStage.h"
#include "IrLowererLowerStatementsCallsStage.h"
#include "IrLowererOnErrorHelpers.h"
#include "IrLowererOperatorArithmeticHelpers.h"
#include "IrLowererOperatorArcHyperbolicHelpers.h"
#include "IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "IrLowererOperatorComparisonHelpers.h"
#include "IrLowererOperatorConversionsAndCallsHelpers.h"
#include "IrLowererOperatorPowAbsSignHelpers.h"
#include "IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "IrLowererReturnInferenceHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererRuntimeErrorHelpers.h"
#include "IrLowererSetupInferenceHelpers.h"
#include "IrLowererSetupLocalsHelpers.h"
#include "IrLowererSetupMathHelpers.h"
#include "IrLowererStatementBindingHelpers.h"
#include "IrLowererStatementCallHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"
#include "IrLowererStructLayoutHelpers.h"
#include "IrLowererStructReturnPathHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"
#include "IrLowererStringLiteralHelpers.h"
#include "IrLowererSharedTypes.h"
#include "IrLowererStringCallHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace primec {
using namespace ir_lowerer;

bool IrLowerer::lower(const Program &program,
                      const SemanticProgram *semanticProgram,
                      const std::string &entryPath,
                      const std::vector<std::string> &defaultEffects,
                      const std::vector<std::string> &entryDefaultEffects,
                      IrModule &out,
                      std::string &error,
                      DiagnosticSinkReport *diagnosticInfo) const {
  out = IrModule{};
  error.clear();
  DiagnosticSink diagnosticSink(diagnosticInfo);
  diagnosticSink.reset();
  bool loweringSucceeded = false;
  struct LoweringDiagnosticScope {
    DiagnosticSink &diagnosticSink;
    std::string &error;
    bool &loweringSucceeded;

    ~LoweringDiagnosticScope() {
      if (!loweringSucceeded && !error.empty()) {
        diagnosticSink.setSummary(error);
      }
    }
  } loweringDiagnosticScope{diagnosticSink, error, loweringSucceeded};

  if (semanticProgram == nullptr) {
    error = "semantic product is required for IR lowering";
    return false;
  }

  ir_lowerer::LowerSetupStageState setupStage;
  if (!ir_lowerer::runLowerSetupStage(
          {
              .program = &program,
              .semanticProgram = semanticProgram,
              .entryPath = &entryPath,
              .defaultEffects = &defaultEffects,
              .entryDefaultEffects = &entryDefaultEffects,
              .outModule = &out,
          },
          setupStage,
          error)) {
    return false;
  }

  const Definition *entryDef = setupStage.entryDef;
  auto &defMap = setupStage.defMap;
  auto &structNames = setupStage.structNames;
  IrFunction &function = setupStage.function;
  bool &sawReturn = setupStage.sawReturn;
  LocalMap &locals = setupStage.locals;
  int32_t &nextLocal = setupStage.nextLocal;
  int32_t &onErrorTempCounter = setupStage.onErrorTempCounter;
  auto &stringTable = setupStage.stringTable;
  auto &loweredCallTargets = setupStage.loweredCallTargets;
  auto &instructionSourceRangesByFunction = setupStage.instructionSourceRangesByFunction;
  auto &fileScopeStack = setupStage.fileScopeStack;
  auto &currentOnError = setupStage.currentOnError;
  auto &currentReturnResult = setupStage.currentReturnResult;
  auto &setupLocalsOrchestration = setupStage.setupLocalsOrchestration;

  const auto &entryReturnConfig = setupLocalsOrchestration.entryReturnConfig;
  bool returnsVoid = entryReturnConfig.returnsVoid;
  ResultReturnInfo entryResultInfo = entryReturnConfig.resultInfo;
  bool entryHasResultInfo = entryReturnConfig.hasResultInfo;

  const auto &runtimeErrorAndStringLiteralSetup = setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup;
  (void)runtimeErrorAndStringLiteralSetup;

  const auto &entryCountAccessSetup = setupLocalsOrchestration.entryCountAccessSetup;
  const auto &countAccessClassifiers = entryCountAccessSetup.classifiers;
  auto isArrayCountCall = countAccessClassifiers.isArrayCountCall;
  auto isVectorCapacityCall = countAccessClassifiers.isVectorCapacityCall;
  auto isStringCountCall = countAccessClassifiers.isStringCountCall;

  const auto &entryCallOnErrorSetup = setupLocalsOrchestration.entryCallOnErrorSetup;
  const auto &callResolutionAdapters = entryCallOnErrorSetup.callResolutionAdapters;
  auto resolveExprPath = callResolutionAdapters.resolveExprPath;
  auto isTailCallCandidate = callResolutionAdapters.isTailCallCandidate;
  if (entryCallOnErrorSetup.hasTailExecution) {
    function.metadata.instrumentationFlags |= InstrumentationTailExecution;
  }
  OnErrorByDefinition onErrorByDef = entryCallOnErrorSetup.onErrorByDefinition;

  const auto &setupMathResolvers = setupLocalsOrchestration.setupMathResolvers;
  (void)setupMathResolvers;

  const auto &bindingTypeAdapters = setupLocalsOrchestration.bindingTypeAdapters;
  auto isBindingMutable = bindingTypeAdapters.isBindingMutable;
  auto setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo;
  auto bindingKind = bindingTypeAdapters.bindingKind;
  auto hasExplicitBindingTypeTransform = bindingTypeAdapters.hasExplicitBindingTypeTransform;
  if (!hasExplicitBindingTypeTransform) {
    hasExplicitBindingTypeTransform = [](const Expr &expr) {
      return ir_lowerer::hasExplicitBindingTypeTransform(expr);
    };
  }
  auto isStringBinding = bindingTypeAdapters.isStringBinding;
  auto isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding;
  auto bindingValueKind = bindingTypeAdapters.bindingValueKind;

  const auto &setupTypeAndStructTypeAdapters = setupLocalsOrchestration.setupTypeAndStructTypeAdapters;
  (void)setupTypeAndStructTypeAdapters;

  const auto &structArrayInfoAdapters = setupLocalsOrchestration.structArrayInfoAdapters;
  auto applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo;

  const auto &structSlotResolutionAdapters = setupLocalsOrchestration.structSlotResolutionAdapters;
  auto resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;

  const auto &uninitializedResolutionAdapters = setupLocalsOrchestration.uninitializedResolutionAdapters;
  (void)uninitializedResolutionAdapters;

  auto applyStructValueInfo = setupLocalsOrchestration.applyStructValueInfo;

  ir_lowerer::LowerReturnEmitStageState returnEmitStage;
  if (!ir_lowerer::runLowerReturnEmitStage(
          {
              .setupStage = &setupStage,
              .onErrorByDef = &onErrorByDef,
          },
          returnEmitStage,
          error)) {
    return false;
  }

  auto &getReturnInfo = setupStage.inferenceSetupBootstrap.getReturnInfo;
  auto &inferExprKind = setupStage.inferenceSetupBootstrap.inferExprKind;
  auto &resolveMethodCallDefinition = setupStage.inferenceSetupBootstrap.resolveMethodCallDefinition;

  auto &emitExpr = returnEmitStage.emitExpr;
  auto &emitStatement = returnEmitStage.emitStatement;
  auto &allocTempLocal = returnEmitStage.allocTempLocal;
  auto &activeInlineContext = returnEmitStage.activeInlineContext;
  auto &inlineStack = returnEmitStage.inlineStack;
  auto &appendInstructionSourceRange = returnEmitStage.appendInstructionSourceRange;
  auto &emitFileScopeCleanup = returnEmitStage.emitFileScopeCleanup;
  auto &pushFileScope = returnEmitStage.pushFileScope;
  auto &popFileScope = returnEmitStage.popFileScope;
  auto &resolveDefinitionCall = returnEmitStage.resolveDefinitionCall;
  auto &emitInlineDefinitionCall = returnEmitStage.emitInlineDefinitionCall;

  if (!ir_lowerer::runLowerStatementsCallsStage(
          {
              .program = &program,
              .entryDef = entryDef,
              .semanticProgram = semanticProgram,
              .defaultEffects = &defaultEffects,
              .entryDefaultEffects = &entryDefaultEffects,
              .defMap = &defMap,
              .structNames = &structNames,
              .onErrorByDef = &onErrorByDef,
              .stringTable = &stringTable,
              .loweredCallTargets = &loweredCallTargets,
              .instructionSourceRangesByFunction = &instructionSourceRangesByFunction,
              .currentOnError = &currentOnError,
              .currentReturnResult = &currentReturnResult,
              .sawReturn = &sawReturn,
              .nextLocal = &nextLocal,
              .onErrorTempCounter = &onErrorTempCounter,
              .returnsVoid = returnsVoid,
              .entryHasResultInfo = entryHasResultInfo,
              .entryResultInfo = &entryResultInfo,
              .function = &function,
              .locals = &locals,
              .outModule = &out,
              .inferExprKind =
                  [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
              .emitExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                return emitExpr(valueExpr, valueLocals);
              },
              .emitStatement = [&](const Expr &stmt, LocalMap &localsIn) { return emitStatement(stmt, localsIn); },
              .allocTempLocal = [&]() { return allocTempLocal(); },
              .appendInstructionSourceRange = appendInstructionSourceRange,
              .pushFileScope = [&]() { pushFileScope(); },
              .emitCurrentFileScopeCleanup = [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
              .popFileScope = [&]() { popFileScope(); },
              .resolveExprPath = [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
              .resolveMethodCallDefinition =
                  [&](const Expr &callExpr, const LocalMap &callLocals) {
                    return resolveMethodCallDefinition(callExpr, callLocals);
                  },
              .resolveDefinitionCall = [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
              .getReturnInfo =
                  [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                    return getReturnInfo(definitionPath, returnInfo);
                  },
              .emitInlineDefinitionCall =
                  [&](const Expr &callExpr, const Definition &def, const LocalMap &definitionLocals, bool expectValue) {
                    return emitInlineDefinitionCall(callExpr, def, definitionLocals, expectValue);
                  },
              .isTailCallCandidate = [&](const Expr &expr) { return isTailCallCandidate(expr); },
              .isStructDefinition = [&](const Definition &def) { return isStructDefinition(def); },
              .isArrayCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
                return isArrayCountCall(callExpr, callLocals);
              },
              .isStringCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
                return isStringCountCall(callExpr, callLocals);
              },
              .isVectorCapacityCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
                return isVectorCapacityCall(callExpr, callLocals);
              },
              .buildDefinitionCallContext =
                  [&](const Definition &def,
                      int32_t &definitionNextLocal,
                      LocalMap &definitionLocals,
                      Expr &callExpr,
                      std::string &callError) {
                    return ir_lowerer::buildCallableDefinitionCallContext(
                        def,
                        structNames,
                        definitionNextLocal,
                        definitionLocals,
                        callExpr,
                        [&](const std::string &structPath, ir_lowerer::StructSlotLayoutInfo &layoutOut) {
                          return resolveStructSlotLayout(structPath, layoutOut);
                        },
                        [&](const Expr &param,
                            const LocalMap &localsForKindInference,
                            LocalInfo &info,
                            std::string &inferError) {
                          return ir_lowerer::inferCallParameterLocalInfo(param,
                                                                         localsForKindInference,
                                                                         isBindingMutable,
                                                                         hasExplicitBindingTypeTransform,
                                                                         bindingKind,
                                                                         bindingValueKind,
                                                                         inferExprKind,
                                                                         isFileErrorBinding,
                                                                         setReferenceArrayInfo,
                                                                         applyStructArrayInfo,
                                                                         applyStructValueInfo,
                                                                         isStringBinding,
                                                                         info,
                                                                         inferError,
                                                                         [&](const Expr &nestedCallExpr,
                                                                             const LocalMap &nestedCallLocals) {
                                                                           return resolveMethodCallDefinition(
                                                                               nestedCallExpr, nestedCallLocals);
                                                                         },
                                                                         [&](const Expr &nestedCallExpr) {
                                                                           return resolveDefinitionCall(nestedCallExpr);
                                                                         },
                                                                         [&](const std::string &definitionPath,
                                                                             ReturnInfo &returnInfo) {
                                                                           return getReturnInfo(definitionPath, returnInfo);
                                                                         },
                                                                         callResolutionAdapters.semanticProgram,
                                                                         &callResolutionAdapters.semanticProductTargets.semanticIndex);
                        },
                        callError);
                  },
              .resetDefinitionLoweringState = [&]() {
                onErrorTempCounter = 0;
                sawReturn = false;
                activeInlineContext = nullptr;
                inlineStack.clear();
                fileScopeStack.clear();
                currentOnError.reset();
                currentReturnResult.reset();
              },
          },
          error)) {
    return false;
  }

  error.clear();
  loweringSucceeded = true;
  return true;
}

} // namespace primec
