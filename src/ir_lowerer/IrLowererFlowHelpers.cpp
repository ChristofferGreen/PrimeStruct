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

} // namespace primec::ir_lowerer
