#include "IrLowererIndexKindHelpers.h"

namespace primec::ir_lowerer {

LocalInfo::ValueKind normalizeIndexKind(LocalInfo::ValueKind kind) {
  return (kind == LocalInfo::ValueKind::Bool) ? LocalInfo::ValueKind::Int32 : kind;
}

bool isSupportedIndexKind(LocalInfo::ValueKind kind) {
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
         kind == LocalInfo::ValueKind::UInt64;
}

IrOpcode pushZeroForIndex(LocalInfo::ValueKind kind) {
  return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
}

IrOpcode cmpLtForIndex(LocalInfo::ValueKind kind) {
  return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::CmpLtI32 : IrOpcode::CmpLtI64;
}

IrOpcode cmpGeForIndex(LocalInfo::ValueKind kind) {
  if (kind == LocalInfo::ValueKind::Int32) {
    return IrOpcode::CmpGeI32;
  }
  if (kind == LocalInfo::ValueKind::Int64) {
    return IrOpcode::CmpGeI64;
  }
  return IrOpcode::CmpGeU64;
}

IrOpcode pushOneForIndex(LocalInfo::ValueKind kind) {
  return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
}

IrOpcode addForIndex(LocalInfo::ValueKind kind) {
  return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::AddI32 : IrOpcode::AddI64;
}

IrOpcode mulForIndex(LocalInfo::ValueKind kind) {
  return (kind == LocalInfo::ValueKind::Int32) ? IrOpcode::MulI32 : IrOpcode::MulI64;
}

} // namespace primec::ir_lowerer
