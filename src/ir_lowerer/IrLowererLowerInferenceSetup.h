#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/Ast.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererSetupMathHelpers.h"
#include "IrLowererSetupInferenceHelpers.h"
#include "IrLowererSharedTypes.h"
#include "IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

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

  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
};

struct LowerInferenceSetupBootstrapInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;

  IsArrayCountCallFn isArrayCountCall;
  IsVectorCapacityCallFn isVectorCapacityCall;
  IsEntryArgsNameFn isEntryArgsName;
  ResolveExprPathFn resolveExprPath;
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName;
};

struct LowerInferenceArrayKindSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath;
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath;
  IsArrayCountCallFn isArrayCountCall;
  IsStringCountCallFn isStringCountCall;
};

struct LowerInferenceExprKindBaseSetupInput {
  GetSetupMathConstantNameFn getMathConstantName;
};

struct LowerInferenceExprKindCallBaseSetupInput {
  InferStructExprWithLocalsFn inferStructExprPath;
  ResolveStructFieldSlotFn resolveStructFieldSlot;
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage;
};
struct LowerInferenceExprKindCallReturnSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath;
  IsArrayCountCallFn isArrayCountCall;
  IsStringCountCallFn isStringCountCall;
};

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
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

} // namespace primec::ir_lowerer
