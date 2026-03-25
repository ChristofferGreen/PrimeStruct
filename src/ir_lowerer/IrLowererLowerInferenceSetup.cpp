#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererLowerInferenceBaseKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererUninitializedTypeHelpers.h"

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
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isVectorCapacityCall = input.isVectorCapacityCall;
  const auto isEntryArgsName = input.isEntryArgsName;
  const auto resolveExprPath = input.resolveExprPath;
  const auto getBuiltinOperatorName = input.getBuiltinOperatorName;

  stateOut.resolveMethodCallDefinition = [defMap,
                                          importAliases,
                                          structNames,
                                          isArrayCountCall,
                                          isVectorCapacityCall,
                                          isEntryArgsName,
                                          resolveExprPath,
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

bool runLowerInferenceExprKindBaseSetup(const LowerInferenceExprKindBaseSetupInput &input,
                                        LowerInferenceSetupBootstrapState &stateInOut,
                                        std::string &errorOut) {
  if (!input.getMathConstantName) {
    errorOut = "native backend missing inference expr-kind base setup dependency: getMathConstantName";
    return false;
  }

  const auto getMathConstantName = input.getMathConstantName;
  stateInOut.inferLiteralOrNameExprKind =
      [getMathConstantName](const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        return inferLiteralOrNameExprKindImpl(expr, localsIn, getMathConstantName, kindOut);
      };
  return true;
}

bool runLowerInferenceExprKindCallBaseSetup(const LowerInferenceExprKindCallBaseSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut) {
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: inferStructExprPath";
    return false;
  }
  if (!input.resolveStructFieldSlot) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: resolveStructFieldSlot";
    return false;
  }
  if (!input.resolveUninitializedStorage) {
    errorOut = "native backend missing inference expr-kind call-base setup dependency: resolveUninitializedStorage";
    return false;
  }

  const auto inferStructExprPath = input.inferStructExprPath;
  const auto resolveStructFieldSlot = input.resolveStructFieldSlot;
  const auto resolveUninitializedStorage = input.resolveUninitializedStorage;
  stateInOut.inferCallExprBaseKind =
      [inferStructExprPath, resolveStructFieldSlot, resolveUninitializedStorage, &stateInOut](
          const Expr &expr, const LocalMap &localsIn, LocalInfo::ValueKind &kindOut) {
        const ResolveMethodCallWithLocalsFn resolveMethodCall =
            [&stateInOut](const Expr &candidate, const LocalMap &candidateLocals) -> const Definition * {
          return stateInOut.resolveMethodCallDefinition != nullptr
                     ? stateInOut.resolveMethodCallDefinition(candidate, candidateLocals)
                     : nullptr;
        };
        const ResolveCallDefinitionFn resolveDefinitionCall = [&stateInOut](const Expr &candidate) -> const Definition * {
          return stateInOut.resolveDefinitionCall != nullptr ? stateInOut.resolveDefinitionCall(candidate) : nullptr;
        };
        const LookupReturnInfoFn lookupReturnInfo = [&stateInOut](const std::string &path, ReturnInfo &returnInfoOut) {
          return stateInOut.getReturnInfo != nullptr ? stateInOut.getReturnInfo(path, returnInfoOut) : false;
        };
        return inferCallExprBaseKindImpl(
            expr,
            localsIn,
            inferStructExprPath,
            resolveStructFieldSlot,
            resolveUninitializedStorage,
            &resolveMethodCall,
            &resolveDefinitionCall,
            &lookupReturnInfo,
            &stateInOut.inferExprKind,
            kindOut);
      };
  return true;
}

} // namespace primec::ir_lowerer
