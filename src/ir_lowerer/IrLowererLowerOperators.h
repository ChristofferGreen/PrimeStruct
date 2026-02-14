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
            IrOpcode negOp = IrOpcode::NegI32;
            if (kind == LocalInfo::ValueKind::Int64) {
              negOp = IrOpcode::NegI64;
            } else if (kind == LocalInfo::ValueKind::Float64) {
              negOp = IrOpcode::NegF64;
            } else if (kind == LocalInfo::ValueKind::Float32) {
              negOp = IrOpcode::NegF32;
            }
            function.instructions.push_back({negOp, 0});
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
            } else if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::AddF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::AddF32;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::AddI64;
            } else {
              op = IrOpcode::AddI32;
            }
          } else if (builtin == "minus") {
            if (leftPointer || rightPointer) {
              op = IrOpcode::SubI64;
            } else if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::SubF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::SubF32;
            } else if (numericKind == LocalInfo::ValueKind::Int64 ||
                       numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::SubI64;
            } else {
              op = IrOpcode::SubI32;
            }
          } else if (builtin == "multiply") {
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::MulF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::MulF32;
            } else {
              op = (numericKind == LocalInfo::ValueKind::Int64 || numericKind == LocalInfo::ValueKind::UInt64)
                       ? IrOpcode::MulI64
                       : IrOpcode::MulI32;
            }
          } else if (builtin == "divide") {
            if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::DivU64;
            } else if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::DivF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::DivF32;
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
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::CmpEqF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::CmpEqF32;
            } else {
              op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                       ? IrOpcode::CmpEqI64
                       : IrOpcode::CmpEqI32;
            }
          } else if (builtin == "not_equal") {
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::CmpNeF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::CmpNeF32;
            } else {
              op = (numericKind == LocalInfo::ValueKind::UInt64 || numericKind == LocalInfo::ValueKind::Int64)
                       ? IrOpcode::CmpNeI64
                       : IrOpcode::CmpNeI32;
            }
          } else if (builtin == "less_than") {
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::CmpLtF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::CmpLtF32;
            } else if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLtI64;
            } else {
              op = IrOpcode::CmpLtI32;
            }
          } else if (builtin == "less_equal") {
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::CmpLeF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::CmpLeF32;
            } else if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpLeU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpLeI64;
            } else {
              op = IrOpcode::CmpLeI32;
            }
          } else if (builtin == "greater_than") {
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::CmpGtF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::CmpGtF32;
            } else if (numericKind == LocalInfo::ValueKind::UInt64) {
              op = IrOpcode::CmpGtU64;
            } else if (numericKind == LocalInfo::ValueKind::Int64) {
              op = IrOpcode::CmpGtI64;
            } else {
              op = IrOpcode::CmpGtI32;
            }
          } else if (builtin == "greater_equal") {
            if (numericKind == LocalInfo::ValueKind::Float64) {
              op = IrOpcode::CmpGeF64;
            } else if (numericKind == LocalInfo::ValueKind::Float32) {
              op = IrOpcode::CmpGeF32;
            } else if (numericKind == LocalInfo::ValueKind::UInt64) {
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
          if (powKind == LocalInfo::ValueKind::Float32 || powKind == LocalInfo::ValueKind::Float64) {
            error = powName + " requires integer arguments in the native backend";
            return false;
          }
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
          auto pushConst = [&](double value) {
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
        std::string predicateName;
        if (getBuiltinMathPredicateName(expr, predicateName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = predicateName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = predicateName + " requires float argument";
            return false;
          }
          int32_t tempValue = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValue)});

          auto pushFloatZero = [&]() {
            if (argKind == LocalInfo::ValueKind::Float64) {
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

          IrOpcode cmpEq = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
          IrOpcode cmpNe = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpNeF64 : IrOpcode::CmpNeF32;
          IrOpcode subOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::SubF64 : IrOpcode::SubF32;

          if (predicateName == "is_nan") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({cmpNe, 0});
            return true;
          }

          if (predicateName == "is_finite") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({subOp, 0});
            pushFloatZero();
            function.instructions.push_back({cmpEq, 0});
            return true;
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({cmpEq, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({subOp, 0});
          pushFloatZero();
          function.instructions.push_back({cmpNe, 0});
          function.instructions.push_back({IrOpcode::MulI32, 0});
          return true;
        }
        std::string roundingName;
        if (getBuiltinRoundingName(expr, roundingName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = roundingName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::Bool ||
              argKind == LocalInfo::ValueKind::String) {
            error = roundingName + " requires numeric argument";
            return false;
          }
          int32_t tempValue = allocTempLocal();
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
          auto emitFloatTrunc = [&](int32_t valueLocal, int32_t outLocal) {
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
          auto emitFloorOrCeil = [&](int32_t valueLocal, int32_t outLocal, bool isCeil) {
            emitFloatTrunc(valueLocal, outLocal);
            IrOpcode cmpOp = isCeil ? (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::CmpLtF64
                                                                               : IrOpcode::CmpLtF32)
                                   : (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::CmpGtF64
                                                                               : IrOpcode::CmpGtF32);
            IrOpcode addSubOp = isCeil ? (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::AddF64
                                                                                   : IrOpcode::AddF32)
                                       : (argKind == LocalInfo::ValueKind::Float64 ? IrOpcode::SubF64
                                                                                   : IrOpcode::SubF32);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({cmpOp, 0});
            size_t useValue = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(outLocal)});
            pushFloatConst(1.0);
            function.instructions.push_back({addSubOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(outLocal)});
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t useValueIndex = function.instructions.size();
            function.instructions[useValue].imm = static_cast<int32_t>(useValueIndex);
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          };

          if (argKind == LocalInfo::ValueKind::Int32 || argKind == LocalInfo::ValueKind::Int64 ||
              argKind == LocalInfo::ValueKind::UInt64) {
            if (roundingName == "fract") {
              if (argKind == LocalInfo::ValueKind::Int32) {
                function.instructions.push_back({IrOpcode::PushI32, 0});
              } else {
                function.instructions.push_back({IrOpcode::PushI64, 0});
              }
              return true;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            return true;
          }

          const bool isF32 = (argKind == LocalInfo::ValueKind::Float32);
          const IrOpcode addOp = isF32 ? IrOpcode::AddF32 : IrOpcode::AddF64;
          const IrOpcode subOp = isF32 ? IrOpcode::SubF32 : IrOpcode::SubF64;
          const IrOpcode cmpLtOp = isF32 ? IrOpcode::CmpLtF32 : IrOpcode::CmpLtF64;

          if (roundingName == "trunc") {
            int32_t tempOut = allocTempLocal();
            emitFloatTrunc(tempValue, tempOut);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }

          int32_t tempOut = allocTempLocal();
          if (roundingName == "floor") {
            emitFloorOrCeil(tempValue, tempOut, false);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }
          if (roundingName == "ceil") {
            emitFloorOrCeil(tempValue, tempOut, true);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }
          if (roundingName == "fract") {
            emitFloorOrCeil(tempValue, tempOut, false);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            function.instructions.push_back({subOp, 0});
            return true;
          }
          if (roundingName == "round") {
            int32_t tempAdjusted = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpLtOp, 0});
            size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.5);
            function.instructions.push_back({subOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAdjusted)});
            emitFloorOrCeil(tempAdjusted, tempOut, true);
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.5);
            function.instructions.push_back({addOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempAdjusted)});
            emitFloorOrCeil(tempAdjusted, tempOut, false);

            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }
          error = roundingName + " requires numeric argument";
          return false;
        }
        std::string rootName;
        if (getBuiltinRootName(expr, rootName, hasMathImport)) {
          if (expr.args.size() != 1) {
            error = rootName + " requires exactly one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind != LocalInfo::ValueKind::Float32 && argKind != LocalInfo::ValueKind::Float64) {
            error = rootName + " requires float argument";
            return false;
          }
          int32_t tempValue = allocTempLocal();
          int32_t tempOut = allocTempLocal();
          int32_t tempIter = allocTempLocal();
          int32_t tempX = allocTempLocal();

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

          IrOpcode addOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::AddF64 : IrOpcode::AddF32;
          IrOpcode mulOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::MulF64 : IrOpcode::MulF32;
          IrOpcode divOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::DivF64 : IrOpcode::DivF32;
          IrOpcode cmpEqOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpEqF64 : IrOpcode::CmpEqF32;
          IrOpcode cmpLtOp = (argKind == LocalInfo::ValueKind::Float64) ? IrOpcode::CmpLtF64 : IrOpcode::CmpLtF32;

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

          if (rootName == "sqrt") {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
            pushFloatConst(0.0);
            function.instructions.push_back({cmpLtOp, 0});
            size_t jumpIfNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            pushFloatConst(0.0);
            pushFloatConst(0.0);
            function.instructions.push_back({divOp, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});
            size_t jumpToEndNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpIfNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
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
            function.instructions[jumpToEndNegative].imm = static_cast<int32_t>(endIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
            return true;
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::PushI32, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempIter)});

          const int32_t iterations = (argKind == LocalInfo::ValueKind::Float64) ? 10 : 8;
          const size_t loopStart = function.instructions.size();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempIter)});
          function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(iterations)});
          function.instructions.push_back({IrOpcode::CmpLtI32, 0});
          size_t jumpEndLoop = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempOut)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempX)});
          pushFloatConst(2.0);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValue)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempOut)});
          function.instructions.push_back({divOp, 0});
          function.instructions.push_back({addOp, 0});
          pushFloatConst(3.0);
          function.instructions.push_back({divOp, 0});
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
          LocalInfo::ValueKind targetKind = valueKindFromTypeName(typeName);
          if (targetKind == LocalInfo::ValueKind::Unknown || targetKind == LocalInfo::ValueKind::String) {
            error = "native backend only supports convert<int>, convert<i32>, convert<i64>, convert<u64>, "
                    "convert<bool>, convert<f32>, or convert<f64>";
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
            if (targetKind == LocalInfo::ValueKind::Bool) {
              function.instructions.push_back({IrOpcode::PushI32, value != 0.0 ? 1u : 0u});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Int32) {
              function.instructions.push_back(
                  {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(value))});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Int64) {
              function.instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(value))});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
              return true;
            }
            if (targetKind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              std::memcpy(&bits, &value, sizeof(bits));
              function.instructions.push_back({IrOpcode::PushF64, bits});
              return true;
            }
            float f32 = static_cast<float>(value);
            uint32_t bits = 0;
            std::memcpy(&bits, &f32, sizeof(bits));
            function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
            return true;
          };
          if (tryEmitMathConstantConvert(expr.args.front())) {
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          if (targetKind == LocalInfo::ValueKind::Bool) {
            LocalInfo::ValueKind kind = inferExprKind(expr.args.front(), localsIn);
            if (!emitCompareToZero(kind, false)) {
              return false;
            }
            return true;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args.front(), localsIn);
          if (argKind == LocalInfo::ValueKind::Bool) {
            argKind = LocalInfo::ValueKind::Int32;
          }
          if (argKind == LocalInfo::ValueKind::Unknown || argKind == LocalInfo::ValueKind::String) {
            error = "convert requires numeric argument";
            return false;
          }
          if (argKind == targetKind) {
            return true;
          }
          IrOpcode convertOp = IrOpcode::ConvertI32ToF32;
          bool needsConvert = true;
          if (targetKind == LocalInfo::ValueKind::Float32) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              convertOp = IrOpcode::ConvertI32ToF32;
            } else if (argKind == LocalInfo::ValueKind::Int64) {
              convertOp = IrOpcode::ConvertI64ToF32;
            } else if (argKind == LocalInfo::ValueKind::UInt64) {
              convertOp = IrOpcode::ConvertU64ToF32;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToF32;
            } else {
              error = "convert requires numeric argument";
              return false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Float64) {
            if (argKind == LocalInfo::ValueKind::Int32) {
              convertOp = IrOpcode::ConvertI32ToF64;
            } else if (argKind == LocalInfo::ValueKind::Int64) {
              convertOp = IrOpcode::ConvertI64ToF64;
            } else if (argKind == LocalInfo::ValueKind::UInt64) {
              convertOp = IrOpcode::ConvertU64ToF64;
            } else if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToF64;
            } else {
              error = "convert requires numeric argument";
              return false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Int32) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToI32;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToI32;
            } else {
              needsConvert = false;
            }
          } else if (targetKind == LocalInfo::ValueKind::Int64) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToI64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToI64;
            } else {
              needsConvert = false;
            }
          } else if (targetKind == LocalInfo::ValueKind::UInt64) {
            if (argKind == LocalInfo::ValueKind::Float32) {
              convertOp = IrOpcode::ConvertF32ToU64;
            } else if (argKind == LocalInfo::ValueKind::Float64) {
              convertOp = IrOpcode::ConvertF64ToU64;
            } else {
              needsConvert = false;
            }
          }
          if (needsConvert) {
            function.instructions.push_back({convertOp, 0});
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
            if (kind == LocalInfo::ValueKind::Float64) {
              uint64_t bits = 0;
              double one = 1.0;
              std::memcpy(&bits, &one, sizeof(bits));
              function.instructions.push_back({IrOpcode::PushF64, bits});
              function.instructions.push_back({isIncrement ? IrOpcode::AddF64 : IrOpcode::SubF64, 0});
              return true;
            }
            if (kind == LocalInfo::ValueKind::Float32) {
              float one = 1.0f;
              uint32_t bits = 0;
              std::memcpy(&bits, &one, sizeof(bits));
              function.instructions.push_back({IrOpcode::PushF32, static_cast<uint64_t>(bits)});
              function.instructions.push_back({isIncrement ? IrOpcode::AddF32 : IrOpcode::SubF32, 0});
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
