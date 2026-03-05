#include "IrLowererFileWriteHelpers.h"

namespace primec::ir_lowerer {

bool resolveFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut) {
  if (mode == "Read") {
    opcodeOut = IrOpcode::FileOpenRead;
    return true;
  }
  if (mode == "Write") {
    opcodeOut = IrOpcode::FileOpenWrite;
    return true;
  }
  if (mode == "Append") {
    opcodeOut = IrOpcode::FileOpenAppend;
    return true;
  }
  return false;
}

bool emitFileOpenCall(const std::string &mode,
                      int32_t stringIndex,
                      const EmitInstructionForWriteFn &emitInstruction,
                      std::string &error) {
  IrOpcode opcode = IrOpcode::FileOpenRead;
  if (!resolveFileOpenModeOpcode(mode, opcode)) {
    error = "File requires Read, Write, or Append mode";
    return false;
  }
  emitInstruction(opcode, static_cast<uint64_t>(stringIndex));
  return true;
}

FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error) {
  if (expr.isMethodCall || !isSimpleCallName(expr, "File")) {
    return FileConstructorCallEmitResult::NotMatched;
  }
  if (expr.templateArgs.size() != 1) {
    error = "File requires exactly one template argument";
    return FileConstructorCallEmitResult::Error;
  }
  if (expr.args.size() != 1) {
    error = "File requires exactly one path argument";
    return FileConstructorCallEmitResult::Error;
  }

  int32_t stringIndex = -1;
  size_t length = 0;
  if (!resolveStringTableTarget(expr.args.front(), localsIn, stringIndex, length)) {
    error = "native backend only supports File() with string literals or literal-backed bindings";
    return FileConstructorCallEmitResult::Error;
  }
  if (!emitFileOpenCall(expr.templateArgs.front(), stringIndex, emitInstruction, error)) {
    return FileConstructorCallEmitResult::Error;
  }
  return FileConstructorCallEmitResult::Emitted;
}

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

bool emitFileWriteCall(const Expr &expr,
                       int32_t handleIndex,
                       const EmitFileWriteStepFn &emitWriteStep,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction,
                       const GetInstructionCountForWriteFn &getInstructionCount,
                       const PatchInstructionImmForWriteFn &patchInstructionImm) {
  const int32_t errorLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
  for (size_t i = 1; i < expr.args.size(); ++i) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpEqI64, 0);
    const size_t skipIndex = getInstructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    if (!emitWriteStep(expr.args[i], errorLocal)) {
      return false;
    }
    const size_t afterIndex = getInstructionCount();
    patchInstructionImm(skipIndex, static_cast<int32_t>(afterIndex));
  }
  if (expr.name == "write_line") {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpEqI64, 0);
    const size_t skipIndex = getInstructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
    emitInstruction(IrOpcode::FileWriteNewline, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
    const size_t afterIndex = getInstructionCount();
    patchInstructionImm(skipIndex, static_cast<int32_t>(afterIndex));
  }
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
  return true;
}

bool emitFileWriteByteCall(const Expr &expr,
                           int32_t handleIndex,
                           const EmitExprForWriteFn &emitExpr,
                           const EmitInstructionForWriteFn &emitInstruction,
                           std::string &error) {
  if (expr.args.size() != 2) {
    error = "write_byte requires exactly one argument";
    return false;
  }
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
  if (!emitExpr(expr.args[1])) {
    return false;
  }
  emitInstruction(IrOpcode::FileWriteByte, 0);
  return true;
}

bool emitFileWriteBytesCall(const Expr &expr,
                            int32_t handleIndex,
                            const EmitExprForWriteFn &emitExpr,
                            const AllocTempLocalForWriteFn &allocTempLocal,
                            const EmitInstructionForWriteFn &emitInstruction,
                            const GetInstructionCountForWriteFn &getInstructionCount,
                            const PatchInstructionImmForWriteFn &patchInstructionImm,
                            std::string &error) {
  if (expr.args.size() != 2) {
    error = "write_bytes requires exactly one argument";
    return false;
  }
  return emitFileWriteBytesLoop(expr.args[1],
                                handleIndex,
                                emitExpr,
                                allocTempLocal,
                                emitInstruction,
                                getInstructionCount,
                                patchInstructionImm);
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

FileHandleMethodCallEmitResult tryEmitFileHandleMethodCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const AllocTempLocalForWriteFn &allocTempLocal,
    const EmitInstructionForWriteFn &emitInstruction,
    const GetInstructionCountForWriteFn &getInstructionCount,
    const PatchInstructionImmForWriteFn &patchInstructionImm,
    std::string &error) {
  if (!expr.isMethodCall || expr.args.empty() || expr.args.front().kind != Expr::Kind::Name) {
    return FileHandleMethodCallEmitResult::NotMatched;
  }
  auto it = localsIn.find(expr.args.front().name);
  if (it == localsIn.end() || !it->second.isFileHandle) {
    return FileHandleMethodCallEmitResult::NotMatched;
  }

  const int32_t handleIndex = it->second.index;
  auto emitWriteStep = [&](const Expr &arg, int32_t errorLocal) -> bool {
    return emitFileWriteStep(
        arg,
        handleIndex,
        errorLocal,
        [&](const Expr &valueExpr, int32_t &stringIndex, size_t &length) {
          return resolveStringTableTarget(valueExpr, localsIn, stringIndex, length);
        },
        [&](const Expr &valueExpr) { return inferExprKind(valueExpr, localsIn); },
        [&](const Expr &valueExpr) { return emitExpr(valueExpr, localsIn); },
        emitInstruction,
        error);
  };

  if (expr.name == "write" || expr.name == "write_line") {
    if (!emitFileWriteCall(expr,
                           handleIndex,
                           emitWriteStep,
                           allocTempLocal,
                           emitInstruction,
                           getInstructionCount,
                           patchInstructionImm)) {
      return FileHandleMethodCallEmitResult::Error;
    }
    return FileHandleMethodCallEmitResult::Emitted;
  }
  if (expr.name == "write_byte") {
    if (!emitFileWriteByteCall(expr,
                               handleIndex,
                               [&](const Expr &valueExpr) { return emitExpr(valueExpr, localsIn); },
                               emitInstruction,
                               error)) {
      return FileHandleMethodCallEmitResult::Error;
    }
    return FileHandleMethodCallEmitResult::Emitted;
  }
  if (expr.name == "write_bytes") {
    if (!emitFileWriteBytesCall(expr,
                                handleIndex,
                                [&](const Expr &valueExpr) { return emitExpr(valueExpr, localsIn); },
                                allocTempLocal,
                                emitInstruction,
                                getInstructionCount,
                                patchInstructionImm,
                                error)) {
      return FileHandleMethodCallEmitResult::Error;
    }
    return FileHandleMethodCallEmitResult::Emitted;
  }
  if (expr.name == "flush") {
    emitFileFlushCall(handleIndex, emitInstruction);
    return FileHandleMethodCallEmitResult::Emitted;
  }
  if (expr.name == "close") {
    emitFileCloseCall(handleIndex, allocTempLocal, emitInstruction);
    return FileHandleMethodCallEmitResult::Emitted;
  }
  return FileHandleMethodCallEmitResult::NotMatched;
}

void emitFileFlushCall(int32_t handleIndex, const EmitInstructionForWriteFn &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
  emitInstruction(IrOpcode::FileFlush, 0);
}

void emitFileCloseCall(int32_t handleIndex,
                       const AllocTempLocalForWriteFn &allocTempLocal,
                       const EmitInstructionForWriteFn &emitInstruction) {
  const int32_t errorLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
  emitInstruction(IrOpcode::FileClose, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
  emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(-1)));
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(handleIndex));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal));
}

} // namespace primec::ir_lowerer
