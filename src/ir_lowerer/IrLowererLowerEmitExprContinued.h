              const bool isBufferAllocateCall =
                  normalized == "allocate" ||
                  normalized.rfind("allocate__t", 0) == 0 ||
                  normalized == "std/gfx/Buffer/allocate" ||
                  normalized == "std/gfx/experimental/Buffer/allocate" ||
                  normalized.rfind("std/gfx/Buffer/allocate__t", 0) == 0 ||
                  normalized.rfind("std/gfx/experimental/Buffer/allocate__t", 0) == 0;
              if (isBufferAllocateCall && valueExpr.templateArgs.size() == 1) {
                collectionKindOut = LocalInfo::Kind::Buffer;
                collectionValueKindOut = valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front()));
                return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
              }
              const bool isBufferUploadCall =
                  normalized == "upload" ||
                  normalized.rfind("upload__t", 0) == 0 ||
                  normalized == "std/gfx/Buffer/upload" ||
                  normalized == "std/gfx/experimental/Buffer/upload" ||
                  normalized.rfind("std/gfx/Buffer/upload__t", 0) == 0 ||
                  normalized.rfind("std/gfx/experimental/Buffer/upload__t", 0) == 0;
              if (isBufferUploadCall) {
                if (valueExpr.templateArgs.size() == 1) {
                  collectionKindOut = LocalInfo::Kind::Buffer;
                  collectionValueKindOut = valueKindFromTypeName(trimTemplateTypeText(valueExpr.templateArgs.front()));
                  return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
                }
                if (valueExpr.args.size() == 1) {
                  const Expr &sourceExpr = valueExpr.args.front();
                  if (sourceExpr.kind == Expr::Kind::Name) {
                    auto localIt = valueLocals.find(sourceExpr.name);
                    if (localIt != valueLocals.end() &&
                        ir_lowerer::isSupportedPackedResultCollectionKind(localIt->second.kind)) {
                      collectionKindOut = LocalInfo::Kind::Buffer;
                      collectionValueKindOut = localIt->second.valueKind;
                      return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
                    }
                  }
                  if (sourceExpr.kind == Expr::Kind::Call) {
                    std::string sourceCollectionName;
                    if (!sourceExpr.isMethodCall && getBuiltinCollectionName(sourceExpr, sourceCollectionName) &&
                        ((((sourceCollectionName == "array" || sourceCollectionName == "vector") &&
                           sourceExpr.templateArgs.size() == 1) ||
                          (sourceCollectionName == "Buffer" && sourceExpr.templateArgs.size() == 1) ||
                          (sourceCollectionName == "map" && sourceExpr.templateArgs.size() == 2)))) {
                      collectionKindOut = LocalInfo::Kind::Buffer;
                      collectionValueKindOut = valueKindFromTypeName(
                          trimTemplateTypeText(sourceCollectionName == "map" ? sourceExpr.templateArgs.back()
                                                                             : sourceExpr.templateArgs.front()));
                      return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
                    }
                  }
                }
              }
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
              collectionKindOut = LocalInfo::Kind::Value;
            } else if (declaredCollection == "Buffer") {
              if (declaredCollectionArgs.size() != 1) {
                return false;
              }
              collectionKindOut = LocalInfo::Kind::Buffer;
            } else {
              return false;
            }
            collectionValueKindOut = valueKindFromTypeName(
                trimTemplateTypeText(declaredCollection == "map" ? declaredCollectionArgs.back()
                                                                 : declaredCollectionArgs.front()));
            return collectionValueKindOut != LocalInfo::ValueKind::Unknown;
          };
          auto describePackedResultPayload = [&](const Expr &payloadExpr) {
            if (!payloadExpr.name.empty()) {
              return function.name + " -> " + payloadExpr.name;
            }
            return function.name + " -> <expr>";
          };
          auto resolveSemanticProductPackedResultPayload =
              [&](LocalInfo::ValueKind &payloadKindOut,
                  std::string &payloadStructTypeOut) -> std::optional<bool> {
            payloadKindOut = LocalInfo::ValueKind::Unknown;
            payloadStructTypeOut.clear();
            const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
            if (!semanticTargets.hasSemanticProduct || valueExpr.semanticNodeId == 0) {
              return std::nullopt;
            }
            std::vector<std::string> candidateTypeTexts;
            if (const SemanticProgramBindingFact *bindingFact =
                    findSemanticProductBindingFact(semanticTargets, valueExpr);
                bindingFact != nullptr) {
              appendSemanticProductTypeTextCandidate(
                  candidateTypeTexts,
                  bindingFact->bindingTypeText,
                  bindingFact->bindingTypeTextId);
            }
            if (const SemanticProgramQueryFact *queryFact =
                    findSemanticProductQueryFact(semanticTargets, valueExpr);
                queryFact != nullptr) {
              appendSemanticProductTypeTextCandidate(
                  candidateTypeTexts,
                  queryFact->bindingTypeText,
                  queryFact->bindingTypeTextId);
              appendSemanticProductTypeTextCandidate(
                  candidateTypeTexts,
                  queryFact->queryTypeText,
                  queryFact->queryTypeTextId);
            }
            if (candidateTypeTexts.empty()) {
              return std::nullopt;
            }
            for (const std::string &typeText : candidateTypeTexts) {
              const std::string normalizedTypeText = trimTemplateTypeText(typeText);
              if (normalizedTypeText.empty()) {
                continue;
              }
              LocalInfo::Kind semanticCollectionKind = LocalInfo::Kind::Value;
              LocalInfo::ValueKind semanticMapKeyKind = LocalInfo::ValueKind::Unknown;
              if (ir_lowerer::resolveSupportedResultCollectionType(normalizedTypeText,
                                                                    semanticCollectionKind,
                                                                    payloadKindOut,
                                                                    &semanticMapKeyKind) &&
                  (ir_lowerer::isSupportedPackedResultCollectionKind(semanticCollectionKind) ||
                   semanticMapKeyKind != LocalInfo::ValueKind::Unknown)) {
                return true;
              }
              std::string semanticTypeBase;
              std::string semanticTypeArgs;
              if (splitTemplateTypeName(normalizedTypeText, semanticTypeBase, semanticTypeArgs) &&
                  normalizeCollectionBindingTypeName(trimTemplateTypeText(semanticTypeBase)) ==
                      "File") {
                payloadKindOut = LocalInfo::ValueKind::Int64;
                return true;
              }
              payloadKindOut = valueKindFromTypeName(normalizedTypeText);
              if (payloadKindOut != LocalInfo::ValueKind::Unknown) {
                return true;
              }
              if (resolveStructTypeName(normalizedTypeText,
                                        valueExpr.namespacePrefix,
                                        payloadStructTypeOut) ||
                  resolveStructTypeName(normalizedTypeText,
                                        function.name,
                                        payloadStructTypeOut)) {
                return true;
              }
            }
            error = "stale semantic-product packed Result payload metadata: " +
                    describePackedResultPayload(valueExpr);
            return false;
          };
          std::string inferredStructType;
          LocalInfo::ValueKind inferredValueKind = LocalInfo::ValueKind::Unknown;
          const std::optional<bool> resolvedPackedResultPayloadBySemanticProduct =
              resolveSemanticProductPackedResultPayload(inferredValueKind, inferredStructType);
          if (resolvedPackedResultPayloadBySemanticProduct.has_value() &&
              !*resolvedPackedResultPayloadBySemanticProduct) {
            return false;
          }
          if (!resolvedPackedResultPayloadBySemanticProduct.has_value()) {
            LocalInfo::Kind collectionKind = LocalInfo::Kind::Value;
            LocalInfo::ValueKind collectionValueKind = LocalInfo::ValueKind::Unknown;
            if (resolveCollectionPayload(collectionKind, collectionValueKind) &&
                (ir_lowerer::isSupportedPackedResultCollectionKind(collectionKind) ||
                 collectionKind == LocalInfo::Kind::Value)) {
              packedKindOut = collectionValueKind;
              return true;
            }
          }
          if (!resolvedPackedResultPayloadBySemanticProduct.has_value()) {
            inferredValueKind = inferExprKind(valueExpr, valueLocals);
          }
          if (inferredValueKind == LocalInfo::ValueKind::Unknown) {
            std::string builtinComparison;
            if (getBuiltinComparisonName(valueExpr, builtinComparison)) {
              inferredValueKind = LocalInfo::ValueKind::Bool;
            }
          }
          if (isFileHandleExpr(valueExpr, valueLocals) && inferredValueKind == LocalInfo::ValueKind::Int64) {
            packedKindOut = inferredValueKind;
            return true;
          }
          if (inferredStructType.empty()) {
            ir_lowerer::inferPackedResultStructType(
                valueExpr,
                valueLocals,
                [&](const Expr &candidateExpr) { return resolveDefinitionCall(candidateExpr); },
                [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                  return inferStructExprPath(candidateExpr, candidateLocals);
                },
                inferredStructType);
          }
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
          packedKindOut = inferredValueKind;
          structTypeOut.clear();
          return ir_lowerer::isSupportedPackedResultValueKind(packedKindOut);
        };
        auto materializePackedResultStructLocal = [&](int32_t payloadLocal,
                                                      const std::string &structType,
                                                      LocalInfo &paramInfo) -> bool {
          ir_lowerer::PackedResultStructPayloadInfo payloadInfo;
          if (!ir_lowerer::resolvePackedResultStructPayloadInfo(
                  structType,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  payloadInfo)) {
            return false;
          }
          const int32_t baseLocal = nextLocal;
          nextLocal += payloadInfo.slotCount;
          const int32_t ptrLocal = nextLocal++;
          function.instructions.push_back(
              {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(payloadInfo.slotCount - 1))});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          if (payloadInfo.isPackedSingleSlot) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(payloadLocal)});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal + payloadInfo.fieldOffset)});
          } else if (!emitStructCopyFromPtrs(ptrLocal, payloadLocal, payloadInfo.slotCount)) {
            return false;
          }
          paramInfo.index = ptrLocal;
          paramInfo.kind = LocalInfo::Kind::Value;
          paramInfo.valueKind = LocalInfo::ValueKind::Int64;
          paramInfo.structTypeName = structType;
          return true;
        };
        #include "IrLowererLowerEmitExprResultHelpers.h"
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
            error,
            &callResolutionAdapters.semanticProductTargets);
        if (resultOkCallResult == ir_lowerer::ResultOkMethodCallEmitResult::Emitted) {
          return true;
        }
        if (resultOkCallResult == ir_lowerer::ResultOkMethodCallEmitResult::Error) {
          return false;
        }
        const auto resultErrorCallResult = ir_lowerer::tryEmitResultErrorCall(
            expr,
            localsIn,
            defMap,
            resolveResultExprInfo,
            [&](const Expr &valueExpr) { return resolveDefinitionCall(valueExpr); },
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            &callResolutionAdapters.semanticProductTargets,
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
            [&](const Expr &valueExpr) { return resolveDefinitionCall(valueExpr); },
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
            emitFileErrorWhy,
            &callResolutionAdapters.semanticProductTargets,
            &function.instructions,
            error);
        if (resultWhyDispatchResult == ir_lowerer::ResultWhyDispatchEmitResult::Emitted) {
          return true;
        }
        if (resultWhyDispatchResult == ir_lowerer::ResultWhyDispatchEmitResult::Error) {
          return false;
        }
        std::function<void(int32_t)> emitFileErrorWhyThunk;
        if (emitFileErrorWhy) {
          emitFileErrorWhyThunk = [&](int32_t errorLocal) {
            (void)emitFileErrorWhy(errorLocal);
          };
        }
        const auto fileErrorWhyCallResult = ir_lowerer::tryEmitFileErrorWhyCall(
            expr,
            localsIn,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) {
              return emitExpr(valueExpr, valueLocals);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            emitFileErrorWhyThunk,
            error);
        if (fileErrorWhyCallResult == ir_lowerer::FileErrorWhyCallEmitResult::Emitted) {
          return true;
        }
        if (fileErrorWhyCallResult == ir_lowerer::FileErrorWhyCallEmitResult::Error) {
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
            error,
            &callResolutionAdapters.semanticProductTargets);
        if (bufferBuiltinResult != ir_lowerer::BufferBuiltinDispatchResult::NotHandled) {
          return bufferBuiltinResult == ir_lowerer::BufferBuiltinDispatchResult::Emitted;
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "slice") &&
            expr.args.size() == 3) {
          auto emitInstruction = [&](IrOpcode opcode, uint64_t imm) {
            function.instructions.push_back({opcode, imm});
          };
          auto patchInstructionImm = [&](size_t instructionIndex, uint64_t imm) {
            function.instructions[instructionIndex].imm = imm;
          };
          auto emitSliceBoundsFailureIfTrue = [&]() {
            const size_t jumpOk = function.instructions.size();
            emitInstruction(IrOpcode::JumpIfZero, 0);
            emitArrayIndexOutOfBounds();
            patchInstructionImm(jumpOk,
                                static_cast<uint64_t>(function.instructions.size()));
          };
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(
              expr.args.front(),
              localsIn,
              {},
              semanticProgram,
              &callResolutionAdapters.semanticProductTargets.semanticIndex);
          if (!targetInfo.isArrayOrVectorTarget || targetInfo.isVectorTarget) {
            error = "slice requires array target";
            return false;
          }
          const int32_t elementSlotCount =
              targetInfo.elemSlotCount > 0 ? targetInfo.elemSlotCount : 1;
          const int32_t ptrLocal = allocTempLocal();
          if (!emitExpr(expr.args.front(), localsIn)) {
            return false;
          }
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));

          const int32_t startLocal = allocTempLocal();
          const ir_lowerer::LocalInfo::ValueKind startKind =
              inferExprKind(expr.args[1], localsIn);
          if (!emitExpr(expr.args[1], localsIn)) {
            return false;
          }
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(startLocal));

          const int32_t endLocal = allocTempLocal();
          const ir_lowerer::LocalInfo::ValueKind endKind =
              inferExprKind(expr.args[2], localsIn);
          if (!emitExpr(expr.args[2], localsIn)) {
            return false;
          }
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(endLocal));

          const int32_t countLocal = allocTempLocal();
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
          emitInstruction(IrOpcode::LoadIndirect, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal));

          if (startKind != ir_lowerer::LocalInfo::ValueKind::UInt64) {
            emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(startLocal));
            emitInstruction(IrOpcode::PushI64, 0);
            emitInstruction(IrOpcode::CmpLtI64, 0);
            emitSliceBoundsFailureIfTrue();
          }
          if (endKind != ir_lowerer::LocalInfo::ValueKind::UInt64) {
            emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(endLocal));
            emitInstruction(IrOpcode::PushI64, 0);
            emitInstruction(IrOpcode::CmpLtI64, 0);
            emitSliceBoundsFailureIfTrue();
          }

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(endLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(startLocal));
          emitInstruction(startKind == ir_lowerer::LocalInfo::ValueKind::UInt64 ||
                                  endKind == ir_lowerer::LocalInfo::ValueKind::UInt64
                              ? IrOpcode::CmpLtU64
                              : IrOpcode::CmpLtI64,
                          0);
          emitSliceBoundsFailureIfTrue();

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(endLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
          emitInstruction(endKind == ir_lowerer::LocalInfo::ValueKind::UInt64
                              ? IrOpcode::CmpGtU64
                              : IrOpcode::CmpGtI64,
                          0);
          emitSliceBoundsFailureIfTrue();

          const int32_t sliceLenLocal = allocTempLocal();
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(endLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(startLocal));
          emitInstruction(IrOpcode::SubI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sliceLenLocal));

          const int32_t totalCopySlotsLocal = allocTempLocal();
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(sliceLenLocal));
          emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(elementSlotCount));
          emitInstruction(IrOpcode::MulI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(totalCopySlotsLocal));

          const int32_t slicePtrLocal = allocTempLocal();
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(totalCopySlotsLocal));
          emitInstruction(IrOpcode::PushI64, 1);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::HeapAlloc, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(slicePtrLocal));

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(slicePtrLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(sliceLenLocal));
          emitInstruction(IrOpcode::StoreIndirect, 0);
          emitInstruction(IrOpcode::Pop, 0);

          const int32_t sourceSlotOffsetLocal = allocTempLocal();
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(startLocal));
          emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(elementSlotCount));
          emitInstruction(IrOpcode::MulI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(sourceSlotOffsetLocal));

          const int32_t copyIndexLocal = allocTempLocal();
          emitInstruction(IrOpcode::PushI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(copyIndexLocal));

          const int32_t srcSlotPtrLocal = allocTempLocal();
          const int32_t destSlotPtrLocal = allocTempLocal();
          const int32_t copyValueLocal = allocTempLocal();
          const size_t loopStart = function.instructions.size();
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(totalCopySlotsLocal));
          emitInstruction(IrOpcode::CmpLtI64, 0);
          const size_t jumpLoopEnd = function.instructions.size();
          emitInstruction(IrOpcode::JumpIfZero, 0);

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
          emitInstruction(IrOpcode::LoadLocal,
                          static_cast<uint64_t>(sourceSlotOffsetLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal));
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::PushI64, 1);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::PushI64, IrSlotBytes);
          emitInstruction(IrOpcode::MulI64, 0);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(srcSlotPtrLocal));

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(slicePtrLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal));
          emitInstruction(IrOpcode::PushI64, 1);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::PushI64, IrSlotBytes);
          emitInstruction(IrOpcode::MulI64, 0);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destSlotPtrLocal));

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(srcSlotPtrLocal));
          emitInstruction(IrOpcode::LoadIndirect, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(copyValueLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(destSlotPtrLocal));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(copyValueLocal));
          emitInstruction(IrOpcode::StoreIndirect, 0);
          emitInstruction(IrOpcode::Pop, 0);

          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(copyIndexLocal));
          emitInstruction(IrOpcode::PushI64, 1);
          emitInstruction(IrOpcode::AddI64, 0);
          emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(copyIndexLocal));
          emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(loopStart));

          patchInstructionImm(jumpLoopEnd,
                              static_cast<uint64_t>(function.instructions.size()));
          emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(slicePtrLocal));
          return true;
        }
        if (auto collectionHelpersResult = tryLowerEmitExprCollectionHelpers(
                defMap, function, nextLocal,
                emitExpr, allocTempLocal, emitStructCopySlots, resolveDefinitionCall,
                resolveExprPath, resolveStructSlotLayout, inferExprKind, inferStructExprPath,
                emitArrayIndexOutOfBounds, callResolutionAdapters, semanticProgram,
                expr, localsIn, error);
            collectionHelpersResult.has_value()) {
          return *collectionHelpersResult;
        }
        ir_lowerer::TailDispatchContext tailDispatchHelpers(setupStage, stateOut, callResolutionAdapters, error);
        #include "IrLowererLowerEmitExprTailDispatch.h"
