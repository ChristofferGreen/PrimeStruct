#include "primec/ir_lowerer/IrLowererLowerReturnEmitStage.h"

#include "primec/ir/SoaPathHelpers.h"
#include "primec/support/SourceLocationMapper.h"

#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererFileWriteHelpers.h"
#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererIndexKindHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineCallContextHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineParamHelpers.h"
#include "primec/ir_lowerer/IrLowererInlineStructArgHelpers.h"
#include "IrLowererLowerEmitExprCollectionHelpers.h"
#include "IrLowererLowerEmitExprPrintArg.h"
#include "IrLowererLowerEmitExprTailDispatchHelpers.h"
#include "IrLowererLowerEmitExprTryHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallActiveContextStep.h"
#include "IrLowererLowerInlineCalls.h"
#include "IrLowererLowerStatementsExprHelpers.h"
#include "IrLowererLowerSumHelpers.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallCleanupStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallContextSetupStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallGpuLocalsStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallReturnValueStep.h"
#include "primec/ir_lowerer/IrLowererLowerInlineCallStatementStep.h"
#include "primec/ir_lowerer/IrLowererLowerStatementsCallsStep.h"
#include "primec/ir_lowerer/IrLowererOnErrorHelpers.h"
#include "IrLowererRequirementContractHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorArithmeticHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorArcHyperbolicHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorClampMinMaxTrigHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorComparisonHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorConversionsAndCallsHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorPowAbsSignHelpers.h"
#include "primec/ir_lowerer/IrLowererOperatorSaturateRoundingRootsHelpers.h"
#include "primec/ir_lowerer/IrLowererReferenceCallAddress.h"
#include "primec/ir_lowerer/IrLowererReturnInferenceHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupMathHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererStatementBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererStatementCallHelpers.h"
#include "primec/ir_lowerer/IrLowererStructFieldBindingHelpers.h"
#include "primec/ir_lowerer/IrLowererStructLayoutHelpers.h"
#include "primec/ir_lowerer/IrLowererStructReturnPathHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStringLiteralHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "IrLowererTaskSpawn.h"
#include "primec/ir_lowerer/IrLowererUninitializedTypeHelpers.h"
#include "IrLowererVectorRecordLayoutHelpers.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

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
  auto &emitVectorCapacityExceeded = runtimeErrorEmitters.emitVectorCapacityExceeded;
  auto &emitVectorPopOnEmpty = runtimeErrorEmitters.emitVectorPopOnEmpty;
  auto &emitVectorIndexOutOfBounds = runtimeErrorEmitters.emitVectorIndexOutOfBounds;
  auto &emitVectorReserveNegative = runtimeErrorEmitters.emitVectorReserveNegative;
  auto &emitVectorReserveExceeded = runtimeErrorEmitters.emitVectorReserveExceeded;
  auto &emitLoopCountNegative = runtimeErrorEmitters.emitLoopCountNegative;
  auto &emitPowNegativeExponent = runtimeErrorEmitters.emitPowNegativeExponent;
  auto &emitFloatToIntNonFinite = runtimeErrorEmitters.emitFloatToIntNonFinite;

  const auto &entryReturnConfig = setupLocalsOrchestration.entryReturnConfig;
  const bool &returnsVoid = entryReturnConfig.returnsVoid;
  const int32_t &realCallStructReturnLocal = entryReturnConfig.realCallStructReturnLocal;

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
  const SemanticProgram *const &semanticProgram = callResolutionAdapters.semanticProgram;
  auto &resolveExprPath = callResolutionAdapters.resolveExprPath;

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
    if (setupStage.expandedSource != nullptr && expr.sourceLine > 0 && expr.sourceColumn > 0) {
      const std::optional<SourceUnitLocation> location =
          mapExpandedSourceLocation(*setupStage.expandedSource, expr.sourceLine, expr.sourceColumn);
      if (location.has_value()) {
        if (location->line > 0) {
          range.line = static_cast<uint32_t>(location->line);
        }
        if (location->column > 0) {
          range.column = static_cast<uint32_t>(location->column);
        }
        range.sourceUnit = location->file;
      }
    }
    instructionSourceRangesByFunction[functionName].push_back(range);
  };

  auto *&activeInlineContext = stateOut.activeInlineContext;
  auto &emitFileErrorWhy = stateOut.emitFileErrorWhy;
  auto &emitMovePassthroughCall = stateOut.emitMovePassthroughCall;
  auto &emitUploadPassthroughCall = stateOut.emitUploadPassthroughCall;
  auto &emitReadbackPassthroughCall = stateOut.emitReadbackPassthroughCall;
  auto &emitFloatLiteral = stateOut.emitFloatLiteral;
  auto &emitCompareToZero = stateOut.emitCompareToZero;
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

  ir_lowerer::SumHelpersContext sumHelpers(setupStage, stateOut, callResolutionAdapters, error);

#include "IrLowererLowerReturnInfo.h"

  // The struct path of the owning value a call produces when nobody binds it (a discarded
  // statement result, a field-access receiver): a definition returning a struct by value (not a
  // reference, pointer or constructor) that needs destroying.
  auto ownedTemporaryStructPath = [&](const Expr &callExpr, const LocalMap &locals) {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isFieldAccess ||
        isReturnCall(callExpr)) {
      return std::string{};
    }
    const Definition *callee = callExpr.isMethodCall ? resolveMethodCallDefinition(callExpr, locals)
                                                     : resolveDefinitionCall(callExpr);
    if (callee == nullptr || ir_lowerer::isStructDefinition(*callee)) {
      return std::string{};
    }
    ReturnInfo info;
    if (!getReturnInfo(callee->fullPath, info) || info.returnsVoid || info.returnsArray) {
      return std::string{};
    }
    for (const auto &transform : callee->transforms) {
      std::string base;
      std::string argText;
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          splitTemplateTypeName(
              trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
        const std::string normalizedBase =
            normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
        if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
          return std::string{};
        }
      }
    }
    std::string structPath = inferStructExprPath(callExpr, locals);
    if (structPath.empty() || sumHelpers.resolveSumDefinitionByPath(structPath) != nullptr ||
        !ir_lowerer::structNeedsDestroyHelpers(
            structPath,
            [&](const std::string &path) {
              return ir_lowerer::findStackDestroyHelper(defMap, path);
            },
            [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(path, layoutOut);
            })) {
      return std::string{};
    }
    return structPath;
  };

  // The struct path of the owning value a statement discards: a call result (above), or a
  // constructor such as `Named{...}` or a bindings-only definition called as `round()` (an
  // implicit struct), whose temporary is destroyed at the end of the statement
  // (docs/spec/value-lifecycle.md, Ownership).
  auto discardedTemporaryStructPath = [&](const Expr &stmtExpr, const LocalMap &locals) {
    if (std::string structPath = ownedTemporaryStructPath(stmtExpr, locals); !structPath.empty()) {
      return structPath;
    }
    if (stmtExpr.kind != Expr::Kind::Call || stmtExpr.isBinding ||
        !ir_lowerer::isStructConstructorCallShape(stmtExpr)) {
      return std::string{};
    }
    const Definition *callee = resolveDefinitionCall(stmtExpr);
    if (callee == nullptr || !ir_lowerer::isStructDefinition(*callee) ||
        sumHelpers.resolveSumDefinitionByPath(callee->fullPath) != nullptr ||
        !ir_lowerer::structNeedsDestroyHelpers(
            callee->fullPath,
            [&](const std::string &path) {
              return ir_lowerer::findStackDestroyHelper(defMap, path);
            },
            [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(path, layoutOut);
            })) {
      return std::string{};
    }
    return callee->fullPath;
  };

  // Gives a builtin vector pushed onto a `vector<vector<T>>` from a place a buffer of its own.
  auto emitPushedVectorCopy = [&](int32_t destPtrLocal,
                                  int32_t srcPtrLocal,
                                  const std::string &elementTypeText,
                                  const std::string &namespacePrefix,
                                  const LocalMap &locals) {
    std::string vectorElementPath;
    std::string innerElementPath;
    if (!ir_lowerer::resolveBuiltinVectorElementStructPath(
            elementTypeText,
            namespacePrefix,
            [&](const std::string &typeName, const std::string &prefix, std::string &pathOut) {
              return resolveStructTypeName(typeName, prefix, pathOut);
            },
            vectorElementPath) ||
        !ir_lowerer::isNestedBuiltinVectorElementPath(vectorElementPath, &innerElementPath)) {
      return false;
    }
    return ir_lowerer::emitBuiltinVectorCopyFromPtrs(
        destPtrLocal,
        srcPtrLocal,
        innerElementPath,
        [&](const std::string &path) -> const Definition * {
          auto copyIt = defMap.find(path + "/Copy");
          return copyIt == defMap.end() ? nullptr : copyIt->second;
        },
        [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
          return resolveStructSlotLayout(path, layoutOut);
        },
        [&]() { return allocTempLocal(); },
        function.instructions,
        locals,
        emitInlineDefinitionCall,
        error);
  };

  // The struct (or nested builtin vector) path of a vector element type that `pop` or `clear`
  // destroys when it drops an element; empty when the element owns nothing.
  auto resolveDroppedVectorElementStructPath = [&](const std::string &elementText,
                                                   const std::string &namespacePrefix,
                                                   std::string &structPathOut) {
    structPathOut.clear();
    std::string elementPath;
    if (!ir_lowerer::resolveBuiltinVectorElementStructPath(
            elementText,
            namespacePrefix,
            [&](const std::string &typeName, const std::string &prefix, std::string &pathOut) {
              return resolveStructTypeName(typeName, prefix, pathOut);
            },
            elementPath)) {
      return false;
    }
    if (!elementPath.empty() && ir_lowerer::structNeedsDestroyHelpers(
                                    elementPath,
                                    [&](const std::string &path) {
                                      return ir_lowerer::findStackDestroyHelper(defMap, path);
                                    },
                                    [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                                      return resolveStructSlotLayout(path, layoutOut);
                                    })) {
      structPathOut = elementPath;
    }
    return true;
  };

  // Destroys the value of `structPath` at the address in `valuePtrLocal`.
  auto emitStructDestroyFromPtr = [&](int32_t valuePtrLocal,
                                      const std::string &structPath,
                                      const LocalMap &locals) {
    return ir_lowerer::emitStructDestroyHelpersFromPtr(
        valuePtrLocal,
        structPath,
        [&](const std::string &path) { return ir_lowerer::findStackDestroyHelper(defMap, path); },
        [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
          return resolveStructSlotLayout(path, layoutOut);
        },
        [&]() { return allocTempLocal(); },
        function.instructions,
        locals,
        emitInlineDefinitionCall,
        error);
  };

  // Whether a struct value copied out of `sourceExpr` is a temporary the copy takes over (and
  // disarms) rather than a place that keeps owning it (an element access, unless a user `at`).
  auto isTemporaryStructCopySource = [&](const Expr &sourceExpr, const LocalMap &locals) {
    return ir_lowerer::isTemporaryStructCopySourceExpr(sourceExpr, [&](const Expr &callExpr) {
      return callExpr.isMethodCall ? resolveMethodCallDefinition(callExpr, locals)
                                   : resolveDefinitionCall(callExpr);
    });
  };

  emitInlineDefinitionCall = [&](const Expr &callExpr,
                                 const Definition &callee,
                                 const LocalMap &callerLocals,
                                 bool requireValue) -> bool {
    // The first inline call emitted owns the "give me the address" request of a consumer of a `Reference` result.
    const bool addressWanted = ir_lowerer::takeReferenceCallAddressWanted();
    if (!ir_lowerer::emitInlineDefinitionCallImpl(setupStage,
                                                  stateOut,
                                                  callResolutionAdapters,
                                                  input.diagnosticInfo,
                                                  input.onErrorByDef,
                                                  callExpr,
                                                  callee,
                                                  callerLocals,
                                                  requireValue,
                                                  error)) {
      return false;
    }
    // A value read of a call that returns `Reference<T>` (T a scalar) loads the referent from the address the
    // call leaves; a consumer that wants the address kept it (ReferenceCallAddressScope).
    std::string referentType;
    if (requireValue && !addressWanted &&
        ir_lowerer::definitionReturnsReference(callee, referentType) &&
        valueKindFromTypeName(referentType) != LocalInfo::ValueKind::Unknown) {
      setupStage.function.instructions.push_back({IrOpcode::LoadIndirect, 0});
    }
    return true;
  };

#include "IrLowererLowerEmitExpr.h"
#include "IrLowererLowerEmitExprContinued.h"
#include "IrLowererLowerOperators.h"

  ir_lowerer::StatementsExprContext statementsExprHelpers(setupStage, stateOut, callResolutionAdapters, error);

#include "IrLowererLowerStatementsExpr.h"
#include "IrLowererLowerStatementsExprContinued.h"

  emitPrintArg = [&](const Expr &printArgExpr, const LocalMap &printArgLocals,
                     const PrintBuiltin &printBuiltin) -> bool {
    return ir_lowerer::emitPrintArgImpl(setupStage, stateOut, callResolutionAdapters, printArgExpr,
                                        printArgLocals, printBuiltin, error);
  };

#include "IrLowererLowerStatementsBindings.h"
#include "IrLowererLowerStatementsBindingsContinued.h"
#include "IrLowererLowerStatementsLoops.h"
    const bool lowered = ir_lowerer::runLowerStatementsCallsStep(
        {
            .semanticProgram = semanticProgram,
            .semanticIndex = &callResolutionAdapters.semanticProductTargets.semanticIndex,
            .inferExprKind =
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
            .inferStructExprPath =
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return inferStructExprPath(valueExpr, valueLocals);
                },
            .emitExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            .allocTempLocal = [&]() { return allocTempLocal(); },
            .resolveExprPath = [&](const Expr &valueExpr) { return resolveExprPath(valueExpr); },
            .findDefinitionByPath = [&](const std::string &path) -> const Definition * {
              auto it = defMap.find(path);
              return it == defMap.end() ? nullptr : it->second;
            },
            .resolveDestroyHelperForStruct = [](const std::string &) -> const Definition * { return nullptr; },
            .resolveMoveHelperForStruct = [](const std::string &) -> const Definition * { return nullptr; },
            .isArrayCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isArrayCountCall(callExpr, callLocals);
            },
            .isStringCountCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isStringCountCall(callExpr, callLocals);
            },
            .isVectorCapacityCall = [&](const Expr &callExpr, const LocalMap &callLocals) {
              return isVectorCapacityCall(callExpr, callLocals);
            },
            .resolveStructSlotLayout =
                [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                  return resolveStructSlotLayout(structPath, layoutOut);
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
            .emitArrayIndexOutOfBounds = [&]() { emitArrayIndexOutOfBounds(); },
            .emitVectorCapacityExceeded = [&]() { emitVectorCapacityExceeded(); },
            .emitVectorPopOnEmpty = [&]() { emitVectorPopOnEmpty(); },
            .emitVectorIndexOutOfBounds = [&]() { emitVectorIndexOutOfBounds(); },
            .emitVectorReserveNegative = [&]() { emitVectorReserveNegative(); },
            .emitVectorReserveExceeded = [&]() { emitVectorReserveExceeded(); },
            .emitPushedVectorCopy = emitPushedVectorCopy,
            .emitPushedStructCopy =
                [&](int32_t destPtrLocal, int32_t srcPtrLocal, const std::string &structPath,
                    const LocalMap &callLocals) {
                  bool ranCopyHelper = false;
                  return ir_lowerer::emitStructCopyHelpersFromPtrs(
                      destPtrLocal,
                      srcPtrLocal,
                      structPath,
                      [&](const std::string &path) -> const Definition * {
                        auto copyIt = defMap.find(path + "/Copy");
                        return copyIt == defMap.end() ? nullptr : copyIt->second;
                      },
                      [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                        return resolveStructSlotLayout(path, layoutOut);
                      },
                      [&]() { return allocTempLocal(); },
                      function.instructions,
                      callLocals,
                      emitInlineDefinitionCall,
                      ranCopyHelper,
                      error);
                },
            .isTemporaryStructSource =
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return isTemporaryStructCopySource(valueExpr, valueLocals);
                },
            .resolveDroppedVectorElementStructPath = resolveDroppedVectorElementStructPath,
            .emitStructDestroyFromPtr = emitStructDestroyFromPtr,
            .instructions = &function.instructions,
        },
        stmt,
        localsIn,
        error);
    if (!lowered && error.empty()) {
      error = "IR lower return-emit statements call step failed without diagnostic";
    }
    return lowered;
  };

  // A statement destroys the hidden locals its expressions made (a vector temporary an element was read
  // from) once it is lowered; a scope that ended inside the statement has destroyed its own already. A
  // `return` leaves them to the scope: its exit path destroys them after the value is copied out.
  emitStatement = [&, emitStatementBody = std::move(emitStatement)](const Expr &stmt,
                                                                    LocalMap &localsIn) -> bool {
    const size_t firstTemporary = setupStage.statementTemporaryDrops.size();
    ++setupStage.statementNesting;
    const bool lowered = emitStatementBody(stmt, localsIn);
    --setupStage.statementNesting;
    while (setupStage.statementTemporaryDrops.size() > firstTemporary) {
      const int32_t scopeEntry = setupStage.statementTemporaryDrops.back();
      setupStage.statementTemporaryDrops.pop_back();
      if (!lowered || isReturnCall(stmt)) {
        continue;
      }
      for (auto &scope : fileScopeStack) {
        const auto found = std::find(scope.begin(), scope.end(), scopeEntry);
        if (found == scope.end()) {
          continue;
        }
        scope.erase(found);
        const LowerSetupStageState::DropEntry entry =
            setupStage.dropEntries[static_cast<size_t>(-scopeEntry - 1)];
        (void)emitDropEntryCleanup(entry);
        break;
      }
    }
    return lowered;
  };

  if (input.consumeStage) {
    return input.consumeStage(stateOut, error);
  }

  return true;
}

} // namespace primec::ir_lowerer
