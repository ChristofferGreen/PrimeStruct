#include "IrLowererStatementCallHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererLowerEffects.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "buffer_store")) {
    return BufferStoreStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() != 3) {
    error = "buffer_store requires buffer, index, and value";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (stmt.args[0].kind == Expr::Kind::Name) {
    auto it = localsIn.find(stmt.args[0].name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
      elemKind = it->second.valueKind;
    }
  } else if (stmt.args[0].kind == Expr::Kind::Call && isSimpleCallName(stmt.args[0], "buffer") &&
             stmt.args[0].templateArgs.size() == 1) {
    elemKind = valueKindFromTypeName(stmt.args[0].templateArgs.front());
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_store requires numeric/bool buffer";
    return BufferStoreStatementEmitResult::Error;
  }

  const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
    error = "buffer_store requires integer index";
    return BufferStoreStatementEmitResult::Error;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(stmt.args[0], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  const int32_t valueLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({pushOneForIndex(indexKind), 1});
  instructions.push_back({addForIndex(indexKind), 0});
  instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
  instructions.push_back({mulForIndex(indexKind), 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return BufferStoreStatementEmitResult::Emitted;
}

DispatchStatementEmitResult tryEmitDispatchStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &)> &resolveExprPath,
    const std::function<const Definition *(const std::string &)> &findDefinitionByPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "dispatch")) {
    return DispatchStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() < 4) {
    error = "dispatch requires kernel and three dimension arguments";
    return DispatchStatementEmitResult::Error;
  }
  if (stmt.args.front().kind != Expr::Kind::Name) {
    error = "dispatch requires kernel name as first argument";
    return DispatchStatementEmitResult::Error;
  }

  const Expr &kernelExpr = stmt.args.front();
  const std::string kernelPath = resolveExprPath(kernelExpr);
  const Definition *kernelDef = findDefinitionByPath(kernelPath);
  if (!kernelDef) {
    error = "dispatch requires known kernel: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  bool isCompute = false;
  for (const auto &transform : kernelDef->transforms) {
    if (transform.name == "compute") {
      isCompute = true;
      break;
    }
  }
  if (!isCompute) {
    error = "dispatch requires compute definition: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  for (size_t i = 1; i <= 3; ++i) {
    const LocalInfo::ValueKind dimKind = inferExprKind(stmt.args[i], localsIn);
    if (dimKind != LocalInfo::ValueKind::Int32) {
      error = "dispatch requires i32 dimensions";
      return DispatchStatementEmitResult::Error;
    }
  }

  if (stmt.args.size() != kernelDef->parameters.size() + 4) {
    error = "dispatch argument count mismatch for " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  const int32_t gxLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gxLocal)});
  const int32_t gyLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gyLocal)});
  const int32_t gzLocal = allocTempLocal();
  if (!emitExpr(stmt.args[3], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gzLocal)});

  const int32_t xLocal = allocTempLocal();
  const int32_t yLocal = allocTempLocal();
  const int32_t zLocal = allocTempLocal();
  const int32_t gidXLocal = allocTempLocal();
  const int32_t gidYLocal = allocTempLocal();
  const int32_t gidZLocal = allocTempLocal();

  LocalMap dispatchLocals = localsIn;
  LocalInfo gidInfo;
  gidInfo.index = gidXLocal;
  gidInfo.kind = LocalInfo::Kind::Value;
  gidInfo.valueKind = LocalInfo::ValueKind::Int32;
  dispatchLocals.emplace(kGpuGlobalIdXName, gidInfo);
  gidInfo.index = gidYLocal;
  dispatchLocals.emplace(kGpuGlobalIdYName, gidInfo);
  gidInfo.index = gidZLocal;
  dispatchLocals.emplace(kGpuGlobalIdZName, gidInfo);

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  const size_t zCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gzLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t zJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  const size_t yCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gyLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t yJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  const size_t xCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gxLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t xJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidXLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidYLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidZLocal)});

  Expr kernelCall;
  kernelCall.kind = Expr::Kind::Call;
  kernelCall.name = kernelPath;
  kernelCall.namespacePrefix = kernelExpr.namespacePrefix;
  for (size_t i = 4; i < stmt.args.size(); ++i) {
    kernelCall.args.push_back(stmt.args[i]);
    kernelCall.argNames.push_back(std::nullopt);
  }
  if (!emitInlineDefinitionCall(kernelCall, *kernelDef, dispatchLocals, false)) {
    return DispatchStatementEmitResult::Error;
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(xCheck)});
  const size_t xEnd = instructions.size();
  instructions[xJumpEnd].imm = static_cast<int32_t>(xEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(yCheck)});
  const size_t yEnd = instructions.size();
  instructions[yJumpEnd].imm = static_cast<int32_t>(yEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(zCheck)});
  const size_t zEnd = instructions.size();
  instructions[zJumpEnd].imm = static_cast<int32_t>(zEnd);

  return DispatchStatementEmitResult::Emitted;
}

DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  if (stmt.isMethodCall && !isArrayCountCall(stmt, localsIn) && !isStringCountCall(stmt, localsIn) &&
      !isVectorCapacityCall(stmt, localsIn)) {
    const Definition *callee = resolveMethodCallDefinition(stmt, localsIn);
    if (!callee) {
      return DirectCallStatementEmitResult::Error;
    }
    if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
      error = "native backend does not support block arguments on calls";
      return DirectCallStatementEmitResult::Error;
    }
    if (!emitInlineDefinitionCall(stmt, *callee, localsIn, false)) {
      return DirectCallStatementEmitResult::Error;
    }
    return DirectCallStatementEmitResult::Emitted;
  }

  const Definition *callee = resolveDefinitionCall(stmt);
  if (!callee) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    error = "native backend does not support block arguments on calls";
    return DirectCallStatementEmitResult::Error;
  }

  ReturnInfo info;
  if (!getReturnInfo(callee->fullPath, info)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!emitInlineDefinitionCall(stmt, *callee, localsIn, false)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!info.returnsVoid) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return DirectCallStatementEmitResult::Emitted;
}

AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    std::vector<IrInstruction> &instructions) {
  if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
    if (!emitExpr(stmt, localsIn)) {
      return AssignOrExprStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::Pop, 0});
    return AssignOrExprStatementEmitResult::Emitted;
  }
  if (!emitExpr(stmt, localsIn)) {
    return AssignOrExprStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::Pop, 0});
  return AssignOrExprStatementEmitResult::Emitted;
}

bool buildCallableDefinitionCallContext(
    const Definition &def,
    int32_t &nextLocal,
    LocalMap &definitionLocals,
    Expr &callExpr,
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

  for (const auto &param : def.parameters) {
    LocalInfo info;
    info.index = nextLocal++;
    if (!inferParameterLocalInfo(param, definitionLocals, info, error)) {
      return false;
    }
    if (info.valueKind == LocalInfo::ValueKind::Unknown && info.structTypeName.empty()) {
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
  for (const auto &def : program.definitions) {
    if (def.fullPath == entryDef.fullPath || isStructDefinition(def) ||
        loweredCallTargets.find(def.fullPath) == loweredCallTargets.end()) {
      continue;
    }

    ReturnInfo returnInfo;
    if (!getReturnInfo(def.fullPath, returnInfo)) {
      return CallableDefinitionOrchestrationResult::Error;
    }

    function = IrFunction{};
    function.name = def.fullPath;
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
  }

  return CallableDefinitionOrchestrationResult::Emitted;
}

} // namespace primec::ir_lowerer
