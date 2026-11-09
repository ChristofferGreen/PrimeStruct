#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/ir_lowerer/IrLowererLowerImportsStructsSetup.h"
#include "primec/ir_lowerer/IrLowererLowerInferenceSetup.h"
#include "primec/ir_lowerer/IrLowererLowerStatementsSourceMapStep.h"
#include "primec/ir_lowerer/IrLowererSetupLocalsHelpers.h"
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
  // Lowering for the VM target: string accesses may use LoadStringByteDynamic.
  bool allowDynamicStrings = false;
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
  // Per-scope cleanup: a non-negative entry is a file handle local to close, a negative entry
  // `-(i + 1)` is `dropEntries[i]`, an owning struct local to destroy if its drop flag is set.
  std::vector<std::vector<int32_t>> fileScopeStack;
  struct DropEntry {
    int32_t ptrLocal = -1;
    int32_t flagLocal = -1;
    std::string structPath;
    // Non-zero for a builtin vector record: slots per element. Its elements are destroyed
    // (structPath names the element struct, empty when elements need no destroy) and its data
    // buffer freed.
    int32_t builtinVectorElementSlots = 0;
    // Set for a sum local: its active variant's payload is destroyed.
    const Definition *sumDef = nullptr;
  };
  std::vector<DropEntry> dropEntries;
  std::optional<OnErrorHandler> currentOnError;
  std::optional<ResultReturnInfo> currentReturnResult;
  bool hasMathImport = false;
  // Module being lowered; set by lowerer so call-site emitters can register
  // module-level tables (host imports) as they are discovered.
  IrModule *outModule = nullptr;

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
  // A call site whose callee has a real-call shape (isRealCallShapeEligible) and is not a tiny leaf appends it here
  // while lowering, so the set grows to every such definition the program actually calls.
  std::vector<std::string> realCallEligibleOrder;
  std::unordered_map<std::string, uint64_t> realCallReservationIndex;
  // Struct parameters and results and `mut` receivers take real calls (the VM and the optimizing C++ emitters).
  bool realCallAllowStructs = false;
};

bool runLowerSetupStage(const LowerSetupStageInput &input,
                        LowerSetupStageState &stateOut,
                        std::string &errorOut);

// The `copy` and `move` parameters of a real-call body under the address ABI belong to the body, as they belong to an
// inlined body (docs/spec/value-lifecycle.md, Ownership): each struct parameter that needs destroying gets a drop flag,
// set here, and a drop entry in the innermost file scope (the body's, just pushed), so the body destroys it on every
// exit unless it moves the value on. The call site only binds them.
bool registerRealCallOwnedParameters(
    const Definition &def,
    LocalMap &bodyLocals,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::vector<LowerSetupStageState::DropEntry> &dropEntries,
    std::vector<std::vector<int32_t>> &fileScopeStack,
    std::string &errorOut);

} // namespace primec::ir_lowerer
