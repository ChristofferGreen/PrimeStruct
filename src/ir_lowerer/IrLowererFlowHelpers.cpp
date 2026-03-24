#include "IrLowererFlowHelpers.h"

#include <cstdlib>
#include <cstring>
#include <utility>

namespace primec::ir_lowerer {

OnErrorScope::OnErrorScope(std::optional<OnErrorHandler> &targetIn,
                           std::optional<OnErrorHandler> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

OnErrorScope::~OnErrorScope() {
  target = std::move(previous);
}

ResultReturnScope::ResultReturnScope(std::optional<ResultReturnInfo> &targetIn,
                                     std::optional<ResultReturnInfo> next)
    : target(targetIn), previous(targetIn) {
  target = std::move(next);
}

ResultReturnScope::~ResultReturnScope() {
  target = std::move(previous);
}

bool emitCompareToZero(std::vector<IrInstruction> &instructions,
                       LocalInfo::ValueKind kind,
                       bool equals,
                       std::string &error) {
  if (kind == LocalInfo::ValueKind::Int64 ||
      kind == LocalInfo::ValueKind::UInt64) {
    instructions.push_back({IrOpcode::PushI64, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Float64) {
    instructions.push_back({IrOpcode::PushF64, 0});
    instructions.push_back({equals ? IrOpcode::CmpEqF64 : IrOpcode::CmpNeF64, 0});
    return true;
  }
  if (kind == LocalInfo::ValueKind::Int32 ||
      kind == LocalInfo::ValueKind::Bool) {
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

bool emitFloatLiteral(std::vector<IrInstruction> &instructions,
                      const Expr &expr,
                      std::string &error) {
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
  if (returnInfo.returnsArray ||
      returnInfo.kind == LocalInfo::ValueKind::Int64 ||
      returnInfo.kind == LocalInfo::ValueKind::UInt64 ||
      returnInfo.kind == LocalInfo::ValueKind::String) {
    instructions.push_back({IrOpcode::ReturnI64, 0});
    return true;
  }
  if (returnInfo.kind == LocalInfo::ValueKind::Int32 ||
      returnInfo.kind == LocalInfo::ValueKind::Bool) {
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

UnaryPassthroughCallResult tryEmitUnaryPassthroughCall(
    const Expr &expr,
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

} // namespace primec::ir_lowerer
