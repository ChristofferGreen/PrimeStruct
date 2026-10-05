        bool handledConversionsAndCalls = false;
        if (!ir_lowerer::emitConversionsAndCallsOperatorExpr(
                expr,
                localsIn,
                nextLocal,
                emitExpr,
                inferExprKind,
                emitCompareToZero,
                allocTempLocal,
                emitFloatToIntNonFinite,
                emitPointerIndexOutOfBounds,
                emitArrayIndexOutOfBounds,
                resolveStringTableTarget,
                valueKindFromTypeName,
                getMathConstantName,
                inferStructExprPath,
                [&](const std::string &typeName,
                    const std::string &namespacePrefix,
                    std::string &structPathOut) {
                  return resolveStructTypeName(typeName, namespacePrefix, structPathOut);
                },
                [&](const std::string &structTypeName, int32_t &slotCount) {
                  StructSlotLayout layout;
                  if (!resolveStructSlotLayout(structTypeName, layout)) {
                    return false;
                  }
                  slotCount = layout.totalSlots;
                  return true;
                },
                [&](const std::string &structTypeName,
                    const std::string &fieldName,
                    int32_t &slotOffset,
                    int32_t &slotCount,
                    std::string &fieldStructPath) {
                  StructSlotFieldInfo fieldInfo;
                  if (!resolveStructFieldSlot(structTypeName, fieldName, fieldInfo)) {
                    return false;
                  }
                  slotOffset = fieldInfo.slotOffset;
                  slotCount = fieldInfo.slotCount;
                  fieldStructPath = fieldInfo.structPath;
                  return true;
                },
                [&](const std::string &structTypeName,
                    const std::string &fieldName,
                    LayoutFieldBinding &fieldBindingOut) {
                  return resolveStructLayoutFieldBinding(
                      structTypeName, fieldName, structFieldInfoByName, defMap, fieldBindingOut);
                },
                emitStructCopyFromPtrs,
                function.instructions,
                handledConversionsAndCalls,
                error,
                [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                &callResolutionAdapters.semanticProductTargets,
                activeInlineContext != nullptr ? activeInlineContext->defPath : function.name,
                [&](int32_t destPtrLocal,
                    int32_t srcPtrLocal,
                    int32_t slotCount,
                    const std::string &structPath,
                    const Expr &rhsExpr,
                    int32_t destDropFlagLocal) -> bool {
                  const auto findHelper = [&](std::vector<std::string> helperNames) {
                    return [&, helperNames = std::move(helperNames)](
                               const std::string &path) -> const Definition * {
                      for (const std::string &helperName : helperNames) {
                        auto helperIt = defMap.find(path + helperName);
                        if (helperIt != defMap.end() && helperIt->second != nullptr) {
                          return helperIt->second;
                        }
                      }
                      return nullptr;
                    };
                  };
                  const auto findCopyHelper = findHelper({"/Copy"});
                  const auto findDestroyHelper = [&](const std::string &path) {
                    return ir_lowerer::findStackDestroyHelper(defMap, path);
                  };
                  const auto resolveLayout = [&](const std::string &path,
                                                 StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(path, layoutOut);
                  };
                  const auto emitInstruction = [&](IrOpcode op, uint64_t imm) {
                    function.instructions.push_back({op, imm});
                  };
                  const bool needsDestroy = ir_lowerer::structNeedsDestroyHelpers(
                      structPath, findDestroyHelper, resolveLayout);
                  const bool needsCopy = ir_lowerer::structNeedsDestroyHelpers(
                      structPath, findCopyHelper, resolveLayout);
                  if (!needsDestroy && !needsCopy) {
                    return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, slotCount);
                  }
                  // Copy the new value first (so assigning a value to itself stays valid), then
                  // destroy the old one and install the copy.
                  const int32_t newBaseLocal = nextLocal;
                  nextLocal += slotCount;
                  if (!emitStructCopySlots(newBaseLocal, srcPtrLocal, slotCount)) {
                    return false;
                  }
                  const int32_t newPtrLocal = allocTempLocal();
                  emitInstruction(IrOpcode::AddressOfLocal, static_cast<uint64_t>(newBaseLocal));
                  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(newPtrLocal));
                  if (ir_lowerer::shouldDisarmStructCopySourceExpr(rhsExpr)) {
                    ir_lowerer::emitDisarmTemporaryStructAfterCopy(
                        emitInstruction, srcPtrLocal, structPath);
                  } else {
                    bool ranCopyHelper = false;
                    if (!ir_lowerer::emitStructCopyHelpersFromPtrs(newPtrLocal,
                                                                   srcPtrLocal,
                                                                   structPath,
                                                                   findCopyHelper,
                                                                   resolveLayout,
                                                                   allocTempLocal,
                                                                   emitInstruction,
                                                                   localsIn,
                                                                   emitInlineDefinitionCall,
                                                                   ranCopyHelper,
                                                                   error)) {
                      return false;
                    }
                  }
                  if (needsDestroy) {
                    size_t skipJump = 0;
                    if (destDropFlagLocal >= 0) {
                      emitInstruction(IrOpcode::LoadLocal,
                                      static_cast<uint64_t>(destDropFlagLocal));
                      skipJump = function.instructions.size();
                      emitInstruction(IrOpcode::JumpIfZero, 0);
                    }
                    if (!ir_lowerer::emitStructDestroyHelpersFromPtr(destPtrLocal,
                                                                     structPath,
                                                                     findDestroyHelper,
                                                                     resolveLayout,
                                                                     allocTempLocal,
                                                                     emitInstruction,
                                                                     localsIn,
                                                                     emitInlineDefinitionCall,
                                                                     error)) {
                      return false;
                    }
                    if (destDropFlagLocal >= 0) {
                      function.instructions[skipJump].imm =
                          static_cast<uint64_t>(function.instructions.size());
                    }
                  }
                  if (!emitStructCopyFromPtrs(destPtrLocal, newPtrLocal, slotCount)) {
                    return false;
                  }
                  if (destDropFlagLocal >= 0) {
                    emitInstruction(IrOpcode::PushI32, 1);
                    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(destDropFlagLocal));
                  }
                  return true;
                })) {
          return false;
        }
        if (handledConversionsAndCalls) {
          return true;
        }

        std::vector<std::optional<OnErrorHandler>> conversionsAndCallsScopeStack;
        bool handledConversionsAndCallsControlTail = false;
        if (!ir_lowerer::emitConversionsAndCallsControlExprTail(
                expr,
                localsIn,
                emitExpr,
                emitStatement,
                inferExprKind,
                combineNumericKinds,
                hasNamedArguments,
                resolveDefinitionCall,
                resolveExprPath,
                lowerMatchToIf,
                isBindingMutable,
                bindingKind,
                hasExplicitBindingTypeTransform,
                bindingValueKind,
                inferStructExprPath,
                applyStructArrayInfo,
                applyStructValueInfo,
                [&]() {
                  conversionsAndCallsScopeStack.push_back(currentOnError);
                  currentOnError = std::nullopt;
                  pushFileScope();
                },
                [&]() {
                  emitFileScopeCleanup(fileScopeStack.back());
                  popFileScope();
                  currentOnError = conversionsAndCallsScopeStack.back();
                  conversionsAndCallsScopeStack.pop_back();
                },
                isReturnCall,
                isBlockCall,
                isMatchCall,
                isIfCall,
                function.instructions,
                handledConversionsAndCallsControlTail,
                error)) {
          return false;
        }
        if (handledConversionsAndCallsControlTail) {
          return true;
        }
