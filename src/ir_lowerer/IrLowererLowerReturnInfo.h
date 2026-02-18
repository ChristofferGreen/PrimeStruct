    bool hasReturnTransformLocal = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "struct" || transform.name == "pod" || transform.name == "handle" ||
          transform.name == "gpu_lane" || transform.name == "no_padding" ||
          transform.name == "platform_independent_padding") {
        info.returnsVoid = true;
        hasReturnTransformLocal = true;
        break;
      }
      if (transform.name != "return") {
        continue;
      }
      hasReturnTransformLocal = true;
      if (transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string &typeName = transform.templateArgs.front();
      if (typeName == "void") {
        info.returnsVoid = true;
        break;
      }
      info.kind = valueKindFromTypeName(typeName);
      info.returnsVoid = false;
      break;
    }

    if (hasReturnTransformLocal) {
      if (!info.returnsVoid) {
        if (info.kind == LocalInfo::ValueKind::Unknown) {
          error = "native backend does not support return type on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
        if (info.kind == LocalInfo::ValueKind::String) {
          error = "native backend does not support string return types on " + def.fullPath;
          returnInferenceStack.erase(path);
          return false;
        }
      }
    } else {
      if (!def.hasReturnStatement) {
        info.returnsVoid = true;
      } else {
        LocalMap localsForInference;
        for (const auto &param : def.parameters) {
          LocalInfo paramInfo;
          paramInfo.index = 0;
          paramInfo.isMutable = isBindingMutable(param);
          paramInfo.kind = bindingKind(param);
          if (hasExplicitBindingTypeTransform(param)) {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          } else if (param.args.size() == 1 && paramInfo.kind == LocalInfo::Kind::Value) {
            paramInfo.valueKind = inferExprKind(param.args.front(), localsForInference);
            if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              paramInfo.valueKind = LocalInfo::ValueKind::Int32;
            }
          } else {
            paramInfo.valueKind = bindingValueKind(param, paramInfo.kind);
          }
          if (isStringBinding(param)) {
            if (paramInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              returnInferenceStack.erase(path);
              return false;
            }
          }
          if (paramInfo.valueKind == LocalInfo::ValueKind::Unknown) {
            error = "native backend requires typed parameters on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          localsForInference.emplace(param.name, paramInfo);
        }

        LocalInfo::ValueKind inferred = LocalInfo::ValueKind::Unknown;
        bool sawReturnLocal = false;
        bool inferredVoid = false;
        std::function<bool(const Expr &, LocalMap &)> inferStatement;
        inferStatement = [&](const Expr &stmt, LocalMap &activeLocals) -> bool {
          if (stmt.isBinding) {
            LocalInfo bindingInfo;
            bindingInfo.index = 0;
            bindingInfo.isMutable = isBindingMutable(stmt);
            bindingInfo.kind = bindingKind(stmt);
            if (hasExplicitBindingTypeTransform(stmt)) {
              bindingInfo.valueKind = bindingValueKind(stmt, bindingInfo.kind);
            } else if (stmt.args.size() == 1 && bindingInfo.kind == LocalInfo::Kind::Value) {
              bindingInfo.valueKind = inferExprKind(stmt.args.front(), activeLocals);
              if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
                bindingInfo.valueKind = LocalInfo::ValueKind::Int32;
              }
            } else {
              bindingInfo.valueKind = LocalInfo::ValueKind::Unknown;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::String && bindingInfo.kind != LocalInfo::Kind::Value) {
              error = "native backend does not support string pointers or references";
              return false;
            }
            if (bindingInfo.valueKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend requires typed bindings on " + def.fullPath;
              return false;
            }
            activeLocals.emplace(stmt.name, bindingInfo);
            return true;
          }
          if (isReturnCall(stmt)) {
            sawReturnLocal = true;
            if (stmt.args.empty()) {
              inferredVoid = true;
              return true;
            }
            LocalInfo::ValueKind kind = inferExprKind(stmt.args.front(), activeLocals);
            if (kind == LocalInfo::ValueKind::Unknown) {
              error = "unable to infer return type on " + def.fullPath;
              return false;
            }
            if (kind == LocalInfo::ValueKind::String) {
              error = "native backend does not support string return types on " + def.fullPath;
              return false;
            }
            if (inferred == LocalInfo::ValueKind::Unknown) {
              inferred = kind;
              return true;
            }
            if (inferred != kind) {
              error = "conflicting return types on " + def.fullPath;
              return false;
            }
            return true;
          }
          if (isIfCall(stmt) && stmt.args.size() == 3) {
            const Expr &thenBlock = stmt.args[1];
            const Expr &elseBlock = stmt.args[2];
            auto walkBlock = [&](const Expr &block) -> bool {
              LocalMap blockLocals = activeLocals;
              for (const auto &bodyStmt : block.bodyArguments) {
                if (!inferStatement(bodyStmt, blockLocals)) {
                  return false;
                }
              }
              return true;
            };
            if (!walkBlock(thenBlock)) {
              return false;
            }
            if (!walkBlock(elseBlock)) {
              return false;
            }
          }
          if (isRepeatCall(stmt)) {
            LocalMap blockLocals = activeLocals;
            for (const auto &bodyStmt : stmt.bodyArguments) {
              if (!inferStatement(bodyStmt, blockLocals)) {
                return false;
              }
            }
          }
          return true;
        };

        LocalMap locals = localsForInference;
        for (const auto &stmt : def.statements) {
          if (!inferStatement(stmt, locals)) {
            returnInferenceStack.erase(path);
            return false;
          }
        }
        if (!sawReturnLocal || inferredVoid) {
          if (sawReturnLocal && inferred != LocalInfo::ValueKind::Unknown) {
            error = "conflicting return types on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
          info.returnsVoid = true;
        } else {
          info.returnsVoid = false;
          info.kind = inferred;
          if (info.kind == LocalInfo::ValueKind::Unknown) {
            error = "unable to infer return type on " + def.fullPath;
            returnInferenceStack.erase(path);
            return false;
          }
        }
      }
    }

    returnInferenceStack.erase(path);
    returnInfoCache.emplace(path, info);
    outInfo = info;
    return true;
  };

  struct InlineContext {
    std::string defPath;
    bool returnsVoid = false;
    LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
    int32_t returnLocal = -1;
    std::vector<size_t> returnJumps;
  };

  InlineContext *activeInlineContext = nullptr;
  std::unordered_set<std::string> inlineStack;

  std::function<bool(const Expr &, const LocalMap &)> emitExpr;
  std::function<bool(const Expr &, LocalMap &)> emitStatement;
  auto allocTempLocal = [&]() -> int32_t {
    return nextLocal++;
  };

  auto emitBlock = [&](const Expr &blockExpr, LocalMap &blockLocals) -> bool {
    if (blockExpr.kind != Expr::Kind::Call) {
      error = "native backend expects if branch blocks to be calls";
      return false;
    }
    if (!blockExpr.args.empty()) {
      error = "native backend does not support arguments on if branch blocks";
      return false;
    }
    for (const auto &stmt : blockExpr.bodyArguments) {
      if (!emitStatement(stmt, blockLocals)) {
        return false;
      }
    }
    return true;
  };

  auto emitFloatLiteral = [&](const Expr &expr) -> bool {
    char *end = nullptr;
    const char *text = expr.floatValue.c_str();
    double value = std::strtod(text, &end);
    if (end == text || (end && *end != '\0')) {
      error = "invalid float literal";
      return false;
    }
    if (expr.floatWidth == 64) {
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

  auto emitCompareToZero = [&](LocalInfo::ValueKind kind, bool equals) -> bool {
    if (kind == LocalInfo::ValueKind::Int64 || kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PushI64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI64 : IrOpcode::CmpNeI64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Float64) {
      function.instructions.push_back({IrOpcode::PushF64, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqF64 : IrOpcode::CmpNeF64, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqI32 : IrOpcode::CmpNeI32, 0});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Float32) {
      function.instructions.push_back({IrOpcode::PushF32, 0});
      function.instructions.push_back({equals ? IrOpcode::CmpEqF32 : IrOpcode::CmpNeF32, 0});
      return true;
    }
    error = "boolean conversion requires numeric operand";
    return false;
  };

  auto resolveDefinitionCall = [&](const Expr &callExpr) -> const Definition * {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall) {
      return nullptr;
    }
    const std::string resolved = resolveExprPath(callExpr);
    auto it = defMap.find(resolved);
    if (it == defMap.end()) {
      return nullptr;
    }
    return it->second;
  };
  auto buildOrderedCallArguments = [&](const Expr &callExpr,
                                       const std::vector<Expr> &params,
                                       std::vector<const Expr *> &ordered) -> bool {
    ordered.assign(params.size(), nullptr);
    size_t positionalIndex = 0;
    for (size_t i = 0; i < callExpr.args.size(); ++i) {
      if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
        const std::string &name = *callExpr.argNames[i];
        size_t index = params.size();
        for (size_t p = 0; p < params.size(); ++p) {
          if (params[p].name == name) {
            index = p;
            break;
          }
        }
        if (index >= params.size()) {
          error = "unknown named argument: " + name;
          return false;
        }
        if (ordered[index] != nullptr) {
          error = "named argument duplicates parameter: " + name;
          return false;
        }
        ordered[index] = &callExpr.args[i];
        continue;
      }
      while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
        ++positionalIndex;
      }
      if (positionalIndex >= ordered.size()) {
        error = "argument count mismatch";
        return false;
      }
      ordered[positionalIndex] = &callExpr.args[i];
      ++positionalIndex;
    }
    for (size_t i = 0; i < ordered.size(); ++i) {
      if (ordered[i] != nullptr) {
        continue;
      }
      if (!params[i].args.empty()) {
        ordered[i] = &params[i].args.front();
        continue;
      }
      error = "argument count mismatch";
      return false;
    }
    return true;
  };

  auto emitStringValueForCall = [&](const Expr &arg,
                                    const LocalMap &callerLocals,
                                    LocalInfo::StringSource &sourceOut,
                                    int32_t &stringIndexOut,
                                    bool &argvCheckedOut) -> bool {
    sourceOut = LocalInfo::StringSource::None;
    stringIndexOut = -1;
    argvCheckedOut = true;
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!parseStringLiteral(arg.stringValue, decoded)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
      sourceOut = LocalInfo::StringSource::TableIndex;
      stringIndexOut = index;
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = callerLocals.find(arg.name);
      if (it == callerLocals.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      if (it->second.valueKind != LocalInfo::ValueKind::String || it->second.stringSource == LocalInfo::StringSource::None) {
        error = "native backend requires string arguments to use string literals, bindings, or entry args";
        return false;
      }
      sourceOut = it->second.stringSource;
      stringIndexOut = it->second.stringIndex;
      argvCheckedOut = it->second.argvChecked;
      function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
      return true;
    }
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (!getBuiltinArrayAccessName(arg, accessName)) {
        error = "native backend requires string arguments to use string literals, bindings, or entry args";
        return false;
      }
      if (arg.args.size() != 2) {
        error = accessName + " requires exactly two arguments";
        return false;
      }
      if (!isEntryArgsName(arg.args.front(), callerLocals)) {
        error = "native backend only supports entry argument indexing";
        return false;
      }
      LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(arg.args[1], callerLocals));
      if (!isSupportedIndexKind(indexKind)) {
        error = "native backend requires integer indices for " + accessName;
        return false;
      }

      const int32_t argvIndexLocal = allocTempLocal();
      if (!emitExpr(arg.args[1], callerLocals)) {
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
      sourceOut = LocalInfo::StringSource::ArgvIndex;
      argvCheckedOut = (accessName == "at");
      return true;
    }
    error = "native backend requires string arguments to use string literals, bindings, or entry args";
    return false;
  };
