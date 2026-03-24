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
          const bool isAggregateReference =
              it->second.kind == LocalInfo::Kind::Reference &&
              (!it->second.structTypeName.empty() || it->second.referenceToArray ||
               it->second.referenceToVector || it->second.referenceToMap || it->second.referenceToBuffer);
          if (it->second.kind == LocalInfo::Kind::Reference && !isAggregateReference) {
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
            auto emitIndirectPointer = [&](const Expr &pointerExpr, int32_t ptrLocal) -> bool {
              if (pointerExpr.kind == Expr::Kind::Name) {
                auto it = localsIn.find(pointerExpr.name);
                if (it != localsIn.end() &&
                    (it->second.kind == LocalInfo::Kind::Pointer || it->second.kind == LocalInfo::Kind::Reference)) {
                  function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
                  function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
                  return true;
                }
              }
              if (!emitExpr(pointerExpr, localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
              return true;
            };
            if (access.location == UninitializedStorageAccess::Location::Local) {
              const LocalInfo &storageInfo = *access.local;
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
            if (access.location == UninitializedStorageAccess::Location::Indirect) {
              const int32_t ptrLocal = allocTempLocal();
              if (access.pointerExpr == nullptr || !emitIndirectPointer(*access.pointerExpr, ptrLocal)) {
                return false;
              }
              if (!access.typeInfo.structPath.empty()) {
                StructSlotLayout layout;
                if (!resolveStructSlotLayout(access.typeInfo.structPath, layout)) {
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
                if (!emitStructCopyFromPtrs(destPtrLocal, ptrLocal, layout.totalSlots)) {
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
            if (!ir_lowerer::isSupportedPackedResultValueInfo(
                    resultInfo,
                    [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                      return resolveStructSlotLayout(structPath, layoutOut);
                    })) {
              error = ir_lowerer::unsupportedPackedResultValueKindError("try");
              return false;
            }
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
            if (!resultInfo.valueStructType.empty()) {
              bool isPackedSingleSlot = false;
              LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
              int32_t slotCount = 0;
              if (!ir_lowerer::resolveSupportedResultStructPayloadInfo(
                      resultInfo.valueStructType,
                      [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                        return resolveStructSlotLayout(structPath, layoutOut);
                      },
                      isPackedSingleSlot,
                      packedStructKind,
                      slotCount)) {
                error = ir_lowerer::unsupportedPackedResultValueKindError("try");
                return false;
              }
              (void)packedStructKind;
              (void)slotCount;
              if (isPackedSingleSlot) {
                const int32_t baseLocal = nextLocal;
                ++nextLocal;
                const int32_t ptrLocal = nextLocal++;
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
                function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
              }
            }
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
        auto isFileHandleExpr = [&](const Expr &valueExpr, const LocalMap &valueLocals) {
          if (valueExpr.kind != Expr::Kind::Name) {
            return false;
          }
          auto localIt = valueLocals.find(valueExpr.name);
          return localIt != valueLocals.end() && localIt->second.isFileHandle;
        };
        auto resolveResultLambdaValueExpr =
            [&](const Expr &lambdaExpr,
                LocalMap &lambdaLocals,
                const std::string &builtinName,
                const Expr *&mappedValueExprOut) -> bool {
          mappedValueExprOut = nullptr;
          for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
            const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
            const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
            if (bodyExpr.isBinding) {
              if (!emitStatement(bodyExpr, lambdaLocals)) {
                return false;
              }
              continue;
            }
            if (isSimpleCallName(bodyExpr, "return")) {
              if (bodyExpr.args.size() != 1) {
                error = "IR backends require " + builtinName + " lambda returns with a value";
                return false;
              }
              if (!isLast) {
                error = "IR backends require " + builtinName + " lambda returns to be final";
                return false;
              }
              mappedValueExprOut = &bodyExpr.args.front();
              break;
            }
            if (!isLast) {
              if (!emitStatement(bodyExpr, lambdaLocals)) {
                return false;
              }
              continue;
            }
            mappedValueExprOut = &bodyExpr;
          }
          if (mappedValueExprOut == nullptr) {
            error = "IR backends require " + builtinName + " lambdas to produce a value";
            return false;
          }
          return true;
        };
        auto resolvePackedResultPayloadInfo = [&](const Expr &valueExpr,
                                                  const LocalMap &valueLocals,
                                                  LocalInfo::ValueKind &packedKindOut,
                                                  std::string &structTypeOut) -> bool {
          packedKindOut = inferExprKind(valueExpr, valueLocals);
          structTypeOut.clear();
          auto isBufferHandleCall = [&](const Expr &candidate) {
            if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.isBinding) {
              return false;
            }
            return candidate.name == "Buffer" || candidate.name == "/std/gfx/Buffer" ||
                   candidate.name == "/std/gfx/experimental/Buffer" ||
                   candidate.name.rfind("/std/gfx/Buffer__t", 0) == 0 ||
                   candidate.name.rfind("/std/gfx/experimental/Buffer__t", 0) == 0;
          };
          auto resolveCollectionPayload = [&](LocalInfo::Kind &collectionKindOut,
                                             LocalInfo::ValueKind &collectionValueKindOut) {
            collectionKindOut = LocalInfo::Kind::Value;
            collectionValueKindOut = LocalInfo::ValueKind::Unknown;
            if (valueExpr.kind == Expr::Kind::Name) {
              auto localIt = valueLocals.find(valueExpr.name);
              if (localIt == valueLocals.end() ||
                  !ir_lowerer::isSupportedPackedResultCollectionKind(localIt->second.kind)) {
                return false;
              }
              collectionKindOut = localIt->second.kind;
              collectionValueKindOut = localIt->second.valueKind;
              return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
            }
            if (valueExpr.kind != Expr::Kind::Call) {
              return false;
            }
            std::string collectionName;
            if (!valueExpr.isMethodCall && getBuiltinCollectionName(valueExpr, collectionName) &&
                ((collectionName == "array" || collectionName == "vector") && valueExpr.templateArgs.size() == 1 ||
                 (collectionName == "Buffer" && valueExpr.templateArgs.size() == 1) ||
                 (collectionName == "map" && valueExpr.templateArgs.size() == 2))) {
              if (collectionName == "array") {
                collectionKindOut = LocalInfo::Kind::Array;
              } else if (collectionName == "vector") {
                collectionKindOut = LocalInfo::Kind::Vector;
              } else if (collectionName == "Buffer") {
                collectionKindOut = LocalInfo::Kind::Buffer;
              } else if (collectionName == "map") {
                if (valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front())) ==
                    LocalInfo::ValueKind::Unknown) {
                  return false;
                }
                collectionKindOut = LocalInfo::Kind::Map;
              } else {
                return false;
              }
              collectionValueKindOut = valueKindFromTypeName(
                  trimTemplateTypeText(collectionKindOut == LocalInfo::Kind::Map ? valueExpr.templateArgs.back()
                                                                                : valueExpr.templateArgs.front()));
              return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
            }
            if (!valueExpr.isMethodCall && isBufferHandleCall(valueExpr) && valueExpr.templateArgs.size() == 1) {
              collectionKindOut = LocalInfo::Kind::Buffer;
              collectionValueKindOut = valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front()));
              return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
            }
            const Definition *callee = resolveDefinitionCall(valueExpr);
            if (callee == nullptr) {
              return false;
            }
            std::string declaredCollection;
            std::vector<std::string> declaredCollectionArgs;
            if (!ir_lowerer::inferDeclaredReturnCollection(*callee, declaredCollection, declaredCollectionArgs)) {
              return false;
            }
            if (declaredCollection == "array") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Array;
            } else if (declaredCollection == "vector") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Vector;
            } else if (declaredCollection == "map") {
              if (declaredCollectionArgs.size() != 2) {
                return false;
              }
              if (valueKindFromTypeName(trimTemplateTypeText(declaredCollectionArgs.front())) ==
                  LocalInfo::ValueKind::Unknown) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Map;
            } else if (declaredCollection == "Buffer") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Buffer;
            } else {
              return false;
            }
            collectionValueKindOut = valueKindFromTypeName(
                trimTemplateTypeText(collectionKindOut == LocalInfo::Kind::Map ? declaredCollectionArgs.back()
                                                                              : declaredCollectionArgs.front()));
            return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
          };
          LocalInfo::Kind collectionKind = LocalInfo::Kind::Value;
          LocalInfo::ValueKind collectionValueKind = LocalInfo::ValueKind::Unknown;
          if (resolveCollectionPayload(collectionKind, collectionValueKind) &&
              ir_lowerer::isSupportedPackedResultCollectionKind(collectionKind)) {
            packedKindOut = collectionValueKind;
            return true;
          }
          if (isFileHandleExpr(valueExpr, valueLocals) && packedKindOut == LocalInfo::ValueKind::Int64) {
            return true;
          }
          if (ir_lowerer::isSupportedPackedResultValueKind(packedKindOut)) {
            return true;
          }
          const std::string inferredStructType = inferStructExprPath(valueExpr, valueLocals);
          bool isPackedSingleSlot = false;
          LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
          int32_t slotCount = 0;
          if (ir_lowerer::resolveSupportedResultStructPayloadInfo(
                  inferredStructType,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  isPackedSingleSlot,
                  packedStructKind,
                  slotCount)) {
            (void)slotCount;
            structTypeOut = inferredStructType;
            packedKindOut = isPackedSingleSlot ? packedStructKind : LocalInfo::ValueKind::Unknown;
            return true;
          }
          packedKindOut = LocalInfo::ValueKind::Unknown;
          structTypeOut.clear();
          return false;
        };
        auto materializePackedResultStructLocal = [&](int32_t payloadLocal,
                                                      const std::string &structType,
                                                      LocalInfo &paramInfo) -> bool {
          bool isPackedSingleSlot = false;
          LocalInfo::ValueKind packedStructKind = LocalInfo::ValueKind::Unknown;
          int32_t slotCount = 0;
          if (!ir_lowerer::resolveSupportedResultStructPayloadInfo(
                  structType,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  isPackedSingleSlot,
                  packedStructKind,
                  slotCount)) {
            return false;
          }
          (void)packedStructKind;
          const int32_t baseLocal = nextLocal;
          nextLocal += slotCount;
          const int32_t ptrLocal = nextLocal++;
          function.instructions.push_back(
              {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(slotCount - 1))});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          if (isPackedSingleSlot) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
          } else if (!emitStructCopyFromPtrs(ptrLocal, payloadLocal, slotCount)) {
            return false;
          }
          paramInfo.index = ptrLocal;
          paramInfo.kind = LocalInfo::Kind::Value;
          paramInfo.valueKind = LocalInfo::ValueKind::Int64;
          paramInfo.structTypeName = structType;
          return true;
        };
        auto emitPackedResultValueExpr = [&](const Expr &valueExpr,
                                             const LocalMap &valueLocals,
                                             const std::string &builtinName) -> bool {
          LocalInfo::ValueKind packedValueKind = LocalInfo::ValueKind::Unknown;
          std::string structType;
          if (!resolvePackedResultPayloadInfo(valueExpr, valueLocals, packedValueKind, structType)) {
            error = ir_lowerer::unsupportedPackedResultValueKindError(builtinName);
            return false;
          }
          if (!emitExpr(valueExpr, valueLocals)) {
            return false;
          }
          if (!structType.empty() && packedValueKind != LocalInfo::ValueKind::Unknown) {
            const int32_t ptrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        };
        auto tryEmitResultMapCall = [&](const Expr &callExpr, const LocalMap &callLocals) -> std::optional<bool> {
          if (!(callExpr.isMethodCall && callExpr.name == "map" && callExpr.args.size() == 3 &&
                callExpr.args.front().kind == Expr::Kind::Name && callExpr.args.front().name == "Result")) {
            return std::nullopt;
          }

          ir_lowerer::ResultExprInfo sourceResultInfo;
          if (!resolveResultExprInfo ||
              !resolveResultExprInfo(callExpr.args[1], callLocals, sourceResultInfo) ||
              !sourceResultInfo.isResult) {
            error = "Result.map requires Result argument";
            return false;
          }
          if (!sourceResultInfo.hasValue) {
            error = "Result.map requires value Result";
            return false;
          }
          if (!ir_lowerer::isSupportedPackedResultValueInfo(
                  sourceResultInfo,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  })) {
            error = ir_lowerer::unsupportedPackedResultValueKindError("Result.map");
            return false;
          }

          const Expr &lambdaExpr = callExpr.args[2];
          if (!lambdaExpr.isLambda) {
            error = "Result.map requires a lambda argument";
            return false;
          }
          if (lambdaExpr.args.size() != 1) {
            error = "Result.map requires a single-parameter lambda";
            return false;
          }
          if (lambdaExpr.bodyArguments.empty()) {
            error = "IR backends require Result.map lambda bodies";
            return false;
          }

          if (!emitExpr(callExpr.args[1], callLocals)) {
            return false;
          }
          const int32_t resultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});

          const int32_t errorLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::DivI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t payloadLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::MulI64, 0});
          function.instructions.push_back({IrOpcode::SubI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadLocal)});

          LocalMap lambdaLocals = callLocals;
          LocalInfo paramInfo;
          if (ir_lowerer::isSupportedPackedResultCollectionKind(sourceResultInfo.valueCollectionKind)) {
            paramInfo.index = payloadLocal;
            paramInfo.kind = sourceResultInfo.valueCollectionKind;
            paramInfo.valueKind = sourceResultInfo.valueKind;
            if (sourceResultInfo.valueCollectionKind == LocalInfo::Kind::Map) {
              paramInfo.mapKeyKind = sourceResultInfo.valueMapKeyKind;
              paramInfo.mapValueKind = sourceResultInfo.valueKind;
            }
          } else if (!sourceResultInfo.valueStructType.empty()) {
            if (!materializePackedResultStructLocal(payloadLocal, sourceResultInfo.valueStructType, paramInfo)) {
              error = ir_lowerer::unsupportedPackedResultValueKindError("Result.map");
              return false;
            }
          } else {
            paramInfo.index = payloadLocal;
            paramInfo.kind = LocalInfo::Kind::Value;
            paramInfo.valueKind = sourceResultInfo.valueKind;
            paramInfo.isFileHandle = sourceResultInfo.valueIsFileHandle;
          }
          if (sourceResultInfo.valueCollectionKind == LocalInfo::Kind::Value &&
              sourceResultInfo.valueStructType.empty() &&
              sourceResultInfo.valueKind == LocalInfo::ValueKind::String) {
            paramInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
          }
          lambdaLocals[lambdaExpr.args.front().name] = paramInfo;

          const Expr *mappedValueExpr = nullptr;
          if (!resolveResultLambdaValueExpr(lambdaExpr, lambdaLocals, "Result.map", mappedValueExpr)) {
            return false;
          }

          if (!emitPackedResultValueExpr(*mappedValueExpr, lambdaLocals, "Result.map")) {
            return false;
          }
          const size_t jumpEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          const size_t errorIndex = function.instructions.size();
          function.instructions[jumpError].imm = static_cast<int32_t>(errorIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});

          const size_t endIndex = function.instructions.size();
          function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          return true;
        };
        auto tryEmitResultAndThenCall = [&](const Expr &callExpr, const LocalMap &callLocals) -> std::optional<bool> {
          if (!(callExpr.isMethodCall && callExpr.name == "and_then" && callExpr.args.size() == 3 &&
                callExpr.args.front().kind == Expr::Kind::Name && callExpr.args.front().name == "Result")) {
            return std::nullopt;
          }

          ir_lowerer::ResultExprInfo sourceResultInfo;
          if (!resolveResultExprInfo ||
              !resolveResultExprInfo(callExpr.args[1], callLocals, sourceResultInfo) ||
              !sourceResultInfo.isResult) {
            error = "Result.and_then requires Result argument";
            return false;
          }
          if (!sourceResultInfo.hasValue) {
            error = "Result.and_then requires value Result";
            return false;
          }
          if (!ir_lowerer::isSupportedPackedResultValueInfo(
                  sourceResultInfo,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  })) {
            error = ir_lowerer::unsupportedPackedResultValueKindError("Result.and_then");
            return false;
          }

          const Expr &lambdaExpr = callExpr.args[2];
          if (!lambdaExpr.isLambda) {
            error = "Result.and_then requires a lambda argument";
            return false;
          }
          if (lambdaExpr.args.size() != 1) {
            error = "Result.and_then requires a single-parameter lambda";
            return false;
          }
          if (lambdaExpr.bodyArguments.empty()) {
            error = "IR backends require Result.and_then lambda bodies";
            return false;
          }

          if (!emitExpr(callExpr.args[1], callLocals)) {
            return false;
          }
          const int32_t resultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(resultLocal)});

          const int32_t errorLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::DivI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t payloadLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::MulI64, 0});
          function.instructions.push_back({IrOpcode::SubI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(payloadLocal)});

          LocalMap lambdaLocals = callLocals;
          LocalInfo paramInfo;
          if (ir_lowerer::isSupportedPackedResultCollectionKind(sourceResultInfo.valueCollectionKind)) {
            paramInfo.index = payloadLocal;
            paramInfo.kind = sourceResultInfo.valueCollectionKind;
            paramInfo.valueKind = sourceResultInfo.valueKind;
            if (sourceResultInfo.valueCollectionKind == LocalInfo::Kind::Map) {
              paramInfo.mapKeyKind = sourceResultInfo.valueMapKeyKind;
              paramInfo.mapValueKind = sourceResultInfo.valueKind;
            }
          } else if (!sourceResultInfo.valueStructType.empty()) {
            if (!materializePackedResultStructLocal(payloadLocal, sourceResultInfo.valueStructType, paramInfo)) {
              error = ir_lowerer::unsupportedPackedResultValueKindError("Result.and_then");
              return false;
            }
          } else {
            paramInfo.index = payloadLocal;
            paramInfo.kind = LocalInfo::Kind::Value;
            paramInfo.valueKind = sourceResultInfo.valueKind;
            paramInfo.isFileHandle = sourceResultInfo.valueIsFileHandle;
          }
          if (sourceResultInfo.valueCollectionKind == LocalInfo::Kind::Value &&
              sourceResultInfo.valueStructType.empty() &&
              sourceResultInfo.valueKind == LocalInfo::ValueKind::String) {
            paramInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
          }
          lambdaLocals[lambdaExpr.args.front().name] = paramInfo;

          const Expr *chainedResultExpr = nullptr;
          if (!resolveResultLambdaValueExpr(lambdaExpr, lambdaLocals, "Result.and_then", chainedResultExpr)) {
            return false;
          }

          ir_lowerer::ResultExprInfo chainedResultInfo;
          if (!resolveResultExprInfo ||
              !resolveResultExprInfo(*chainedResultExpr, lambdaLocals, chainedResultInfo) ||
              !chainedResultInfo.isResult) {
            error = "IR backends require Result.and_then lambdas to produce Result values";
            return false;
          }
          if (chainedResultInfo.hasValue &&
              !ir_lowerer::isSupportedPackedResultValueInfo(
                  chainedResultInfo,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  })) {
            error = ir_lowerer::unsupportedPackedResultValueKindError("Result.and_then");
            return false;
          }
          if (!emitExpr(*chainedResultExpr, lambdaLocals)) {
            return false;
          }
          const size_t jumpEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          const size_t errorIndex = function.instructions.size();
          function.instructions[jumpError].imm = static_cast<int32_t>(errorIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});

          const size_t endIndex = function.instructions.size();
          function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          return true;
        };
        auto tryEmitResultMap2Call = [&](const Expr &callExpr, const LocalMap &callLocals) -> std::optional<bool> {
          if (!(callExpr.isMethodCall && callExpr.name == "map2" && callExpr.args.size() == 4 &&
                callExpr.args.front().kind == Expr::Kind::Name && callExpr.args.front().name == "Result")) {
            return std::nullopt;
          }

          ir_lowerer::ResultExprInfo leftResultInfo;
          ir_lowerer::ResultExprInfo rightResultInfo;
          if (!resolveResultExprInfo ||
              !resolveResultExprInfo(callExpr.args[1], callLocals, leftResultInfo) ||
              !leftResultInfo.isResult ||
              !resolveResultExprInfo(callExpr.args[2], callLocals, rightResultInfo) ||
              !rightResultInfo.isResult) {
            error = "Result.map2 requires Result arguments";
            return false;
          }
          if (!leftResultInfo.hasValue || !rightResultInfo.hasValue) {
            error = "Result.map2 requires value Results";
            return false;
          }
          if (!leftResultInfo.errorType.empty() && !rightResultInfo.errorType.empty() &&
              leftResultInfo.errorType != rightResultInfo.errorType) {
            error = "Result.map2 requires matching error types";
            return false;
          }
          if (!ir_lowerer::isSupportedPackedResultValueInfo(
                  leftResultInfo,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  }) ||
              !ir_lowerer::isSupportedPackedResultValueInfo(
                  rightResultInfo,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  })) {
            error = ir_lowerer::unsupportedPackedResultValueKindError("Result.map2");
            return false;
          }

          const Expr &lambdaExpr = callExpr.args[3];
          if (!lambdaExpr.isLambda) {
            error = "Result.map2 requires a lambda argument";
            return false;
          }
          if (lambdaExpr.args.size() != 2) {
            error = "Result.map2 requires a two-parameter lambda";
            return false;
          }
          if (lambdaExpr.bodyArguments.empty()) {
            error = "IR backends require Result.map2 lambda bodies";
            return false;
          }

          if (!emitExpr(callExpr.args[1], callLocals)) {
            return false;
          }
          const int32_t leftResultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(leftResultLocal)});

          const int32_t leftErrorLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftResultLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::DivI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(leftErrorLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftErrorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpLeftError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          if (!emitExpr(callExpr.args[2], callLocals)) {
            return false;
          }
          const int32_t rightResultLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(rightResultLocal)});

          const int32_t rightErrorLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightResultLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::DivI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(rightErrorLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightErrorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpRightError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t leftPayloadLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftResultLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftErrorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::MulI64, 0});
          function.instructions.push_back({IrOpcode::SubI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(leftPayloadLocal)});

          const int32_t rightPayloadLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightResultLocal)});
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightErrorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
          function.instructions.push_back({IrOpcode::MulI64, 0});
          function.instructions.push_back({IrOpcode::SubI64, 0});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(rightPayloadLocal)});

          LocalMap lambdaLocals = callLocals;
          LocalInfo leftParamInfo;
          if (ir_lowerer::isSupportedPackedResultCollectionKind(leftResultInfo.valueCollectionKind)) {
            leftParamInfo.index = leftPayloadLocal;
            leftParamInfo.kind = leftResultInfo.valueCollectionKind;
            leftParamInfo.valueKind = leftResultInfo.valueKind;
            if (leftResultInfo.valueCollectionKind == LocalInfo::Kind::Map) {
              leftParamInfo.mapKeyKind = leftResultInfo.valueMapKeyKind;
              leftParamInfo.mapValueKind = leftResultInfo.valueKind;
            }
          } else if (!leftResultInfo.valueStructType.empty()) {
            if (!materializePackedResultStructLocal(leftPayloadLocal, leftResultInfo.valueStructType, leftParamInfo)) {
              error = ir_lowerer::unsupportedPackedResultValueKindError("Result.map2");
              return false;
            }
          } else {
            leftParamInfo.index = leftPayloadLocal;
            leftParamInfo.kind = LocalInfo::Kind::Value;
            leftParamInfo.valueKind = leftResultInfo.valueKind;
            leftParamInfo.isFileHandle = leftResultInfo.valueIsFileHandle;
          }
          if (leftResultInfo.valueCollectionKind == LocalInfo::Kind::Value &&
              leftResultInfo.valueStructType.empty() &&
              leftResultInfo.valueKind == LocalInfo::ValueKind::String) {
            leftParamInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
          }
          lambdaLocals[lambdaExpr.args[0].name] = leftParamInfo;

          LocalInfo rightParamInfo;
          if (ir_lowerer::isSupportedPackedResultCollectionKind(rightResultInfo.valueCollectionKind)) {
            rightParamInfo.index = rightPayloadLocal;
            rightParamInfo.kind = rightResultInfo.valueCollectionKind;
            rightParamInfo.valueKind = rightResultInfo.valueKind;
            if (rightResultInfo.valueCollectionKind == LocalInfo::Kind::Map) {
              rightParamInfo.mapKeyKind = rightResultInfo.valueMapKeyKind;
              rightParamInfo.mapValueKind = rightResultInfo.valueKind;
            }
          } else if (!rightResultInfo.valueStructType.empty()) {
            if (!materializePackedResultStructLocal(rightPayloadLocal, rightResultInfo.valueStructType, rightParamInfo)) {
              error = ir_lowerer::unsupportedPackedResultValueKindError("Result.map2");
              return false;
            }
          } else {
            rightParamInfo.index = rightPayloadLocal;
            rightParamInfo.kind = LocalInfo::Kind::Value;
            rightParamInfo.valueKind = rightResultInfo.valueKind;
            rightParamInfo.isFileHandle = rightResultInfo.valueIsFileHandle;
          }
          if (rightResultInfo.valueCollectionKind == LocalInfo::Kind::Value &&
              rightResultInfo.valueStructType.empty() &&
              rightResultInfo.valueKind == LocalInfo::ValueKind::String) {
            rightParamInfo.stringSource = LocalInfo::StringSource::RuntimeIndex;
          }
          lambdaLocals[lambdaExpr.args[1].name] = rightParamInfo;

          const Expr *mappedValueExpr = nullptr;
          if (!resolveResultLambdaValueExpr(lambdaExpr, lambdaLocals, "Result.map2", mappedValueExpr)) {
            return false;
          }

          if (!emitPackedResultValueExpr(*mappedValueExpr, lambdaLocals, "Result.map2")) {
            return false;
          }
          const size_t jumpEnd = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          const size_t leftErrorIndex = function.instructions.size();
          function.instructions[jumpLeftError].imm = static_cast<int32_t>(leftErrorIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(leftResultLocal)});
          const size_t jumpAfterLeftError = function.instructions.size();
          function.instructions.push_back({IrOpcode::Jump, 0});

          const size_t rightErrorIndex = function.instructions.size();
          function.instructions[jumpRightError].imm = static_cast<int32_t>(rightErrorIndex);
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightResultLocal)});

          const size_t endIndex = function.instructions.size();
          function.instructions[jumpEnd].imm = static_cast<int32_t>(endIndex);
          function.instructions[jumpAfterLeftError].imm = static_cast<int32_t>(endIndex);
          return true;
        };
        if (const auto resultMapCallResult = tryEmitResultMapCall(expr, localsIn);
            resultMapCallResult.has_value()) {
          return *resultMapCallResult;
        }
        if (const auto resultAndThenCallResult = tryEmitResultAndThenCall(expr, localsIn);
            resultAndThenCallResult.has_value()) {
          return *resultAndThenCallResult;
        }
        if (const auto resultMap2CallResult = tryEmitResultMap2Call(expr, localsIn);
            resultMap2CallResult.has_value()) {
          return *resultMap2CallResult;
        }
        const auto resultOkCallResult = ir_lowerer::tryEmitResultOkCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return inferExprKind(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return inferStructExprPath(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr) { return resolveDefinitionCall(valueExpr); },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return isFileHandleExpr(valueExpr, valueLocals);
            },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(structPath, layoutOut);
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
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(valueExpr, localMap);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return isEntryArgsName(valueExpr, localMap);
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
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              if (!callExpr.args.empty() && callExpr.args.front().kind == Expr::Kind::Name &&
                  callExpr.args.front().name == "self" && localMap.find("self") != localMap.end()) {
                return false;
              }
              const Definition *callee = resolveMethodCallDefinition(callExpr, localMap);
              if (callee == nullptr) {
                return false;
              }
              return callee->fullPath.rfind("/File/write", 0) == 0 ||
                     callee->fullPath.rfind("/File/write_line", 0) == 0 ||
                     callee->fullPath.rfind("/File/close", 0) == 0;
            },
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
        auto rewriteStdlibMapConstructorExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
            return false;
          }
          const Definition *callee = resolveDefinitionCall(callExpr);
          if (callee == nullptr) {
            return false;
          }
          auto matchesWrapperPath = [&](std::string_view basePath) {
            return callee->fullPath == basePath ||
                   callee->fullPath.rfind(std::string(basePath) + "__t", 0) == 0;
          };
          const bool isStdlibMapConstructor =
              matchesWrapperPath("/std/collections/map/map") ||
              matchesWrapperPath("/std/collections/mapNew") ||
              matchesWrapperPath("/std/collections/mapSingle") ||
              matchesWrapperPath("/std/collections/mapDouble") ||
              matchesWrapperPath("/std/collections/mapPair") ||
              matchesWrapperPath("/std/collections/mapTriple") ||
              matchesWrapperPath("/std/collections/mapQuad") ||
              matchesWrapperPath("/std/collections/mapQuint") ||
              matchesWrapperPath("/std/collections/mapSext") ||
              matchesWrapperPath("/std/collections/mapSept") ||
              matchesWrapperPath("/std/collections/mapOct");
          if (!isStdlibMapConstructor) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = "/map/map";
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          if (rewrittenExpr.templateArgs.empty()) {
            for (const auto &transform : callee->transforms) {
              if (transform.name != "return" || transform.templateArgs.size() != 1) {
                continue;
              }
              std::string base;
              std::string argList;
              if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, argList)) {
                break;
              }
              if (normalizeCollectionBindingTypeName(base) != "map") {
                break;
              }
              std::vector<std::string> templateArgs;
              if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 2) {
                break;
              }
              templateArgs[0] = trimTemplateTypeText(templateArgs[0]);
              templateArgs[1] = trimTemplateTypeText(templateArgs[1]);
              rewrittenExpr.templateArgs = std::move(templateArgs);
              break;
            }
          }
          return true;
        };
        Expr rewrittenStdlibMapConstructorExpr;
        if (rewriteStdlibMapConstructorExpr(expr, rewrittenStdlibMapConstructorExpr)) {
          return emitExpr(rewrittenStdlibMapConstructorExpr, localsIn);
        }
        auto rewriteTemporaryMapMethodToDirectHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty()) {
            return false;
          }
          std::string helperName = callExpr.name;
          if (!helperName.empty() && helperName.front() == '/') {
            helperName.erase(helperName.begin());
          }
          if (helperName != "count" && helperName != "contains" && helperName != "tryAt" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          const Expr &receiverExpr = callExpr.args.front();
          if (receiverExpr.kind != Expr::Kind::Call ||
              !ir_lowerer::resolveMapAccessTargetInfo(receiverExpr, localsIn).isMapTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.namespacePrefix = "/std/collections/map";
          rewrittenExpr.name = helperName;
          rewrittenExpr.templateArgs.clear();
          return true;
        };
        Expr rewrittenTemporaryMapMethodHelperExpr;
        if (rewriteTemporaryMapMethodToDirectHelperExpr(expr, rewrittenTemporaryMapMethodHelperExpr)) {
          return emitExpr(rewrittenTemporaryMapMethodHelperExpr, localsIn);
        }
        auto emitMaterializedCollectionReceiverExpr = [&](const Expr &callExpr) -> std::optional<bool> {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return std::nullopt;
          }

          std::string helperName;
          const Expr *receiverExpr = nullptr;
          auto matchesDirectHelperName = [&](const Expr &candidate, std::string_view bareName) {
            if (isSimpleCallName(candidate, bareName.data())) {
              return true;
            }
            std::string normalizedName = candidate.name;
            if (!normalizedName.empty() && normalizedName.front() == '/') {
              normalizedName.erase(normalizedName.begin());
            }
            return normalizedName == bareName ||
                   normalizedName == "map/" + std::string(bareName) ||
                   normalizedName == "std/collections/map/" + std::string(bareName);
          };
          if (callExpr.isMethodCall) {
            helperName = callExpr.name;
            receiverExpr = &callExpr.args.front();
          } else if (getBuiltinArrayAccessName(callExpr, helperName) && callExpr.args.size() == 2) {
            receiverExpr = &callExpr.args.front();
          } else if ((matchesDirectHelperName(callExpr, "count") ||
                      matchesDirectHelperName(callExpr, "contains") ||
                      matchesDirectHelperName(callExpr, "tryAt") ||
                      matchesDirectHelperName(callExpr, "capacity")) &&
                     !callExpr.args.empty()) {
            helperName = callExpr.name;
            if (!helperName.empty() && helperName.front() == '/') {
              helperName.erase(helperName.begin());
            }
            const size_t lastSlash = helperName.find_last_of('/');
            if (lastSlash != std::string::npos) {
              helperName = helperName.substr(lastSlash + 1);
            }
            receiverExpr = &callExpr.args.front();
          } else {
            return std::nullopt;
          }

          if (receiverExpr == nullptr || receiverExpr->kind != Expr::Kind::Call) {
            return std::nullopt;
          }

          const Definition *receiverDef = resolveDefinitionCall(*receiverExpr);
          if (receiverDef == nullptr) {
            return std::nullopt;
          }

          std::string collectionName;
          std::vector<std::string> collectionArgs;
          if (!ir_lowerer::inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs)) {
            return std::nullopt;
          }

          auto supportsHelperForCollection = [&](const std::string &name) {
            if (collectionName == "map") {
              return name == "count" || name == "contains" || name == "tryAt" ||
                     name == "at" || name == "at_unsafe";
            }
            if (collectionName == "vector" || collectionName == "array") {
              return name == "count" || name == "capacity" ||
                     name == "at" || name == "at_unsafe";
            }
            return false;
          };
          if (!supportsHelperForCollection(helperName)) {
            return std::nullopt;
          }

          ir_lowerer::LocalInfo materializedInfo;
          materializedInfo.index = allocTempLocal();
          if (collectionName == "map") {
            if (collectionArgs.size() != 2) {
              return std::nullopt;
            }
            materializedInfo.kind = ir_lowerer::LocalInfo::Kind::Map;
            materializedInfo.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
            materializedInfo.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
            materializedInfo.valueKind = materializedInfo.mapValueKind;
          } else {
            if (collectionArgs.size() != 1) {
              return std::nullopt;
            }
            materializedInfo.kind = (collectionName == "vector")
                                        ? ir_lowerer::LocalInfo::Kind::Vector
                                        : ir_lowerer::LocalInfo::Kind::Array;
            materializedInfo.valueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
          }

          if (!emitExpr(*receiverExpr, localsIn)) {
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(materializedInfo.index)});

          Expr rewrittenExpr = callExpr;
          Expr rewrittenReceiver;
          rewrittenReceiver.kind = Expr::Kind::Name;
          rewrittenReceiver.name = "__collection_receiver_" + std::to_string(materializedInfo.index);
          rewrittenExpr.args.front() = rewrittenReceiver;

          ir_lowerer::LocalMap rewrittenLocals = localsIn;
          rewrittenLocals.emplace(rewrittenReceiver.name, materializedInfo);
          return emitExpr(rewrittenExpr, rewrittenLocals);
        };
        if (const auto materializedReceiverResult = emitMaterializedCollectionReceiverExpr(expr);
            materializedReceiverResult.has_value()) {
          return *materializedReceiverResult;
        }
        auto rewriteBareVectorHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.empty() ||
              !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
            return false;
          }
          const std::string helperName = callExpr.name;
          if (helperName != "count" && helperName != "capacity" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(
              callExpr.args.front(),
              localsIn,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
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
                if ((collectionName != "array" && collectionName != "vector") || collectionArgs.size() != 1) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = (collectionName == "vector");
                targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
                return true;
              });
          if (!targetInfo.isVectorTarget) {
            return false;
          }
          std::string elemTypeName;
          if (callExpr.args.front().kind == Expr::Kind::Call) {
            std::string receiverCollectionName;
            if (getBuiltinCollectionName(callExpr.args.front(), receiverCollectionName) &&
                (receiverCollectionName == "array" || receiverCollectionName == "vector") &&
                callExpr.args.front().templateArgs.size() == 1) {
              elemTypeName = callExpr.args.front().templateArgs.front();
            } else if (const Definition *receiverDef = resolveDefinitionCall(callExpr.args.front())) {
              std::string receiverCollectionNameFromDef;
              std::vector<std::string> receiverCollectionArgs;
              if (ir_lowerer::inferDeclaredReturnCollection(
                      *receiverDef, receiverCollectionNameFromDef, receiverCollectionArgs) &&
                  (receiverCollectionNameFromDef == "array" || receiverCollectionNameFromDef == "vector") &&
                  receiverCollectionArgs.size() == 1) {
                elemTypeName = receiverCollectionArgs.front();
              }
            }
          }

          auto tryRewritePath = [&](const std::string &path) {
            Expr candidate = callExpr;
            candidate.name = path;
            candidate.namespacePrefix.clear();
            candidate.isMethodCall = false;
            if (candidate.templateArgs.empty() && !elemTypeName.empty()) {
              candidate.templateArgs = {elemTypeName};
            }
            if (resolveDefinitionCall(candidate) == nullptr) {
              return false;
            }
            rewrittenExpr = std::move(candidate);
            return true;
          };

          return tryRewritePath("/std/collections/vector/" + helperName) ||
                 tryRewritePath("/vector/" + helperName);
        };
        Expr rewrittenBareVectorHelperExpr;
        if (rewriteBareVectorHelperExpr(expr, rewrittenBareVectorHelperExpr)) {
          return emitExpr(rewrittenBareVectorHelperExpr, localsIn);
        }
        auto rewriteBareVectorMethodHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty() ||
              !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
            return false;
          }
          const std::string helperName = callExpr.name;
          if (helperName != "count" && helperName != "capacity" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          const size_t expectedArgCount =
              (helperName == "at" || helperName == "at_unsafe") ? 2u : 1u;
          if (callExpr.args.size() != expectedArgCount) {
            return false;
          }
          std::vector<std::string> receiverCollectionArgs;
          bool knownVectorCallReceiver = false;
          if (callExpr.args.front().kind == Expr::Kind::Call) {
            const Expr &receiverCallExpr = callExpr.args.front();
            knownVectorCallReceiver =
                receiverCallExpr.name.rfind("/std/collections/vector/vector", 0) == 0 ||
                receiverCallExpr.name.rfind("/std/collections/experimental_vector/vector", 0) == 0 ||
                receiverCallExpr.name.rfind("/vector/vector", 0) == 0;
            std::string receiverCollectionName;
            if (getBuiltinCollectionName(receiverCallExpr, receiverCollectionName) &&
                receiverCollectionName == "vector" &&
                receiverCallExpr.templateArgs.size() == 1) {
              receiverCollectionArgs = receiverCallExpr.templateArgs;
              knownVectorCallReceiver = true;
            } else if (const Definition *receiverDef = resolveDefinitionCall(receiverCallExpr)) {
              if (!ir_lowerer::inferDeclaredReturnCollection(
                      *receiverDef, receiverCollectionName, receiverCollectionArgs) ||
                  receiverCollectionName != "vector" || receiverCollectionArgs.size() != 1) {
                receiverCollectionArgs.clear();
              } else {
                knownVectorCallReceiver = true;
              }
            }
          }
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(
              callExpr.args.front(),
              localsIn,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
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
                if ((collectionName != "array" && collectionName != "vector") || collectionArgs.size() != 1) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = (collectionName == "vector");
                targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
                return true;
              });
          if (!targetInfo.isVectorTarget && !knownVectorCallReceiver) {
            return false;
          }
          if (knownVectorCallReceiver) {
            auto tryRewriteNamespace = [&](const std::string &namespacePrefix) {
              Expr candidate = callExpr;
              candidate.isMethodCall = false;
              candidate.namespacePrefix = namespacePrefix;
              candidate.name = helperName;
              if (candidate.templateArgs.empty() && !receiverCollectionArgs.empty()) {
                candidate.templateArgs = receiverCollectionArgs;
              }
              rewrittenExpr = std::move(candidate);
              return true;
            };
            return tryRewriteNamespace("/std/collections/vector") ||
                   tryRewriteNamespace("/vector");
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.name = helperName;
          return true;
        };
        Expr rewrittenBareVectorMethodHelperExpr;
        if (rewriteBareVectorMethodHelperExpr(expr, rewrittenBareVectorMethodHelperExpr)) {
          const std::string priorError = error;
          if (!emitExpr(rewrittenBareVectorMethodHelperExpr, localsIn)) {
            return false;
          }
          error = priorError;
          return true;
        }
        auto rewriteBareMapAccessMethodExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.size() != 2) {
            return false;
          }
          std::string accessName;
          if (!getBuiltinArrayAccessName(callExpr, accessName) ||
              (accessName != "at" && accessName != "at_unsafe")) {
            return false;
          }
          if (callExpr.args.front().kind != Expr::Kind::Call) {
            return false;
          }
          ir_lowerer::MapAccessTargetInfo targetInfoOut;
          const Definition *callee = resolveDefinitionCall(callExpr.args.front());
          if (callee == nullptr) {
            return false;
          }
          std::string collectionName;
          std::vector<std::string> collectionArgs;
          if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) ||
              collectionName != "map" || collectionArgs.size() != 2) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.isMethodCall = true;
          return true;
        };
        Expr rewrittenBareMapAccessMethodExpr;
        if (rewriteBareMapAccessMethodExpr(expr, rewrittenBareMapAccessMethodExpr)) {
          const std::string priorError = error;
          if (!emitExpr(rewrittenBareMapAccessMethodExpr, localsIn)) {
            return false;
          }
          error = priorError;
          return true;
        }
        Expr inlineDispatchExpr = expr;
        const auto inlineDispatchResult = ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            inlineDispatchExpr,
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
        auto rewriteExplicitMapHelperBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.empty()) {
            return false;
          }
          std::string helperName;
          if (callExpr.name == "/map/count" || callExpr.name == "/std/collections/map/count") {
            helperName = "count";
          } else if (callExpr.name == "/map/at" || callExpr.name == "/std/collections/map/at") {
            helperName = "at";
          } else if (callExpr.name == "/map/at_unsafe" || callExpr.name == "/std/collections/map/at_unsafe") {
            helperName = "at_unsafe";
          } else {
            return false;
          }
          if (!ir_lowerer::resolveMapAccessTargetInfo(callExpr.args.front(), localsIn).isMapTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = helperName;
          return true;
        };
        auto rewriteImplicitBorrowedMapReceiverExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }

          auto isBorrowedOrPointerMapReceiver = [&](const Expr &receiverExpr) {
            if (receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
                receiverExpr.args.size() == 1) {
              return false;
            }
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.name);
              return it != localsIn.end() &&
                     ((it->second.kind == ir_lowerer::LocalInfo::Kind::Reference && it->second.referenceToMap) ||
                      (it->second.kind == ir_lowerer::LocalInfo::Kind::Pointer && it->second.pointerToMap));
            }
            std::string accessName;
            if (receiverExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(receiverExpr, accessName) &&
                receiverExpr.args.size() == 2 && receiverExpr.args.front().kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.args.front().name);
              return it != localsIn.end() && it->second.isArgsPack &&
                     (((it->second.argsPackElementKind == ir_lowerer::LocalInfo::Kind::Reference) &&
                       it->second.referenceToMap) ||
                      ((it->second.argsPackElementKind == ir_lowerer::LocalInfo::Kind::Pointer) &&
                       it->second.pointerToMap));
            }
            return false;
          };

          auto shouldRewriteReceiver = [&](const Expr &candidate) {
            if (candidate.isMethodCall) {
              return candidate.name == "contains" || candidate.name == "tryAt" ||
                     candidate.name == "at" || candidate.name == "at_unsafe";
            }
            if (candidate.name == "contains" || candidate.name == "tryAt" ||
                candidate.name == "/map/contains" || candidate.name == "/std/collections/map/contains" ||
                candidate.name == "/map/tryAt" || candidate.name == "/std/collections/map/tryAt" ||
                candidate.name == "at" || candidate.name == "at_unsafe") {
              return true;
            }
            return false;
          };

          if (!shouldRewriteReceiver(callExpr) || !isBorrowedOrPointerMapReceiver(callExpr.args.front())) {
            return false;
          }

          Expr derefExpr;
          derefExpr.kind = Expr::Kind::Call;
          derefExpr.name = "dereference";
          derefExpr.args.push_back(callExpr.args.front());

          rewrittenExpr = callExpr;
          rewrittenExpr.args.front() = derefExpr;
          return true;
        };
        Expr nativeTailExpr = inlineDispatchExpr;
        Expr rewrittenExplicitMapHelperExpr;
        if (rewriteExplicitMapHelperBuiltinExpr(expr, rewrittenExplicitMapHelperExpr)) {
          nativeTailExpr = rewrittenExplicitMapHelperExpr;
        }
        Expr rewrittenBorrowedMapReceiverExpr;
        if (rewriteImplicitBorrowedMapReceiverExpr(nativeTailExpr, rewrittenBorrowedMapReceiverExpr)) {
          nativeTailExpr = rewrittenBorrowedMapReceiverExpr;
        }
        const auto nativeTailResult = ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            nativeTailExpr,
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
