  emitInlineDefinitionCall = [&](const Expr &callExpr,
                                 const Definition &callee,
                                 const LocalMap &callerLocals,
                                 bool requireValue) -> bool {
    const auto isInternalSoaMetadataInlineHelper =
        [](std::string_view path, std::string_view fieldName) {
          if (path.rfind("/std/collections/internal_soa_storage/SoaColumn", 0) != 0 &&
              path.rfind("/std/collections/internal_soa_storage/SoaFieldView", 0) != 0) {
            return false;
          }
          std::string leaf(path.substr(path.find_last_of('/') == std::string_view::npos
                                           ? 0
                                           : path.find_last_of('/') + 1));
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == fieldName;
        };
    const auto isInternalSoaMetadataOwner =
        [](std::string_view path) {
          if (path.rfind("/std/collections/internal_soa_storage/SoaColumn", 0) != 0 &&
              path.rfind("/std/collections/internal_soa_storage/SoaFieldView", 0) != 0) {
            return false;
          }
          const size_t leafStart = path.find_last_of('/');
          std::string leaf(path.substr(leafStart == std::string_view::npos
                                           ? 0
                                           : leafStart + 1));
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == "SoaColumn" || leaf == "SoaFieldView";
        };
    const auto callLeafName = [](const Expr &expr) {
      std::string path = expr.name;
      if (path.find('/') == std::string::npos && !expr.namespacePrefix.empty()) {
        path = expr.namespacePrefix == "/" ? "/" + path
                                           : expr.namespacePrefix + "/" + path;
      }
      const size_t leafStart = path.find_last_of('/');
      std::string leaf =
          leafStart == std::string::npos ? path : path.substr(leafStart + 1);
      const size_t generatedSuffix = leaf.find("__");
      if (generatedSuffix != std::string::npos) {
        leaf.erase(generatedSuffix);
      }
      return leaf;
    };
    const auto isExperimentalVectorConstructor =
        [](std::string_view path) {
          constexpr std::string_view Prefix =
              "/std/collections/experimental_vector/";
          if (path.rfind(Prefix, 0) != 0) {
            return false;
          }
          std::string leaf(path.substr(Prefix.size()));
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          return leaf == "vector" || leaf == "vectorNew" ||
                 leaf == "vectorSingle" || leaf == "vectorPair" ||
                 leaf == "vectorTriple" ||
                 leaf == "vectorQuad" || leaf == "vectorQuint" ||
                 leaf == "vectorSext" || leaf == "vectorSept" ||
                 leaf == "vectorOct";
        };
    if (isExperimentalVectorConstructor(callee.fullPath)) {
      auto extractVectorParameterTypeName = [](const Expr &paramExpr) {
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
      auto extractVectorReturnTypeName = [](
          const Definition &definition) {
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
              normalizeCollectionBindingTypeName(base) != "vector") {
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
      Expr vectorLiteralExpr = callExpr;
      vectorLiteralExpr.name = "vector";
      vectorLiteralExpr.namespacePrefix.clear();
      vectorLiteralExpr.isMethodCall = false;
      vectorLiteralExpr.semanticNodeId = 0;
      if (vectorLiteralExpr.templateArgs.empty() && !callee.parameters.empty()) {
        const std::string elementType =
            extractVectorParameterTypeName(callee.parameters.front());
        if (!elementType.empty()) {
          vectorLiteralExpr.templateArgs = {elementType};
        }
      }
      if (vectorLiteralExpr.templateArgs.empty()) {
        const std::string elementType = extractVectorReturnTypeName(callee);
        if (!elementType.empty()) {
          vectorLiteralExpr.templateArgs = {elementType};
        }
      }
      std::string collectionName;
      if (getBuiltinCollectionName(vectorLiteralExpr, collectionName) &&
          collectionName == "vector") {
        if (vectorLiteralExpr.templateArgs.size() != 1 &&
            !vectorLiteralExpr.args.empty()) {
          error = "vector literal requires exactly one template argument";
          return false;
        }
        LocalInfo::ValueKind elemKind =
            vectorLiteralExpr.templateArgs.size() == 1
                ? valueKindFromTypeName(vectorLiteralExpr.templateArgs.front())
                : LocalInfo::ValueKind::Unknown;
        if (elemKind == LocalInfo::ValueKind::Unknown &&
            !vectorLiteralExpr.args.empty()) {
          elemKind = inferExprKind(vectorLiteralExpr.args.front(), callerLocals);
        }
        if (elemKind == LocalInfo::ValueKind::Unknown &&
            !vectorLiteralExpr.args.empty()) {
          error =
              "native backend only supports numeric/bool/string vector literals";
          return false;
        }
        if (vectorLiteralExpr.args.size() >
            static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
          error = "vector literal too large for native backend";
          return false;
        }
        const int32_t literalCount =
            static_cast<int32_t>(vectorLiteralExpr.args.size());
        StructSlotLayoutInfo vectorLayout;
        if (!resolveStructSlotLayout(
                "/std/collections/experimental_vector/Vector", vectorLayout)) {
          error =
              "native backend cannot resolve experimental vector record layout";
          return false;
        }
        VectorRecordFieldSlots vectorSlots;
        if (!resolveVectorRecordFieldSlotsFromLayout(
                vectorLayout, vectorSlots)) {
          error =
              "native backend cannot resolve experimental vector record fields";
          return false;
        }
        const int32_t baseLocal = nextLocal;
        nextLocal += vectorSlots.totalSlots;
        emitVectorRecordHeader(function.instructions,
                               baseLocal,
                               vectorSlots,
                               literalCount,
                               literalCount,
                               literalCount,
                               literalCount == 0,
                               literalCount != 0);

        for (size_t argIndex = 0; argIndex < vectorLiteralExpr.args.size();
             ++argIndex) {
          const Expr &argExpr = vectorLiteralExpr.args[argIndex];
          function.instructions.push_back(
              {IrOpcode::LoadLocal,
               static_cast<uint64_t>(baseLocal + vectorSlots.data)});
          const uint64_t offsetBytes =
              static_cast<uint64_t>(argIndex) * IrSlotBytes;
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
                  "native backend requires vector literal string elements to "
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
                  "native backend requires vector literal elements to be "
                  "numeric/bool values";
              return false;
            }
            if (argKind != elemKind) {
              error = "vector literal element type mismatch";
              return false;
            }
            if (!emitExpr(argExpr, callerLocals)) {
              return false;
            }
          }
          function.instructions.push_back({IrOpcode::StoreIndirect, 0});
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
    auto resolveVectorVoidInlineHelperBuiltinName =
        [](std::string path, std::string &helperNameOut) {
          helperNameOut.clear();
          if (!path.empty() && path.front() == '/') {
            path.erase(path.begin());
          }
          std::string leaf;
          for (std::string_view prefix :
               {std::string_view{"std/collections/vector/"},
                std::string_view{"std/collections/internal_vector/"},
                std::string_view{"std/collections/experimental_vector/"}}) {
            if (path.rfind(prefix, 0) != 0 ||
                path.find('/', prefix.size()) != std::string::npos) {
              continue;
            }
            leaf = path.substr(prefix.size());
            break;
          }
          if (leaf.empty()) {
            return false;
          }
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          if (leaf == "push" || leaf == "vectorPush") {
            helperNameOut = "push";
          } else if (leaf == "pop" || leaf == "vectorPop") {
            helperNameOut = "pop";
          } else if (leaf == "reserve" || leaf == "vectorReserve") {
            helperNameOut = "reserve";
          } else if (leaf == "clear" || leaf == "vectorClear") {
            helperNameOut = "clear";
          } else if (leaf == "remove_at" || leaf == "vectorRemoveAt") {
            helperNameOut = "remove_at";
          } else if (leaf == "remove_swap" || leaf == "vectorRemoveSwap") {
            helperNameOut = "remove_swap";
          }
          return !helperNameOut.empty();
        };
    std::string vectorVoidInlineHelper;
    if (requireValue &&
        resolveVectorVoidInlineHelperBuiltinName(callee.fullPath,
                                                 vectorVoidInlineHelper)) {
      Expr vectorStmt = callExpr;
      vectorStmt.name = vectorVoidInlineHelper;
      vectorStmt.namespacePrefix.clear();
      vectorStmt.isMethodCall = false;
      vectorStmt.templateArgs.clear();
      const auto vectorStatementResult = ir_lowerer::tryEmitVectorStatementHelper(
          vectorStmt,
          callerLocals,
          function.instructions,
          [&]() { return allocTempLocal(); },
          [&](const Expr &valueExpr, const LocalMap &valueLocals) {
            return inferExprKind(valueExpr, valueLocals);
          },
          [&](const Expr &valueExpr, const LocalMap &valueLocals) {
            return inferStructExprPath(valueExpr, valueLocals);
          },
          [&](const Expr &valueExpr, const LocalMap &valueLocals) {
            return emitExpr(valueExpr, valueLocals);
          },
          [&](const std::string &structPath) -> const Definition * {
            auto destroyIt = defMap.find(structPath + "/DestroyStack");
            if (destroyIt != defMap.end()) {
              return destroyIt->second;
            }
            destroyIt = defMap.find(structPath + "/Destroy");
            return destroyIt == defMap.end() ? nullptr : destroyIt->second;
          },
          [&](const std::string &structPath) -> const Definition * {
            auto moveIt = defMap.find(structPath + "/Move");
            if (moveIt != defMap.end()) {
              return moveIt->second;
            }
            moveIt = defMap.find(structPath + "/Copy");
            return moveIt == defMap.end() ? nullptr : moveIt->second;
          },
          [&](const Expr &nestedCallExpr,
              const Definition &nestedCallee,
              const LocalMap &nestedLocals,
              bool nestedRequireValue) {
            return emitInlineDefinitionCall(nestedCallExpr,
                                            nestedCallee,
                                            nestedLocals,
                                            nestedRequireValue);
          },
          [](const Expr &) { return false; },
          [&]() { emitVectorCapacityExceeded(); },
          [&]() { emitVectorPopOnEmpty(); },
          [&]() { emitVectorIndexOutOfBounds(); },
          [&]() { emitArrayIndexOutOfBounds(); },
          [&]() { emitVectorReserveNegative(); },
          [&]() { emitVectorReserveExceeded(); },
          error);
      if (vectorStatementResult == ir_lowerer::VectorStatementHelperEmitResult::Error) {
        return false;
      }
      if (vectorStatementResult == ir_lowerer::VectorStatementHelperEmitResult::Emitted) {
        function.instructions.push_back({IrOpcode::PushI32, 0});
        return true;
      }
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
      std::function<bool(const std::string &,
                         LocalInfo::ValueKind &,
                         LocalInfo::ValueKind &)> inferMapKindsFromTypeText;
      inferMapKindsFromTypeText = [&](const std::string &typeText,
                                      LocalInfo::ValueKind &keyKindOut,
                                      LocalInfo::ValueKind &valueKindOut) {
        keyKindOut = LocalInfo::ValueKind::Unknown;
        valueKindOut = LocalInfo::ValueKind::Unknown;

        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
          return false;
        }

        const std::string normalizedBase = trimTemplateTypeText(base);
        if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
          return inferMapKindsFromTypeText(argText, keyKindOut, valueKindOut);
        }
        const bool isMapBase =
            normalizedBase == "map" || normalizedBase == "/map" ||
            normalizedBase == "std/collections/map" ||
            normalizedBase == "/std/collections/map" ||
            normalizedBase == "Map" || normalizedBase == "/Map" ||
            normalizedBase == "std/collections/experimental_map/Map" ||
            normalizedBase == "/std/collections/experimental_map/Map";
        if (!isMapBase) {
          return false;
        }

        std::vector<std::string> mapArgs;
        if (!splitTemplateArgs(argText, mapArgs) || mapArgs.size() != 2) {
          return false;
        }
        keyKindOut = valueKindFromTypeName(trimTemplateTypeText(mapArgs.front()));
        valueKindOut = valueKindFromTypeName(trimTemplateTypeText(mapArgs.back()));
        return keyKindOut != LocalInfo::ValueKind::Unknown &&
               valueKindOut != LocalInfo::ValueKind::Unknown;
      };
      auto resolveSemanticTypeText = [&](SymbolId typeTextId,
                                         const std::string &typeText) {
        if (callResolutionAdapters.semanticProgram != nullptr &&
            typeTextId != InvalidSymbolId) {
          const std::string resolvedText =
              std::string(semanticProgramResolveCallTargetString(
                  *callResolutionAdapters.semanticProgram,
                  typeTextId));
          if (!resolvedText.empty()) {
            return trimTemplateTypeText(resolvedText);
          }
        }
        return trimTemplateTypeText(typeText);
      };
      auto tryApplySemanticMapTypeText = [&](SymbolId typeTextId,
                                             const std::string &typeText,
                                             LocalInfo &infoOut) {
        LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
        LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
        if (!inferMapKindsFromTypeText(resolveSemanticTypeText(typeTextId, typeText),
                                       keyKind,
                                       valueKind)) {
          return false;
        }
        infoOut.mapKeyKind = keyKind;
        infoOut.mapValueKind = valueKind;
        return true;
      };
      auto tryApplySemanticCollectionSpecialization = [&](const Expr &receiverExpr,
                                                         LocalInfo &infoOut) {
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (!semanticTargets.hasSemanticProduct || receiverExpr.semanticNodeId == 0) {
          return false;
        }
        const auto *collectionFact =
            findSemanticProductCollectionSpecialization(semanticTargets.semanticIndex,
                                                        receiverExpr);
        if (collectionFact == nullptr) {
          return false;
        }
        const std::string collectionFamily =
            resolveSemanticTypeText(collectionFact->collectionFamilyId,
                                    collectionFact->collectionFamily);
        if (collectionFamily != "map" && collectionFamily != "/map" &&
            collectionFamily != "std/collections/map" &&
            collectionFamily != "/std/collections/map") {
          return false;
        }
        const LocalInfo::ValueKind keyKind = valueKindFromTypeName(
            resolveSemanticTypeText(collectionFact->keyTypeTextId,
                                    collectionFact->keyTypeText));
        const LocalInfo::ValueKind valueKind = valueKindFromTypeName(
            resolveSemanticTypeText(collectionFact->valueTypeTextId,
                                    collectionFact->valueTypeText));
        if (keyKind == LocalInfo::ValueKind::Unknown ||
            valueKind == LocalInfo::ValueKind::Unknown) {
          return false;
        }
        infoOut.mapKeyKind = keyKind;
        infoOut.mapValueKind = valueKind;
        return true;
      };
      auto tryPopulateMapKindsFromSemanticReceiver = [&](const Expr &receiverExpr,
                                                        LocalInfo &infoOut) {
        const auto &semanticTargets = callResolutionAdapters.semanticProductTargets;
        if (!semanticTargets.hasSemanticProduct ||
            callResolutionAdapters.semanticProgram == nullptr ||
            receiverExpr.semanticNodeId == 0) {
          return false;
        }
        if (tryApplySemanticCollectionSpecialization(receiverExpr, infoOut)) {
          return true;
        }
        if (const auto *queryFact =
                findSemanticProductQueryFact(callResolutionAdapters.semanticProgram,
                                             semanticTargets.semanticIndex,
                                             receiverExpr);
            queryFact != nullptr) {
          if (tryApplySemanticMapTypeText(queryFact->bindingTypeTextId,
                                          queryFact->bindingTypeText,
                                          infoOut) ||
              tryApplySemanticMapTypeText(queryFact->queryTypeTextId,
                                          queryFact->queryTypeText,
                                          infoOut) ||
              tryApplySemanticMapTypeText(queryFact->receiverBindingTypeTextId,
                                          queryFact->receiverBindingTypeText,
                                          infoOut)) {
            return true;
          }
        }
        if (const auto *bindingFact =
                findSemanticProductBindingFact(semanticTargets.semanticIndex,
                                               receiverExpr);
            bindingFact != nullptr &&
            tryApplySemanticMapTypeText(bindingFact->bindingTypeTextId,
                                        bindingFact->bindingTypeText,
                                        infoOut)) {
          return true;
        }
        if (const auto *localAutoFact =
                findSemanticProductLocalAutoFact(callResolutionAdapters.semanticProgram,
                                                 semanticTargets.semanticIndex,
                                                 receiverExpr);
            localAutoFact != nullptr &&
            tryApplySemanticMapTypeText(localAutoFact->bindingTypeTextId,
                                        localAutoFact->bindingTypeText,
                                        infoOut)) {
          return true;
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
        tryPopulateMapKindsFromSemanticReceiver(*originalValuesArg, valuesIt->second);
        if (originalValuesArg->kind == Expr::Kind::Name) {
          auto callerValuesIt = callerLocals.find(originalValuesArg->name);
          if (callerValuesIt != callerLocals.end() &&
              valuesIt->second.mapKeyKind == LocalInfo::ValueKind::Unknown &&
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
