  emitStatement = [&](const Expr &stmtInput, LocalMap &localsIn) -> bool {
    Expr normalizedStmt = stmtInput;
    auto resolveDirectMapHelperPath = [&](const Expr &exprIn) {
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
    auto findDirectMapHelperDefinition = [&](const std::string &rawPath) -> const Definition * {
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
    std::function<void(Expr &)> canonicalizeExplicitBuiltinMapHelpers =
        [&](Expr &exprIn) {
          for (auto &argExpr : exprIn.args) {
            canonicalizeExplicitBuiltinMapHelpers(argExpr);
          }
          for (auto &bodyExpr : exprIn.bodyArguments) {
            canonicalizeExplicitBuiltinMapHelpers(bodyExpr);
          }
          if (exprIn.kind != Expr::Kind::Call || exprIn.isMethodCall || exprIn.args.empty()) {
            return;
          }
          std::string helperName;
          if (!resolveMapHelperAliasName(exprIn, helperName) ||
              (helperName != "count" && helperName != "contains" &&
               helperName != "tryAt")) {
            return;
          }
          if (exprIn.name.find('/') == std::string::npos &&
              exprIn.namespacePrefix.empty() &&
              exprIn.templateArgs.empty()) {
            return;
          }
          const std::string rawPath = resolveDirectMapHelperPath(exprIn);
          if ((rawPath.rfind("/map/", 0) == 0 ||
               rawPath.rfind("/std/collections/map/", 0) == 0) &&
              findDirectMapHelperDefinition(rawPath) != nullptr) {
            return;
          }
          exprIn.name = helperName;
          exprIn.namespacePrefix.clear();
          exprIn.templateArgs.clear();
        };
    canonicalizeExplicitBuiltinMapHelpers(normalizedStmt);
    const Expr &stmt = normalizedStmt;
    auto rewriteBuiltinMapConstructorExpr = [&](const Expr &callExpr,
                                                const std::vector<std::string> &declaredTemplateArgs,
                                                LocalInfo::ValueKind fallbackKeyKind,
                                                LocalInfo::ValueKind fallbackValueKind,
                                                Expr &rewrittenExpr) {
      if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
        return false;
      }
      const Definition *callee = resolveDefinitionCall(callExpr);
      if (callee == nullptr ||
          !isResolvedCanonicalPublishedStdlibSurfaceConstructorPath(
              callee->fullPath,
              primec::StdlibSurfaceId::CollectionsMapConstructors)) {
        return false;
      }
      rewrittenExpr = callExpr;
      rewrittenExpr.name = "/map/map";
      rewrittenExpr.namespacePrefix.clear();
      rewrittenExpr.isMethodCall = false;
      rewrittenExpr.semanticNodeId = 0;
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
        declaredReturnBase == "Reference" || declaredReturnBase == "Pointer";
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
        const std::string bindingTypeText =
            localAutoFact != nullptr ? trimTemplateTypeText(localAutoFact->bindingTypeText)
                                     : std::string{};
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
      LocalInfo::ValueKind mapKeyKind = bindingTypeInfo.mapKeyKind;
      LocalInfo::ValueKind mapValueKind = bindingTypeInfo.mapValueKind;
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
      info.mapKeyKind = mapKeyKind;
      info.mapValueKind = mapValueKind;
      info.structTypeName = structTypeName;
      info.referenceToArray = bindingTypeInfo.referenceToArray;
      info.pointerToArray = bindingTypeInfo.pointerToArray;
      info.referenceToVector = bindingTypeInfo.referenceToVector;
      info.pointerToVector = bindingTypeInfo.pointerToVector;
      info.referenceToBuffer = bindingTypeInfo.referenceToBuffer;
      info.pointerToBuffer = bindingTypeInfo.pointerToBuffer;
      info.referenceToMap = bindingTypeInfo.referenceToMap;
      info.pointerToMap = bindingTypeInfo.pointerToMap;
      info.isSoaVector = bindingTypeInfo.isSoaVector;
      info.usesBuiltinCollectionLayout = bindingTypeInfo.usesBuiltinCollectionLayout;
      const bool semanticLocalAutoBinding = bindingTypeExpr != &stmt;
      const Expr &bindingTypeExprRef = *bindingTypeExpr;
#include "IrLowererLowerStatementsBindingLocalInfo.h"
      auto resolveBindingSumDefinition = [&]() -> const Definition * {
        if (const Definition *sumDef =
                resolveSumDefinitionForTypeText(info.structTypeName, stmt.namespacePrefix)) {
          return sumDef;
        }
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
                  resolveSumDefinitionForTypeText(typeText, stmt.namespacePrefix)) {
            return sumDef;
          }
          break;
        }
        return nullptr;
      };
      if (const Definition *sumDef = resolveBindingSumDefinition()) {
        int32_t totalSlots = 0;
        if (!loweredSumSlotCount(*sumDef, totalSlots)) {
          LoweredSumVariantSelection selection;
          if (selectSumVariantForInitializer(init, *sumDef, localsIn, selection) &&
              selection.variant != nullptr) {
            error = unsupportedSumPayloadError(*sumDef, *selection.variant);
          } else if (const SumVariant *unsupportedVariant = firstUnsupportedSumPayloadVariant(*sumDef);
                     unsupportedVariant != nullptr) {
            error = unsupportedSumPayloadError(*sumDef, *unsupportedVariant);
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
        info.index = nextLocal++;
        emitLoweredSumHeader(baseLocal, totalSlots);
        function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
        function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(info.index)});
        bool emittedSumMove = false;
        if (!tryEmitLoweredSumMoveIntoLocal(baseLocal, *sumDef, init, localsIn, emittedSumMove)) {
          return false;
        }
        if (!emittedSumMove && !emitLoweredSumConstructionIntoLocal(baseLocal, *sumDef, init, localsIn)) {
          return false;
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
                       transform.name == "/std/collections/ContainerError") {
              info.structTypeName = "/std/collections/ContainerError";
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
      if ((info.kind == LocalInfo::Kind::Value || info.kind == LocalInfo::Kind::Map) &&
          !info.structTypeName.empty()) {
        std::string initStruct = inferStructExprPath(init, localsIn);
        const Definition *initCallee = nullptr;
        if (init.kind == Expr::Kind::Call) {
          initCallee = resolveDefinitionCall(init);
        }
        if (initCallee != nullptr) {
          if (ir_lowerer::isStructDefinition(*initCallee)) {
            initStruct = initCallee->fullPath;
            if (!info.structTypeName.empty() &&
                info.structTypeName.front() != '/') {
              const std::string declaredStructSurface =
                  trimTemplateTypeText(info.structTypeName);
              const size_t calleeLeafOffset =
                  initCallee->fullPath.find_last_of('/');
              const std::string calleeStructSurface =
                  calleeLeafOffset == std::string::npos
                      ? initCallee->fullPath
                      : initCallee->fullPath.substr(calleeLeafOffset + 1);
              if (declaredStructSurface == calleeStructSurface) {
                info.structTypeName = initCallee->fullPath;
              }
            }
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
        if (init.kind == Expr::Kind::Call &&
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
                  baseLocal)) {
            return false;
          }
          localsIn.emplace(stmt.name, info);
          return true;
        }
        int32_t srcPtrLocal = allocTempLocal();
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
        if (!emittedStructArgsPackAccessInit &&
            init.kind == Expr::Kind::Call &&
            !init.isMethodCall &&
            isSimpleCallName(init, "try") &&
            init.args.size() == 1 &&
            init.args.front().kind == Expr::Kind::Call &&
            (init.args.front().name == "map" ||
             init.args.front().name == "and_then" ||
             init.args.front().name == "map2")) {
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
          kind != LocalInfo::Kind::Array &&
          kind != LocalInfo::Kind::Vector &&
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
    const auto pickStatementResult = tryEmitPickStatement(stmt, localsIn);
    if (pickStatementResult == LoweredSumPickEmitResult::Error) {
      return false;
    }
    if (pickStatementResult == LoweredSumPickEmitResult::Emitted) {
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
          !declaredReturnIsPointerLikeHandle &&
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
