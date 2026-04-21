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
          std::string semanticTryFactError;
          auto applySemanticTryValueType = [&](const std::string &valueTypeText,
                                               ResultExprInfo &resultInfoOut) {
            const std::string trimmedValueType = trimTemplateTypeText(valueTypeText);
            if (trimmedValueType.empty()) {
              return false;
            }
            resultInfoOut.valueCollectionKind = LocalInfo::Kind::Value;
            resultInfoOut.valueKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueIsFileHandle = false;
            resultInfoOut.valueStructType.clear();
            if (ir_lowerer::resolveSupportedResultCollectionType(
                    trimmedValueType,
                    resultInfoOut.valueCollectionKind,
                    resultInfoOut.valueKind,
                    &resultInfoOut.valueMapKeyKind)) {
              return true;
            }
            std::string baseType;
            std::string typeArgs;
            if (splitTemplateTypeName(trimmedValueType, baseType, typeArgs) &&
                normalizeCollectionBindingTypeName(baseType) == "File") {
              resultInfoOut.valueKind = LocalInfo::ValueKind::Int64;
              resultInfoOut.valueIsFileHandle = true;
              return true;
            }
            resultInfoOut.valueKind = valueKindFromTypeName(trimmedValueType);
            if (resultInfoOut.valueKind != LocalInfo::ValueKind::Unknown) {
              return true;
            }
            std::string resolvedStructPath;
            if (resolveStructTypeName(trimmedValueType, expr.namespacePrefix, resolvedStructPath)) {
              resultInfoOut.valueStructType = std::move(resolvedStructPath);
              return true;
            }
            resultInfoOut.valueStructType = trimmedValueType;
            if (!resultInfoOut.valueStructType.empty() && resultInfoOut.valueStructType.front() != '/') {
              resultInfoOut.valueStructType.insert(resultInfoOut.valueStructType.begin(), '/');
            }
            return true;
          };
          auto applySemanticOperandResultType = [&](const Expr &operandExpr,
                                                    ResultExprInfo &resultInfoOut) {
            if (!callResolutionAdapters.semanticProductTargets.hasSemanticProduct ||
                operandExpr.semanticNodeId == 0) {
              return false;
            }
            const auto *queryFact =
                findSemanticProductQueryFactBySemanticId(
                    callResolutionAdapters.semanticProductTargets.semanticIndex,
                    operandExpr);
            if (queryFact == nullptr || !queryFact->hasResultType) {
              return false;
            }
            resultInfoOut.isResult = true;
            resultInfoOut.hasValue = queryFact->resultTypeHasValue;
            resultInfoOut.errorType = queryFact->resultErrorType;
            if (!resultInfoOut.hasValue) {
              return true;
            }
            return applySemanticTryValueType(queryFact->resultValueType, resultInfoOut);
          };
          auto resolveLambdaReturnedValueExpr = [&](const Expr &lambdaExpr,
                                                    const Expr *&valueExprOut) {
            valueExprOut = nullptr;
            if (!lambdaExpr.isLambda) {
              return false;
            }
            for (size_t i = 0; i < lambdaExpr.bodyArguments.size(); ++i) {
              const Expr &bodyExpr = lambdaExpr.bodyArguments[i];
              const bool isLast = (i + 1 == lambdaExpr.bodyArguments.size());
              if (bodyExpr.isBinding || !isLast) {
                continue;
              }
              if (isSimpleCallName(bodyExpr, "return")) {
                if (bodyExpr.args.size() != 1) {
                  return false;
                }
                valueExprOut = &bodyExpr.args.front();
                return true;
              }
              valueExprOut = &bodyExpr;
              return true;
            }
            return false;
          };
          auto applyBuiltinCombinatorStructFallback = [&](const Expr &operandExpr,
                                                          ResultExprInfo &resultInfoOut) {
            if (operandExpr.kind != Expr::Kind::Call) {
              return false;
            }
            const Expr *candidateValueExpr = nullptr;
            if ((operandExpr.name == "map" && operandExpr.args.size() == 3) ||
                (operandExpr.name == "and_then" && operandExpr.args.size() == 3)) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[2], candidateValueExpr)) {
                return false;
              }
            } else if (operandExpr.name == "map2" && operandExpr.args.size() == 4) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[3], candidateValueExpr)) {
                return false;
              }
            } else {
              return false;
            }
            if (candidateValueExpr == nullptr) {
              return false;
            }
            const auto isResultOkCall = [&](const Expr &candidateExpr) {
              return candidateExpr.kind == Expr::Kind::Call &&
                     candidateExpr.name == "ok" &&
                     candidateExpr.args.size() == 2 &&
                     candidateExpr.args.front().kind == Expr::Kind::Name &&
                     candidateExpr.args.front().name == "Result";
            };
            if (operandExpr.name == "and_then" && isResultOkCall(*candidateValueExpr)) {
              candidateValueExpr = &candidateValueExpr->args[1];
            }
            if (candidateValueExpr->kind != Expr::Kind::Call) {
              return false;
            }
            auto resolveScopedCallPath = [](const Expr &candidateExpr) {
              if (!candidateExpr.name.empty() && candidateExpr.name.front() == '/') {
                return candidateExpr.name;
              }
              if (candidateExpr.namespacePrefix.empty()) {
                return candidateExpr.name;
              }
              std::string scopedPath = candidateExpr.namespacePrefix;
              if (!scopedPath.empty() && scopedPath.front() != '/') {
                scopedPath.insert(scopedPath.begin(), '/');
              }
              if (!candidateExpr.name.empty()) {
                if (!scopedPath.empty() && scopedPath.back() != '/') {
                  scopedPath.push_back('/');
                }
                scopedPath += candidateExpr.name;
              }
              return scopedPath;
            };
            std::string valueStructType;
            const Definition *valueDef = resolveDefinitionCall(*candidateValueExpr);
            const std::string candidateValuePath = resolveScopedCallPath(*candidateValueExpr);
            if (valueDef != nullptr && isStructDefinition(*valueDef)) {
              valueStructType = valueDef->fullPath;
            } else if (candidateValuePath == "ContainerError" ||
                       candidateValuePath == "/std/collections/ContainerError") {
              valueStructType = "/std/collections/ContainerError";
            } else if (candidateValuePath == "ImageError" ||
                       candidateValuePath == "/std/image/ImageError") {
              valueStructType = "/std/image/ImageError";
            } else if (candidateValuePath == "GfxError" ||
                       candidateValuePath == "/std/gfx/GfxError" ||
                       candidateValuePath == "/std/gfx/experimental/GfxError") {
              valueStructType = candidateValuePath == "/std/gfx/experimental/GfxError"
                                    ? "/std/gfx/experimental/GfxError"
                                    : "/std/gfx/GfxError";
            }
            if (valueStructType.empty()) {
              return false;
            }
            resultInfoOut.isResult = true;
            resultInfoOut.hasValue = true;
            resultInfoOut.valueCollectionKind = LocalInfo::Kind::Value;
            resultInfoOut.valueKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueIsFileHandle = false;
            resultInfoOut.valueStructType = std::move(valueStructType);
            return true;
          };
          auto applyBuiltinCombinatorValueKindFallback = [&](const Expr &operandExpr,
                                                             ResultExprInfo &resultInfoOut) {
            if (operandExpr.kind != Expr::Kind::Call ||
                resultInfoOut.valueCollectionKind != LocalInfo::Kind::Value ||
                resultInfoOut.valueKind != LocalInfo::ValueKind::Unknown ||
                resultInfoOut.valueMapKeyKind != LocalInfo::ValueKind::Unknown ||
                !resultInfoOut.valueStructType.empty() ||
                resultInfoOut.valueIsFileHandle) {
              return false;
            }
            const Expr *candidateValueExpr = nullptr;
            if ((operandExpr.name == "map" && operandExpr.args.size() == 3) ||
                (operandExpr.name == "and_then" && operandExpr.args.size() == 3)) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[2], candidateValueExpr)) {
                return false;
              }
            } else if (operandExpr.name == "map2" && operandExpr.args.size() == 4) {
              if (!resolveLambdaReturnedValueExpr(operandExpr.args[3], candidateValueExpr)) {
                return false;
              }
            } else {
              return false;
            }
            const auto isResultOkCall = [&](const Expr &candidateExpr) {
              return candidateExpr.kind == Expr::Kind::Call &&
                     candidateExpr.name == "ok" &&
                     candidateExpr.args.size() == 2 &&
                     candidateExpr.args.front().kind == Expr::Kind::Name &&
                     candidateExpr.args.front().name == "Result";
            };
            if (operandExpr.name == "and_then" && candidateValueExpr != nullptr &&
                isResultOkCall(*candidateValueExpr)) {
              candidateValueExpr = &candidateValueExpr->args[1];
            }
            if (candidateValueExpr == nullptr) {
              return false;
            }
            std::string builtinComparisonName;
            if (!ir_lowerer::getBuiltinComparisonName(*candidateValueExpr, builtinComparisonName)) {
              return false;
            }
            resultInfoOut.isResult = true;
            resultInfoOut.hasValue = true;
            resultInfoOut.valueCollectionKind = LocalInfo::Kind::Value;
            resultInfoOut.valueKind = LocalInfo::ValueKind::Bool;
            resultInfoOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            resultInfoOut.valueIsFileHandle = false;
            resultInfoOut.valueStructType.clear();
            return true;
          };
          if (callResolutionAdapters.semanticProductTargets.hasSemanticProduct && expr.semanticNodeId != 0) {
            const auto *tryFact =
                findSemanticProductTryFactBySemanticId(
                    callResolutionAdapters.semanticProductTargets.semanticIndex,
                    expr);
            if (tryFact != nullptr) {
              resultInfo.isResult = true;
              resultInfo.hasValue = true;
              resultInfo.errorType = tryFact->errorType;
              if (!applySemanticTryValueType(tryFact->valueType, resultInfo)) {
                semanticTryFactError = "incomplete semantic-product try fact: try";
                resultInfo = ResultExprInfo{};
              }
            } else {
              semanticTryFactError = "missing semantic-product try fact: try";
            }
          }
          auto resolveResultFieldInfo = [&](const Expr &valueExpr, ResultExprInfo &fieldResultOut) {
            fieldResultOut = ResultExprInfo{};
            if (!(valueExpr.kind == Expr::Kind::Call && valueExpr.isFieldAccess && valueExpr.args.size() == 1)) {
              return false;
            }

            const std::string receiverStructPath = inferStructExprPath(valueExpr.args.front(), localsIn);
            if (receiverStructPath.empty()) {
              return false;
            }

            LayoutFieldBinding fieldBinding;
            if (!ir_lowerer::resolveStructLayoutFieldBinding(
                    receiverStructPath, valueExpr.name, structFieldInfoByName, defMap, fieldBinding) ||
                normalizeCollectionBindingTypeName(fieldBinding.typeName) != "Result") {
              return false;
            }

            std::vector<std::string> resultArgs;
            if (fieldBinding.typeTemplateArg.empty() ||
                !splitTemplateArgs(fieldBinding.typeTemplateArg, resultArgs) ||
                (resultArgs.size() != 1 && resultArgs.size() != 2)) {
              return false;
            }

            fieldResultOut.isResult = true;
            fieldResultOut.hasValue = (resultArgs.size() == 2);
            fieldResultOut.errorType = trimTemplateTypeText(resultArgs.back());
            if (!fieldResultOut.hasValue) {
              return true;
            }

            const std::string valueTypeText = trimTemplateTypeText(resultArgs.front());
            fieldResultOut.valueCollectionKind = LocalInfo::Kind::Value;
            fieldResultOut.valueKind = LocalInfo::ValueKind::Unknown;
            fieldResultOut.valueMapKeyKind = LocalInfo::ValueKind::Unknown;
            if (!ir_lowerer::resolveSupportedResultCollectionType(
                    valueTypeText,
                    fieldResultOut.valueCollectionKind,
                    fieldResultOut.valueKind,
                    &fieldResultOut.valueMapKeyKind)) {
              std::string valueStructType;
              if (resolveStructTypeName(valueTypeText, valueExpr.namespacePrefix, valueStructType)) {
                fieldResultOut.valueStructType = std::move(valueStructType);
              } else {
                fieldResultOut.valueKind = valueKindFromTypeName(valueTypeText);
              }
            }
            return fieldResultOut.valueCollectionKind != LocalInfo::Kind::Value ||
                   fieldResultOut.valueKind != LocalInfo::ValueKind::Unknown ||
                   !fieldResultOut.valueStructType.empty();
          };
          if (((!resultInfo.isResult &&
                !ir_lowerer::resolveResultExprInfoFromLocals(
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
                   resultInfo,
                   callResolutionAdapters.semanticProgram,
                   &callResolutionAdapters.semanticProductTargets.semanticIndex,
                   &error)) ||
               !resultInfo.isResult) &&
              !resolveResultFieldInfo(expr.args.front(), resultInfo)) {
            if (error.empty()) {
              error = !semanticTryFactError.empty() ? semanticTryFactError
                                                    : "try requires Result argument";
            }
            return false;
          }
          if (resultInfo.isResult && resultInfo.hasValue &&
              resultInfo.valueCollectionKind == LocalInfo::Kind::Value &&
              resultInfo.valueMapKeyKind == LocalInfo::ValueKind::Unknown &&
              resultInfo.valueStructType.empty() &&
              !resultInfo.valueIsFileHandle) {
            (void)applySemanticOperandResultType(expr.args.front(), resultInfo);
            if (resultInfo.valueStructType.empty()) {
              (void)applyBuiltinCombinatorStructFallback(expr.args.front(), resultInfo);
            }
            (void)applyBuiltinCombinatorValueKindFallback(expr.args.front(), resultInfo);
          }
          auto normalizeSpecializedMapResultInfo = [&](ResultExprInfo &valueResultInfo) {
            if (valueResultInfo.valueCollectionKind != LocalInfo::Kind::Value ||
                valueResultInfo.valueKind != LocalInfo::ValueKind::Unknown ||
                valueResultInfo.valueStructType.empty()) {
              return;
            }
            auto resolveSpecializedVectorElementKind = [&](const std::string &typeText,
                                                           LocalInfo::ValueKind &elemKindOut) {
              elemKindOut = LocalInfo::ValueKind::Unknown;
              std::string normalized = trimTemplateTypeText(typeText);
              if (!normalized.empty() && normalized.front() != '/') {
                normalized.insert(normalized.begin(), '/');
              }
              if (normalized.rfind("/std/collections/experimental_vector/Vector__", 0) != 0) {
                return false;
              }
              Expr syntheticExpr;
              syntheticExpr.kind = Expr::Kind::Call;
              syntheticExpr.name = normalized;
              const Definition *structDef = resolveDefinitionCall(syntheticExpr);
              if (structDef == nullptr || !isStructDefinition(*structDef)) {
                return false;
              }
              for (const auto &fieldExpr : structDef->statements) {
                if (!fieldExpr.isBinding || fieldExpr.name != "data") {
                  continue;
                }
                for (const auto &transform : fieldExpr.transforms) {
                  if (transform.name == "effects" || transform.name == "capabilities" ||
                      isLayoutQualifierName(transform.name) || !transform.arguments.empty()) {
                    continue;
                  }
                  if (normalizeCollectionBindingTypeName(transform.name) != "Pointer" ||
                      transform.templateArgs.size() != 1) {
                    break;
                  }
                  std::string elementType = trimTemplateTypeText(transform.templateArgs.front());
                  if (!extractTopLevelUninitializedTypeText(elementType, elementType)) {
                    break;
                  }
                  elemKindOut = valueKindFromTypeName(elementType);
                  return elemKindOut != LocalInfo::ValueKind::Unknown;
                }
              }
              return false;
            };
            auto resolveSpecializedMapTypeKinds = [&](const std::string &typeText,
                                                      LocalInfo::ValueKind &keyKindOut,
                                                      LocalInfo::ValueKind &valueKindOut) {
              keyKindOut = LocalInfo::ValueKind::Unknown;
              valueKindOut = LocalInfo::ValueKind::Unknown;
              std::string normalized = trimTemplateTypeText(typeText);
              if (!normalized.empty() && normalized.front() != '/') {
                normalized.insert(normalized.begin(), '/');
              }
              if (normalized.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
                return false;
              }
              Expr syntheticExpr;
              syntheticExpr.kind = Expr::Kind::Call;
              syntheticExpr.name = normalized;
              const Definition *structDef = resolveDefinitionCall(syntheticExpr);
              if (structDef == nullptr || !isStructDefinition(*structDef)) {
                return false;
              }
              for (const auto &fieldExpr : structDef->statements) {
                if (!fieldExpr.isBinding) {
                  continue;
                }
                for (const auto &transform : fieldExpr.transforms) {
                  if (transform.name == "effects" || transform.name == "capabilities" ||
                      isLayoutQualifierName(transform.name) || !transform.arguments.empty()) {
                    continue;
                  }
                  const std::string rawTypeName = transform.name;
                  if (normalizeCollectionBindingTypeName(rawTypeName) != "vector") {
                    break;
                  }
                  LocalInfo::ValueKind fieldKind = LocalInfo::ValueKind::Unknown;
                  if (transform.templateArgs.size() == 1) {
                    fieldKind = valueKindFromTypeName(trimTemplateTypeText(transform.templateArgs.front()));
                  } else if (!resolveSpecializedVectorElementKind(rawTypeName, fieldKind)) {
                    break;
                  }
                  if (fieldKind == LocalInfo::ValueKind::Unknown) {
                    break;
                  }
                  if (fieldExpr.name == "keys") {
                    keyKindOut = fieldKind;
                  } else if (fieldExpr.name == "payloads") {
                    valueKindOut = fieldKind;
                  }
                  break;
                }
              }
              return keyKindOut != LocalInfo::ValueKind::Unknown &&
                     valueKindOut != LocalInfo::ValueKind::Unknown;
            };
            LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
            LocalInfo::ValueKind payloadKind = LocalInfo::ValueKind::Unknown;
            if (!resolveSpecializedMapTypeKinds(valueResultInfo.valueStructType, keyKind, payloadKind)) {
              return;
            }
            valueResultInfo.valueCollectionKind = LocalInfo::Kind::Map;
            valueResultInfo.valueMapKeyKind = keyKind;
            valueResultInfo.valueKind = payloadKind;
            valueResultInfo.valueStructType.clear();
          };
          normalizeSpecializedMapResultInfo(resultInfo);
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
            if (ir_lowerer::usesInlineBufferResultErrorDiscriminator(resultInfo)) {
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
