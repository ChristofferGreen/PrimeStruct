#include "IrLowererFlowHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace primec::ir_lowerer {

OnErrorScope::OnErrorScope(std::optional<OnErrorHandler> &targetIn, std::optional<OnErrorHandler> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

OnErrorScope::~OnErrorScope() {
  target = std::move(previous);
}

ResultReturnScope::ResultReturnScope(std::optional<ResultReturnInfo> &targetIn, std::optional<ResultReturnInfo> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

ResultReturnScope::~ResultReturnScope() {
  target = std::move(previous);
}

void emitFileCloseIfValid(std::vector<IrInstruction> &instructions, int32_t localIndex) {
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
  instructions.push_back({IrOpcode::PushI64, 0});
  instructions.push_back({IrOpcode::CmpGeI64, 0});
  const size_t jumpSkip = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIndex)});
  instructions.push_back({IrOpcode::FileClose, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  const size_t skipIndex = instructions.size();
  instructions[jumpSkip].imm = static_cast<int32_t>(skipIndex);
}

void emitFileScopeCleanup(std::vector<IrInstruction> &instructions, const std::vector<int32_t> &scope) {
  for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
    emitFileCloseIfValid(instructions, *it);
  }
}

void emitAllFileScopeCleanup(std::vector<IrInstruction> &instructions,
                             const std::vector<std::vector<int32_t>> &fileScopeStack) {
  for (auto it = fileScopeStack.rbegin(); it != fileScopeStack.rend(); ++it) {
    emitFileScopeCleanup(instructions, *it);
  }
}

bool emitStructCopyFromPtrs(std::vector<IrInstruction> &instructions,
                            int32_t destPtrLocal,
                            int32_t srcPtrLocal,
                            int32_t slotCount) {
  if (slotCount <= 0) {
    return true;
  }
  for (int32_t i = 0; i < slotCount; ++i) {
    const uint64_t offsetBytes = static_cast<uint64_t>(i * 16);
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    if (offsetBytes != 0) {
      instructions.push_back({IrOpcode::PushI64, offsetBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return true;
}

bool emitStructCopySlots(std::vector<IrInstruction> &instructions,
                         int32_t destBaseLocal,
                         int32_t srcPtrLocal,
                         int32_t slotCount,
                         const std::function<int32_t()> &allocTempLocal) {
  const int32_t destPtrLocal = allocTempLocal();
  instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(destBaseLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
  return emitStructCopyFromPtrs(instructions, destPtrLocal, srcPtrLocal, slotCount);
}

bool emitCompareToZero(std::vector<IrInstruction> &instructions,
                       LocalInfo::ValueKind kind,
                       bool equals,
                       std::string &error) {
  if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
    instructions.push_back({IrOpcode::PushI64, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::PushF64, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqF64 : IrOpcode::CmpNeF64, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Float32) {
    instructions.push_back({IrOpcode::PushF32, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqF32 : IrOpcode::CmpNeF32, 0});
    return true;
  }
  error = "boolean conversion requires numeric operand";
  return false;
}

bool emitFloatLiteral(std::vector<IrInstruction> &instructions, const Expr &expr, std::string &error) {
  char *end = nullptr;
  const char *text = expr.floatValue.c_str();
  double value = std::strtod(text, &end);
  if (end == text || (end && *end != '\0')) {
    error = "invalid float literal";
    return false;
  }
  if (expr.floatWidth == 64) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    instructions.push_back({IrOpcode::PushF64, bits});
    return true;
  }
  float f32 = static_cast<float>(value);
  uint32_t bits = 0;
  std::memcpy(&bits, &f32, sizeof(bits));
  instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
  return true;
}

bool emitReturnForDefinition(std::vector<IrInstruction> &instructions,
                             const std::string &defPath,
                             const ReturnInfo &returnInfo,
                             std::string &error) {
  if (returnInfo.returnsVoid) {
    instructions.push_back({IrOpcode::ReturnVoid, 0});
    return true;
  }
  if (returnInfo.returnsArray || returnInfo.kind == LocalInfo::ValueKind::Int64 ||
      returnInfo.kind == LocalInfo::ValueKind::UInt64 || returnInfo.kind == LocalInfo::ValueKind::String) {
    instructions.push_back({IrOpcode::ReturnI64, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Int32 || returnInfo.kind == LocalInfo::ValueKind::Bool) {
    instructions.push_back({IrOpcode::ReturnI32, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Float32) {
    instructions.push_back({IrOpcode::ReturnF32, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::ReturnF64, 0});
    return true;
  }
  error = "native backend does not support return type on " + defPath;
  return false;
}

const char *resolveGpuBuiltinLocalName(const std::string &gpuBuiltin) {
  if (gpuBuiltin == "global_id_x") {
    return kGpuGlobalIdXName;
  }
  if (gpuBuiltin == "global_id_y") {
    return kGpuGlobalIdYName;
  }
  if (gpuBuiltin == "global_id_z") {
    return kGpuGlobalIdZName;
  }
  return nullptr;
}

bool emitGpuBuiltinLoad(
    const std::string &gpuBuiltin,
    const std::function<std::optional<int32_t>(const char *)> &resolveLocalIndex,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  const char *localName = resolveGpuBuiltinLocalName(gpuBuiltin);
  if (!localName) {
    error = "native backend does not support gpu builtin: " + gpuBuiltin;
    return false;
  }

  const std::optional<int32_t> localIndex = resolveLocalIndex(localName);
  if (!localIndex.has_value()) {
    error = "gpu builtin requires dispatch context: " + gpuBuiltin;
    return false;
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(*localIndex));
  return true;
}

UnaryPassthroughCallResult tryEmitUnaryPassthroughCall(const Expr &expr,
                                                       const char *callName,
                                                       const std::function<bool(const Expr &)> &emitExpr,
                                                       std::string &error) {
  if (!isSimpleCallName(expr, callName)) {
    return UnaryPassthroughCallResult::NotMatched;
  }
  if (expr.args.size() != 1) {
    error = std::string(callName) + " requires exactly one argument";
    return UnaryPassthroughCallResult::Error;
  }
  if (!emitExpr(expr.args.front())) {
    return UnaryPassthroughCallResult::Error;
  }
  return UnaryPassthroughCallResult::Emitted;
}

bool resolveCountedLoopKind(LocalInfo::ValueKind inferredKind,
                            bool allowBool,
                            const char *errorMessage,
                            LocalInfo::ValueKind &countKindOut,
                            std::string &error) {
  countKindOut = inferredKind;
  if (allowBool && countKindOut == LocalInfo::ValueKind::Bool) {
    countKindOut = LocalInfo::ValueKind::Int32;
  }
  if (countKindOut == LocalInfo::ValueKind::Int32 ||
      countKindOut == LocalInfo::ValueKind::Int64 ||
      countKindOut == LocalInfo::ValueKind::UInt64) {
    return true;
  }
  error = errorMessage;
  return false;
}

bool emitCountedLoopPrologue(
    LocalInfo::ValueKind countKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, int32_t)> &patchInstructionImm,
    const std::function<void()> &emitLoopCountNegative,
    CountedLoopControl &out,
    std::string &error) {
  if (countKind != LocalInfo::ValueKind::Int32 &&
      countKind != LocalInfo::ValueKind::Int64 &&
      countKind != LocalInfo::ValueKind::UInt64) {
    error = "counted loop requires integer count";
    return false;
  }

  out = CountedLoopControl{};
  out.counterLocal = allocTempLocal();
  out.countKind = countKind;
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(out.counterLocal));

  if (countKind == LocalInfo::ValueKind::Int32) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(out.counterLocal));
    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::CmpLtI32, 0);
    const size_t jumpNonNegative = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitLoopCountNegative();
    const size_t nonNegativeIndex = instructionCount();
    patchInstructionImm(jumpNonNegative, static_cast<int32_t>(nonNegativeIndex));
  } else if (countKind == LocalInfo::ValueKind::Int64) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(out.counterLocal));
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpLtI64, 0);
    const size_t jumpNonNegative = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitLoopCountNegative();
    const size_t nonNegativeIndex = instructionCount();
    patchInstructionImm(jumpNonNegative, static_cast<int32_t>(nonNegativeIndex));
  }

  out.checkIndex = instructionCount();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(out.counterLocal));
  if (countKind == LocalInfo::ValueKind::Int32) {
    emitInstruction(IrOpcode::PushI32, 0);
    emitInstruction(IrOpcode::CmpGtI32, 0);
  } else if (countKind == LocalInfo::ValueKind::Int64) {
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpGtI64, 0);
  } else {
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpNeI64, 0);
  }
  out.jumpEndIndex = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  return true;
}

void emitCountedLoopIterationStep(
    const CountedLoopControl &control,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(control.counterLocal));
  if (control.countKind == LocalInfo::ValueKind::Int32) {
    emitInstruction(IrOpcode::PushI32, 1);
    emitInstruction(IrOpcode::SubI32, 0);
  } else {
    emitInstruction(IrOpcode::PushI64, 1);
    emitInstruction(IrOpcode::SubI64, 0);
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(control.counterLocal));
  emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(control.checkIndex));
}

void patchCountedLoopEnd(
    const CountedLoopControl &control,
    const std::function<size_t()> &instructionCount,
    const std::function<void(size_t, int32_t)> &patchInstructionImm) {
  const size_t endIndex = instructionCount();
  patchInstructionImm(control.jumpEndIndex, static_cast<int32_t>(endIndex));
}

bool emitBodyStatements(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement) {
  LocalMap bodyLocals = localsIn;
  for (const auto &bodyStmt : bodyStatements) {
    if (!emitStatement(bodyStmt, bodyLocals)) {
      return false;
    }
  }
  return true;
}

bool emitBodyStatementsWithFileScope(
    const std::vector<Expr> &bodyStatements,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, LocalMap &)> &emitStatement,
    const std::function<bool()> &emitAfterBody,
    const std::function<void()> &pushFileScope,
    const std::function<void()> &emitCurrentFileScopeCleanup,
    const std::function<void()> &popFileScope) {
  pushFileScope();
  if (!emitBodyStatements(bodyStatements, localsIn, emitStatement)) {
    return false;
  }
  if (emitAfterBody && !emitAfterBody()) {
    return false;
  }
  emitCurrentFileScopeCleanup();
  popFileScope();
  return true;
}

bool declareForConditionBinding(
    const Expr &binding,
    LocalMap &locals,
    int32_t &nextLocal,
    const std::function<bool(const Expr &)> &isBindingMutable,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const Expr &)> &hasExplicitBindingTypeTransform,
    const std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)> &bindingValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructArrayInfo,
    const std::function<void(const Expr &, LocalInfo &)> &applyStructValueInfo,
    std::string &error) {
  if (binding.args.size() != 1) {
    error = "binding requires exactly one argument";
    return false;
  }
  if (locals.count(binding.name) > 0) {
    error = "binding redefines existing name: " + binding.name;
    return false;
  }

  LocalInfo info;
  info.index = nextLocal++;
  info.isMutable = isBindingMutable(binding);
  info.kind = bindingKind(binding);
  info.valueKind = LocalInfo::ValueKind::Unknown;
  info.mapKeyKind = LocalInfo::ValueKind::Unknown;
  info.mapValueKind = LocalInfo::ValueKind::Unknown;
  if (hasExplicitBindingTypeTransform(binding)) {
    info.valueKind = bindingValueKind(binding, info.kind);
  } else if (info.kind == LocalInfo::Kind::Value) {
    info.valueKind = inferExprKind(binding.args.front(), locals);
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      std::string inferredStruct = inferStructExprPath(binding.args.front(), locals);
      if (!inferredStruct.empty()) {
        info.structTypeName = inferredStruct;
      } else {
        info.valueKind = LocalInfo::ValueKind::Int32;
      }
    }
  }
  applyStructArrayInfo(binding, info);
  applyStructValueInfo(binding, info);
  locals.emplace(binding.name, info);
  return true;
}

bool emitForConditionBindingInit(
    const Expr &binding,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  if (binding.args.size() != 1) {
    error = "binding requires exactly one argument";
    return false;
  }
  auto it = localsIn.find(binding.name);
  if (it == localsIn.end()) {
    error = "binding missing local: " + binding.name;
    return false;
  }
  if (!emitExpr(binding.args.front(), localsIn)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index));
  return true;
}

VectorStatementHelperEmitResult tryEmitVectorStatementHelper(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    const std::function<void()> &emitVectorCapacityExceeded,
    const std::function<void()> &emitVectorPopOnEmpty,
    const std::function<void()> &emitVectorIndexOutOfBounds,
    const std::function<void()> &emitVectorReserveNegative,
    const std::function<void()> &emitVectorReserveExceeded,
    std::string &error) {
  std::string vectorHelper;
  if (isSimpleCallName(stmt, "push")) {
    vectorHelper = "push";
  } else if (isSimpleCallName(stmt, "pop")) {
    vectorHelper = "pop";
  } else if (isSimpleCallName(stmt, "reserve")) {
    vectorHelper = "reserve";
  } else if (isSimpleCallName(stmt, "clear")) {
    vectorHelper = "clear";
  } else if (isSimpleCallName(stmt, "remove_at")) {
    vectorHelper = "remove_at";
  } else if (isSimpleCallName(stmt, "remove_swap")) {
    vectorHelper = "remove_swap";
  }

  if (vectorHelper.empty()) {
    return VectorStatementHelperEmitResult::NotMatched;
  }
  if (isUserDefinedVectorHelperCall(stmt)) {
    return VectorStatementHelperEmitResult::NotMatched;
  }
  if (!stmt.templateArgs.empty()) {
    error = vectorHelper + " does not accept template arguments";
    return VectorStatementHelperEmitResult::Error;
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    error = vectorHelper + " does not accept block arguments";
    return VectorStatementHelperEmitResult::Error;
  }

  const size_t expectedArgs = (vectorHelper == "pop" || vectorHelper == "clear") ? 1 : 2;
  if (stmt.args.size() != expectedArgs) {
    if (expectedArgs == 1) {
      error = vectorHelper + " requires exactly one argument";
    } else {
      error = vectorHelper + " requires exactly two arguments";
    }
    return VectorStatementHelperEmitResult::Error;
  }

  const Expr &target = stmt.args.front();
  if (target.kind != Expr::Kind::Name) {
    error = vectorHelper + " requires mutable vector binding";
    return VectorStatementHelperEmitResult::Error;
  }
  auto it = localsIn.find(target.name);
  if (it == localsIn.end() || it->second.kind != LocalInfo::Kind::Vector || !it->second.isMutable) {
    error = vectorHelper + " requires mutable vector binding";
    return VectorStatementHelperEmitResult::Error;
  }

  auto pushIndexConst = [&](LocalInfo::ValueKind kind, int32_t value) {
    if (kind == LocalInfo::ValueKind::Int32) {
      instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(value)});
    } else {
      instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
    }
  };

  const int32_t ptrLocal = allocTempLocal();
  const int32_t elementOffset = 2;
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

  if (vectorHelper == "clear") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  const int32_t countLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadIndirect, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

  int32_t capacityLocal = -1;
  if (vectorHelper == "push" || vectorHelper == "reserve") {
    capacityLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});
  }

  if (vectorHelper == "reserve") {
    LocalInfo::ValueKind capacityKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
    if (!isSupportedIndexKind(capacityKind)) {
      error = "reserve requires integer capacity";
      return VectorStatementHelperEmitResult::Error;
    }

    const int32_t desiredLocal = allocTempLocal();
    if (!emitExpr(stmt.args[1], localsIn)) {
      return VectorStatementHelperEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(desiredLocal)});

    if (capacityKind != LocalInfo::ValueKind::UInt64) {
      instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
      instructions.push_back({pushZeroForIndex(capacityKind), 0});
      instructions.push_back({cmpLtForIndex(capacityKind), 0});
      size_t jumpNonNegative = instructions.size();
      instructions.push_back({IrOpcode::JumpIfZero, 0});
      emitVectorReserveNegative();
      size_t nonNegativeIndex = instructions.size();
      instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
    }

    const IrOpcode cmpLeOp = (capacityKind == LocalInfo::ValueKind::Int32)
                                 ? IrOpcode::CmpLeI32
                                 : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLeI64 : IrOpcode::CmpLeU64);
    const IrOpcode cmpGtOp = (capacityKind == LocalInfo::ValueKind::Int32)
                                 ? IrOpcode::CmpGtI32
                                 : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpGtI64 : IrOpcode::CmpGtU64);
    const IrOpcode pushLimitOp =
        (capacityKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({cmpLeOp, 0});
    const size_t jumpNeedsGrowth = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    const size_t jumpReserveEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t growthPathIndex = instructions.size();
    instructions[jumpNeedsGrowth].imm = static_cast<int32_t>(growthPathIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({pushLimitOp, static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)});
    instructions.push_back({cmpGtOp, 0});
    const size_t jumpWithinLimit = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorReserveExceeded();
    const size_t withinLimitIndex = instructions.size();
    instructions[jumpWithinLimit].imm = static_cast<int32_t>(withinLimitIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});

    const size_t reserveEndIndex = instructions.size();
    instructions[jumpReserveEnd].imm = static_cast<int32_t>(reserveEndIndex);
    return VectorStatementHelperEmitResult::Emitted;
  }

  if (vectorHelper == "push") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpNeedsGrowth = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    const size_t pushBodyIndex = instructions.size();
    const int32_t valueLocal = allocTempLocal();
    if (!emitExpr(stmt.args[1], localsIn)) {
      return VectorStatementHelperEmitResult::Error;
    }
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});

    const int32_t destPtrLocal = allocTempLocal();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(elementOffset)});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::PushI32, IrSlotBytesI32});
    instructions.push_back({IrOpcode::MulI32, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    const size_t jumpEnd = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});

    const size_t growthPathIndex = instructions.size();
    instructions[jumpNeedsGrowth].imm = static_cast<int32_t>(growthPathIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(kVectorLocalDynamicCapacityLimit)});
    instructions.push_back({IrOpcode::CmpLtI32, 0});
    const size_t jumpOom = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::AddI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});

    instructions.push_back({IrOpcode::Jump, static_cast<int32_t>(pushBodyIndex)});

    const size_t errorIndex = instructions.size();
    instructions[jumpOom].imm = static_cast<int32_t>(errorIndex);
    emitVectorCapacityExceeded();
    const size_t endIndex = instructions.size();
    instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
    return VectorStatementHelperEmitResult::Emitted;
  }

  if (vectorHelper == "pop") {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 0});
    instructions.push_back({IrOpcode::CmpEqI32, 0});
    size_t jumpNonEmpty = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorPopOnEmpty();
    size_t nonEmptyIndex = instructions.size();
    instructions[jumpNonEmpty].imm = static_cast<int32_t>(nonEmptyIndex);

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
    error = vectorHelper + " requires integer index";
    return VectorStatementHelperEmitResult::Error;
  }

  IrOpcode cmpLtOp =
      (indexKind == LocalInfo::ValueKind::Int32)
          ? IrOpcode::CmpLtI32
          : (indexKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtU64);
  IrOpcode addOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::AddI32 : IrOpcode::AddI64;
  IrOpcode subOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::SubI32 : IrOpcode::SubI64;
  IrOpcode mulOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::MulI32 : IrOpcode::MulI64;

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return VectorStatementHelperEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

  if (indexKind != LocalInfo::ValueKind::UInt64) {
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    instructions.push_back({pushZeroForIndex(indexKind), 0});
    instructions.push_back({cmpLtForIndex(indexKind), 0});
    size_t jumpNonNegative = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitVectorIndexOutOfBounds();
    size_t nonNegativeIndex = instructions.size();
    instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({cmpGeForIndex(indexKind), 0});
  size_t jumpInRange = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});
  emitVectorIndexOutOfBounds();
  size_t inRangeIndex = instructions.size();
  instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);

  const int32_t lastIndexLocal = allocTempLocal();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({subOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(lastIndexLocal)});

  if (vectorHelper == "remove_swap") {
    const int32_t destPtrLocal = allocTempLocal();
    const int32_t srcPtrLocal = allocTempLocal();
    const int32_t tempValueLocal = allocTempLocal();

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
    pushIndexConst(indexKind, elementOffset);
    instructions.push_back({addOp, 0});
    pushIndexConst(indexKind, IrSlotBytesI32);
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
    pushIndexConst(indexKind, elementOffset);
    instructions.push_back({addOp, 0});
    pushIndexConst(indexKind, IrSlotBytesI32);
    instructions.push_back({mulOp, 0});
    instructions.push_back({IrOpcode::AddI64, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValueLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValueLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::PushI32, 1});
    instructions.push_back({IrOpcode::SubI32, 0});
    instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
    instructions.push_back({IrOpcode::StoreIndirect, 0});
    instructions.push_back({IrOpcode::Pop, 0});
    return VectorStatementHelperEmitResult::Emitted;
  }

  const int32_t destPtrLocal = allocTempLocal();
  const int32_t srcPtrLocal = allocTempLocal();
  const int32_t tempValueLocal = allocTempLocal();

  const size_t loopStart = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
  instructions.push_back({cmpLtOp, 0});
  size_t jumpLoopEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, elementOffset);
  instructions.push_back({addOp, 0});
  pushIndexConst(indexKind, IrSlotBytesI32);
  instructions.push_back({mulOp, 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, elementOffset + 1);
  instructions.push_back({addOp, 0});
  pushIndexConst(indexKind, IrSlotBytesI32);
  instructions.push_back({mulOp, 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
  instructions.push_back({IrOpcode::LoadIndirect, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValueLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  pushIndexConst(indexKind, 1);
  instructions.push_back({addOp, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

  size_t loopEndIndex = instructions.size();
  instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEndIndex);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::SubI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return VectorStatementHelperEmitResult::Emitted;
}

bool resolveBufferInitInfo(const Expr &expr,
                           const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
                           BufferInitInfo &out,
                           std::string &error) {
  if (expr.templateArgs.size() != 1) {
    error = "buffer requires exactly one template argument";
    return false;
  }
  if (expr.args.size() != 1) {
    error = "buffer requires exactly one argument";
    return false;
  }
  if (expr.args.front().kind != Expr::Kind::Literal || expr.args.front().isUnsigned || expr.args.front().intWidth == 64) {
    error = "buffer requires constant i32 size";
    return false;
  }
  const int64_t count64 = static_cast<int64_t>(expr.args.front().literalValue);
  if (count64 < 0 || count64 > std::numeric_limits<int32_t>::max()) {
    error = "buffer size out of range";
    return false;
  }
  const LocalInfo::ValueKind elemKind = resolveValueKind(expr.templateArgs.front());
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer requires numeric/bool element type";
    return false;
  }

  out.count = static_cast<int32_t>(count64);
  out.elemKind = elemKind;
  out.zeroOpcode = IrOpcode::PushI32;
  if (elemKind == LocalInfo::ValueKind::Int64 || elemKind == LocalInfo::ValueKind::UInt64) {
    out.zeroOpcode = IrOpcode::PushI64;
  } else if (elemKind == LocalInfo::ValueKind::Float64) {
    out.zeroOpcode = IrOpcode::PushF64;
  } else if (elemKind == LocalInfo::ValueKind::Float32) {
    out.zeroOpcode = IrOpcode::PushF32;
  }
  return true;
}

bool resolveBufferLoadInfo(
    const Expr &expr,
    const std::function<std::optional<LocalInfo::ValueKind>(const std::string &)> &resolveNamedBufferElemKind,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &)> &inferExprKind,
    BufferLoadInfo &out,
    std::string &error) {
  if (expr.args.size() != 2) {
    error = "buffer_load requires buffer and index";
    return false;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (expr.args[0].kind == Expr::Kind::Name) {
    const std::optional<LocalInfo::ValueKind> localKind = resolveNamedBufferElemKind(expr.args[0].name);
    if (localKind.has_value()) {
      elemKind = *localKind;
    }
  } else if (expr.args[0].kind == Expr::Kind::Call) {
    if (isSimpleCallName(expr.args[0], "buffer") && expr.args[0].templateArgs.size() == 1) {
      elemKind = resolveValueKind(expr.args[0].templateArgs.front());
    }
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_load requires numeric/bool buffer";
    return false;
  }

  const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1]));
  if (!isSupportedIndexKind(indexKind)) {
    error = "buffer_load requires integer index";
    return false;
  }

  out.elemKind = elemKind;
  out.indexKind = indexKind;
  return true;
}

bool emitBufferLoadCall(const Expr &expr,
                        LocalInfo::ValueKind indexKind,
                        const std::function<bool(const Expr &)> &emitExpr,
                        const std::function<int32_t()> &allocTempLocal,
                        const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(expr.args[0])) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(expr.args[1])) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(pushOneForIndex(indexKind), 1);
  emitInstruction(addForIndex(indexKind), 0);
  emitInstruction(pushOneForIndex(indexKind), IrSlotBytesI32);
  emitInstruction(mulForIndex(indexKind), 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
  return true;
}

BufferBuiltinCallEmitResult tryEmitBufferBuiltinCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  if (isSimpleCallName(expr, "buffer")) {
    BufferInitInfo initInfo;
    if (!resolveBufferInitInfo(expr, resolveValueKind, initInfo, error)) {
      return BufferBuiltinCallEmitResult::Error;
    }

    const int32_t headerSlots = 1;
    const int32_t baseLocal = allocLocalRange(headerSlots + initInfo.count);
    emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(initInfo.count));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal));
    for (int32_t i = 0; i < initInfo.count; ++i) {
      emitInstruction(initInfo.zeroOpcode, 0);
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + headerSlots + i));
    }
    emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal));
    return BufferBuiltinCallEmitResult::Emitted;
  }

  if (isSimpleCallName(expr, "buffer_load")) {
    BufferLoadInfo loadInfo;
    if (!resolveBufferLoadInfo(
            expr,
            [&](const std::string &name) -> std::optional<LocalInfo::ValueKind> {
              auto it = localsIn.find(name);
              if (it == localsIn.end() || it->second.kind != LocalInfo::Kind::Buffer) {
                return std::nullopt;
              }
              return it->second.valueKind;
            },
            resolveValueKind,
            [&](const Expr &indexExpr) { return inferExprKind(indexExpr, localsIn); },
            loadInfo,
            error)) {
      return BufferBuiltinCallEmitResult::Error;
    }

    if (!emitBufferLoadCall(
            expr,
            loadInfo.indexKind,
            [&](const Expr &argExpr) { return emitExpr(argExpr, localsIn); },
            allocTempLocal,
            emitInstruction)) {
      return BufferBuiltinCallEmitResult::Error;
    }
    return BufferBuiltinCallEmitResult::Emitted;
  }

  return BufferBuiltinCallEmitResult::NotMatched;
}

} // namespace primec::ir_lowerer
