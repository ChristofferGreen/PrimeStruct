        const auto countAccessResult = tryEmitCountAccessCall(
            expr,
            localsIn,
            isArrayCountCall,
            isVectorCapacityCall,
            isStringCountCall,
            isEntryArgsName,
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(targetExpr, targetLocals);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              const bool isExperimentalVectorTarget =
                  structPath == "/std/collections/experimental_vector/Vector" ||
                  structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
              const bool isExperimentalMapTarget =
                  structPath == "/std/collections/experimental_map/Map" ||
                  structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0;
              return targetInfo.isArrayOrVectorTarget || structPath == "/array" ||
                     structPath == "/vector" || structPath == "/Buffer" || structPath == "/map" ||
                     structPath == "/soa_vector" || isExperimentalVectorTarget ||
                     isExperimentalMapTarget;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(targetExpr, targetLocals);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     structPath == "/std/collections/experimental_vector/Vector" ||
                     structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(targetExpr, targetLocals);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     structPath == "/std/collections/experimental_vector/Vector" ||
                     structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
            },
            inferExprKind,
            resolveStringTableTarget,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            [&](IrOpcode opcode, uint64_t imm) { function.instructions.push_back({opcode, imm}); },
            error);
        if (countAccessResult == CountAccessCallEmitResult::Emitted) {
          return true;
        }
        if (countAccessResult == CountAccessCallEmitResult::Error) {
          return false;
        }
        error =
            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
            resolveExprPath(expr) + ", name=" + expr.name +
            ", args=" + std::to_string(expr.args.size()) +
            ", method=" + std::string(expr.isMethodCall ? "true" : "false") + ")";
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
      if (!ir_lowerer::parseLowererStringLiteral(arg.stringValue, decoded, error)) {
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
        if (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
          return true;
        }
      }
    }
    if (!emitExpr(arg, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
    if (kind == LocalInfo::ValueKind::String) {
      function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
      return true;
    }
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
