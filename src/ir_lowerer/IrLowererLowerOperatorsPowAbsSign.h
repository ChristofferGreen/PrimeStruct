        if (getBuiltinPowName(expr, powName, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = powName + " requires exactly two arguments";
            return false;
          }
          bool sawUnsigned = false;
          bool sawSigned = false;
          for (const auto &arg : expr.args) {
            LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
            if (arg.kind == Expr::Kind::Literal && arg.isUnsigned) {
              sawUnsigned = true;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              sawUnsigned = true;
            } else if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Int64) {
              sawSigned = true;
            }
          }
          if (sawUnsigned && sawSigned) {
            error = powName + " requires numeric arguments of the same type";
            return false;
          }
          LocalInfo::ValueKind powKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (powKind == LocalInfo::ValueKind::Unknown || powKind == LocalInfo::ValueKind::Bool ||
              powKind == LocalInfo::ValueKind::String) {
            error = powName + " requires numeric arguments of the same type";
            return false;
          }

          if (powKind == LocalInfo::ValueKind::Float32 || powKind == LocalInfo::ValueKind::Float64) {
            IrOpcode addOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
            IrOpcode subOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
            IrOpcode mulOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
            IrOpcode divOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
            IrOpcode cmpLtOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
            IrOpcode cmpEqOp = (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
            IrOpcode convertOp =
                (powKind == LocalInfo::ValueKind::Float64) ? IrOpcode::ConvertI32ToF64 : IrOpcode::ConvertI32ToF32;

            auto pushFloatConst = [&](double value) {
              if (powKind == LocalInfo::ValueKind::Float64) {
                uint64_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                function.instructions.push_back({IrOpcode::PushF64, bits});
                return;
              }
              float f32 = static_cast<float>(value);
              uint32_t bits = 0;
              std::memcpy(&bits, &f32, sizeof(bits));
              function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
            };

            auto emitExpForLocal = [&](int32_t valueLocal, int32_t outLocal) {
              int32_t tempTerm = allocTempLocal();
              int32_t tempSum = allocTempLocal();
              int32_t tempIter = allocTempLocal();

              pushFloatConst(1.0);
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
              pushFloatConst(1.0);
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
              function.instructions.push_back({IrOpcode::PushI32, 1});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

              const int32_t iterations = (powKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
              const size_t loopStart = function.instructions.size();
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
              function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations + 1)});
              function.instructions.push_back({IrOpcode::CmpLtI32, 0});
              size_t jumpEndLoop = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              function.instructions.push_back({mulOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
              function.instructions.push_back({convertOp, 0});
              function.instructions.push_back({divOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
              function.instructions.push_back({addOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
              function.instructions.push_back({IrOpcode::PushI32, 1});
              function.instructions.push_back({IrOpcode::AddI32, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});
              function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

              const size_t loopEndIndex = function.instructions.size();
              function.instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            };

            auto emitLogForLocal = [&](int32_t valueLocal, int32_t outLocal) {
              int32_t tempZ = allocTempLocal();
              int32_t tempZ2 = allocTempLocal();
              int32_t tempTerm = allocTempLocal();
              int32_t tempSum = allocTempLocal();

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              pushFloatConst(0.0);
              function.instructions.push_back({cmpLtOp, 0});
              size_t jumpIfNotNegative = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              pushFloatConst(0.0);
              pushFloatConst(0.0);
              function.instructions.push_back({divOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
              size_t jumpToEnd = function.instructions.size();
              function.instructions.push_back({IrOpcode::Jump, 0});

              size_t nonNegativeIndex = function.instructions.size();
              function.instructions[jumpIfNotNegative].imm = static_cast<int32_t>(nonNegativeIndex);

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              pushFloatConst(0.0);
              function.instructions.push_back({cmpEqOp, 0});
              size_t jumpIfNotZero = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              pushFloatConst(-1.0);
              pushFloatConst(0.0);
              function.instructions.push_back({divOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
              size_t jumpToEndZero = function.instructions.size();
              function.instructions.push_back({IrOpcode::Jump, 0});

              size_t positiveIndex = function.instructions.size();
              function.instructions[jumpIfNotZero].imm = static_cast<int32_t>(positiveIndex);

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              pushFloatConst(1.0);
              function.instructions.push_back({subOp, 0});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              pushFloatConst(1.0);
              function.instructions.push_back({addOp, 0});
              function.instructions.push_back({divOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ)});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
              function.instructions.push_back({mulOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ2)});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

              auto appendSeries = [&](double divisor) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ2)});
                function.instructions.push_back({mulOp, 0});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
                pushFloatConst(1.0 / divisor);
                function.instructions.push_back({mulOp, 0});
                function.instructions.push_back({addOp, 0});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
              };

              appendSeries(3.0);
              appendSeries(5.0);
              appendSeries(7.0);
              appendSeries(9.0);

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
              pushFloatConst(2.0);
              function.instructions.push_back({mulOp, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});

              const size_t endIndex = function.instructions.size();
              function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
              function.instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
            };

            int32_t tempBase = allocTempLocal();
            int32_t tempExponent = allocTempLocal();
            int32_t tempOut = allocTempLocal();
            int32_t tempLog = allocTempLocal();
            int32_t tempMul = allocTempLocal();

            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempBase)});
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempExponent)});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempBase)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNotNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpIfNotNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempBase)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpEqOp, 0});
            size_t jumpIfNotZeroBase = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExponent)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpEqOp, 0});
            size_t jumpIfExpNotZero = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(1.0);
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndZeroExp = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t expNotZeroIndex = function.instructions.size();
            function.instructions[jumpIfExpNotZero].imm = static_cast<int32_t>(expNotZeroIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExponent)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpLtOp, 0});
            size_t jumpIfExpPositive = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(1.0);
            pushFloatConst(0.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndNegExp = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t expPositiveIndex = function.instructions.size();
            function.instructions[jumpIfExpPositive].imm = static_cast<int32_t>(expPositiveIndex);
            pushFloatConst(0.0);
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndZeroBase = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t nonZeroBaseIndex = function.instructions.size();
            function.instructions[jumpIfNotZeroBase].imm = static_cast<int32_t>(nonZeroBaseIndex);

            emitLogForLocal(tempBase, tempLog);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExponent)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempLog)});
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMul)});
            emitExpForLocal(tempMul, tempOut);

            size_t endIndex = function.instructions.size();
            function.instructions[jumpToEndNegative].imm = static_cast<int32_t>(endIndex);
            function.instructions[jumpToEndZeroExp].imm = static_cast<int32_t>(endIndex);
            function.instructions[jumpToEndNegExp].imm = static_cast<int32_t>(endIndex);
            function.instructions[jumpToEndZeroBase].imm = static_cast<int32_t>(endIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }

          IrOpcode mulOp =
              (powKind == LocalInfo::ValueKind::Int64 || powKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::MulI64
                  : IrOpcode::MulI32;
          IrOpcode subOp =
              (powKind == LocalInfo::ValueKind::Int64 || powKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::SubI64
                  : IrOpcode::SubI32;
          IrOpcode cmpGtOp = IrOpcode::CmpGtI32;
          if (powKind == LocalInfo::ValueKind::UInt64) {
            cmpGtOp = IrOpcode::CmpGtU64;
          } else if (powKind == LocalInfo::ValueKind::Int64) {
            cmpGtOp = IrOpcode::CmpGtI64;
          } else {
            cmpGtOp = IrOpcode::CmpGtI32;
          }
          IrOpcode pushOp = (powKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;

          int32_t tempBase = allocTempLocal();
          int32_t tempExp = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempBase)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempExp)});

          if (powKind == LocalInfo::ValueKind::Int32 || powKind == LocalInfo::ValueKind::Int64) {
            IrOpcode cmpLtOp = (powKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtI32;
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExp)});
            function.instructions.push_back({pushOp, 0});
            function.instructions.push_back({cmpLtOp, 0});
            const size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitPowNegativeExponent();
            const size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
          }

          function.instructions.push_back({pushOp, 1});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          const size_t loopStart = function.instructions.size();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExp)});
          function.instructions.push_back({pushOp, 0});
          function.instructions.push_back({cmpGtOp, 0});
          const size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempBase)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExp)});
          function.instructions.push_back({pushOp, 1});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempExp)});

          function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

          const size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string absSignName;
        if (getBuiltinAbsSignName(expr, absSignName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = absSignName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::Bool ||
              argKind == LocalInfo::ValueKind::String) {
            error = absSignName + " requires numeric argument";
            return false;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

          auto pushFloatConst = [&](double value) {
            if (argKind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              std::memcpy(&bits, &value, sizeof(bits));
              function.instructions.push_back({IrOpcode::PushF64, bits});
              return;
            }
            float f32 = static_cast<float>(value);
            uint32_t bits = 0;
            std::memcpy(&bits, &f32, sizeof(bits));
            function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
          };
          auto pushZero = [&]() {
            if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
              pushFloatConst(0.0);
              return;
            }
            function.instructions.push_back(
                {argKind == LocalInfo::ValueKind::Int32 ? IrOpcode::PushI32 : IrOpcode::PushI64, 0});
          };
          auto pushOne = [&](double value) {
            if (argKind == LocalInfo::ValueKind::Float32 || argKind == LocalInfo::ValueKind::Float64) {
              pushFloatConst(value);
              return;
            }
            if (argKind == LocalInfo::ValueKind::Int32) {
              function.instructions.push_back(
                  {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
            } else {
              function.instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
            }
          };

          if (absSignName == "abs") {
            if (argKind == LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
              return true;
            }
            IrOpcode cmpLt = IrOpcode::CmpLtI32;
            IrOpcode negOp = IrOpcode::NegI32;
            if (argKind == LocalInfo::ValueKind::Int64) {
              cmpLt = IrOpcode::CmpLtI64;
              negOp = IrOpcode::NegI64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              cmpLt = IrOpcode::CmpLtF64;
              negOp = IrOpcode::NegF64;
            } else if (argKind == LocalInfo::ValueKind::Float32) {
              cmpLt = IrOpcode::CmpLtF32;
              negOp = IrOpcode::NegF32;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushZero();
            function.instructions.push_back({cmpLt, 0});
            size_t useValue = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({negOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t useValueIndex = function.instructions.size();
            function.instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t endIndex = function.instructions.size();
            function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }

          if (argKind == LocalInfo::ValueKind::UInt64) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushZero();
            function.instructions.push_back({IrOpcode::CmpEqI64, 0});
            size_t nonZeroIndex = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushOne(0);
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t notZeroLabel = function.instructions.size();
            function.instructions[nonZeroIndex].imm = static_cast<int32_t>(notZeroLabel);
            pushOne(1);
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t endIndex = function.instructions.size();
            function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }

          IrOpcode cmpLt = IrOpcode::CmpLtI32;
          IrOpcode cmpGt = IrOpcode::CmpGtI32;
          if (argKind == LocalInfo::ValueKind::Int64) {
            cmpLt = IrOpcode::CmpLtI64;
            cmpGt = IrOpcode::CmpGtI64;
          } else if (argKind == LocalInfo::ValueKind::Float64) {
            cmpLt = IrOpcode::CmpLtF64;
            cmpGt = IrOpcode::CmpGtF64;
          } else if (argKind == LocalInfo::ValueKind::Float32) {
            cmpLt = IrOpcode::CmpLtF32;
            cmpGt = IrOpcode::CmpGtF32;
          }
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushZero();
          function.instructions.push_back({cmpLt, 0});
          size_t checkPositive = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushOne(-1);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t positiveIndex = function.instructions.size();
          function.instructions[checkPositive].imm = static_cast<int32_t>(positiveIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushZero();
          function.instructions.push_back({cmpGt, 0});
          size_t useZero = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushOne(1);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t zeroIndex = function.instructions.size();
          function.instructions[useZero].imm = static_cast<int32_t>(zeroIndex);
          pushOne(0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
