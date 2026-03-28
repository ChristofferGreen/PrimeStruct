        auto emitPackedResultValueExpr = [&](const Expr &valueExpr,
                                             const LocalMap &valueLocals,
                                             const std::string &builtinName) -> bool {
          LocalInfo::ValueKind packedValueKind = LocalInfo::ValueKind::Unknown;
          std::string structType;
          if (!resolvePackedResultPayloadInfo(valueExpr, valueLocals, packedValueKind, structType)) {
            error = ir_lowerer::unsupportedPackedResultValueKindError(builtinName);
            return false;
          }
          if (!structType.empty() && packedValueKind != LocalInfo::ValueKind::Unknown &&
              valueExpr.kind == Expr::Kind::Call && !valueExpr.isMethodCall && valueExpr.args.size() == 1) {
            return emitExpr(valueExpr.args.front(), valueLocals);
          }
          if (!emitExpr(valueExpr, valueLocals)) {
            return false;
          }
          if (!structType.empty() && packedValueKind != LocalInfo::ValueKind::Unknown) {
            ir_lowerer::PackedResultStructPayloadInfo payloadInfo;
            if (!ir_lowerer::resolvePackedResultStructPayloadInfo(
                    structType,
                    [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                      return resolveStructSlotLayout(structPath, layoutOut);
                    },
                    payloadInfo)) {
              error = ir_lowerer::unsupportedPackedResultValueKindError(builtinName);
              return false;
            }
            const int32_t ptrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            const uint64_t fieldOffsetBytes = static_cast<uint64_t>(payloadInfo.fieldOffset) * IrSlotBytes;
            if (fieldOffsetBytes != 0) {
              function.instructions.push_back({IrOpcode::PushI64, fieldOffsetBytes});
              function.instructions.push_back({IrOpcode::AddI64, 0});
            }
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          }
          return true;
        };
        auto tryEmitResultMapCall = [&](const Expr &callExpr, const LocalMap &callLocals) -> std::optional<bool> {
          if (!(callExpr.kind == Expr::Kind::Call && callExpr.name == "map" && callExpr.args.size() == 3 &&
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
          ir_lowerer::emitResultWhyErrorLocalFromResult(
              resultLocal,
              sourceResultInfo,
              errorLocal,
              allocTempLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t payloadLocal = allocTempLocal();
          ir_lowerer::emitPackedResultPayloadLocalFromResult(
              resultLocal,
              sourceResultInfo,
              errorLocal,
              payloadLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });

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
          if (!(callExpr.kind == Expr::Kind::Call && callExpr.name == "and_then" && callExpr.args.size() == 3 &&
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
          ir_lowerer::emitResultWhyErrorLocalFromResult(
              resultLocal,
              sourceResultInfo,
              errorLocal,
              allocTempLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(errorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t payloadLocal = allocTempLocal();
          ir_lowerer::emitPackedResultPayloadLocalFromResult(
              resultLocal,
              sourceResultInfo,
              errorLocal,
              payloadLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });

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
          if (!(callExpr.kind == Expr::Kind::Call && callExpr.name == "map2" && callExpr.args.size() == 4 &&
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
          ir_lowerer::emitResultWhyErrorLocalFromResult(
              leftResultLocal,
              leftResultInfo,
              leftErrorLocal,
              allocTempLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
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
          ir_lowerer::emitResultWhyErrorLocalFromResult(
              rightResultLocal,
              rightResultInfo,
              rightErrorLocal,
              allocTempLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(rightErrorLocal)});
          function.instructions.push_back({IrOpcode::PushI64, 0});
          function.instructions.push_back({IrOpcode::CmpEqI64, 0});
          const size_t jumpRightError = function.instructions.size();
          function.instructions.push_back({IrOpcode::JumpIfZero, 0});

          const int32_t leftPayloadLocal = allocTempLocal();
          ir_lowerer::emitPackedResultPayloadLocalFromResult(
              leftResultLocal,
              leftResultInfo,
              leftErrorLocal,
              leftPayloadLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });

          const int32_t rightPayloadLocal = allocTempLocal();
          ir_lowerer::emitPackedResultPayloadLocalFromResult(
              rightResultLocal,
              rightResultInfo,
              rightErrorLocal,
              rightPayloadLocal,
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); });

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
