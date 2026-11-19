#include "IrLowererReturnShapes.h"

#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"

namespace primec::ir_lowerer {

bool isPackedResultReturnValue(
    const Expr &valueExpr,
    const LocalMap &locals,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Definition &)> &returnsStdlibResultSum,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferKind) {
  if (valueExpr.kind == Expr::Kind::Name) {
    const auto localIt = locals.find(valueExpr.name);
    if (localIt == locals.end() || !localIt->second.structTypeName.empty() ||
        localIt->second.kind != LocalInfo::Kind::Value) {
      return false;
    }
    if (localIt->second.isResult) {
      return true;
    }
  } else if (valueExpr.kind == Expr::Kind::Call) {
    if (const Definition *callee = resolveDefinitionCall(valueExpr);
        callee != nullptr && returnsStdlibResultSum(*callee)) {
      return false;
    }
  } else if (valueExpr.kind != Expr::Kind::Literal) {
    return false;
  }
  if (!inferStructPath(valueExpr, locals).empty()) {
    return false;
  }
  const LocalInfo::ValueKind kind = inferKind(valueExpr, locals);
  return kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64 ||
         (kind == LocalInfo::ValueKind::UInt64 || kind == LocalInfo::ValueKind::UInt8);
}

bool isTaskVectorWait(const Expr &expr, const LocalMap &locals) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name != "wait" ||
      expr.args.size() != 1 || expr.args.front().kind != Expr::Kind::Name) {
    return false;
  }
  const auto handleIt = locals.find(expr.args.front().name);
  return handleIt != locals.end() && !handleIt->second.taskResultVectorElementText.empty();
}

bool definitionReturnsBuiltinVector(
    const std::unordered_map<std::string, const Definition *> &defMap, const std::string &path) {
  const auto it = defMap.find(path);
  return it != defMap.end() && it->second != nullptr &&
         !declaredBuiltinVectorReturnElementText(*it->second).empty();
}

bool emitRealCallVectorReturn(const Expr &returnStmt,
                              const LocalMap &locals,
                              int32_t resultBlockLocal,
                              const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
                              const std::function<int32_t()> &allocTempLocal,
                              std::vector<IrInstruction> &instructions,
                              Expr &rewrittenStmt,
                              LocalMap &rewrittenLocals,
                              std::string &error) {
  constexpr int32_t VectorRecordSlots = 5;
  const Expr &valueExpr = returnStmt.args.front();
  const auto vectorIt =
      valueExpr.kind == Expr::Kind::Name ? locals.find(valueExpr.name) : locals.end();
  if (vectorIt == locals.end() || vectorIt->second.kind != LocalInfo::Kind::Vector ||
      vectorIt->second.dropFlagLocal < 0) {
    error = "a real call that returns a vector returns a local vector that owns its buffer";
    return false;
  }
  const int32_t srcPtrLocal = allocTempLocal();
  if (!emitExpr(valueExpr, locals)) {
    return false;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
  if (!emitStructCopyFromPtrs(instructions, resultBlockLocal, srcPtrLocal, VectorRecordSlots)) {
    return false;
  }
  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back(
      {IrOpcode::StoreLocal, static_cast<uint64_t>(vectorIt->second.dropFlagLocal)});
  rewrittenLocals = locals;
  LocalInfo returnInfo;
  returnInfo.kind = LocalInfo::Kind::Value;
  returnInfo.valueKind = LocalInfo::ValueKind::Int64;
  returnInfo.index = resultBlockLocal;
  const std::string tempName = "__native_return_vector_" + std::to_string(srcPtrLocal);
  rewrittenLocals.emplace(tempName, returnInfo);
  rewrittenStmt = returnStmt;
  Expr nameExpr;
  nameExpr.kind = Expr::Kind::Name;
  nameExpr.name = tempName;
  rewrittenStmt.args.front() = std::move(nameExpr);
  return true;
}

bool emitSumStorageReturn(int32_t ptrLocal,
                          int32_t totalSlots,
                          int32_t realCallResultBlockLocal,
                          const std::optional<ReturnStatementInlineContext> &inlineContext,
                          std::vector<IrInstruction> &instructions,
                          const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs,
                          const SharedReturnExitFn &emitSharedReturn,
                          const std::function<void()> &cleanupAll,
                          bool &sawReturn,
                          std::string &error) {
  if (realCallResultBlockLocal >= 0 && !inlineContext.has_value()) {
    if (!copyFromPtrs(realCallResultBlockLocal, ptrLocal, totalSlots)) {
      return false;
    }
    ptrLocal = realCallResultBlockLocal;
  }
  if (inlineContext.has_value()) {
    if (inlineContext->returnLocal < 0) {
      error = "native backend missing inline return local";
      return false;
    }
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back(
        {IrOpcode::StoreLocal, static_cast<uint64_t>(inlineContext->returnLocal)});
    const size_t jumpIndex = instructions.size();
    instructions.push_back({IrOpcode::Jump, 0});
    if (inlineContext->returnJumps != nullptr) {
      inlineContext->returnJumps->push_back(jumpIndex);
    }
    return true;
  }
  if (!emitSharedReturn(IrOpcode::ReturnI64, ptrLocal)) {
    cleanupAll();
    instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
    instructions.push_back({IrOpcode::ReturnI64, 0});
  }
  sawReturn = true;
  return true;
}

} // namespace primec::ir_lowerer
