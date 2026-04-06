#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut) {
  stateOut = LowerInferenceSetupBootstrapState{};

  if (input.defMap == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: defMap";
    return false;
  }
  if (input.importAliases == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: importAliases";
    return false;
  }
  if (input.structNames == nullptr) {
    errorOut = "native backend missing inference setup bootstrap dependency: structNames";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing inference setup bootstrap dependency: isArrayCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing inference setup bootstrap dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.isEntryArgsName) {
    errorOut = "native backend missing inference setup bootstrap dependency: isEntryArgsName";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing inference setup bootstrap dependency: resolveExprPath";
    return false;
  }
  if (!input.getBuiltinOperatorName) {
    errorOut = "native backend missing inference setup bootstrap dependency: getBuiltinOperatorName";
    return false;
  }

  const auto *defMap = input.defMap;
  const auto *importAliases = input.importAliases;
  const auto *structNames = input.structNames;
  const auto *semanticProductTargets = input.semanticProductTargets;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isVectorCapacityCall = input.isVectorCapacityCall;
  const auto isEntryArgsName = input.isEntryArgsName;
  const auto resolveExprPath = input.resolveExprPath;
  const auto getBuiltinOperatorName = input.getBuiltinOperatorName;

  stateOut.semanticProductTargets = semanticProductTargets;

  stateOut.resolveMethodCallDefinition = [defMap,
                                          importAliases,
                                          structNames,
                                          isArrayCountCall,
                                          isVectorCapacityCall,
                                          isEntryArgsName,
                                          resolveExprPath,
                                          semanticProductTargets,
                                          &stateOut,
                                          &errorOut](const Expr &callExpr,
                                                     const LocalMap &localsIn) -> const Definition * {
    return resolveMethodCallDefinitionFromExpr(callExpr,
                                               localsIn,
                                               isArrayCountCall,
                                               isVectorCapacityCall,
                                               isEntryArgsName,
                                               *importAliases,
                                               *structNames,
                                               stateOut.inferExprKind,
                                               resolveExprPath,
                                               semanticProductTargets,
                                               stateOut.getReturnInfo,
                                               *defMap,
                                               errorOut);
  };
  stateOut.resolveDefinitionCall = [defMap, resolveExprPath](const Expr &callExpr) -> const Definition * {
    return primec::ir_lowerer::resolveDefinitionCall(callExpr, *defMap, resolveExprPath);
  };

  stateOut.inferPointerTargetKind = [getBuiltinOperatorName](const Expr &expr,
                                                             const LocalMap &localsIn) -> LocalInfo::ValueKind {
    return inferPointerTargetValueKind(
        expr,
        localsIn,
        [&](const Expr &candidate, std::string &builtinName) {
          return getBuiltinOperatorName(candidate, builtinName);
        });
  };

  return true;
}

bool runLowerInferenceSetup(const LowerInferenceSetupInput &input,
                            LowerInferenceSetupBootstrapState &stateOut,
                            std::string &errorOut) {
  if (!runLowerInferenceSetupBootstrap(
          {
              .defMap = input.defMap,
              .importAliases = input.importAliases,
              .structNames = input.structNames,
              .semanticProductTargets = input.semanticProductTargets,
              .isArrayCountCall = input.isArrayCountCall,
              .isVectorCapacityCall = input.isVectorCapacityCall,
              .isEntryArgsName = input.isEntryArgsName,
              .resolveExprPath = input.resolveExprPath,
              .getBuiltinOperatorName = input.getBuiltinOperatorName,
          },
          stateOut,
          errorOut)) {
    return false;
  }

  if (!runLowerInferenceArrayKindSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallBaseSetup(
          {
              .inferStructExprPath = input.inferStructExprPath,
              .resolveStructFieldSlot = input.resolveStructFieldSlot,
              .resolveUninitializedStorage = input.resolveUninitializedStorage,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallReturnSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallFallbackSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .isArrayCountCall = input.isArrayCountCall,
              .isStringCountCall = input.isStringCountCall,
              .isVectorCapacityCall = input.isVectorCapacityCall,
              .isEntryArgsName = input.isEntryArgsName,
              .inferStructExprPath = input.inferStructExprPath,
              .resolveStructFieldSlot = input.resolveStructFieldSlot,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallOperatorFallbackSetup(
          {
              .hasMathImport = input.hasMathImport,
              .combineNumericKinds = input.combineNumericKinds,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallControlFlowFallbackSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .lowerMatchToIf = input.lowerMatchToIf,
              .combineNumericKinds = input.combineNumericKinds,
              .isBindingMutable = input.isBindingMutable,
              .bindingKind = input.bindingKind,
              .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
              .bindingValueKind = input.bindingValueKind,
              .applyStructArrayInfo = input.applyStructArrayInfo,
              .applyStructValueInfo = input.applyStructValueInfo,
              .inferStructExprPath = input.inferStructExprPath,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindCallPointerFallbackSetup({}, stateOut, errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindBaseSetup(
          {
              .getMathConstantName = input.getMathConstantName,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (!runLowerInferenceExprKindDispatchSetup(
          {
              .defMap = input.defMap,
              .resolveExprPath = input.resolveExprPath,
              .error = &errorOut,
          },
          stateOut,
          errorOut)) {
    return false;
  }
  if (input.program == nullptr) {
    errorOut = "native backend missing inference setup dependency: program";
    return false;
  }

  auto &returnInfoCache = stateOut.returnInfoCache;
  auto &returnInferenceStack = stateOut.returnInferenceStack;
  auto &inferExprKind = stateOut.inferExprKind;
  auto &inferArrayElementKind = stateOut.inferArrayElementKind;
  return runLowerInferenceGetReturnInfoSetup(
      {
          .program = input.program,
          .defMap = input.defMap,
          .returnInfoCache = &returnInfoCache,
          .returnInferenceStack = &returnInferenceStack,
          .semanticProductTargets = input.semanticProductTargets,
          .resolveStructTypeName = input.resolveStructTypeName,
          .resolveStructArrayInfoFromPath = input.resolveStructArrayInfoFromPath,
          .isBindingMutable = input.isBindingMutable,
          .bindingKind = input.bindingKind,
          .hasExplicitBindingTypeTransform = input.hasExplicitBindingTypeTransform,
          .bindingValueKind = input.bindingValueKind,
          .inferExprKind = inferExprKind,
          .isFileErrorBinding = input.isFileErrorBinding,
          .setReferenceArrayInfo = input.setReferenceArrayInfo,
          .applyStructArrayInfo = input.applyStructArrayInfo,
          .applyStructValueInfo = input.applyStructValueInfo,
          .inferStructExprPath = input.inferStructExprPath,
          .isStringBinding = input.isStringBinding,
          .inferArrayElementKind = inferArrayElementKind,
          .lowerMatchToIf = input.lowerMatchToIf,
          .error = &errorOut,
      },
      stateOut.getReturnInfo,
      errorOut);
}

} // namespace primec::ir_lowerer
