          return true;
        }
        std::string builtin;
        if (getBuiltinOperatorName(expr, builtin)) {
          if (builtin == "negate") {
            if (expr.args.size() != 1) {
              error = "negate requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (kind == LocalInfo::ValueKind::Bool || kind == LocalInfo::ValueKind::Unknown) {
              error = "negate requires numeric operand";
              return false;
            }
            if (kind == LocalInfo::ValueKind::UInt64) {
              error = "negate does not support unsigned operands";
              return false;
            }
            function.instructions.push_back({kind == LocalInfo::ValueKind::Int64 ? IrOpcode::NegI64 : IrOpcode::NegI32,
                                             0});
            return true;
          }
          if (expr.args.size() != 2) {
            error = builtin + " requires exactly two arguments";
            return false;
          }
          std::function<bool(const Expr &, const LocalMap &)> isPointerOperand;
          isPointerOperand = [&](const Expr &candidate, const LocalMap &localsRef) -> bool {
            if (candidate.kind == Expr::Kind::Name) {
              auto it = localsRef.find(candidate.name);
              return it != localsRef.end() && it->second.kind == LocalInfo::Kind::Pointer;
            }
            if (candidate.kind == Expr::Kind::Call && isSimpleCallName(candidate, "location")) {
              return true;
            }
            if (candidate.kind == Expr::Kind::Call) {
              std::string opName;
              if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
                  candidate.args.size() == 2) {
                return isPointerOperand(candidate.args[0], localsRef) && !isPointerOperand(candidate.args[1], localsRef);
              }
            }
            return false;
          };
          bool leftPointer = false;
          bool rightPointer = false;
          if (builtin == "plus" || builtin == "minus") {
            leftPointer = isPointerOperand(expr.args[0], localsIn);
            rightPointer = isPointerOperand(expr.args[1], localsIn);
            if (leftPointer && rightPointer) {
              error = "pointer arithmetic does not support pointer + pointer";
              return false;
            }
            if (rightPointer) {
              error = "pointer arithmetic requires pointer on the left";
              return false;
            }
            if (leftPointer || rightPointer) {
              const Expr &offsetExpr = leftPointer ? expr.args[1] : expr.args[0];
              LocalInfo::ValueKind offsetKind = inferExprKind(offsetExpr, localsIn);
              if (offsetKind != LocalInfo::ValueKind::Int32 && offsetKind != LocalInfo::ValueKind::Int64 &&
                  offsetKind != LocalInfo::ValueKind::UInt64) {
                error = "pointer arithmetic requires an integer offset";
                return false;
              }
            }
          }
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          LocalInfo::ValueKind numericKind =
              combineNumericKinds(inferExprKind(expr.args[0], localsIn), inferExprKind(expr.args[1], localsIn));
          if (numericKind == LocalInfo::ValueKind::Unknown && !(leftPointer || rightPointer)) {
            error = "unsupported operand types for " + builtin;
            return false;
          }
          IrOpcode op = IrOpcode::AddI32;
          if (builtin == "plus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::AddI64;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::AddI64;
            } else {
              op = IrOpcode::AddI32;
            }
          } else if (builtin == "minus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::SubI64;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::SubI64;
            } else {
              op = IrOpcode::SubI32;
            }
          } else if (builtin == "multiply") {
            op = (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64)
                     ? IrOpcode::MulI64
                     : IrOpcode::MulI32;
          } else if (builtin == "divide") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::DivU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::DivI64;
            } else {
              op = IrOpcode::DivI32;
            }
          }
          function.instructions.push_back({op, 0});
          return true;
        }
        if (getBuiltinComparisonName(expr, builtin)) {
          if (builtin == "not") {
            if (expr.args.size() != 1) {
              error = "not requires exactly one argument";
              return false;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            return emitCompareToZero(kind, true);
          }
          if (builtin == "and") {
            if (expr.args.size() != 2) {
              error = "and requires exactly two arguments";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
            if (!emitCompareToZero(leftKind, false)) {
              return false;
            }
            size_t jumpFalse = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
            if (!emitCompareToZero(rightKind, false)) {
              return false;
            }
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t falseIndex = function.instructions.size();
            function.instructions[jumpFalse].imm = static_cast<int32_t>(falseIndex);
            function.instructions.push_back({IrOpcode::PushI32, 0});
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          if (builtin == "or") {
            if (expr.args.size() != 2) {
              error = "or requires exactly two arguments";
              return false;
            }
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
            if (!emitCompareToZero(leftKind, false)) {
              return false;
            }
            size_t jumpEval = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t evalIndex = function.instructions.size();
            function.instructions[jumpEval].imm = static_cast<int32_t>(evalIndex);
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
            if (!emitCompareToZero(rightKind, false)) {
              return false;
            }
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          if (expr.args.size() != 2) {
            error = builtin + " requires exactly two arguments";
            return false;
          }
          LocalInfo::ValueKind leftKind = inferExprKind(expr.args[0], localsIn);
          LocalInfo::ValueKind rightKind = inferExprKind(expr.args[1], localsIn);
          if (leftKind == LocalInfo::ValueKind::String || rightKind == LocalInfo::ValueKind::String) {
            error = "native backend does not support string comparisons";
            return false;
          }
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          LocalInfo::ValueKind numericKind = comparisonKind(leftKind, rightKind);
          if (numericKind == LocalInfo::ValueKind::Unknown) {
            error = "unsupported operand types for " + builtin;
            return false;
          }
          IrOpcode op = IrOpcode::CmpEqI32;
          if (builtin == "equal") {
            op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                     ? IrOpcode::CmpEqI64
                     : IrOpcode::CmpEqI32;
          } else if (builtin == "not_equal") {
            op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                     ? IrOpcode::CmpNeI64
                     : IrOpcode::CmpNeI32;
          } else if (builtin == "less_than") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLtI64;
            } else {
              op = IrOpcode::CmpLtI32;
            }
          } else if (builtin == "less_equal") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLeI64;
            } else {
              op = IrOpcode::CmpLeI32;
            }
          } else if (builtin == "greater_than") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGtI64;
            } else {
              op = IrOpcode::CmpGtI32;
            }
          } else if (builtin == "greater_equal") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGeI64;
            } else {
              op = IrOpcode::CmpGeI32;
            }
          }
          function.instructions.push_back({op, 0});
          return true;
        }
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
          IrOpcode addOp =
              (lerpKind == LocalInfo::ValueKind::Int64 || lerpKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::AddI64
                  : IrOpcode::AddI32;
          IrOpcode subOp =
              (lerpKind == LocalInfo::ValueKind::Int64 || lerpKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::SubI64
                  : IrOpcode::SubI32;
          IrOpcode mulOp =
              (lerpKind == LocalInfo::ValueKind::Int64 || lerpKind == LocalInfo::ValueKind::UInt64)
                  ? IrOpcode::MulI64
                  : IrOpcode::MulI32;
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
        std::string powName;
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

          auto pushZero = [&]() {
            function.instructions.push_back(
                {argKind == LocalInfo::ValueKind::Int32 ? IrOpcode::PushI32 : IrOpcode::PushI64, 0});
          };
          auto pushOne = [&](int64_t value) {
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
            IrOpcode cmpLt = (argKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtI32;
            IrOpcode negOp = (argKind == LocalInfo::ValueKind::Int64) ? IrOpcode::NegI64 : IrOpcode::NegI32;
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

          IrOpcode cmpLt = (argKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtI32;
          IrOpcode cmpGt = (argKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpGtI64 : IrOpcode::CmpGtI32;
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
        std::string saturateName;
        if (getBuiltinSaturateName(expr, saturateName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = saturateName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::Bool ||
              argKind == LocalInfo::ValueKind::String) {
            error = saturateName + " requires numeric argument";
            return false;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

          auto pushConst = [&](int64_t value) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              function.instructions.push_back(
                  {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
            } else {
              function.instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
            }
          };

          if (argKind == LocalInfo::ValueKind::UInt64) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushConst(1);
            function.instructions.push_back({IrOpcode::CmpGtU64, 0});
            size_t useValue = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushConst(1);
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

          IrOpcode cmpLt = (argKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtI32;
          IrOpcode cmpGt = (argKind == LocalInfo::ValueKind::Int64) ? IrOpcode::CmpGtI64 : IrOpcode::CmpGtI32;
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushConst(0);
          function.instructions.push_back({cmpLt, 0});
          size_t checkMax = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushConst(0);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t checkMaxIndex = function.instructions.size();
          function.instructions[checkMax].imm = static_cast<int32_t>(checkMaxIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          pushConst(1);
          function.instructions.push_back({cmpGt, 0});
          size_t useValue = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          pushConst(1);
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t jumpToEnd2 = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t useValueIndex = function.instructions.size();
          function.instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
          size_t endIndex = function.instructions.size();
          function.instructions[jumpToEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpToEnd2].imm = static_cast<int32_t>(endIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          return true;
        }
        if (getBuiltinConvertName(expr)) {
          if (expr.templateArgs.size() != 1) {
            error = "convert requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "convert requires exactly one argument";
            return false;
          }
          const std::string &typeName = expr.templateArgs.front();
          if (typeName != "int" && typeName != "i32" && typeName != "i64" && typeName != "u64" && typeName != "bool") {
            error = "native backend only supports convert<int>, convert<i32>, convert<i64>, convert<u64>, or "
                    "convert<bool>";
            return false;
          }
          auto tryEmitMathConstantConvert = [&](const Expr &arg) -> bool {
            if (arg.kind != Expr::Kind::Name) {
              return false;
            }
            std::string mathConst;
            if (!getMathConstantName(arg.name, mathConst)) {
              return false;
            }
            double value = 0.0;
            if (mathConst == "pi") {
              value = 3.14159265358979323846;
            } else if (mathConst == "tau") {
              value = 6.28318530717958647692;
            } else if (mathConst == "e") {
              value = 2.71828182845904523536;
            }
            if (typeName == "bool") {
              function.instructions.push_back({IrOpcode::PushI32, value != 0.0 ? 1u : 0u});
              return true;
            }
            if (typeName == "int" || typeName == "i32") {
              function.instructions.push_back(
                  {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
              return true;
            }
            if (typeName == "i64") {
              function.instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
              return true;
            }
            function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
            return true;
          };
          if (tryEmitMathConstantConvert(expr.args.front())) {
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          if (typeName == "bool") {
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (!emitCompareToZero(kind, false)) {
              return false;
            }
          }
          return true;
        }
        if (getBuiltinPointerName(expr, builtin)) {
          if (expr.args.size() != 1) {
            error = builtin + " requires exactly one argument";
            return false;
          }
          if (builtin == "location") {
            const Expr &target = expr.args.front();
            if (target.kind != Expr::Kind::Name) {
              error = "location requires a local binding";
              return false;
            }
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "location requires a local binding";
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(it->second.index)});
            }
            return true;
          }
          const Expr &pointerExpr = expr.args.front();
          if (pointerExpr.kind == Expr::Kind::Name) {
            auto it = localsIn.find(pointerExpr.name);
            if (it == localsIn.end()) {
              error = "native backend does not know identifier: " + pointerExpr.name;
              return false;
            }
            if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
              error = "dereference requires a pointer or reference";
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          } else {
            if (!emitExpr(pointerExpr, localsIn)) {
              return false;
            }
          }
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (getBuiltinCollectionName(expr, builtin)) {
          if (builtin == "array" || builtin == "vector") {
            if (expr.templateArgs.size() != 1) {
              error = builtin + " literal requires exactly one template argument";
              return false;
            }
            LocalInfo::ValueKind elemKind = valueKindFromTypeName(expr.templateArgs.front());
            if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool " + builtin + " literals";
              return false;
            }
            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = builtin + " literal too large for native backend";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += static_cast<int32_t>(1 + expr.args.size());

            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(expr.args.size()))});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires " + builtin + " literal elements to be numeric/bool values";
                return false;
              }
              if (argKind != elemKind) {
                error = builtin + " literal element type mismatch";
                return false;
              }
              if (!emitExpr(arg, localsIn)) {
                return false;
              }
              function.instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1 + static_cast<int32_t>(i))});
            }

            function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          if (builtin == "map") {
            if (expr.templateArgs.size() != 2) {
              error = "map literal requires exactly two template arguments";
              return false;
            }
            if (expr.args.size() % 2 != 0) {
              error = "map literal requires an even number of arguments";
              return false;
            }

            LocalInfo::ValueKind keyKind = valueKindFromTypeName(expr.templateArgs[0]);
            LocalInfo::ValueKind valueKind = valueKindFromTypeName(expr.templateArgs[1]);
            if (keyKind == LocalInfo::ValueKind::Unknown || keyKind == LocalInfo::ValueKind::String ||
                valueKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool map literals";
              return false;
            }

            if (expr.args.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = "map literal too large for native backend";
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += static_cast<int32_t>(1 + expr.args.size());

            const int32_t pairCount = static_cast<int32_t>(expr.args.size() / 2);
            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(pairCount)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});

            for (size_t i = 0; i < expr.args.size(); ++i) {
              const Expr &arg = expr.args[i];
              LocalInfo::ValueKind argKind = inferExprKind(arg, localsIn);
              if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
                error = "native backend requires map literal arguments to be numeric/bool values";
                return false;
              }
              LocalInfo::ValueKind expectedKind = (i % 2 == 0) ? keyKind : valueKind;
              if (argKind != expectedKind) {
                error = (i % 2 == 0) ? "map literal key type mismatch" : "map literal value type mismatch";
                return false;
              }
              if (!emitExpr(arg, localsIn)) {
                return false;
              }
              function.instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + 1 + static_cast<int32_t>(i))});
            }

            function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            return true;
          }
          error = "native backend does not support " + builtin + " literals";
          return false;
        }
        if (isSimpleCallName(expr, "increment") || isSimpleCallName(expr, "decrement")) {
          const bool isIncrement = isSimpleCallName(expr, "increment");
          const char *mutateName = isIncrement ? "increment" : "decrement";
          if (expr.args.size() != 1) {
            error = std::string(mutateName) + " requires exactly one argument";
            return false;
          }
          const Expr &target = expr.args.front();
          auto emitDelta = [&](LocalInfo::ValueKind kind) -> bool {
            if (kind == LocalInfo::ValueKind::Int32) {
              function.instructions.push_back({IrOpcode::PushI32, 1});
              function.instructions.push_back({isIncrement ? IrOpcode::AddI32 : IrOpcode::SubI32, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::PushI64, 1});
              function.instructions.push_back({isIncrement ? IrOpcode::AddI64 : IrOpcode::SubI64, 0});
              return true;
            }
            error = std::string(mutateName) + " requires numeric operand";
            return false;
          };
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = std::string(mutateName) + " target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = std::string(mutateName) + " target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              const int32_t ptrLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              if (!emitDelta(it->second.valueKind)) {
                return false;
              }
              const int32_t valueLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              function.instructions.push_back({IrOpcode::StoreIndirect, 0});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              return true;
            }
            if (it->second.kind != LocalInfo::Kind::Value) {
              error = std::string(mutateName) + " target must be a mutable binding";
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            if (!emitDelta(it->second.valueKind)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::Dup, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            LocalInfo::ValueKind valueKind = inferExprKind(target, localsIn);
            if (valueKind == LocalInfo::ValueKind::Unknown || valueKind == LocalInfo::ValueKind::Bool ||
                valueKind == LocalInfo::ValueKind::String) {
              error = std::string(mutateName) + " requires numeric operand";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            const int32_t ptrLocal = allocTempLocal();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = std::string(mutateName) + " target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = std::string(mutateName) + " target must be a mutable pointer binding";
                return false;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            if (!emitDelta(valueKind)) {
              return false;
            }
            const int32_t valueLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::StoreIndirect, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            return true;
          }
          error = std::string(mutateName) + " target must be a mutable binding";
          return false;
        }
        if (isSimpleCallName(expr, "assign")) {
          if (expr.args.size() != 2) {
            error = "assign requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it == localsIn.end()) {
              error = "assign target must be a known binding: " + target.name;
              return false;
            }
            if (!it->second.isMutable) {
              error = "assign target must be mutable: " + target.name;
              return false;
            }
            if (it->second.kind == LocalInfo::Kind::Reference) {
              const int32_t ptrLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              const int32_t valueLocal = allocTempLocal();
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              function.instructions.push_back({IrOpcode::StoreIndirect, 0});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              return true;
            }
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::Dup, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
            return true;
          }
          if (target.kind == Expr::Kind::Call && isSimpleCallName(target, "dereference")) {
            if (target.args.size() != 1) {
              error = "dereference requires exactly one argument";
              return false;
            }
            const Expr &pointerExpr = target.args.front();
            const int32_t ptrLocal = allocTempLocal();
            if (pointerExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(pointerExpr.name);
              if (it == localsIn.end() || !it->second.isMutable) {
                error = "assign target must be a mutable binding";
                return false;
              }
              if (it->second.kind != LocalInfo::Kind::Pointer && it->second.kind != LocalInfo::Kind::Reference) {
                error = "assign target must be a mutable pointer binding";
                return false;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
            } else {
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::StoreIndirect, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            return true;
          }
          error = "native backend only supports assign to local names or dereference";
          return false;
        }
        if (isBlockCall(expr) && expr.hasBodyArguments) {
          if (!expr.args.empty() || !expr.templateArgs.empty() || hasNamedArguments(expr.argNames)) {
            error = "block expression does not accept arguments";
            return false;
          }
          if (resolveDefinitionCall(expr) != nullptr) {
            error = "block arguments require a definition target: " + resolveExprPath(expr);
            return false;
          }
          if (expr.bodyArguments.empty()) {
            error = "block expression requires a value";
            return false;
          }
          LocalMap blockLocals = localsIn;
          for (size_t i = 0; i < expr.bodyArguments.size(); ++i) {
            const Expr &bodyStmt = expr.bodyArguments[i];
            const bool isLast = (i + 1 == expr.bodyArguments.size());
            if (isLast) {
              if (bodyStmt.isBinding) {
                error = "block expression must end with an expression";
                return false;
              }
              return emitExpr(bodyStmt, blockLocals);
            }
            if (!emitStatement(bodyStmt, blockLocals)) {
              return false;
            }
          }
          error = "block expression requires a value";
          return false;
        }
        if (isIfCall(expr)) {
          if (expr.args.size() != 3) {
            error = "if requires condition, then, else";
            return false;
          }
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error = "if does not accept trailing block arguments";
            return false;
          }
          const Expr &cond = expr.args[0];
          const Expr &thenArg = expr.args[1];
          const Expr &elseArg = expr.args[2];
          auto isIfBlockEnvelope = [&](const Expr &candidate) -> bool {
            if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
              return false;
            }
            if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
              return false;
            }
            if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
              return false;
            }
            return resolveDefinitionCall(candidate) == nullptr;
          };
          auto inferBranchValueKind = [&](const Expr &candidate, const LocalMap &localsBase) -> LocalInfo::ValueKind {
            if (!isIfBlockEnvelope(candidate)) {
              return inferExprKind(candidate, localsBase);
            }
            LocalMap branchLocals = localsBase;
            bool sawValue = false;
            LocalInfo::ValueKind lastKind = LocalInfo::ValueKind::Unknown;
            for (const auto &bodyExpr : candidate.bodyArguments) {
              if (bodyExpr.isBinding) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                LocalInfo info;
                info.index = 0;
                info.isMutable = isBindingMutable(bodyExpr);
                info.kind = bindingKind(bodyExpr);
