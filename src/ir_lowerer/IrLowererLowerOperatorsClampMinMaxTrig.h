        if (getBuiltinClampName(expr, hasMathImport)) {
          if (expr.args.size() != 3) {
            error = "clamp requires exactly three arguments";
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
            error = "clamp requires numeric arguments of the same type";
            return false;
          }
          LocalInfo::ValueKind clampKind =
              combineNumericKinds(combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                      inferExprKind(expr.args[1], localsIn)),
                                  inferExprKind(expr.args[2], localsIn));
          if (clampKind == LocalInfo::ValueKind::Unknown) {
            error = "clamp requires numeric arguments of the same type";
            return false;
          }
          IrOpcode cmpLt = IrOpcode::CmpLtI32;
          IrOpcode cmpGt = IrOpcode::CmpGtI32;
          if (clampKind == LocalInfo::ValueKind::UInt64) {
            cmpLt = IrOpcode::CmpLtU64;
            cmpGt = IrOpcode::CmpGtU64;
          } else if (clampKind == LocalInfo::ValueKind::Int64) {
            cmpLt = IrOpcode::CmpLtI64;
            cmpGt = IrOpcode::CmpGtI64;
          } else if (clampKind == LocalInfo::ValueKind::Float64) {
            cmpLt = IrOpcode::CmpLtF64;
            cmpGt = IrOpcode::CmpGtF64;
          } else if (clampKind == LocalInfo::ValueKind::Float32) {
            cmpLt = IrOpcode::CmpLtF32;
            cmpGt = IrOpcode::CmpGtF32;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempMin = allocTempLocal();
          int32_t tempMax = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMin)});
          if (!emitExpr(expr.args[2], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMax)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          function.instructions.push_back({cmpLt, 0});
          size_t skipMin = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMin)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t checkMax = function.instructions.size();
          function.instructions[skipMin].imm = static_cast<int32_t>(checkMax);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          function.instructions.push_back({cmpGt, 0});
          size_t skipMax = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMax)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t useValue = function.instructions.size();
          function.instructions[skipMax].imm = static_cast<int32_t>(useValue);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string minMaxName;
        if (getBuiltinMinMaxName(expr, minMaxName, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = minMaxName + " requires exactly two arguments";
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
            error = minMaxName + " requires numeric arguments of the same type";
            return false;
          }
          LocalInfo::ValueKind minMaxKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (minMaxKind == LocalInfo::ValueKind::Unknown) {
            error = minMaxName + " requires numeric arguments of the same type";
            return false;
          }
          IrOpcode cmpOp = IrOpcode::CmpLtI32;
          if (minMaxName == "max") {
            cmpOp = IrOpcode::CmpGtI32;
          }
          if (minMaxKind == LocalInfo::ValueKind::UInt64) {
            cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtU64 : IrOpcode::CmpLtU64;
          } else if (minMaxKind == LocalInfo::ValueKind::Int64) {
            cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtI64 : IrOpcode::CmpLtI64;
          } else if (minMaxKind == LocalInfo::ValueKind::Float64) {
            cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtF64 : IrOpcode::CmpLtF64;
          } else if (minMaxKind == LocalInfo::ValueKind::Float32) {
            cmpOp = (minMaxName == "max") ? IrOpcode::CmpGtF32 : IrOpcode::CmpLtF32;
          }
          int32_t tempLeft = allocTempLocal();
          int32_t tempRight = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempLeft)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempRight)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempLeft)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempRight)});
          function.instructions.push_back({cmpOp, 0});
          size_t useRight = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempLeft)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t useRightIndex = function.instructions.size();
          function.instructions[useRight].imm = static_cast<int32_t>(useRightIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempRight)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string lerpName;
        if (getBuiltinLerpName(expr, lerpName, hasMathImport)) {
          if (expr.args.size() != 3) {
            error = lerpName + " requires exactly three arguments";
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
            error = lerpName + " requires numeric arguments of the same type";
            return false;
          }
          LocalInfo::ValueKind lerpKind =
              combineNumericKinds(combineNumericKinds(inferExprKind(expr.args[0], localsIn),
                                                      inferExprKind(expr.args[1], localsIn)),
                                  inferExprKind(expr.args[2], localsIn));
          if (lerpKind == LocalInfo::ValueKind::Unknown) {
            error = lerpName + " requires numeric arguments of the same type";
            return false;
          }
          IrOpcode addOp = IrOpcode::AddI32;
          IrOpcode subOp = IrOpcode::SubI32;
          IrOpcode mulOp = IrOpcode::MulI32;
          if (lerpKind == LocalInfo::ValueKind::Int64 || lerpKind == LocalInfo::ValueKind::UInt64) {
            addOp = IrOpcode::AddI64;
            subOp = IrOpcode::SubI64;
            mulOp = IrOpcode::MulI64;
          } else if (lerpKind == LocalInfo::ValueKind::Float64) {
            addOp = IrOpcode::AddF64;
            subOp = IrOpcode::SubF64;
            mulOp = IrOpcode::MulF64;
          } else if (lerpKind == LocalInfo::ValueKind::Float32) {
            addOp = IrOpcode::AddF32;
            subOp = IrOpcode::SubF32;
            mulOp = IrOpcode::MulF32;
          }
          int32_t tempStart = allocTempLocal();
          int32_t tempEnd = allocTempLocal();
          int32_t tempT = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempStart)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempEnd)});
          if (!emitExpr(expr.args[2], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempT)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempEnd)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempStart)});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempT)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempStart)});
          function.instructions.push_back({addOp, 0});
          return true;
        }
        std::string fmaName;
        if (getBuiltinFmaName(expr, fmaName, hasMathImport)) {
          if (expr.args.size() != 3) {
            error = fmaName + " requires exactly three arguments";
            return false;
          }
          LocalInfo::ValueKind aKind = inferExprKind(expr.args[0], localsIn);
          LocalInfo::ValueKind bKind = inferExprKind(expr.args[1], localsIn);
          LocalInfo::ValueKind cKind = inferExprKind(expr.args[2], localsIn);
          auto isFloatKind = [](LocalInfo::ValueKind kind) {
            return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
          };
          if (!isFloatKind(aKind) || !isFloatKind(bKind) || !isFloatKind(cKind)) {
            error = fmaName + " requires float arguments";
            return false;
          }
          if (aKind != bKind || aKind != cKind) {
            error = fmaName + " requires float arguments of the same type";
            return false;
          }
          IrOpcode mulOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode addOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({mulOp, 0});
          if (!emitExpr(expr.args[2], localsIn)) {
            return false;
          }
          function.instructions.push_back({addOp, 0});
          return true;
        }
        std::string hypotName;
        if (getBuiltinHypotName(expr, hypotName, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = hypotName + " requires exactly two arguments";
            return false;
          }
          LocalInfo::ValueKind aKind = inferExprKind(expr.args[0], localsIn);
          LocalInfo::ValueKind bKind = inferExprKind(expr.args[1], localsIn);
          auto isFloatKind = [](LocalInfo::ValueKind kind) {
            return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
          };
          if (!isFloatKind(aKind) || !isFloatKind(bKind)) {
            error = hypotName + " requires float arguments";
            return false;
          }
          if (aKind != bKind) {
            error = hypotName + " requires float arguments of the same type";
            return false;
          }
          int32_t tempA = allocTempLocal();
          int32_t tempB = allocTempLocal();
          int32_t tempValue = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          int32_t tempIter = allocTempLocal();
          int32_t tempX = allocTempLocal();

          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempA)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempB)});

          IrOpcode addOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode mulOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpEqOp = (aKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;

          auto pushFloatConst = [&](double value) {
            if (aKind == LocalInfo::ValueKind::Float64) {
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

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempA)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempA)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempB)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempB)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpEqOp, 0});
          size_t jumpIfNotZero = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(0.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEndZero = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t nonZeroIndex = function.instructions.size();
          function.instructions[jumpIfNotZero].imm = static_cast<int32_t>(nonZeroIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::PushI32, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

          const int32_t iterations = (aKind == LocalInfo::ValueKind::Float64) ? 8 : 6;
          const size_t loopStart = function.instructions.size();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations)});
          function.instructions.push_back({IrOpcode::CmpLtI32, 0});
          size_t jumpEndLoop = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
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

          size_t loopEndIndex = function.instructions.size();
          function.instructions[jumpEndLoop].imm = static_cast<int32_t>(loopEndIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEndZero].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string copySignName;
        if (getBuiltinCopysignName(expr, copySignName, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = copySignName + " requires exactly two arguments";
            return false;
          }
          LocalInfo::ValueKind xKind = inferExprKind(expr.args[0], localsIn);
          LocalInfo::ValueKind yKind = inferExprKind(expr.args[1], localsIn);
          auto isFloatKind = [](LocalInfo::ValueKind kind) {
            return kind == LocalInfo::ValueKind::Float32 || kind == LocalInfo::ValueKind::Float64;
          };
          if (!isFloatKind(xKind) || !isFloatKind(yKind)) {
            error = copySignName + " requires float arguments";
            return false;
          }
          if (xKind != yKind) {
            error = copySignName + " requires float arguments of the same type";
            return false;
          }
          int32_t tempX = allocTempLocal();
          int32_t tempY = allocTempLocal();
          int32_t tempAbs = allocTempLocal();
          int32_t tempOut = allocTempLocal();

          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempY)});

          IrOpcode cmpLtOp = (xKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;
          IrOpcode negOp = (xKind == LocalInfo::ValueKind::Float64) ? IrOpcode::NegF64 : IrOpcode::NegF32;

          auto pushFloatZero = [&]() {
            if (xKind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              double value = 0.0;
              std::memcpy(&bits, &value, sizeof(bits));
              function.instructions.push_back({IrOpcode::PushF64, bits});
              return;
            }
            uint32_t bits = 0;
            float value = 0.0f;
            std::memcpy(&bits, &value, sizeof(bits));
            function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
          };

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatZero();
          function.instructions.push_back({cmpLtOp, 0});
          size_t useNeg = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({negOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAbs)});
          size_t jumpAbsEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t usePosIndex = function.instructions.size();
          function.instructions[useNeg].imm = static_cast<int32_t>(usePosIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAbs)});

          size_t absEndIndex = function.instructions.size();
          function.instructions[jumpAbsEnd].imm = static_cast<int32_t>(absEndIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
          pushFloatZero();
          function.instructions.push_back({cmpLtOp, 0});
          size_t usePositive = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAbs)});
          function.instructions.push_back({negOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t usePositiveIndex = function.instructions.size();
          function.instructions[usePositive].imm = static_cast<int32_t>(usePositiveIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempAbs)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        std::string angleName;
        if (getBuiltinAngleName(expr, angleName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = angleName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = angleName + " requires float argument";
            return false;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
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
          if (angleName == "radians") {
            pushFloatConst(0.017453292519943295); // pi / 180
          } else {
            pushFloatConst(57.29577951308232); // 180 / pi
          }
          function.instructions.push_back(
              {argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::MulF64 : IrOpcode::MulF32, 0});
          return true;
        }
        std::string trigName;
        if (getBuiltinTrigName(expr, trigName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = trigName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = trigName + " requires float argument";
            return false;
          }
          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;

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
          auto emitSquare = [&](int32_t valueLocal, int32_t outLocal) {
            emitMulLocal(valueLocal, valueLocal, outLocal);
          };

          auto emitFloatTruncToLocal = [&](int32_t valueLocal, int32_t outLocal) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            if (argKind == LocalInfo::ValueKind::Float32) {
              function.instructions.push_back({IrOpcode::ConvertF32ToI32, 0});
              function.instructions.push_back({IrOpcode::ConvertI32ToF32, 0});
            } else {
              function.instructions.push_back({IrOpcode::ConvertF64ToI64, 0});
              function.instructions.push_back({IrOpcode::ConvertI64ToF64, 0});
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
          };

          auto emitFloorToLocal = [&](int32_t valueLocal, int32_t outLocal) {
            emitFloatTruncToLocal(valueLocal, outLocal);
            IrOpcode cmpOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpGtF64 : IrOpcode::CmpGtF32;
            IrOpcode subFloatOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({cmpOp, 0});
            size_t useValue = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
            pushFloatConst(1.0);
            function.instructions.push_back({subFloatOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t useValueIndex = function.instructions.size();
            function.instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          };

          int32_t tempX = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          constexpr double kPi = 3.14159265358979323846;
          constexpr double kTau = 6.28318530717958647692;
          constexpr double kHalfPi = 1.57079632679489661923;
          int32_t tempScale = allocTempLocal();
          int32_t tempFloor = allocTempLocal();
          int32_t tempMul = allocTempLocal();
          IrOpcode cmpGtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpGtF64 : IrOpcode::CmpGtF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

          // Range-reduce x into [-pi, pi] to keep the series stable.
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(kTau);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempScale)});
          emitFloorToLocal(tempScale, tempFloor);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempFloor)});
          pushFloatConst(kTau);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempMul)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempMul)});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(kPi);
          function.instructions.push_back({cmpGtOp, 0});
          size_t skipWrap = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(kTau);
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          size_t jumpWrapEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t skipWrapIndex = function.instructions.size();
          function.instructions[skipWrap].imm = static_cast<int32_t>(skipWrapIndex);
          size_t wrapEndIndex = function.instructions.size();
          function.instructions[jumpWrapEnd].imm = static_cast<int32_t>(wrapEndIndex);

          int32_t tempSinSign = allocTempLocal();
          int32_t tempCosSign = allocTempLocal();
          pushFloatConst(1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSinSign)});
          pushFloatConst(1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(kHalfPi);
          function.instructions.push_back({cmpGtOp, 0});
          size_t skipUpper = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushFloatConst(kPi);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(-1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});
          size_t skipUpperIndex = function.instructions.size();
          function.instructions[skipUpper].imm = static_cast<int32_t>(skipUpperIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(-kHalfPi);
          function.instructions.push_back({cmpLtOp, 0});
          size_t skipLower = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(kPi);
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(-1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSinSign)});
          pushFloatConst(-1.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCosSign)});
          size_t skipLowerIndex = function.instructions.size();
          function.instructions[skipLower].imm = static_cast<int32_t>(skipLowerIndex);

          int32_t tempX2 = allocTempLocal();
          emitSquare(tempX, tempX2);

          if (trigName == "sin") {
            int32_t tempX3 = allocTempLocal();
            int32_t tempX5 = allocTempLocal();
            int32_t tempX7 = allocTempLocal();
            emitMulLocal(tempX2, tempX, tempX3);
            emitMulLocal(tempX3, tempX2, tempX5);
            emitMulLocal(tempX5, tempX2, tempX7);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
            pushFloatConst(6.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
            pushFloatConst(120.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({addOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
            pushFloatConst(5040.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSinSign)});
            function.instructions.push_back({mulOp, 0});
            return true;
          }

          if (trigName == "cos") {
            int32_t tempX4 = allocTempLocal();
            int32_t tempX6 = allocTempLocal();
            emitMulLocal(tempX2, tempX2, tempX4);
            emitMulLocal(tempX4, tempX2, tempX6);

            pushFloatConst(1.0);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
            pushFloatConst(2.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX4)});
            pushFloatConst(24.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({addOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX6)});
            pushFloatConst(720.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCosSign)});
            function.instructions.push_back({mulOp, 0});
            return true;
          }

          // tan(x) ≈ sin(x) / cos(x)
          int32_t tempX3 = allocTempLocal();
          int32_t tempX5 = allocTempLocal();
          int32_t tempX7 = allocTempLocal();
          int32_t tempX4 = allocTempLocal();
          int32_t tempX6 = allocTempLocal();
          int32_t tempSin = allocTempLocal();
          int32_t tempCos = allocTempLocal();

          emitMulLocal(tempX2, tempX, tempX3);
          emitMulLocal(tempX3, tempX2, tempX5);
          emitMulLocal(tempX5, tempX2, tempX7);
          emitMulLocal(tempX2, tempX2, tempX4);
          emitMulLocal(tempX4, tempX2, tempX6);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX3)});
          pushFloatConst(6.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX5)});
          pushFloatConst(120.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX7)});
          pushFloatConst(5040.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSinSign)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempSin)});

          pushFloatConst(1.0);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX2)});
          pushFloatConst(2.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX4)});
          pushFloatConst(24.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX6)});
          pushFloatConst(720.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCosSign)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempCos)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempSin)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempCos)});
          function.instructions.push_back({divOp, 0});
          return true;
        }
        std::string trig2Name;
        if (getBuiltinTrig2Name(expr, trig2Name, hasMathImport)) {
          if (expr.args.size() != 2) {
            error = trig2Name + " requires exactly two arguments";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args[0], localsIn);
          LocalInfo::ValueKind argKind2 = inferExprKind(expr.args[1], localsIn);
          if ((argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) ||
              argKind != argKind2) {
            error = trig2Name + " requires float arguments of the same type";
            return false;
          }

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

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

          int32_t tempY = allocTempLocal();
          int32_t tempX = allocTempLocal();
          int32_t tempZ = allocTempLocal();
          int32_t tempZ2 = allocTempLocal();
          int32_t tempZ3 = allocTempLocal();
          int32_t tempZ5 = allocTempLocal();
          int32_t tempOut = allocTempLocal();

          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempY)});
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});

          // Handle x == 0 axis cases.
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpEqOp, 0});
          size_t jumpIfNotZeroX = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpEqOp, 0});
          size_t jumpIfNotZeroY = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          pushFloatConst(0.0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpAxisEndZero = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t nonZeroYIndex = function.instructions.size();
          function.instructions[jumpIfNotZeroY].imm = static_cast<int32_t>(nonZeroYIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpLtOp, 0});
          size_t jumpIfPosY = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          pushFloatConst(-1.5707963267948966);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpAxisEndNeg = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t posYIndex = function.instructions.size();
          function.instructions[jumpIfPosY].imm = static_cast<int32_t>(posYIndex);
          pushFloatConst(1.5707963267948966);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpAxisEndPos = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t notZeroXIndex = function.instructions.size();
          function.instructions[jumpIfNotZeroX].imm = static_cast<int32_t>(notZeroXIndex);

          // atan2(y, x) ≈ atan(y/x) with quadrant adjustment.
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempZ)});

          auto emitMulLocal = [&](int32_t leftLocal, int32_t rightLocal, int32_t outLocal) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightLocal)});
            function.instructions.push_back({mulOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
          };

          emitMulLocal(tempZ, tempZ, tempZ2);
          emitMulLocal(tempZ2, tempZ, tempZ3);
          emitMulLocal(tempZ3, tempZ2, tempZ5);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ3)});
          pushFloatConst(3.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({subOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempZ5)});
          pushFloatConst(5.0);
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpLtOp, 0});
          size_t jumpIfNotNegX = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempY)});
          pushFloatConst(0.0);
          function.instructions.push_back({cmpLtOp, 0});
          size_t jumpIfPosYAdj = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          pushFloatConst(-3.14159265358979323846);
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpAdjustEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          size_t posYAdjIndex = function.instructions.size();
          function.instructions[jumpIfPosYAdj].imm = static_cast<int32_t>(posYAdjIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          pushFloatConst(3.14159265358979323846);
          function.instructions.push_back({addOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t adjustEndIndex = function.instructions.size();
          function.instructions[jumpAdjustEnd].imm = static_cast<int32_t>(adjustEndIndex);

          size_t notNegXIndex = function.instructions.size();
          function.instructions[jumpIfNotNegX].imm = static_cast<int32_t>(notNegXIndex);

          size_t endIndex = function.instructions.size();
          function.instructions[jumpAxisEndZero].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpAxisEndNeg].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpAxisEndPos].imm = static_cast<int32_t>(endIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
