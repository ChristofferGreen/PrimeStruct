          if (statementsExprHelpers.resolveSameFamilyKeyValueHelperMemberName(
              expr, expr.args.front(), keyValueCountHelperName, localsIn) &&
            keyValueCountHelperName == "count" &&
            statementsExprHelpers.hasSemanticKeyValueHelperDefinition(keyValueCountHelperName)) {
            if (const auto *metadata = statementsExprHelpers.keyValueHelperMetadata();
              metadata != nullptr) {
              const std::string canonicalCountPath =
              stdlibSurfaceCanonicalHelperPath(metadata->id,
                keyValueCountHelperName);
              if (!canonicalCountPath.empty()) {
                countAccessExpr.name = canonicalCountPath;
                countAccessExpr.namespacePrefix.clear();
                countAccessExpr.semanticNodeId = 0;
                if (expr.args.front().kind != Expr::Kind::Call) {
                  if (const Definition *canonicalCountDef =
                    statementsExprHelpers.findDirectHelperDefinition(canonicalCountPath);
                    canonicalCountDef != nullptr) {
                    if (!emitInlineDefinitionCall(
                        countAccessExpr, *canonicalCountDef, localsIn, true)) {
                      return false;
                    }
                    return true;
                  }
                }
              }
            }
          }
        }  // end if (!expr.isMethodCall && expr.args.size() == 1) [key-value count fast path]

        if (!countAccessExpr.isMethodCall && countAccessExpr.args.size() == 1) {
          std::string vectorMetadataHelperName;
          const std::string vectorMetadataPath = resolveExprPath(countAccessExpr);
          if ((resolveVectorHelperAliasName(
                countAccessExpr, vectorMetadataHelperName) &&
              (vectorMetadataHelperName == "count" ||
                vectorMetadataHelperName == "capacity")) ||
            (vectorMetadataPath == primec::collection_helpers::kCanonicalVectorCount &&
              (vectorMetadataHelperName = "count", true)) ||
            (vectorMetadataPath == primec::collection_helpers::kCanonicalVectorCapacity &&
              (vectorMetadataHelperName = "capacity", true))) {
            if (const Definition *directVectorMetadataCallee =
              statementsExprHelpers.resolveDirectHelperDefinition(countAccessExpr);
              directVectorMetadataCallee != nullptr &&
              !directVectorMetadataCallee->parameters.empty()) {
              std::string receiverTypeName;
              std::vector<std::string> receiverTemplateArgs;
              if (extractFirstBindingTypeTransform(
                  directVectorMetadataCallee->parameters.front(),
                  receiverTypeName,
                  receiverTemplateArgs) &&
                normalizeCollectionBindingTypeName(receiverTypeName) ==
                "map") {
                if (!emitInlineDefinitionCall(
                    countAccessExpr,
                    *directVectorMetadataCallee,
                    localsIn,
                    true)) {
                  return false;
                }
                return true;
              }
            }
          }
        }

        const auto countAccessResult = tryEmitCountAccessCall(
          countAccessExpr,
          localsIn,
          isArrayCountCall,
          isVectorCapacityCall,
          isStringCountCall,
          isEntryArgsName,
          [&](const Expr &targetExpr, const LocalMap &targetLocals) {
            if (semanticProgram != nullptr) {
              if (const auto *queryFact =
                ir_lowerer::findSemanticProductQueryFact(
                  semanticProgram,
                  callResolutionAdapters.semanticProductTargets
                  .semanticIndex,
                  targetExpr);
                queryFact != nullptr) {
                auto resolveFactTypeText = [&](SymbolId typeTextId,
                  const std::string &fallback) {
                  if (typeTextId != InvalidSymbolId) {
                    const std::string resolvedTypeText =
                    std::string(semanticProgramResolveCallTargetString(
                        *semanticProgram, typeTextId));
                    if (!resolvedTypeText.empty()) {
                      return trimTemplateTypeText(resolvedTypeText);
                    }
                  }
                  return trimTemplateTypeText(fallback);
                };
                const std::string queryType = resolveFactTypeText(
                  queryFact->queryTypeTextId, queryFact->queryTypeText);
                const std::string bindingType = resolveFactTypeText(
                  queryFact->bindingTypeTextId,
                  queryFact->bindingTypeText);
                if (queryType == "string" || queryType == primec::collection_helpers::kRootedString ||
                  bindingType == "string" || bindingType == primec::collection_helpers::kRootedString) {
                  return false;
                }
              }
            }
            const auto targetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(
              targetExpr,
              targetLocals,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                  targetCallExpr, targetInfoOut, targetLocals);
            });
            const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
            const bool isCollectionVectorTarget =
            statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
            const bool isExperimentalKeyValueTarget =
            structPath == keyValueStorageStructRootPath() ||
            statementsExprHelpers.matchesGeneratedSpecializedType(structPath, "map", "Map");
            const bool isSemanticKeyValueTarget =
            ir_lowerer::resolveCollectionPairTypeInfo(
              targetExpr,
              targetLocals,
              {},
              semanticProgram,
              &callResolutionAdapters.semanticProductTargets.semanticIndex)
            .isKeyValueTarget;
            const bool isExperimentalSoaVectorTarget =
            structPath == collection_paths::memberPath(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
            structPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0;
            return targetInfo.isArrayOrVectorTarget || structPath == primec::collection_helpers::kRootedArray ||
            structPath == primec::collection_helpers::kRootedVector || structPath == "/Buffer" || structPath == primec::collection_helpers::kRootedMap ||
            structPath == primec::collection_helpers::kRootedSoa || isCollectionVectorTarget ||
            isExperimentalKeyValueTarget || isSemanticKeyValueTarget ||
            isExperimentalSoaVectorTarget;
          },
          [&](const Expr &targetExpr, const LocalMap &targetLocals) {
            const auto targetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(
              targetExpr,
              targetLocals,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                  targetCallExpr, targetInfoOut, targetLocals);
            });
            const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
            return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
            statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
          },
          [&](const Expr &targetExpr, const LocalMap &targetLocals) {
            const auto targetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(
              targetExpr,
              targetLocals,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                  targetCallExpr, targetInfoOut, targetLocals);
            });
            const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
            return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
            statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
          },
          inferExprKind,
          resolveStringTableTarget,
          [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
          [&](IrOpcode opcode, uint64_t imm) { function.instructions.push_back({opcode, imm}); },
          error,
          semanticProgram,
          &callResolutionAdapters.semanticProductTargets.semanticIndex);
        if (countAccessResult == CountAccessCallEmitResult::Emitted) {
          return true;
        }
        if (countAccessResult == CountAccessCallEmitResult::Error) {
          return false;
        }
        const auto countFallbackResult = tryEmitNonMethodCountFallback(
          expr,
          [&](const Expr &callExpr) { return isArrayCountCall(callExpr, localsIn); },
          [&](const Expr &callExpr) { return isStringCountCall(callExpr, localsIn); },
          [&](const Expr &callExpr) {
            return resolveMethodCallDefinition(callExpr, localsIn);
          },
          [&](const Expr &callExpr, const Definition &callee) {
            return emitInlineDefinitionCall(callExpr, callee, localsIn, true);
          },
          error);
        if (countFallbackResult == CountMethodFallbackResult::Emitted) {
          return true;
        }
        if (countFallbackResult == CountMethodFallbackResult::Error) {
          return false;
        }
        if (expr.isMethodCall) {
          auto isInternalSoaMetadataReceiver = [&](const Expr &receiver) {
            auto unwrapInternalSoaMetadataPath = [](std::string structPath) {
              structPath = trimTemplateTypeText(structPath);
              for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
                if (structPath.rfind(wrapper, 0) == 0 &&
                  structPath.size() > wrapper.size() &&
                  structPath.back() == '>') {
                  structPath = trimTemplateTypeText(
                    structPath.substr(wrapper.size(),
                      structPath.size() - wrapper.size() - 1));
                  break;
                }
              }
              return structPath;
            };
            if (receiver.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiver.name);
              if (localIt != localsIn.end()) {
                std::string localStructPath =
                unwrapInternalSoaMetadataPath(localIt->second.structTypeName);
                const size_t localTemplateStart = localStructPath.find('<');
                if (localTemplateStart != std::string::npos) {
                  localStructPath.erase(localTemplateStart);
                }
                const size_t localLeafStart = localStructPath.find_last_of('/');
                const size_t localSuffixStart =
                localStructPath.find("__",
                  localLeafStart == std::string::npos
                  ? 0
                  : localLeafStart + 1);
                if (localSuffixStart != std::string::npos) {
                  localStructPath.erase(localSuffixStart);
                }
                if (localStructPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
                  localStructPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView")) {
                  return true;
                }
              }
            }
            std::string structPath =
            unwrapInternalSoaMetadataPath(inferStructExprPath(receiver, localsIn));
            const size_t templateStart = structPath.find('<');
            if (templateStart != std::string::npos) {
              structPath.erase(templateStart);
            }
            const size_t leafStart = structPath.find_last_of('/');
            const size_t suffixStart =
            structPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
            if (suffixStart != std::string::npos) {
              structPath.erase(suffixStart);
            }
            return structPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
            structPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView");
          };
          auto emitInternalSoaMetadataBase = [&](const Expr &receiver) {
            if (receiver.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiver.name);
              if (localIt != localsIn.end() &&
                isInternalSoaMetadataReceiver(receiver)) {
                function.instructions.push_back(
                  {IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index)});
                return true;
              }
            }
            return emitExpr(receiver, localsIn);
          };
          if (expr.args.size() == 1 &&
            (isSimpleCallName(expr, "field_count") ||
              isSimpleCallName(expr, "field_capacity")) &&
            isInternalSoaMetadataReceiver(expr.args.front())) {
            if (!emitInternalSoaMetadataBase(expr.args.front())) {
              return false;
            }
            const uint64_t slotOffset =
            isSimpleCallName(expr, "field_capacity") ? IrSlotBytes * 2 : IrSlotBytes;
            function.instructions.push_back({IrOpcode::PushI64, slotOffset});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
          const std::string priorError = error;
          const Definition *methodCallee =
          resolveMethodCallDefinition(expr, localsIn);
          if (methodCallee == nullptr) {
            methodCallee = statementsExprHelpers.findDirectHelperDefinition(resolveExprPath(expr));
          }
          if (methodCallee != nullptr && expr.args.size() == 1 &&
            (isSimpleCallName(expr, "field_count") ||
              isSimpleCallName(expr, "field_capacity")) &&
            (methodCallee->fullPath.rfind(
                collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) == 0 ||
              methodCallee->fullPath.rfind(
                collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView"), 0) == 0)) {
            if (!emitInternalSoaMetadataBase(expr.args.front())) {
              return false;
            }
            const uint64_t slotOffset =
            isSimpleCallName(expr, "field_capacity") ? IrSlotBytes * 2 : IrSlotBytes;
            function.instructions.push_back({IrOpcode::PushI64, slotOffset});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            error = priorError;
            return true;
          }
          if (methodCallee != nullptr) {
            auto methodCalleeFirstParameterIsStruct = [&]() {
              if (methodCallee->parameters.empty()) {
                return false;
              }
              std::string typeName;
              std::vector<std::string> templateArgs;
              if (!extractFirstBindingTypeTransform(
                  methodCallee->parameters.front(),
                  typeName,
                  templateArgs) ||
                !templateArgs.empty()) {
                return false;
              }
              std::string resolvedStructPath;
              return resolveStructTypeName(
                typeName, methodCallee->namespacePrefix, resolvedStructPath);
            };
            auto isWrapperReturnedKeyValueAccessCall =
            [&](const Expr &candidate) {
              if (candidate.kind != Expr::Kind::Call ||
                candidate.args.size() < 2 ||
                candidate.args.front().kind != Expr::Kind::Call) {
                return false;
              }
              std::string helperName;
              if (resolveKeyValueHelperAliasName(candidate, helperName)) {
                return helperName == "at" || helperName == "at_unsafe" ||
                helperName == primec::collection_helpers::kAtRef ||
                helperName == primec::collection_helpers::kAtUnsafeRef;
              }
              auto isAccessHelperPath = [&](std::string path) {
                path = statementsExprHelpers.stripGeneratedHelperSuffix(
                  normalizeCollectionHelperPath(std::move(path)));
                return statementsExprHelpers.isKeyValueHelperMemberPath(path, "at") ||
                statementsExprHelpers.isKeyValueHelperMemberPath(path, "at_unsafe") ||
                path == "at" || path == "at_unsafe" ||
                path == primec::collection_helpers::kCanonicalMapAt ||
                path == primec::collection_helpers::kCanonicalMapAtUnsafe;
              };
              return isAccessHelperPath(candidate.name) ||
              isAccessHelperPath(statementsExprHelpers.resolveDirectHelperPath(candidate)) ||
              isAccessHelperPath(resolveExprPath(candidate));
            };
            if (expr.args.size() == 1 &&
              methodCalleeFirstParameterIsStruct() &&
              isWrapperReturnedKeyValueAccessCall(expr.args.front())) {
              error = "struct parameter type mismatch";
              return false;
            }
            if (!emitInlineDefinitionCall(expr, *methodCallee, localsIn, true)) {
              return false;
            }
            error = priorError;
            return true;
          }  // end if (methodCallee != nullptr)
          error = priorError;
        }  // end if (expr.isMethodCall)
        if (!expr.isMethodCall && statementsExprHelpers.hasKeyValueEntryCtorArgs(expr) &&
          statementsExprHelpers.isCanonicalKeyValueConstructorPath(resolveExprPath(expr))) {
          error = "native backend does not support variadic entry map constructors";
          return false;
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "capacity") &&
          expr.args.size() == 1) {
          std::string receiverCollectionName;
          const bool isDirectVectorConstructor =
          expr.args.front().kind == Expr::Kind::Call &&
          getBuiltinCollectionName(expr.args.front(), receiverCollectionName) &&
          receiverCollectionName == "vector";
          const auto targetInfo =
          ir_lowerer::resolveArrayVectorAccessTargetInfo(
            expr.args.front(),
            localsIn,
            [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                targetCallExpr, targetInfoOut, localsIn);
            },
            semanticProgram,
            &callResolutionAdapters.semanticProductTargets.semanticIndex);
          const std::string structPath = inferStructExprPath(expr.args.front(), localsIn);
          const bool isSemanticVectorTarget =
          (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
          statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
          if (!isDirectVectorConstructor &&
            (expr.args.front().kind == Expr::Kind::Call ||
              isSemanticVectorTarget)) {
            if (const Definition *directVectorMetadataCallee =
              statementsExprHelpers.resolveDirectHelperDefinition(expr);
              directVectorMetadataCallee != nullptr &&
              !directVectorMetadataCallee->parameters.empty()) {
              std::string receiverTypeName;
              std::vector<std::string> receiverTemplateArgs;
              if (extractFirstBindingTypeTransform(
                  directVectorMetadataCallee->parameters.front(),
                  receiverTypeName,
                  receiverTemplateArgs) &&
                (normalizeCollectionBindingTypeName(receiverTypeName) ==
                  "map" ||
                  normalizeCollectionBindingTypeName(receiverTypeName) ==
                  "vector")) {
                return emitInlineDefinitionCall(
                  expr, *directVectorMetadataCallee, localsIn, true);
              }
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
        }
        std::string resolvedKeyValueInsertHelperName;
        const std::string exprPath = resolveExprPath(expr);
        if (!expr.isMethodCall &&
          ((statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(exprPath) &&
              statementsExprHelpers.resolveKeyValueHelperMemberName(exprPath, resolvedKeyValueInsertHelperName) &&
              (primec::collection_helpers::isInsertHelperName(resolvedKeyValueInsertHelperName))) ||
            exprPath.rfind(collection_paths::memberPath(collection_paths::kMapFolder, "insert"), 0) == 0)) {
          if (const Definition *directCallee = statementsExprHelpers.resolveDirectHelperDefinition(expr);
            directCallee != nullptr) {
            if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
              return false;
            }
            return true;
          }
        }
        if (expr.isMethodCall && expr.args.size() == 2) {
          std::string vectorAccessName;
          if ((resolveVectorHelperAliasName(expr, vectorAccessName) ||
              getBuiltinArrayAccessName(expr, vectorAccessName) ||
              ((isSimpleCallName(expr, "at") ||
                  isSimpleCallName(expr, "at_unsafe")) &&
                (vectorAccessName = expr.name, true))) &&
            (vectorAccessName == "at" || vectorAccessName == "at_unsafe")) {
            if (const Definition *directVectorAccessCallee =
              statementsExprHelpers.resolveDirectHelperDefinition(expr);
              directVectorAccessCallee != nullptr) {
              return emitInlineDefinitionCall(
                expr, *directVectorAccessCallee, localsIn, true);
            }
            const auto arrayVectorTargetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(
              expr.args.front(),
              localsIn,
              {},
              semanticProgram,
              &callResolutionAdapters.semanticProductTargets.semanticIndex);
            const bool localVectorTarget =
            expr.args.front().kind == Expr::Kind::Name &&
            [&]() {
              auto localIt = localsIn.find(expr.args.front().name);
              return localIt != localsIn.end() &&
              (localIt->second.kind == LocalInfo::Kind::Vector ||
                localIt->second.referenceToVector ||
                localIt->second.pointerToVector ||
                statementsExprHelpers.isCollectionVectorRecordTypePath(localIt->second.structTypeName));
            }();
            if ((arrayVectorTargetInfo.isArrayOrVectorTarget &&
                arrayVectorTargetInfo.isVectorTarget) ||
              localVectorTarget) {
              return ir_lowerer::emitArrayVectorIndexedAccess(
                vectorAccessName,
                expr.args.front(),
                expr.args[1],
                localsIn,
                [&](const Expr &indexExpr, const LocalMap &indexLocals) {
                  return inferExprKind(indexExpr, indexLocals);
                },
                [&]() { return allocTempLocal(); },
                [&](const Expr &nestedExpr, const LocalMap &nestedLocals) {
                  return emitExpr(nestedExpr, nestedLocals);
                },
                [&]() { emitArrayIndexOutOfBounds(); },
                [&]() { return function.instructions.size(); },
                [&](IrOpcode op, uint64_t imm) {
                  function.instructions.push_back({op, imm});
                },
                [&](size_t indexToPatch, uint64_t target) {
                  function.instructions[indexToPatch].imm = target;
                },
                error);
            }
            // TODO-5301: a genuine named-local `Reference<map<K,V>>`
            // receiver's bracket-index access (`ref[key]`) is desugared by
            // SemanticsValidatorExprDirectCollectionFallbacks.cpp's generic
            // resolveMethodTarget fallback into this same isMethodCall
            // "at"/"at_unsafe" shape, but - unlike the array/vector case
            // above - no monomorphized `/std/collections/map/at` Definition
            // ever gets materialized into defMap for it (nothing else
            // triggers that specialization), so the defMap-based method
            // lookup this whole isMethodCall branch otherwise relies on can
            // never resolve it. The `!expr.isMethodCall` bare key-value
            // access path below already has a defMap-free native emission
            // for exactly this call shape (ir_lowerer::emitKeyValueLookupAccess);
            // mirror it here, narrowly scoped to a receiver that
            // resolveKeyValueAccessReceiverInfo actually recognizes as a
            // key-value target, so this does not change resolution for any
            // other method-call "at"/"at_unsafe" shape (e.g. a real
            // user-defined method named "at").
            const auto methodCallKeyValueTargetInfo =
            statementsExprHelpers.resolveKeyValueAccessReceiverInfo(
              expr, expr.args.front(), localsIn);
            if (methodCallKeyValueTargetInfo.isKeyValueTarget) {
              if (expr.args.front().kind == Expr::Kind::Call &&
                !inferStructExprPath(expr, localsIn).empty()) {
                error = "struct parameter type mismatch";
                return false;
              }
              return ir_lowerer::emitKeyValueLookupAccess(
                vectorAccessName,
                methodCallKeyValueTargetInfo.keyValueKeyKind,
                methodCallKeyValueTargetInfo.structTypeName,
                expr.args.front(),
                expr.args[1],
                localsIn,
                [&]() { return allocTempLocal(); },
                [&](const Expr &nestedExpr,
                  const ir_lowerer::LocalMap &nestedLocals) {
                  return emitExpr(nestedExpr, nestedLocals);
                },
                resolveStringTableTarget,
                [&](const Expr &nestedExpr,
                  const ir_lowerer::LocalMap &nestedLocals) {
                  return inferExprKind(nestedExpr, nestedLocals);
                },
                [&]() { emitMapKeyNotFound(); },
                [&]() { return function.instructions.size(); },
                [&](IrOpcode op, uint64_t imm) {
                  function.instructions.push_back({op, imm});
                },
                [&](size_t indexToPatch, uint64_t target) {
                  function.instructions[indexToPatch].imm = target;
                },
                error);
            }
          }
        }  // end if (expr.isMethodCall && expr.args.size() == 2)
        if (!expr.isMethodCall && expr.args.size() == 2) {
          std::string vectorAccessName;
          const std::string vectorAccessPath = resolveExprPath(expr);
          if (!statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(vectorAccessPath) &&
            (resolveVectorHelperAliasName(expr, vectorAccessName) ||
              getBuiltinArrayAccessName(expr, vectorAccessName) ||
              (vectorAccessPath == primec::collection_helpers::kCanonicalVectorAt &&
                (vectorAccessName = "at", true)) ||
              (vectorAccessPath == primec::collection_helpers::kCanonicalVectorAtUnsafe &&
                (vectorAccessName = "at_unsafe", true))) &&
            (vectorAccessName == "at" || vectorAccessName == "at_unsafe")) {
            // A same-path user definition overriding the canonical
            // /std/collections/vector/at(_unsafe) helper must win over the
            // native indexed-access fast path below - otherwise a
            // struct-returning override's call sites get the builtin
            // scalar-element access pattern instead of the user's own
            // body, corrupting the IR for any subsequent struct handling
            // (see TODO-4804). Mirrors the same guard already applied to
            // the method-call form immediately above.
            if (const Definition *directVectorAccessCallee =
              statementsExprHelpers.resolveDirectHelperDefinition(expr);
              directVectorAccessCallee != nullptr) {
              return emitInlineDefinitionCall(
                expr, *directVectorAccessCallee, localsIn, true);
            }
            const auto keyValueTargetInfo =
            statementsExprHelpers.resolveKeyValueAccessReceiverInfo(expr, expr.args.front(), localsIn);
            const auto arrayVectorTargetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(
              expr.args.front(),
              localsIn,
              {},
              semanticProgram,
              &callResolutionAdapters.semanticProductTargets.semanticIndex);
            if (!keyValueTargetInfo.isKeyValueTarget &&
              arrayVectorTargetInfo.isArrayOrVectorTarget) {
              return ir_lowerer::emitArrayVectorIndexedAccess(
                vectorAccessName,
                expr.args.front(),
                expr.args[1],
                localsIn,
                {},
                [&](const Expr &indexExpr, const LocalMap &indexLocals) {
                  return inferExprKind(indexExpr, indexLocals);
                },
                [&]() { return allocTempLocal(); },
                [&](const Expr &nestedExpr, const LocalMap &nestedLocals) {
                  return emitExpr(nestedExpr, nestedLocals);
                },
                [&]() { emitArrayIndexOutOfBounds(); },
                [&]() { return function.instructions.size(); },
                [&](IrOpcode op, uint64_t imm) {
                  function.instructions.push_back({op, imm});
                },
                [&](size_t indexToPatch, uint64_t target) {
                  function.instructions[indexToPatch].imm = target;
                },
                error,
                semanticProgram,
                &callResolutionAdapters.semanticProductTargets.semanticIndex);
            }
          }
        }  // end if (!expr.isMethodCall && expr.args.size() == 2)
        std::string bareKeyValueAccessName;
        const std::string bareKeyValueAccessPath = resolveExprPath(expr);
        std::string bareKeyValueAccessLeaf = bareKeyValueAccessPath;
        if (const size_t leafStart = bareKeyValueAccessLeaf.find_last_of('/');
          leafStart != std::string::npos) {
          bareKeyValueAccessLeaf = bareKeyValueAccessLeaf.substr(leafStart + 1);
        }
        if (const size_t generatedSuffix = bareKeyValueAccessLeaf.find("__");
          generatedSuffix != std::string::npos) {
          bareKeyValueAccessLeaf.erase(generatedSuffix);
        }
        const bool isCanonicalBareKeyValueAccess =
        (bareKeyValueAccessPath.rfind(primec::collection_helpers::kCanonicalMapAt, 0) == 0 ||
          bareKeyValueAccessPath.rfind("std/collections/map/at", 0) == 0);
        if (!expr.isMethodCall &&
          expr.args.size() == 2 &&
          ((getBuiltinArrayAccessName(expr, bareKeyValueAccessName) &&
              (bareKeyValueAccessName == "at" ||
                bareKeyValueAccessName == "at_unsafe")) ||
            (resolveKeyValueHelperAliasName(expr, bareKeyValueAccessName) &&
              (bareKeyValueAccessName == "at" ||
                bareKeyValueAccessName == "at_unsafe" ||
                bareKeyValueAccessName == primec::collection_helpers::kAtRef ||
                bareKeyValueAccessName == primec::collection_helpers::kAtUnsafeRef)) ||
            (isCanonicalBareKeyValueAccess &&
              (bareKeyValueAccessName = bareKeyValueAccessLeaf, true)) ||
            ((isSimpleCallName(expr, "at") ||
                isSimpleCallName(expr, "at_unsafe")) &&
              (bareKeyValueAccessName = expr.name, true))) &&
          (primec::collection_helpers::isAtHelperName(bareKeyValueAccessName) ||
            primec::collection_helpers::isAtUnsafeHelperName(bareKeyValueAccessName))) {
          if (bareKeyValueAccessName == primec::collection_helpers::kAtRef) {
            bareKeyValueAccessName = "at";
          } else if (bareKeyValueAccessName == primec::collection_helpers::kAtUnsafeRef) {
            bareKeyValueAccessName = "at_unsafe";
          }
          auto resolveAccessTargetInfo = [&](const Expr &receiverExpr) {
            return statementsExprHelpers.resolveKeyValueAccessReceiverInfo(expr, receiverExpr, localsIn);
          };
          auto targetInfo = resolveAccessTargetInfo(expr.args.front());
          size_t receiverArgIndex = 0;
          if (!targetInfo.isKeyValueTarget && expr.args.size() > 1) {
            auto alternateTargetInfo = resolveAccessTargetInfo(expr.args[1]);
            if (alternateTargetInfo.isKeyValueTarget) {
              targetInfo = std::move(alternateTargetInfo);
              receiverArgIndex = 1;
            }
          }
          if (isCanonicalBareKeyValueAccess && !targetInfo.isKeyValueTarget) {
            const auto arrayVectorTargetInfo =
            ir_lowerer::resolveArrayVectorAccessTargetInfo(
              expr.args.front(),
              localsIn,
              {},
              semanticProgram,
              &callResolutionAdapters.semanticProductTargets.semanticIndex);
            if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
              return ir_lowerer::emitArrayVectorIndexedAccess(
                bareKeyValueAccessName,
                expr.args.front(),
                expr.args[1],
                localsIn,
                [&](const Expr &indexExpr, const LocalMap &indexLocals) {
                  return inferExprKind(indexExpr, indexLocals);
                },
                [&]() { return allocTempLocal(); },
                [&](const Expr &nestedExpr, const LocalMap &nestedLocals) {
                  return emitExpr(nestedExpr, nestedLocals);
                },
                [&]() { emitArrayIndexOutOfBounds(); },
                [&]() { return function.instructions.size(); },
                [&](IrOpcode op, uint64_t imm) {
                  function.instructions.push_back({op, imm});
                },
                [&](size_t indexToPatch, uint64_t target) {
                  function.instructions[indexToPatch].imm = target;
                },
                error);
            }
          }
          if (targetInfo.isKeyValueTarget) {
            Expr accessExpr = expr;
            if (receiverArgIndex != 0 && accessExpr.args.size() > receiverArgIndex) {
              std::swap(accessExpr.args[0], accessExpr.args[receiverArgIndex]);
            }
            if (bareKeyValueAccessName == "at" ||
              bareKeyValueAccessName == "at_unsafe") {
              if (accessExpr.args.front().kind == Expr::Kind::Call &&
                !inferStructExprPath(expr, localsIn).empty()) {
                error = "struct parameter type mismatch";
                return false;
              }
              if (!ir_lowerer::emitKeyValueLookupAccess(
                  bareKeyValueAccessName,
                  targetInfo.keyValueKeyKind,
                  targetInfo.structTypeName,
                  accessExpr.args.front(),
                  accessExpr.args[1],
                  localsIn,
                  [&]() { return allocTempLocal(); },
                  [&](const Expr &nestedExpr,
                    const ir_lowerer::LocalMap &nestedLocals) {
                    return emitExpr(nestedExpr, nestedLocals);
                  },
                  resolveStringTableTarget,
                  [&](const Expr &nestedExpr,
                    const ir_lowerer::LocalMap &nestedLocals) {
                    return inferExprKind(nestedExpr, nestedLocals);
                  },
                  [&]() { emitMapKeyNotFound(); },
                  [&]() { return function.instructions.size(); },
                  [&](IrOpcode op, uint64_t imm) {
                    function.instructions.push_back({op, imm});
                  },
                  [&](size_t indexToPatch, uint64_t target) {
                    function.instructions[indexToPatch].imm = target;
                  },
                  error)) {
                return false;
              }
              return true;
            }
            const std::string priorError = error;
            if (const Definition *directCallee =
              statementsExprHelpers.resolveDirectHelperDefinition(accessExpr);
              directCallee != nullptr && !isCanonicalBareKeyValueAccess) {
              error = priorError;
              return emitInlineDefinitionCall(
                accessExpr, *directCallee, localsIn, true);
            }
            std::string receiverStructPath = targetInfo.structTypeName;
            if (receiverStructPath.empty()) {
              receiverStructPath =
              inferStructExprPath(accessExpr.args.front(), localsIn);
            }
            Expr methodExpr = accessExpr;
            methodExpr.name = statementsExprHelpers.keyValueImplementationMethodSpelling(
              receiverStructPath, bareKeyValueAccessName);
            methodExpr.namespacePrefix.clear();
            methodExpr.isMethodCall = true;
            methodExpr.semanticNodeId = 0;
            auto emitAccessMethodCall =
            [&](const Definition &methodCallee) -> bool {
              if (!receiverStructPath.empty() &&
                methodCallee.parameters.size() + 1 == methodExpr.args.size()) {
                Definition calleeWithThis = methodCallee;
                calleeWithThis.isNested = false;
                calleeWithThis.parameters.insert(
                  calleeWithThis.parameters.begin(),
                  ir_lowerer::makeStructHelperThisParam(
                    receiverStructPath,
                    definitionHasTransform(methodCallee, "mut")));
                return emitInlineDefinitionCall(
                  methodExpr, calleeWithThis, localsIn, true);
              }
              return emitInlineDefinitionCall(
                methodExpr, methodCallee, localsIn, true);
            };
            if (const Definition *methodCallee =
              resolveMethodCallDefinition(methodExpr, localsIn);
              methodCallee != nullptr) {
              error = priorError;
              return emitAccessMethodCall(*methodCallee);
            }
            if (!receiverStructPath.empty()) {
              Expr methodLookup;
              methodLookup.kind = Expr::Kind::Call;
              methodLookup.name = receiverStructPath + "/" + methodExpr.name;
              if (const Definition *methodCallee =
                resolveDefinitionCall(methodLookup);
                methodCallee != nullptr) {
                error = priorError;
                return emitAccessMethodCall(*methodCallee);
              }
            }
            error = priorError;
            return emitExpr(methodExpr, localsIn);
          }  // end if (targetInfo.isKeyValueTarget)
        }
        if (!expr.isMethodCall && expr.args.size() == 1) {
          std::string vectorMetadataHelperName;
          const std::string vectorMetadataPath = resolveExprPath(expr);
          if (((resolveVectorHelperAliasName(expr, vectorMetadataHelperName) &&
                (vectorMetadataHelperName == "count" ||
                  vectorMetadataHelperName == "capacity")) ||
              (vectorMetadataPath == primec::collection_helpers::kCanonicalVectorCount &&
                (vectorMetadataHelperName = "count", true)) ||
              (vectorMetadataPath == primec::collection_helpers::kCanonicalVectorCapacity &&
                (vectorMetadataHelperName = "capacity", true)))) {
            if (const Definition *directVectorMetadataCallee =
              statementsExprHelpers.resolveDirectHelperDefinition(expr);
              directVectorMetadataCallee != nullptr &&
              !directVectorMetadataCallee->parameters.empty()) {
              std::string receiverTypeName;
              std::vector<std::string> receiverTemplateArgs;
              if (extractFirstBindingTypeTransform(
                  directVectorMetadataCallee->parameters.front(),
                  receiverTypeName,
                  receiverTemplateArgs) &&
                (normalizeCollectionBindingTypeName(receiverTypeName) ==
                  "map" ||
                  normalizeCollectionBindingTypeName(receiverTypeName) ==
                  "vector")) {
                return emitInlineDefinitionCall(
                  expr, *directVectorMetadataCallee, localsIn, true);
              }
            }
            auto metadataTargetReturnsString = [&]() {
              if (vectorMetadataHelperName != "count" ||
                semanticProgram == nullptr) {
                return false;
              }
              const Expr &targetExpr = expr.args.front();
              if (inferExprKind(targetExpr, localsIn) ==
                LocalInfo::ValueKind::String) {
                return true;
              }
              const auto *queryFact =
              ir_lowerer::findSemanticProductQueryFact(
                semanticProgram,
                callResolutionAdapters.semanticProductTargets
                .semanticIndex,
                targetExpr);
              if (queryFact == nullptr) {
                return false;
              }
              auto resolveFactTypeText = [&](SymbolId typeTextId,
                const std::string &fallback) {
                if (typeTextId != InvalidSymbolId) {
                  const std::string resolvedTypeText = std::string(
                    semanticProgramResolveCallTargetString(
                      *semanticProgram, typeTextId));
                  if (!resolvedTypeText.empty()) {
                    return trimTemplateTypeText(resolvedTypeText);
                  }
                }
                return trimTemplateTypeText(fallback);
              };
              const std::string queryType = resolveFactTypeText(
                queryFact->queryTypeTextId, queryFact->queryTypeText);
              const std::string bindingType = resolveFactTypeText(
                queryFact->bindingTypeTextId,
                queryFact->bindingTypeText);
              return queryType == "string" || queryType == primec::collection_helpers::kRootedString ||
              bindingType == "string" || bindingType == primec::collection_helpers::kRootedString;
            };
            if (metadataTargetReturnsString()) {
              if (const Definition *stringCountCallee =
                statementsExprHelpers.findDirectHelperDefinition(primec::collection_helpers::kRootedStringCount);
                stringCountCallee != nullptr) {
                Expr stringCountExpr = expr;
                stringCountExpr.name = primec::collection_helpers::kRootedStringCount;
                stringCountExpr.namespacePrefix.clear();
                stringCountExpr.semanticNodeId = 0;
                return emitInlineDefinitionCall(
                  stringCountExpr, *stringCountCallee, localsIn, true);
              }
              if (!emitExpr(expr.args.front(), localsIn)) {
                return false;
              }
              function.instructions.push_back({IrOpcode::LoadStringLength, 0});
              return true;
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            if (vectorMetadataHelperName == "capacity") {
              function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
              function.instructions.push_back({IrOpcode::AddI64, 0});
            }
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
        }  // end if (!expr.isMethodCall && expr.args.size() == 1) [vector count/capacity fast path]
        error =
        "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
        resolveExprPath(expr) + ", name=" + expr.name +
        ", args=" + std::to_string(expr.args.size()) +
        ", method=" + std::string(expr.isMethodCall ? "true" : "false") + ")";
        return false;
      }
      default:
      error = "native backend only supports literals, names, and calls";
      return false;
    }
  };
