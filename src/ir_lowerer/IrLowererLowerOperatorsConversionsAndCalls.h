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
                resolveStringTableTarget,
                valueKindFromTypeName,
                getMathConstantName,
                inferStructExprPath,
                [&](const std::string &structTypeName, int32_t &slotCount) {
                  StructSlotLayout layout;
                  if (!resolveStructSlotLayout(structTypeName, layout)) {
                    return false;
                  }
                  slotCount = layout.totalSlots;
                  return true;
                },
                emitStructCopyFromPtrs,
                function.instructions,
                handledConversionsAndCalls,
                error)) {
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
