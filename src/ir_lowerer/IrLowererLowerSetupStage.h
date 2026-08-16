#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "IrLowererLowerImportsStructsSetup.h"
#include "IrLowererLowerInferenceSetup.h"
#include "IrLowererLowerStatementsSourceMapStep.h"
#include "IrLowererSetupLocalsHelpers.h"
#include "primec/frontend/ExpandedSource.h"
#include "primec/ir/Ir.h"
#include "primec/ir/IrValidation.h"

namespace primec::ir_lowerer {

struct LowerSetupStageInput {
  const Program *program = nullptr;
  const SemanticProgram *semanticProgram = nullptr;
  const primec::ExpandedSource *expandedSource = nullptr;
  const std::string *entryPath = nullptr;
  const std::vector<std::string> *defaultEffects = nullptr;
  const std::vector<std::string> *entryDefaultEffects = nullptr;
  IrValidationTarget validationTarget = IrValidationTarget::Native;
  IrModule *outModule = nullptr;
};

struct LowerSetupStageState {
  const Definition *entryDef = nullptr;
  std::unordered_map<std::string, const Definition *> defMap;
  std::unordered_set<std::string> structNames;
  std::unordered_map<std::string, std::string> importAliases;
  std::unordered_map<std::string, std::vector<LayoutFieldBinding>> structFieldInfoByName;

  IrFunction function;
  bool sawReturn = false;
  LocalMap locals;
  int32_t nextLocal = 0;
  int32_t onErrorTempCounter = 0;
  std::vector<std::string> stringTable;
  std::unordered_set<std::string> loweredCallTargets;
  std::unordered_map<std::string, std::vector<InstructionSourceRange>> instructionSourceRangesByFunction;
  std::unordered_map<std::string, FunctionSyntaxProvenance> functionSyntaxProvenanceByName;
  const primec::ExpandedSource *expandedSource = nullptr;
  std::vector<std::vector<int32_t>> fileScopeStack;
  std::optional<OnErrorHandler> currentOnError;
  std::optional<ResultReturnInfo> currentReturnResult;
  bool hasMathImport = false;

  SetupLocalsOrchestration setupLocalsOrchestration{};
  LowerInferenceSetupBootstrapState inferenceSetupBootstrap{};

  // Definitions selected for real (non-inlined) Call/CallVoid emission
  // (TODO-4747 Phase 1) - see computeRealCallEligibleDefinitionPaths.
  // realCallEligibleOrder is realCallReservationIndex's keys in the fixed
  // order they were assigned reservation indices 0..N-1; both are computed
  // once here and read by the inline-call redirect and the callable-body
  // lowering loop later in the pipeline. Empty when nothing is eligible
  // (the common case today - this is a purely additive, deliberately
  // conservative candidate set).
  std::vector<std::string> realCallEligibleOrder;
  std::unordered_map<std::string, uint64_t> realCallReservationIndex;
};

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut);

} // namespace primec::ir_lowerer
