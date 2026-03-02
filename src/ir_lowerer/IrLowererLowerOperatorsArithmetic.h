          return true;
        }
        ir_lowerer::OperatorArithmeticEmitResult arithmeticResult = ir_lowerer::emitArithmeticOperatorExpr(
            expr,
            localsIn,
            emitExpr,
            inferExprKind,
            combineNumericKinds,
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (arithmeticResult == ir_lowerer::OperatorArithmeticEmitResult::Error) {
          return false;
        }
        if (arithmeticResult == ir_lowerer::OperatorArithmeticEmitResult::Handled) {
          return true;
        }
        std::string builtin;
