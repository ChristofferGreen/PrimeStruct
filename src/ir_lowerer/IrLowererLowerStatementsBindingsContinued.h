            if (declaredStructSurface == calleeStructSurface) {
              info.structTypeName = callee.fullPath;
            }
          }
        };
        if (initCallee != nullptr) {
          if (init.isBraceConstructor &&
              ir_lowerer::isStructDefinition(*initCallee)) {
            adoptStructInitializerCallee(*initCallee);
          } else if (initStruct.empty()) {
            initStruct = ir_lowerer::inferStructReturnPathFromDefinition(
                initCallee->fullPath,
                structNames,
                [&](const std::string &typeName, const std::string &namespacePrefix) {
                  std::string structPathOut;
                  return resolveStructTypeName(typeName, namespacePrefix, structPathOut) ? structPathOut
                                                                                        : std::string{};
                },
                [&](const Expr &exprIn) { return resolveExprPath(exprIn); },
                defMap);
          }
        }
        if (initCallee == nullptr && init.kind == Expr::Kind::Call &&
            !init.isFieldAccess &&
            init.isBraceConstructor &&
            !initStruct.empty()) {
          const auto initDefIt = defMap.find(initStruct);
          if (initDefIt != defMap.end() && initDefIt->second != nullptr &&
              ir_lowerer::isStructDefinition(*initDefIt->second)) {
            initCallee = initDefIt->second;
            adoptStructInitializerCallee(*initCallee);
          }
        }
        auto experimentalCollectionTypePath =
            [&](std::string_view collectionName, std::string_view typeName) {
              return collection_paths::memberPath(
                  collection_paths::typeIdentityFolder(collectionName), typeName);
            };
        auto collectionWrapperAlias =
            [](std::string_view collectionName, std::string_view suffix) {
              return std::string(collectionName) + std::string(suffix);
            };
        auto matchesGeneratedSpecializedType =
            [&](std::string_view path, std::string_view collectionName,
                std::string_view typeName) {
              const std::string typePath =
                  experimentalCollectionTypePath(collectionName, typeName);
              return path.rfind(typePath + "__", 0) == 0;
            };
        auto isCollectionVectorConstructorCallee = [&](const Definition *callee) {
          if (callee == nullptr) {
            return false;
          }
          const std::string prefix = vectorBackingMemberRoot();
          const std::string_view prefixView(prefix.data(), prefix.size());
          if (callee->fullPath.rfind(prefixView, 0) != 0) {
            return false;
          }
          std::string leaf = callee->fullPath.substr(prefix.size());
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == "vector" ||
                 leaf == collectionWrapperAlias("vector", "New") ||
                 leaf == collectionWrapperAlias("vector", "Single") ||
                 leaf == collectionWrapperAlias("vector", "Pair") ||
                 leaf == collectionWrapperAlias("vector", "Triple") ||
                 leaf == collectionWrapperAlias("vector", "Quad") ||
                 leaf == collectionWrapperAlias("vector", "Quint") ||
                 leaf == collectionWrapperAlias("vector", "Sext") ||
                 leaf == collectionWrapperAlias("vector", "Sept") ||
                 leaf == collectionWrapperAlias("vector", "Oct");
        };
        if (!info.structTypeName.empty() &&
            (info.structTypeName == vectorBackingTypePath() ||
             matchesGeneratedSpecializedType(info.structTypeName, "vector", "Vector")) &&
            (isCollectionVectorConstructorCallee(initCallee) ||
             [&]() {
               std::string collectionName;
               return getBuiltinCollectionName(init, collectionName) &&
                      collectionName == "vector";
             }())) {
          initStruct = info.structTypeName;
        }
        if (!initStruct.empty() && initStruct != info.structTypeName) {
          error = "struct binding initializer type mismatch on " + stmt.name;
          return false;
        }
        // An owning struct local is destroyed when its scope ends unless it was moved out
        // (docs/spec/value-lifecycle.md): set its drop flag and register it with the scope.
        const auto registerOwnedStructLocal = [&](LocalInfo &ownedInfo) {
          if (fileScopeStack.empty() ||
              !ir_lowerer::structNeedsDestroyHelpers(
                  ownedInfo.structTypeName,
                  [&](const std::string &path) {
                    return ir_lowerer::findStackDestroyHelper(defMap, path);
                  },
                  [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(path, layoutOut);
                  })) {
            return;
          }
          ownedInfo.dropFlagLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(ownedInfo.dropFlagLocal)});
          setupStage.dropEntries.push_back(
              {ownedInfo.index, ownedInfo.dropFlagLocal, ownedInfo.structTypeName});
          fileScopeStack.back().push_back(-static_cast<int32_t>(setupStage.dropEntries.size()));
        };
        // Builtin-storage key/value maps (constructed via `map<K, V>(...)` sugar
        // without a stdlib constructor definition in scope) are materialized by
        // `tryEmitBuiltinKeyValueConstructor` as a heap pointer to an inline
        // [count, key, value, ...] region, not as a by-value map backing struct.
        // Store that pointer in a single slot instead of struct-copying the
        // map backing layout, which would read past the smaller builtin region.
        if (hasKeyValueKinds(info) &&
            init.kind == Expr::Kind::Call && !init.isMethodCall &&
            (initCallee == nullptr ||
             returnsMapLikeValue(initCallee) ||
             !ir_lowerer::isStructDefinition(*initCallee)) &&
            (initCallee != nullptr || init.templateArgs.size() == 2)) {
          std::string builtinMapCollectionName;
          if ((initCallee != nullptr && returnsMapLikeValue(initCallee)) ||
              (getBuiltinCollectionName(init, builtinMapCollectionName) &&
               builtinMapCollectionName == "map")) {
            if (!emitExpr(init, localsIn)) {
              return false;
            }
            info.index = nextLocal++;
            localsIn.emplace(stmt.name, info);
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
            return true;
          }
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
        if (init.kind == Expr::Kind::Call &&
            !init.isFieldAccess &&
            init.isBraceConstructor &&
            initCallee != nullptr &&
            ir_lowerer::isStructDefinition(*initCallee) &&
            initCallee->fullPath == info.structTypeName) {
          std::vector<Expr> callParams;
          std::vector<const Expr *> orderedArgs;
          std::vector<const Expr *> packedArgs;
          size_t packedParamIndex = 0;
          if (!ir_lowerer::buildInlineCallOrderedArguments(
                  init,
                  *initCallee,
                  structNames,
                  localsIn,
                  callParams,
                  orderedArgs,
                  packedArgs,
                  packedParamIndex,
                  error)) {
            return false;
          }
          if (!packedArgs.empty()) {
            error = "struct constructors do not support variadic field packs";
            return false;
          }
          if (!ir_lowerer::emitInlineStructDefinitionArguments(
                  initCallee->fullPath,
                  callParams,
                  orderedArgs,
                  localsIn,
                  false,
                  nextLocal,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                    return inferExprKind(candidateExpr, candidateLocals);
                  },
                  [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                    return inferStructExprPath(candidateExpr, candidateLocals);
                  },
                  [&](const Expr &candidateExpr, const LocalMap &candidateLocals) {
                    return emitExpr(candidateExpr, candidateLocals);
                  },
                  [&](const Expr &fieldParam,
                      const LocalMap &fieldLocals,
                      LocalInfo &infoOut,
                      std::string &errorOut) {
                    return ir_lowerer::inferCallParameterLocalInfo(fieldParam,
                                                                   fieldLocals,
                                                                   isBindingMutable,
                                                                   hasExplicitBindingTypeTransform,
                                                                   bindingKind,
                                                                   bindingValueKind,
                                                                   inferExprKind,
                                                                   isFileErrorBinding,
                                                                   setReferenceArrayInfo,
                                                                   applyStructArrayInfo,
                                                                   applyStructValueInfo,
                                                                   isStringBinding,
                                                                   infoOut,
                                                                   errorOut,
                                                                   [&](const Expr &callExpr,
                                                                       const LocalMap &callLocals) {
                                                                     return resolveMethodCallDefinition(callExpr,
                                                                                                        callLocals);
                                                                   },
                                                                   [&](const Expr &callExpr) {
                                                                     return resolveDefinitionCall(callExpr);
                                                                   },
                                                                   [&](const std::string &definitionPath,
                                                                       ReturnInfo &returnInfo) {
                                                                     return getReturnInfo(definitionPath, returnInfo);
                                                                   },
                                                                   callResolutionAdapters.semanticProgram,
                                                                   &callResolutionAdapters.semanticProductTargets.semanticIndex);
                  },
                  [&](int32_t destBase, int32_t srcPtrLocal, int32_t slotCount) {
                    return emitStructCopySlots(destBase, srcPtrLocal, slotCount);
                  },
                  [&]() { return allocTempLocal(); },
                  [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                  error,
                  baseLocal,
                  [&](int32_t destPtrLocal, int32_t srcPtrLocal, const std::string &structPath) {
                    bool ranCopyHelper = false;
                    return ir_lowerer::emitStructCopyHelpersFromPtrs(
                        destPtrLocal,
                        srcPtrLocal,
                        structPath,
                        [&](const std::string &path) -> const Definition * {
                          auto copyIt = defMap.find(path + "/Copy");
                          return copyIt == defMap.end() ? nullptr : copyIt->second;
                        },
                        [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                          return resolveStructSlotLayout(path, layoutOut);
                        },
                        [&]() { return allocTempLocal(); },
                        [&](IrOpcode op, uint64_t imm) {
                          function.instructions.push_back({op, imm});
                        },
                        localsIn,
                        emitInlineDefinitionCall,
                        ranCopyHelper,
                        error);
                  })) {
            return false;
          }
          registerOwnedStructLocal(info);
          localsIn.emplace(stmt.name, info);
          return true;
        }
        int32_t srcPtrLocal = allocTempLocal();
        bool emittedStructArgsPackAccessInit = false;
        std::string accessName;
        if (init.kind == Expr::Kind::Call &&
            getBuiltinArrayAccessName(init, accessName) &&
            init.args.size() == 2) {
          const auto targetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(
                  init.args.front(),
                  localsIn,
                  {},
                  callResolutionAdapters.semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex);
          const bool isVectorArgsPackAccess =
              targetInfo.isArgsPackTarget &&
              targetInfo.argsPackElementKind == LocalInfo::Kind::Vector;
          const bool isStructArgsPackAccess =
              targetInfo.isArgsPackTarget &&
              !targetInfo.isVectorTarget &&
              !isVectorArgsPackAccess &&
              !targetInfo.structTypeName.empty() &&
              targetInfo.elemSlotCount > 0;
          if (isStructArgsPackAccess) {
            if (!ir_lowerer::emitArrayVectorIndexedAccess(
                    accessName,
                    init.args.front(),
                    init.args[1],
                    localsIn,
                    [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                      return inferExprKind(valueExpr, valueLocals);
                    },
                    [&]() { return allocTempLocal(); },
                    [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                      return emitExpr(valueExpr, valueLocals);
                    },
                    [&]() { emitArrayIndexOutOfBounds(); },
                    [&]() { return function.instructions.size(); },
                    [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                    [&](size_t indexToPatch, uint64_t target) {
                      function.instructions[indexToPatch].imm = target;
                    },
                    error)) {
              return false;
            }
            emittedStructArgsPackAccessInit = true;
          }
        }
        if (!emittedStructArgsPackAccessInit && !emitExpr(init, localsIn)) {
          return false;
        }
        bool shouldMaterializePackedTryScalar = false;
        if (!emittedStructArgsPackAccessInit &&
            init.kind == Expr::Kind::Call &&
            !init.isMethodCall &&
            isSimpleCallName(init, "try") &&
            init.args.size() == 1) {
          if (init.args.front().kind == Expr::Kind::Call &&
              (init.args.front().name == "map" ||
               init.args.front().name == "and_then" ||
               init.args.front().name == "map2")) {
            shouldMaterializePackedTryScalar = true;
          } else {
            ResultExprInfo tryResultInfo;
            if (ir_lowerer::resolveResultExprInfoFromLocals(
                    init.args.front(),
                    localsIn,
                    [&](const Expr &callExpr, const LocalMap &callLocals) {
                      return resolveMethodCallDefinition(callExpr, callLocals);
                    },
                    [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                    [&](const std::string &definitionPath, ReturnInfo &returnInfo) {
                      return getReturnInfo(definitionPath, returnInfo);
                    },
                    [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                      return inferExprKind(valueExpr, valueLocals);
                    },
                    tryResultInfo,
                    callResolutionAdapters.semanticProgram,
                    &callResolutionAdapters.semanticProductTargets.semanticIndex,
                    &error) &&
                tryResultInfo.isResult &&
                tryResultInfo.hasValue &&
                tryResultInfo.valueStructType.empty()) {
              shouldMaterializePackedTryScalar = true;
            }
          }
        }
        if (shouldMaterializePackedTryScalar) {
          ir_lowerer::PackedResultStructPayloadInfo payloadInfo;
          if (ir_lowerer::resolvePackedResultStructPayloadInfo(
                  info.structTypeName,
                  [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(structPath, layoutOut);
                  },
                  payloadInfo) &&
              payloadInfo.isPackedSingleSlot) {
            const int32_t packedBaseLocal = nextLocal;
            nextLocal += payloadInfo.slotCount;
            const int32_t packedPtrLocal = allocTempLocal();
            function.instructions.push_back(
                {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(payloadInfo.slotCount - 1))});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(packedBaseLocal)});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(packedBaseLocal + payloadInfo.fieldOffset)});
            function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(packedBaseLocal)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(packedPtrLocal)});
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(packedPtrLocal)});
          }
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          return false;
        }
        if (shouldDisarmStructCopySourceExpr(init)) {
          ir_lowerer::emitDisarmTemporaryStructAfterCopy(
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              srcPtrLocal,
              structTypeName);
        } else if (!info.isMutable && ir_lowerer::emitDisarmTemporaryStructAfterCopy(
                                          [&](IrOpcode op, uint64_t imm) {
                                            function.instructions.push_back({op, imm});
                                          },
                                          info.index,
                                          structTypeName)) {
          // A read-only collection binding is a view of the place: it shares the storage
          // without owning it, so its scope has nothing to destroy.
          localsIn.emplace(stmt.name, info);
          return true;
        } else {
          // Any other binding initialized from an existing place owns a copy of it
          // (docs/spec/value-lifecycle.md, Copies): containers copy their elements.
          bool ranCopyHelper = false;
          if (!ir_lowerer::emitStructCopyHelpersFromPtrs(
                  info.index,
                  srcPtrLocal,
                  structTypeName,
                  [&](const std::string &path) -> const Definition * {
                    auto copyIt = defMap.find(path + "/Copy");
                    return copyIt == defMap.end() ? nullptr : copyIt->second;
                  },
                  [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(path, layoutOut);
                  },
                  [&]() { return allocTempLocal(); },
                  [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                  localsIn,
                  emitInlineDefinitionCall,
                  ranCopyHelper,
                  error)) {
            return false;
          }
        }
        registerOwnedStructLocal(info);
        localsIn.emplace(stmt.name, info);
        return true;
      }

      if (valueKind == LocalInfo::ValueKind::String && kind == LocalInfo::Kind::Value) {
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
      if (valueKind == LocalInfo::ValueKind::Unknown &&
          !hasKeyValueKinds(info) &&
          kind != LocalInfo::Kind::Array &&
          kind != LocalInfo::Kind::Vector &&
          info.structTypeName.empty() &&
          !info.isSoaVector &&
          info.kind != LocalInfo::Kind::Pointer &&
          info.kind != LocalInfo::Kind::Reference) {
        error = "native backend requires typed bindings";
        return false;
      }
      if (!emitExpr(init, localsIn)) {
        return false;
      }
      info.index = nextLocal++;
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
          if (const Definition *sumDef = sumHelpers.resolveSumDefinitionByPath(structPath)) {
            int32_t totalSlots = 0;
            if (!sumHelpers.loweredSumSlotCount(*sumDef, totalSlots)) {
              return false;
            }
            layoutOut = {};
            layoutOut.structPath = sumDef->fullPath;
            layoutOut.totalSlots = totalSlots;
            return true;
          }
          return resolveStructSlotLayout(structPath, layoutOut);
        },
        [&]() { return allocTempLocal(); },
        [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) {
          return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, slotCount);
        },
        [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
        error,
        [&](const ir_lowerer::UninitializedStorageAccessInfo &access,
            int32_t valuePtrLocal,
            bool &handledOut) {
          handledOut = false;
          const Definition *sumDef = sumHelpers.resolveSumDefinitionByPath(access.typeInfo.structPath);
          if (sumDef == nullptr) {
            return true;
          }
          handledOut = true;
          if (valuePtrLocal < 0) {
            return true;
          }
          return sumHelpers.emitActiveSumPayloadDestroyFromSumPtr(*sumDef, valuePtrLocal, localsIn);
        });
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
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); });
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
        [&](const Expr &callExpr) -> const Definition * {
          if (callExpr.isMethodCall) {
            // A user struct method sharing a path-space builtin name
            // (`values.insert(key, value)`) is a definition call, not the
            // builtin; probe without leaking a resolution error.
            const std::string priorError = error;
            const Definition *methodDefinition =
                resolveMethodCallDefinition(callExpr, localsIn);
            error = priorError;
            return methodDefinition;
          }
          return resolveDefinitionCall(callExpr);
        },
        [&](const Expr &argExpr, const LocalMap &valueLocals) { return emitExpr(argExpr, valueLocals); },
        function.instructions,
        error);
    if (printPathSpaceResult == ir_lowerer::StatementPrintPathSpaceEmitResult::Error) {
      return false;
    }
    if (printPathSpaceResult == ir_lowerer::StatementPrintPathSpaceEmitResult::Emitted) {
      return true;
    }
    const auto pickStatementResult = sumHelpers.tryEmitPickStatement(stmt, localsIn);
    if (pickStatementResult == LoweredSumPickEmitResult::Error) {
      return false;
    }
    if (pickStatementResult == LoweredSumPickEmitResult::Emitted) {
      return true;
    }
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      // A local named in the returned value (returned itself, or wrapped in a Result or an
      // aggregate) hands its value to the caller; its scope must not destroy it. Clearing a
      // flag the value did not need to give up only leaks.
      std::function<void(const Expr &)> releaseReturnedLocals = [&](const Expr &returnedExpr) {
        if (returnedExpr.kind == Expr::Kind::Name) {
          ir_lowerer::emitReleaseDropFlag(
              localsIn, returnedExpr.name, [&](IrOpcode op, uint64_t imm) {
                function.instructions.push_back({op, imm});
              });
          return;
        }
        for (const Expr &arg : returnedExpr.args) {
          releaseReturnedLocals(arg);
        }
        for (const Expr &bodyArg : returnedExpr.bodyArguments) {
          releaseReturnedLocals(bodyArg);
        }
      };
      releaseReturnedLocals(stmt.args.front());
    }
    const std::optional<ir_lowerer::ReturnStatementInlineContext> returnInlineContext = [&]()
        -> std::optional<ir_lowerer::ReturnStatementInlineContext> {
      if (!activeInlineContext) {
        return std::nullopt;
      }
      return ir_lowerer::ReturnStatementInlineContext{
          activeInlineContext->returnsVoid,
          activeInlineContext->returnsArray,
          activeInlineContext->returnKind,
          activeInlineContext->returnLocal,
          &activeInlineContext->returnJumps,
      };
    }();
    Expr rewrittenReturnStmt;
    const Expr *emittedReturnStmt = &stmt;
    LocalMap rewrittenReturnLocals;
    const LocalMap *emittedReturnLocals = &localsIn;
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      const Expr &returnValueExpr = emittedReturnStmt->args.front();
      if (const Definition *returnSumDef = extractDeclaredSumReturnDefinition();
          returnSumDef != nullptr &&
          sumHelpers.isStdlibResultSumDefinition(*returnSumDef) &&
          (sumHelpers.isLegacyResultOkCall(returnValueExpr) ||
           sumHelpers.isStdlibResultVariantHelperCall(returnValueExpr, "ok") ||
           sumHelpers.isStdlibResultVariantHelperCall(returnValueExpr, "error") ||
           sumHelpers.isLegacyResultMapCall(returnValueExpr) ||
           sumHelpers.isLegacyResultAndThenCall(returnValueExpr) || sumHelpers.isLegacyResultMap2Call(returnValueExpr))) {
        int32_t totalSlots = 0;
        if (!sumHelpers.loweredSumSlotCount(*returnSumDef, totalSlots)) {
          if (!error.empty()) {
            return false;
          }
          error = "native backend does not support sum payload type on " +
                  returnSumDef->fullPath;
          return false;
        }
        const int32_t baseLocal = nextLocal;
        nextLocal += totalSlots;
        const int32_t ptrLocal = nextLocal++;
        sumHelpers.emitLoweredSumHeader(baseLocal, totalSlots);
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        if (!sumHelpers.emitLoweredSumConstructionIntoLocal(baseLocal, *returnSumDef, returnValueExpr, localsIn)) {
          return false;
        }
        rewrittenReturnLocals = localsIn;
        LocalInfo returnInfo;
        returnInfo.kind = LocalInfo::Kind::Value;
        returnInfo.valueKind = LocalInfo::ValueKind::Int64;
        const SumVariant *okVariant = sumHelpers.findSumVariantByName(*returnSumDef, "ok");
        const SumVariant *errorVariant = sumHelpers.findSumVariantByName(*returnSumDef, "error");
        bool emittedPackedResultReturn = false;
        if (okVariant != nullptr && errorVariant != nullptr) {
          LoweredSumPayloadStorageInfo okPayload;
          LoweredSumPayloadStorageInfo errorPayload;
          int32_t okTag = 0;
          int32_t errorTag = 0;
          if (!sumHelpers.resolveSemanticProductSumPayloadStorageInfo(
                  *returnSumDef, *okVariant, "packed Result return ok payload", okPayload) ||
              !sumHelpers.resolveSemanticProductSumPayloadStorageInfo(
                  *returnSumDef, *errorVariant, "packed Result return error payload", errorPayload) ||
              !sumHelpers.resolveSemanticProductSumVariantTag(
                  *returnSumDef, *okVariant, "packed Result return ok tag", okTag) ||
              !sumHelpers.resolveSemanticProductSumVariantTag(
                  *returnSumDef, *errorVariant, "packed Result return error tag", errorTag)) {
            return false;
          }
          const bool okPayloadTypeIsScalar =
              !okVariant->hasPayload ||
              valueKindFromTypeName(sumHelpers.sumPayloadTypeText(*okVariant)) !=
                  LocalInfo::ValueKind::Unknown;
          const bool errorPayloadTypeIsScalar =
              valueKindFromTypeName(sumHelpers.sumPayloadTypeText(*errorVariant)) !=
              LocalInfo::ValueKind::Unknown;
          if (okPayloadTypeIsScalar && errorPayloadTypeIsScalar &&
              !okPayload.isAggregate && !errorPayload.isAggregate) {
            const int32_t packedLocal = nextLocal++;
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(baseLocal + 1)});
            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(okTag)});
            function.instructions.push_back({IrOpcode::CmpEqI32, 0});
            const size_t jumpToError = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            if (okVariant->hasPayload) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(baseLocal + 2)});
            } else {
              function.instructions.push_back({IrOpcode::PushI64, 0});
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(packedLocal)});
            const size_t jumpToEnd = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            function.instructions[jumpToError].imm =
                static_cast<uint64_t>(function.instructions.size());
            if (errorVariant->hasPayload) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(baseLocal + 2)});
            } else {
              function.instructions.push_back({IrOpcode::PushI64, 1});
            }
            function.instructions.push_back({IrOpcode::PushI64, 4294967296ull});
            function.instructions.push_back({IrOpcode::MulI64, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(packedLocal)});
            function.instructions[jumpToEnd].imm =
                static_cast<uint64_t>(function.instructions.size());
            returnInfo.index = packedLocal;
            sumHelpers.applyStdlibResultSumInfoToLocal(*returnSumDef, returnInfo);
            emittedPackedResultReturn = true;
          }
        }
        if (!emittedPackedResultReturn) {
          returnInfo.structTypeName = returnSumDef->fullPath;
          returnInfo.structSlotCount = totalSlots;
          returnInfo.index = ptrLocal;
          sumHelpers.applyStdlibResultSumInfoToLocal(*returnSumDef, returnInfo);
          if (returnInlineContext.has_value()) {
            if (returnInlineContext->returnLocal < 0) {
              error = "native backend missing inline return local";
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(returnInlineContext->returnLocal)});
            const size_t jumpIndex = function.instructions.size();
            function.instructions.push_back({IrOpcode::Jump, 0});
            if (returnInlineContext->returnJumps != nullptr) {
              returnInlineContext->returnJumps->push_back(jumpIndex);
            }
            return true;
          }
          emitFileScopeCleanupAll();
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
          function.instructions.push_back({IrOpcode::ReturnI64, 0});
          sawReturn = true;
          return true;
        }
        const std::string tempReturnName =
            emittedPackedResultReturn
                ? "__native_return_packed_result_" + std::to_string(returnInfo.index)
                : "__native_return_sum_" + std::to_string(ptrLocal);
        rewrittenReturnLocals.emplace(tempReturnName, returnInfo);
        rewrittenReturnStmt = *emittedReturnStmt;
        Expr stableReturnValueExpr;
        stableReturnValueExpr.kind = Expr::Kind::Name;
        stableReturnValueExpr.name = tempReturnName;
        rewrittenReturnStmt.args.front() = std::move(stableReturnValueExpr);
        emittedReturnStmt = &rewrittenReturnStmt;
        emittedReturnLocals = &rewrittenReturnLocals;
      }
      const Expr &stableReturnValueExpr = emittedReturnStmt->args.front();
      const bool stableReturnValueIsPackedResult =
          stableReturnValueExpr.kind == Expr::Kind::Name &&
          [&]() {
            auto localIt = emittedReturnLocals->find(stableReturnValueExpr.name);
            return localIt != emittedReturnLocals->end() &&
                   localIt->second.isResult &&
                   localIt->second.structTypeName.empty();
          }();
      StructSlotLayout layout;
      std::string aggregateStructPath;
      const std::string inferredStructPath = inferStructExprPath(stableReturnValueExpr, *emittedReturnLocals);
      if (!inferredStructPath.empty() && resolveStructSlotLayout(inferredStructPath, layout)) {
        aggregateStructPath = inferredStructPath;
      }
      if (aggregateStructPath.empty() && !stableReturnValueIsPackedResult) {
        const std::string declaredStructPath = extractDeclaredStructReturnPath();
        if (!declaredStructPath.empty() && resolveStructSlotLayout(declaredStructPath, layout)) {
          aggregateStructPath = declaredStructPath;
        }
      }
      const bool shouldStabilizeAggregateReturn =
          !aggregateStructPath.empty() &&
          !declaredReturnIsPointerLikeHandle &&
          (stableReturnValueExpr.kind == Expr::Kind::Call || stableReturnValueExpr.kind == Expr::Kind::Name);
      if (shouldStabilizeAggregateReturn) {
        const int32_t baseLocal = nextLocal;
        nextLocal += layout.totalSlots;
        const int32_t ptrLocal = nextLocal++;
        function.instructions.push_back(
            {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
        const int32_t srcPtrLocal = allocTempLocal();
        if (!emitExpr(stableReturnValueExpr, *emittedReturnLocals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          return false;
        }
        if (stableReturnValueExpr.kind == Expr::Kind::Name ||
            shouldDisarmStructCopySourceExpr(stableReturnValueExpr)) {
          ir_lowerer::emitDisarmTemporaryStructAfterCopy(
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              srcPtrLocal,
              aggregateStructPath);
        }
        rewrittenReturnLocals = localsIn;
        LocalInfo returnInfo;
        returnInfo.kind = LocalInfo::Kind::Value;
        returnInfo.valueKind = LocalInfo::ValueKind::Int64;
        returnInfo.structTypeName = aggregateStructPath;
        returnInfo.index = ptrLocal;
        const std::string tempReturnName = "__native_return_struct_" + std::to_string(ptrLocal);
        rewrittenReturnLocals.emplace(tempReturnName, returnInfo);
        rewrittenReturnStmt = *emittedReturnStmt;
        Expr stableReturnValueExpr;
        stableReturnValueExpr.kind = Expr::Kind::Name;
        stableReturnValueExpr.name = tempReturnName;
        rewrittenReturnStmt.args.front() = std::move(stableReturnValueExpr);
        emittedReturnStmt = &rewrittenReturnStmt;
        emittedReturnLocals = &rewrittenReturnLocals;
      }
    }
    const auto returnResult = ir_lowerer::tryEmitReturnStatement(
        *emittedReturnStmt,
        *emittedReturnLocals,
        function.instructions,
        returnInlineContext,
        declaredReturnIsReferenceHandle,
        currentReturnResult,
        returnsVoid,
        sawReturn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        ir_lowerer::makeResolveResultExprInfoFromLocals(
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
            callResolutionAdapters.semanticProgram,
            &callResolutionAdapters.semanticProductTargets.semanticIndex,
            &error),
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferArrayElementKind(valueExpr, valueLocals); },
        [&]() { emitFileScopeCleanupAll(); },
        error);
    if (returnResult == ir_lowerer::ReturnStatementEmitResult::Error) {
      return false;
    }
    if (returnResult == ir_lowerer::ReturnStatementEmitResult::Emitted) {
      // A return from a block nested in an inlined callee jumps to the call's exit, which only
      // cleans the callee body's scope: clean the scopes in between before the jump.
      if (activeInlineContext != nullptr &&
          fileScopeStack.size() > activeInlineContext->bodyScopeDepth &&
          !function.instructions.empty() && function.instructions.back().op == IrOpcode::Jump &&
          !activeInlineContext->returnJumps.empty() &&
          activeInlineContext->returnJumps.back() + 1 == function.instructions.size()) {
        const IrInstruction returnJump = function.instructions.back();
        function.instructions.pop_back();
        for (size_t depth = fileScopeStack.size(); depth > activeInlineContext->bodyScopeDepth;
             --depth) {
          emitFileScopeCleanup(fileScopeStack[depth - 1]);
        }
        activeInlineContext->returnJumps.back() = function.instructions.size();
        function.instructions.push_back(returnJump);
      }
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
