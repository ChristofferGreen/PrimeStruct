#include "primec/ir_lowerer/IrLowererLowerInlineCallActiveContextStep.h"

namespace primec::ir_lowerer {

bool runLowerInlineCallActiveContextStep(const LowerInlineCallActiveContextStepInput &input,
                                         std::string &errorOut) {
  if (input.callee == nullptr) {
    errorOut = "native backend missing inline-call active-context step dependency: callee";
    return false;
  }
  if (!input.activateInlineContext) {
    errorOut = "native backend missing inline-call active-context step dependency: activateInlineContext";
    return false;
  }
  if (!input.restoreInlineContext) {
    errorOut = "native backend missing inline-call active-context step dependency: restoreInlineContext";
    return false;
  }
  if (!input.structDefinition && !input.emitInlineStatement) {
    errorOut = "native backend missing inline-call active-context step dependency: emitInlineStatement";
    return false;
  }
  if (!input.runInlineCleanup) {
    errorOut = "native backend missing inline-call active-context step dependency: runInlineCleanup";
    return false;
  }

  input.activateInlineContext();
  bool success = true;
  if (!input.structDefinition) {
    for (const auto &stmt : input.callee->statements) {
      if (!input.emitInlineStatement(stmt)) {
        success = false;
        break;
      }
    }
    // The parser keeps the value of a body's `return(value)` statement in `returnExpr` too. When the body has such a
    // statement, control never falls off its end, and lowering `returnExpr` again would only append an unreachable
    // copy of the value: inlined into an inlined callee, that copy doubled the code at each level.
    // `returnExpr` alone (no return statement) is a generated tail value and is lowered here.
    bool hasTopLevelReturn = false;
    for (const auto &stmt : input.callee->statements) {
      if (stmt.kind == Expr::Kind::Call && !stmt.isBinding && stmt.name == "return") {
        hasTopLevelReturn = true;
        break;
      }
    }
    const Expr *implicitReturnExpr = nullptr;
    if (success && hasTopLevelReturn) {
      implicitReturnExpr = nullptr;
    } else if (success && input.callee->returnExpr.has_value()) {
      implicitReturnExpr = &*input.callee->returnExpr;
    } else if (success && !input.definitionReturnsVoid && !input.callee->hasReturnStatement) {
      for (const auto &stmt : input.callee->statements) {
        if (!stmt.isBinding) {
          implicitReturnExpr = &stmt;
        }
      }
    }
    if (success && implicitReturnExpr != nullptr) {
      Expr returnStmt;
      returnStmt.kind = Expr::Kind::Call;
      returnStmt.name = "return";
      returnStmt.namespacePrefix = input.callee->namespacePrefix;
      returnStmt.sourceLine = implicitReturnExpr->sourceLine;
      returnStmt.sourceColumn = implicitReturnExpr->sourceColumn;
      returnStmt.args.push_back(*implicitReturnExpr);
      returnStmt.argNames.push_back(std::nullopt);
      if (!input.emitInlineStatement(returnStmt)) {
        success = false;
      }
    }
  }
  if (success) {
    success = input.runInlineCleanup();
  }
  input.restoreInlineContext();
  return success;
}

} // namespace primec::ir_lowerer
