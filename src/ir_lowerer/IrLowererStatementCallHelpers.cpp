#include "IrLowererStatementCallHelpers.h"

#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "buffer_store")) {
    return BufferStoreStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() != 3) {
    error = "buffer_store requires buffer, index, and value";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (stmt.args[0].kind == Expr::Kind::Name) {
    auto it = localsIn.find(stmt.args[0].name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
      elemKind = it->second.valueKind;
    }
  } else if (stmt.args[0].kind == Expr::Kind::Call && isSimpleCallName(stmt.args[0], "buffer") &&
             stmt.args[0].templateArgs.size() == 1) {
    elemKind = valueKindFromTypeName(stmt.args[0].templateArgs.front());
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_store requires numeric/bool buffer";
    return BufferStoreStatementEmitResult::Error;
  }

  const LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
  if (!isSupportedIndexKind(indexKind)) {
    error = "buffer_store requires integer index";
    return BufferStoreStatementEmitResult::Error;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(stmt.args[0], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  const int32_t valueLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({pushOneForIndex(indexKind), 1});
  instructions.push_back({addForIndex(indexKind), 0});
  instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
  instructions.push_back({mulForIndex(indexKind), 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return BufferStoreStatementEmitResult::Emitted;
}

} // namespace primec::ir_lowerer
