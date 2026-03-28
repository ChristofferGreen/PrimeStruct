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
          if (!ir_lowerer::resolveResultExprInfoFromLocals(
                  expr.args.front(),
                  localsIn,
                  [&](const Expr &callExpr, const LocalMap &callLocals) {
                    return resolveMethodCallDefinition(callExpr, callLocals);
                  },
                  [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                  [&](const std::string &definitionPath, ReturnInfo &returnInfoOut) {
                    return getReturnInfo && getReturnInfo(definitionPath, returnInfoOut);
                  },
                  [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                    return inferExprKind(valueExpr, valueLocals);
                  },
                  resultInfo) ||
              !resultInfo.isResult) {
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
            ir_lowerer::emitResultWhyErrorLocalFromResult(
                resultLocal,
                resultInfo,
                errorLocal,
                allocTempLocal,
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
            function.instructions.push_back({IrOpcode::PushI64, 0});
            function.instructions.push_back({IrOpcode::CmpEqI64, 0});
            size_t jumpError = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            if (resultInfo.valueCollectionKind == LocalInfo::Kind::Buffer) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
            } else {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(resultLocal)});
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
              function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
              function.instructions.push_back({IrOpcode::MulI64, 0});
              function.instructions.push_back({IrOpcode::SubI64, 0});
            }
            if (!resultInfo.valueStructType.empty()) {
              ir_lowerer::PackedResultStructPayloadInfo payloadInfo;
              if (!ir_lowerer::resolvePackedResultStructPayloadInfo(
                      resultInfo.valueStructType,
                      [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                        return resolveStructSlotLayout(structPath, layoutOut);
                      },
                      payloadInfo)) {
                error = ir_lowerer::unsupportedPackedResultValueKindError("try");
                return false;
              }
              if (payloadInfo.isPackedSingleSlot) {
                const int32_t baseLocal = nextLocal;
                nextLocal += payloadInfo.slotCount;
                const int32_t ptrLocal = nextLocal++;
                function.instructions.push_back(
                    {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(payloadInfo.slotCount - 1))});
                function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
                function.instructions.push_back(
                    {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + payloadInfo.fieldOffset)});
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
