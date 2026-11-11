  emitStatement = [&](const Expr &stmtInput, LocalMap &localsIn) -> bool {
    Expr normalizedStmt = stmtInput;
    auto resolveDirectKeyValueHelperPath = [&](const Expr &exprIn) {
      if (!exprIn.name.empty() && exprIn.name.front() == '/') {
        return exprIn.name;
      }
      if (!exprIn.namespacePrefix.empty()) {
        std::string scoped = exprIn.namespacePrefix;
        if (!scoped.empty() && scoped.front() != '/') {
          scoped.insert(scoped.begin(), '/');
        }
        return scoped + "/" + exprIn.name;
      }
      return exprIn.name;
    };
    auto findDirectKeyValueHelperDefinition = [&](const std::string &rawPath) -> const Definition * {
      auto defIt = defMap.find(rawPath);
      if (defIt != defMap.end()) {
        return defIt->second;
      }
      const std::string specializedPrefix = rawPath + "__t";
      const std::string overloadPrefix = rawPath + "__ov";
      for (const auto &[path, def] : defMap) {
        if (def == nullptr) {
          continue;
        }
        if (path.rfind(specializedPrefix, 0) == 0 ||
            path.rfind(overloadPrefix, 0) == 0) {
          return def;
        }
      }
      return nullptr;
    };
    auto returnsMapLikeValue = [&](const Definition *callee) {
      if (callee == nullptr) {
        return false;
      }
      for (const auto &transform : callee->transforms) {
        if (transform.name != "return" || transform.templateArgs.empty()) {
          continue;
        }
        std::string base;
        std::string argText;
        const std::string returnType = trimTemplateTypeText(transform.templateArgs.front());
        if (splitTemplateTypeName(returnType, base, argText)) {
          return normalizeCollectionBindingTypeName(trimTemplateTypeText(base)) == "map";
        }
        return normalizeCollectionBindingTypeName(returnType) == "map";
      }
      return false;
    };
    std::function<void(Expr &)> canonicalizeExplicitBuiltinKeyValueHelpers =
        [&](Expr &exprIn) {
          for (auto &argExpr : exprIn.args) {
            canonicalizeExplicitBuiltinKeyValueHelpers(argExpr);
          }
          for (auto &bodyExpr : exprIn.bodyArguments) {
            canonicalizeExplicitBuiltinKeyValueHelpers(bodyExpr);
          }
          if (exprIn.kind != Expr::Kind::Call || exprIn.isMethodCall || exprIn.args.empty()) {
            return;
          }
          std::string helperName;
          if (!resolveKeyValueHelperAliasName(exprIn, helperName) ||
              (helperName != "count" && helperName != "contains" &&
               helperName != "tryAt")) {
            return;
          }
          if (exprIn.name.find('/') == std::string::npos &&
              exprIn.namespacePrefix.empty() &&
              exprIn.templateArgs.empty()) {
            return;
          }
          const std::string rawPath = resolveDirectKeyValueHelperPath(exprIn);
          std::string directHelperName;
          const auto *metadata =
              keyValueHelperSurfaceMetadata();
          if (metadata != nullptr &&
              resolvePublishedStdlibSurfaceMemberName(
                  rawPath,
                  metadata->id,
                  directHelperName) &&
              findDirectKeyValueHelperDefinition(rawPath) != nullptr) {
            return;
          }
          exprIn.name = helperName;
          exprIn.namespacePrefix.clear();
          exprIn.templateArgs.clear();
        };
    canonicalizeExplicitBuiltinKeyValueHelpers(normalizedStmt);
    const Expr &stmt = normalizedStmt;
    auto extractDeclaredStructReturnPath = [&]() {
      const std::string &definitionPath =
          activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
      auto defIt = defMap.find(definitionPath);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        return std::string{};
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        const std::string typeName = trimTemplateTypeText(transform.templateArgs.front());
        if (!typeName.empty() && typeName.front() == '/') {
          return typeName;
        }
        std::string resolvedStructPath;
        if (resolveStructTypeName(typeName, defIt->second->namespacePrefix, resolvedStructPath)) {
          return resolvedStructPath;
        }
      }
      return std::string{};
    };
    // A builtin vector local owns its record only when the initializer makes a fresh vector (the
    // vector constructor or a user definition returning one), never when it aliases a name, field,
    // element or stdlib accessor result.
    auto initializerMakesFreshBuiltinVector = [&](const LocalInfo &vectorInfo,
                                                  const Expr &initExpr) {
      if (fileScopeStack.empty() || vectorInfo.kind != LocalInfo::Kind::Vector ||
          vectorInfo.isSoaVector) {
        return false;
      }
      return ir_lowerer::isFreshBuiltinVectorExpr(
          initExpr, [&](const Expr &candidate) { return resolveDefinitionCall(candidate); });
    };
    auto extractDeclaredSumReturnDefinition = [&]() -> const Definition * {
      const std::string &definitionPath =
          activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
      auto defIt = defMap.find(definitionPath);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        return nullptr;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        if (const Definition *sumDef = sumHelpers.resolveSumDefinitionForTypeText(
            trimTemplateTypeText(transform.templateArgs.front()),
            defIt->second->namespacePrefix)) {
          return sumDef;
        }
        break;
      }
      const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
      if (semanticTargets.hasSemanticProduct) {
        const SemanticProgramReturnFact *returnFact =
            findSemanticProductReturnFactByPath(semanticTargets, definitionPath);
        if (returnFact != nullptr) {
          auto resolveSemanticReturnSum =
              [&](const std::string &typeText, SymbolId typeTextId) -> const Definition * {
            return sumHelpers.resolveSumDefinitionForTypeText(
                resolveSemanticProductTypeText(semanticTargets.semanticProgram,
                                               typeText,
                                               typeTextId),
                defIt->second->namespacePrefix);
          };
          if (const Definition *sumDef =
                  resolveSemanticReturnSum(returnFact->structPath,
                                           returnFact->structPathId)) {
            return sumDef;
          }
          if (const Definition *sumDef =
                  resolveSemanticReturnSum(returnFact->bindingTypeText,
                                           returnFact->bindingTypeTextId)) {
            return sumDef;
          }
        }
      }
      return nullptr;
    };
    auto declaredReturnBase = [&]() {
      const std::string &definitionPath =
          activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
      auto defIt = defMap.find(definitionPath);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        return std::string{};
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, arg)) {
          return std::string{};
        }
        return normalizeDeclaredCollectionTypeBase(base);
      }
      return std::string{};
    }();
    const bool declaredReturnIsReferenceHandle = declaredReturnBase == "Reference";
    const bool declaredReturnIsPointerLikeHandle =
        declaredReturnBase == "Reference" || declaredReturnBase == "Pointer" ||
        declaredReturnBase == "map";
    if (!stmt.isBinding && stmt.kind == Expr::Kind::StringLiteral) {
      error = "native backend does not support string literal statements";
      return false;
    }
    if (stmt.isBinding) {
      if (stmt.args.size() != 1) {
        error = "binding requires exactly one argument";
        return false;
      }
      if (localsIn.count(stmt.name) > 0) {
        error = "binding redefines existing name: " + stmt.name;
        return false;
      }
      const Expr &init = stmt.args.front();
      std::string uninitializedType;
      if (!extractUninitializedTemplateArg(stmt, uninitializedType) &&
          init.kind == Expr::Kind::Call &&
          !init.isMethodCall &&
          isSimpleCallName(init, "uninitialized") &&
          init.templateArgs.size() == 1) {
        uninitializedType = trimTemplateTypeText(init.templateArgs.front());
      }
      if (!uninitializedType.empty()) {
        if (const Definition *sumDef = sumHelpers.resolveSumDefinitionForTypeText(uninitializedType, stmt.namespacePrefix)) {
          int32_t totalSlots = 0;
          if (!sumHelpers.loweredSumSlotCount(*sumDef, totalSlots)) {
            if (!error.empty()) {
              return false;
            }
            if (const SumVariant *unsupportedVariant = sumHelpers.firstUnsupportedSumPayloadVariant(*sumDef);
                unsupportedVariant != nullptr) {
              error = sumHelpers.unsupportedSumPayloadError(*sumDef, *unsupportedVariant);
            } else {
              error = "native backend does not support sum payload type on " + sumDef->fullPath;
            }
            return false;
          }
          LocalInfo info;
          info.isMutable = isBindingMutable(stmt);
          info.isUninitializedStorage = true;
          info.kind = LocalInfo::Kind::Value;
          info.structTypeName = sumDef->fullPath;
          info.structSlotCount = totalSlots;
          const int32_t baseLocal = nextLocal;
          nextLocal += totalSlots;
          info.index = nextLocal++;
          sumHelpers.emitLoweredSumHeader(baseLocal, totalSlots);
          function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
          localsIn.emplace(stmt.name, info);
          return true;
        }
        UninitializedTypeInfo uninitInfo;
        if (!resolveUninitializedTypeInfo(uninitializedType, stmt.namespacePrefix, uninitInfo)) {
          if (error.empty()) {
            error = "native backend does not support uninitialized storage for type: " + uninitializedType;
          }
          return false;
        }
        LocalInfo info;
        info.isMutable = isBindingMutable(stmt);
        info.isUninitializedStorage = true;
        info.kind = uninitInfo.kind;
        info.valueKind = uninitInfo.valueKind;
        info.keyValueKeyKind = uninitInfo.keyValueKeyKind;
        info.keyValueValueKind = uninitInfo.keyValueValueKind;
        info.structTypeName = uninitInfo.structPath;
        if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
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
          localsIn.emplace(stmt.name, info);
          return true;
        }
        info.index = nextLocal++;
        IrOpcode zeroOp = IrOpcode::PushI32;
        uint64_t zeroImm = 0;
        if (!selectUninitializedStorageZeroInstruction(
                info.kind, info.valueKind, stmt.name, zeroOp, zeroImm, error)) {
          return false;
        }
        function.instructions.push_back({zeroOp, zeroImm});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        localsIn.emplace(stmt.name, info);
        return true;
      }
      auto isLocalAutoBindingCandidate = [&](const Expr &bindingExpr) {
        std::string explicitTypeName;
        std::vector<std::string> explicitTemplateArgs;
        if (!extractFirstBindingTypeTransform(bindingExpr, explicitTypeName, explicitTemplateArgs)) {
          return true;
        }
        return trimTemplateTypeText(explicitTypeName) == "auto";
      };
      Expr semanticLocalAutoBindingExpr;
      const Expr *bindingTypeExpr = &stmt;
      if (callResolutionAdapters.semanticProgram != nullptr &&
          stmt.semanticNodeId != 0 &&
          isLocalAutoBindingCandidate(stmt)) {
        const SemanticProgramLocalAutoFact *localAutoFact =
            findSemanticProductLocalAutoFactBySemanticId(
                callResolutionAdapters.semanticProductTargets.semanticIndex,
                stmt);
        std::string bindingTypeText;
        if (localAutoFact != nullptr) {
          if (localAutoFact->bindingTypeTextId != InvalidSymbolId) {
            bindingTypeText = std::string(semanticProgramResolveCallTargetString(
                *callResolutionAdapters.semanticProgram,
                localAutoFact->bindingTypeTextId));
          }
          if (bindingTypeText.empty()) {
            bindingTypeText = localAutoFact->bindingTypeText;
          }
          bindingTypeText = trimTemplateTypeText(bindingTypeText);
        }
        if (bindingTypeText.empty()) {
          const std::string scopePath =
              activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
          error = "missing semantic-product local-auto fact: " + scopePath + " -> local " +
                  (stmt.name.empty() ? std::string("<unnamed>") : stmt.name);
          return false;
        }
        semanticLocalAutoBindingExpr = stmt;
        semanticLocalAutoBindingExpr.semanticNodeId = 0;
        semanticLocalAutoBindingExpr.transforms.clear();
        Transform semanticTypeTransform;
        std::string semanticTypeBase;
        std::string semanticTypeArgList;
        if (splitTemplateTypeName(bindingTypeText, semanticTypeBase, semanticTypeArgList)) {
          semanticTypeTransform.name = trimTemplateTypeText(semanticTypeBase);
          if (!semanticTypeArgList.empty()) {
            if (!splitTemplateArgs(semanticTypeArgList, semanticTypeTransform.templateArgs)) {
              semanticTypeTransform.templateArgs.push_back(trimTemplateTypeText(semanticTypeArgList));
            }
          }
        } else {
          semanticTypeTransform.name = bindingTypeText;
        }
        semanticLocalAutoBindingExpr.transforms.push_back(std::move(semanticTypeTransform));
        bindingTypeExpr = &semanticLocalAutoBindingExpr;
      }
      auto isTaskTransformName = [](std::string name) {
        if (!name.empty() && name.front() == '/') {
          name.erase(name.begin());
        }
        return trimTemplateTypeText(name) == "Task";
      };
      auto isTaskSpawnTransformName = [](std::string name) {
        if (!name.empty() && name.front() == '/') {
          name.erase(name.begin());
        }
        return trimTemplateTypeText(name) == "spawn";
      };
      auto extractTaskResultType = [&](const Expr &bindingExpr,
                                       std::string &resultTypeOut) {
        resultTypeOut.clear();
        for (const Transform &transform : bindingExpr.transforms) {
          if (!isTaskTransformName(transform.name) ||
              transform.templateArgs.size() != 1 ||
              !transform.arguments.empty()) {
            continue;
          }
          resultTypeOut = trimTemplateTypeText(transform.templateArgs.front());
          return !resultTypeOut.empty();
        }
        return false;
      };
      auto removeTaskSpawnTransform = [&](Expr expr) {
        expr.transforms.erase(
            std::remove_if(expr.transforms.begin(),
                           expr.transforms.end(),
                           [&](const Transform &transform) {
                             return isTaskSpawnTransformName(transform.name);
                           }),
            expr.transforms.end());
        return expr;
      };
      auto hasTaskSpawnTransform = [&](const Expr &candidate) {
        return std::any_of(candidate.transforms.begin(),
                           candidate.transforms.end(),
                           [&](const Transform &transform) {
                             return isTaskSpawnTransformName(transform.name);
                           });
      };
      auto extractTaskSpawnInitializer = [&](const Expr &candidate,
                                             Expr &spawnedCallOut) {
        spawnedCallOut = {};
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
          return false;
        }
        if (hasTaskSpawnTransform(candidate)) {
          spawnedCallOut = removeTaskSpawnTransform(candidate);
          return spawnedCallOut.kind == Expr::Kind::Call && !spawnedCallOut.isBinding;
        }
        if (candidate.isMethodCall || candidate.isFieldAccess ||
            !isTaskTransformName(candidate.name) ||
            candidate.templateArgs.size() != 1) {
          return false;
        }
        bool hasNamedArg = false;
        bool hasSpawnLabel = false;
        for (const auto &argName : candidate.argNames) {
          if (!argName.has_value()) {
            continue;
          }
          hasNamedArg = true;
          if (isTaskSpawnTransformName(*argName) && !hasSpawnLabel) {
            hasSpawnLabel = true;
            continue;
          }
          return false;
        }
        if (hasNamedArg && (!hasSpawnLabel || candidate.argNames.size() != 1 ||
                            candidate.args.size() != 1 ||
                            candidate.hasBodyArguments ||
                            !candidate.bodyArguments.empty())) {
          return false;
        }
        const Expr *initializer = nullptr;
        if (candidate.args.size() == 1 && !candidate.hasBodyArguments &&
            candidate.bodyArguments.empty()) {
          initializer = &candidate.args.front();
        } else if (candidate.args.empty() && candidate.hasBodyArguments &&
                   candidate.bodyArguments.size() == 1) {
          initializer = &candidate.bodyArguments.front();
        }
        if (initializer == nullptr ||
            (!hasSpawnLabel && !hasTaskSpawnTransform(*initializer)) ||
            initializer->kind != Expr::Kind::Call || initializer->isBinding) {
          return false;
        }
        spawnedCallOut = removeTaskSpawnTransform(*initializer);
        return spawnedCallOut.kind == Expr::Kind::Call && !spawnedCallOut.isBinding;
      };
      auto replaceTaskTransformWithResultType = [&](Expr &bindingExpr,
                                                    const std::string &resultType) {
        for (Transform &transform : bindingExpr.transforms) {
          if (!isTaskTransformName(transform.name) ||
              transform.templateArgs.size() != 1 ||
              !transform.arguments.empty()) {
            continue;
          }
          std::string base;
          std::string argList;
          transform.arguments.clear();
          transform.templateArgs.clear();
          if (splitTemplateTypeName(resultType, base, argList)) {
            transform.name = trimTemplateTypeText(base);
            if (!argList.empty() &&
                !splitTemplateArgs(argList, transform.templateArgs)) {
              transform.templateArgs.push_back(trimTemplateTypeText(argList));
            }
          } else {
            transform.name = trimTemplateTypeText(resultType);
          }
          return true;
        }
        return false;
      };
      std::string taskResultType;
      Expr spawnedTaskCall;
      if (extractTaskResultType(*bindingTypeExpr, taskResultType) &&
          extractTaskSpawnInitializer(init, spawnedTaskCall)) {
        if (setupStage.tasksRunOnScheduler) {
          // The runtime runs the spawned call as a task when a real call takes its shape: the arguments are evaluated
          // here and become the task's, and the binding holds the handle (docs/spec/vm-design.md, Tasks).
          const Definition *spawnedCallee = resolveDefinitionCall(spawnedTaskCall);
          std::string inlineReason = "it is not a definition";
          ir_lowerer::TaskSpawnShape taskShape;
          if (spawnedCallee != nullptr &&
              ir_lowerer::taskSpawnRunsOnScheduler(
                  *spawnedCallee, setupStage, taskShape, inlineReason)) {
            std::vector<Expr> taskParams;
            std::vector<const Expr *> taskArgs;
            std::vector<const Expr *> taskPackedArgs;
            size_t taskPackedParamIndex = 0;
            if (!ir_lowerer::buildInlineCallOrderedArguments(spawnedTaskCall,
                                                             *spawnedCallee,
                                                             structNames,
                                                             localsIn,
                                                             taskParams,
                                                             taskArgs,
                                                             taskPackedArgs,
                                                             taskPackedParamIndex,
                                                             error)) {
              return false;
            }
            if (!taskPackedArgs.empty() || taskArgs.size() != spawnedCallee->parameters.size() ||
                taskArgs.size() != taskShape.parameters.size()) {
              error = "internal error: unexpected arguments for spawned task " +
                      spawnedCallee->fullPath;
              return false;
            }
            for (const Expr *taskArg : taskArgs) {
              if (taskArg == nullptr) {
                error =
                    "internal error: missing argument for spawned task " + spawnedCallee->fullPath;
                return false;
              }
            }
            const bool usesTaskBlock =
                !taskShape.resultStructPath.empty() ||
                std::any_of(taskShape.parameters.begin(),
                            taskShape.parameters.end(),
                            [](const ir_lowerer::TaskSpawnParameterShape &parameter) {
                              return parameter.kind !=
                                     ir_lowerer::TaskSpawnParameterShape::Kind::Scalar;
                            });
            if (!usesTaskBlock) {
              for (const Expr *taskArg : taskArgs) {
                if (!emitExpr(*taskArg, localsIn)) {
                  return false;
                }
              }
              const uint64_t reservation =
                  ir_lowerer::reserveTaskSpawnTarget(setupStage, *spawnedCallee);
              function.instructions.push_back(
                  {IrOpcode::TaskSpawn, (uint64_t(1) << 32) | reservation});
              LocalInfo handleInfo;
              handleInfo.kind = LocalInfo::Kind::Value;
              handleInfo.isTaskHandle = true;
              handleInfo.valueKind = taskResultType == "void"
                                         ? LocalInfo::ValueKind::Unknown
                                         : valueKindFromTypeName(taskResultType);
              handleInfo.index = nextLocal++;
              function.instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(handleInfo.index)});
              localsIn.emplace(stmt.name, handleInfo);
              return true;
            }
            // Structs cross in a heap block (LowerSetupStageState::TaskSpawnSite): every argument is evaluated here
            // first, a struct into a hidden binding of this scope that owns the copy (or the moved or temporary
            // value), whose slots the block then aliases for the task to read.
            LowerSetupStageState::TaskSpawnSite site;
            const int32_t siteIndex = static_cast<int32_t>(setupStage.taskSpawnSites.size());
            StructSlotLayout resultLayout;
            if (!taskShape.resultStructPath.empty()) {
              if (!resolveStructSlotLayout(taskShape.resultStructPath, resultLayout) ||
                  resultLayout.totalSlots <= 0) {
                error = "internal error: no struct layout for the result of spawned task " +
                        spawnedCallee->fullPath;
                return false;
              }
              site.resultSlots = resultLayout.totalSlots;
            }
            struct TaskArgument {
              int32_t local = -1;
              int32_t slots = 0;
              int32_t blockOffset = 0;
            };
            std::vector<TaskArgument> taskArguments(taskArgs.size());
            int32_t blockSlots = site.resultSlots;
            for (size_t argIndex = 0; argIndex < taskArgs.size(); ++argIndex) {
              const ir_lowerer::TaskSpawnParameterShape &parameter = taskShape.parameters[argIndex];
              TaskArgument &argument = taskArguments[argIndex];
              if (parameter.kind == ir_lowerer::TaskSpawnParameterShape::Kind::Scalar) {
                if (!emitExpr(*taskArgs[argIndex], localsIn)) {
                  return false;
                }
                argument.local = allocTempLocal();
                function.instructions.push_back(
                    {IrOpcode::StoreLocal, static_cast<uint64_t>(argument.local)});
                continue;
              }
              StructSlotLayout argumentLayout;
              if (!resolveStructSlotLayout(parameter.structPath, argumentLayout) ||
                  argumentLayout.totalSlots <= 0) {
                error = "internal error: no struct layout for an argument of spawned task " +
                        spawnedCallee->fullPath;
                return false;
              }
              Expr argumentBinding = stmt;
              argumentBinding.name =
                  "__task_argument_" + std::to_string(siteIndex) + "_" + std::to_string(argIndex);
              argumentBinding.transforms.clear();
              Transform argumentType;
              argumentType.name = parameter.structPath;
              argumentBinding.transforms.push_back(std::move(argumentType));
              Transform argumentMut;
              argumentMut.name = "mut";
              argumentBinding.transforms.push_back(std::move(argumentMut));
              argumentBinding.args.front() = *taskArgs[argIndex];
              argumentBinding.semanticNodeId = 0;
              if (!emitStatement(argumentBinding, localsIn)) {
                return false;
              }
              const auto argumentIt = localsIn.find(argumentBinding.name);
              if (argumentIt == localsIn.end() || argumentIt->second.structTypeName.empty()) {
                error = "internal error: struct argument of spawned task " +
                        spawnedCallee->fullPath + " is not a struct local";
                return false;
              }
              argument.local = argumentIt->second.index;
              argument.slots = argumentLayout.totalSlots;
              argument.blockOffset = blockSlots;
              blockSlots += argument.slots;
              if (argumentIt->second.dropFlagLocal >= 0) {
                // While the task may read the value its owner must not destroy it; an owned (`copy` or `move`)
                // parameter is the task's to destroy.
                function.instructions.push_back({IrOpcode::PushI32, 0});
                function.instructions.push_back(
                    {IrOpcode::StoreLocal,
                     static_cast<uint64_t>(argumentIt->second.dropFlagLocal)});
                if (parameter.kind == ir_lowerer::TaskSpawnParameterShape::Kind::Struct) {
                  site.lentArgumentDropFlags.push_back(argumentIt->second.dropFlagLocal);
                }
              }
            }
            site.blockLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::PushI32, static_cast<uint64_t>(blockSlots)});
            function.instructions.push_back({IrOpcode::HeapAlloc, 0});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(site.blockLocal)});
            const auto emitBlockAddress = [&](int32_t slotOffset) {
              function.instructions.push_back(
                  {IrOpcode::LoadLocal, static_cast<uint64_t>(site.blockLocal)});
              if (slotOffset != 0) {
                function.instructions.push_back(
                    {IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
                function.instructions.push_back({IrOpcode::AddI64, 0});
              }
            };
            for (size_t argIndex = 0; argIndex < taskArguments.size(); ++argIndex) {
              const TaskArgument &argument = taskArguments[argIndex];
              if (argument.slots == 0) {
                continue;
              }
              const int32_t destPtrLocal = allocTempLocal();
              emitBlockAddress(argument.blockOffset);
              function.instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(destPtrLocal)});
              if (!ir_lowerer::emitStructCopyFromPtrs(
                      function.instructions, destPtrLocal, argument.local, argument.slots, [&]() {
                        return allocTempLocal();
                      })) {
                return false;
              }
            }
            for (const TaskArgument &argument : taskArguments) {
              if (argument.slots == 0) {
                function.instructions.push_back(
                    {IrOpcode::LoadLocal, static_cast<uint64_t>(argument.local)});
              } else {
                emitBlockAddress(argument.blockOffset);
              }
            }
            if (site.resultSlots > 0) {
              emitBlockAddress(0);
            }
            const uint64_t reservation =
                ir_lowerer::reserveTaskSpawnTarget(setupStage, *spawnedCallee);
            function.instructions.push_back(
                {IrOpcode::TaskSpawn, (uint64_t(1) << 32) | reservation});
            site.handleLocal = nextLocal++;
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(site.handleLocal)});
            LocalInfo taskInfo;
            taskInfo.kind = LocalInfo::Kind::Value;
            taskInfo.taskSpawnSite = siteIndex;
            if (site.resultSlots == 0) {
              taskInfo.isTaskHandle = true;
              taskInfo.valueKind = taskResultType == "void" ? LocalInfo::ValueKind::Unknown
                                                            : valueKindFromTypeName(taskResultType);
              taskInfo.index = site.handleLocal;
            } else {
              // The binding is the struct local the wait fills: a block of this frame, owned (and destroyed with
              // this scope) once the result is in it.
              const int32_t resultBase = nextLocal;
              nextLocal += site.resultSlots;
              taskInfo.index = nextLocal++;
              function.instructions.push_back(
                  {IrOpcode::AddressOfLocal, static_cast<uint64_t>(resultBase)});
              function.instructions.push_back(
                  {IrOpcode::StoreLocal, static_cast<uint64_t>(taskInfo.index)});
              taskInfo.structTypeName = taskShape.resultStructPath;
              taskInfo.structSlotCount = site.resultSlots;
              if (!fileScopeStack.empty() &&
                  ir_lowerer::structNeedsDestroyHelpers(
                      taskInfo.structTypeName,
                      [&](const std::string &path) {
                        return ir_lowerer::findStackDestroyHelper(defMap, path);
                      },
                      [&](const std::string &path, StructSlotLayoutInfo &layoutOut) {
                        return resolveStructSlotLayout(path, layoutOut);
                      })) {
                taskInfo.dropFlagLocal = allocTempLocal();
                function.instructions.push_back({IrOpcode::PushI32, 0});
                function.instructions.push_back(
                    {IrOpcode::StoreLocal, static_cast<uint64_t>(taskInfo.dropFlagLocal)});
                setupStage.dropEntries.push_back(
                    {taskInfo.index, taskInfo.dropFlagLocal, taskInfo.structTypeName});
                fileScopeStack.back().push_back(
                    -static_cast<int32_t>(setupStage.dropEntries.size()));
              }
            }
            setupStage.taskSpawnSites.push_back(std::move(site));
            localsIn.emplace(stmt.name, taskInfo);
            return true;
          }
          if (setupStage.loweringNotes != nullptr) {
            setupStage.loweringNotes->push_back(
                "spawn of " +
                (spawnedCallee != nullptr ? spawnedCallee->fullPath : spawnedTaskCall.name) +
                " runs inline: " + inlineReason);
          }
        }
        if (taskResultType == "void") {
          // Run inline, a `Task<void>` spawn is its call as a statement; the binding holds 0, which its wait
          // statement reads and drops.
          if (!emitStatement(spawnedTaskCall, localsIn)) {
            return false;
          }
          LocalInfo doneInfo;
          doneInfo.kind = LocalInfo::Kind::Value;
          doneInfo.index = nextLocal++;
          function.instructions.push_back({IrOpcode::PushI32, 0});
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(doneInfo.index)});
          localsIn.emplace(stmt.name, doneInfo);
          return true;
        }
        Expr loweredTaskResultStmt = stmt;
        loweredTaskResultStmt.transforms = bindingTypeExpr->transforms;
        loweredTaskResultStmt.args.front() = std::move(spawnedTaskCall);
        loweredTaskResultStmt.semanticNodeId = 0;
        if (!replaceTaskTransformWithResultType(loweredTaskResultStmt,
                                                taskResultType)) {
          error = "task binding missing result type on " + stmt.name;
          return false;
        }
        return emitStatement(loweredTaskResultStmt, localsIn);
      }
      const StatementBindingTypeInfo bindingTypeInfo = inferStatementBindingTypeInfo(
          *bindingTypeExpr,
          init,
          localsIn,
          hasExplicitBindingTypeTransform,
          bindingKind,
          bindingValueKind,
          inferExprKind,
          [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
          callResolutionAdapters.semanticProgram,
          &callResolutionAdapters.semanticProductTargets.semanticIndex);
      LocalInfo::Kind kind = bindingTypeInfo.kind;
      LocalInfo::ValueKind valueKind = bindingTypeInfo.valueKind;
      LocalInfo::ValueKind keyValueKeyKind = bindingTypeInfo.keyValueKeyKind;
      LocalInfo::ValueKind keyValueValueKind = bindingTypeInfo.keyValueValueKind;
      std::string structTypeName = bindingTypeInfo.structTypeName;
      LocalInfo info;
      auto extractDeclaredResultValueType = [&](const std::string &typeText, std::string &valueTypeOut) {
        valueTypeOut.clear();
        std::string base;
        std::string argList;
        std::vector<std::string> args;
        if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argList) ||
            normalizeCollectionBindingTypeName(base) != "Result" ||
            !splitTemplateArgs(argList, args) || args.size() != 2) {
          return false;
        }
        valueTypeOut = trimTemplateTypeText(args.front());
        return true;
      };
      auto assignDeclaredResultStructType = [&](const std::string &typeText) {
        if (!info.resultHasValue || info.resultValueCollectionKind != LocalInfo::Kind::Value) {
          return;
        }
        std::string structPath;
        const std::string normalizedTypeText = trimTemplateTypeText(typeText);
        if (resolveStructTypeName(normalizedTypeText, stmt.namespacePrefix, structPath)) {
          info.resultValueStructType = std::move(structPath);
          info.resultValueKind = LocalInfo::ValueKind::Unknown;
        } else if (normalizedTypeText == "ContainerError" ||
                   normalizedTypeText == primec::collection_helpers::kCanonicalContainerErrorType) {
          info.resultValueStructType = primec::collection_helpers::kCanonicalContainerErrorType;
          info.resultValueKind = LocalInfo::ValueKind::Unknown;
        } else if (normalizedTypeText == "ImageError" ||
                   normalizedTypeText == "/std/image/ImageError") {
          info.resultValueStructType = "/std/image/ImageError";
          info.resultValueKind = LocalInfo::ValueKind::Unknown;
        } else if (normalizedTypeText == "GfxError" ||
                   normalizedTypeText == "/std/gfx/GfxError" ||
                   normalizedTypeText == "/std/gfx/experimental/GfxError") {
          info.resultValueStructType = normalizedTypeText == "/std/gfx/experimental/GfxError"
                                           ? "/std/gfx/experimental/GfxError"
                                           : "/std/gfx/GfxError";
          info.resultValueKind = LocalInfo::ValueKind::Unknown;
        }
      };
      auto assignDeclaredResultFileHandle = [&](const std::string &typeText) {
        std::string base;
        std::string arg;
        info.resultValueIsFileHandle =
            info.resultHasValue && splitTemplateTypeName(trimTemplateTypeText(typeText), base, arg) &&
            normalizeCollectionBindingTypeName(base) == "File";
      };
      auto assignDeclaredResultCollection = [&](const std::string &typeText) {
        info.resultValueCollectionKind = LocalInfo::Kind::Value;
        info.resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
        if (!info.resultHasValue) {
          return;
        }
        resolveSupportedResultCollectionType(
            typeText, info.resultValueCollectionKind, info.resultValueKind, &info.resultValueMapKeyKind);
      };
      info.isMutable = isBindingMutable(stmt);
      info.kind = kind;
      info.valueKind = valueKind;
      info.keyValueKeyKind = keyValueKeyKind;
      info.keyValueValueKind = keyValueValueKind;
      info.structTypeName = structTypeName;
      info.referenceToArray = bindingTypeInfo.referenceToArray;
      info.pointerToArray = bindingTypeInfo.pointerToArray;
      info.referenceToVector = bindingTypeInfo.referenceToVector;
      info.pointerToVector = bindingTypeInfo.pointerToVector;
      info.referenceToBuffer = bindingTypeInfo.referenceToBuffer;
      info.pointerToBuffer = bindingTypeInfo.pointerToBuffer;
      info.isSoaVector = bindingTypeInfo.isSoaVector;
      info.usesBuiltinCollectionLayout = bindingTypeInfo.usesBuiltinCollectionLayout;
      const bool semanticLocalAutoBinding = bindingTypeExpr != &stmt;
      const Expr &bindingTypeExprRef = *bindingTypeExpr;
#include "IrLowererLowerStatementsBindingLocalInfo.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"
      auto applyWrappedStdlibResultSumBindingInfo = [&]() {
        if (info.kind != LocalInfo::Kind::Reference &&
            info.kind != LocalInfo::Kind::Pointer) {
          return;
        }
        for (const auto &transform : bindingTypeExprRef.transforms) {
          if ((info.kind == LocalInfo::Kind::Reference &&
               transform.name != "Reference") ||
              (info.kind == LocalInfo::Kind::Pointer &&
               transform.name != "Pointer") ||
              transform.templateArgs.size() != 1) {
            continue;
          }
          const std::string targetType =
              trimTemplateTypeText(transform.templateArgs.front());
          if (const Definition *sumDef =
                  sumHelpers.resolveSumDefinitionForTypeText(targetType,
                                                  stmt.namespacePrefix);
              sumDef != nullptr && sumHelpers.isStdlibResultSumDefinition(*sumDef)) {
            sumHelpers.applyStdlibResultSumInfoToLocal(*sumDef, info);
          }
          return;
        }
      };
      applyWrappedStdlibResultSumBindingInfo();
      auto resolveBindingSumDefinition = [&]() -> const Definition * {
        if (info.kind != LocalInfo::Kind::Value) {
          return nullptr;
        }
        if (const Definition *sumDef =
                sumHelpers.resolveSumDefinitionForTypeText(info.structTypeName, stmt.namespacePrefix)) {
          return sumDef;
        }
        auto resolveSemanticSumTypeText =
            [&](const std::string &typeText, SymbolId typeTextId) -> const Definition * {
          const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
          return sumHelpers.resolveSumDefinitionForTypeText(
              resolveSemanticProductTypeText(semanticTargets.semanticProgram,
                                             typeText,
                                             typeTextId),
              stmt.namespacePrefix);
        };
        for (const auto &transform : bindingTypeExprRef.transforms) {
          if (transform.name == "effects" || transform.name == "capabilities" ||
              isBindingQualifierName(transform.name) || !transform.arguments.empty()) {
            continue;
          }
          std::string typeText = trimTemplateTypeText(transform.name);
          if (!transform.templateArgs.empty()) {
            typeText += "<" + joinTemplateArgsText(transform.templateArgs) + ">";
          }
          if (const Definition *sumDef =
                  sumHelpers.resolveSumDefinitionForTypeText(typeText, stmt.namespacePrefix)) {
            return sumDef;
          }
          break;
        }
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (semanticTargets.hasSemanticProduct) {
          const SemanticProgramBindingFact *bindingFact = nullptr;
          if (stmt.semanticNodeId != 0) {
            bindingFact = findSemanticProductBindingFact(semanticTargets, stmt);
          }
          if (bindingFact != nullptr) {
            if (const Definition *sumDef = resolveSemanticSumTypeText(
                    bindingFact->bindingTypeText,
                    bindingFact->bindingTypeTextId)) {
              return sumDef;
            }
          }
          if (const SemanticProgramBindingFact *initBindingFact =
                  findSemanticProductBindingFact(semanticTargets, init);
              initBindingFact != nullptr) {
            if (const Definition *sumDef = resolveSemanticSumTypeText(
                    initBindingFact->bindingTypeText,
                    initBindingFact->bindingTypeTextId)) {
              return sumDef;
            }
          }
          if (const SemanticProgramQueryFact *initQueryFact =
                  findSemanticProductQueryFact(semanticTargets, init);
              initQueryFact != nullptr) {
            if (const Definition *sumDef = resolveSemanticSumTypeText(
                    initQueryFact->bindingTypeText,
                    initQueryFact->bindingTypeTextId)) {
              return sumDef;
            }
            if (const Definition *sumDef = resolveSemanticSumTypeText(
                    initQueryFact->queryTypeText,
                    initQueryFact->queryTypeTextId)) {
              return sumDef;
            }
          }
        }
        return nullptr;
      };
      if (const Definition *sumDef = resolveBindingSumDefinition()) {
        int32_t totalSlots = 0;
        if (!sumHelpers.loweredSumSlotCount(*sumDef, totalSlots)) {
          if (!error.empty()) {
            return false;
          }
          LoweredSumVariantSelection selection;
          const bool selectedForDiagnostic =
              sumHelpers.selectSumVariantForInitializer(init, *sumDef, localsIn, selection);
          if (!selectedForDiagnostic && !error.empty()) {
            return false;
          }
          if (selectedForDiagnostic && selection.variant != nullptr) {
            error = sumHelpers.unsupportedSumPayloadError(*sumDef, *selection.variant);
          } else if (const SumVariant *unsupportedVariant = sumHelpers.firstUnsupportedSumPayloadVariant(*sumDef);
                     unsupportedVariant != nullptr) {
            error = sumHelpers.unsupportedSumPayloadError(*sumDef, *unsupportedVariant);
          } else {
            error = "native backend does not support sum payload type on " + sumDef->fullPath;
          }
          return false;
        }
        const int32_t baseLocal = nextLocal;
        nextLocal += totalSlots;
        info.kind = LocalInfo::Kind::Value;
        info.valueKind = LocalInfo::ValueKind::Int64;
        info.structTypeName = sumDef->fullPath;
        info.structSlotCount = totalSlots;
        sumHelpers.applyStdlibResultSumInfoToLocal(*sumDef, info);
        info.index = nextLocal++;
        sumHelpers.emitLoweredSumHeader(baseLocal, totalSlots);
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        bool emittedSumMove = false;
        if (!sumHelpers.tryEmitLoweredSumMoveIntoLocal(baseLocal, *sumDef, init, localsIn, emittedSumMove)) {
          return false;
        }
        if (!emittedSumMove && !sumHelpers.emitLoweredSumConstructionIntoLocal(baseLocal, *sumDef, init, localsIn)) {
          return false;
        }
        // A sum local built from a fresh value owns its payload and destroys it at scope end. A `mut` one
        // built from a place only stands for that place's value until an assignment gives it one of its own.
        const bool ownsFreshSum = !emittedSumMove && init.kind == Expr::Kind::Call &&
                                  !init.isFieldAccess &&
                                  isTemporaryStructCopySource(init, localsIn);
        // `move(other)` hands over what `other` owned.
        int32_t movedFromDropFlagLocal = -1;
        if (emittedSumMove) {
          if (const auto movedIt = localsIn.find(init.args.front().name);
              movedIt != localsIn.end()) {
            movedFromDropFlagLocal = movedIt->second.dropFlagLocal;
          }
        }
        if (!fileScopeStack.empty() &&
            (ownsFreshSum || info.isMutable || movedFromDropFlagLocal >= 0) &&
            sumHelpers.sumPayloadsNeedDestroy(*sumDef)) {
          info.dropFlagLocal = allocTempLocal();
          if (movedFromDropFlagLocal >= 0) {
            function.instructions.push_back(
                {IrOpcode::LoadLocal, static_cast<uint64_t>(movedFromDropFlagLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(movedFromDropFlagLocal)});
          } else {
            function.instructions.push_back({IrOpcode::PushI32, ownsFreshSum ? 1u : 0u});
          }
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(info.dropFlagLocal)});
          LowerSetupStageState::DropEntry entry{info.index, info.dropFlagLocal, sumDef->fullPath};
          entry.sumDef = sumDef;
          setupStage.dropEntries.push_back(std::move(entry));
          fileScopeStack.back().push_back(-static_cast<int32_t>(setupStage.dropEntries.size()));
        }
        localsIn.emplace(stmt.name, info);
        return true;
      }
      if (info.kind == LocalInfo::Kind::Value &&
          info.valueKind == LocalInfo::ValueKind::Unknown &&
          info.structTypeName.empty()) {
        if (init.kind == Expr::Kind::Call) {
          if (const Definition *initCallee = resolveDefinitionCall(init);
              initCallee != nullptr && ir_lowerer::isStructDefinition(*initCallee)) {
            info.structTypeName = initCallee->fullPath;
            info.valueKind = LocalInfo::ValueKind::Int64;
          }
        }
        if (info.structTypeName.empty()) {
          for (const auto &transform : bindingTypeExprRef.transforms) {
            if (transform.name == "effects" || transform.name == "capabilities" ||
                isBindingQualifierName(transform.name) || !transform.arguments.empty()) {
              continue;
            }
            std::string resolvedStructPath;
            if (resolveStructTypeName(transform.name, stmt.namespacePrefix, resolvedStructPath)) {
              info.structTypeName = std::move(resolvedStructPath);
            } else if (transform.name == "ImageError" || transform.name == "/std/image/ImageError") {
              info.structTypeName = "/std/image/ImageError";
            } else if (transform.name == "ContainerError" ||
                       transform.name == primec::collection_helpers::kCanonicalContainerErrorType) {
              info.structTypeName = primec::collection_helpers::kCanonicalContainerErrorType;
            } else if (transform.name == "GfxError" ||
                       transform.name == "/std/gfx/GfxError" ||
                       transform.name == "/std/gfx/experimental/GfxError") {
              info.structTypeName =
                  transform.name == "/std/gfx/experimental/GfxError" ? "/std/gfx/experimental/GfxError"
                                                                      : "/std/gfx/GfxError";
            }
            break;
          }
          if (!info.structTypeName.empty()) {
            info.valueKind = LocalInfo::ValueKind::Int64;
          }
        }
      }
      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        std::string initStruct = inferStructExprPath(init, localsIn);
        const Definition *initCallee = nullptr;
        if (init.kind == Expr::Kind::Call) {
          initCallee = resolveDefinitionCall(init);
        }
        auto adoptStructInitializerCallee = [&](const Definition &callee) {
          initStruct = callee.fullPath;
          if (!info.structTypeName.empty() && info.structTypeName.front() != '/') {
            const std::string declaredStructSurface =
                trimTemplateTypeText(info.structTypeName);
            const size_t calleeLeafOffset = callee.fullPath.find_last_of('/');
            const std::string calleeStructSurface =
                calleeLeafOffset == std::string::npos
                    ? callee.fullPath
                    : callee.fullPath.substr(calleeLeafOffset + 1);
