#include "IrLowererFileWriteHelpers.h"

namespace primec::ir_lowerer {

bool resolveFileWriteValueOpcode(LocalInfo::ValueKind kind, IrOpcode &opcodeOut) {
  if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
    opcodeOut = IrOpcode::FileWriteI32;
    return true;
  }
  if (kind == LocalInfo::ValueKind::Int64) {
    opcodeOut = IrOpcode::FileWriteI64;
    return true;
  }
  if (kind == LocalInfo::ValueKind::UInt64) {
    opcodeOut = IrOpcode::FileWriteU64;
    return true;
  }
  return false;
}

bool emitFileWriteStep(const Expr &arg,
                       int32_t handleIndex,
                       int32_t errorLocal,
                       const ResolveStringTableTargetForWriteFn &resolveStringTableTarget,
                       const InferExprKindForWriteFn &inferExprKind,
                       const EmitExprForWriteFn &emitExpr,
                       const EmitInstructionForWriteFn &emitInstruction,
                       std::string &error) {
  int32_t stringIndex = -1;
  size_t length = 0;
  if (resolveStringTableTarget(arg, stringIndex, length)) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
    emitInstruction(IrOpcode::FileWriteString, static_cast<uint64_t>(stringIndex));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
    return true;
  }

  const LocalInfo::ValueKind kind = inferExprKind(arg);
  IrOpcode writeOp = IrOpcode::FileWriteI32;
  if (!resolveFileWriteValueOpcode(kind, writeOp)) {
    error = "file write requires integer/bool or string arguments";
    return false;
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
  if (!emitExpr(arg)) {
    return false;
  }
  emitInstruction(writeOp, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
  return true;
}

} // namespace primec::ir_lowerer
