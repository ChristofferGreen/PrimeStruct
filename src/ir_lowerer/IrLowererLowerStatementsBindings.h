  emitStatement = [&](const Expr &stmt, LocalMap &localsIn) -> bool {
    if (!stmt.isBinding && stmt.kind == Expr::Kind::StringLiteral) {
      error = "native backend does not support string literal statements";
      return false;
    }
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
      std::string uninitializedType;
      if (extractUninitializedTemplateArg(stmt, uninitializedType)) {
        UninitializedTypeInfo uninitInfo;
        if (!resolveUninitializedTypeInfo(uninitializedType, stmt.namespacePrefix, uninitInfo)) {
          if (error.empty()) {
            error = "native backend does not support uninitialized storage for type: " + uninitializedType;
          }
          return false;
        }
        LocalInfo info;
        info.isMutable = isBindingMutable(stmt);
        info.isUninitializedStorage = true;
        info.kind = uninitInfo.kind;
        info.valueKind = uninitInfo.valueKind;
        info.mapKeyKind = uninitInfo.mapKeyKind;
        info.mapValueKind = uninitInfo.mapValueKind;
        info.structTypeName = uninitInfo.structPath;
        if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
          StructSlotLayout layout;
          if (!resolveStructSlotLayout(info.structTypeName, layout)) {
            return false;
          }
          const int32_t baseLocal = nextLocal;
          nextLocal += layout.totalSlots;
          info.index = nextLocal++;
          function.instructions.push_back(
              {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
          localsIn.emplace(stmt.name, info);
          return true;
        }
        info.index = nextLocal++;
        IrOpcode zeroOp = IrOpcode::PushI32;
        uint64_t zeroImm = 0;
        if (!selectUninitializedStorageZeroInstruction(
                info.kind, info.valueKind, stmt.name, zeroOp, zeroImm, error)) {
          return false;
        }
        function.instructions.push_back({zeroOp, zeroImm});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        localsIn.emplace(stmt.name, info);
        return true;
      }
      const StatementBindingTypeInfo bindingTypeInfo = inferStatementBindingTypeInfo(
          stmt, init, localsIn, hasExplicitBindingTypeTransform, bindingKind, bindingValueKind, inferExprKind);
      LocalInfo::Kind kind = bindingTypeInfo.kind;
      LocalInfo::ValueKind valueKind = bindingTypeInfo.valueKind;
      LocalInfo::ValueKind mapKeyKind = bindingTypeInfo.mapKeyKind;
      LocalInfo::ValueKind mapValueKind = bindingTypeInfo.mapValueKind;
      LocalInfo info;
      info.isMutable = isBindingMutable(stmt);
      info.kind = kind;
      info.valueKind = valueKind;
      info.mapKeyKind = mapKeyKind;
      info.mapValueKind = mapValueKind;
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "soa_vector") {
          info.isSoaVector = true;
          break;
        }
      }
      if (!info.isSoaVector && init.kind == Expr::Kind::Name) {
        auto existing = localsIn.find(init.name);
        if (existing != localsIn.end() && existing->second.isSoaVector) {
          info.isSoaVector = true;
        }
      }
      if (!info.isSoaVector && init.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection) && collection == "soa_vector") {
          info.isSoaVector = true;
        }
      }
      setReferenceArrayInfo(stmt, info);
      applyStructArrayInfo(stmt, info);
      applyStructValueInfo(stmt, info);
      if (info.kind == LocalInfo::Kind::Value && info.valueKind == LocalInfo::ValueKind::Unknown &&
          info.structTypeName.empty()) {
        std::string inferredStruct = inferStructExprPath(init, localsIn);
        if (!inferredStruct.empty()) {
          info.structTypeName = inferredStruct;
        } else {
          info.valueKind = LocalInfo::ValueKind::Int32;
        }
      }
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "File") {
          info.isFileHandle = true;
          info.valueKind = LocalInfo::ValueKind::Int64;
        } else if (transform.name == "Result") {
          info.isResult = true;
          info.resultHasValue = (transform.templateArgs.size() == 2);
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          if (!transform.templateArgs.empty()) {
            info.resultErrorType = transform.templateArgs.back();
          }
        }
      }
      info.isFileError = isFileErrorBinding(stmt);
      valueKind = info.valueKind;

      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        std::string initStruct = inferStructExprPath(init, localsIn);
        if (initStruct.empty() || initStruct != info.structTypeName) {
          error = "struct binding initializer type mismatch on " + stmt.name;
          return false;
        }
        StructSlotLayout layout;
        if (!resolveStructSlotLayout(info.structTypeName, layout)) {
          return false;
        }
        const int32_t baseLocal = nextLocal;
        nextLocal += layout.totalSlots;
        info.index = nextLocal++;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        const int32_t srcPtrLocal = allocTempLocal();
        if (!emitExpr(init, localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          return false;
        }
        localsIn.emplace(stmt.name, info);
        return true;
      }

      if (valueKind == LocalInfo::ValueKind::String) {
        if (kind != LocalInfo::Kind::Value) {
          error = "native backend does not support string pointers or references";
          return false;
        }
        if (!ir_lowerer::emitStringStatementBindingInitializer(
                stmt,
                init,
                localsIn,
                nextLocal,
                function.instructions,
                [&](const Expr &bindingExpr) { return isBindingMutable(bindingExpr); },
                [&](const std::string &decoded) { return internString(decoded); },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return emitExpr(valueExpr, valueLocals);
                },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return inferExprKind(valueExpr, valueLocals);
                },
                [&]() { return allocTempLocal(); },
                [&](const Expr &entryArgsExpr, const LocalMap &valueLocals) {
                  return isEntryArgsName(entryArgsExpr, valueLocals);
                },
                [&]() { emitArrayIndexOutOfBounds(); },
                error)) {
          return false;
        }
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
      if (info.isFileHandle && !fileScopeStack.empty()) {
        fileScopeStack.back().push_back(info.index);
      }
      return true;
    }
    const auto uninitializedInitDropResult = ir_lowerer::tryEmitUninitializedStorageInitDropStatement(
        stmt,
        localsIn,
        function.instructions,
        [&](const Expr &storageExpr,
            const LocalMap &valueLocals,
            ir_lowerer::UninitializedStorageAccessInfo &accessOut,
            bool &resolvedOut) { return resolveUninitializedStorage(storageExpr, valueLocals, accessOut, resolvedOut); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&](const std::string &structPath, ir_lowerer::StructSlotLayoutInfo &layoutOut) {
          return resolveStructSlotLayout(structPath, layoutOut);
        },
        [&]() { return allocTempLocal(); },
        [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) {
          return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, slotCount);
        },
        error);
    if (uninitializedInitDropResult == ir_lowerer::UninitializedStorageInitDropEmitResult::Error) {
      return false;
    }
    if (uninitializedInitDropResult == ir_lowerer::UninitializedStorageInitDropEmitResult::Emitted) {
      return true;
    }
    const auto uninitializedTakeResult = ir_lowerer::tryEmitUninitializedStorageTakeStatement(
        stmt,
        localsIn,
        function.instructions,
        [&](const Expr &storageExpr,
            const LocalMap &valueLocals,
            ir_lowerer::UninitializedStorageAccessInfo &accessOut,
            bool &resolvedOut) { return resolveUninitializedStorage(storageExpr, valueLocals, accessOut, resolvedOut); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        error);
    if (uninitializedTakeResult == ir_lowerer::UninitializedStorageTakeEmitResult::Error) {
      return false;
    }
    if (uninitializedTakeResult == ir_lowerer::UninitializedStorageTakeEmitResult::Emitted) {
      return true;
    }
    const auto printPathSpaceResult = ir_lowerer::tryEmitPrintPathSpaceStatementBuiltin(
        stmt,
        localsIn,
        [&](const Expr &argExpr, const LocalMap &valueLocals, const ir_lowerer::PrintBuiltin &builtin) {
          return emitPrintArg(argExpr, valueLocals, builtin);
        },
        [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
        [&](const Expr &argExpr, const LocalMap &valueLocals) { return emitExpr(argExpr, valueLocals); },
        function.instructions,
        error);
    if (printPathSpaceResult == ir_lowerer::StatementPrintPathSpaceEmitResult::Error) {
      return false;
    }
    if (printPathSpaceResult == ir_lowerer::StatementPrintPathSpaceEmitResult::Emitted) {
      return true;
    }
    const std::optional<ir_lowerer::ReturnStatementInlineContext> returnInlineContext = [&]()
        -> std::optional<ir_lowerer::ReturnStatementInlineContext> {
      if (!activeInlineContext) {
        return std::nullopt;
      }
      return ir_lowerer::ReturnStatementInlineContext{
          activeInlineContext->returnsVoid,
          activeInlineContext->returnsArray,
          activeInlineContext->returnLocal,
          &activeInlineContext->returnJumps,
      };
    }();
    const auto returnResult = ir_lowerer::tryEmitReturnStatement(
        stmt,
        localsIn,
        function.instructions,
        returnInlineContext,
        returnsVoid,
        sawReturn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferArrayElementKind(valueExpr, valueLocals); },
        [&]() { emitFileScopeCleanupAll(); },
        error);
    if (returnResult == ir_lowerer::ReturnStatementEmitResult::Error) {
      return false;
    }
    if (returnResult == ir_lowerer::ReturnStatementEmitResult::Emitted) {
      return true;
    }
    const auto matchIfResult = ir_lowerer::tryEmitMatchIfStatement(
        stmt,
        localsIn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &blockExpr, LocalMap &blockLocals) { return emitBlock(blockExpr, blockLocals); },
        [&](const Expr &loweredStmt, LocalMap &statementLocals) { return emitStatement(loweredStmt, statementLocals); },
        function.instructions,
        error);
    if (matchIfResult == ir_lowerer::StatementMatchIfEmitResult::Error) {
      return false;
    }
    if (matchIfResult == ir_lowerer::StatementMatchIfEmitResult::Emitted) {
      return true;
    }
