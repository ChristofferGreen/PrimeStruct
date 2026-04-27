#include "IrLowererStatementCallHelpers.h"

#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererStructTypeHelpers.h"

#include <optional>
#include <utility>

namespace primec::ir_lowerer {
namespace {

std::unordered_map<std::string, const Definition *> buildDefinitionBodyLookup(const Program &program) {
  std::unordered_map<std::string, const Definition *> definitionsByPath;
  definitionsByPath.reserve(program.definitions.size());
  for (const auto &def : program.definitions) {
    definitionsByPath.emplace(def.fullPath, &def);
  }
  return definitionsByPath;
}

} // namespace

bool buildCallableDefinitionCallContext(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    int32_t &nextLocal,
    LocalMap &definitionLocals,
    Expr &callExpr,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)> &inferParameterLocalInfo,
    std::string &error) {
  definitionLocals.clear();
  callExpr = Expr{};
  callExpr.kind = Expr::Kind::Call;
  callExpr.name = def.fullPath;
  callExpr.namespacePrefix = def.namespacePrefix;

  bool isComputeDefinition = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "compute") {
      isComputeDefinition = true;
      break;
    }
  }
  if (isComputeDefinition) {
    auto addGpuLocal = [&](const char *name) {
      LocalInfo gpuInfo;
      gpuInfo.index = nextLocal++;
      gpuInfo.kind = LocalInfo::Kind::Value;
      gpuInfo.valueKind = LocalInfo::ValueKind::Int32;
      definitionLocals.emplace(name, gpuInfo);
    };
    addGpuLocal(kGpuGlobalIdXName);
    addGpuLocal(kGpuGlobalIdYName);
    addGpuLocal(kGpuGlobalIdZName);
  }

  std::string helperParent;
  if (isStructHelperDefinition(def, structNames, helperParent) &&
      !definitionHasTransform(def, "static")) {
    Expr thisParam = makeStructHelperThisParam(
        helperParent,
        definitionHasTransform(def, "mut"));
    LocalInfo thisInfo;
    thisInfo.index = nextLocal++;
    if (!inferParameterLocalInfo(thisParam, definitionLocals, thisInfo, error)) {
      return false;
    }
    if (thisInfo.isFileHandle) {
      thisInfo.structTypeName.clear();
    } else if (!thisInfo.structTypeName.empty() &&
               thisInfo.structSlotCount <= 0) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(thisInfo.structTypeName, layout)) {
        error = "internal error: missing struct slot layout for " + thisInfo.structTypeName;
        return false;
      }
      thisInfo.structSlotCount = layout.totalSlots;
    }
    definitionLocals.emplace(thisParam.name, thisInfo);

    Expr argExpr;
    argExpr.kind = Expr::Kind::Name;
    argExpr.name = thisParam.name;
    callExpr.args.push_back(std::move(argExpr));
    callExpr.argNames.push_back(std::nullopt);
  }

  for (const auto &param : def.parameters) {
    LocalInfo info;
    info.index = nextLocal++;
    if (!inferParameterLocalInfo(param, definitionLocals, info, error)) {
      return false;
    }
    if (info.isFileHandle) {
      info.structTypeName.clear();
    } else if (!info.structTypeName.empty() && info.structSlotCount <= 0) {
      StructSlotLayoutInfo layout;
      if (!resolveStructSlotLayout(info.structTypeName, layout)) {
        error = "internal error: missing struct slot layout for " + info.structTypeName;
        return false;
      }
      info.structSlotCount = layout.totalSlots;
    }
    if (info.isArgsPack && info.argsPackElementCount < 0) {
      // Synthetic callable lowering has no concrete caller, but mixed forwarding still
      // needs a non-negative pack size to lower deterministically.
      info.argsPackElementCount = 0;
    }
    if (!info.isArgsPack && info.valueKind == LocalInfo::ValueKind::Unknown && info.structTypeName.empty()) {
      error = "native backend requires typed parameters on " + def.fullPath;
      return false;
    }
    definitionLocals.emplace(param.name, info);

    Expr argExpr;
    argExpr.kind = Expr::Kind::Name;
    argExpr.name = param.name;
    callExpr.args.push_back(std::move(argExpr));
    callExpr.argNames.push_back(std::nullopt);
  }
  return true;
}

CallableDefinitionOrchestrationResult lowerCallableDefinitionOrchestration(
    const Program &program,
    const Definition &entryDef,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const std::string &, const ReturnInfo &)> &appendReturnForDefinition,
    IrFunction &function,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    std::string &error) {
  return lowerCallableDefinitionOrchestration(
      program,
      entryDef,
      nullptr,
      loweredCallTargets,
      isStructDefinition,
      getReturnInfo,
      defaultEffects,
      entryDefaultEffects,
      isTailCallCandidate,
      resetDefinitionLoweringState,
      buildDefinitionCallContext,
      emitInlineDefinitionCall,
      appendReturnForDefinition,
      function,
      nextLocal,
      outFunctions,
      error);
}

CallableDefinitionOrchestrationResult lowerCallableDefinitionOrchestration(
    const Program &program,
    const Definition &entryDef,
    const SemanticProgram *semanticProgram,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<bool(const std::string &, const ReturnInfo &)> &appendReturnForDefinition,
    IrFunction &function,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    std::string &error) {
  const auto definitionsByPath = buildDefinitionBodyLookup(program);
  auto lowerDefinition = [&](const Definition &def) -> CallableDefinitionOrchestrationResult {
    if (def.fullPath == entryDef.fullPath || isStructDefinition(def) ||
        loweredCallTargets.find(def.fullPath) == loweredCallTargets.end()) {
      return CallableDefinitionOrchestrationResult::Emitted;
    }
    bool hasArgsPackParam = false;
    for (const auto &param : def.parameters) {
      if (isArgsPackBinding(param)) {
        hasArgsPackParam = true;
        break;
      }
    }
    if (hasArgsPackParam) {
      return CallableDefinitionOrchestrationResult::Emitted;
    }

    ReturnInfo returnInfo;
    if (!getReturnInfo(def.fullPath, returnInfo)) {
      return CallableDefinitionOrchestrationResult::Error;
    }

    function = IrFunction{};
    function.name = def.fullPath;
    if (const auto *callableSummary =
            findSemanticProductCallableSummary(semanticProgram, def.fullPath);
        callableSummary != nullptr) {
      function.metadata.effectMask = 0;
      for (const auto &effect : callableSummary->activeEffects) {
        uint64_t bit = 0;
        if (!effectBitForName(effect, bit)) {
          error = "unsupported effect in metadata: " + effect;
          return CallableDefinitionOrchestrationResult::Error;
        }
        function.metadata.effectMask |= bit;
      }

      function.metadata.capabilityMask = 0;
      for (const auto &capability : callableSummary->activeCapabilities) {
        uint64_t bit = 0;
        if (!effectBitForName(capability, bit)) {
          error = "unsupported capability in metadata: " + capability;
          return CallableDefinitionOrchestrationResult::Error;
        }
        function.metadata.capabilityMask |= bit;
      }
    } else {
      if (semanticProgram != nullptr) {
        error = "missing semantic-product callable summary: " + def.fullPath;
        return CallableDefinitionOrchestrationResult::Error;
      }
      if (!resolveEffectMask(
              def.transforms, false, defaultEffects, entryDefaultEffects, function.metadata.effectMask, error)) {
        return CallableDefinitionOrchestrationResult::Error;
      }
      if (!resolveCapabilityMask(def.transforms,
                                 resolveActiveEffects(def.transforms, false, defaultEffects, entryDefaultEffects),
                                 def.fullPath,
                                 function.metadata.capabilityMask,
                                 error)) {
        return CallableDefinitionOrchestrationResult::Error;
      }
    }
    function.metadata.schedulingScope = IrSchedulingScope::Default;
    function.metadata.instrumentationFlags = 0;
    if (hasTailExecutionCandidate(def.statements, returnInfo.returnsVoid, isTailCallCandidate)) {
      function.metadata.instrumentationFlags |= InstrumentationTailExecution;
    }

    resetDefinitionLoweringState();
    nextLocal = 0;

    LocalMap definitionLocals;
    Expr callExpr;
    if (!buildDefinitionCallContext(def, nextLocal, definitionLocals, callExpr, error)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    if (!emitInlineDefinitionCall(callExpr, def, definitionLocals, !returnInfo.returnsVoid)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    if (!appendReturnForDefinition(def.fullPath, returnInfo)) {
      return CallableDefinitionOrchestrationResult::Error;
    }
    outFunctions.push_back(std::move(function));
    return CallableDefinitionOrchestrationResult::Emitted;
  };

  if (semanticProgram != nullptr && !semanticProgram->definitions.empty()) {
    for (const auto &definitionEntry : semanticProgram->definitions) {
      const auto defIt = definitionsByPath.find(definitionEntry.fullPath);
      if (defIt == definitionsByPath.end() || defIt->second == nullptr) {
        error = "semantic product definition missing AST body: " + definitionEntry.fullPath;
        return CallableDefinitionOrchestrationResult::Error;
      }
      if (lowerDefinition(*defIt->second) == CallableDefinitionOrchestrationResult::Error) {
        return CallableDefinitionOrchestrationResult::Error;
      }
    }
    return CallableDefinitionOrchestrationResult::Emitted;
  }

  for (const auto &def : program.definitions) {
    if (lowerDefinition(def) == CallableDefinitionOrchestrationResult::Error) {
      return CallableDefinitionOrchestrationResult::Error;
    }
  }

  return CallableDefinitionOrchestrationResult::Emitted;
}

EntryCallableExecutionResult emitEntryCallableExecutionWithCleanup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    bool &sawReturn,
    std::optional<OnErrorHandler> &currentOnError,
    const std::optional<OnErrorHandler> &entryOnError,
    std::optional<ResultReturnInfo> &currentReturnResult,
    const std::optional<ResultReturnInfo> &entryResult,
    const std::function<bool(const Expr &)> &emitStatement,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  OnErrorScope entryOnErrorScope(currentOnError, entryOnError);
  ResultReturnScope entryResultScope(currentReturnResult, entryResult);
  pushFileScope();
  for (const auto &stmt : entryDef.statements) {
    if (!emitStatement(stmt)) {
      return EntryCallableExecutionResult::Error;
    }
  }
  if (!sawReturn && entryDef.returnExpr.has_value()) {
    Expr returnStmt;
    returnStmt.kind = Expr::Kind::Call;
    returnStmt.name = "return";
    returnStmt.namespacePrefix = entryDef.namespacePrefix;
    returnStmt.sourceLine = entryDef.returnExpr->sourceLine;
    returnStmt.sourceColumn = entryDef.returnExpr->sourceColumn;
    returnStmt.args.push_back(*entryDef.returnExpr);
    returnStmt.argNames.push_back(std::nullopt);
    if (!emitStatement(returnStmt)) {
      return EntryCallableExecutionResult::Error;
    }
  }
  emitCurrentFileScopeCleanup();
  popFileScope();

  if (!sawReturn) {
    if (definitionReturnsVoid) {
      instructions.push_back({IrOpcode::ReturnVoid, 0});
    } else {
      error = "native backend requires an explicit return statement";
      return EntryCallableExecutionResult::Error;
    }
  }
  return EntryCallableExecutionResult::Emitted;
}

FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(
    const Program &program,
    const Definition &entryDef,
    IrFunction &entryFunction,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    int32_t &entryIndex,
    std::string &error) {
  return finalizeEntryFunctionTableAndLowerCallables(
      program,
      entryDef,
      nullptr,
      entryFunction,
      loweredCallTargets,
      isStructDefinition,
      getReturnInfo,
      defaultEffects,
      entryDefaultEffects,
      isTailCallCandidate,
      resetDefinitionLoweringState,
      buildDefinitionCallContext,
      emitInlineDefinitionCall,
      nextLocal,
      outFunctions,
      entryIndex,
      error);
}

FunctionTableFinalizationResult finalizeEntryFunctionTableAndLowerCallables(
    const Program &program,
    const Definition &entryDef,
    const SemanticProgram *semanticProgram,
    IrFunction &entryFunction,
    const std::unordered_set<std::string> &loweredCallTargets,
    const std::function<bool(const Definition &)> &isStructDefinition,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::vector<std::string> &defaultEffects,
    const std::vector<std::string> &entryDefaultEffects,
    const std::function<bool(const Expr &)> &isTailCallCandidate,
    const std::function<void()> &resetDefinitionLoweringState,
    const std::function<bool(const Definition &, int32_t &, LocalMap &, Expr &, std::string &)> &buildDefinitionCallContext,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    int32_t &nextLocal,
    std::vector<IrFunction> &outFunctions,
    int32_t &entryIndex,
    std::string &error) {
  outFunctions.push_back(std::move(entryFunction));
  entryIndex = 0;

  const auto callableLoweringResult = lowerCallableDefinitionOrchestration(
      program,
      entryDef,
      semanticProgram,
      loweredCallTargets,
      isStructDefinition,
      getReturnInfo,
      defaultEffects,
      entryDefaultEffects,
      isTailCallCandidate,
      resetDefinitionLoweringState,
      buildDefinitionCallContext,
      emitInlineDefinitionCall,
      [&](const std::string &definitionPath, const ReturnInfo &returnInfo) {
        return emitReturnForDefinition(entryFunction.instructions, definitionPath, returnInfo, error);
      },
      entryFunction,
      nextLocal,
      outFunctions,
      error);
  if (callableLoweringResult == CallableDefinitionOrchestrationResult::Error) {
    return FunctionTableFinalizationResult::Error;
  }
  return FunctionTableFinalizationResult::Emitted;
}

} // namespace primec::ir_lowerer
