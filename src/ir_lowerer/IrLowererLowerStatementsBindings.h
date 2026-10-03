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
