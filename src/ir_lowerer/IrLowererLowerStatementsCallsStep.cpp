#include "IrLowererLowerStatementsCallsStep.h"

#include "IrLowererFlowHelpers.h"

namespace primec::ir_lowerer {

bool runLowerStatementsCallsStep(const LowerStatementsCallsStepInput &input,
                                 const Expr &stmt,
                                 const LocalMap &localsIn,
                                 std::string &errorOut) {
  if (!input.inferExprKind) {
    errorOut = "native backend missing statements/calls step dependency: inferExprKind";
    return false;
  }
  if (!input.inferStructExprPath) {
    errorOut = "native backend missing statements/calls step dependency: inferStructExprPath";
    return false;
  }
  if (!input.emitExpr) {
    errorOut = "native backend missing statements/calls step dependency: emitExpr";
    return false;
  }
  if (!input.allocTempLocal) {
    errorOut = "native backend missing statements/calls step dependency: allocTempLocal";
    return false;
  }
  if (!input.resolveExprPath) {
    errorOut = "native backend missing statements/calls step dependency: resolveExprPath";
    return false;
  }
  if (!input.findDefinitionByPath) {
    errorOut = "native backend missing statements/calls step dependency: findDefinitionByPath";
    return false;
  }
  if (!input.resolveDestroyHelperForStruct) {
    errorOut = "native backend missing statements/calls step dependency: resolveDestroyHelperForStruct";
    return false;
  }
  if (!input.resolveMoveHelperForStruct) {
    errorOut = "native backend missing statements/calls step dependency: resolveMoveHelperForStruct";
    return false;
  }
  if (!input.isArrayCountCall) {
    errorOut = "native backend missing statements/calls step dependency: isArrayCountCall";
    return false;
  }
  if (!input.isStringCountCall) {
    errorOut = "native backend missing statements/calls step dependency: isStringCountCall";
    return false;
  }
  if (!input.isVectorCapacityCall) {
    errorOut = "native backend missing statements/calls step dependency: isVectorCapacityCall";
    return false;
  }
  if (!input.resolveMethodCallDefinition) {
    errorOut = "native backend missing statements/calls step dependency: resolveMethodCallDefinition";
    return false;
  }
  if (!input.resolveDefinitionCall) {
    errorOut = "native backend missing statements/calls step dependency: resolveDefinitionCall";
    return false;
  }
  if (!input.getReturnInfo) {
    errorOut = "native backend missing statements/calls step dependency: getReturnInfo";
    return false;
  }
  if (!input.emitInlineDefinitionCall) {
    errorOut = "native backend missing statements/calls step dependency: emitInlineDefinitionCall";
    return false;
  }
  if (!input.emitArrayIndexOutOfBounds) {
    errorOut = "native backend missing statements/calls step dependency: emitArrayIndexOutOfBounds";
    return false;
  }
  if (!input.emitVectorCapacityExceeded) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorCapacityExceeded";
    return false;
  }
  if (!input.emitVectorPopOnEmpty) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorPopOnEmpty";
    return false;
  }
  if (!input.emitVectorIndexOutOfBounds) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorIndexOutOfBounds";
    return false;
  }
  if (!input.emitVectorReserveNegative) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorReserveNegative";
    return false;
  }
  if (!input.emitVectorReserveExceeded) {
    errorOut = "native backend missing statements/calls step dependency: emitVectorReserveExceeded";
    return false;
  }
  if (input.instructions == nullptr) {
    errorOut = "native backend missing statements/calls step dependency: instructions";
    return false;
  }

  auto &instructions = *input.instructions;
  const auto bufferStoreResult = tryEmitBufferStoreStatement(
      stmt,
      localsIn,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.inferExprKind(valueExpr, valueLocals); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      [&]() { return input.allocTempLocal(); },
      instructions,
      errorOut);
  if (bufferStoreResult == BufferStoreStatementEmitResult::Error) {
    return false;
  }
  if (bufferStoreResult == BufferStoreStatementEmitResult::Emitted) {
    return true;
  }

  const auto dispatchResult = tryEmitDispatchStatement(
      stmt,
      localsIn,
      [&](const Expr &valueExpr) { return input.resolveExprPath(valueExpr); },
      [&](const std::string &path) { return input.findDefinitionByPath(path); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.inferExprKind(valueExpr, valueLocals); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      [&]() { return input.allocTempLocal(); },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
      },
      instructions,
      errorOut);
  if (dispatchResult == DispatchStatementEmitResult::Error) {
    return false;
  }
  if (dispatchResult == DispatchStatementEmitResult::Emitted) {
    return true;
  }

  const auto vectorHelperResult = tryEmitVectorStatementHelper(
      stmt,
      localsIn,
      instructions,
      [&]() { return input.allocTempLocal(); },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
        return input.inferExprKind(valueExpr, valueLocals);
      },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
        return input.inferStructExprPath(valueExpr, valueLocals);
      },
      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
        return input.emitExpr(valueExpr, valueLocals);
      },
      [&](const std::string &structPath) {
        return input.resolveDestroyHelperForStruct(structPath);
      },
      [&](const std::string &structPath) {
        return input.resolveMoveHelperForStruct(structPath);
      },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool requireValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, requireValue);
      },
      [&](const Expr &candidate) {
        if (candidate.isMethodCall && candidate.namespacePrefix.empty() &&
            !candidate.args.empty() &&
            (candidate.name == "push" || candidate.name == "pop" ||
             candidate.name == "reserve" || candidate.name == "clear" ||
             candidate.name == "remove_at" || candidate.name == "remove_swap") &&
            candidate.args.front().kind == Expr::Kind::Name) {
          auto localIt = localsIn.find(candidate.args.front().name);
          if (localIt != localsIn.end() && !localIt->second.isSoaVector &&
              (localIt->second.kind == LocalInfo::Kind::Vector ||
               localIt->second.structTypeName == "/std/collections/experimental_vector/Vector" ||
               localIt->second.structTypeName.rfind(
                   "/std/collections/experimental_vector/Vector__", 0) == 0)) {
            return false;
          }
        }
        if (candidate.isMethodCall && !input.isArrayCountCall(candidate, localsIn) &&
            !input.isStringCountCall(candidate, localsIn) &&
            !input.isVectorCapacityCall(candidate, localsIn)) {
          return input.resolveMethodCallDefinition(candidate, localsIn) != nullptr;
        }
        return input.resolveDefinitionCall(candidate) != nullptr;
      },
      [&]() { input.emitVectorCapacityExceeded(); },
      [&]() { input.emitVectorPopOnEmpty(); },
      [&]() { input.emitVectorIndexOutOfBounds(); },
      [&]() { input.emitArrayIndexOutOfBounds(); },
      [&]() { input.emitVectorReserveNegative(); },
      [&]() { input.emitVectorReserveExceeded(); },
      errorOut);
  if (vectorHelperResult == VectorStatementHelperEmitResult::Error) {
    return false;
  }
  if (vectorHelperResult == VectorStatementHelperEmitResult::Emitted) {
    return true;
  }

  const auto directCallResult = tryEmitDirectCallStatement(
      stmt,
      localsIn,
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isArrayCountCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isStringCountCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.isVectorCapacityCall(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) { return input.emitExpr(callExpr, callLocals); },
      [&](const Expr &callExpr, const LocalMap &callLocals) {
        return input.resolveMethodCallDefinition(callExpr, callLocals);
      },
      [&](const Expr &callExpr) { return input.resolveDefinitionCall(callExpr); },
      [&](const std::string &definitionPath, ReturnInfo &returnInfo) { return input.getReturnInfo(definitionPath, returnInfo); },
      [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals, bool expectValue) {
        return input.emitInlineDefinitionCall(callExpr, callee, callLocals, expectValue);
      },
      [&]() { input.emitArrayIndexOutOfBounds(); },
      instructions,
      errorOut);
  if (directCallResult == DirectCallStatementEmitResult::Error) {
    return false;
  }
  if (directCallResult == DirectCallStatementEmitResult::Emitted) {
    return true;
  }

  const auto assignOrExprResult = emitAssignOrExprStatementWithPop(
      stmt,
      localsIn,
      [&](const Expr &valueExpr, const LocalMap &valueLocals) { return input.emitExpr(valueExpr, valueLocals); },
      instructions);
  if (assignOrExprResult == AssignOrExprStatementEmitResult::Error) {
    return false;
  }
  return true;
}

} // namespace primec::ir_lowerer
