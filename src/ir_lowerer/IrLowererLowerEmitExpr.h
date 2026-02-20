  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (expr.isLambda) {
      error = "native backend does not support lambdas";
      return false;
    }
    switch (expr.kind) {
      case Expr::Kind::Literal: {
        if (expr.intWidth == 64 || expr.isUnsigned) {
          IrInstruction inst;
          inst.op = IrOpcode::PushI64;
          inst.imm = expr.literalValue;
          function.instructions.push_back(inst);
          return true;
        }
        int64_t value = static_cast<int64_t>(expr.literalValue);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
          error = "i32 literal out of range for native backend";
          return false;
        }
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = static_cast<int32_t>(value);
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::FloatLiteral:
        return emitFloatLiteral(expr);
      case Expr::Kind::StringLiteral:
        error = "native backend does not support string literals";
        return false;
      case Expr::Kind::BoolLiteral: {
        IrInstruction inst;
        inst.op = IrOpcode::PushI32;
        inst.imm = expr.boolValue ? 1 : 0;
        function.instructions.push_back(inst);
        return true;
      }
      case Expr::Kind::Name: {
        auto it = localsIn.find(expr.name);
        if (it != localsIn.end()) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          if (it->second.kind == LocalInfo::Kind::Reference) {
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        }
        if (hasEntryArgs && expr.name == entryArgsName) {
          error = "native backend only supports count() on entry arguments";
          return false;
        }
        std::string mathConst;
        if (getMathConstantName(expr.name, mathConst)) {
          double value = 0.0;
          if (mathConst == "pi") {
            value = 3.14159265358979323846;
          } else if (mathConst == "tau") {
            value = 6.28318530717958647692;
          } else if (mathConst == "e") {
            value = 2.71828182845904523536;
          }
          uint64_t bits = 0;
          std::memcpy(&bits, &value, sizeof(bits));
          function.instructions.push_back({IrOpcode::PushF64, bits});
          return true;
        }
        error = "native backend does not know identifier: " + expr.name;
        return false;
      }
      case Expr::Kind::Call: {
        if ((expr.hasBodyArguments || !expr.bodyArguments.empty()) && !isBlockCall(expr)) {
          Expr callExpr = expr;
          callExpr.bodyArguments.clear();
          callExpr.hasBodyArguments = false;
          if (!emitExpr(callExpr, localsIn)) {
            return false;
          }
          const int32_t resultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});
          OnErrorScope onErrorScope(currentOnError, std::nullopt);
          pushFileScope();
          LocalMap blockLocals = localsIn;
          for (const auto &bodyExpr : expr.bodyArguments) {
            if (!emitStatement(bodyExpr, blockLocals)) {
              return false;
            }
          }
          emitFileScopeCleanup(fileScopeStack.back());
          popFileScope();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          return true;
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "try")) {
          if (expr.args.size() != 1) {
            error = "try requires exactly one argument";
            return false;
          }
          if (!currentOnError.has_value()) {
            error = "missing on_error for ? usage";
            return false;
          }
          if (!currentReturnResult.has_value() || !currentReturnResult->isResult) {
            error = "try requires Result return type";
            return false;
          }
          ResultExprInfo resultInfo;
          if (!resolveResultExprInfo(expr.args.front(), localsIn, resultInfo) || !resultInfo.isResult) {
            error = "try requires Result argument";
            return false;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          const int32_t resultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});

          auto emitOnErrorReturn = [&](int32_t errorLocal) -> bool {
            if (!currentOnError.has_value()) {
              error = "missing on_error for ? usage";
              return false;
            }
            const OnErrorHandler &handler = *currentOnError;
            auto defIt = defMap.find(handler.handlerPath);
            if (defIt == defMap.end()) {
              error = "unknown on_error handler: " + handler.handlerPath;
              return false;
            }
            std::string errorName = "__on_error_err_" + std::to_string(onErrorTempCounter++);
            LocalInfo errorInfo;
            errorInfo.index = errorLocal;
            errorInfo.isMutable = false;
            errorInfo.kind = LocalInfo::Kind::Value;
            errorInfo.valueKind = LocalInfo::ValueKind::Int32;
            LocalMap handlerLocals = localsIn;
            handlerLocals.emplace(errorName, errorInfo);

            Expr errorExpr;
            errorExpr.kind = Expr::Kind::Name;
            errorExpr.name = errorName;
            errorExpr.namespacePrefix = expr.namespacePrefix;

            Expr callExpr;
            callExpr.kind = Expr::Kind::Call;
            callExpr.name = handler.handlerPath;
            callExpr.namespacePrefix = expr.namespacePrefix;
            callExpr.isMethodCall = false;
            callExpr.isBinding = false;
            callExpr.args.reserve(handler.boundArgs.size() + 1);
            callExpr.args.push_back(errorExpr);
            for (const auto &argExpr : handler.boundArgs) {
              callExpr.args.push_back(argExpr);
            }
            ReturnInfo handlerReturn;
            if (!getReturnInfo(defIt->second->fullPath, handlerReturn)) {
              return false;
            }
            if (!emitInlineDefinitionCall(callExpr, *defIt->second, handlerLocals, false)) {
              return false;
            }
            if (!handlerReturn.returnsVoid) {
              function.instructions.push_back({IrOpcode::Pop, 0});
            }

            const bool returnHasValue = currentReturnResult->hasValue;
            if (activeInlineContext) {
              InlineContext &context = *activeInlineContext;
              if (context.returnLocal < 0) {
                error = "native backend missing inline return local";
                return false;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              if (returnHasValue) {
                function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
                function.instructions.push_back({IrOpcode::MulI64, 0});
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
              size_t jumpIndex = function.instructions.size();
              function.instructions.push_back({IrOpcode::Jump, 0});
              context.returnJumps.push_back(jumpIndex);
              return true;
            }
            emitFileScopeCleanupAll();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
            if (returnHasValue) {
              function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
              function.instructions.push_back({IrOpcode::MulI64, 0});
              function.instructions.push_back({IrOpcode::ReturnI64, 0});
            } else {
              function.instructions.push_back({IrOpcode::ReturnI32, 0});
            }
            return true;
          };

          if (resultInfo.hasValue) {
            const int32_t errorLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
            function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
            function.instructions.push_back({IrOpcode::DivI64, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
            function.instructions.push_back({IrOpcode::PushI64, 0});
            function.instructions.push_back({IrOpcode::CmpEqI64, 0});
            size_t jumpError = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
            function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
            function.instructions.push_back({IrOpcode::MulI64, 0});
            function.instructions.push_back({IrOpcode::SubI64, 0});
            size_t jumpEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            size_t errorIndex = function.instructions.size();
            function.instructions[jumpError].imm = static_cast<int32_t>(errorIndex);
            if (!emitOnErrorReturn(errorLocal)) {
              return false;
            }
            size_t endIndex = function.instructions.size();
            function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
            return true;
          }
          const int32_t errorLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          size_t jumpError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});
          function.instructions.push_back({IrOpcode::PushI32, 0});
          size_t jumpEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});
          size_t errorIndex = function.instructions.size();
          function.instructions[jumpError].imm = static_cast<int32_t>(errorIndex);
          if (!emitOnErrorReturn(errorLocal)) {
            return false;
          }
          size_t endIndex = function.instructions.size();
          function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          return true;
        }
        if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
            expr.args.front().name == "Result" && expr.name == "ok") {
          if (expr.args.size() == 1) {
            function.instructions.push_back({IrOpcode::PushI32, 0});
            return true;
          }
          if (expr.args.size() != 2) {
            error = "Result.ok accepts at most one argument";
            return false;
          }
          LocalInfo::ValueKind argKind = inferExprKind(expr.args[1], localsIn);
          if (argKind != LocalInfo::ValueKind::Int32 && argKind != LocalInfo::ValueKind::Bool) {
            error = "native backend only supports Result.ok with 32-bit values";
            return false;
          }
          return emitExpr(expr.args[1], localsIn);
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "File")) {
          if (expr.templateArgs.size() != 1) {
            error = "File requires exactly one template argument";
            return false;
          }
          if (expr.args.size() != 1) {
            error = "File requires exactly one path argument";
            return false;
          }
          int32_t stringIndex = -1;
          size_t length = 0;
          if (!resolveStringTableTarget(expr.args.front(), localsIn, stringIndex, length)) {
            error = "native backend only supports File() with string literals or literal-backed bindings";
            return false;
          }
          if (expr.templateArgs.front() == "Read") {
            function.instructions.push_back({IrOpcode::FileOpenRead, static_cast<uint64_t>(stringIndex)});
            return true;
          }
          if (expr.templateArgs.front() == "Write") {
            function.instructions.push_back({IrOpcode::FileOpenWrite, static_cast<uint64_t>(stringIndex)});
            return true;
          }
          if (expr.templateArgs.front() == "Append") {
            function.instructions.push_back({IrOpcode::FileOpenAppend, static_cast<uint64_t>(stringIndex)});
            return true;
          }
          error = "File requires Read, Write, or Append mode";
          return false;
        }
        if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
          auto it = localsIn.find(expr.args.front().name);
          if (it != localsIn.end() && it->second.isFileHandle) {
            const int32_t handleIndex = it->second.index;
            auto emitWriteStep = [&](const Expr &arg, int32_t errorLocal) -> bool {
              int32_t stringIndex = -1;
              size_t length = 0;
              if (resolveStringTableTarget(arg, localsIn, stringIndex, length)) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
                function.instructions.push_back({IrOpcode::FileWriteString, static_cast<uint64_t>(stringIndex)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
                return true;
              }
              LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
              if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::FileWriteI32, 0});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
                return true;
              }
              if (kind == LocalInfo::ValueKind::Int64) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::FileWriteI64, 0});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
                return true;
              }
              if (kind == LocalInfo::ValueKind::UInt64) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
                if (!emitExpr(arg, localsIn)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::FileWriteU64, 0});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
                return true;
              }
              error = "file write requires integer/bool or string arguments";
              return false;
            };
            if (expr.name == "write" || expr.name == "write_line") {
              const int32_t errorLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::PushI32, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
              for (size_t i = 1; i < expr.args.size(); ++i) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
                function.instructions.push_back({IrOpcode::PushI64, 0});
                function.instructions.push_back({IrOpcode::CmpEqI64, 0});
                size_t skipIndex = function.instructions.size();
                function.instructions.push_back({IrOpcode::JumpIfZero, 0});
                if (!emitWriteStep(expr.args[i], errorLocal)) {
                  return false;
                }
                size_t afterIndex = function.instructions.size();
                function.instructions[skipIndex].imm = static_cast<int32_t>(afterIndex);
              }
              if (expr.name == "write_line") {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
                function.instructions.push_back({IrOpcode::PushI64, 0});
                function.instructions.push_back({IrOpcode::CmpEqI64, 0});
                size_t skipIndex = function.instructions.size();
                function.instructions.push_back({IrOpcode::JumpIfZero, 0});
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
                function.instructions.push_back({IrOpcode::FileWriteNewline, 0});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
                size_t afterIndex = function.instructions.size();
                function.instructions[skipIndex].imm = static_cast<int32_t>(afterIndex);
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              return true;
            }
            if (expr.name == "write_byte") {
              if (expr.args.size() != 2) {
                error = "write_byte requires exactly one argument";
                return false;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::FileWriteByte, 0});
              return true;
            }
            if (expr.name == "write_bytes") {
              if (expr.args.size() != 2) {
                error = "write_bytes requires exactly one argument";
                return false;
              }
              const Expr &bytesExpr = expr.args[1];
              const int32_t ptrLocal = allocTempLocal();
              if (!emitExpr(bytesExpr, localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              const int32_t countLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});
              const int32_t indexLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::PushI32, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
              const int32_t errorLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::PushI32, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});

              size_t loopStart = function.instructions.size();
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              function.instructions.push_back({IrOpcode::PushI64, 0});
              function.instructions.push_back({IrOpcode::CmpEqI64, 0});
              size_t jumpError = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
              function.instructions.push_back({IrOpcode::CmpLtI32, 0});
              size_t jumpLoopEnd = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({IrOpcode::PushI32, 1});
              function.instructions.push_back({IrOpcode::AddI32, 0});
              function.instructions.push_back({IrOpcode::PushI32, 16});
              function.instructions.push_back({IrOpcode::MulI32, 0});
              function.instructions.push_back({IrOpcode::AddI64, 0});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              function.instructions.push_back({IrOpcode::FileWriteByte, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({IrOpcode::PushI32, 1});
              function.instructions.push_back({IrOpcode::AddI32, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

              size_t loopEnd = function.instructions.size();
              function.instructions[jumpError].imm = static_cast<int32_t>(loopEnd);
              function.instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEnd);
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              return true;
            }
            if (expr.name == "flush") {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
              function.instructions.push_back({IrOpcode::FileFlush, 0});
              return true;
            }
            if (expr.name == "close") {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(handleIndex)});
              function.instructions.push_back({IrOpcode::FileClose, 0});
              const int32_t errorLocal = allocTempLocal();
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
              function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(static_cast<int64_t>(-1))});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(handleIndex)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              return true;
            }
          }
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 &&
            !isArrayCountCall(expr, localsIn) && !isStringCountCall(expr, localsIn)) {
          Expr methodExpr = expr;
          methodExpr.isMethodCall = true;
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee) {
            if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
              error = "native backend does not support block arguments on calls";
              return false;
            }
            return emitInlineDefinitionCall(methodExpr, *callee, localsIn, true);
          }
        }
        if (expr.isMethodCall && !isArrayCountCall(expr, localsIn) && !isStringCountCall(expr, localsIn) &&
            !isVectorCapacityCall(expr, localsIn)) {
          const Definition *callee = resolveMethodCallDefinition(expr, localsIn);
          if (!callee) {
            return false;
          }
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error = "native backend does not support block arguments on calls";
            return false;
          }
          return emitInlineDefinitionCall(expr, *callee, localsIn, true);
        }
        if (const Definition *callee = resolveDefinitionCall(expr)) {
          if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
            error = "native backend does not support block arguments on calls";
            return false;
          }
          return emitInlineDefinitionCall(expr, *callee, localsIn, true);
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 &&
            !isArrayCountCall(expr, localsIn) && !isStringCountCall(expr, localsIn)) {
          Expr methodExpr = expr;
          methodExpr.isMethodCall = true;
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee) {
            if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
              error = "native backend does not support block arguments on calls";
              return false;
            }
            return emitInlineDefinitionCall(methodExpr, *callee, localsIn, true);
          }
        }
        std::string mathName;
        if (getMathBuiltinName(expr, mathName)) {
          if (mathName == "abs" || mathName == "sign" || mathName == "min" || mathName == "max" ||
              mathName == "clamp" || mathName == "saturate" || mathName == "lerp" || mathName == "fma" ||
              mathName == "hypot" || mathName == "copysign" || mathName == "radians" || mathName == "degrees" ||
              mathName == "sin" || mathName == "cos" || mathName == "tan" || mathName == "atan2" ||
              mathName == "asin" || mathName == "acos" || mathName == "atan" || mathName == "sinh" ||
              mathName == "cosh" || mathName == "tanh" || mathName == "asinh" || mathName == "acosh" ||
              mathName == "atanh" || mathName == "exp" || mathName == "exp2" || mathName == "log" ||
              mathName == "log2" || mathName == "log10" || mathName == "pow" || mathName == "is_nan" ||
              mathName == "is_inf" || mathName == "is_finite" || mathName == "floor" || mathName == "ceil" ||
              mathName == "round" || mathName == "trunc" || mathName == "fract" || mathName == "sqrt" ||
              mathName == "cbrt") {
            // Supported in native/VM lowering.
          } else {
            error = "native backend does not support math builtin: " + mathName;
            return false;
          }
        }
        if (isArrayCountCall(expr, localsIn)) {
          if (isEntryArgsName(expr.args.front(), localsIn)) {
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            return true;
          }
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (isVectorCapacityCall(expr, localsIn)) {
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::PushI64, 16});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (isStringCountCall(expr, localsIn)) {
          if (expr.args.size() != 1) {
            error = "count requires exactly one argument";
            return false;
          }
          int32_t stringIndex = -1;
          size_t length = 0;
          if (!resolveStringTableTarget(expr.args.front(), localsIn, stringIndex, length)) {
            error = "native backend only supports count() on string literals or string bindings";
            return false;
          }
          if (length > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            error = "native backend string too large for count()";
            return false;
          }
          function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(length))});
          return true;
        }
        if (isSimpleCallName(expr, "count")) {
          error = "count requires array, vector, map, or string target";
          return false;
        }
        if (isSimpleCallName(expr, "capacity")) {
          error = "capacity requires vector target";
          return false;
        }
        std::string vectorHelper;
        if (isSimpleCallName(expr, "push")) {
          vectorHelper = "push";
        } else if (isSimpleCallName(expr, "pop")) {
          vectorHelper = "pop";
        } else if (isSimpleCallName(expr, "reserve")) {
          vectorHelper = "reserve";
        } else if (isSimpleCallName(expr, "clear")) {
          vectorHelper = "clear";
        } else if (isSimpleCallName(expr, "remove_at")) {
          vectorHelper = "remove_at";
        } else if (isSimpleCallName(expr, "remove_swap")) {
          vectorHelper = "remove_swap";
        }
        if (!vectorHelper.empty()) {
          error = "native backend does not support vector helper: " + vectorHelper;
          return false;
        }
        PrintBuiltin printBuiltin;
        if (getPrintBuiltin(expr, printBuiltin)) {
          error = printBuiltin.name + " is only supported as a statement in the native backend";
          return false;
        }
        std::string accessName;
        if (getBuiltinArrayAccessName(expr, accessName)) {
          if (expr.args.size() != 2) {
            error = accessName + " requires exactly two arguments";
            return false;
          }
          const Expr &target = expr.args.front();

          int32_t stringIndex = -1;
          size_t stringLength = 0;
          if (resolveStringTableTarget(target, localsIn, stringIndex, stringLength)) {
            LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
            if (!isSupportedIndexKind(indexKind)) {
              error = "native backend requires integer indices for " + accessName;
              return false;
            }
            if (stringLength > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
              error = "native backend string too large for indexing";
              return false;
            }

            const int32_t indexLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
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
                emitStringIndexOutOfBounds();
                size_t nonNegativeIndex = function.instructions.size();
                function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
              }

              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              IrOpcode lengthOp =
                  (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
              function.instructions.push_back({lengthOp, static_cast<uint64_t>(stringLength)});
              function.instructions.push_back({cmpGeForIndex(indexKind), 0});
              size_t jumpInRange = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitStringIndexOutOfBounds();
              size_t inRangeIndex = function.instructions.size();
              function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::LoadStringByte, static_cast<uint64_t>(stringIndex)});
            return true;
          }
          if (target.kind == Expr::Kind::StringLiteral) {
            return false;
          }
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports indexing into string literals or string bindings";
              return false;
            }
          }
          if (isEntryArgsName(target, localsIn)) {
            error = "native backend only supports entry argument indexing in print calls or string bindings";
            return false;
          }

          bool isMapTarget = false;
          LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
          LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Map) {
              isMapTarget = true;
              mapKeyKind = it->second.mapKeyKind;
              mapValueKind = it->second.mapValueKind;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && collection == "map" && target.templateArgs.size() == 2) {
              isMapTarget = true;
              mapKeyKind = valueKindFromTypeName(target.templateArgs[0]);
              mapValueKind = valueKindFromTypeName(target.templateArgs[1]);
            }
          }
          if (isMapTarget) {
            if (mapKeyKind == LocalInfo::ValueKind::Unknown || mapValueKind == LocalInfo::ValueKind::Unknown) {
              error = "native backend requires typed map bindings for " + accessName;
              return false;
            }
            if (mapValueKind == LocalInfo::ValueKind::String) {
              error = "native backend only supports numeric/bool map values";
              return false;
            }

            const int32_t ptrLocal = allocTempLocal();
            if (!emitExpr(expr.args[0], localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

            const int32_t keyLocal = allocTempLocal();
            if (mapKeyKind == LocalInfo::ValueKind::String) {
              int32_t stringIndex = -1;
              size_t length = 0;
              if (!resolveStringTableTarget(expr.args[1], localsIn, stringIndex, length)) {
                error =
                    "native backend requires map lookup key to be string literal or binding backed by literals";
                return false;
              }
              function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(keyLocal)});
            } else {
              LocalInfo::ValueKind lookupKeyKind = inferExprKind(expr.args[1], localsIn);
              if (lookupKeyKind == LocalInfo::ValueKind::Unknown || lookupKeyKind == LocalInfo::ValueKind::String) {
                error = "native backend requires map lookup key to be numeric/bool";
                return false;
              }
              if (lookupKeyKind != mapKeyKind) {
                error = "native backend requires map lookup key type to match map key type";
                return false;
              }
              if (!emitExpr(expr.args[1], localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(keyLocal)});
            }

            const int32_t countLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

            const int32_t indexLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

            size_t loopStart = function.instructions.size();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            size_t jumpLoopEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 2});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 16});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal)});
            IrOpcode cmpKeyOp = IrOpcode::CmpEqI32;
            if (mapKeyKind == LocalInfo::ValueKind::Int64 || mapKeyKind == LocalInfo::ValueKind::UInt64) {
              cmpKeyOp = IrOpcode::CmpEqI64;
            } else if (mapKeyKind == LocalInfo::ValueKind::Float64) {
              cmpKeyOp = IrOpcode::CmpEqF64;
            } else if (mapKeyKind == LocalInfo::ValueKind::Float32) {
              cmpKeyOp = IrOpcode::CmpEqF32;
            }
            function.instructions.push_back({cmpKeyOp, 0});
            size_t jumpNotMatch = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            size_t jumpFound = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});

            size_t notMatchIndex = function.instructions.size();
            function.instructions[jumpNotMatch].imm = static_cast<int32_t>(notMatchIndex);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 1});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(loopStart)});

            size_t loopEndIndex = function.instructions.size();
            function.instructions[jumpLoopEnd].imm = static_cast<int32_t>(loopEndIndex);
            function.instructions[jumpFound].imm = static_cast<int32_t>(loopEndIndex);

            if (accessName == "at") {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
              function.instructions.push_back({IrOpcode::CmpEqI32, 0});
              size_t jumpKeyFound = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitMapKeyNotFound();
              size_t keyFoundIndex = function.instructions.size();
              function.instructions[jumpKeyFound].imm = static_cast<int32_t>(keyFoundIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 2});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 2});
            function.instructions.push_back({IrOpcode::AddI32, 0});
            function.instructions.push_back({IrOpcode::PushI32, 16});
            function.instructions.push_back({IrOpcode::MulI32, 0});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }

          LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
          bool isVectorTarget = false;
          if (target.kind == Expr::Kind::Name) {
            auto it = localsIn.find(target.name);
            if (it != localsIn.end() &&
                (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
              elemKind = it->second.valueKind;
              isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
            } else if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference &&
                       it->second.referenceToArray) {
              elemKind = it->second.valueKind;
            }
          } else if (target.kind == Expr::Kind::Call) {
            std::string collection;
            if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
                target.templateArgs.size() == 1) {
              elemKind = valueKindFromTypeName(target.templateArgs.front());
              isVectorTarget = (collection == "vector");
            }
          }
          if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
            error = "native backend only supports at() on numeric/bool arrays or vectors";
            return false;
          }

          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(expr.args[1], localsIn));
          if (!isSupportedIndexKind(indexKind)) {
            error = "native backend requires integer indices for " + accessName;
            return false;
          }

          const uint64_t headerSlots = isVectorTarget ? 2 : 1;
          const int32_t ptrLocal = allocTempLocal();
          if (!emitExpr(expr.args[0], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            const int32_t countLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal)});

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
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal)});
            function.instructions.push_back({cmpGeForIndex(indexKind), 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          function.instructions.push_back({pushOneForIndex(indexKind), headerSlots});
          function.instructions.push_back({addForIndex(indexKind), 0});
          function.instructions.push_back({pushOneForIndex(indexKind), 16});
          function.instructions.push_back({mulForIndex(indexKind), 0});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
