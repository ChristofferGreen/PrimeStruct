  emitInlineDefinitionCall = [&](const Expr &callExpr,
                                 const Definition &callee,
                                 const LocalMap &callerLocals,
                                 bool requireValue) -> bool {
    if (callExpr.isMethodCall && callExpr.args.size() == 1 &&
        (isSimpleCallName(callExpr, "field_count") ||
         isSimpleCallName(callExpr, "field_capacity")) &&
        (callee.fullPath.rfind("/std/collections/internal_soa_storage/SoaColumn", 0) == 0 ||
         callee.fullPath.rfind("/std/collections/internal_soa_storage/SoaFieldView", 0) == 0)) {
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
      if (isSimpleCallName(callExpr, "field_capacity")) {
        function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
        function.instructions.push_back({IrOpcode::AddI64, 0});
      }
      function.instructions.push_back({IrOpcode::LoadIndirect, 0});
      return true;
    }
    ir_lowerer::InlineDefinitionCallContextSetup callSetup;
    if (!ir_lowerer::prepareInlineDefinitionCallContext(
            callee,
            requireValue,
            [&](const std::string &path, ReturnInfo &infoOut) { return getReturnInfo(path, infoOut); },
            [&](const Definition &candidate) { return isStructDefinition(candidate); },
            inlineStack,
            loweredCallTargets,
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
              error)) {
        popInlineStack();
        return false;
      }
      popInlineStack();
      return true;
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
            [&](const Expr &argExpr, const LocalMap &locals) { return inferStructExprPath(argExpr, locals); },
            [&](const Expr &argExpr, const LocalMap &locals) { return inferExprKind(argExpr, locals); },
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
            })) {
      popInlineStack();
      return false;
    }

    if (!ir_lowerer::runLowerInlineCallGpuLocalsStep(
            {
                .callerLocals = &callerLocals,
                .calleeLocals = &calleeLocals,
            },
            error)) {
      popInlineStack();
      return false;
    }

    const bool isBuiltinCanonicalMapInsertCallee =
        callee.fullPath == "/std/collections/map/insert_builtin" ||
        callee.fullPath.rfind("/std/collections/map/insert_builtin__", 0) == 0;
    const bool isGeneratedMapInsertHelper =
        callee.fullPath == "/std/collections/mapInsert" ||
        callee.fullPath.rfind("/std/collections/mapInsert__", 0) == 0 ||
        callee.fullPath == "/std/collections/experimental_map/mapInsert" ||
        callee.fullPath.rfind("/std/collections/experimental_map/mapInsert__", 0) == 0 ||
        callee.fullPath == "/std/collections/experimental_map/mapInsertRef" ||
        callee.fullPath.rfind("/std/collections/experimental_map/mapInsertRef__", 0) == 0;
    if (isBuiltinCanonicalMapInsertCallee || isGeneratedMapInsertHelper) {
      auto extractParameterTypeName = [](const Expr &paramExpr) {
        for (const auto &transform : paramExpr.transforms) {
          if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
              transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
              transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
              transform.name == "effects" || transform.name == "capabilities") {
            continue;
          }
          if (!transform.arguments.empty()) {
            continue;
          }
          std::string typeName = transform.name;
          if (!transform.templateArgs.empty()) {
            typeName += "<";
            for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
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
      auto inferValueKindFromTypeText = [&](std::string typeText,
                                            LocalInfo::ValueKind &kindOut) {
        kindOut = LocalInfo::ValueKind::Unknown;
        typeText = trimTemplateTypeText(typeText);
        while (!typeText.empty()) {
          kindOut = valueKindFromTypeName(typeText);
          if (kindOut != LocalInfo::ValueKind::Unknown) {
            return true;
          }

          std::string base;
          std::string argText;
          if (!splitTemplateTypeName(typeText, base, argText)) {
            return false;
          }

          const std::string normalizedBase = trimTemplateTypeText(base);
          if ((normalizedBase == "Reference" || normalizedBase == "Pointer") && !argText.empty()) {
            typeText = trimTemplateTypeText(argText);
            continue;
          }
          return false;
        }
        return false;
      };
      auto valuesIt = calleeLocals.find("values");
      if (valuesIt == calleeLocals.end()) {
        valuesIt = calleeLocals.find("entries");
      }
      auto keyIt = calleeLocals.find("key");
      auto valueIt = calleeLocals.find("value");
      if (valuesIt == calleeLocals.end() || keyIt == calleeLocals.end() || valueIt == calleeLocals.end()) {
        error = "builtin canonical map insert lowering requires values or entries plus key/value locals";
        popInlineStack();
        return false;
      }
      const Expr *originalValuesArg = nullptr;
      if (!orderedArgs.empty() && orderedArgs.front() != nullptr) {
        originalValuesArg = orderedArgs.front();
        if (originalValuesArg->kind == Expr::Kind::Call &&
            isSimpleCallName(*originalValuesArg, "dereference") &&
            originalValuesArg->args.size() == 1 &&
            originalValuesArg->args.front().kind == Expr::Kind::Name) {
          originalValuesArg = &originalValuesArg->args.front();
        }
        if (originalValuesArg->kind == Expr::Kind::Name) {
          auto callerValuesIt = callerLocals.find(originalValuesArg->name);
          if (callerValuesIt != callerLocals.end() &&
              callerValuesIt->second.mapKeyKind != LocalInfo::ValueKind::Unknown &&
              callerValuesIt->second.mapValueKind != LocalInfo::ValueKind::Unknown) {
            valuesIt->second.mapKeyKind = callerValuesIt->second.mapKeyKind;
            valuesIt->second.mapValueKind = callerValuesIt->second.mapValueKind;
          }
        }
      }
      auto isExperimentalMapStructPath = [](const std::string &structPath) {
        return structPath == "/std/collections/experimental_map/Map" ||
               structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0;
      };
      bool receiverUsesExperimentalMapStruct =
          isExperimentalMapStructPath(valuesIt->second.structTypeName);
      if (!receiverUsesExperimentalMapStruct && originalValuesArg != nullptr &&
          originalValuesArg->kind == Expr::Kind::Name) {
        auto callerValuesIt = callerLocals.find(originalValuesArg->name);
        if (callerValuesIt != callerLocals.end()) {
          receiverUsesExperimentalMapStruct =
              isExperimentalMapStructPath(callerValuesIt->second.structTypeName);
        }
      }
      if (!isBuiltinCanonicalMapInsertCallee && receiverUsesExperimentalMapStruct) {
        // Experimental-map receivers still need the real stdlib helper body,
        // because the builtin canonical insert path mutates the flat map
        // storage layout, not the struct-backed experimental map layout.
      } else {
        if (valuesIt->second.mapKeyKind == LocalInfo::ValueKind::Unknown &&
            callee.parameters.size() >= 3) {
          LocalInfo::ValueKind inferredKeyKind = LocalInfo::ValueKind::Unknown;
          LocalInfo::ValueKind inferredValueKind = LocalInfo::ValueKind::Unknown;
          if (inferValueKindFromTypeText(extractParameterTypeName(callee.parameters[1]), inferredKeyKind) &&
              inferValueKindFromTypeText(extractParameterTypeName(callee.parameters[2]), inferredValueKind)) {
            valuesIt->second.mapKeyKind = inferredKeyKind;
            valuesIt->second.mapValueKind = inferredValueKind;
          }
        }
        if (valuesIt->second.mapKeyKind == LocalInfo::ValueKind::Unknown) {
          error = "builtin canonical map insert lowering requires typed map bindings";
          popInlineStack();
          return false;
        }
        auto isDirectMapStorageLocal = [](const LocalInfo &info) {
          return info.kind == LocalInfo::Kind::Map ||
                 (info.kind == LocalInfo::Kind::Value &&
                  info.mapKeyKind != LocalInfo::ValueKind::Unknown &&
                  info.mapValueKind != LocalInfo::ValueKind::Unknown);
        };
        int32_t valuesLocal =
            isDirectMapStorageLocal(valuesIt->second) ? valuesIt->second.index : -1;
        int32_t valuesWrapperLocal = -1;
        int32_t ptrLocal = valuesIt->second.index;
        if (originalValuesArg != nullptr) {
          if (originalValuesArg->kind == Expr::Kind::Name) {
            auto callerValuesIt = callerLocals.find(originalValuesArg->name);
            if (callerValuesIt != callerLocals.end()) {
              if (isDirectMapStorageLocal(callerValuesIt->second)) {
                valuesLocal = callerValuesIt->second.index;
              } else if ((callerValuesIt->second.kind == LocalInfo::Kind::Reference &&
                          callerValuesIt->second.referenceToMap) ||
                         (callerValuesIt->second.kind == LocalInfo::Kind::Pointer &&
                          callerValuesIt->second.pointerToMap)) {
                valuesWrapperLocal = callerValuesIt->second.index;
              }
            }
          }
        }
        if (valuesIt->second.kind == LocalInfo::Kind::Reference ||
            valuesIt->second.kind == LocalInfo::Kind::Pointer) {
          if (valuesIt->second.referenceToMap || valuesIt->second.pointerToMap) {
            valuesWrapperLocal = valuesIt->second.index;
            ptrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valuesIt->second.index)});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
          } else {
            ptrLocal = valuesIt->second.index;
          }
        }
        if (!ir_lowerer::emitBuiltinCanonicalMapInsertOverwriteOrGrow(
                valuesLocal,
                valuesWrapperLocal,
                ptrLocal,
                keyIt->second.index,
                valueIt->second.index,
                valuesIt->second.mapKeyKind,
                [&]() { return allocTempLocal(); },
                [&]() { return function.instructions.size(); },
                [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                [&](size_t indexToPatch, uint64_t target) { function.instructions[indexToPatch].imm = target; })) {
          error = "failed to lower builtin canonical map insert helper";
          popInlineStack();
          return false;
        }
        if (requireValue) {
          function.instructions.push_back({IrOpcode::PushI32, 0});
        }
        emitFileScopeCleanup(fileScopeStack.back());
        popFileScope();
        popInlineStack();
        return true;
      }
    }

    InlineContext context;
    context.defPath = callee.fullPath;
    ir_lowerer::LowerInlineCallContextSetupStepOutput contextSetup;
    if (!ir_lowerer::runLowerInlineCallContextSetupStep(
            {
                .function = &function,
                .returnInfo = &returnInfo,
                .allocTempLocal = [&]() { return allocTempLocal(); },
            },
            contextSetup,
            error)) {
      popInlineStack();
      return false;
    }
    context.returnsVoid = contextSetup.returnsVoid;
    context.returnsArray = contextSetup.returnsArray;
    context.returnKind = contextSetup.returnKind;
    context.returnLocal = contextSetup.returnLocal;

    InlineContext *prevContext = activeInlineContext;
    if (!ir_lowerer::runLowerInlineCallActiveContextStep(
            {
                .callee = &callee,
                .structDefinition = structDef,
                .definitionReturnsVoid = context.returnsVoid,
                .activateInlineContext = [&]() { activeInlineContext = &context; },
                .restoreInlineContext = [&]() { activeInlineContext = prevContext; },
                .emitInlineStatement = [&](const Expr &stmt) {
                  return ir_lowerer::runLowerInlineCallStatementStep(
                      {
                          .function = &function,
                          .emitStatement = [&](const Expr &inlineStmt) { return emitStatement(inlineStmt, calleeLocals); },
                          .appendInstructionSourceRange = [&](const std::string &functionName,
                                                              const Expr &inlineStmt,
                                                              size_t beginIndex,
                                                              size_t endIndex) {
                            appendInstructionSourceRange(functionName, inlineStmt, beginIndex, endIndex);
                          },
                      },
                      stmt,
                      error);
                },
                .runInlineCleanup = [&]() {
                  return ir_lowerer::runLowerInlineCallCleanupStep(
                      {
                          .function = &function,
                          .returnJumps = &context.returnJumps,
                          .emitCurrentFileScopeCleanup = [&]() { emitFileScopeCleanup(fileScopeStack.back()); },
                          .popFileScope = [&]() { popFileScope(); },
                      },
                      error);
                },
            },
            error)) {
      popInlineStack();
      return false;
    }

    if (!ir_lowerer::runLowerInlineCallReturnValueStep(
            {
                .function = &function,
                .returnsVoid = context.returnsVoid,
                .returnLocal = context.returnLocal,
                .structDefinition = structDef,
                .requireValue = requireValue,
            },
            error)) {
      popInlineStack();
      return false;
    }
    if (requireValue && context.returnsVoid && !structDef && isGeneratedMapInsertHelper) {
      function.instructions.push_back({IrOpcode::PushI32, 0});
    }

    popInlineStack();
    return true;
  };
