        ir_lowerer::OperatorComparisonEmitResult comparisonResult = ir_lowerer::emitComparisonOperatorExpr(
            expr,
            localsIn,
            emitExpr,
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
