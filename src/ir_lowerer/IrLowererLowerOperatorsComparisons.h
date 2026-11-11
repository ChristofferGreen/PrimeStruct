ir_lowerer::OperatorComparisonEmitResult comparisonResult = ir_lowerer::emitComparisonOperatorExpr(
    expr,
    localsIn,
    [&](const Expr &operandExpr, const LocalMap &operandLocals) {
      // The right side of `and`/`or` may not run.
      if (expr.args.size() == 2 && &operandExpr == &expr.args[1]) {
        std::string logicalBuiltin;
        if (getBuiltinComparisonName(expr, logicalBuiltin) &&
            (logicalBuiltin == "and" || logicalBuiltin == "or")) {
          return emitScopedValueExpr(operandExpr, operandLocals);
        }
      }
      return emitExpr(operandExpr, operandLocals);
    },
    inferExprKind,
    comparisonKind,
    emitCompareToZero,
    [&]() { return allocTempLocal(); },
    function.instructions,
    error);
if (comparisonResult == ir_lowerer::OperatorComparisonEmitResult::Error) {
  return false;
}
        if (comparisonResult == ir_lowerer::OperatorComparisonEmitResult::Handled) {
          return true;
        }
