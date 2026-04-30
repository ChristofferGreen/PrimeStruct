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
                [&](const std::string &typeName, const std::string &namespacePrefix, std::string &structPathOut) {
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
                [&](const std::string &structTypeName, const std::string &fieldName, LayoutFieldBinding &fieldBindingOut) {
                  return resolveStructLayoutFieldBinding(
                      structTypeName, fieldName, structFieldInfoByName, defMap, fieldBindingOut);
                },
	                emitStructCopyFromPtrs,
	                function.instructions,
	                handledConversionsAndCalls,
	                error,
		                [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
		                &callResolutionAdapters.semanticProductTargets)) {
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
