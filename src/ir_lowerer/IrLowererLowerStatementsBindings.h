  emitStatement = [&](const Expr &stmt, LocalMap &localsIn) -> bool {
    auto rewriteBuiltinMapConstructorExpr = [&](const Expr &callExpr,
                                                const std::vector<std::string> &declaredTemplateArgs,
                                                LocalInfo::ValueKind fallbackKeyKind,
                                                LocalInfo::ValueKind fallbackValueKind,
                                                Expr &rewrittenExpr) {
      if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
        return false;
      }
      const Definition *callee = resolveDefinitionCall(callExpr);
      if (callee == nullptr) {
        return false;
      }
      auto matchesExperimentalMapConstructor = [&](std::string_view basePath) {
        return callee->fullPath == basePath || callee->fullPath.rfind(std::string(basePath) + "__t", 0) == 0;
      };
      if (!(matchesExperimentalMapConstructor("/std/collections/experimental_map/mapNew") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapSingle") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapDouble") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapPair") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapTriple") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapQuad") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapQuint") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapSext") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapSept") ||
            matchesExperimentalMapConstructor("/std/collections/experimental_map/mapOct"))) {
        return false;
      }
      rewrittenExpr = callExpr;
      rewrittenExpr.name = "/map/map";
      rewrittenExpr.namespacePrefix.clear();
      rewrittenExpr.isMethodCall = false;
      if (rewrittenExpr.templateArgs.empty()) {
        if (declaredTemplateArgs.size() == 2) {
          rewrittenExpr.templateArgs = declaredTemplateArgs;
        } else if (callee->parameters.size() >= 2) {
          auto extractParameterTypeName = [&](const Expr &paramExpr) {
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
          const std::string keyTypeName = extractParameterTypeName(callee->parameters[0]);
          const std::string valueTypeName = extractParameterTypeName(callee->parameters[1]);
          if (!keyTypeName.empty() && !valueTypeName.empty()) {
            rewrittenExpr.templateArgs = {keyTypeName, valueTypeName};
          }
        } else if (fallbackKeyKind != LocalInfo::ValueKind::Unknown &&
                   fallbackValueKind != LocalInfo::ValueKind::Unknown) {
          rewrittenExpr.templateArgs = {
              typeNameForValueKind(fallbackKeyKind),
              typeNameForValueKind(fallbackValueKind),
          };
        }
      }
      return true;
    };
    auto extractBuiltinMapReturnTemplateArgs = [&]() {
      std::vector<std::string> templateArgs;
      const std::string &definitionPath =
          activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
      auto defIt = defMap.find(definitionPath);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        return templateArgs;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string argList;
        if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, argList)) {
          return std::vector<std::string>{};
        }
        if (normalizeCollectionBindingTypeName(base) != "map") {
          return std::vector<std::string>{};
        }
        if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 2) {
          return std::vector<std::string>{};
        }
        templateArgs[0] = trimTemplateTypeText(templateArgs[0]);
        templateArgs[1] = trimTemplateTypeText(templateArgs[1]);
        return templateArgs;
      }
      return templateArgs;
    };
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
    auto declaredReturnIsReferenceHandle = [&]() {
      const std::string &definitionPath =
          activeInlineContext != nullptr ? activeInlineContext->defPath : function.name;
      auto defIt = defMap.find(definitionPath);
      if (defIt == defMap.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, arg)) {
          return false;
        }
        return normalizeDeclaredCollectionTypeBase(base) == "Reference";
      }
      return false;
    }();
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
      if (extractUninitializedTemplateArg(stmt, uninitializedType)) {
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
        info.mapKeyKind = uninitInfo.mapKeyKind;
        info.mapValueKind = uninitInfo.mapValueKind;
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
      const StatementBindingTypeInfo bindingTypeInfo = inferStatementBindingTypeInfo(
          stmt, init, localsIn, hasExplicitBindingTypeTransform, bindingKind, bindingValueKind, inferExprKind);
      LocalInfo::Kind kind = bindingTypeInfo.kind;
      LocalInfo::ValueKind valueKind = bindingTypeInfo.valueKind;
      LocalInfo::ValueKind mapKeyKind = bindingTypeInfo.mapKeyKind;
      LocalInfo::ValueKind mapValueKind = bindingTypeInfo.mapValueKind;
      std::string structTypeName = bindingTypeInfo.structTypeName;
      LocalInfo info;
      auto assignDeclaredResultStructType = [&](const std::string &typeText) {
        if (!info.resultHasValue || info.resultValueKind != LocalInfo::ValueKind::Unknown) {
          return;
        }
        std::string structPath;
        if (resolveStructTypeName(trimTemplateTypeText(typeText), stmt.namespacePrefix, structPath)) {
          info.resultValueStructType = std::move(structPath);
        }
      };
      auto assignDeclaredResultFileHandle = [&](const std::string &typeText) {
        std::string base;
        std::string arg;
        info.resultValueIsFileHandle =
            info.resultHasValue && splitTemplateTypeName(trimTemplateTypeText(typeText), base, arg) &&
            normalizeCollectionBindingTypeName(base) == "File";
      };
      info.isMutable = isBindingMutable(stmt);
      info.kind = kind;
      info.valueKind = valueKind;
      info.mapKeyKind = mapKeyKind;
      info.mapValueKind = mapValueKind;
      info.structTypeName = structTypeName;
      if (!hasExplicitBindingTypeTransform(stmt) && info.kind == LocalInfo::Kind::Value) {
        ResultExprInfo inferredResultInfo;
        if (resolveResultExprInfoFromLocals(
                init,
                localsIn,
                [&](const Expr &callExpr, const LocalMap &callLocals) {
                  return resolveMethodCallDefinition(callExpr, callLocals);
                },
                [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
                [&](const std::string &path, ReturnInfo &infoOut) { return getReturnInfo(path, infoOut); },
                [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
                inferredResultInfo) &&
            inferredResultInfo.isResult) {
          info.isResult = true;
          info.resultHasValue = inferredResultInfo.hasValue;
          info.resultValueKind = inferredResultInfo.valueKind;
          info.resultValueIsFileHandle = inferredResultInfo.valueIsFileHandle;
          info.resultValueStructType = inferredResultInfo.valueStructType;
          info.resultErrorType = inferredResultInfo.errorType;
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
        }
      }
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "soa_vector") {
          info.isSoaVector = true;
          break;
        }
      }
      if (!info.isSoaVector && init.kind == Expr::Kind::Name) {
        auto existing = localsIn.find(init.name);
        if (existing != localsIn.end() && existing->second.isSoaVector) {
          info.isSoaVector = true;
        }
      }
      if (!info.isSoaVector && init.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(init, collection) && collection == "soa_vector") {
          info.isSoaVector = true;
        }
      }
      setReferenceArrayInfo(stmt, info);
      applyStructArrayInfo(stmt, info);
      applyStructValueInfo(stmt, info);
      for (const auto &transform : stmt.transforms) {
        if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1) {
          std::string targetType;
          if (ir_lowerer::extractTopLevelUninitializedTypeText(transform.templateArgs.front(), targetType)) {
            info.targetsUninitializedStorage = true;
            break;
          }
        }
      }
      if (!info.targetsUninitializedStorage && init.kind == Expr::Kind::Name) {
        auto existing = localsIn.find(init.name);
        if (existing != localsIn.end() &&
            (existing->second.kind == LocalInfo::Kind::Pointer ||
             existing->second.kind == LocalInfo::Kind::Reference)) {
          info.targetsUninitializedStorage = existing->second.targetsUninitializedStorage;
        }
      }
      if (!info.targetsUninitializedStorage && isPointerMemoryIntrinsicCall(init)) {
        info.targetsUninitializedStorage =
            inferPointerMemoryIntrinsicTargetsUninitializedStorage(init, localsIn);
      }
      if ((info.kind == LocalInfo::Kind::Pointer || info.kind == LocalInfo::Kind::Reference) &&
          info.structTypeName.empty()) {
        std::function<std::string(const Expr &)> inferPointerStructType = [&](const Expr &exprIn) -> std::string {
          if (exprIn.kind == Expr::Kind::Name) {
            auto localIt = localsIn.find(exprIn.name);
            if (localIt == localsIn.end()) {
              return "";
            }
            if (localIt->second.kind == LocalInfo::Kind::Pointer || localIt->second.kind == LocalInfo::Kind::Reference) {
              return localIt->second.structTypeName;
            }
            return "";
          }
          if (exprIn.kind != Expr::Kind::Call) {
            return "";
          }
          std::string memoryBuiltin;
          if (getBuiltinMemoryName(exprIn, memoryBuiltin)) {
            if (memoryBuiltin == "alloc" && exprIn.templateArgs.size() == 1) {
              const std::string targetType =
                  ir_lowerer::unwrapTopLevelUninitializedTypeText(exprIn.templateArgs.front());
              std::string resolvedStruct;
              if (resolveStructTypeName(targetType, exprIn.namespacePrefix, resolvedStruct)) {
                return resolvedStruct;
              }
              return "";
            }
            if (memoryBuiltin == "realloc" && exprIn.args.size() == 2) {
              return inferPointerStructType(exprIn.args.front());
            }
          }
          std::string builtinName;
          if (getBuiltinOperatorName(exprIn, builtinName) &&
              (builtinName == "plus" || builtinName == "minus") &&
              exprIn.args.size() == 2) {
            return inferPointerStructType(exprIn.args.front());
          }
          if (isSimpleCallName(exprIn, "location") && exprIn.args.size() == 1) {
            if (exprIn.args.front().kind != Expr::Kind::Name) {
              return "";
            }
            auto localIt = localsIn.find(exprIn.args.front().name);
            return localIt == localsIn.end() ? "" : localIt->second.structTypeName;
          }
          return "";
        };
        info.structTypeName = inferPointerStructType(init);
      }
      if (info.kind == LocalInfo::Kind::Value && info.structTypeName.empty()) {
        std::string inferredStruct = inferStructExprPath(init, localsIn);
        if (!inferredStruct.empty()) {
          info.structTypeName = inferredStruct;
        } else if (info.valueKind == LocalInfo::ValueKind::Unknown) {
          info.valueKind = LocalInfo::ValueKind::Int32;
        }
      }
      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        info.valueKind = LocalInfo::ValueKind::Int64;
      }
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "File") {
          info.isFileHandle = true;
          info.valueKind = LocalInfo::ValueKind::Int64;
        } else if (transform.name == "Result") {
          info.isResult = true;
          info.resultHasValue = (transform.templateArgs.size() == 2);
          info.resultValueKind =
              info.resultHasValue ? valueKindFromTypeName(transform.templateArgs.front())
                                  : LocalInfo::ValueKind::Unknown;
          if (info.resultHasValue && !transform.templateArgs.empty()) {
            assignDeclaredResultFileHandle(transform.templateArgs.front());
            if (info.resultValueIsFileHandle) {
              info.resultValueKind = LocalInfo::ValueKind::Int64;
            }
            assignDeclaredResultStructType(transform.templateArgs.front());
          }
          info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
          if (!transform.templateArgs.empty()) {
            info.resultErrorType = transform.templateArgs.back();
          }
        } else if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1) {
          const std::string originalTargetType = trimTemplateTypeText(transform.templateArgs.front());
          std::string targetType = originalTargetType;
          if (ir_lowerer::extractTopLevelUninitializedTypeText(originalTargetType, targetType)) {
            info.targetsUninitializedStorage = true;
          }
          std::string wrappedBase;
          std::string wrappedArg;
          if (splitTemplateTypeName(targetType, wrappedBase, wrappedArg) &&
              normalizeCollectionBindingTypeName(wrappedBase) == "File") {
            info.isFileHandle = true;
            info.valueKind = LocalInfo::ValueKind::Int64;
          }
          bool resultHasValue = false;
          LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
          std::string resultErrorType;
          if (parseResultTypeName(targetType, resultHasValue, resultValueKind, resultErrorType)) {
            info.isResult = true;
            info.resultHasValue = resultHasValue;
            info.resultValueKind = resultValueKind;
            if (info.resultHasValue) {
              assignDeclaredResultFileHandle(targetType);
              if (info.resultValueIsFileHandle) {
                info.resultValueKind = LocalInfo::ValueKind::Int64;
              }
              assignDeclaredResultStructType(targetType);
            }
            info.valueKind = info.resultHasValue ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
            info.resultErrorType = resultErrorType;
          }
        }
      }
      info.isFileError = isFileErrorBinding(stmt);
      valueKind = info.valueKind;

      if (info.kind == LocalInfo::Kind::Value && !info.structTypeName.empty()) {
        std::string initStruct = inferStructExprPath(init, localsIn);
        if (initStruct.empty() && init.kind == Expr::Kind::Call) {
          const Definition *initCallee = resolveDefinitionCall(init);
          if (initCallee != nullptr) {
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
        if (!initStruct.empty() && initStruct != info.structTypeName) {
          error = "struct binding initializer type mismatch on " + stmt.name;
          return false;
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
        if (init.kind == Expr::Kind::Call) {
          const Definition *initCallee = resolveDefinitionCall(init);
          if (initCallee != nullptr && isStructDefinition(*initCallee) && initCallee->fullPath == info.structTypeName) {
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
            if (packedArgs.empty()) {
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
                      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                        return inferExprKind(valueExpr, valueLocals);
                      },
                      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                        return inferStructExprPath(valueExpr, valueLocals);
                      },
                      [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                        return emitExpr(valueExpr, valueLocals);
                      },
                      [&](const Expr &fieldParam,
                          const LocalMap &fieldLocals,
                          LocalInfo &fieldInfoOut,
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
                                                                       fieldInfoOut,
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
                                                                       });
                      },
                      [&](int32_t destBaseLocal, int32_t srcPtrLocal, int32_t slotCount) {
                        return emitStructCopySlots(destBaseLocal, srcPtrLocal, slotCount);
                      },
                      [&]() { return allocTempLocal(); },
                      [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                      error,
                      baseLocal)) {
                return false;
              }
              localsIn.emplace(stmt.name, info);
              return true;
            }
          }
        }
        const int32_t srcPtrLocal = allocTempLocal();
        bool emittedStructArgsPackAccessInit = false;
        std::string accessName;
        if (init.kind == Expr::Kind::Call &&
            getBuiltinArrayAccessName(init, accessName) &&
            init.args.size() == 2) {
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(init.args.front(), localsIn);
          const bool isStructArgsPackAccess =
              targetInfo.isArgsPackTarget &&
              !targetInfo.isVectorTarget &&
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
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          return false;
        }
        if (init.kind == Expr::Kind::Call) {
          ir_lowerer::emitDisarmTemporaryStructAfterCopy(
              [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
              srcPtrLocal,
              structTypeName);
        }
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
          kind != LocalInfo::Kind::Map &&
          info.structTypeName.empty() &&
          !info.isSoaVector &&
          info.kind != LocalInfo::Kind::Pointer &&
          info.kind != LocalInfo::Kind::Reference) {
        error = "native backend requires typed bindings";
        return false;
      }
      Expr rewrittenMapInitExpr;
      const Expr *emittedInitExpr = &init;
      if (info.kind == LocalInfo::Kind::Map && init.kind == Expr::Kind::Call && !init.isMethodCall) {
        if (rewriteBuiltinMapConstructorExpr(
                init, {}, info.mapKeyKind, info.mapValueKind, rewrittenMapInitExpr)) {
          emittedInitExpr = &rewrittenMapInitExpr;
        }
      }
      if (!emitExpr(*emittedInitExpr, localsIn)) {
        return false;
      }
      info.index = nextLocal++;
      if (info.kind == LocalInfo::Kind::Reference) {
        if (init.kind != Expr::Kind::Call || !isSimpleCallName(init, "location") || init.args.size() != 1) {
          error = "reference binding requires location(...) initializer";
          return false;
        }
      }
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
          return resolveStructSlotLayout(structPath, layoutOut);
        },
        [&]() { return allocTempLocal(); },
        [&](int32_t destPtrLocal, int32_t srcPtrLocal, int32_t slotCount) {
          return emitStructCopyFromPtrs(destPtrLocal, srcPtrLocal, slotCount);
        },
        error);
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
        [&](const Expr &callExpr) { return resolveDefinitionCall(callExpr); },
        [&](const Expr &argExpr, const LocalMap &valueLocals) { return emitExpr(argExpr, valueLocals); },
        function.instructions,
        error);
    if (printPathSpaceResult == ir_lowerer::StatementPrintPathSpaceEmitResult::Error) {
      return false;
    }
    if (printPathSpaceResult == ir_lowerer::StatementPrintPathSpaceEmitResult::Emitted) {
      return true;
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
      Expr rewrittenReturnValueExpr;
      const std::vector<std::string> declaredMapTemplateArgs = extractBuiltinMapReturnTemplateArgs();
      LocalInfo::ValueKind returnMapKeyKind = LocalInfo::ValueKind::Unknown;
      LocalInfo::ValueKind returnMapValueKind = LocalInfo::ValueKind::Unknown;
      if (stmt.args.front().kind == Expr::Kind::Call && stmt.args.front().args.size() >= 2) {
        returnMapKeyKind = inferExprKind(stmt.args.front().args.front(), localsIn);
        returnMapValueKind = inferExprKind(stmt.args.front().args[1], localsIn);
      }
      if (rewriteBuiltinMapConstructorExpr(stmt.args.front(),
                                          declaredMapTemplateArgs,
                                          returnMapKeyKind,
                                          returnMapValueKind,
                                          rewrittenReturnValueExpr)) {
        rewrittenReturnStmt = stmt;
        rewrittenReturnStmt.args.front() = std::move(rewrittenReturnValueExpr);
        emittedReturnStmt = &rewrittenReturnStmt;
      }
      const Expr &returnValueExpr = emittedReturnStmt->args.front();
      StructSlotLayout layout;
      std::string aggregateStructPath;
      const std::string inferredStructPath = inferStructExprPath(returnValueExpr, localsIn);
      if (!inferredStructPath.empty() && resolveStructSlotLayout(inferredStructPath, layout)) {
        aggregateStructPath = inferredStructPath;
      }
      if (aggregateStructPath.empty()) {
        const std::string declaredStructPath = extractDeclaredStructReturnPath();
        if (!declaredStructPath.empty() && resolveStructSlotLayout(declaredStructPath, layout)) {
          aggregateStructPath = declaredStructPath;
        }
      }
      const bool shouldStabilizeAggregateReturn =
          !aggregateStructPath.empty() &&
          (returnValueExpr.kind == Expr::Kind::Call || returnValueExpr.kind == Expr::Kind::Name);
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
        if (!emitExpr(returnValueExpr, localsIn)) {
          return false;
        }
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
        if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
          return false;
        }
        ir_lowerer::emitDisarmTemporaryStructAfterCopy(
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            srcPtrLocal,
            aggregateStructPath);
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
        returnsVoid,
        sawReturn,
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferExprKind(valueExpr, valueLocals); },
        [&](const Expr &valueExpr, const LocalMap &valueLocals) { return inferArrayElementKind(valueExpr, valueLocals); },
        [&]() { emitFileScopeCleanupAll(); },
        error);
    if (returnResult == ir_lowerer::ReturnStatementEmitResult::Error) {
      return false;
    }
    if (returnResult == ir_lowerer::ReturnStatementEmitResult::Emitted) {
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
