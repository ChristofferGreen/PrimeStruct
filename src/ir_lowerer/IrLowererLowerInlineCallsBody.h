    if (isCollectionVectorConstructor(callee.fullPath)) {
      const std::string constructorCollectionName("vector");
      auto extractCollectionParameterTypeName = [](const Expr &paramExpr) {
        for (const auto &transform : paramExpr.transforms) {
          if (transform.name == "mut" || transform.name == "public" ||
              transform.name == "private" || transform.name == "static" ||
              transform.name == "shared" || transform.name == "placement" ||
              transform.name == "align" || transform.name == "packed" ||
              transform.name == "reflection" || transform.name == "effects" ||
              transform.name == "capabilities") {
            continue;
          }
          if (!transform.arguments.empty()) {
            continue;
          }
          if (transform.name == "args" && transform.templateArgs.size() == 1) {
            return trimTemplateTypeText(transform.templateArgs.front());
          }
          std::string typeName = transform.name;
          if (!transform.templateArgs.empty()) {
            typeName += "<";
            for (size_t index = 0; index < transform.templateArgs.size();
                 ++index) {
              if (index != 0) {
                typeName += ", ";
              }
              typeName += trimTemplateTypeText(transform.templateArgs[index]);
            }
            typeName += ">";
          }
          return typeName;
        }
        return std::string{};
      };
      auto extractCollectionReturnElementTypeName =
          [&](const Definition &definition) {
        for (const auto &transform : definition.transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string argText;
          if (!splitTemplateTypeName(
                  trimTemplateTypeText(transform.templateArgs.front()),
                  base,
                  argText) ||
              normalizeCollectionBindingTypeName(base) !=
                  constructorCollectionName) {
            continue;
          }
          std::vector<std::string> args;
          if (!splitTemplateArgs(argText, args) || args.size() != 1) {
            continue;
          }
          return trimTemplateTypeText(args.front());
        }
        return std::string{};
      };
      Expr collectionLiteralExpr = callExpr;
      collectionLiteralExpr.name = constructorCollectionName;
      collectionLiteralExpr.namespacePrefix.clear();
      collectionLiteralExpr.isMethodCall = false;
      collectionLiteralExpr.semanticNodeId = 0;
      if (collectionLiteralExpr.templateArgs.empty() && !callee.parameters.empty()) {
        const std::string elementType =
            extractCollectionParameterTypeName(callee.parameters.front());
        if (!elementType.empty()) {
          collectionLiteralExpr.templateArgs = {elementType};
        }
      }
      if (collectionLiteralExpr.templateArgs.empty()) {
        const std::string elementType =
            extractCollectionReturnElementTypeName(callee);
        if (!elementType.empty()) {
          collectionLiteralExpr.templateArgs = {elementType};
        }
      }
      std::string collectionName;
      if (getBuiltinCollectionName(collectionLiteralExpr, collectionName) &&
          collectionName == constructorCollectionName) {
        if (collectionLiteralExpr.templateArgs.size() != 1 &&
            !collectionLiteralExpr.args.empty()) {
          error = "collection literal requires exactly one template argument";
          return false;
        }
        LocalInfo::ValueKind elemKind =
            collectionLiteralExpr.templateArgs.size() == 1
                ? valueKindFromTypeName(collectionLiteralExpr.templateArgs.front())
                : LocalInfo::ValueKind::Unknown;
        if (elemKind == LocalInfo::ValueKind::Unknown &&
            !collectionLiteralExpr.args.empty()) {
          elemKind =
              inferExprKind(collectionLiteralExpr.args.front(), callerLocals);
        }
        if (elemKind == LocalInfo::ValueKind::Unknown &&
            !collectionLiteralExpr.args.empty()) {
          error =
              "native backend only supports numeric/bool/string collection literals";
          return false;
        }
        if (collectionLiteralExpr.args.size() >
            static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
          error = "collection literal too large for native backend";
          return false;
        }
        const int32_t literalCount =
            static_cast<int32_t>(collectionLiteralExpr.args.size());
        StructSlotLayoutInfo vectorLayout;
        std::string vectorStructPath =
            inferStructExprPath(callExpr, callerLocals);
        if (vectorStructPath.empty()) {
          vectorStructPath = vectorBackingTypePath();
        }
        if (!resolveStructSlotLayout(vectorStructPath, vectorLayout)) {
          error =
              "native backend cannot resolve experimental collection record layout";
          return false;
        }
        VectorRecordFieldSlots vectorSlots;
        if (!resolveVectorRecordFieldSlotsFromLayout(
                vectorLayout, vectorSlots)) {
          error =
              "native backend cannot resolve experimental collection record fields";
          return false;
        }
        // A vector<u8> literal stores its elements packed, one byte each.
        const bool packedBytes = elemKind == LocalInfo::ValueKind::UInt8;
        const int32_t baseLocal = nextLocal;
        nextLocal += vectorSlots.totalSlots;
        emitVectorRecordHeader(function.instructions,
                               baseLocal,
                               vectorSlots,
                               literalCount,
                               literalCount,
                               literalCount,
                               literalCount == 0,
                               literalCount != 0,
                               packedBytes);

        for (size_t argIndex = 0; argIndex < collectionLiteralExpr.args.size();
             ++argIndex) {
          const Expr &argExpr = collectionLiteralExpr.args[argIndex];
          function.instructions.push_back(
              {IrOpcode::LoadLocal,
               static_cast<uint64_t>(baseLocal + vectorSlots.data)});
          const uint64_t offsetBytes =
              static_cast<uint64_t>(argIndex) * (packedBytes ? 1u : IrSlotBytes);
          if (offsetBytes != 0) {
            function.instructions.push_back({IrOpcode::PushI64, offsetBytes});
            function.instructions.push_back({IrOpcode::AddI64, 0});
          }
          if (elemKind == LocalInfo::ValueKind::String) {
            int32_t stringIndex = -1;
            size_t length = 0;
            if (!resolveStringTableTarget(
                    argExpr, callerLocals, stringIndex, length)) {
              error =
                  "native backend requires collection literal string elements to "
                  "be string literals or literal-backed bindings";
              return false;
            }
            function.instructions.push_back(
                {IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)});
          } else {
            const LocalInfo::ValueKind argKind =
                inferExprKind(argExpr, callerLocals);
            if (argKind == LocalInfo::ValueKind::Unknown ||
                argKind == LocalInfo::ValueKind::String) {
              error =
                  "native backend requires collection literal elements to be "
                  "numeric/bool values";
              return false;
            }
            if (argKind != elemKind) {
              error = "collection literal element type mismatch";
              return false;
            }
            if (!emitExpr(argExpr, callerLocals)) {
              return false;
            }
          }
          function.instructions.push_back({packedBytes ? IrOpcode::StoreU8 : IrOpcode::StoreIndirect, 0});
          function.instructions.push_back({IrOpcode::Pop, 0});
        }
        function.instructions.push_back(
            {IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        return true;
      }
    }
    if (callExpr.args.size() == 1 &&
        (isInternalSoaMetadataInlineHelper(callee.fullPath, "field_count") ||
         isInternalSoaMetadataInlineHelper(callee.fullPath, "field_capacity") ||
         (isInternalSoaMetadataOwner(callee.fullPath) &&
          (callLeafName(callExpr) == "field_count" ||
           callLeafName(callExpr) == "field_capacity")))) {
      const Expr &receiver = callExpr.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        auto localIt = callerLocals.find(receiver.name);
        if (localIt != callerLocals.end()) {
          function.instructions.push_back(
              {IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index)});
        } else if (!emitExpr(receiver, callerLocals)) {
          return false;
        }
      } else if (!emitExpr(receiver, callerLocals)) {
        return false;
      }
      const uint64_t fieldOffset =
          (isInternalSoaMetadataInlineHelper(callee.fullPath, "field_capacity") ||
           callLeafName(callExpr) == "field_capacity")
              ? IrSlotBytes * 2
              : IrSlotBytes;
      function.instructions.push_back({IrOpcode::PushI64, fieldOffset});
      function.instructions.push_back({IrOpcode::AddI64, 0});
      function.instructions.push_back({IrOpcode::LoadIndirect, 0});
      return true;
    }
    if (callExpr.args.size() == 2 &&
        (isInternalSoaMetadataInlineHelper(callee.fullPath, "set_field_count") ||
         isInternalSoaMetadataInlineHelper(callee.fullPath, "set_field_capacity") ||
         (isInternalSoaMetadataOwner(callee.fullPath) &&
          (callLeafName(callExpr) == "set_field_count" ||
           callLeafName(callExpr) == "set_field_capacity")))) {
      const Expr &receiver = callExpr.args.front();
      const Expr &value = callExpr.args[1];
      auto emitBoundsTrapIfStackTrue = [&]() {
        const size_t okJump = function.instructions.size();
        function.instructions.push_back({IrOpcode::JumpIfZero, 0});
        emitArrayIndexOutOfBounds();
        function.instructions[okJump].imm = function.instructions.size();
      };
      auto emitReceiverAddress = [&](uint64_t slotOffset) {
        if (receiver.kind == Expr::Kind::Name) {
          auto localIt = callerLocals.find(receiver.name);
          if (localIt != callerLocals.end()) {
            function.instructions.push_back(
                {IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index)});
          } else if (!emitExpr(receiver, callerLocals)) {
            return false;
          }
        } else if (!emitExpr(receiver, callerLocals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::PushI64, slotOffset});
        function.instructions.push_back({IrOpcode::AddI64, 0});
        return true;
      };
      auto emitReceiverLoad = [&](uint64_t slotOffset) {
        if (!emitReceiverAddress(slotOffset)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::LoadIndirect, 0});
        return true;
      };

      if (!emitExpr(value, callerLocals)) {
        return false;
      }
      function.instructions.push_back({IrOpcode::PushI32, 0});
      function.instructions.push_back({IrOpcode::CmpLtI32, 0});
      emitBoundsTrapIfStackTrue();

      if (isInternalSoaMetadataInlineHelper(callee.fullPath, "set_field_count") ||
          callLeafName(callExpr) == "set_field_count") {
        if (!emitExpr(value, callerLocals) ||
            !emitReceiverLoad(IrSlotBytes * 2)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitReceiverAddress(IrSlotBytes) ||
            !emitExpr(value, callerLocals)) {
          return false;
        }
      } else {
        if (!emitReceiverLoad(IrSlotBytes) ||
            !emitExpr(value, callerLocals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitExpr(value, callerLocals)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::PushI32, 1073741823});
        function.instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitReceiverAddress(IrSlotBytes * 2) ||
            !emitExpr(value, callerLocals)) {
          return false;
        }
      }
      function.instructions.push_back({IrOpcode::StoreIndirect, 0});
      function.instructions.push_back({IrOpcode::Pop, 0});
      if (requireValue) {
        function.instructions.push_back({IrOpcode::PushI32, 0});
      }
      return true;
    }
    ir_lowerer::InlineDefinitionCallContextSetup callSetup;
    if (!ir_lowerer::prepareInlineDefinitionCallContext(
            callee,
            requireValue,
            [&](const std::string &path, ReturnInfo &infoOut) { return getReturnInfo(path, infoOut); },
            [&](const Definition &candidate) { return isStructDefinition(candidate); },
            inlineStack,
            onErrorByDef,
            callSetup,
            error)) {
      return false;
    }
    const ReturnInfo &returnInfo = callSetup.returnInfo;
    const bool structDef = callSetup.structDefinition;
    OnErrorScope onErrorScope(currentOnError, callSetup.scopedOnError);
    ResultReturnScope resultScope(currentReturnResult, callSetup.scopedResult);
    pushFileScope();
    auto popInlineStack = [&]() {
      if (callSetup.insertedInlineStackEntry) {
        inlineStack.erase(callee.fullPath);
      }
    };
    std::vector<Expr> callParams;
    std::vector<const Expr *> orderedArgs;
    std::vector<const Expr *> packedArgs;
    size_t packedParamIndex = 0;
    if (!ir_lowerer::buildInlineCallOrderedArguments(
            callExpr,
            callee,
            structNames,
            callerLocals,
            callParams,
            orderedArgs,
            packedArgs,
            packedParamIndex,
            error)) {
      popInlineStack();
      return false;
    }

    if (structDef) {
      if (!ir_lowerer::emitInlineStructDefinitionArguments(
              callee.fullPath,
              callParams,
              orderedArgs,
              callerLocals,
              requireValue,
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
                                                                 return resolveMethodCallDefinition(callExpr, callLocals);
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
              [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
                return emitStructCopySlots(destBaseLocal, srcPtrLocal, slotCount);
              },
              [&]() { return allocTempLocal(); },
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              error,
              std::nullopt,
              [&](int32_t destPtrLocal, int32_t srcPtrLocal, const StructSlotFieldInfo &field) {
                bool ranCopyHelper = false;
                return ir_lowerer::emitStructFieldCopyHelpersFromPtrs(
                    destPtrLocal,
                    srcPtrLocal,
                    field,
                    [&](const std::string &path) -> const Definition * {
                      auto copyIt = setupStage.defMap.find(path + "/Copy");
                      return copyIt == setupStage.defMap.end() ? nullptr : copyIt->second;
                    },
                    [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                      return resolveStructSlotLayout(path, layoutOut);
                    },
                    [&]() { return allocTempLocal(); },
                    function.instructions,
                    callerLocals,
                    stateOut.emitInlineDefinitionCall,
                    ranCopyHelper,
                    error);
              },
              [&](const Expr &argExpr, const LocalMap &argLocals) {
                return ir_lowerer::isTemporaryStructCopySourceExpr(
                    argExpr, [&](const Expr &callExpr) {
                      return callExpr.isMethodCall
                                 ? resolveMethodCallDefinition(callExpr, argLocals)
                                 : resolveDefinitionCall(callExpr);
                    });
              })) {
        popFileScope();
        popInlineStack();
        return false;
      }
      // A constructor call has no body scope of its own; leaving it pushed would capture the
      // caller's later locals and skip their block's cleanup.
      emitFileScopeCleanup(fileScopeStack.back());
      popFileScope();
      popInlineStack();
      return true;
    }

    // The arguments written at the call site are evaluated in the caller, before the callee
    // starts, so one of them may call this same definition: `twice(twice(x))` is not recursion.
    // A default argument belongs to the callee and stays guarded.
    bool argumentsFromCallSite = callSetup.insertedInlineStackEntry;
    auto writtenAtCallSite = [&](const Expr *arg) {
      return arg == nullptr ||
             (!callExpr.args.empty() && !std::less<const Expr *>{}(arg, callExpr.args.data()) &&
              std::less<const Expr *>{}(arg, callExpr.args.data() + callExpr.args.size()));
    };
    for (const Expr *arg : orderedArgs) {
      argumentsFromCallSite = argumentsFromCallSite && writtenAtCallSite(arg);
    }
    for (const Expr *arg : packedArgs) {
      argumentsFromCallSite = argumentsFromCallSite && writtenAtCallSite(arg);
    }
    if (argumentsFromCallSite) {
      inlineStack.erase(callee.fullPath);
    }
    LocalMap calleeLocals;
    if (!ir_lowerer::emitInlineDefinitionCallParameters(
            callParams,
            orderedArgs,
            packedArgs,
            packedParamIndex,
            callee.fullPath,
            callerLocals,
            nextLocal,
            calleeLocals,
            [&](const Expr &param, LocalInfo &infoOut, std::string &errorOut) {
              return ir_lowerer::inferCallParameterLocalInfo(param,
                                                             callerLocals,
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
                                                               return resolveMethodCallDefinition(callExpr, callLocals);
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
            [&](const Expr &param) { return isStringBinding(param); },
            [&](const Expr &argExpr,
                const LocalMap &locals,
                LocalInfo::StringSource &sourceOut,
                int32_t &indexOut,
                bool &argvCheckedOut) {
              return emitStringValueForCall(argExpr, locals, sourceOut, indexOut, argvCheckedOut);
            },
            [&](const Expr &argExpr, const LocalMap &locals) {
              return inferStructExprPath(argExpr, locals);
            },
            [&](const Expr &argExpr, const LocalMap &locals) {
              return inferExprKind(argExpr, locals);
            },
            [&](const Expr &argExpr) { return resolveDefinitionCall(argExpr); },
            [&](const std::string &structPath, StructSlotLayoutInfo &layoutOut) {
              return resolveStructSlotLayout(structPath, layoutOut);
            },
            [&](const Expr &argExpr, const LocalMap &locals) { return emitExpr(argExpr, locals); },
            [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
              return emitStructCopySlots(destBaseLocal, srcPtrLocal, slotCount);
            },
            [&]() { return allocTempLocal(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](int32_t localIndex) { fileScopeStack.back().push_back(localIndex); },
            error,
            [&](const Expr &argExpr,
                const LocalMap &localsForInference,
                LocalInfo &infoOut,
                std::string &infoError) -> bool {
              const Expr *targetExpr = &argExpr;
              for (size_t peelSteps = 0; peelSteps < 8; ++peelSteps) {
                if (targetExpr->kind != Expr::Kind::Call ||
                    targetExpr->args.size() != 1 ||
                    (!isSimpleCallName(*targetExpr, "location") &&
                     !isSimpleCallName(*targetExpr, "dereference"))) {
                  break;
                }
                targetExpr = &targetExpr->args.front();
              }

              if (targetExpr->kind == Expr::Kind::Name) {
                auto existingIt = localsForInference.find(targetExpr->name);
                if (existingIt != localsForInference.end()) {
                  infoOut = existingIt->second;
                  return true;
                }
              }

              infoOut = {};
              infoOut.kind = LocalInfo::Kind::Value;
              infoOut.valueKind = inferExprKind(*targetExpr, localsForInference);
              infoOut.structTypeName = inferStructExprPath(*targetExpr, localsForInference);
              infoError.clear();
              return true;
            },
            [&]() { return function.instructions.size(); },
            [&](size_t index, uint64_t target) { function.instructions[index].imm = target; },
            emitArrayIndexOutOfBounds,
            [&](int32_t destPtrLocal,
                int32_t srcPtrLocal,
                const std::string &structPath,
                bool &ranHelper) {
              return ir_lowerer::emitStructCopyHelpersFromPtrs(
                  destPtrLocal,
                  srcPtrLocal,
                  structPath,
                  [&](const std::string &path) -> const Definition * {
                    auto copyIt = setupStage.defMap.find(path + "/Copy");
                    return copyIt == setupStage.defMap.end() ? nullptr : copyIt->second;
                  },
                  [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(path, layoutOut);
                  },
                  [&]() { return allocTempLocal(); },
                  function.instructions,
                  callerLocals,
                  stateOut.emitInlineDefinitionCall,
                  ranHelper,
                  error);
            })) {
      if (std::string_view(error) == VariadicArgsReferenceForwardingDiagnosticMessage) {
        const Expr *diagnosticAnchor = &callExpr;
        for (const Expr *packedArg : packedArgs) {
          if (packedArg != nullptr) {
            diagnosticAnchor = packedArg;
            break;
          }
        }
        captureLoweringDiagnosticPrimarySpan(*diagnosticAnchor);
      }
      popInlineStack();
      return false;
    }
    if (argumentsFromCallSite) {
      inlineStack.insert(callee.fullPath);
    }
    // A `copy` or `move` parameter is the callee's own value, and so is a temporary argument (a
    // call result or constructor nobody else owns): the callee body's scope destroys it unless
    // the callee moves it on (docs/spec/value-lifecycle.md, Ownership).
    for (size_t paramIndex = 0; paramIndex < callParams.size(); ++paramIndex) {
      const Expr &param = callParams[paramIndex];
      const Expr *argExpr = paramIndex < orderedArgs.size() ? orderedArgs[paramIndex] : nullptr;
      const bool temporaryArg = argExpr != nullptr && ir_lowerer::isOwnedTemporaryArgumentExpr(
                                                          *argExpr, [&](const Expr &candidate) {
                                                            return resolveDefinitionCall(candidate);
                                                          });
      // A builtin `vector<T>` `copy` parameter gets a buffer of its own, except from a fresh vector
      // (a constructor or a call returning one) or `move(x)`, which the callee takes over; a fresh vector passed
      // to a read-only parameter belongs to the callee too. The callee's scope frees it
      // (docs/spec/value-lifecycle.md, Ownership).
      if (const auto vectorIt = calleeLocals.find(param.name);
          !fileScopeStack.empty() && argExpr != nullptr && vectorIt != calleeLocals.end() &&
          vectorIt->second.kind == LocalInfo::Kind::Vector && !vectorIt->second.isSoaVector) {
        const bool moveParam = ir_lowerer::parameterHasTransform(param, "move");
        const bool copyParam = !moveParam && ir_lowerer::parameterHasTransform(param, "copy");
        const bool mutParam = ir_lowerer::parameterHasTransform(param, "mut");
        // `move(x)` or a `move` parameter hands the caller's vector over (its drop flag is released): no copy.
        const bool freshArg =
            moveParam || (copyParam && isSimpleCallName(*argExpr, "move")) ||
            ir_lowerer::isFreshBuiltinVectorExpr(
                *argExpr, [&](const Expr &candidate) { return resolveDefinitionCall(candidate); });
        std::string elementStructPath;
        bool elementKnown = false;
        for (const auto &transform : param.transforms) {
          if (transform.templateArgs.size() != 1 ||
              normalizeCollectionBindingTypeName(transform.name) != "vector") {
            continue;
          }
          // A struct, a builtin vector ("/vector<E>") or a scalar element.
          elementKnown = ir_lowerer::resolveBuiltinVectorElementStructPath(
              transform.templateArgs.front(),
              param.namespacePrefix,
              [&](const std::string &typeName,
                  const std::string &namespacePrefix,
                  std::string &pathOut) {
                return resolveStructTypeName(typeName, namespacePrefix, pathOut);
              },
              elementStructPath);
          break;
        }
        // The slots of an inline element: a temporary argument carries no local to take them from.
        if (StructSlotLayoutInfo elementLayout;
            elementKnown && !elementStructPath.empty() &&
            vectorIt->second.vectorStructElementSlotCount == 0 &&
            resolveStructSlotLayout(elementStructPath, elementLayout)) {
          vectorIt->second.vectorStructElementSlotCount = elementLayout.totalSlots;
        }
        if (elementKnown && mutParam && !copyParam && !freshArg) {
          // A mutable borrow of the caller's vector: assigning to it replaces the caller's record in
          // place, and the caller stays the owner.
          vectorIt->second.vectorElementLifecyclePath = elementStructPath;
        }
        if (elementKnown && (copyParam || freshArg)) {
          LocalInfo &vectorInfo = vectorIt->second;
          if (mutParam) {
            vectorInfo.vectorElementLifecyclePath = elementStructPath;
          }
          if (!freshArg) {
            constexpr int32_t VectorRecordSlots = 5;
            const int32_t recordBaseLocal = nextLocal;
            nextLocal += VectorRecordSlots;
            if (!emitStructCopySlots(recordBaseLocal, vectorInfo.index, VectorRecordSlots)) {
              return false;
            }
            const int32_t recordPtrLocal = allocTempLocal();
            function.instructions.push_back(
                {IrOpcode::AddressOfLocal, static_cast<uint64_t>(recordBaseLocal)});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(recordPtrLocal)});
            if (!ir_lowerer::emitBuiltinVectorCopyFromPtrs(
                    recordPtrLocal,
                    vectorInfo.index,
                    elementStructPath,
                    [&](const std::string &path) -> const Definition * {
                      auto copyIt = setupStage.defMap.find(path + "/Copy");
                      return copyIt == setupStage.defMap.end() ? nullptr : copyIt->second;
                    },
                    [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                      return resolveStructSlotLayout(path, layoutOut);
                    },
                    [&]() { return allocTempLocal(); },
                    function.instructions,
                    callerLocals,
                    stateOut.emitInlineDefinitionCall,
                    error)) {
              return false;
            }
            function.instructions.push_back(
                {IrOpcode::LoadLocal, static_cast<uint64_t>(recordPtrLocal)});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(vectorInfo.index)});
          }
          std::string destroyElementPath;
          if (!elementStructPath.empty() &&
              ir_lowerer::structNeedsDestroyHelpers(
                  elementStructPath,
                  [&](const std::string &path) {
                    return ir_lowerer::findStackDestroyHelper(setupStage.defMap, path);
                  },
                  [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                    return resolveStructSlotLayout(path, layoutOut);
                  })) {
            destroyElementPath = elementStructPath;
          }
          vectorInfo.dropFlagLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(vectorInfo.dropFlagLocal)});
          LowerSetupStageState::DropEntry entry{
              vectorInfo.index, vectorInfo.dropFlagLocal, destroyElementPath};
          entry.builtinVectorElementSlots =
              std::max<int32_t>(1, vectorInfo.vectorStructElementSlotCount);
          setupStage.dropEntries.push_back(std::move(entry));
          fileScopeStack.back().push_back(-static_cast<int32_t>(setupStage.dropEntries.size()));
          continue;
        }
      }
      const bool ownedParam = ir_lowerer::parameterHasTransform(param, "copy") ||
                              ir_lowerer::parameterHasTransform(param, "move");
      if (fileScopeStack.empty() || (!ownedParam && !temporaryArg)) {
        continue;
      }
      // A real call's body owns its `copy` and `move` parameters and destroys them itself (it may move them on);
      // the call site only binds them.
      if (ownedParam && addressAbiCallImm.has_value()) {
        continue;
      }
      auto ownedIt = calleeLocals.find(param.name);
      // A sum nobody else owns (a call result or a constructor) or one handed to a `move` parameter is the
      // callee's: its active payload is destroyed when the callee's body ends.
      if ((temporaryArg || ir_lowerer::parameterHasTransform(param, "move")) &&
          ownedIt != calleeLocals.end() && ownedIt->second.kind == LocalInfo::Kind::Value) {
        SumHelpersContext sumHelpers(setupStage, stateOut, callResolutionAdapters, error);
        if (const Definition *sumDef =
                sumHelpers.resolveSumDefinitionByPath(ownedIt->second.structTypeName);
            sumDef != nullptr && sumHelpers.sumPayloadsNeedDestroy(*sumDef)) {
          LocalInfo &sumInfo = ownedIt->second;
          sumInfo.dropFlagLocal = allocTempLocal();
          function.instructions.push_back({IrOpcode::PushI32, 1});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(sumInfo.dropFlagLocal)});
          LowerSetupStageState::DropEntry entry{
              sumInfo.index, sumInfo.dropFlagLocal, sumDef->fullPath};
          entry.sumDef = sumDef;
          setupStage.dropEntries.push_back(std::move(entry));
          fileScopeStack.back().push_back(-static_cast<int32_t>(setupStage.dropEntries.size()));
          continue;
        }
      }
      if (ownedIt == calleeLocals.end() || ownedIt->second.structTypeName.empty() ||
          (ownedIt->second.kind != LocalInfo::Kind::Value &&
           ownedIt->second.kind != LocalInfo::Kind::Reference) ||
          !ir_lowerer::structNeedsDestroyHelpers(
              ownedIt->second.structTypeName,
              [&](const std::string &path) {
                return ir_lowerer::findStackDestroyHelper(setupStage.defMap, path);
              },
              [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                return resolveStructSlotLayout(path, layoutOut);
              })) {
        continue;
      }
      LocalInfo &ownedInfo = ownedIt->second;
      ownedInfo.dropFlagLocal = allocTempLocal();
      function.instructions.push_back({IrOpcode::PushI32, 1});
      function.instructions.push_back(
          {IrOpcode::StoreLocal, static_cast<uint64_t>(ownedInfo.dropFlagLocal)});
      setupStage.dropEntries.push_back(
          {ownedInfo.index, ownedInfo.dropFlagLocal, ownedInfo.structTypeName});
      fileScopeStack.back().push_back(-static_cast<int32_t>(setupStage.dropEntries.size()));
    }
