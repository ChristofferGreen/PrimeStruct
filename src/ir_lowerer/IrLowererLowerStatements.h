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

  emitStatement = [&](const Expr &stmt, LocalMap &localsIn) -> bool {
    if (stmt.isBinding) {
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (localsIn.count(stmt.name) > 0) {
        error = "binding redefines existing name: " + stmt.name;
        return false;
      }
      const Expr &init = stmt.args.front();
      LocalInfo::Kind kind = bindingKind(stmt);
      if (!hasExplicitBindingTypeTransform(stmt) && kind == LocalInfo::Kind::Value) {
        if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it != localsIn.end()) {
            kind = it->second.kind;
          }
        } else if (init.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(init, collection)) {
            if (collection == "array") {
              kind = LocalInfo::Kind::Array;
            } else if (collection == "vector") {
              kind = LocalInfo::Kind::Vector;
            } else if (collection == "map") {
              kind = LocalInfo::Kind::Map;
            }
          }
        }
      }
      LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
      if (kind == LocalInfo::Kind::Map) {
        if (hasExplicitBindingTypeTransform(stmt)) {
          for (const auto &transform : stmt.transforms) {
            if (transform.name == "map" && transform.templateArgs.size() == 2) {
              mapKeyKind = valueKindFromTypeName(transform.templateArgs[0]);
              mapValueKind = valueKindFromTypeName(transform.templateArgs[1]);
              break;
            }
          }
        } else if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
            mapKeyKind = it->second.mapKeyKind;
            mapValueKind = it->second.mapValueKind;
          }
        } else if (init.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(init, collection) && collection == "map" && init.templateArgs.size() == 2) {
            mapKeyKind = valueKindFromTypeName(init.templateArgs[0]);
            mapValueKind = valueKindFromTypeName(init.templateArgs[1]);
          }
        }
        valueKind = mapValueKind;
      } else if (hasExplicitBindingTypeTransform(stmt)) {
        valueKind = bindingValueKind(stmt, kind);
      } else if (kind == LocalInfo::Kind::Value) {
        valueKind = inferExprKind(init, localsIn);
        if (valueKind == LocalInfo::ValueKind::Unknown) {
          valueKind = LocalInfo::ValueKind::Int32;
        }
      } else if (kind == LocalInfo::Kind::Array || kind == LocalInfo::Kind::Vector) {
        if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it != localsIn.end() &&
              (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
            valueKind = it->second.valueKind;
          }
        } else if (init.kind == Expr::Kind::Call) {
          std::string collection;
          if (getBuiltinCollectionName(init, collection) && (collection == "array" || collection == "vector") &&
              init.templateArgs.size() == 1) {
            valueKind = valueKindFromTypeName(init.templateArgs.front());
          }
        }
      }
      if (valueKind == LocalInfo::ValueKind::String) {
        if (kind != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          return false;
        }
        int32_t index = -1;
        LocalInfo::StringSource source = LocalInfo::StringSource::None;
        bool argvChecked = true;
        if (init.kind == Expr::Kind::StringLiteral) {
          std::string decoded;
          if (!parseStringLiteral(init.stringValue, decoded)) {
            return false;
          }
          index = internString(decoded);
          source = LocalInfo::StringSource::TableIndex;
        } else if (init.kind == Expr::Kind::Name) {
          auto it = localsIn.find(init.name);
          if (it == localsIn.end()) {
            error = "native backend does not know identifier: " + init.name;
            return false;
          }
          if (it->second.valueKind != LocalInfo::ValueKind::String ||
              it->second.stringSource == LocalInfo::StringSource::None) {
            error = "native backend requires string bindings to use string literals, bindings, or entry args";
            return false;
          }
          source = it->second.stringSource;
          index = it->second.stringIndex;
          if (source == LocalInfo::StringSource::ArgvIndex) {
            argvChecked = it->second.argvChecked;
          }
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        } else if (init.kind == Expr::Kind::Call) {
          std::string accessName;
          if (!getBuiltinArrayAccessName(init, accessName)) {
            error = "native backend requires string bindings to use string literals, bindings, or entry args";
            return false;
          }
          if (init.args.size() != 2) {
            error = accessName + " requires exactly two arguments";
            return false;
          }
          if (!isEntryArgsName(init.args.front(), localsIn)) {
            error = "native backend only supports entry argument indexing";
            return false;
          }
          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(init.args[1], localsIn));
          if (!isSupportedIndexKind(indexKind)) {
            error = "native backend requires integer indices for " + accessName;
            return false;
          }

          const int32_t argvIndexLocal = allocTempLocal();
          if (!emitExpr(init.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(argvIndexLocal)});

          if (accessName == "at") {
            if (indexKind != LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
              function.instructions.push_back({pushZeroForIndex(indexKind), 0});
              function.instructions.push_back({cmpLtForIndex(indexKind), 0});
              size_t jumpNonNegative = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              size_t nonNegativeIndex = function.instructions.size();
              function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            function.instructions.push_back({cmpGeForIndex(indexKind), 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(argvIndexLocal)});
          source = LocalInfo::StringSource::ArgvIndex;
          argvChecked = (accessName == "at");
        } else {
          error = "native backend requires string bindings to use string literals, bindings, or entry args";
          return false;
        }
        LocalInfo info;
        info.index = nextLocal++;
        info.isMutable = isBindingMutable(stmt);
        info.kind = LocalInfo::Kind::Value;
        info.valueKind = LocalInfo::ValueKind::String;
        info.stringSource = source;
        info.stringIndex = index;
        info.argvChecked = argvChecked;
        if (init.kind == Expr::Kind::StringLiteral) {
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
        }
        localsIn.emplace(stmt.name, info);
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        return true;
      }
      if (valueKind == LocalInfo::ValueKind::Unknown && kind != LocalInfo::Kind::Map) {
        error = "native backend requires typed bindings";
        return false;
      }
      if (!emitExpr(init, localsIn)) {
        return false;
      }
      LocalInfo info;
      info.index = nextLocal++;
      info.isMutable = isBindingMutable(stmt);
      info.kind = kind;
      info.valueKind = valueKind;
      info.mapKeyKind = mapKeyKind;
      info.mapValueKind = mapValueKind;
      if (info.kind == LocalInfo::Kind::Reference) {
        if (init.kind != Expr::Kind::Call || !isSimpleCallName(init, "location") || init.args.size() != 1) {
          error = "reference binding requires location(...) initializer";
          return false;
        }
      }
      localsIn.emplace(stmt.name, info);
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
      return true;
    }
    PrintBuiltin printBuiltin;
    if (stmt.kind == Expr::Kind::Call && getPrintBuiltin(stmt, printBuiltin)) {
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = printBuiltin.name + " does not support body arguments";
        return false;
      }
      if (stmt.args.size() != 1) {
        error = printBuiltin.name + " requires exactly one argument";
        return false;
      }
      return emitPrintArg(stmt.args.front(), localsIn, printBuiltin);
    }
    PathSpaceBuiltin pathSpaceBuiltin;
    if (stmt.kind == Expr::Kind::Call && getPathSpaceBuiltin(stmt, pathSpaceBuiltin) && !resolveDefinitionCall(stmt)) {
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = pathSpaceBuiltin.name + " does not support body arguments";
        return false;
      }
      if (!stmt.templateArgs.empty()) {
        error = pathSpaceBuiltin.name + " does not support template arguments";
        return false;
      }
      if (stmt.args.size() != pathSpaceBuiltin.argumentCount) {
        error = pathSpaceBuiltin.name + " requires exactly " + std::to_string(pathSpaceBuiltin.argumentCount) +
                " argument" + (pathSpaceBuiltin.argumentCount == 1 ? "" : "s");
        return false;
      }
      for (const auto &arg : stmt.args) {
        if (arg.kind == Expr::Kind::StringLiteral) {
          continue;
        }
        if (!emitExpr(arg, localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::Pop, 0});
      }
      return true;
    }
    if (isReturnCall(stmt)) {
      if (activeInlineContext) {
        InlineContext &context = *activeInlineContext;
        if (stmt.args.empty()) {
          if (!context.returnsVoid) {
            error = "return requires exactly one argument";
            return false;
          }
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        if (stmt.args.size() != 1) {
          error = "return requires exactly one argument";
          return false;
        }
        if (context.returnsVoid) {
          error = "return value not allowed for void definition";
          return false;
        }
        if (context.returnLocal < 0) {
          error = "native backend missing inline return local";
          return false;
        }
        if (!emitExpr(stmt.args.front(), localsIn)) {
          return false;
        }
        LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
        if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64 ||
            returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool ||
            returnKind == LocalInfo::ValueKind::Float32 || returnKind == LocalInfo::ValueKind::Float64) {
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
        }
        error = "native backend only supports returning numeric or bool values";
        return false;
      }

      if (stmt.args.empty()) {
        if (!returnsVoid) {
          error = "return requires exactly one argument";
          return false;
        }
        function.instructions.push_back({IrOpcode::ReturnVoid, 0});
        sawReturn = true;
        return true;
      }
      if (stmt.args.size() != 1) {
        error = "return requires exactly one argument";
        return false;
      }
      if (returnsVoid) {
        error = "return value not allowed for void definition";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo::ValueKind returnKind = inferExprKind(stmt.args.front(), localsIn);
      if (returnKind == LocalInfo::ValueKind::Int64 || returnKind == LocalInfo::ValueKind::UInt64) {
        function.instructions.push_back({IrOpcode::ReturnI64, 0});
      } else if (returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
        function.instructions.push_back({IrOpcode::ReturnI32, 0});
      } else if (returnKind == LocalInfo::ValueKind::Float32) {
        function.instructions.push_back({IrOpcode::ReturnF32, 0});
      } else if (returnKind == LocalInfo::ValueKind::Float64) {
        function.instructions.push_back({IrOpcode::ReturnF64, 0});
      } else {
        error = "native backend only supports returning numeric or bool values";
        return false;
      }
      sawReturn = true;
      return true;
    }
    if (isIfCall(stmt)) {
      if (stmt.args.size() != 3) {
        error = "if requires condition, then, else";
        return false;
      }
      if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
        error = "if does not accept trailing block arguments";
        return false;
      }
      if (!emitExpr(stmt.args[0], localsIn)) {
        return false;
      }
      LocalInfo::ValueKind condKind = inferExprKind(stmt.args[0], localsIn);
      if (condKind != LocalInfo::ValueKind::Bool) {
        error = "if condition requires bool";
        return false;
      }
      const Expr &thenArg = stmt.args[1];
      const Expr &elseArg = stmt.args[2];
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
        return true;
      };
      if (!isIfBlockEnvelope(thenArg) || !isIfBlockEnvelope(elseArg)) {
        error = "if branches require block envelopes";
        return false;
      }
      auto emitBranch = [&](const Expr &branch, LocalMap &branchLocals) -> bool {
        return emitBlock(branch, branchLocals);
      };
      size_t jumpIfZeroIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});
      LocalMap thenLocals = localsIn;
      if (!emitBranch(thenArg, thenLocals)) {
        return false;
      }
      size_t jumpIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::Jump, 0});
      size_t elseIndex = function.instructions.size();
      function.instructions[jumpIfZeroIndex].imm = static_cast<int32_t>(elseIndex);
      LocalMap elseLocals = localsIn;
      if (!emitBranch(elseArg, elseLocals)) {
        return false;
      }
      size_t endIndex = function.instructions.size();
      function.instructions[jumpIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    auto isLoopBlockEnvelope = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
        return false;
      }
      if (!candidate.args.empty() || !candidate.templateArgs.empty()) {
        return false;
      }
      if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
        return false;
      }
      return true;
    };
    if (isLoopCall(stmt)) {
      if (stmt.args.size() != 2) {
        error = "loop requires count and body";
        return false;
      }
      const Expr &countExpr = stmt.args[0];
      if (!emitExpr(countExpr, localsIn)) {
        return false;
      }
      LocalInfo::ValueKind countKind = inferExprKind(countExpr, localsIn);
      if (countKind != LocalInfo::ValueKind::Int32 && countKind != LocalInfo::ValueKind::Int64 &&
          countKind != LocalInfo::ValueKind::UInt64) {
        error = "loop count requires integer";
        return false;
      }

      const int32_t counterLocal = allocTempLocal();
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(counterLocal)});

      const size_t checkIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::PushI32, 0});
        function.instructions.push_back({IrOpcode::CmpGtI32, 0});
      } else if (countKind == LocalInfo::ValueKind::Int64) {
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpGtI64, 0});
      } else {
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpNeI64, 0});
      }

      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      const Expr &body = stmt.args[1];
      if (!isLoopBlockEnvelope(body)) {
        error = "loop body requires a block envelope";
        return false;
      }
      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : body.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::PushI32, 1});
        function.instructions.push_back({IrOpcode::SubI32, 0});
      } else {
        function.instructions.push_back({IrOpcode::PushI64, 1});
        function.instructions.push_back({IrOpcode::SubI64, 0});
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(counterLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});

      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (isWhileCall(stmt)) {
      if (stmt.args.size() != 2) {
        error = "while requires condition and body";
        return false;
      }
      const Expr &cond = stmt.args[0];
      const Expr &body = stmt.args[1];
      if (!isLoopBlockEnvelope(body)) {
        error = "while body requires a block envelope";
        return false;
      }
      const size_t checkIndex = function.instructions.size();
      if (!emitExpr(cond, localsIn)) {
        return false;
      }
      LocalInfo::ValueKind condKind = inferExprKind(cond, localsIn);
      if (condKind != LocalInfo::ValueKind::Bool) {
        error = "while condition requires bool";
        return false;
      }
      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : body.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }

      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});
      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (isForCall(stmt)) {
      if (stmt.args.size() != 4) {
        error = "for requires init, condition, step, and body";
        return false;
      }
      auto declareLoopBinding = [&](const Expr &binding, LocalMap &locals) -> bool {
        if (binding.args.size() != 1) {
          error = "binding requires exactly one argument";
          return false;
        }
        if (locals.count(binding.name) > 0) {
          error = "binding redefines existing name: " + binding.name;
          return false;
        }
        LocalInfo info;
        info.index = nextLocal++;
        info.isMutable = isBindingMutable(binding);
        info.kind = bindingKind(binding);
        info.valueKind = LocalInfo::ValueKind::Unknown;
        info.mapKeyKind = LocalInfo::ValueKind::Unknown;
        info.mapValueKind = LocalInfo::ValueKind::Unknown;
        if (hasExplicitBindingTypeTransform(binding)) {
          info.valueKind = bindingValueKind(binding, info.kind);
        } else if (info.kind == LocalInfo::Kind::Value) {
          info.valueKind = inferExprKind(binding.args.front(), locals);
          if (info.valueKind == LocalInfo::ValueKind::Unknown) {
            info.valueKind = LocalInfo::ValueKind::Int32;
          }
        }
        locals.emplace(binding.name, info);
        return true;
      };
      auto emitLoopBindingInit = [&](const Expr &binding, LocalMap &locals) -> bool {
        if (binding.args.size() != 1) {
          error = "binding requires exactly one argument";
          return false;
        }
        auto it = locals.find(binding.name);
        if (it == locals.end()) {
          error = "binding missing local: " + binding.name;
          return false;
        }
        if (!emitExpr(binding.args.front(), locals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(it->second.index)});
        return true;
      };
      LocalMap loopLocals = localsIn;
      if (!emitStatement(stmt.args[0], loopLocals)) {
        return false;
      }
      const Expr &cond = stmt.args[1];
      const Expr &step = stmt.args[2];
      const Expr &body = stmt.args[3];
      if (!isLoopBlockEnvelope(body)) {
        error = "for body requires a block envelope";
        return false;
      }
      if (cond.isBinding) {
        if (!declareLoopBinding(cond, loopLocals)) {
          return false;
        }
      }

      const size_t checkIndex = function.instructions.size();
      LocalInfo::ValueKind condKind = LocalInfo::ValueKind::Unknown;
      if (cond.isBinding) {
        if (!emitLoopBindingInit(cond, loopLocals)) {
          return false;
        }
        Expr condName;
        condName.kind = Expr::Kind::Name;
        condName.name = cond.name;
        condName.namespacePrefix = cond.namespacePrefix;
        if (!emitExpr(condName, loopLocals)) {
          return false;
        }
        condKind = inferExprKind(condName, loopLocals);
      } else {
        if (!emitExpr(cond, loopLocals)) {
          return false;
        }
        condKind = inferExprKind(cond, loopLocals);
      }
      if (condKind != LocalInfo::ValueKind::Bool) {
        error = "for condition requires bool";
        return false;
      }
      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      LocalMap bodyLocals = loopLocals;
      for (const auto &bodyStmt : body.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }
      if (!emitStatement(step, loopLocals)) {
        return false;
      }

      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});
      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (isRepeatCall(stmt)) {
      if (stmt.args.size() != 1) {
        error = "repeat requires exactly one argument";
        return false;
      }
      if (!emitExpr(stmt.args.front(), localsIn)) {
        return false;
      }
      LocalInfo::ValueKind countKind = inferExprKind(stmt.args.front(), localsIn);
      if (countKind == LocalInfo::ValueKind::Bool) {
        countKind = LocalInfo::ValueKind::Int32;
      }
      if (countKind != LocalInfo::ValueKind::Int32 && countKind != LocalInfo::ValueKind::Int64 &&
          countKind != LocalInfo::ValueKind::UInt64) {
        error = "repeat count requires integer or bool";
        return false;
      }

      const int32_t counterLocal = allocTempLocal();
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(counterLocal)});

      const size_t checkIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::PushI32, 0});
        function.instructions.push_back({IrOpcode::CmpGtI32, 0});
      } else if (countKind == LocalInfo::ValueKind::Int64) {
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpGtI64, 0});
      } else {
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpNeI64, 0});
      }

      const size_t jumpEndIndex = function.instructions.size();
      function.instructions.push_back({IrOpcode::JumpIfZero, 0});

      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }

      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::PushI32, 1});
        function.instructions.push_back({IrOpcode::SubI32, 0});
      } else {
        function.instructions.push_back({IrOpcode::PushI64, 1});
        function.instructions.push_back({IrOpcode::SubI64, 0});
      }
      function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(counterLocal)});
      function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(checkIndex)});

      const size_t endIndex = function.instructions.size();
      function.instructions[jumpEndIndex].imm = static_cast<int32_t>(endIndex);
      return true;
    }
    if (stmt.kind == Expr::Kind::Call && isBlockCall(stmt) && stmt.hasBodyArguments && resolveDefinitionCall(stmt) == nullptr) {
      if (!stmt.args.empty() || !stmt.templateArgs.empty() || hasNamedArguments(stmt.argNames)) {
        error = "block does not accept arguments";
        return false;
      }
      LocalMap blockLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, blockLocals)) {
          return false;
        }
      }
      return true;
    }
    if (stmt.kind == Expr::Kind::Call) {
      std::string vectorHelper;
      if (isSimpleCallName(stmt, "push")) {
        vectorHelper = "push";
      } else if (isSimpleCallName(stmt, "pop")) {
        vectorHelper = "pop";
      } else if (isSimpleCallName(stmt, "reserve")) {
        vectorHelper = "reserve";
      } else if (isSimpleCallName(stmt, "clear")) {
        vectorHelper = "clear";
      } else if (isSimpleCallName(stmt, "remove_at")) {
        vectorHelper = "remove_at";
      } else if (isSimpleCallName(stmt, "remove_swap")) {
        vectorHelper = "remove_swap";
      }

      if (!vectorHelper.empty()) {
        if (!stmt.templateArgs.empty()) {
          error = vectorHelper + " does not accept template arguments";
          return false;
        }
        if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
          error = vectorHelper + " does not accept block arguments";
          return false;
        }

        const size_t expectedArgs = (vectorHelper == "pop" || vectorHelper == "clear") ? 1 : 2;
        if (stmt.args.size() != expectedArgs) {
          if (expectedArgs == 1) {
            error = vectorHelper + " requires exactly one argument";
          } else {
            error = vectorHelper + " requires exactly two arguments";
          }
          return false;
        }

        const Expr &target = stmt.args.front();
        if (target.kind != Expr::Kind::Name) {
          error = vectorHelper + " requires mutable vector binding";
          return false;
        }
        auto it = localsIn.find(target.name);
        if (it == localsIn.end() || it->second.kind != LocalInfo::Kind::Vector || !it->second.isMutable) {
          error = vectorHelper + " requires mutable vector binding";
          return false;
        }

        auto pushIndexConst = [&](LocalInfo::ValueKind kind, int32_t value) {
          if (kind == LocalInfo::ValueKind::Int32) {
            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(value)});
          } else {
            function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(value)});
          }
        };

        const int32_t ptrLocal = allocTempLocal();
        const int32_t elementOffset = 2;
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

        if (vectorHelper == "clear") {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::PushI32, 0});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});
          return true;
        }

        const int32_t countLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        function.instructions.push_back({IrOpcode::LoadIndirect, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

        int32_t capacityLocal = -1;
        if (vectorHelper == "push" || vectorHelper == "reserve") {
          capacityLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 16});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(capacityLocal)});
        }

        if (vectorHelper == "reserve") {
          LocalInfo::ValueKind capacityKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
          if (!isSupportedIndexKind(capacityKind)) {
            error = "reserve requires integer capacity";
            return false;
          }

          const int32_t desiredLocal = allocTempLocal();
          if (!emitExpr(stmt.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(desiredLocal)});

          if (capacityKind != LocalInfo::ValueKind::UInt64) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
            function.instructions.push_back({pushZeroForIndex(capacityKind), 0});
            function.instructions.push_back({cmpLtForIndex(capacityKind), 0});
            size_t jumpNonNegative = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitVectorReserveNegative();
            size_t nonNegativeIndex = function.instructions.size();
            function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
          }

          IrOpcode cmpLtOp =
              (capacityKind == LocalInfo::ValueKind::Int32)
                  ? IrOpcode::CmpLtI32
                  : (capacityKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtU64);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(desiredLocal)});
          function.instructions.push_back({cmpLtOp, 0});
          size_t jumpWithinCapacity = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitVectorReserveExceeded();
          size_t withinCapacityIndex = function.instructions.size();
          function.instructions[jumpWithinCapacity].imm = static_cast<int32_t>(withinCapacityIndex);
          return true;
        }

        if (vectorHelper == "push") {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(capacityLocal)});
          function.instructions.push_back({IrOpcode::CmpLtI32, 0});
          size_t jumpHasSpace = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t valueLocal = allocTempLocal();
          if (!emitExpr(stmt.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});

          const int32_t destPtrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(elementOffset)});
          function.instructions.push_back({IrOpcode::AddI32, 0});
          function.instructions.push_back({IrOpcode::PushI32, 16});
          function.instructions.push_back({IrOpcode::MulI32, 0});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back({IrOpcode::AddI32, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});

          size_t jumpEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t errorIndex = function.instructions.size();
          emitVectorCapacityExceeded();
          size_t endIndex = function.instructions.size();
          function.instructions[jumpHasSpace].imm = static_cast<int32_t>(errorIndex);
          function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          return true;
        }

        if (vectorHelper == "pop") {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::PushI32, 0});
          function.instructions.push_back({IrOpcode::CmpEqI32, 0});
          size_t jumpNonEmpty = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitVectorPopOnEmpty();
          size_t nonEmptyIndex = function.instructions.size();
          function.instructions[jumpNonEmpty].imm = static_cast<int32_t>(nonEmptyIndex);

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back({IrOpcode::SubI32, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});
          return true;
        }

        LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(stmt.args[1], localsIn));
        if (!isSupportedIndexKind(indexKind)) {
          error = vectorHelper + " requires integer index";
          return false;
        }

        IrOpcode cmpLtOp =
            (indexKind == LocalInfo::ValueKind::Int32)
                ? IrOpcode::CmpLtI32
                : (indexKind == LocalInfo::ValueKind::Int64 ? IrOpcode::CmpLtI64 : IrOpcode::CmpLtU64);
        IrOpcode addOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::AddI32 : IrOpcode::AddI64;
        IrOpcode subOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::SubI32 : IrOpcode::SubI64;
        IrOpcode mulOp = (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::MulI32 : IrOpcode::MulI64;

        const int32_t indexLocal = allocTempLocal();
        if (!emitExpr(stmt.args[1], localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

        if (indexKind != LocalInfo::ValueKind::UInt64) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          function.instructions.push_back({pushZeroForIndex(indexKind), 0});
          function.instructions.push_back({cmpLtForIndex(indexKind), 0});
          size_t jumpNonNegative = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          emitVectorIndexOutOfBounds();
          size_t nonNegativeIndex = function.instructions.size();
          function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
        }

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        function.instructions.push_back({cmpGeForIndex(indexKind), 0});
        size_t jumpInRange = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitVectorIndexOutOfBounds();
        size_t inRangeIndex = function.instructions.size();
        function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);

        const int32_t lastIndexLocal = allocTempLocal();
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        pushIndexConst(indexKind, 1);
        function.instructions.push_back({subOp, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(lastIndexLocal)});

        if (vectorHelper == "remove_swap") {
          const int32_t destPtrLocal = allocTempLocal();
          const int32_t srcPtrLocal = allocTempLocal();
          const int32_t tempValueLocal = allocTempLocal();

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          pushIndexConst(indexKind, elementOffset);
          function.instructions.push_back({addOp, 0});
          pushIndexConst(indexKind, 16);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
          pushIndexConst(indexKind, elementOffset);
          function.instructions.push_back({addOp, 0});
          pushIndexConst(indexKind, 16);
          function.instructions.push_back({mulOp, 0});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValueLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValueLocal)});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back({IrOpcode::SubI32, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});
          return true;
        }

        const int32_t destPtrLocal = allocTempLocal();
        const int32_t srcPtrLocal = allocTempLocal();
        const int32_t tempValueLocal = allocTempLocal();

        const size_t loopStart = function.instructions.size();
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(lastIndexLocal)});
        function.instructions.push_back({cmpLtOp, 0});
        size_t jumpLoopEnd = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        pushIndexConst(indexKind, elementOffset);
        function.instructions.push_back({addOp, 0});
        pushIndexConst(indexKind, 16);
        function.instructions.push_back({mulOp, 0});
        function.instructions.push_back({IrOpcode::AddI64, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        pushIndexConst(indexKind, elementOffset + 1);
        function.instructions.push_back({addOp, 0});
        pushIndexConst(indexKind, 16);
        function.instructions.push_back({mulOp, 0});
        function.instructions.push_back({IrOpcode::AddI64, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(srcPtrLocal)});
        function.instructions.push_back({IrOpcode::LoadIndirect, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(tempValueLocal)});

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(tempValueLocal)});
        function.instructions.push_back({IrOpcode::StoreIndirect, 0});
        function.instructions.push_back({IrOpcode::Pop, 0});

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
        pushIndexConst(indexKind, 1);
        function.instructions.push_back({addOp, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
        function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

        size_t loopEndIndex = function.instructions.size();
        function.instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEndIndex);

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        function.instructions.push_back({IrOpcode::PushI32, 1});
        function.instructions.push_back({IrOpcode::SubI32, 0});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
        function.instructions.push_back({IrOpcode::StoreIndirect, 0});
        function.instructions.push_back({IrOpcode::Pop, 0});
        return true;
      }
    }
    if (stmt.kind == Expr::Kind::Call) {
      if (stmt.isMethodCall && !isArrayCountCall(stmt, localsIn) && !isStringCountCall(stmt, localsIn) &&
          !isVectorCapacityCall(stmt, localsIn)) {
        const Definition *callee = resolveMethodCallDefinition(stmt, localsIn);
        if (!callee) {
          return false;
        }
        if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
          error = "native backend does not support block arguments on calls";
          return false;
        }
        return emitInlineDefinitionCall(stmt, *callee, localsIn, false);
      }
      if (const Definition *callee = resolveDefinitionCall(stmt)) {
        if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
          error = "native backend does not support block arguments on calls";
          return false;
        }
        ReturnInfo info;
        if (!getReturnInfo(callee->fullPath, info)) {
          return false;
        }
        if (!emitInlineDefinitionCall(stmt, *callee, localsIn, false)) {
          return false;
        }
        if (!info.returnsVoid) {
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
        return true;
      }
    }
    if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
      if (!emitExpr(stmt, localsIn)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::Pop, 0});
      return true;
    }
    if (!emitExpr(stmt, localsIn)) {
      return false;
    }
    function.instructions.push_back({IrOpcode::Pop, 0});
    return true;
  };

  for (const auto &stmt : entryDef->statements) {
    if (!emitStatement(stmt, locals)) {
      return false;
    }
  }

  if (!sawReturn) {
    if (returnsVoid) {
      function.instructions.push_back({IrOpcode::ReturnVoid, 0});
    } else {
      error = "native backend requires an explicit return statement";
      return false;
    }
  }

  out.functions.push_back(std::move(function));
  out.entryIndex = 0;
  out.stringTable = std::move(stringTable);
  return true;
}
