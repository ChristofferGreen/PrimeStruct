#include "IrLowererLowerReturnEmitStage.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererFileWriteHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererInlineCallContextHelpers.h"
#include "IrLowererInlineParamHelpers.h"
#include "IrLowererInlineStructArgHelpers.h"
#include "IrLowererLowerInlineCallActiveContextStep.h"
#include "IrLowererLowerInlineCallCleanupStep.h"
#include "IrLowererLowerInlineCallContextSetupStep.h"
#include "IrLowererLowerInlineCallGpuLocalsStep.h"
#include "IrLowererLowerInlineCallReturnValueStep.h"
#include "IrLowererLowerInlineCallStatementStep.h"
#include "IrLowererLowerStatementsCallsStep.h"
#include "IrLowererOnErrorHelpers.h"
#include "IrLowererOperatorArithmeticHelpers.h"
#include "IrLowererOperatorArcHyperbolicHelpers.h"
#include "IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "IrLowererOperatorComparisonHelpers.h"
#include "IrLowererOperatorConversionsAndCallsHelpers.h"
#include "IrLowererOperatorPowAbsSignHelpers.h"
#include "IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "IrLowererReturnInferenceHelpers.h"
#include "IrLowererSetupMathHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererStatementBindingHelpers.h"
#include "IrLowererStatementCallHelpers.h"
#include "IrLowererStructFieldBindingHelpers.h"
#include "IrLowererStructLayoutHelpers.h"
#include "IrLowererStructReturnPathHelpers.h"
#include "IrLowererStructTypeHelpers.h"
#include "IrLowererStringLiteralHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

#include <cstring>
#include <limits>

namespace primec::ir_lowerer {

bool runLowerReturnEmitStage(const LowerReturnEmitStageInput &input,
                             LowerReturnEmitStageState &stateOut,
                             std::string &errorOut) {
  if (input.setupStage == nullptr || input.onErrorByDef == nullptr) {
    errorOut = "native backend missing return-emit stage input";
    return false;
  }

  stateOut = LowerReturnEmitStageState{};
  std::string &error = errorOut;

  LowerSetupStageState &setupStage = *input.setupStage;
  auto &defMap = setupStage.defMap;
  auto &structNames = setupStage.structNames;
  auto &structFieldInfoByName = setupStage.structFieldInfoByName;
  IrFunction &function = setupStage.function;
  bool &sawReturn = setupStage.sawReturn;
  int32_t &nextLocal = setupStage.nextLocal;
  int32_t &onErrorTempCounter = setupStage.onErrorTempCounter;
  auto &loweredCallTargets = setupStage.loweredCallTargets;
  auto &instructionSourceRangesByFunction = setupStage.instructionSourceRangesByFunction;
  auto &fileScopeStack = setupStage.fileScopeStack;
  auto &currentOnError = setupStage.currentOnError;
  auto &currentReturnResult = setupStage.currentReturnResult;
  auto &setupLocalsOrchestration = setupStage.setupLocalsOrchestration;

  const auto &runtimeErrorAndStringLiteralSetup =
      setupLocalsOrchestration.runtimeErrorAndStringLiteralSetup;
  const auto &stringLiteralHelpers = runtimeErrorAndStringLiteralSetup.stringLiteralHelpers;
  auto &internString = stringLiteralHelpers.internString;
  auto &resolveStringTableTarget = stringLiteralHelpers.resolveStringTableTarget;

  const auto &runtimeErrorEmitters = runtimeErrorAndStringLiteralSetup.runtimeErrorEmitters;
  auto &emitArrayIndexOutOfBounds = runtimeErrorEmitters.emitArrayIndexOutOfBounds;
  auto &emitPointerIndexOutOfBounds = runtimeErrorEmitters.emitPointerIndexOutOfBounds;
  auto &emitStringIndexOutOfBounds = runtimeErrorEmitters.emitStringIndexOutOfBounds;
  auto &emitMapKeyNotFound = runtimeErrorEmitters.emitMapKeyNotFound;
  auto &emitVectorIndexOutOfBounds = runtimeErrorEmitters.emitVectorIndexOutOfBounds;
  auto &emitVectorPopOnEmpty = runtimeErrorEmitters.emitVectorPopOnEmpty;
  auto &emitVectorCapacityExceeded = runtimeErrorEmitters.emitVectorCapacityExceeded;
  auto &emitVectorReserveNegative = runtimeErrorEmitters.emitVectorReserveNegative;
  auto &emitVectorReserveExceeded = runtimeErrorEmitters.emitVectorReserveExceeded;
  auto &emitLoopCountNegative = runtimeErrorEmitters.emitLoopCountNegative;
  auto &emitPowNegativeExponent = runtimeErrorEmitters.emitPowNegativeExponent;
  auto &emitFloatToIntNonFinite = runtimeErrorEmitters.emitFloatToIntNonFinite;

  const auto &entryReturnConfig = setupLocalsOrchestration.entryReturnConfig;
  bool returnsVoid = entryReturnConfig.returnsVoid;

  const auto &entryCountAccessSetup = setupLocalsOrchestration.entryCountAccessSetup;
  const bool &hasEntryArgs = entryCountAccessSetup.hasEntryArgs;
  const std::string &entryArgsName = entryCountAccessSetup.entryArgsName;
  const auto &countAccessClassifiers = entryCountAccessSetup.classifiers;
  auto &isEntryArgsName = countAccessClassifiers.isEntryArgsName;
  auto &isArrayCountCall = countAccessClassifiers.isArrayCountCall;
  auto &isVectorCapacityCall = countAccessClassifiers.isVectorCapacityCall;
  auto &isStringCountCall = countAccessClassifiers.isStringCountCall;

  const auto &entryCallOnErrorSetup = setupLocalsOrchestration.entryCallOnErrorSetup;
  const auto &callResolutionAdapters = entryCallOnErrorSetup.callResolutionAdapters;
  const SemanticProgram *semanticProgram = callResolutionAdapters.semanticProgram;
  auto &resolveExprPath = callResolutionAdapters.resolveExprPath;
  OnErrorByDefinition &onErrorByDef = *input.onErrorByDef;

  const auto &setupMathResolvers = setupLocalsOrchestration.setupMathResolvers;
  stateOut.getMathBuiltinName = setupMathResolvers.getMathBuiltinName;
  stateOut.getMathConstantName = setupMathResolvers.getMathConstantName;

  const auto &bindingTypeAdapters = setupLocalsOrchestration.bindingTypeAdapters;
  auto &isBindingMutable = bindingTypeAdapters.isBindingMutable;
  auto &setReferenceArrayInfo = bindingTypeAdapters.setReferenceArrayInfo;
  auto &bindingKind = bindingTypeAdapters.bindingKind;
  auto &hasExplicitBindingTypeTransform = stateOut.hasExplicitBindingTypeTransform;
  hasExplicitBindingTypeTransform = bindingTypeAdapters.hasExplicitBindingTypeTransform;
  if (!hasExplicitBindingTypeTransform) {
    hasExplicitBindingTypeTransform = [](const Expr &expr) {
      return ir_lowerer::hasExplicitBindingTypeTransform(expr);
    };
  }
  auto &isStringBinding = bindingTypeAdapters.isStringBinding;
  auto &isFileErrorBinding = bindingTypeAdapters.isFileErrorBinding;
  auto &bindingValueKind = bindingTypeAdapters.bindingValueKind;

  const auto &setupTypeAndStructTypeAdapters = setupLocalsOrchestration.setupTypeAndStructTypeAdapters;
  auto &valueKindFromTypeName = setupTypeAndStructTypeAdapters.valueKindFromTypeName;
  const auto &structTypeResolutionAdapters = setupTypeAndStructTypeAdapters.structTypeResolutionAdapters;
  auto &resolveStructTypeName = structTypeResolutionAdapters.resolveStructTypeName;

  using StructSlotFieldInfo = ir_lowerer::StructSlotFieldInfo;
  const auto &structArrayInfoAdapters = setupLocalsOrchestration.structArrayInfoAdapters;
  auto &applyStructArrayInfo = structArrayInfoAdapters.applyStructArrayInfo;

  using StructSlotLayout = ir_lowerer::StructSlotLayoutInfo;
  const auto &structSlotResolutionAdapters = setupLocalsOrchestration.structSlotResolutionAdapters;
  auto &resolveStructSlotLayout = structSlotResolutionAdapters.resolveStructSlotLayout;
  auto &resolveStructFieldSlot = structSlotResolutionAdapters.resolveStructFieldSlot;

  using UninitializedStorageAccess = ir_lowerer::UninitializedStorageAccessInfo;
  const auto &uninitializedResolutionAdapters = setupLocalsOrchestration.uninitializedResolutionAdapters;
  auto &resolveUninitializedTypeInfo = uninitializedResolutionAdapters.resolveUninitializedTypeInfo;
  auto &resolveUninitializedStorage = uninitializedResolutionAdapters.resolveUninitializedStorage;
  auto &inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;

  auto &applyStructValueInfo = setupLocalsOrchestration.applyStructValueInfo;
  stateOut.hasMathImport = setupStage.hasMathImport;
  const bool &hasMathImport = stateOut.hasMathImport;
  auto &combineNumericKinds = setupTypeAndStructTypeAdapters.combineNumericKinds;
  auto &stringTable = setupStage.stringTable;

  auto &getReturnInfo = setupStage.inferenceSetupBootstrap.getReturnInfo;
  auto &inferExprKind = setupStage.inferenceSetupBootstrap.inferExprKind;
  auto &inferArrayElementKind = setupStage.inferenceSetupBootstrap.inferArrayElementKind;
  auto &resolveMethodCallDefinition = setupStage.inferenceSetupBootstrap.resolveMethodCallDefinition;

  auto &appendInstructionSourceRange = stateOut.appendInstructionSourceRange;
  appendInstructionSourceRange = [&](const std::string &functionName,
                                     const Expr &expr,
                                     size_t beginIndex,
                                     size_t endIndex) {
    if (functionName.empty() || endIndex <= beginIndex) {
      return;
    }
    InstructionSourceRange range;
    range.beginIndex = beginIndex;
    range.endIndex = endIndex;
    if (expr.sourceLine > 0) {
      range.line = static_cast<uint32_t>(expr.sourceLine);
    }
    if (expr.sourceColumn > 0) {
      range.column = static_cast<uint32_t>(expr.sourceColumn);
    }
    instructionSourceRangesByFunction[functionName].push_back(range);
  };

  using InlineContext = LowerReturnEmitInlineContext;
  auto *&activeInlineContext = stateOut.activeInlineContext;
  auto &inlineStack = stateOut.inlineStack;
  auto &emitFileErrorWhy = stateOut.emitFileErrorWhy;
  auto &emitMovePassthroughCall = stateOut.emitMovePassthroughCall;
  auto &emitUploadPassthroughCall = stateOut.emitUploadPassthroughCall;
  auto &emitReadbackPassthroughCall = stateOut.emitReadbackPassthroughCall;
  auto &emitFloatLiteral = stateOut.emitFloatLiteral;
  auto &emitCompareToZero = stateOut.emitCompareToZero;
  auto &getMathBuiltinName = stateOut.getMathBuiltinName;
  auto &getMathConstantName = stateOut.getMathConstantName;
  auto &resolveDefinitionCall = stateOut.resolveDefinitionCall;
  auto &resolveResultExprInfo = stateOut.resolveResultExprInfo;
  auto &emitStringValueForCall = stateOut.emitStringValueForCall;
  auto &emitPrintArg = stateOut.emitPrintArg;
  auto &emitExpr = stateOut.emitExpr;
  auto &emitStatement = stateOut.emitStatement;
  auto &emitInlineDefinitionCall = stateOut.emitInlineDefinitionCall;
  auto &allocTempLocal = stateOut.allocTempLocal;
  auto &emitStructCopyFromPtrs = stateOut.emitStructCopyFromPtrs;
  auto &emitStructCopySlots = stateOut.emitStructCopySlots;
  auto &emitFileScopeCleanup = stateOut.emitFileScopeCleanup;
  auto &emitFileScopeCleanupAll = stateOut.emitFileScopeCleanupAll;
  auto &pushFileScope = stateOut.pushFileScope;
  auto &popFileScope = stateOut.popFileScope;
  auto &emitBlock = stateOut.emitBlock;

  activeInlineContext = nullptr;
  emitExpr = {};
  emitStatement = {};

#include "IrLowererLowerReturnInfo.h"
#include "IrLowererLowerInlineCalls.h"
#include "IrLowererLowerEmitExpr.h"
#include "IrLowererLowerOperators.h"
#include "IrLowererLowerStatementsExpr.h"
#include "IrLowererLowerStatementsBindings.h"
#include "IrLowererLowerStatementsLoops.h"
    return ir_lowerer::runLowerStatementsCallsStep(
        {
            .inferExprKind =
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
            .emitExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            .allocTempLocal = [&]() { return allocTempLocal(); },
            .resolveExprPath = [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
            .findDefinitionByPath = [&](const std::string &path) -> const Definition * {
              auto it = defMap.find(path);
              return it == defMap.end() ? nullptr : it->second;
            },
            .isArrayCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isArrayCountCall(callExpr, callLocals);
            },
            .isStringCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isStringCountCall(callExpr, callLocals);
            },
            .isVectorCapacityCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isVectorCapacityCall(callExpr, callLocals);
            },
            .resolveMethodCallDefinition = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return resolveMethodCallDefinition(callExpr, callLocals);
            },
            .resolveDefinitionCall = [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
            .getReturnInfo =
                [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                  return getReturnInfo(definitionPath, returnInfo);
                },
            .emitInlineDefinitionCall =
                [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
                  return emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
                },
            .instructions = &function.instructions,
        },
        stmt,
        localsIn,
        error);
  };

  return true;
}

} // namespace primec::ir_lowerer
