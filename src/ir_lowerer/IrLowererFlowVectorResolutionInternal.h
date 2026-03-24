#pragma once

#include "IrLowererFlowHelpers.h"

namespace primec::ir_lowerer {

enum class SignedLiteralIntegerEvalResult { NotFoldable, Value, Overflow };
enum class UnsignedLiteralIntegerEvalResult { NotFoldable, Value, Overflow };
enum class VectorStatementHelperPrepareResult { NotMatched, Ready, Error };

struct PreparedVectorStatementHelperCall {
  std::string vectorHelper;
  Expr callStmt;
};

SignedLiteralIntegerEvalResult tryEvaluateSignedLiteralIntegerExpr(const Expr &expr, int64_t &out);
UnsignedLiteralIntegerEvalResult tryEvaluateUnsignedLiteralIntegerExpr(const Expr &expr,
                                                                      uint64_t &out);
VectorStatementHelperPrepareResult prepareVectorStatementHelperCall(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const std::function<bool(const Expr &)> &isUserDefinedVectorHelperCall,
    PreparedVectorStatementHelperCall &out,
    std::string &error);

} // namespace primec::ir_lowerer
