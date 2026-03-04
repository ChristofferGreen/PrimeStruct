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

bool emitFileWriteBytesLoop(const Expr &bytesExpr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm) {
  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(bytesExpr)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));
  const int32_t countLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal));
  const int32_t indexLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
  const int32_t errorLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));

  const size_t loopStart = getInstructionCount();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  emitInstruction(IrOpcode::PushI64, 0);
  emitInstruction(IrOpcode::CmpEqI64, 0);
  const size_t jumpError = getInstructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
  const size_t jumpLoopEnd = getInstructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::FileWriteByte, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(loopStart));

  const size_t loopEnd = getInstructionCount();
  patchInstructionImm(jumpError, static_cast<int32_t>(loopEnd));
  patchInstructionImm(jumpLoopEnd, static_cast<int32_t>(loopEnd));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  return true;
}

} // namespace primec::ir_lowerer
