#include "IrLowererLowerInlineCallActiveContextStep.h"

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
    const Expr *implicitReturnExpr = nullptr;
    if (success && input.callee->returnExpr.has_value()) {
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
