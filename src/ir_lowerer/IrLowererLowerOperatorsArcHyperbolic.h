        ir_lowerer::OperatorArcHyperbolicEmitResult arcHyperbolicResult =
            ir_lowerer::emitArcHyperbolicOperatorExpr(
                expr,
                localsIn,
                hasMathImport,
                emitExpr,
                inferExprKind,
                allocTempLocal,
                function.instructions,
                error);
        if (arcHyperbolicResult == ir_lowerer::OperatorArcHyperbolicEmitResult::Error) {
          return false;
        }
        if (arcHyperbolicResult == ir_lowerer::OperatorArcHyperbolicEmitResult::Handled) {
          return true;
        }
