  auto emitFileErrorWhy = [&](int32_t errorLocal) -> bool {
    ir_lowerer::emitFileErrorWhy(function, errorLocal, internString);
    return true;
  };

  emitExpr = [&](const Expr &expr, const LocalMap &localsIn) -> bool {
    if (expr.isLambda) {
      error = "IR backends do not support lambdas";
      return false;
    }
    if (!expr.isMethodCall) {
      const auto moveResult = ir_lowerer::tryEmitUnaryPassthroughCall(
          expr,
          "move",
          [&](const Expr &argExpr) { return emitExpr(argExpr, localsIn); },
          error);
      if (moveResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
        return moveResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
      }
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
        const auto resultWhyCallResult = ir_lowerer::tryEmitResultWhyCall(
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
        if (resultWhyCallResult == ir_lowerer::ResultWhyMethodCallEmitResult::Emitted) {
          return true;
        }
        if (resultWhyCallResult == ir_lowerer::ResultWhyMethodCallEmitResult::Error) {
          return false;
        }
        const auto fileErrorWhyResult = ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](int32_t errorLocal) { emitFileErrorWhy(errorLocal); },
            error);
        if (fileErrorWhyResult == ir_lowerer::FileErrorWhyCallEmitResult::Emitted) {
          return true;
        }
        if (fileErrorWhyResult == ir_lowerer::FileErrorWhyCallEmitResult::Error) {
          return false;
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
          return ir_lowerer::emitFileOpenCall(
              expr.templateArgs.front(),
              stringIndex,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              error);
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
        const auto uploadResult = ir_lowerer::tryEmitUnaryPassthroughCall(
              expr,
              "upload",
              [&](const Expr &argExpr) { return emitExpr(argExpr, localsIn); },
              error);
        if (uploadResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
          return uploadResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
        }
        const auto readbackResult = ir_lowerer::tryEmitUnaryPassthroughCall(
              expr,
              "readback",
              [&](const Expr &argExpr) { return emitExpr(argExpr, localsIn); },
              error);
        if (readbackResult != ir_lowerer::UnaryPassthroughCallResult::NotMatched) {
          return readbackResult == ir_lowerer::UnaryPassthroughCallResult::Emitted;
        }
        const auto bufferBuiltinResult = ir_lowerer::tryEmitBufferBuiltinCall(
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
        if (bufferBuiltinResult != ir_lowerer::BufferBuiltinCallEmitResult::NotMatched) {
          return bufferBuiltinResult == ir_lowerer::BufferBuiltinCallEmitResult::Emitted;
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
        const auto nativeTailResult = ir_lowerer::tryEmitNativeCallTailDispatch(
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
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
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
