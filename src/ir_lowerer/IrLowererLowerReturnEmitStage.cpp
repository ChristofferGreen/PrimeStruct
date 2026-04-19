#include "IrLowererLowerReturnEmitStage.h"

#include "IrLowererHelpers.h"
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
#include "IrLowererOnErrorHelpers.h"
#include "IrLowererStructLayoutHelpers.h"

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
  IrFunction &function = setupStage.function;
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
  auto &emitVectorIndexOutOfBounds = runtimeErrorEmitters.emitVectorIndexOutOfBounds;
  auto &emitVectorPopOnEmpty = runtimeErrorEmitters.emitVectorPopOnEmpty;
  auto &emitVectorCapacityExceeded = runtimeErrorEmitters.emitVectorCapacityExceeded;
  auto &emitVectorReserveNegative = runtimeErrorEmitters.emitVectorReserveNegative;
  auto &emitVectorReserveExceeded = runtimeErrorEmitters.emitVectorReserveExceeded;
  auto &emitPowNegativeExponent = runtimeErrorEmitters.emitPowNegativeExponent;
  auto &emitFloatToIntNonFinite = runtimeErrorEmitters.emitFloatToIntNonFinite;

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
  auto &resolveExprPath = callResolutionAdapters.resolveExprPath;
  OnErrorByDefinition &onErrorByDef = *input.onErrorByDef;

  const auto &setupMathResolvers = setupLocalsOrchestration.setupMathResolvers;
  auto &getMathConstantName = setupMathResolvers.getMathConstantName;

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

  const auto &uninitializedResolutionAdapters = setupLocalsOrchestration.uninitializedResolutionAdapters;
  auto &inferStructExprPath = uninitializedResolutionAdapters.inferStructExprPath;

  auto &applyStructValueInfo = setupLocalsOrchestration.applyStructValueInfo;

  auto &getReturnInfo = setupStage.inferenceSetupBootstrap.getReturnInfo;
  auto &inferExprKind = setupStage.inferenceSetupBootstrap.inferExprKind;
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
  auto &resolveDefinitionCall = stateOut.resolveDefinitionCall;
  auto &resolveResultExprInfo = stateOut.resolveResultExprInfo;
  auto &emitStringValueForCall = stateOut.emitStringValueForCall;
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

  return true;
}

} // namespace primec::ir_lowerer
