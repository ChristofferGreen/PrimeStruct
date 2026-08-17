// soa-surface-audit: exempt
// collection-surface-audit: exempt
        const SemanticProductIndex *const tailDispatchSemanticIndexPtr =
            semanticProgram == nullptr ? nullptr
                                       : &callResolutionAdapters.semanticProductTargets.semanticIndex;
        const SemanticProductIndex *const tailDispatchKeyValueSemanticIndexPtr =
            tailDispatchSemanticIndexPtr;
        Expr inlineDispatchExpr = expr;
        Expr rewrittenBuiltinKeyValueInsertBuiltinExpr;
        if (tailDispatchHelpers.rewriteBuiltinKeyValueInsertBuiltinExpr(expr, rewrittenBuiltinKeyValueInsertBuiltinExpr, localsIn)) {
          inlineDispatchExpr = rewrittenBuiltinKeyValueInsertBuiltinExpr;
        }
        std::string inlineDispatchRawPath =
            tailDispatchHelpers.resolveTailDispatchDirectHelperPath(inlineDispatchExpr);
        if (const size_t generatedSuffix = inlineDispatchRawPath.find(
                "__", inlineDispatchRawPath.find_last_of('/') + 1);
            generatedSuffix != std::string::npos) {
          inlineDispatchRawPath.erase(generatedSuffix);
        }
        if (!inlineDispatchExpr.isMethodCall &&
            inlineDispatchExpr.args.size() == 1 &&
            (inlineDispatchRawPath == "/std/collections/soa/count" ||
             findSemanticProductDirectCallTarget(
                 semanticProgram, inlineDispatchExpr) ==
                 "/std/collections/soa/count")) {
          if (!emitExpr(inlineDispatchExpr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::PushI64, 2ull * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (inlineDispatchExpr.isMethodCall &&
            inlineDispatchExpr.args.size() == 1 &&
            inlineDispatchExpr.args.front().kind == Expr::Kind::Name &&
            [&]() {
              auto localIt =
                  localsIn.find(inlineDispatchExpr.args.front().name);
              return localIt != localsIn.end() &&
                     localIt->second.isSoaVector;
            }() &&
            (inlineDispatchRawPath == "count" ||
             inlineDispatchRawPath == "/std/collections/soa/count" ||
             findSemanticProductMethodCallTarget(
                 semanticProgram, inlineDispatchExpr) ==
                 "/std/collections/soa/count")) {
          if (!emitExpr(inlineDispatchExpr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::PushI64, 2ull * IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        }
        if (!inlineDispatchExpr.isMethodCall &&
            inlineDispatchExpr.args.size() == 2 &&
            (inlineDispatchRawPath == "/std/collections/soa/get" ||
             inlineDispatchRawPath == "/std/collections/soa/ref" ||
             findSemanticProductDirectCallTarget(
                 semanticProgram, inlineDispatchExpr) ==
                 "/std/collections/soa/get" ||
             findSemanticProductDirectCallTarget(
                 semanticProgram, inlineDispatchExpr) ==
                 "/std/collections/soa/ref")) {
          const bool returnsReference =
              inlineDispatchRawPath == "/std/collections/soa/ref" ||
              findSemanticProductDirectCallTarget(
                  semanticProgram, inlineDispatchExpr) ==
                  "/std/collections/soa/ref";
          const auto targetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(
                  inlineDispatchExpr.args.front(),
                  localsIn,
                  {},
                  semanticProgram,
                  tailDispatchSemanticIndexPtr);
          const int32_t storagePtrLocal = allocTempLocal();
          if (!emitExpr(inlineDispatchExpr.args.front(), localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back(
              {IrOpcode::StoreLocal,
               static_cast<uint64_t>(storagePtrLocal)});
          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(inlineDispatchExpr.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
          // targetInfo.elemSlotCount reflects the SoaVector<T> local's own
          // slot metadata, not the element type T's slot count, so it is
          // almost always 0/1 here even when T is a multi-slot struct.
          // Resolve T's real slot count via the "storage" field's template
          // argument (SoaVector<T>.storage : SoaColumn<T>) so indexing into
          // the backing buffer uses T's actual per-element stride.
          int32_t elementSlotCount =
              targetInfo.elemSlotCount > 0 ? targetInfo.elemSlotCount : 1;
          std::string soaContainerStructTypeName = targetInfo.structTypeName;
          if (soaContainerStructTypeName.empty() &&
              inlineDispatchExpr.args.front().kind == Expr::Kind::Name) {
            auto receiverLocalIt =
                localsIn.find(inlineDispatchExpr.args.front().name);
            if (receiverLocalIt != localsIn.end()) {
              soaContainerStructTypeName = receiverLocalIt->second.structTypeName;
            }
          }
          // Walk SoaVector<T>.storage (: SoaColumn<T>) -> SoaColumn<T>.data
          // (: Pointer<uninitialized<T>>) to recover T's own struct path, since
          // after monomorphization these field bindings carry already-specialized
          // type names rather than template-argument text.
          auto resolveFieldStructPath = [&](const std::string &ownerStructPath,
                                            const std::string &fieldName,
                                            std::string &fieldStructPathOut) {
            auto ownerDefIt = defMap.find(ownerStructPath);
            if (ownerDefIt == defMap.end() || ownerDefIt->second == nullptr) {
              return false;
            }
            for (const auto &fieldExpr : ownerDefIt->second->statements) {
              if (!fieldExpr.isBinding || fieldExpr.name != fieldName) {
                continue;
              }
              std::string fieldTypeName;
              std::vector<std::string> fieldTemplateArgs;
              if (!ir_lowerer::extractFirstBindingTypeTransform(
                      fieldExpr, fieldTypeName, fieldTemplateArgs)) {
                return false;
              }
              if (fieldTemplateArgs.empty()) {
                // Already-specialized field type (e.g. SoaColumn__tHASH).
                fieldStructPathOut = fieldTypeName;
                if (!fieldStructPathOut.empty() && fieldStructPathOut.front() != '/') {
                  fieldStructPathOut.insert(fieldStructPathOut.begin(), '/');
                }
                return !fieldStructPathOut.empty();
              }
              return resolveStructTypeName(
                  ir_lowerer::trimTemplateTypeText(fieldTemplateArgs.front()),
                  "",
                  fieldStructPathOut);
            }
            return false;
          };
          if (targetInfo.elemKind == LocalInfo::ValueKind::Unknown &&
              !soaContainerStructTypeName.empty()) {
            std::string columnStructPath;
            if (resolveFieldStructPath(soaContainerStructTypeName, "storage", columnStructPath)) {
              auto columnDefIt = defMap.find(columnStructPath);
              if (columnDefIt != defMap.end() && columnDefIt->second != nullptr) {
                for (const auto &fieldExpr : columnDefIt->second->statements) {
                  if (!fieldExpr.isBinding || fieldExpr.name != "data") {
                    continue;
                  }
                  std::string dataTypeName;
                  std::vector<std::string> dataTemplateArgs;
                  if (ir_lowerer::extractFirstBindingTypeTransform(
                          fieldExpr, dataTypeName, dataTemplateArgs) &&
                      dataTemplateArgs.size() == 1) {
                    std::string elementTypeText = dataTemplateArgs.front();
                    ir_lowerer::extractTopLevelUninitializedTypeText(elementTypeText, elementTypeText);
                    std::string elementStructPath;
                    if (resolveStructTypeName(
                            ir_lowerer::trimTemplateTypeText(elementTypeText),
                            "",
                            elementStructPath)) {
                      ir_lowerer::StructSlotLayoutInfo elementLayout;
                      if (resolveStructSlotLayout(elementStructPath, elementLayout) &&
                          elementLayout.totalSlots > 0) {
                        elementSlotCount = elementLayout.totalSlots;
                      }
                    }
                  }
                  break;
                }
              }
            }
          }
          ir_lowerer::emitArrayVectorAccessLoad(
              "at",
              storagePtrLocal,
              indexLocal,
              inferExprKind(inlineDispatchExpr.args[1], localsIn),
              true,
              1,
              elementSlotCount,
              !returnsReference &&
                  targetInfo.elemKind != LocalInfo::ValueKind::Unknown,
              [&]() { return allocTempLocal(); },
              [&]() { emitArrayIndexOutOfBounds(); },
              [&]() { return function.instructions.size(); },
              [&](IrOpcode op, uint64_t imm) {
                function.instructions.push_back({op, imm});
              },
              [&](size_t indexToPatch, uint64_t target) {
                function.instructions[indexToPatch].imm = target;
              });
          return true;
        }
        Expr rewrittenSameFamilyKeyValueCountExpr;
        if (tailDispatchHelpers.rewriteSameFamilyKeyValueCountExpr(
                inlineDispatchExpr, rewrittenSameFamilyKeyValueCountExpr, localsIn)) {
          inlineDispatchExpr = rewrittenSameFamilyKeyValueCountExpr;
        }
        Expr rewrittenCanonicalExperimentalKeyValueHelperExpr;
        if (tailDispatchHelpers.rewriteCanonicalKeyValueHelperForExperimentalReceiverExpr(
                inlineDispatchExpr, rewrittenCanonicalExperimentalKeyValueHelperExpr, localsIn)) {
          inlineDispatchExpr = rewrittenCanonicalExperimentalKeyValueHelperExpr;
        }
        Expr rewrittenInlineExplicitKeyValueHelperExpr;
        if (tailDispatchHelpers.rewriteExplicitKeyValueHelperBuiltinExpr(
                inlineDispatchExpr, rewrittenInlineExplicitKeyValueHelperExpr, localsIn)) {
          inlineDispatchExpr = rewrittenInlineExplicitKeyValueHelperExpr;
        }
        Expr rewrittenMethodCanonicalKeyValueHelperDefinitionExpr;
        if (tailDispatchHelpers.rewriteCanonicalKeyValueHelperDefinitionExpr(
                inlineDispatchExpr,
                rewrittenMethodCanonicalKeyValueHelperDefinitionExpr, localsIn)) {
          inlineDispatchExpr = rewrittenMethodCanonicalKeyValueHelperDefinitionExpr;
        }
        auto emitInternalSoaMetadataBeforeInline = [&]() -> std::optional<bool> {
          auto metadataLeaf = [](const Expr &callExpr) {
            std::string path = callExpr.name;
            if (path.find('/') == std::string::npos &&
                !callExpr.namespacePrefix.empty()) {
              path = callExpr.namespacePrefix == "/"
                         ? "/" + path
                         : callExpr.namespacePrefix + "/" + path;
            }
            const size_t leafStart = path.find_last_of('/');
            std::string leaf =
                leafStart == std::string::npos ? path : path.substr(leafStart + 1);
            const size_t generatedSuffix = leaf.find("__");
            if (generatedSuffix != std::string::npos) {
              leaf.erase(generatedSuffix);
            }
            if (leaf == "field_count" || leaf == "field_capacity") {
              return leaf;
            }
            return std::string{};
          }(inlineDispatchExpr);
          if (metadataLeaf.empty() || inlineDispatchExpr.args.size() != 1) {
            return std::nullopt;
          }
          auto normalizeInternalSoaType = [](std::string typeText) {
            typeText = ir_lowerer::trimTemplateTypeText(typeText);
            for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
              if (typeText.rfind(wrapper, 0) == 0 &&
                  typeText.size() > wrapper.size() &&
                  typeText.back() == '>') {
                typeText = ir_lowerer::trimTemplateTypeText(
                    typeText.substr(wrapper.size(),
                                    typeText.size() - wrapper.size() - 1));
                break;
              }
            }
            const size_t templateStart = typeText.find('<');
            if (templateStart != std::string::npos) {
              typeText.erase(templateStart);
            }
            const size_t leafStart = typeText.find_last_of('/');
            const size_t suffixStart =
                typeText.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
            if (suffixStart != std::string::npos) {
              typeText.erase(suffixStart);
            }
            return typeText;
          };
          auto isInternalSoaType = [&](const std::string &typeText) {
            const std::string normalized = normalizeInternalSoaType(typeText);
            return normalized == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
                   normalized == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView");
          };
          const Expr &receiverExpr = inlineDispatchExpr.args.front();
          auto classifyInternalReceiverFromSemanticFacts = [&]() -> std::optional<bool> {
            if (semanticProgram == nullptr || tailDispatchSemanticIndexPtr == nullptr) {
              return std::nullopt;
            }
            if (const auto *queryFact = ir_lowerer::findSemanticProductQueryFact(
                    semanticProgram, *tailDispatchSemanticIndexPtr, inlineDispatchExpr);
                queryFact != nullptr) {
              return isInternalSoaType(tailDispatchHelpers.resolveSemanticReceiverTypeText(
                  queryFact->receiverBindingTypeText,
                  queryFact->receiverBindingTypeTextId));
            }
            return std::nullopt;
          };
          bool hasInternalReceiver = false;
          if (const std::optional<bool> semanticInternalReceiver =
                  classifyInternalReceiverFromSemanticFacts();
              semanticInternalReceiver.has_value()) {
            hasInternalReceiver = *semanticInternalReceiver;
          } else if (receiverExpr.kind == Expr::Kind::Name) {
            auto localIt = localsIn.find(receiverExpr.name);
            hasInternalReceiver =
                localIt != localsIn.end() &&
                isInternalSoaType(localIt->second.structTypeName);
          }
          if (!hasInternalReceiver) {
            return std::nullopt;
          }
          if (!emitExpr(receiverExpr, localsIn)) {
            return false;
          }
          const uint64_t slotOffset =
              metadataLeaf == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
          function.instructions.push_back({IrOpcode::PushI64, slotOffset});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        };
        if (const std::optional<bool> metadataResult =
                emitInternalSoaMetadataBeforeInline();
            metadataResult.has_value()) {
          return *metadataResult;
        }
        auto emitVectorIndexedAccessBeforeInline = [&]() -> std::optional<bool> {
          if (inlineDispatchExpr.kind != Expr::Kind::Call ||
              inlineDispatchExpr.args.size() != 2) {
            return std::nullopt;
          }
          std::string accessName;
          const std::string resolvedAccessPath = resolveExprPath(inlineDispatchExpr);
          if (!(getBuiltinArrayAccessName(inlineDispatchExpr, accessName) ||
                resolveVectorHelperAliasName(inlineDispatchExpr, accessName) ||
                (resolvedAccessPath == "/std/collections/vector/at" &&
                 (accessName = "at", true)) ||
                (resolvedAccessPath == "/std/collections/vector/at_unsafe" &&
                 (accessName = "at_unsafe", true)))) {
            return std::nullopt;
          }
          if (accessName != "at" && accessName != "at_unsafe") {
            return std::nullopt;
          }
          const auto targetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(
                  inlineDispatchExpr.args.front(),
                  localsIn,
                  {},
                  semanticProgram,
                  tailDispatchSemanticIndexPtr);
          if (!targetInfo.isArrayOrVectorTarget || !targetInfo.isVectorTarget) {
            return std::nullopt;
          }
          if (inlineDispatchExpr.isMethodCall) {
            const std::string methodResolvedPath =
                findSemanticProductMethodCallTarget(semanticProgram, inlineDispatchExpr);
            const Definition *methodCallee =
                tailDispatchHelpers.resolveTailDispatchDirectHelperDefinition(inlineDispatchExpr);
            if (!methodResolvedPath.empty() && methodCallee != nullptr &&
                (!methodCallee->statements.empty() ||
                 methodCallee->hasReturnStatement ||
                 methodCallee->returnExpr.has_value())) {
              return std::nullopt;
            }
            if (!methodResolvedPath.empty() &&
                !semanticKeyValueAccessHelperKeepsBuiltinReturn(semanticProgram,
                                                                methodResolvedPath)) {
              return std::nullopt;
            }
          } else {
            // A same-path user definition overriding the canonical
            // /std/collections/vector/at(_unsafe) direct-call helper must
            // win over the builtin indexed-access fast path below, exactly
            // like the method-call form just above - otherwise a
            // struct-returning override's call sites get the builtin
            // scalar-element access pattern instead of the user's own
            // body, corrupting the IR for any subsequent struct handling
            // (see TODO-4804).
            const Definition *directCallee =
                tailDispatchHelpers.resolveTailDispatchDirectHelperDefinition(inlineDispatchExpr);
            if (directCallee != nullptr &&
                (!directCallee->statements.empty() ||
                 directCallee->hasReturnStatement ||
                 directCallee->returnExpr.has_value())) {
              return std::nullopt;
            }
          }
          return ir_lowerer::emitArrayVectorIndexedAccess(
              accessName,
              inlineDispatchExpr.args.front(),
              inlineDispatchExpr.args[1],
              localsIn,
              {},
              [&](const Expr &indexExpr, const ir_lowerer::LocalMap &indexLocals) {
                return inferExprKind(indexExpr, indexLocals);
              },
              [&]() { return allocTempLocal(); },
              [&](const Expr &nestedExpr, const ir_lowerer::LocalMap &nestedLocals) {
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
              tailDispatchSemanticIndexPtr);
        };
        if (const std::optional<bool> vectorAccessResult =
                emitVectorIndexedAccessBeforeInline();
            vectorAccessResult.has_value()) {
          return *vectorAccessResult;
        }
        if (!inlineDispatchExpr.isMethodCall &&
            inlineDispatchExpr.args.size() == 1) {
          std::string vectorMetadataHelperName;
          const std::string vectorMetadataPath =
              resolveExprPath(inlineDispatchExpr);
          if ((resolveVectorHelperAliasName(
                   inlineDispatchExpr, vectorMetadataHelperName) &&
               (vectorMetadataHelperName == "count" ||
                vectorMetadataHelperName == "capacity")) ||
              (vectorMetadataPath == "/std/collections/vector/count" &&
               (vectorMetadataHelperName = "count", true)) ||
              (vectorMetadataPath == "/std/collections/vector/capacity" &&
               (vectorMetadataHelperName = "capacity", true))) {
            const Definition *directVectorMetadataCallee =
                tailDispatchHelpers.resolveTailDispatchDirectHelperDefinition(inlineDispatchExpr);
            if (directVectorMetadataCallee == nullptr) {
              auto directDefIt = defMap.find(vectorMetadataPath);
              if (directDefIt != defMap.end()) {
                directVectorMetadataCallee = directDefIt->second;
              }
            }
            if (directVectorMetadataCallee != nullptr &&
                inlineDispatchExpr.args.front().kind == Expr::Kind::Call) {
              if (!emitInlineDefinitionCall(
                      inlineDispatchExpr,
                      *directVectorMetadataCallee,
                      localsIn,
                      true)) {
                return false;
              }
              return true;
            }
            if (directVectorMetadataCallee != nullptr &&
                !directVectorMetadataCallee->parameters.empty()) {
              std::string receiverTypeName;
              std::vector<std::string> receiverTemplateArgs;
              if (ir_lowerer::extractFirstBindingTypeTransform(
                      directVectorMetadataCallee->parameters.front(),
                      receiverTypeName,
                      receiverTemplateArgs) &&
                  (ir_lowerer::normalizeCollectionBindingTypeName(
                       receiverTypeName) == "map" ||
                   ir_lowerer::normalizeCollectionBindingTypeName(
                       receiverTypeName) == "vector" ||
                   ir_lowerer::isBuiltinCollectionTypeName(receiverTypeName, "array"))) {
                if (!emitInlineDefinitionCall(
                        inlineDispatchExpr,
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
        const auto inlineDispatchResult = ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            inlineDispatchExpr,
            localsIn,
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return primec::ir_lowerer::isArrayCountCall(callExpr,
                                                          localMap,
                                                          hasEntryArgs,
                                                          entryArgsName,
                                                          semanticProgram,
                                                          tailDispatchSemanticIndexPtr);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return primec::ir_lowerer::isStringCountCall(callExpr,
                                                           localMap,
                                                           semanticProgram,
                                                           tailDispatchSemanticIndexPtr);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return primec::ir_lowerer::isVectorCapacityCall(callExpr,
                                                              localMap,
                                                              semanticProgram,
                                                              tailDispatchSemanticIndexPtr);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return resolveMethodCallDefinition(callExpr, localMap);
            },
            [&](const Expr &callExpr) {
              return tailDispatchHelpers.resolveTailDispatchDirectHelperDefinition(callExpr);
            },
            [&](const Expr &callExpr, const Definition &callee, const ir_lowerer::LocalMap &localMap) {
              return emitInlineDefinitionCall(callExpr, callee, localMap, true);
            },
            error,
            semanticProgram,
            [&](const Expr &candidateExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(candidateExpr, localMap);
            },
            tailDispatchSemanticIndexPtr);
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Emitted) {
          return true;
        }
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Error) {
          if (error.empty()) {
            if (inlineDispatchExpr.isMethodCall && semanticProgram != nullptr) {
              const std::string semanticTarget =
                  findSemanticProductMethodCallTarget(semanticProgram, inlineDispatchExpr);
              if (!semanticTarget.empty()) {
                error = "semantic-product method-call target missing lowered definition: " +
                        semanticTarget;
              }
            }
            if (error.empty()) {
              error = "inline dispatch failed without diagnostic: " +
                      inlineDispatchExpr.name;
            }
          }
          return false;
        }
        Expr nativeTailExpr = inlineDispatchExpr;
        Expr rewrittenExplicitKeyValueHelperExpr;
        if (tailDispatchHelpers.rewriteExplicitKeyValueHelperBuiltinExpr(nativeTailExpr, rewrittenExplicitKeyValueHelperExpr, localsIn)) {
          nativeTailExpr = rewrittenExplicitKeyValueHelperExpr;
        }
        Expr rewrittenBorrowedKeyValueReceiverExpr;
        if (tailDispatchHelpers.rewriteImplicitBorrowedKeyValueReceiverExpr(nativeTailExpr, rewrittenBorrowedKeyValueReceiverExpr, localsIn)) {
          nativeTailExpr = rewrittenBorrowedKeyValueReceiverExpr;
        }
        auto emitInternalSoaMetadataCall = [&]() -> std::optional<bool> {
          const std::string metadataLeaf =
              tailDispatchHelpers.internalSoaMetadataCallLeaf(nativeTailExpr);
          if (metadataLeaf.empty() || nativeTailExpr.args.size() != 1 ||
              !tailDispatchHelpers.isInternalSoaMetadataReceiver(nativeTailExpr.args.front(),
                                             nativeTailExpr, localsIn)) {
            return std::nullopt;
          }
          if (!emitExpr(nativeTailExpr.args.front(), localsIn)) {
            return false;
          }
          const uint64_t slotOffset =
              metadataLeaf == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
          function.instructions.push_back({IrOpcode::PushI64, slotOffset});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        };
        if (const std::optional<bool> metadataResult =
                emitInternalSoaMetadataCall();
            metadataResult.has_value()) {
          return *metadataResult;
        }

        const auto nativeTailResult = ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            nativeTailExpr,
            localsIn,
            [&](const Expr &callExpr, std::string &mathBuiltinName) {
              return tailDispatchHelpers.resolveCanonicalMathBuiltinName(callExpr, mathBuiltinName);
            },
            [&](const std::string &mathBuiltinName) {
              return ir_lowerer::isSupportedMathBuiltinName(mathBuiltinName);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return primec::ir_lowerer::isArrayCountCall(callExpr,
                                                          localMap,
                                                          hasEntryArgs,
                                                          entryArgsName,
                                                          semanticProgram,
                                                          tailDispatchSemanticIndexPtr);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return primec::ir_lowerer::isVectorCapacityCall(callExpr,
                                                              localMap,
                                                              semanticProgram,
                                                              tailDispatchSemanticIndexPtr);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return primec::ir_lowerer::isStringCountCall(callExpr,
                                                           localMap,
                                                           semanticProgram,
                                                           tailDispatchSemanticIndexPtr);
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
            [&](const Expr &targetCallExpr, ir_lowerer::CollectionPairTypeInfo &targetInfoOut) {
              targetInfoOut = ir_lowerer::resolveCollectionPairTypeInfo(
                  targetCallExpr,
                  localsIn,
                  [&](const Expr &keyValueTargetExpr, ir_lowerer::CollectionPairTypeInfo &keyValueTargetInfoOut) {
                    return tailDispatchHelpers.inferCallKeyValueTargetInfo(
                        keyValueTargetExpr, keyValueTargetInfoOut, localsIn);
                  },
                  semanticProgram,
                  tailDispatchKeyValueSemanticIndexPtr);
              return targetInfoOut.isKeyValueTarget;
            },
            [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              auto resolveSpecializedVectorElementKind = [&](const std::string &typeText,
                                                            ir_lowerer::LocalInfo::ValueKind &elemKindOut) {
                elemKindOut = ir_lowerer::LocalInfo::ValueKind::Unknown;
                std::string normalized = ir_lowerer::trimTemplateTypeText(typeText);
                if (!normalized.empty() && normalized.front() != '/') {
                  normalized.insert(normalized.begin(), '/');
                }
                if (!matchesGeneratedSpecializedType(normalized, "vector", "Vector")) {
                  return false;
                }
                Expr syntheticExpr;
                syntheticExpr.kind = Expr::Kind::Call;
                syntheticExpr.name = normalized;
                const Definition *structDef = resolveDefinitionCall(syntheticExpr);
                if (structDef == nullptr || !ir_lowerer::isStructDefinition(*structDef)) {
                  return false;
                }
                for (const auto &fieldExpr : structDef->statements) {
                  if (!fieldExpr.isBinding || fieldExpr.name != "data") {
                    continue;
                  }
                  std::string typeName;
                  std::vector<std::string> templateArgs;
                  if (!ir_lowerer::extractFirstBindingTypeTransform(fieldExpr, typeName, templateArgs) ||
                      ir_lowerer::normalizeCollectionBindingTypeName(typeName) != "Pointer" ||
                      templateArgs.size() != 1) {
                    continue;
                  }
                  std::string elementType = ir_lowerer::trimTemplateTypeText(templateArgs.front());
                  if (!ir_lowerer::extractTopLevelUninitializedTypeText(elementType, elementType)) {
                    continue;
                  }
                  elemKindOut = ir_lowerer::valueKindFromTypeName(elementType);
                  return elemKindOut != ir_lowerer::LocalInfo::ValueKind::Unknown;
                }
                return false;
              };
              auto populateNativeTailArrayVectorTargetFromElementType =
                  [&](const std::string &collectionName,
                      const std::string &elementTypeText,
                      ir_lowerer::ArrayVectorAccessTargetInfo &targetInfo) {
                    const std::string normalizedElementType =
                        ir_lowerer::trimTemplateTypeText(elementTypeText);
                    if (normalizedElementType.empty()) {
                      return false;
                    }
                    ir_lowerer::ArrayVectorAccessTargetInfo candidate;
                    candidate.isArrayOrVectorTarget = true;
                    candidate.isVectorTarget = (collectionName == "vector");
                    candidate.isSoaVector = (collectionName == "soa");
                    candidate.elemKind =
                        ir_lowerer::valueKindFromTypeName(normalizedElementType);
                    if (candidate.elemKind ==
                            ir_lowerer::LocalInfo::ValueKind::Unknown &&
                        !candidate.isSoaVector) {
                      return false;
                    }
                    if (candidate.isVectorTarget) {
                      candidate.structTypeName =
                          specializedCollectionVectorRecordPathForElementType(
                              normalizedElementType);
                    } else if (candidate.isSoaVector) {
                      candidate.structTypeName =
                          specializedExperimentalSoaVectorStructPathForElementType(
                              normalizedElementType);
                    }
                    targetInfo = candidate;
                    return true;
                  };
              std::function<bool(const std::string &,
                                 ir_lowerer::ArrayVectorAccessTargetInfo &)>
                  inferNativeTailArrayVectorTargetFromTypeText;
              inferNativeTailArrayVectorTargetFromTypeText =
                  [&](const std::string &typeText,
                      ir_lowerer::ArrayVectorAccessTargetInfo &targetInfo) {
                    const std::string normalized =
                        ir_lowerer::trimTemplateTypeText(typeText);
                    if (normalized.empty()) {
                      return false;
                    }
                    std::string normalizedPath = normalized;
                    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
                      normalizedPath.insert(normalizedPath.begin(), '/');
                    }
                    if (matchesGeneratedSpecializedType(
                            normalizedPath, "vector", "Vector")) {
                      ir_lowerer::LocalInfo::ValueKind elemKind;
                      if (!resolveSpecializedVectorElementKind(normalizedPath,
                                                              elemKind)) {
                        return false;
                      }
                      ir_lowerer::ArrayVectorAccessTargetInfo candidate;
                      candidate.isArrayOrVectorTarget = true;
                      candidate.isVectorTarget = true;
                      candidate.elemKind = elemKind;
                      candidate.structTypeName = normalizedPath;
                      targetInfo = candidate;
                      return true;
                    }
                    if (normalizedPath.rfind(
                            collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName),
                            0) == 0) {
                      ir_lowerer::ArrayVectorAccessTargetInfo candidate;
                      candidate.isArrayOrVectorTarget = true;
                      candidate.isVectorTarget = false;
                      candidate.isSoaVector = true;
                      candidate.structTypeName = normalizedPath;
                      targetInfo = candidate;
                      return true;
                    }
                    std::string base;
                    std::string argText;
                    if (!splitTemplateTypeName(normalized, base, argText)) {
                      return false;
                    }
                    const std::string normalizedBase =
                        ir_lowerer::trimTemplateTypeText(base);
                    if (normalizedBase == "Reference" ||
                        normalizedBase == "/Reference" ||
                        normalizedBase == "Pointer" ||
                        normalizedBase == "/Pointer") {
                      std::vector<std::string> wrappedArgs;
                      if (!splitTemplateArgs(argText, wrappedArgs) ||
                          wrappedArgs.size() != 1) {
                        return false;
                      }
                      return inferNativeTailArrayVectorTargetFromTypeText(
                          wrappedArgs.front(), targetInfo);
                    }
                    const std::string collectionName =
                        normalizedBase == "/array"
                            ? "array"
                            : ir_lowerer::normalizeCollectionBindingTypeName(
                                  normalizedBase);
                    if (collectionName != "array" &&
                        collectionName != "vector" &&
                        collectionName != "soa") {
                      return false;
                    }
                    std::vector<std::string> args;
                    if (!splitTemplateArgs(argText, args) || args.size() != 1) {
                      return false;
                    }
                    return populateNativeTailArrayVectorTargetFromElementType(
                        collectionName, args.front(), targetInfo);
                  };
              auto tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText =
                  [&](const std::string &typeText,
                      SymbolId typeTextId,
                      ir_lowerer::ArrayVectorAccessTargetInfo &targetInfo) {
                    return inferNativeTailArrayVectorTargetFromTypeText(
                        tailDispatchHelpers.resolveSemanticReceiverTypeText(typeText, typeTextId),
                        targetInfo);
                  };
              auto tryPopulateNativeTailArrayVectorTargetFromSemanticCollection =
                  [&](const SemanticProductIndex &semanticIndex) {
                    const auto *collectionFact =
                        ir_lowerer::findSemanticProductCollectionSpecialization(
                            semanticIndex, targetCallExpr);
                    if (collectionFact == nullptr) {
                      return false;
                    }
                    const std::string collectionFamily =
                        tailDispatchHelpers.resolveSemanticReceiverTypeText(
                            collectionFact->collectionFamily,
                            collectionFact->collectionFamilyId);
                    const std::string collectionName =
                        collectionFamily == "/array"
                            ? "array"
                            : ir_lowerer::normalizeCollectionBindingTypeName(
                                  collectionFamily);
                    if (collectionName != "array" &&
                        collectionName != "vector" &&
                        collectionName != "soa") {
                      return false;
                    }
                    std::string elementTypeText =
                        tailDispatchHelpers.resolveSemanticReceiverTypeText(
                            collectionFact->elementTypeText,
                            collectionFact->elementTypeTextId);
                    if (elementTypeText.empty()) {
                      elementTypeText =
                          tailDispatchHelpers.resolveSemanticReceiverTypeText(
                              collectionFact->valueTypeText,
                              collectionFact->valueTypeTextId);
                    }
                    return populateNativeTailArrayVectorTargetFromElementType(
                        collectionName, elementTypeText, targetInfoOut);
                  };
              auto tryPopulateArrayVectorFromSemanticReceiverFacts = [&]() {
                if (semanticProgram == nullptr || tailDispatchSemanticIndexPtr == nullptr) {
                  return false;
                }
                const SemanticProductIndex &semanticIndex =
                    *tailDispatchSemanticIndexPtr;
                if (tryPopulateNativeTailArrayVectorTargetFromSemanticCollection(
                        semanticIndex)) {
                  return true;
                }
                const auto *queryFact =
                    ir_lowerer::findSemanticProductQueryFact(semanticProgram, semanticIndex, targetCallExpr);
                if (queryFact != nullptr) {
                  if (tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          queryFact->bindingTypeText,
                          queryFact->bindingTypeTextId,
                          targetInfoOut) ||
                      tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          queryFact->queryTypeText,
                          queryFact->queryTypeTextId,
                          targetInfoOut) ||
                      tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          queryFact->receiverBindingTypeText,
                          queryFact->receiverBindingTypeTextId,
                          targetInfoOut)) {
                    return true;
                  }
                  const std::string bindingType = tailDispatchHelpers.resolveSemanticQueryFactTypeText(*queryFact);
                  if (tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          bindingType,
                          InvalidSymbolId,
                          targetInfoOut)) {
                    return true;
                  }
                }
                if (const auto *bindingFact =
                        ir_lowerer::findSemanticProductBindingFact(
                            semanticIndex, targetCallExpr);
                    bindingFact != nullptr &&
                    tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                        bindingFact->bindingTypeText,
                        bindingFact->bindingTypeTextId,
                        targetInfoOut)) {
                  return true;
                }
                if (const auto *localAutoFact =
                        ir_lowerer::findSemanticProductLocalAutoFact(
                            semanticProgram, semanticIndex, targetCallExpr);
                    localAutoFact != nullptr &&
                    tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                        localAutoFact->bindingTypeText,
                        localAutoFact->bindingTypeTextId,
                        targetInfoOut)) {
                  return true;
                }
                return false;
              };
              if (tryPopulateArrayVectorFromSemanticReceiverFacts()) {
                return true;
              }
              if (targetCallExpr.isFieldAccess && targetCallExpr.args.size() == 1) {
                const std::string receiverStruct = inferStructExprPath(targetCallExpr.args.front(), localsIn);
                if (!receiverStruct.empty()) {
                  StructSlotFieldInfo fieldInfo;
                  if (resolveStructFieldSlot(receiverStruct, targetCallExpr.name, fieldInfo) &&
                      tailDispatchHelpers.isVectorStructPath(fieldInfo.structPath)) {
                    targetInfoOut.isArrayOrVectorTarget = true;
                    targetInfoOut.isVectorTarget = true;
                    targetInfoOut.elemKind = fieldInfo.valueKind;
                    targetInfoOut.structTypeName = fieldInfo.structPath;
                    return true;
                  }
                }
              }
              const std::string inferredReceiverStruct =
                  inferStructExprPath(targetCallExpr, localsIn);
              if (matchesGeneratedSpecializedType(
                      inferredReceiverStruct, "vector", "Vector")) {
                ir_lowerer::LocalInfo::ValueKind elemKind;
                if (!resolveSpecializedVectorElementKind(inferredReceiverStruct,
                                                        elemKind)) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = true;
                targetInfoOut.elemKind = elemKind;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              if (inferredReceiverStruct.rfind(
                      collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0 ||
                  normalizeCollectionBindingTypeName(inferredReceiverStruct) ==
                      "soa") {
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = false;
                targetInfoOut.isSoaVector = true;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              const Definition *callee =
                  tailDispatchHelpers.resolveTailDispatchDirectHelperDefinition(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                return false;
              }
              if ((collectionName != "array" && collectionName != "vector" &&
                   collectionName != "soa") ||
                  collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.isSoaVector = (collectionName == "soa");
              targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              if (targetInfoOut.isSoaVector) {
                std::string elementTypeName = trimTemplateTypeText(collectionArgs.front());
                if (!elementTypeName.empty() && elementTypeName.front() == '/') {
                  elementTypeName.erase(elementTypeName.begin());
                }
                targetInfoOut.structTypeName =
                    specializedExperimentalSoaVectorStructPathForElementType(
                        elementTypeName);
              }
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
            error,
            semanticProgram,
            tailDispatchSemanticIndexPtr);
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Emitted) {
          return true;
        }
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Error) {
          return false;
        }
        if (expr.isMethodCall && semanticProgram != nullptr) {
          const std::string semanticTarget =
              findSemanticProductMethodCallTarget(semanticProgram, expr);
          if (!semanticTarget.empty()) {
            if (((semanticTarget == "/string/count" ||
                  semanticTarget == "/std/collections/vector/count") &&
                 expr.args.size() == 1 &&
                 isSimpleCallName(expr, "count")) ||
                (semanticTarget == "/std/collections/vector/capacity" &&
                 expr.args.size() == 1 &&
                 isSimpleCallName(expr, "capacity")) ||
                (semanticTarget == "/std/collections/soa/to_aos" &&
                 expr.args.size() == 1 &&
                 isSimpleCallName(expr, "to_aos"))) {
              // Builtin bridge forms are emitted by fallback paths below.
            } else {
              error = "semantic-product method-call target missing lowered definition: " +
                      semanticTarget;
              return false;
            }
          }
        }
