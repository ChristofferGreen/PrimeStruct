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
      LocalInfo info;
      info.isMutable = isBindingMutable(stmt);
      info.kind = kind;
      info.valueKind = valueKind;
      info.mapKeyKind = mapKeyKind;
      info.mapValueKind = mapValueKind;
      setReferenceArrayInfo(stmt, info);
      valueKind = info.valueKind;

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
      info.index = nextLocal++;
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
        if (context.returnsArray) {
          LocalInfo::ValueKind arrayKind = inferArrayElementKind(stmt.args.front(), localsIn);
          if (arrayKind == LocalInfo::ValueKind::Unknown) {
            error = "native backend only supports returning array values";
            return false;
          }
          if (arrayKind == LocalInfo::ValueKind::String) {
            error = "native backend does not support string array return types";
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
          size_t jumpIndex = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          context.returnJumps.push_back(jumpIndex);
          return true;
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
      LocalInfo::ValueKind arrayKind = inferArrayElementKind(stmt.args.front(), localsIn);
      if (arrayKind != LocalInfo::ValueKind::Unknown) {
        if (arrayKind == LocalInfo::ValueKind::String) {
          error = "native backend does not support string array return types";
          return false;
        }
        function.instructions.push_back({IrOpcode::ReturnI64, 0});
      } else {
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
        if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
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
