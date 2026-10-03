#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/testing/IrLowererHelpers.h"

namespace primec {

struct Definition;
struct Program;
struct SemanticProgram;

namespace ir_lowerer {

struct ReturnInfo;
struct SemanticProductIndex;
struct SemanticProductTargetAdapter;
struct UninitializedStorageAccessInfo;

struct LowerInferenceSetupBootstrapState {
  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferArrayElementKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferBufferElementKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferLiteralOrNameExprKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprBaseKind;
  std::function<CallExpressionReturnKindResolution(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>
      inferCallExprDirectReturnKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>
      inferCallExprCountAccessGpuFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprOperatorFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, std::string &, LocalInfo::ValueKind &)>
      inferCallExprControlFlowFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprPointerFallbackKind;

  std::function<const ::primec::Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const ::primec::Definition *(const Expr &)> resolveDefinitionCall;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;
  // Internal storage for inference-time errors referenced by nested setup callbacks.
  std::string inferenceError;
};

struct LowerInferenceSetupBootstrapInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;

  ExprLocalsPredicateFn isArrayCountCall = {};
  ExprLocalsPredicateFn isVectorCapacityCall = {};
  ExprLocalsPredicateFn isEntryArgsName = {};
  ExprStringFn resolveExprPath = {};
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName = {};
};

struct LowerInferenceArrayKindSetupInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;

  ExprStringFn resolveExprPath = {};
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath = {};
  ExprLocalsPredicateFn isArrayCountCall = {};
  ExprLocalsPredicateFn isStringCountCall = {};
};

struct LowerInferenceExprKindBaseSetupInput {
  GetSetupMathConstantNameFn getMathConstantName = {};
};

struct LowerInferenceExprKindCallBaseSetupInput {
  InferStructExprWithLocalsFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage = {};
};
struct LowerInferenceExprKindCallReturnSetupInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;

  ExprStringFn resolveExprPath = {};
  ExprLocalsPredicateFn isArrayCountCall = {};
  ExprLocalsPredicateFn isStringCountCall = {};
};
struct LowerInferenceExprKindCallFallbackSetupInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;

  ExprStringFn resolveExprPath = {};
  ExprLocalsPredicateFn isArrayCountCall = {};
  ExprLocalsPredicateFn isStringCountCall = {};
  ExprLocalsPredicateFn isVectorCapacityCall = {};
  ExprLocalsPredicateFn isEntryArgsName = {};
  InferSetupInferenceStructExprPathFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
};
struct LowerInferenceExprKindCallOperatorFallbackSetupInput {
  bool hasMathImport = false;
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
};
struct LowerInferenceExprKindCallControlFlowFallbackSetupInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;
  ExprStringFn resolveExprPath = {};
  LowerSetupInferenceMatchToIfFn lowerMatchToIf = {};
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
  ExprPredicateFn isBindingMutable = {};
  SetupInferenceBindingKindFn bindingKind = {};
  ExprPredicateFn hasExplicitBindingTypeTransform = {};
  SetupInferenceBindingValueKindFn bindingValueKind = {};
  ExprLocalInfoVisitorFn applyStructArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructValueInfo = {};
  InferSetupInferenceStructExprPathFn inferStructExprPath = {};
};
struct LowerInferenceExprKindCallPointerFallbackSetupInput {};
struct LowerInferenceExprKindDispatchSetupInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  ExprStringFn resolveExprPath = {};
  std::string *error = nullptr;
};

struct LowerInferenceReturnInfoSetupInput {
  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ResolveStructArrayInfoForReturnFn resolveStructArrayInfoFromPath = {};
  ExprPredicateFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  ExprPredicateFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  ExprLocalsValueKindFn inferExprKind = {};
  ExprPredicateFn isFileErrorBinding = {};
  ExprLocalInfoVisitorFn setReferenceArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructValueInfo = {};
  InferStructExprPathFromLocalsFn inferStructExprPath = {};
  ExprPredicateFn isStringBinding = {};
  ExprLocalsValueKindFn inferArrayElementKind = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
};
struct LowerInferenceGetReturnInfoStepInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const LowerInferenceReturnInfoSetupInput *returnInfoSetupInput = nullptr;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;
};
struct LowerInferenceGetReturnInfoCallbackSetupInput {
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const LowerInferenceReturnInfoSetupInput *returnInfoSetupInput = nullptr;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;
  std::string *error = nullptr;
};
struct LowerInferenceGetReturnInfoSetupInput {
  const ::primec::Program *program = nullptr;
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;
  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ResolveStructArrayInfoForReturnFn resolveStructArrayInfoFromPath = {};
  ExprPredicateFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  ExprPredicateFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  ExprLocalsValueKindFn inferExprKind = {};
  ExprPredicateFn isFileErrorBinding = {};
  ExprLocalInfoVisitorFn setReferenceArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructValueInfo = {};
  InferStructExprPathFromLocalsFn inferStructExprPath = {};
  ExprPredicateFn isStringBinding = {};
  ExprLocalsValueKindFn inferArrayElementKind = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
  std::string *error = nullptr;
};
struct LowerInferenceSetupInput {
  const ::primec::Program *program = nullptr;
  const std::unordered_map<std::string, const ::primec::Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  const ::primec::SemanticProgram *semanticProgram = nullptr;
  const SemanticProductIndex *semanticIndex = nullptr;

  ExprLocalsPredicateFn isArrayCountCall = {};
  ExprLocalsPredicateFn isStringCountCall = {};
  ExprLocalsPredicateFn isVectorCapacityCall = {};
  ExprLocalsPredicateFn isEntryArgsName = {};
  ExprStringFn resolveExprPath = {};
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName = {};
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath = {};
  InferStructExprWithLocalsFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage = {};
  bool hasMathImport = false;
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
  GetSetupMathConstantNameFn getMathConstantName = {};

  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ExprPredicateFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  ExprPredicateFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  ExprPredicateFn isFileErrorBinding = {};
  ExprLocalInfoVisitorFn setReferenceArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructArrayInfo = {};
  ExprLocalInfoVisitorFn applyStructValueInfo = {};
  ExprPredicateFn isStringBinding = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
};

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut);
bool runLowerInferenceSetup(const LowerInferenceSetupInput &input,
                            LowerInferenceSetupBootstrapState &stateOut,
                            std::string &errorOut);
bool runLowerInferenceArrayKindSetup(const LowerInferenceArrayKindSetupInput &input,
                                     LowerInferenceSetupBootstrapState &stateInOut,
                                     std::string &errorOut);
bool runLowerInferenceExprKindBaseSetup(const LowerInferenceExprKindBaseSetupInput &input,
                                        LowerInferenceSetupBootstrapState &stateInOut,
                                        std::string &errorOut);
bool runLowerInferenceExprKindCallBaseSetup(const LowerInferenceExprKindCallBaseSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut);
bool runLowerInferenceExprKindCallReturnSetup(const LowerInferenceExprKindCallReturnSetupInput &input,
                                              LowerInferenceSetupBootstrapState &stateInOut,
                                              std::string &errorOut);
bool runLowerInferenceExprKindCallFallbackSetup(const LowerInferenceExprKindCallFallbackSetupInput &input,
                                                LowerInferenceSetupBootstrapState &stateInOut,
                                                std::string &errorOut);
bool runLowerInferenceExprKindCallOperatorFallbackSetup(
    const LowerInferenceExprKindCallOperatorFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindCallControlFlowFallbackSetup(
    const LowerInferenceExprKindCallControlFlowFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindCallPointerFallbackSetup(
    const LowerInferenceExprKindCallPointerFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindDispatchSetup(const LowerInferenceExprKindDispatchSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut);
bool runLowerInferenceReturnInfoSetup(const LowerInferenceReturnInfoSetupInput &input,
                                      const ::primec::Definition &definition,
                                      ReturnInfo &infoInOut,
                                      std::string &errorOut);
bool runLowerInferenceGetReturnInfoStep(const LowerInferenceGetReturnInfoStepInput &input,
                                        const std::string &path,
                                        ReturnInfo &outInfo,
                                        std::string &errorOut);
bool runLowerInferenceGetReturnInfoCallbackSetup(const LowerInferenceGetReturnInfoCallbackSetupInput &input,
                                                 std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                                 std::string &errorOut);
bool runLowerInferenceGetReturnInfoSetup(const LowerInferenceGetReturnInfoSetupInput &input,
                                         std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                         std::string &errorOut);

} // namespace ir_lowerer

} // namespace primec
