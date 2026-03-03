        ir_lowerer::OperatorSaturateRoundingRootsEmitResult saturateRoundingRootsResult =
            ir_lowerer::emitSaturateRoundingRootsOperatorExpr(
                expr,
                localsIn,
                hasMathImport,
                emitExpr,
                inferExprKind,
                allocTempLocal,
                function.instructions,
                error);
        if (saturateRoundingRootsResult == ir_lowerer::OperatorSaturateRoundingRootsEmitResult::Error) {
          return false;
        }
        if (saturateRoundingRootsResult == ir_lowerer::OperatorSaturateRoundingRootsEmitResult::Handled) {
          return true;
        }
