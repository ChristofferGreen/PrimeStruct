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
          LocalMap blockLocals = localsIn;
          for (const auto &bodyExpr : expr.bodyArguments) {
            if (!emitStatement(bodyExpr, blockLocals)) {
              return false;
            }
          }
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          return true;
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
