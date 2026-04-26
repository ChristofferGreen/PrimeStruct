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
#include "primec/Ir.h"
#include "primec/IrValidation.h"

namespace primec::ir_lowerer {

struct LowerSetupStageInput {
  const Program *program = nullptr;
  const SemanticProgram *semanticProgram = nullptr;
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
  std::vector<std::vector<int32_t>> fileScopeStack;
  std::optional<OnErrorHandler> currentOnError;
  std::optional<ResultReturnInfo> currentReturnResult;
  bool hasMathImport = false;

  SetupLocalsOrchestration setupLocalsOrchestration{};
  LowerInferenceSetupBootstrapState inferenceSetupBootstrap{};
};

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut);

} // namespace primec::ir_lowerer
