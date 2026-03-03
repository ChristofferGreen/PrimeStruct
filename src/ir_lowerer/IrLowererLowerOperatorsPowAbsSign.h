        ir_lowerer::OperatorPowAbsSignEmitResult powAbsSignResult = ir_lowerer::emitPowAbsSignOperatorExpr(
            expr,
            localsIn,
            hasMathImport,
            emitExpr,
            inferExprKind,
            combineNumericKinds,
            allocTempLocal,
            emitPowNegativeExponent,
            function.instructions,
            error);
        if (powAbsSignResult == ir_lowerer::OperatorPowAbsSignEmitResult::Error) {
          return false;
        }
        if (powAbsSignResult == ir_lowerer::OperatorPowAbsSignEmitResult::Handled) {
          return true;
        }
