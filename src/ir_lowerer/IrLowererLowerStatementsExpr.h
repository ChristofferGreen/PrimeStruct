                LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
                if (hasExplicitBindingTypeTransform(bodyExpr)) {
                  valueKind = bindingValueKind(bodyExpr, info.kind);
                } else if (bodyExpr.args.size() == 1 && info.kind == LocalInfo::Kind::Value) {
                  valueKind = inferExprKind(bodyExpr.args.front(), branchLocals);
                  if (valueKind == LocalInfo::ValueKind::Unknown) {
                    valueKind = LocalInfo::ValueKind::Int32;
                  }
                }
                info.valueKind = valueKind;
                branchLocals.emplace(bodyExpr.name, info);
                continue;
              }
              if (isReturnCall(bodyExpr)) {
                if (bodyExpr.args.size() != 1) {
                  return LocalInfo::ValueKind::Unknown;
                }
                return inferExprKind(bodyExpr.args.front(), branchLocals);
              }
              sawValue = true;
              lastKind = inferExprKind(bodyExpr, branchLocals);
            }
            return sawValue ? lastKind : LocalInfo::ValueKind::Unknown;
          };
          auto emitBranchValue = [&](const Expr &candidate, const LocalMap &localsBase) -> bool {
            if (!isIfBlockEnvelope(candidate)) {
              return emitExpr(candidate, localsBase);
            }
            if (candidate.bodyArguments.empty()) {
              error = "if branch blocks must produce a value";
              return false;
            }
            LocalMap branchLocals = localsBase;
            for (size_t i = 0; i < candidate.bodyArguments.size(); ++i) {
              const Expr &bodyStmt = candidate.bodyArguments[i];
              const bool isLast = (i + 1 == candidate.bodyArguments.size());
              if (bodyStmt.isBinding) {
                if (isLast) {
                  error = "if branch blocks must end with an expression";
                  return false;
                }
                if (!emitStatement(bodyStmt, branchLocals)) {
                  return false;
                }
                continue;
              }
              if (isReturnCall(bodyStmt)) {
                if (bodyStmt.args.size() != 1) {
                  error = "return requires a value in if branch";
                  return false;
                }
                return emitExpr(bodyStmt.args.front(), branchLocals);
              }
              if (isLast) {
                return emitExpr(bodyStmt, branchLocals);
              }
              if (!emitStatement(bodyStmt, branchLocals)) {
                return false;
              }
            }
            error = "if branch blocks must produce a value";
            return false;
          };

          if (!emitExpr(cond, localsIn)) {
            return false;
          }
          LocalInfo::ValueKind condKind = inferExprKind(cond, localsIn);
          if (condKind != LocalInfo::ValueKind::Bool) {
            error = "if condition requires bool";
            return false;
          }

          LocalInfo::ValueKind thenKind = inferBranchValueKind(thenArg, localsIn);
          LocalInfo::ValueKind elseKind = inferBranchValueKind(elseArg, localsIn);
          LocalInfo::ValueKind resultKind = LocalInfo::ValueKind::Unknown;
          if (thenKind == elseKind) {
            resultKind = thenKind;
          } else {
            resultKind = combineNumericKinds(thenKind, elseKind);
          }
          if (resultKind == LocalInfo::ValueKind::Unknown) {
            error = "if branches must return compatible types";
            return false;
          }

          size_t jumpIfZeroIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          if (!emitBranchValue(thenArg, localsIn)) {
            return false;
          }
          size_t jumpEndIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t elseIndex = function.instructions.size();
          function.instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
          if (!emitBranchValue(elseArg, localsIn)) {
            return false;
          }
          size_t endIndex = function.instructions.size();
          function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
          return true;
        }
        error =
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions";
        return false;
      }
      default:
        error = "native backend only supports literals, names, and calls";
        return false;
    }
  };

  auto emitPrintArg = [&](const Expr &arg, const LocalMap &localsIn, const PrintBuiltin &builtin) -> bool {
    uint64_t flags = encodePrintFlags(builtin.newline, builtin.target == PrintTarget::Err);
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName)) {
        if (arg.args.size() != 2) {
          error = accessName + " requires exactly two arguments";
          return false;
        }
        if (isEntryArgsName(arg.args.front(), localsIn)) {
          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(arg.args[1], localsIn));
          if (!isSupportedIndexKind(indexKind)) {
            error = "native backend requires integer indices for " + accessName;
            return false;
          }

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(arg.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            if (indexKind != LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({pushZeroForIndex(indexKind), 0});
              function.instructions.push_back({cmpLtForIndex(indexKind), 0});
              size_t jumpNonNegative = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              size_t nonNegativeIndex = function.instructions.size();
              function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            function.instructions.push_back({cmpGeForIndex(indexKind), 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          IrOpcode printOp = (accessName == "at_unsafe") ? IrOpcode::PrintArgvUnsafe : IrOpcode::PrintArgv;
          function.instructions.push_back({printOp, flags});
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!parseStringLiteral(arg.stringValue, decoded)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back(
          {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(index), flags)});
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      if (it->second.valueKind == LocalInfo::ValueKind::String) {
        if (it->second.stringSource == LocalInfo::StringSource::TableIndex) {
          if (it->second.stringIndex < 0) {
            error = "native backend missing string data for: " + arg.name;
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(it->second.stringIndex), flags)});
          return true;
        }
        if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          IrOpcode printOp = it->second.argvChecked ? IrOpcode::PrintArgv : IrOpcode::PrintArgvUnsafe;
          function.instructions.push_back({printOp, flags});
          return true;
        }
      }
    }
    if (!emitExpr(arg, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
    if (kind == LocalInfo::ValueKind::Int64) {
      function.instructions.push_back({IrOpcode::PrintI64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PrintU64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PrintI32, flags});
      return true;
    }
    error = builtin.name + " requires an integer/bool or string literal/binding argument";
    return false;
  };
