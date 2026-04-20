#include "IrLowererRuntimeErrorHelpers.h"

#include "IrLowererHelpers.h"

#include <cerrno>
#include <unordered_set>
#include <vector>

namespace primec::ir_lowerer {

namespace {
void emitRuntimeError(IrFunction &function, const std::string &message, const InternRuntimeErrorStringFn &internString) {
  uint64_t flags = encodePrintFlags(true, true);
  int32_t msgIndex = internString(message);
  function.instructions.push_back({IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(msgIndex), flags)});
  function.instructions.push_back({IrOpcode::PushI32, 3});
  function.instructions.push_back({IrOpcode::ReturnI32, 0});
}
} // namespace

RuntimeErrorAndStringLiteralSetup makeRuntimeErrorAndStringLiteralSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    std::string &error) {
  RuntimeErrorAndStringLiteralSetup setup{};
  setup.stringLiteralHelpers = makeStringLiteralHelperContext(stringTable, error);
  setup.runtimeErrorEmitters = makeRuntimeErrorEmitters(function, setup.stringLiteralHelpers.internString);
  return setup;
}

RuntimeErrorEmitters makeRuntimeErrorEmitters(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  RuntimeErrorEmitters emitters;
  emitters.emitArrayIndexOutOfBounds = makeEmitArrayIndexOutOfBounds(function, internString);
  emitters.emitPointerIndexOutOfBounds = makeEmitPointerIndexOutOfBounds(function, internString);
  emitters.emitStringIndexOutOfBounds = makeEmitStringIndexOutOfBounds(function, internString);
  emitters.emitMapKeyNotFound = makeEmitMapKeyNotFound(function, internString);
  emitters.emitVectorIndexOutOfBounds = makeEmitVectorIndexOutOfBounds(function, internString);
  emitters.emitVectorPopOnEmpty = makeEmitVectorPopOnEmpty(function, internString);
  emitters.emitVectorCapacityExceeded = makeEmitVectorCapacityExceeded(function, internString);
  emitters.emitVectorReserveNegative = makeEmitVectorReserveNegative(function, internString);
  emitters.emitVectorReserveExceeded = makeEmitVectorReserveExceeded(function, internString);
  emitters.emitLoopCountNegative = makeEmitLoopCountNegative(function, internString);
  emitters.emitPowNegativeExponent = makeEmitPowNegativeExponent(function, internString);
  emitters.emitFloatToIntNonFinite = makeEmitFloatToIntNonFinite(function, internString);
  return emitters;
}

EmitRuntimeErrorFn makeEmitArrayIndexOutOfBounds(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitArrayIndexOutOfBounds(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitPointerIndexOutOfBounds(IrFunction &function,
                                                   const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitPointerIndexOutOfBounds(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitStringIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitStringIndexOutOfBounds(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitMapKeyNotFound(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitVectorIndexOutOfBounds(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitVectorIndexOutOfBounds(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitVectorPopOnEmpty(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitVectorCapacityExceeded(IrFunction &function,
                                                  const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitVectorCapacityExceeded(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitVectorReserveNegative(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitVectorReserveNegative(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitVectorReserveExceeded(IrFunction &function,
                                                 const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitVectorReserveExceeded(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitLoopCountNegative(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitPowNegativeExponent(IrFunction &function,
                                               const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitPowNegativeExponent(*functionPtr, internStringFn);
  };
}

EmitRuntimeErrorFn makeEmitFloatToIntNonFinite(IrFunction &function,
                                                const InternRuntimeErrorStringFn &internString) {
  auto *functionPtr = &function;
  auto internStringFn = internString;
  return [functionPtr, internStringFn]() {
    emitFloatToIntNonFinite(*functionPtr, internStringFn);
  };
}

void emitArrayIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "array index out of bounds", internString);
}

void emitPointerIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "pointer index out of bounds", internString);
}

void emitStringIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "string index out of bounds", internString);
}

void emitMapKeyNotFound(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "map key not found", internString);
}

void emitVectorIndexOutOfBounds(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "container index out of bounds", internString);
}

void emitVectorPopOnEmpty(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "container empty", internString);
}

void emitVectorCapacityExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, vectorPushAllocationFailedMessage(), internString);
}

void emitVectorReserveNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "vector reserve expects non-negative capacity", internString);
}

void emitVectorReserveExceeded(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, vectorReserveAllocationFailedMessage(), internString);
}

void emitLoopCountNegative(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "loop count must be non-negative", internString);
}

void emitPowNegativeExponent(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "pow exponent must be non-negative", internString);
}

void emitFloatToIntNonFinite(IrFunction &function, const InternRuntimeErrorStringFn &internString) {
  emitRuntimeError(function, "float to int conversion requires finite value", internString);
}

void emitFileErrorWhy(IrFunction &function, int32_t errorLocal, const InternRuntimeErrorStringFn &internString) {
  struct FileErrorMapping {
    int code;
    const char *name;
  };

  std::vector<FileErrorMapping> mappings;
#ifdef EACCES
  mappings.push_back({EACCES, "EACCES"});
#endif
#ifdef ENOENT
  mappings.push_back({ENOENT, "ENOENT"});
#endif
#ifdef EEXIST
  mappings.push_back({EEXIST, "EEXIST"});
#endif
#ifdef ENOTDIR
  mappings.push_back({ENOTDIR, "ENOTDIR"});
#endif
#ifdef EISDIR
  mappings.push_back({EISDIR, "EISDIR"});
#endif
#ifdef EINVAL
  mappings.push_back({EINVAL, "EINVAL"});
#endif
#ifdef EIO
  mappings.push_back({EIO, "EIO"});
#endif
#ifdef EBADF
  mappings.push_back({EBADF, "EBADF"});
#endif
#ifdef EPERM
  mappings.push_back({EPERM, "EPERM"});
#endif
#ifdef ENOSPC
  mappings.push_back({ENOSPC, "ENOSPC"});
#endif
#ifdef EROFS
  mappings.push_back({EROFS, "EROFS"});
#endif
#ifdef EPIPE
  mappings.push_back({EPIPE, "EPIPE"});
#endif

  auto emitMatch = [&](uint64_t code, int32_t stringIndex, std::vector<size_t> &jumpToEnd) {
    function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
    function.instructions.push_back({IrOpcode::PushI64, code});
    function.instructions.push_back({IrOpcode::CmpEqI64, 0});
    size_t jumpNext = function.instructions.size();
    function.instructions.push_back({IrOpcode::JumpIfZero, 0});
    function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(stringIndex)});
    size_t jumpEnd = function.instructions.size();
    function.instructions.push_back({IrOpcode::Jump, 0});
    size_t nextIndex = function.instructions.size();
    function.instructions[jumpNext].imm = static_cast<int32_t>(nextIndex);
    jumpToEnd.push_back(jumpEnd);
  };

  std::vector<size_t> jumpToEnd;
  const int32_t emptyIndex = internString("");
  emitMatch(0, emptyIndex, jumpToEnd);
  const int32_t eofIndex = internString("EOF");
  emitMatch(FileReadEofCode, eofIndex, jumpToEnd);
  std::unordered_set<int> seen;
  for (const auto &mapping : mappings) {
    if (mapping.code == 0 || !seen.insert(mapping.code).second) {
      continue;
    }
    const int32_t stringIndex = internString(mapping.name);
    emitMatch(static_cast<uint64_t>(mapping.code), stringIndex, jumpToEnd);
  }
  const int32_t unknownIndex = internString("Unknown file error");
  function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(unknownIndex)});
  const size_t endIndex = function.instructions.size();
  for (size_t jumpIndex : jumpToEnd) {
    function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
  }
}

FileErrorWhyCallEmitResult tryEmitFileErrorWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(int32_t)> &emitFileErrorWhyFn,
    std::string &error) {
  auto resolveDirectCallPath = [&]() {
    if (!expr.name.empty() && expr.name.front() == '/') {
      return expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      std::string scoped = expr.namespacePrefix;
      if (!scoped.empty() && scoped.front() != '/') {
        scoped.insert(scoped.begin(), '/');
      }
      return scoped + "/" + expr.name;
    }
    return expr.name;
  };
  auto resolveScopedExprPath = [](const Expr &candidate) {
    if (!candidate.name.empty() && candidate.name.front() == '/') {
      return candidate.name;
    }
    if (!candidate.namespacePrefix.empty()) {
      std::string scoped = candidate.namespacePrefix;
      if (!scoped.empty() && scoped.front() != '/') {
        scoped.insert(scoped.begin(), '/');
      }
      return scoped + "/" + candidate.name;
    }
    return candidate.name;
  };

  if (!expr.isMethodCall && resolveDirectCallPath() == "/file_error/why") {
    if (expr.args.size() != 1) {
      error = "FileError.why requires exactly one argument";
      return FileErrorWhyCallEmitResult::Error;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return FileErrorWhyCallEmitResult::Error;
    }
    const int32_t errorLocal = allocTempLocal();
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
    emitFileErrorWhyFn(errorLocal);
    return FileErrorWhyCallEmitResult::Emitted;
  }

  if (expr.isMethodCall && expr.name == "why" && !expr.args.empty() &&
      expr.args.front().kind == Expr::Kind::Name) {
    const std::string receiverPath = resolveScopedExprPath(expr.args.front());
    if (receiverPath == "FileError" || receiverPath == "/std/file/FileError") {
      if (expr.args.size() != 2) {
        error = "FileError.why requires exactly one argument";
        return FileErrorWhyCallEmitResult::Error;
      }
      if (!emitExpr(expr.args[1], localsIn)) {
        return FileErrorWhyCallEmitResult::Error;
      }
      const int32_t errorLocal = allocTempLocal();
      emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
      emitFileErrorWhyFn(errorLocal);
      return FileErrorWhyCallEmitResult::Emitted;
    }
  }

  if (expr.isMethodCall && expr.name == "why" && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.args.front().name);
    if (it != localsIn.end() && it->second.isFileError) {
      if (it->second.kind == LocalInfo::Kind::Reference || it->second.kind == LocalInfo::Kind::Pointer) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::LoadIndirect, 0);
        const int32_t errorLocal = allocTempLocal();
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
        emitFileErrorWhyFn(errorLocal);
        return FileErrorWhyCallEmitResult::Emitted;
      }
      emitFileErrorWhyFn(it->second.index);
      return FileErrorWhyCallEmitResult::Emitted;
    }
  }

  if (expr.isMethodCall && expr.name == "why" && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call) {
    const Expr &receiver = expr.args.front();
    std::string accessName;
    if (getBuiltinArrayAccessName(receiver, accessName) && receiver.args.size() == 2 &&
        receiver.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(receiver.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack && it->second.isFileError &&
          (it->second.argsPackElementKind == LocalInfo::Kind::Value ||
           it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
           it->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
        Expr valueExpr = receiver;
        if (it->second.argsPackElementKind != LocalInfo::Kind::Value) {
          valueExpr.kind = Expr::Kind::Call;
          valueExpr.name = "dereference";
          valueExpr.args = {receiver};
          valueExpr.argNames = {std::nullopt};
        }
        if (!emitExpr(valueExpr, localsIn)) {
          return FileErrorWhyCallEmitResult::Error;
        }
        const int32_t errorLocal = allocTempLocal();
        emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
        emitFileErrorWhyFn(errorLocal);
        return FileErrorWhyCallEmitResult::Emitted;
      }
    }
    if (isSimpleCallName(receiver, "dereference") && receiver.args.size() == 1) {
      const Expr &target = receiver.args.front();
      if (target.kind == Expr::Kind::Call && getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
          target.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(target.args.front().name);
        if (it != localsIn.end() && it->second.isArgsPack && it->second.isFileError &&
            (it->second.argsPackElementKind == LocalInfo::Kind::Reference ||
             it->second.argsPackElementKind == LocalInfo::Kind::Pointer)) {
          if (!emitExpr(receiver, localsIn)) {
            return FileErrorWhyCallEmitResult::Error;
          }
          const int32_t errorLocal = allocTempLocal();
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal));
          emitFileErrorWhyFn(errorLocal);
          return FileErrorWhyCallEmitResult::Emitted;
        }
      }
    }
  }

  return FileErrorWhyCallEmitResult::NotHandled;
}

} // namespace primec::ir_lowerer
