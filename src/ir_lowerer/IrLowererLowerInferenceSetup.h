#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/Ast.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererSetupInferenceHelpers.h"
#include "IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

struct LowerInferenceSetupBootstrapState {
  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferArrayElementKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferBufferElementKind;

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

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut);

} // namespace primec::ir_lowerer
