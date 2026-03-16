  ir_lowerer::LowerReturnCallsEmitFileErrorWhyFn emitFileErrorWhy;
  if (!ir_lowerer::runLowerReturnCallsSetup(
          {
              .function = &function,
              .internString = internString,
          },
          emitFileErrorWhy,
          error)) {
    return false;
  }
  ir_lowerer::LowerExprEmitMovePassthroughCallFn emitMovePassthroughCall;
  ir_lowerer::LowerExprEmitUploadPassthroughCallFn emitUploadPassthroughCall;
  ir_lowerer::LowerExprEmitReadbackPassthroughCallFn emitReadbackPassthroughCall;
  if (!ir_lowerer::runLowerExprEmitSetup(
          {},
          emitMovePassthroughCall,
          emitUploadPassthroughCall,
          emitReadbackPassthroughCall,
          error)) {
    return false;
  }

  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (expr.isLambda) {
      error = "IR backends do not support lambdas";
      return false;
    }
    const auto moveResult = ir_lowerer::runLowerExprEmitMovePassthroughStep(
        expr,
        localsIn,
        emitMovePassthroughCall,
        [&](const Expr &argExpr, const LocalMap &argLocals) { return emitExpr(argExpr, argLocals); },
        error);
    if (moveResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
      return moveResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
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
        {
          std::string decoded;
          if (!ir_lowerer::parseLowererStringLiteral(expr.stringValue, decoded, error)) {
            return false;
          }
          int32_t index = internString(decoded);
          function.instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(index)});
          return true;
        }
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
        if (expr.isFieldAccess) {
          if (expr.args.size() != 1) {
            error = "field access requires a receiver";
            return false;
          }
          const Expr &receiver = expr.args.front();
          if (receiver.kind == Expr::Kind::Name && localsIn.find(receiver.name) == localsIn.end()) {
            std::string receiverStructPath;
            if (resolveStructTypeName(receiver.name, receiver.namespacePrefix, receiverStructPath)) {
              auto defIt = defMap.find(receiverStructPath);
              if (defIt == defMap.end() || defIt->second == nullptr) {
                error = "field access requires struct receiver";
                return false;
              }
              auto isStaticField = [](const Expr &stmt) {
                for (const auto &transform : stmt.transforms) {
                  if (transform.name == "static") {
                    return true;
                  }
                }
                return false;
              };
              for (const auto &stmt : defIt->second->statements) {
                if (!stmt.isBinding || !isStaticField(stmt) || stmt.name != expr.name) {
                  continue;
                }
                if (stmt.args.empty()) {
                  error = "static field missing initializer: " + receiverStructPath + "/" + expr.name;
                  return false;
                }
                return emitExpr(stmt.args.front(), localsIn);
              }
              error = "unknown struct field: " + expr.name;
              return false;
            }
          }
          std::string structPath = inferStructExprPath(receiver, localsIn);
          if (structPath.empty()) {
            error = "field access requires struct receiver";
            return false;
          }
          StructSlotFieldInfo fieldInfo;
          if (!resolveStructFieldSlot(structPath, expr.name, fieldInfo)) {
            error = "unknown struct field: " + expr.name;
            return false;
          }
          if (!emitExpr(receiver, localsIn)) {
            return false;
          }
          const int32_t ptrLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          std::string accessName;
          if (receiver.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiver, accessName) &&
              receiver.args.size() == 2 && receiver.args.front().kind == Expr::Kind::Name) {
            auto receiverIt = localsIn.find(receiver.args.front().name);
            if (receiverIt != localsIn.end() && receiverIt->second.isArgsPack &&
                receiverIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
                !receiverIt->second.structTypeName.empty()) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            }
          }
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          const uint64_t offsetBytes = static_cast<uint64_t>(fieldInfo.slotOffset) * IrSlotBytes;
          function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          if (fieldInfo.structPath.empty()) {
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        }
        if (!expr.isMethodCall &&
            (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) &&
            expr.args.size() == 1) {
          const bool isBorrow = isSimpleCallName(expr, "borrow");
          const Expr &storage = expr.args.front();
          UninitializedStorageAccess access;
          bool resolved = false;
          if (!resolveUninitializedStorage(storage, localsIn, access, resolved)) {
            return false;
          }
          if (resolved) {
            auto emitFieldPointer = [&](const Expr &receiver, const StructSlotFieldInfo &field, int32_t ptrLocal) -> bool {
              if (!emitExpr(receiver, localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              const uint64_t offsetBytes = static_cast<uint64_t>(field.slotOffset) * IrSlotBytes;
              if (offsetBytes != 0) {
                function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
                function.instructions.push_back({IrOpcode::AddI64, 0});
              }
              return true;
            };
            if (access.location == UninitializedStorageAccess::Location::Local) {
              const LocalInfo &storageInfo = *access.local;
              if (isBorrow) {
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(storageInfo.index)});
                return true;
              }
              if (!storageInfo.structTypeName.empty()) {
                StructSlotLayout layout;
                if (!resolveStructSlotLayout(storageInfo.structTypeName, layout)) {
                  return false;
                }
                const int32_t baseLocal = nextLocal;
                nextLocal += layout.totalSlots;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                const int32_t destPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
                const int32_t srcPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
                if (!emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, layout.totalSlots)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
                return true;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(storageInfo.index)});
              return true;
            }
            if (access.location == UninitializedStorageAccess::Location::Field) {
              const Expr &receiver = storage.args.front();
              const StructSlotFieldInfo &field = access.fieldSlot;
              const int32_t ptrLocal = allocTempLocal();
              if (!emitFieldPointer(receiver, field, ptrLocal)) {
                return false;
              }
              if (isBorrow) {
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
                return true;
              }
              if (!field.structPath.empty()) {
                const int32_t baseLocal = nextLocal;
                nextLocal += field.slotCount;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(field.slotCount - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                const int32_t destPtrLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
                if (!emitStructCopyFromPtrs(destPtrLocal, ptrLocal, field.slotCount)) {
                  return false;
                }
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(destPtrLocal)});
                return true;
              }
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              return true;
            }
          }
        }
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
          const bool returnsIntStatus = !currentReturnResult.has_value() && !returnsVoid;
          if (!currentOnError.has_value() && !returnsIntStatus) {
            error = "missing on_error for ? usage";
            return false;
          }
          if (!currentReturnResult.has_value() && returnsVoid) {
            error = "try requires Result or int return type";
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
              if (!returnsIntStatus) {
                error = "missing on_error for ? usage";
                return false;
              }
              if (activeInlineContext) {
                InlineContext &context = *activeInlineContext;
                if (context.returnLocal < 0) {
                  error = "native backend missing inline return local";
                  return false;
                }
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(context.returnLocal)});
                size_t jumpIndex = function.instructions.size();
                function.instructions.push_back({IrOpcode::Jump, 0});
                context.returnJumps.push_back(jumpIndex);
                return true;
              }
              emitFileScopeCleanupAll();
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              function.instructions.push_back({IrOpcode::ReturnI32, 0});
              return true;
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

            Expr handlerErrorExpr = errorExpr;
            std::string errorStructPath;
            if (resolveStructTypeName(handler.errorType, expr.namespacePrefix, errorStructPath)) {
              StructSlotLayoutInfo layout;
              if (!resolveStructSlotLayout(errorStructPath, layout)) {
                return false;
              }
              if (layout.fields.size() != 1 || !layout.fields.front().structPath.empty() ||
                  layout.fields.front().valueKind != LocalInfo::ValueKind::Int32) {
                error = "on_error requires int-backed error type";
                return false;
              }
              Expr ctorExpr;
              ctorExpr.kind = Expr::Kind::Call;
              ctorExpr.name = errorStructPath;
              ctorExpr.namespacePrefix = expr.namespacePrefix;
              ctorExpr.isMethodCall = false;
              ctorExpr.isBinding = false;
              ctorExpr.args.push_back(errorExpr);
              ctorExpr.argNames.push_back(std::nullopt);
              handlerErrorExpr = std::move(ctorExpr);
            }

            Expr callExpr;
            callExpr.kind = Expr::Kind::Call;
            callExpr.name = handler.handlerPath;
            callExpr.namespacePrefix = expr.namespacePrefix;
            callExpr.isMethodCall = false;
            callExpr.isBinding = false;
            callExpr.args.reserve(handler.boundArgs.size() + 1);
            callExpr.args.push_back(handlerErrorExpr);
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

            const bool returnHasValue = currentReturnResult.has_value() && currentReturnResult->hasValue;
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
        const auto resultOkCallResult = ir_lowerer::tryEmitResultOkCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return inferExprKind(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (resultOkCallResult == ir_lowerer::ResultOkMethodCallEmitResult::Emitted) {
          return true;
        }
        if (resultOkCallResult == ir_lowerer::ResultOkMethodCallEmitResult::Error) {
          return false;
        }
        const auto resultErrorCallResult = ir_lowerer::tryEmitResultErrorCall(
            expr,
            localsIn,
            resolveResultExprInfo,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (resultErrorCallResult == ir_lowerer::ResultErrorMethodCallEmitResult::Emitted) {
          return true;
        }
        if (resultErrorCallResult == ir_lowerer::ResultErrorMethodCallEmitResult::Error) {
          return false;
        }
        const auto resultWhyDispatchResult = ir_lowerer::tryEmitResultWhyDispatchCall(
            expr,
            localsIn,
            defMap,
            onErrorTempCounter,
            resolveResultExprInfo,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](const std::string &value) { return internString(value); },
            [&](const std::string &typeName, const std::string &nsPrefix, std::string &structPathOut) {
              return resolveStructTypeName(typeName, nsPrefix, structPathOut);
            },
            [&](const std::string &definitionPath, ReturnInfo &returnInfoOut) {
              return getReturnInfo && getReturnInfo(definitionPath, returnInfoOut);
            },
            [&](const Expr &bindingExpr) { return bindingKind(bindingExpr); },
            [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(structPath, layoutOut);
            },
            [&](const std::string &typeName) { return valueKindFromTypeName(typeName); },
            [&](const Expr &callExpr, const Definition &callee, const LocalMap &callLocals) {
              return emitInlineDefinitionCall(callExpr, callee, callLocals, true);
            },
            [&](int32_t emittedErrorLocal) { return emitFileErrorWhy(emittedErrorLocal); },
            error);
        if (resultWhyDispatchResult == ir_lowerer::ResultWhyDispatchEmitResult::Emitted) {
          return true;
        }
        if (resultWhyDispatchResult == ir_lowerer::ResultWhyDispatchEmitResult::Error) {
          return false;
        }
        const auto fileConstructorResult = ir_lowerer::tryEmitFileConstructorCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(valueExpr, localMap, stringIndexOut, lengthOut);
            },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (fileConstructorResult == ir_lowerer::FileConstructorCallEmitResult::Emitted) {
          return true;
        }
        if (fileConstructorResult == ir_lowerer::FileConstructorCallEmitResult::Error) {
          return false;
        }
        const auto fileHandleCallResult = ir_lowerer::tryEmitFileHandleMethodCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(valueExpr, localMap, stringIndexOut, lengthOut);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(valueExpr, localMap);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&]() { return function.instructions.size(); },
            [&](size_t instructionIndex, int32_t imm) { function.instructions[instructionIndex].imm = imm; },
            error);
        if (fileHandleCallResult == ir_lowerer::FileHandleMethodCallEmitResult::Emitted) {
          return true;
        }
        if (fileHandleCallResult == ir_lowerer::FileHandleMethodCallEmitResult::Error) {
          return false;
        }
        std::string gpuBuiltin;
        if (getBuiltinGpuName(expr, gpuBuiltin)) {
          return ir_lowerer::emitGpuBuiltinLoad(
              gpuBuiltin,
              [&](const char *localName) -> std::optional<int32_t> {
                auto it = localsIn.find(localName);
                if (it == localsIn.end()) {
                  return std::nullopt;
                }
                return it->second.index;
              },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              error);
        }
        const auto uploadReadbackResult = ir_lowerer::runLowerExprEmitUploadReadbackPassthroughStep(
            expr,
            localsIn,
            emitUploadPassthroughCall,
            emitReadbackPassthroughCall,
            [&](const Expr &argExpr, const LocalMap &argLocals) { return emitExpr(argExpr, argLocals); },
            error);
        if (uploadReadbackResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
          return uploadReadbackResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
        }
        const auto bufferBuiltinResult = ir_lowerer::tryEmitBufferBuiltinDispatchWithLocals(
            expr,
            localsIn,
            [&](const std::string &typeName) { return valueKindFromTypeName(typeName); },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(valueExpr, localMap);
            },
            [&](int32_t localCount) {
              const int32_t baseLocal = nextLocal;
              nextLocal += localCount;
              return baseLocal;
            },
            [&]() { return allocTempLocal(); },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            error);
        if (bufferBuiltinResult != ir_lowerer::BufferBuiltinDispatchResult::NotHandled) {
          return bufferBuiltinResult == ir_lowerer::BufferBuiltinDispatchResult::Emitted;
        }
        const auto inlineDispatchResult = ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            expr,
            localsIn,
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isArrayCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isStringCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isVectorCapacityCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return resolveMethodCallDefinition(callExpr, localMap);
            },
            [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
            [&](const Expr &callExpr, const Definition &callee, const ir_lowerer::LocalMap &localMap) {
              return emitInlineDefinitionCall(callExpr, callee, localMap, true);
            },
            error);
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Emitted) {
          return true;
        }
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Error) {
          return false;
        }
        const auto nativeTailResult = ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            expr,
            localsIn,
            [&](const Expr &callExpr, std::string &mathBuiltinName) {
              return getMathBuiltinName(callExpr, mathBuiltinName);
            },
            [&](const std::string &mathBuiltinName) {
              return ir_lowerer::isSupportedMathBuiltinName(mathBuiltinName);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isArrayCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isVectorCapacityCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isStringCountCall(callExpr, localMap);
            },
            [&](const Expr &targetExpr, const ir_lowerer::LocalMap &localMap) {
              return isEntryArgsName(targetExpr, localMap);
            },
            [&](const Expr &targetExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(targetExpr, localMap, stringIndexOut, lengthOut);
            },
            stringTable.size(),
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](const Expr &targetCallExpr, ir_lowerer::MapAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              const Definition *callee = resolveDefinitionCall(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                return false;
              }
              if (collectionName != "map" || collectionArgs.size() != 2) {
                return false;
              }
              targetInfoOut.isMapTarget = true;
              targetInfoOut.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs[0]);
              targetInfoOut.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs[1]);
              return true;
            },
            [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              if (targetCallExpr.isFieldAccess && targetCallExpr.args.size() == 1) {
                const std::string receiverStruct = inferStructExprPath(targetCallExpr.args.front(), localsIn);
                if (!receiverStruct.empty()) {
                  StructSlotFieldInfo fieldInfo;
                  if (resolveStructFieldSlot(receiverStruct, targetCallExpr.name, fieldInfo) &&
                      fieldInfo.structPath == "/vector") {
                    targetInfoOut.isArrayOrVectorTarget = true;
                    targetInfoOut.isVectorTarget = true;
                    targetInfoOut.elemKind = fieldInfo.valueKind;
                    return true;
                  }
                }
              }
              const Definition *callee = resolveDefinitionCall(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                return false;
              }
              if ((collectionName != "array" && collectionName != "vector") || collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              return true;
            },
            [&](const Expr &callExpr, std::string &builtinName) {
              PrintBuiltin printBuiltin;
              if (!getPrintBuiltin(callExpr, printBuiltin)) {
                return false;
              }
              builtinName = printBuiltin.name;
              return true;
            },
            [&](const Expr &lookupKeyExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(lookupKeyExpr, localMap);
            },
            [&]() { return allocTempLocal(); },
            [&]() { emitStringIndexOutOfBounds(); },
            [&]() { emitMapKeyNotFound(); },
            [&]() { emitArrayIndexOutOfBounds(); },
            [&]() { return function.instructions.size(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { function.instructions[instructionIndex].imm = imm; },
            error);
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Emitted) {
          return true;
        }
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Error) {
          return false;
        }
