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
              if (isLast) {
                if (bodyStmt.isBinding) {
                  error = "if branch blocks must end with an expression";
                  return false;
                }
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
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign calls in expressions";
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
    error = builtin.name + " requires a numeric/bool or string literal/binding argument";
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
      if (isFloatBinding(stmt)) {
        error = "native backend does not support float types";
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
            returnKind == LocalInfo::ValueKind::Int32 || returnKind == LocalInfo::ValueKind::Bool) {
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
        return resolveDefinitionCall(candidate) == nullptr;
      };
      auto emitBranch = [&](const Expr &branch, LocalMap &branchLocals) -> bool {
        if (isIfBlockEnvelope(branch)) {
          return emitBlock(branch, branchLocals);
        }
        return emitStatement(branch, branchLocals);
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
      if (stmt.isMethodCall && !isArrayCountCall(stmt, localsIn)) {
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
