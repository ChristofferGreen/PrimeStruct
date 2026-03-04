#include "IrLowererFlowHelpers.h"

#include "IrLowererHelpers.h"

#include <cstdlib>
#include <cstring>
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

bool emitUnaryPassthroughCall(const Expr &expr,
                              const std::string &callName,
                              const std::function<bool(const Expr &)> &emitExpr,
                              std::string &error) {
  if (expr.args.size() != 1) {
    error = callName + " requires exactly one argument";
    return false;
  }
  return emitExpr(expr.args.front());
}

} // namespace primec::ir_lowerer
