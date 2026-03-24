#include "IrLowererFlowHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"

#include <limits>

namespace primec::ir_lowerer {

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
  if (expr.args.front().kind != Expr::Kind::Literal || expr.args.front().isUnsigned ||
      expr.args.front().intWidth == 64) {
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
    const std::function<std::optional<LocalInfo::ValueKind>(const Expr &)> &resolveBufferElemKind,
    const std::function<LocalInfo::ValueKind(const std::string &)> &resolveValueKind,
    const std::function<LocalInfo::ValueKind(const Expr &)> &inferExprKind,
    BufferLoadInfo &out,
    std::string &error) {
  if (expr.args.size() != 2) {
    error = "buffer_load requires buffer and index";
    return false;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  const std::optional<LocalInfo::ValueKind> localKind = resolveBufferElemKind(expr.args[0]);
  if (localKind.has_value()) {
    elemKind = *localKind;
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
            [&](const Expr &bufferExpr) -> std::optional<LocalInfo::ValueKind> {
              if (bufferExpr.kind == Expr::Kind::Name) {
                auto it = localsIn.find(bufferExpr.name);
                if (it == localsIn.end() || it->second.kind != LocalInfo::Kind::Buffer) {
                  return std::nullopt;
                }
                return it->second.valueKind;
              }
              if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "dereference") &&
                  bufferExpr.args.size() == 1) {
                const Expr &targetExpr = bufferExpr.args.front();
                if (targetExpr.kind == Expr::Kind::Name) {
                  auto it = localsIn.find(targetExpr.name);
                  if (it != localsIn.end() &&
                      ((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
                       (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
                    return it->second.valueKind;
                  }
                }
                std::string derefAccessName;
                if (targetExpr.kind == Expr::Kind::Call &&
                    getBuiltinArrayAccessName(targetExpr, derefAccessName) &&
                    targetExpr.args.size() == 2 &&
                    targetExpr.args.front().kind == Expr::Kind::Name) {
                  auto it = localsIn.find(targetExpr.args.front().name);
                  if (it != localsIn.end() && it->second.isArgsPack &&
                      ((it->second.argsPackElementKind == LocalInfo::Kind::Reference &&
                        it->second.referenceToBuffer) ||
                       (it->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
                        it->second.pointerToBuffer))) {
                    return it->second.valueKind;
                  }
                }
              }
              std::string accessName;
              if (bufferExpr.kind == Expr::Kind::Call &&
                  getBuiltinArrayAccessName(bufferExpr, accessName) &&
                  bufferExpr.args.size() == 2 &&
                  bufferExpr.args.front().kind == Expr::Kind::Name) {
                auto it = localsIn.find(bufferExpr.args.front().name);
                if (it != localsIn.end() && it->second.isArgsPack &&
                    it->second.argsPackElementKind == LocalInfo::Kind::Buffer) {
                  return it->second.valueKind;
                }
              }
              return std::nullopt;
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
