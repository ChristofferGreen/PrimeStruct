        ir_lowerer::OperatorClampMinMaxTrigEmitResult clampMinMaxTrigResult =
            ir_lowerer::emitClampMinMaxTrigOperatorExpr(
                expr,
                localsIn,
                hasMathImport,
                emitExpr,
                inferExprKind,
                combineNumericKinds,
                allocTempLocal,
                function.instructions,
                error);
        if (clampMinMaxTrigResult == ir_lowerer::OperatorClampMinMaxTrigEmitResult::Error) {
          return false;
        }
        if (clampMinMaxTrigResult == ir_lowerer::OperatorClampMinMaxTrigEmitResult::Handled) {
          return true;
        }
