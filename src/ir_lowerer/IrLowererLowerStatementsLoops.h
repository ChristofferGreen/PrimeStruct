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

      if (countKind == LocalInfo::ValueKind::Int32) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
        function.instructions.push_back({IrOpcode::PushI32, 0});
        function.instructions.push_back({IrOpcode::CmpLtI32, 0});
        size_t jumpNonNegative = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitLoopCountNegative();
        size_t nonNegativeIndex = function.instructions.size();
        function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
      } else if (countKind == LocalInfo::ValueKind::Int64) {
        function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(counterLocal)});
        function.instructions.push_back({IrOpcode::PushI64, 0});
        function.instructions.push_back({IrOpcode::CmpLtI64, 0});
        size_t jumpNonNegative = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitLoopCountNegative();
        size_t nonNegativeIndex = function.instructions.size();
        function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
      }

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
      OnErrorScope onErrorScope(currentOnError, std::nullopt);
      pushFileScope();
      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : body.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();

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
        applyStructArrayInfo(binding, info);
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

      OnErrorScope onErrorScope(currentOnError, std::nullopt);
      pushFileScope();
      LocalMap bodyLocals = loopLocals;
      for (const auto &bodyStmt : body.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }
      if (!emitStatement(step, loopLocals)) {
        return false;
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();

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

      OnErrorScope onErrorScope(currentOnError, std::nullopt);
      pushFileScope();
      LocalMap bodyLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, bodyLocals)) {
          return false;
        }
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();

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
      std::optional<OnErrorHandler> blockOnError;
      if (!parseOnErrorTransform(stmt.transforms, stmt.namespacePrefix, "block", blockOnError)) {
        return false;
      }
      OnErrorScope onErrorScope(currentOnError, blockOnError);
      pushFileScope();
      LocalMap blockLocals = localsIn;
      for (const auto &bodyStmt : stmt.bodyArguments) {
        if (!emitStatement(bodyStmt, blockLocals)) {
          return false;
        }
      }
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();
      return true;
    }
    if (stmt.kind == Expr::Kind::Call) {
      if ((stmt.hasBodyArguments || !stmt.bodyArguments.empty()) && !isBlockCall(stmt)) {
        Expr callExpr = stmt;
        callExpr.bodyArguments.clear();
        callExpr.hasBodyArguments = false;
        const Definition *callee = nullptr;
        if (callExpr.isMethodCall && !isArrayCountCall(callExpr, localsIn) && !isStringCountCall(callExpr, localsIn) &&
            !isVectorCapacityCall(callExpr, localsIn)) {
          callee = resolveMethodCallDefinition(callExpr, localsIn);
          if (!callee) {
            return false;
          }
        } else {
          callee = resolveDefinitionCall(callExpr);
          if (!callee) {
            error = "block arguments require a definition target: " + resolveExprPath(callExpr);
            return false;
          }
        }
        ReturnInfo info;
        if (!getReturnInfo(callee->fullPath, info)) {
          return false;
        }
        if (!emitInlineDefinitionCall(callExpr, *callee, localsIn, false)) {
          return false;
        }
        if (!info.returnsVoid) {
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
        OnErrorScope onErrorScope(currentOnError, std::nullopt);
        pushFileScope();
        LocalMap blockLocals = localsIn;
        for (const auto &bodyExpr : stmt.bodyArguments) {
          if (!emitStatement(bodyExpr, blockLocals)) {
            return false;
          }
        }
        emitFileScopeCleanup(fileScopeStack.back());
        popFileScope();
        return true;
      }
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
