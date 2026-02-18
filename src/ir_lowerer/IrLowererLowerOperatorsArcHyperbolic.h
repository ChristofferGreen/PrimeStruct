        std::string arcName;
        if (getBuiltinArcTrigName(expr, arcName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = arcName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = arcName + " requires float argument";
            return false;
          }
          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;

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

          auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightLocal)});
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
          };

          int32_t tempX = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          int32_t tempX2 = allocTempLocal();
          int32_t tempX3 = allocTempLocal();
          int32_t tempX5 = allocTempLocal();
          int32_t tempX7 = allocTempLocal();
          emitMulLocal(tempX, tempX, tempX2);
          emitMulLocal(tempX2, tempX, tempX3);
          emitMulLocal(tempX3, tempX2, tempX5);
          emitMulLocal(tempX5, tempX2, tempX7);

          if (arcName == "atan") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
            pushFloatConst(0.3333333333333333);
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
            pushFloatConst(0.2);
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({addOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
            pushFloatConst(0.14285714285714285);
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({subOp, 0});
            return true;
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
          pushFloatConst(0.16666666666666666);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
          pushFloatConst(0.075);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
          pushFloatConst(0.044642857142857144);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({addOp, 0});

          if (arcName == "asin") {
            return true;
          }

          int32_t tempAsin = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAsin)});
          pushFloatConst(1.5707963267948966);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAsin)});
          function.instructions.push_back({subOp, 0});
          return true;
        }
        std::string expName;
        if (getBuiltinExpName(expr, expName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = expName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = expName + " requires float argument";
            return false;
          }
          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode convertOp =
              (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::ConvertI32ToF64 : IrOpcode::ConvertI32ToF32;

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

          int32_t tempX = allocTempLocal();
          int32_t tempTerm = allocTempLocal();
          int32_t tempSum = allocTempLocal();
          int32_t tempIter = allocTempLocal();

          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          if (expName == "exp2") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            pushFloatConst(0.69314718055994530942); // ln(2)
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          }

          pushFloatConst(1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempTerm)});
          pushFloatConst(1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

          const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
          const size_t loopStart = function.instructions.size();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations + 1)});
          function.instructions.push_back({IrOpcode::CmpLtI32, 0});
          size_t jumpEndLoop = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempTerm)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
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

          size_t loopEndIndex = function.instructions.size();
          function.instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSum)});
          return true;
        }
        std::string logName;
        if (getBuiltinLogName(expr, logName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = logName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = logName + " requires float argument";
            return false;
          }
          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;

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

          int32_t tempX = allocTempLocal();
          int32_t tempZ = allocTempLocal();
          int32_t tempZ2 = allocTempLocal();
          int32_t tempTerm = allocTempLocal();
          int32_t tempSum = allocTempLocal();
          int32_t tempOut = allocTempLocal();

          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpLtOp, 0});
          size_t jumpIfNotNegative = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(0.0);
          pushFloatConst(0.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t nonNegativeIndex = function.instructions.size();
          function.instructions[jumpIfNotNegative].imm = static_cast<int32_t>(nonNegativeIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpEqOp, 0});
          size_t jumpIfNotZero = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(-1.0);
          pushFloatConst(0.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEndZero = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t positiveIndex = function.instructions.size();
          function.instructions[jumpIfNotZero].imm = static_cast<int32_t>(positiveIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(1.0);
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
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
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          if (logName == "log2") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            pushFloatConst(0.69314718055994530942);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          } else if (logName == "log10") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            pushFloatConst(2.30258509299404568402);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          }

          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string hyperName;
        if (getBuiltinHyperbolicName(expr, hyperName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = hyperName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = hyperName + " requires float argument";
            return false;
          }

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode negOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::NegF64 : IrOpcode::NegF32;
          IrOpcode convertOp =
              (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::ConvertI32ToF64 : IrOpcode::ConvertI32ToF32;

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

            const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 12 : 10;
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

          int32_t tempX = allocTempLocal();
          int32_t tempNegX = allocTempLocal();
          int32_t tempExpPos = allocTempLocal();
          int32_t tempExpNeg = allocTempLocal();

          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({negOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempNegX)});

          emitExpForLocal(tempX, tempExpPos);
          emitExpForLocal(tempNegX, tempExpNeg);

          if (hyperName == "sinh") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
            function.instructions.push_back({subOp, 0});
            pushFloatConst(2.0);
            function.instructions.push_back({divOp, 0});
            return true;
          }

          if (hyperName == "cosh") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
            function.instructions.push_back({addOp, 0});
            pushFloatConst(2.0);
            function.instructions.push_back({divOp, 0});
            return true;
          }

          int32_t tempNumerator = allocTempLocal();
          int32_t tempDenominator = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempNumerator)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpPos)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempExpNeg)});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempDenominator)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempNumerator)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempDenominator)});
          function.instructions.push_back({divOp, 0});
          return true;
        }
        std::string arcHyperName;
        if (getBuiltinArcHyperbolicName(expr, arcHyperName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = arcHyperName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = arcHyperName + " requires float argument";
            return false;
          }

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;

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

          auto emitSqrtForLocal = [&](int32_t valueLocal, int32_t outLocal) {
            int32_t tempX = allocTempLocal();
            int32_t tempIter = allocTempLocal();

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpEqOp, 0});
            size_t jumpIfNotZero = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpToEndZero = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t nonZeroIndex = function.instructions.size();
            function.instructions[jumpIfNotZero].imm = static_cast<int32_t>(nonZeroIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpToEndNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpIfNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

            const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 8 : 6;
            const size_t loopStart = function.instructions.size();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations)});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpEndLoop = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({addOp, 0});
            pushFloatConst(0.5);
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});
            function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

            const size_t loopEndIndex = function.instructions.size();
            function.instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});

            const size_t endIndex = function.instructions.size();
            function.instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
            function.instructions[jumpToEndNegative].imm = static_cast<int32_t>(endIndex);
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

          int32_t tempX = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          int32_t tempOut = allocTempLocal();
          if (arcHyperName == "atanh") {
            int32_t tempOnePlus = allocTempLocal();
            int32_t tempOneMinus = allocTempLocal();
            int32_t tempRatio = allocTempLocal();

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            pushFloatConst(1.0);
            function.instructions.push_back({addOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOnePlus)});

            pushFloatConst(1.0);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOneMinus)});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOnePlus)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOneMinus)});
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempRatio)});

            emitLogForLocal(tempRatio, tempOut);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            pushFloatConst(0.5);
            function.instructions.push_back({mulOp, 0});
            return true;
          }

          int32_t tempX2 = allocTempLocal();
          int32_t tempInner = allocTempLocal();
          int32_t tempSqrt = allocTempLocal();
          int32_t tempSum = allocTempLocal();

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX2)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
          pushFloatConst(1.0);
          function.instructions.push_back({arcHyperName == "asinh" ? addOp : subOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempInner)});

          emitSqrtForLocal(tempInner, tempSqrt);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSqrt)});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSum)});

          emitLogForLocal(tempSum, tempOut);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string powName;
