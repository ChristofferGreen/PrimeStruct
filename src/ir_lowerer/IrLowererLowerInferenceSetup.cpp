#include "IrLowererLowerInferenceSetup.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererResultHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <vector>

namespace primec::ir_lowerer {

namespace {

LocalInfo::ValueKind semanticPointerTargetKindFromTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return LocalInfo::ValueKind::Unknown;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (base != "Pointer" && base != "Reference") {
    return LocalInfo::ValueKind::Unknown;
  }
  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return LocalInfo::ValueKind::Unknown;
  }
  return valueKindFromTypeName(trimTemplateTypeText(args.front()));
}

bool inferSemanticPointerTargetKindForName(const Expr &expr,
                                           const SemanticProgram *semanticProgram,
                                           const SemanticProductIndex *semanticIndex,
                                           LocalInfo::ValueKind &kindOut) {
  kindOut = LocalInfo::ValueKind::Unknown;
  if (expr.kind != Expr::Kind::Name || semanticProgram == nullptr ||
      semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return false;
  }
  if (const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, expr);
      bindingFact != nullptr) {
    kindOut = semanticPointerTargetKindFromTypeText(resolveSemanticProductTypeText(
        semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId));
    return true;
  }
  if (const SemanticProgramLocalAutoFact *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, expr);
      localAutoFact != nullptr) {
    kindOut = semanticPointerTargetKindFromTypeText(resolveSemanticProductTypeText(
        semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId));
    return true;
  }
  if (const SemanticProgramQueryFact *queryFact =
          findSemanticProductQueryFactBySemanticId(*semanticIndex, expr);
      queryFact != nullptr) {
    std::string typeText = resolveSemanticProductTypeText(
        semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (typeText.empty()) {
      typeText = resolveSemanticProductTypeText(
          semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId);
    }
    kindOut = semanticPointerTargetKindFromTypeText(typeText);
    return true;
  }
  return false;
}

} // namespace

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
  const auto *semanticProgram = input.semanticProgram;
  const auto *semanticIndex = input.semanticIndex;
  const auto isArrayCountCall = input.isArrayCountCall;
  const auto isVectorCapacityCall = input.isVectorCapacityCall;
  const auto isEntryArgsName = input.isEntryArgsName;
  const auto resolveExprPath = input.resolveExprPath;
  const auto getBuiltinOperatorName = input.getBuiltinOperatorName;

  stateOut.semanticProgram = semanticProgram;
  stateOut.semanticIndex = semanticIndex;

  stateOut.resolveMethodCallDefinition = [defMap,
                                          importAliases,
                                          structNames,
                                          isArrayCountCall,
                                          isVectorCapacityCall,
                                          isEntryArgsName,
                                          resolveExprPath,
                                          semanticProgram,
                                          &stateOut](const Expr &callExpr,
                                                     const LocalMap &localsIn) -> const Definition * {
    // Use a local temporary error string to avoid capturing the outer
    // `errorOut` reference which would dangle after this function returns.
    std::string localError;
    (void)localError;
    return resolveMethodCallDefinitionFromExpr(callExpr,
                                               localsIn,
                                               isArrayCountCall,
                                               isVectorCapacityCall,
                                               isEntryArgsName,
                                               *importAliases,
                                               *structNames,
                                               stateOut.inferExprKind,
                                               resolveExprPath,
                                               semanticProgram,
                                               stateOut.getReturnInfo,
                                               *defMap,
                                               localError);
  };
  stateOut.resolveDefinitionCall = [defMap, resolveExprPath](const Expr &callExpr) -> const Definition * {
    return primec::ir_lowerer::resolveDefinitionCall(callExpr, *defMap, resolveExprPath);
  };

  stateOut.inferPointerTargetKind = [getBuiltinOperatorName,
                                     semanticProgram,
                                     semanticIndex](const Expr &expr,
                                                    const LocalMap &localsIn) -> LocalInfo::ValueKind {
    LocalInfo::ValueKind semanticKind = LocalInfo::ValueKind::Unknown;
    if (inferSemanticPointerTargetKindForName(expr, semanticProgram, semanticIndex, semanticKind)) {
      return semanticKind;
    }
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
              .semanticProgram = input.semanticProgram,
              .semanticIndex = input.semanticIndex,
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
              .semanticProgram = input.semanticProgram,
              .semanticIndex = input.semanticIndex,
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
              .error = &stateOut.inferenceError,
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
          .semanticProgram = input.semanticProgram,
          .semanticIndex = input.semanticIndex,
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
          .error = &stateOut.inferenceError,
      },
      stateOut.getReturnInfo,
      errorOut);
}

} // namespace primec::ir_lowerer
