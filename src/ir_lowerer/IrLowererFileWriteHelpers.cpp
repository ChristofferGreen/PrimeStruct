#include "IrLowererFileWriteHelpers.h"
#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveScopedExprPath(const Expr &expr) {
  if (expr.name.empty()) {
    return {};
  }
  if (!expr.namespacePrefix.empty()) {
    return expr.namespacePrefix + "/" + expr.name;
  }
  return expr.name;
}

} // namespace

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

bool resolveDynamicFileOpenModeOpcode(const std::string &mode, IrOpcode &opcodeOut) {
  if (mode == "Read") {
    opcodeOut = IrOpcode::FileOpenReadDynamic;
    return true;
  }
  if (mode == "Write") {
    opcodeOut = IrOpcode::FileOpenWriteDynamic;
    return true;
  }
  if (mode == "Append") {
    opcodeOut = IrOpcode::FileOpenAppendDynamic;
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
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const IsEntryArgsNameWithLocalsForWriteFn &isEntryArgsName,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error) {
  const std::string directPath = resolveScopedExprPath(expr);
  if (expr.isMethodCall || !(directPath == "File" || directPath == "/std/file/File")) {
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
  if (resolveStringTableTarget(expr.args.front(), localsIn, stringIndex, length)) {
    if (!emitFileOpenCall(expr.templateArgs.front(), stringIndex, emitInstruction, error)) {
      return FileConstructorCallEmitResult::Error;
    }
    return FileConstructorCallEmitResult::Emitted;
  }

  if (expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String &&
        it->second.stringSource == LocalInfo::StringSource::RuntimeIndex) {
      IrOpcode dynamicOp = IrOpcode::FileOpenReadDynamic;
      if (!resolveDynamicFileOpenModeOpcode(expr.templateArgs.front(), dynamicOp)) {
        error = "File requires Read, Write, or Append mode";
        return FileConstructorCallEmitResult::Error;
      }
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
      emitInstruction(dynamicOp, 0);
      return FileConstructorCallEmitResult::Emitted;
    }
  }

  if (expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr.args.front(), accessName) && !expr.args.front().args.empty() &&
        isEntryArgsName(expr.args.front().args.front(), localsIn)) {
      error = "native backend only supports File() with string literals or literal-backed bindings";
      return FileConstructorCallEmitResult::Error;
    }
  }

  if (inferExprKind(expr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
    IrOpcode dynamicOp = IrOpcode::FileOpenReadDynamic;
    if (!resolveDynamicFileOpenModeOpcode(expr.templateArgs.front(), dynamicOp)) {
      error = "File requires Read, Write, or Append mode";
      return FileConstructorCallEmitResult::Error;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return FileConstructorCallEmitResult::Error;
    }
    emitInstruction(dynamicOp, 0);
    return FileConstructorCallEmitResult::Emitted;
  }

  error = "native backend only supports File() with string literals or literal-backed bindings";
  return FileConstructorCallEmitResult::Error;
}

FileConstructorCallEmitResult tryEmitFileConstructorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const EmitInstructionForWriteFn &emitInstruction,
    std::string &error) {
  return tryEmitFileConstructorCall(
      expr,
      localsIn,
      resolveStringTableTarget,
      [](const Expr &, const LocalMap &) { return LocalInfo::ValueKind::Unknown; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      emitInstruction,
      error);
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
  if (kind == LocalInfo::ValueKind::String) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
    if (!emitExpr(arg)) {
      return false;
    }
    emitInstruction(IrOpcode::FileWriteStringDynamic, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
    return true;
  }

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

bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const AllocTempLocalForWriteFn &allocTempLocal,
                          const EmitInstructionForWriteFn &emitInstruction,
                          const GetInstructionCountForWriteFn &getInstructionCount,
                          const PatchInstructionImmForWriteFn &patchInstructionImm,
                          std::string &error) {
  if (expr.args.size() != 2) {
    error = "read_byte requires exactly one argument";
    return false;
  }
  if (expr.args[1].kind != Expr::Kind::Name) {
    error = "read_byte requires mutable integer binding";
    return false;
  }
  auto it = localsIn.find(expr.args[1].name);
  if (it == localsIn.end() || !it->second.isMutable) {
    error = "read_byte requires mutable integer binding";
    return false;
  }
  const LocalInfo::ValueKind kind = it->second.valueKind;
  if (kind != LocalInfo::ValueKind::Int32 && kind != LocalInfo::ValueKind::Int64 &&
      kind != LocalInfo::ValueKind::UInt64) {
    error = "read_byte requires mutable integer binding";
    return false;
  }

  if (it->second.kind == LocalInfo::Kind::Reference) {
    const int32_t valueLocal = allocTempLocal();
    const int32_t statusLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
    emitInstruction(IrOpcode::FileReadByte, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(statusLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(statusLocal));
    emitInstruction(IrOpcode::PushI64, 0);
    emitInstruction(IrOpcode::CmpEqI64, 0);
    const size_t jumpSkipStore = getInstructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal));
    emitInstruction(IrOpcode::StoreIndirect, 0);
    emitInstruction(IrOpcode::Pop, 0);
    const size_t storeEnd = getInstructionCount();
    patchInstructionImm(jumpSkipStore, static_cast<int32_t>(storeEnd));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(statusLocal));
    return true;
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex));
  emitInstruction(IrOpcode::FileReadByte, static_cast<uint64_t>(it->second.index));
  return true;
}

bool emitFileReadByteCall(const Expr &expr,
                          const LocalMap &localsIn,
                          int32_t handleIndex,
                          const EmitInstructionForWriteFn &emitInstruction,
                          std::string &error) {
  int32_t nextTempLocal = 0;
  return emitFileReadByteCall(
      expr,
      localsIn,
      handleIndex,
      [&]() { return nextTempLocal++; },
      emitInstruction,
      []() { return size_t{0}; },
      [](size_t, int32_t) {},
      error);
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
    const ShouldBypassBuiltinFileMethodWithLocalsFn &shouldBypassBuiltin,
    const ResolveStringTableTargetWithLocalsForWriteFn &resolveStringTableTarget,
    const InferExprKindWithLocalsForWriteFn &inferExprKind,
    const EmitExprWithLocalsForWriteFn &emitExpr,
    const AllocTempLocalForWriteFn &allocTempLocal,
    const EmitInstructionForWriteFn &emitInstruction,
    const GetInstructionCountForWriteFn &getInstructionCount,
    const PatchInstructionImmForWriteFn &patchInstructionImm,
    std::string &error) {
  if (!expr.isMethodCall || expr.args.empty()) {
    return FileHandleMethodCallEmitResult::NotMatched;
  }
  if ((expr.name == "write" || expr.name == "write_line" || expr.name == "close") && shouldBypassBuiltin &&
      shouldBypassBuiltin(expr, localsIn)) {
    return FileHandleMethodCallEmitResult::NotMatched;
  }
  int32_t handleIndex = -1;
  const Expr &receiverExpr = expr.args.front();
  if (receiverExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(receiverExpr.name);
    if (it == localsIn.end() || !it->second.isFileHandle) {
      return FileHandleMethodCallEmitResult::NotMatched;
    }
    handleIndex = it->second.index;
  } else if (receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
             receiverExpr.args.size() == 1) {
    const Expr &targetExpr = receiverExpr.args.front();
    bool isIndirectFileHandle = false;
    if (targetExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.name);
      isIndirectFileHandle =
          it != localsIn.end() &&
          (it->second.kind == LocalInfo::Kind::Reference || it->second.kind == LocalInfo::Kind::Pointer) &&
          it->second.isFileHandle;
    } else if (targetExpr.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(targetExpr, accessName) && targetExpr.args.size() == 2 &&
          targetExpr.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(targetExpr.args.front().name);
        isIndirectFileHandle =
            it != localsIn.end() && it->second.isArgsPack && it->second.isFileHandle &&
            (it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
             it->second.argsPackElementKind == LocalInfo::Kind::Pointer);
      }
    }
    if (!isIndirectFileHandle) {
      return FileHandleMethodCallEmitResult::NotMatched;
    }
    if (!emitExpr(receiverExpr, localsIn)) {
      return FileHandleMethodCallEmitResult::Error;
    }
    handleIndex = allocTempLocal();
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(handleIndex));
  } else if (receiverExpr.kind == Expr::Kind::Call) {
    std::string accessName;
    if (!getBuiltinArrayAccessName(receiverExpr, accessName) || receiverExpr.args.size() != 2 ||
        receiverExpr.args.front().kind != Expr::Kind::Name) {
      return FileHandleMethodCallEmitResult::NotMatched;
    }
    auto it = localsIn.find(receiverExpr.args.front().name);
    if (it == localsIn.end() || !it->second.isArgsPack || !it->second.isFileHandle) {
      return FileHandleMethodCallEmitResult::NotMatched;
    }
    const Expr *materializedReceiverExpr = &receiverExpr;
    Expr derefExpr;
    if (it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
        it->second.argsPackElementKind == LocalInfo::Kind::Pointer) {
      derefExpr.kind = Expr::Kind::Call;
      derefExpr.name = "dereference";
      derefExpr.args.push_back(receiverExpr);
      materializedReceiverExpr = &derefExpr;
    } else if (it->second.argsPackElementKind != LocalInfo::Kind::Value) {
      return FileHandleMethodCallEmitResult::NotMatched;
    }
    if (!emitExpr(*materializedReceiverExpr, localsIn)) {
      return FileHandleMethodCallEmitResult::Error;
    }
    handleIndex = allocTempLocal();
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(handleIndex));
  } else {
    return FileHandleMethodCallEmitResult::NotMatched;
  }

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
  if (expr.name == "read_byte") {
    if (!emitFileReadByteCall(expr,
                              localsIn,
                              handleIndex,
                              allocTempLocal,
                              emitInstruction,
                              getInstructionCount,
                              patchInstructionImm,
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
