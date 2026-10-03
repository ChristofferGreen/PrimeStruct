#include "primec/support/CollectionHelperNames.h"
        if (!expr.isMethodCall) {
          const std::string rawPath = statementsExprHelpers.resolveDirectHelperPath(expr);
          std::string experimentalVectorElementType;
          const bool isCollectionVectorConstructorAlias =
          getExperimentalVectorConstructorElementTypeAlias(
            expr, experimentalVectorElementType) ||
          getExperimentalVectorConstructorElementTypeAliasFromPath(
            resolveExprPath(expr), experimentalVectorElementType);
          if (isCollectionVectorConstructorAlias) {
            Expr rewrittenVectorCtor = expr;
            rewrittenVectorCtor.name =
            statementsExprHelpers.experimentalCollectionMemberPath("vector", "vector");
            rewrittenVectorCtor.namespacePrefix.clear();
            rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};
            if (const Definition *vectorCtor =
              statementsExprHelpers.resolveDirectHelperDefinition(rewrittenVectorCtor)) {
              if (!emitInlineDefinitionCall(
                  rewrittenVectorCtor, *vectorCtor, localsIn, true)) {
                return false;
              }
              return true;
            }
          }
          const Definition *directCallee = resolveDefinitionCall(expr);
          if (const std::string semanticResolvedPath =
            statementsExprHelpers.resolveSemanticCallTargetPath(expr);
            !semanticResolvedPath.empty() &&
            statementsExprHelpers.isSamePathSoaHelperPath(semanticResolvedPath) &&
            (directCallee == nullptr ||
              !statementsExprHelpers.isSamePathSoaHelperPath(directCallee->fullPath))) {
            if (const Definition *semanticSoaHelper =
              statementsExprHelpers.findDirectHelperDefinition(semanticResolvedPath)) {
              directCallee = semanticSoaHelper;
            }
          }
          if (directCallee != nullptr &&
            statementsExprHelpers.isSoaWrapperHelperFamilyPath(rawPath) &&
            !statementsExprHelpers.isSamePathSoaHelperPath(directCallee->fullPath)) {
            if (const Definition *preferredSoaWrapper =
              statementsExprHelpers.findDirectSoaWrapperDefinition(expr, rawPath, localsIn)) {
              directCallee = preferredSoaWrapper;
            }
          }
          if (directCallee == nullptr &&
            statementsExprHelpers.hasKeyValueEntryCtorArgs(expr) &&
            statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath)) {
            directCallee = statementsExprHelpers.findDirectEntryKeyValueConstructorDefinition(expr);
          }
          if (directCallee == nullptr &&
            statementsExprHelpers.isInternalSoaHelperFamilyPath(rawPath)) {
            directCallee = statementsExprHelpers.findDirectInternalSoaDefinition(rawPath);
          }
          if (directCallee == nullptr && !expr.isMethodCall) {
            directCallee = statementsExprHelpers.findDirectStructDefinition(expr);
          }
          if (directCallee == nullptr &&
            (statementsExprHelpers.isSoaWrapperHelperFamilyPath(rawPath) ||
              statementsExprHelpers.isSamePathSoaHelperPath(rawPath))) {
            directCallee = statementsExprHelpers.findDirectSoaWrapperDefinition(expr, rawPath, localsIn);
          }
          const std::string resolvedExprPath = resolveExprPath(expr);
          if (directCallee == nullptr && statementsExprHelpers.isDirectCollectionHelperPath(rawPath)) {
            directCallee = statementsExprHelpers.findDirectHelperDefinition(rawPath);
          }
          if (directCallee == nullptr &&
            statementsExprHelpers.isCanonicalKeyValueConstructorPath(resolvedExprPath)) {
            directCallee = statementsExprHelpers.findDirectHelperDefinition(resolvedExprPath);
          }
          if (directCallee == nullptr && statementsExprHelpers.isDirectCollectionHelperPath(resolvedExprPath)) {
            directCallee = statementsExprHelpers.findDirectHelperDefinition(resolvedExprPath);
          }
          if (directCallee == nullptr) {
            bool handledBuiltinKeyValueConstructor = false;
            if (!statementsExprHelpers.tryEmitBuiltinKeyValueConstructor(
                expr, resolvedExprPath, handledBuiltinKeyValueConstructor, localsIn)) {
              return false;
            }
            if (handledBuiltinKeyValueConstructor) {
              return true;
            }
          }
          auto findExperimentalVectorMetadataMethodDefinition =
          [&]() -> const Definition * {
            if (expr.args.empty() ||
              (!isSimpleCallName(expr, "set_field_count") &&
                !isSimpleCallName(expr, "set_field_capacity"))) {
              return nullptr;
            }
            const Expr &receiver = expr.args.front();
            if (receiver.kind != Expr::Kind::Name) {
              return nullptr;
            }
            auto localIt = localsIn.find(receiver.name);
            if (localIt == localsIn.end()) {
              return nullptr;
            }
            std::string receiverStructPath = localIt->second.structTypeName;
            if (receiverStructPath.empty()) {
              receiverStructPath = inferStructExprPath(receiver, localsIn);
            }
            std::vector<std::string> candidates;
            if (statementsExprHelpers.isCollectionVectorRecordTypePath(receiverStructPath)) {
              candidates.push_back(receiverStructPath + "/" + expr.name);
            }
            candidates.push_back(
              vectorBackingTypePath() + "/" +
              expr.name);
            for (const auto &candidate : candidates) {
              auto defIt = defMap.find(candidate);
              if (defIt != defMap.end() && defIt->second != nullptr) {
                return defIt->second;
              }
            }
            const std::string methodSuffix = "/" + expr.name;
            for (const auto &[candidatePath, candidateDef] : defMap) {
              if (candidateDef == nullptr ||
                !statementsExprHelpers.matchesGeneratedSpecializedType(
                  candidatePath, "vector", "Vector") ||
                !candidatePath.ends_with(methodSuffix)) {
                continue;
              }
              return candidateDef;
            }
            return nullptr;
          };
          if (const Definition *vectorMetadataMethod =
            findExperimentalVectorMetadataMethodDefinition()) {
            if (!emitInlineDefinitionCall(
                expr, *vectorMetadataMethod, localsIn, true)) {
              return false;
            }
            return true;
          }
          auto isInternalSoaMetadataReceiver = [&](const Expr &receiver) {
            std::string structPath = inferStructExprPath(receiver, localsIn);
            const size_t templateStart = structPath.find('<');
            if (templateStart != std::string::npos) {
              structPath.erase(templateStart);
            }
            const size_t leafStart = structPath.find_last_of('/');
            const size_t suffixStart =
            structPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
            if (suffixStart != std::string::npos) {
              structPath.erase(suffixStart);
            }
            if (!structPath.empty() && structPath.front() == '/') {
              structPath.erase(structPath.begin());
            }
            const std::string internalSoaPrefix =
            collection_paths::modulePrefixBare(collection_paths::kInternalSoaStorageFolder);
            if (structPath.rfind(internalSoaPrefix, 0) == 0) {
              structPath.erase(0, internalSoaPrefix.size());
            }
            return structPath == "SoaColumn" || structPath == "SoaFieldView";
          };
          const bool isInternalSoaMetadataMethod =
          expr.isMethodCall && expr.args.size() == 1 &&
          (isSimpleCallName(expr, "field_count") ||
            isSimpleCallName(expr, "field_capacity"));
          const bool hasInternalSoaMetadataCallee =
          directCallee != nullptr &&
          (directCallee->fullPath.rfind(
              collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) == 0 ||
            directCallee->fullPath.rfind(
              collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView"), 0) == 0);
          auto internalSoaMetadataHelperLeaf =
          [](const Definition &definition) -> std::string {
            if (definition.fullPath.rfind(
                collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) != 0 &&
              definition.fullPath.rfind(
                collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView"), 0) != 0) {
              return {};
            }
            const size_t leafStart = definition.fullPath.find_last_of('/');
            std::string leaf =
            leafStart == std::string::npos
            ? definition.fullPath
            : definition.fullPath.substr(leafStart + 1);
            const size_t generatedSuffix = leaf.find("__");
            if (generatedSuffix != std::string::npos) {
              leaf.erase(generatedSuffix);
            }
            if (leaf == "field_count" || leaf == "field_capacity") {
              return leaf;
            }
            return {};
          };
          if (directCallee != nullptr && expr.args.size() == 1) {
            const std::string metadataLeaf =
            internalSoaMetadataHelperLeaf(*directCallee);
            if (!metadataLeaf.empty() &&
              isInternalSoaMetadataReceiver(expr.args.front())) {
              if (!emitExpr(expr.args.front(), localsIn)) {
                return false;
              }
              const uint64_t slotOffset =
              metadataLeaf == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
              function.instructions.push_back({IrOpcode::PushI64, slotOffset});
              function.instructions.push_back({IrOpcode::AddI64, 0});
              function.instructions.push_back({IrOpcode::LoadIndirect, 0});
              return true;
            }
          }
          if (isInternalSoaMetadataMethod &&
            (isInternalSoaMetadataReceiver(expr.args.front()) ||
              hasInternalSoaMetadataCallee)) {
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            const uint64_t slotOffset =
            isSimpleCallName(expr, "field_count") ? 1ull : 2ull;
            function.instructions.push_back({IrOpcode::PushI64, slotOffset * IrSlotBytes});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
          if (directCallee != nullptr &&
            ir_lowerer::isStructDefinition(*directCallee) &&
            !ir_lowerer::isStructConstructorCallShape(expr)) {
            directCallee = nullptr;
          }
          if (directCallee != nullptr) {
            auto directCalleeFirstParameterIsStruct = [&]() {
              if (directCallee->parameters.empty()) {
                return false;
              }
              std::string typeName;
              std::vector<std::string> templateArgs;
              if (!extractFirstBindingTypeTransform(
                  directCallee->parameters.front(), typeName, templateArgs) ||
                !templateArgs.empty()) {
                return false;
              }
              std::string resolvedStructPath;
              return resolveStructTypeName(
                typeName, directCallee->namespacePrefix, resolvedStructPath);
            };
            auto isWrapperReturnedKeyValueAccessCall = [&](const Expr &candidate) {
              if (candidate.kind != Expr::Kind::Call ||
                candidate.args.size() < 2 ||
                candidate.args.front().kind != Expr::Kind::Call) {
                return false;
              }
              std::string helperName;
              if (resolveKeyValueHelperAliasName(candidate, helperName)) {
                return helperName == "at" || helperName == "at_unsafe" ||
                helperName == primec::collection_helpers::kAtRef ||
                helperName == primec::collection_helpers::kAtUnsafeRef;
              }
              auto isAccessHelperPath = [&](std::string path) {
                path = statementsExprHelpers.stripGeneratedHelperSuffix(
                  normalizeCollectionHelperPath(std::move(path)));
                return statementsExprHelpers.isKeyValueHelperMemberPath(path, "at") ||
                statementsExprHelpers.isKeyValueHelperMemberPath(path, "at_unsafe") ||
                path == "at" || path == "at_unsafe" ||
                path == primec::collection_helpers::kCanonicalMapAt ||
                path == primec::collection_helpers::kCanonicalMapAtUnsafe;
              };
              return isAccessHelperPath(candidate.name) ||
              isAccessHelperPath(statementsExprHelpers.resolveDirectHelperPath(candidate)) ||
              isAccessHelperPath(resolveExprPath(candidate));
            };
            if (!expr.args.empty() &&
              directCalleeFirstParameterIsStruct() &&
              isWrapperReturnedKeyValueAccessCall(expr.args.front())) {
              error = "struct parameter type mismatch";
              return false;
            }
            if (ir_lowerer::isStructDefinition(*directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (!isInternalSoaMetadataMethod &&
              directCallee->fullPath.rfind(collection_paths::modulePrefix(collection_paths::kInternalSoaStorageFolder), 0) == 0 &&
              statementsExprHelpers.isInternalSoaHelperFamilyPath(directCallee->fullPath)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            const bool isVisibleSamePathSoaHelper =
            statementsExprHelpers.isSamePathSoaHelperPath(rawPath) &&
            statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee);
            const bool isResolvedSoaWrapperHelper =
            statementsExprHelpers.isSoaWrapperHelperFamilyPath(directCallee->fullPath);
            if (isResolvedSoaWrapperHelper || isVisibleSamePathSoaHelper) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if ((primec::collection_helpers::isRootedArrayPath(rawPath) ||
                primec::collection_helpers::isRootedArrayPath(resolvedExprPath) ||
                primec::collection_helpers::isRootedArrayPath(directCallee->fullPath)) &&
              statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if ((rawPath.rfind(collectionMemberRoot("vector"), 0) == 0 ||
                rawPath.rfind(vectorBackingMemberRoot(), 0) == 0 ||
                directCallee->fullPath.rfind(collectionMemberRoot("vector"), 0) == 0 ||
                directCallee->fullPath.rfind(vectorBackingMemberRoot(), 0) == 0) &&
              statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee)) {
              std::string vectorHelperName;
              const bool isMaterializableVectorMetadataReceiver =
              resolveVectorHelperAliasName(expr, vectorHelperName) &&
              expr.args.size() == 1 &&
              expr.args.front().kind == Expr::Kind::Call &&
              !expr.args.front().isFieldAccess &&
              statementsExprHelpers.resolveDirectHelperDefinition(expr.args.front()) != nullptr &&
              (vectorHelperName == "count" || vectorHelperName == "capacity");
              const bool isExplicitVectorMetadataHelper =
              resolveVectorHelperAliasName(expr, vectorHelperName) &&
              expr.args.size() == 1 &&
              (vectorHelperName == "count" || vectorHelperName == "capacity");
              auto directCalleeFirstParameterCollectionName = [&]() {
                if (directCallee->parameters.empty()) {
                  return std::string{};
                }
                std::string typeName;
                std::vector<std::string> templateArgs;
                if (!extractFirstBindingTypeTransform(
                    directCallee->parameters.front(),
                    typeName,
                    templateArgs)) {
                  return std::string{};
                }
                return normalizeCollectionBindingTypeName(typeName);
              };
              if (isExplicitVectorMetadataHelper &&
                directCalleeFirstParameterCollectionName() == "map") {
                if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                  return false;
                }
                return true;
              }
              const bool isDirectVectorBuiltin =
              (statementsExprHelpers.resolveBuiltinAccessName(expr, vectorHelperName) &&
                expr.args.size() == 2 &&
                (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) ||
              isMaterializableVectorMetadataReceiver ||
              isExplicitVectorMetadataHelper;
              if (!isDirectVectorBuiltin) {
                if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                  return false;
                }
                return true;
              }
            }
            if (statementsExprHelpers.hasKeyValueEntryCtorArgs(expr) &&
              statementsExprHelpers.extractHelperTail(normalizeCollectionHelperPath(directCallee->fullPath)) ==
              "map" &&
              (statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath) ||
                statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(resolvedExprPath)) &&
              statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (!statementsExprHelpers.hasKeyValueEntryCtorArgs(expr) &&
              (statementsExprHelpers.isCanonicalKeyValueConstructorPath(rawPath) ||
                statementsExprHelpers.isCanonicalKeyValueConstructorPath(resolvedExprPath) ||
                statementsExprHelpers.isCanonicalKeyValueConstructorPath(directCallee->fullPath)) &&
              statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee) &&
              ir_lowerer::resolveCollectionPairTypeInfo(
                expr,
                localsIn,
                {},
                semanticProgram,
                &callResolutionAdapters.semanticProductTargets.semanticIndex)
              .isKeyValueTarget) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            std::string helperName;
            const bool hasKeyValueHelperAlias = resolveKeyValueHelperAliasName(expr, helperName);
            bool hasSameFamilyKeyValueHelperAlias = false;
            if (!hasKeyValueHelperAlias) {
              const size_t leafStart = rawPath.find_last_of('/');
              std::string helperLeaf =
              leafStart == std::string::npos ? rawPath : rawPath.substr(leafStart + 1);
              const size_t generatedSuffix = helperLeaf.find("__");
              if (generatedSuffix != std::string::npos) {
                helperLeaf.erase(generatedSuffix);
              }
              if (primec::collection_helpers::isTryAtHelperName(helperLeaf)) {
                helperName = "tryAt";
              } else if (primec::collection_helpers::isAtHelperName(helperLeaf)) {
                helperName = "at";
              } else if (primec::collection_helpers::isAtUnsafeHelperName(helperLeaf)) {
                helperName = "at_unsafe";
              }
              if (helperName.empty() && !expr.args.empty()) {
                hasSameFamilyKeyValueHelperAlias =
                statementsExprHelpers.resolveSameFamilyKeyValueHelperMemberName(
                  expr, expr.args.front(), helperName, localsIn);
              }
            }
            const bool hasCanonicalKeyValueHelperFamily =
            statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath) ||
            statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(directCallee->fullPath);
            if (!helperName.empty() &&
              (helperName == "count" || helperName == "contains" ||
                helperName == "tryAt" || helperName == "at" ||
                helperName == "at_unsafe" || primec::collection_helpers::isInsertHelperName(helperName)) &&
              (hasCanonicalKeyValueHelperFamily ||
                hasSameFamilyKeyValueHelperAlias) &&
              statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee)) {
              const bool deferKeyValueCountToBuiltinEmitter =
              helperName == "count" && expr.args.size() == 1 &&
              expr.args.front().kind == Expr::Kind::Call &&
              statementsExprHelpers.hasSemanticKeyValueHelperDefinition(helperName) &&
              statementsExprHelpers.resolveKeyValueAccessReceiverInfo(expr, expr.args.front(), localsIn)
              .isKeyValueTarget;
              const bool deferWrapperReturnedKeyValueAccessDiagnostic =
              (helperName == "at" || helperName == "at_unsafe") &&
              expr.args.size() == 2 &&
              expr.args.front().kind == Expr::Kind::Call;
              if (deferWrapperReturnedKeyValueAccessDiagnostic) {
                error = "struct parameter type mismatch";
                return false;
              }
              if (!deferKeyValueCountToBuiltinEmitter &&
                !deferWrapperReturnedKeyValueAccessDiagnostic) {
                if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                  return false;
                } else {
                  return true;
                }
              }
            }
            std::string accessName;
            std::string explicitKeyValueAccessHelperName;
            std::string canonicalKeyValueAccessLeaf;
            if (statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath)) {
              const size_t leafStart = rawPath.find_last_of('/');
              canonicalKeyValueAccessLeaf =
              leafStart == std::string::npos ? rawPath : rawPath.substr(leafStart + 1);
              const size_t generatedSuffix = canonicalKeyValueAccessLeaf.find("__");
              if (generatedSuffix != std::string::npos) {
                canonicalKeyValueAccessLeaf.erase(generatedSuffix);
              }
            }
            const bool isExplicitCanonicalKeyValueAccess =
            (getBuiltinArrayAccessName(expr, accessName) &&
              expr.args.size() == 2 &&
              statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath)) ||
            (resolveKeyValueHelperAliasName(expr, explicitKeyValueAccessHelperName) &&
              (primec::collection_helpers::isAtHelperName(explicitKeyValueAccessHelperName) ||
                primec::collection_helpers::isAtUnsafeHelperName(explicitKeyValueAccessHelperName)) &&
              expr.args.size() == 2 &&
              statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath)) ||
            ((primec::collection_helpers::isAtHelperName(canonicalKeyValueAccessLeaf) ||
                primec::collection_helpers::isAtUnsafeHelperName(canonicalKeyValueAccessLeaf)) &&
              expr.args.size() == 2 &&
              statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath));
            if (isExplicitCanonicalKeyValueAccess &&
              statementsExprHelpers.isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (ir_lowerer::resolveCollectionPairTypeInfo(
                  expr.args.front(),
                  localsIn,
                  {},
                  semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex)
                .isKeyValueTarget) {
                std::string builtinAccessName = accessName;
                if (builtinAccessName.empty()) {
                  builtinAccessName = explicitKeyValueAccessHelperName;
                }
                if (builtinAccessName.empty()) {
                  builtinAccessName = canonicalKeyValueAccessLeaf;
                }
                if (builtinAccessName == primec::collection_helpers::kAtRef) {
                  builtinAccessName = "at";
                } else if (builtinAccessName == primec::collection_helpers::kAtUnsafeRef) {
                  builtinAccessName = "at_unsafe";
                }
                Expr rewrittenExpr = expr;
                rewrittenExpr.name = builtinAccessName;
                rewrittenExpr.namespacePrefix.clear();
                rewrittenExpr.semanticNodeId = 0;
                rewrittenExpr.templateArgs.clear();
                return emitExpr(rewrittenExpr, localsIn);
              }
              error =
              "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
              resolveExprPath(expr) + ", name=" + expr.name +
              ", args=" + std::to_string(expr.args.size()) +
              ", method=" + std::string(expr.isMethodCall ? "true" : "false") + ")";
              return false;
            }
          }
        }
        auto generatedPrimitiveDefaultKind = [&]() {
          const std::string resolvedPrimitivePath = resolveExprPath(expr);
          const size_t slash = resolvedPrimitivePath.find_last_of('/');
          const std::string leaf = slash == std::string::npos
          ? resolvedPrimitivePath
          : resolvedPrimitivePath.substr(slash + 1);
          if (leaf == "int" || leaf == "i32" || leaf == "i64" ||
            leaf == "u64" || leaf == "float" || leaf == "f32" ||
            leaf == "f64" || leaf == "bool") {
            return valueKindFromTypeName(leaf);
          }
          return LocalInfo::ValueKind::Unknown;
        };
        if (!expr.isMethodCall &&
          expr.args.empty() &&
          expr.templateArgs.empty() &&
          !expr.hasBodyArguments &&
          expr.bodyArguments.empty()) {
          switch (generatedPrimitiveDefaultKind()) {
            case LocalInfo::ValueKind::Int32:
            case LocalInfo::ValueKind::Bool:
              function.instructions.push_back({IrOpcode::PushI32, 0});
              return true;
            case LocalInfo::ValueKind::Int64:
            case LocalInfo::ValueKind::UInt64:
              function.instructions.push_back({IrOpcode::PushI64, 0});
              return true;
            case LocalInfo::ValueKind::Float32:
              function.instructions.push_back({IrOpcode::PushF32, 0});
              return true;
            case LocalInfo::ValueKind::Float64:
              function.instructions.push_back({IrOpcode::PushF64, 0});
              return true;
            default:
              break;
          }
        }

        std::string accessName;
        if (statementsExprHelpers.resolveBuiltinAccessName(expr, accessName)) {
          const bool isMethodCallTempReceiver =
          expr.isMethodCall &&
          !expr.args.empty() &&
          expr.args.front().kind == Expr::Kind::Call &&
          (accessName == "at" || accessName == "at_unsafe");
          bool tempReceiverSupportsBuiltinAccess = false;
          if (isMethodCallTempReceiver) {
            ir_lowerer::ArrayVectorAccessTargetInfo targetInfo;
            tempReceiverSupportsBuiltinAccess =
            statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
              expr.args.front(), targetInfo, localsIn);
          }
          // A bare/builtin `at`/`at_unsafe` whose receiver is a key-value map must
          // be lowered through the key-value access path below, not the raw
          // array/vector emitter. This matters in particular for canonical
          // `/std/collections/ma p/at_unsafe` calls that get rewritten into a bare
          // builtin access form and re-emitted here.
          // TODO-4760: hasKeyValueKinds/isKeyValueTarget alone can't tell
          // apart the stdlib map constructor's own internal
          // `args<Entry<K, V>>` pack parameter (which must keep deferring
          // here - that path already works) from a user-level
          // `args<map<K, V>>` pack element (which does not - it needs
          // emitBuiltinArrayAccess below instead). `isMapArgsPackElement`
          // (see IrLowererSharedTypes.h) names the distinguishing signal: a
          // bare `args<map<K,V>>` pack element's LocalInfo carries
          // keyValueKeyKind/keyValueValueKind but an EMPTY structTypeName,
          // unlike `args<Entry<K, V>>` (the map constructor's own internal
          // pack), whose structTypeName is always populated with a concrete
          // monomorphized entry-struct path. See docs/ReceiverTargetResolutionConsolidation.md.
          // TODO-5287 (see docs/todo_finished.md): this structTypeName-
          // emptiness check only covers a bare `Name`-kind receiver (a
          // direct args-pack-of-map local). The emission-side twin of this
          // gap - `isMapArgsPackElementTarget` in
          // IrLowererIndexedAccessEmit.cpp's emitBuiltinArrayAccess - covers
          // a `Call`-kind receiver (a nested pack-element access) instead,
          // and does NOT apply an equivalent structTypeName check there; see
          // that call site's comment. Neither `resolveCollectionPairTypeInfo`
          // nor `resolveArrayVectorAccessTargetInfo` bakes this
          // Name-vs-Call/structTypeName distinction into their own
          // args-pack-element resolution helpers - it is only applied here,
          // ad hoc, for the Name-receiver case. See TODO-5292 for the
          // concrete unification/fix this gap motivates.
          const bool isKeyValueAccessReceiverArgsPackOfMap =
          expr.args.front().kind == Expr::Kind::Name &&
          [&]() {
            auto receiverLocalIt = localsIn.find(expr.args.front().name);
            return receiverLocalIt != localsIn.end() &&
            receiverLocalIt->second.isArgsPack &&
            isMapArgsPackElement(receiverLocalIt->second);
          }();
          const bool isKeyValueAccessTarget =
          (accessName == "at" || accessName == "at_unsafe") &&
          !expr.args.empty() &&
          !isKeyValueAccessReceiverArgsPackOfMap &&
          ir_lowerer::resolveCollectionPairTypeInfo(
            expr.args.front(),
            localsIn,
            {},
            semanticProgram,
            &callResolutionAdapters.semanticProductTargets.semanticIndex)
          .isKeyValueTarget;
          // A same-path user definition overriding the canonical
          // /std/collections/vector/at(_unsafe) (or bare at/at_unsafe alias)
          // helper must win over the builtin raw array/vector access below -
          // otherwise a struct-returning override's call sites get the
          // builtin scalar-element access pattern instead of the user's own
          // body, corrupting the IR for any subsequent struct handling (see
          // TODO-4804).
          const Definition *directBuiltinAccessOverrideCallee =
          (accessName == "at" || accessName == "at_unsafe")
          ? statementsExprHelpers.resolveDirectHelperDefinition(expr)
          : nullptr;
          if (directBuiltinAccessOverrideCallee != nullptr) {
            if (!emitInlineDefinitionCall(
                expr, *directBuiltinAccessOverrideCallee, localsIn, true)) {
              return false;
            }
            return true;
          } else if (isKeyValueAccessTarget) {
            // Fall through to the key-value access handling further below.
          } else if (isMethodCallTempReceiver && !tempReceiverSupportsBuiltinAccess) {
            // Let normal helper lowering handle method calls on constructor- or
            // helper-backed temporaries instead of forcing builtin raw access.
          } else {
            if (expr.args.size() != 2) {
              error = accessName + " requires exactly two arguments";
              return false;
            }
            if (!emitBuiltinArrayAccess(
                accessName,
                expr.args[0],
                expr.args[1],
                localsIn,
                resolveStringTableTarget,
                0,
                [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                  return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                    targetCallExpr, targetInfoOut, localsIn);
                },
                inferExprKind,
                isEntryArgsName,
                allocTempLocal,
                [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                  return emitExpr(valueExpr, valueLocals);
                },
                emitStringIndexOutOfBounds,
                emitArrayIndexOutOfBounds,
                [&]() { return function.instructions.size(); },
                [&](IrOpcode opcode, uint64_t imm) {
                  function.instructions.push_back({opcode, imm});
                },
                [&](size_t instructionIndex, uint64_t imm) {
                  function.instructions[instructionIndex].imm = imm;
                },
                error,
                semanticProgram,
                &callResolutionAdapters.semanticProductTargets.semanticIndex)) {
              return false;
            }
            return true;
          }  // end if/else-if/else (directBuiltinAccessOverrideCallee != nullptr)
        }  // end if (statementsExprHelpers.resolveBuiltinAccessName(expr, accessName))

        auto semanticQueryExprReturnsString = [&](const Expr &candidate) {
          if (semanticProgram == nullptr) {
            return false;
          }
          const auto *queryFact = ir_lowerer::findSemanticProductQueryFact(
            semanticProgram,
            callResolutionAdapters.semanticProductTargets.semanticIndex,
            candidate);
          if (queryFact == nullptr) {
            return false;
          }
          const std::string queryType = resolveSemanticProductTypeText(
            semanticProgram, queryFact->queryTypeText,
            queryFact->queryTypeTextId);
          const std::string bindingType = resolveSemanticProductTypeText(
            semanticProgram, queryFact->bindingTypeText,
            queryFact->bindingTypeTextId);
          return queryType == "string" || queryType == primec::collection_helpers::kRootedString ||
          bindingType == "string" || bindingType == primec::collection_helpers::kRootedString;
        };
        if (expr.isMethodCall && expr.args.size() == 1 &&
          (findSemanticProductMethodCallTarget(semanticProgram, expr) ==
            primec::collection_helpers::kRootedStringCount ||
            (isSimpleCallName(expr, "count") &&
              semanticQueryExprReturnsString(expr.args.front())))) {
          if (const Definition *stringCountCallee =
            statementsExprHelpers.findDirectHelperDefinition(primec::collection_helpers::kRootedStringCount);
            stringCountCallee != nullptr) {
            Expr directStringCountExpr = expr;
            directStringCountExpr.isMethodCall = false;
            directStringCountExpr.isFieldAccess = false;
            directStringCountExpr.namespacePrefix.clear();
            directStringCountExpr.name = primec::collection_helpers::kRootedStringCount;
            directStringCountExpr.semanticNodeId = 0;
            if (!emitInlineDefinitionCall(
                directStringCountExpr, *stringCountCallee, localsIn, true)) {
              return false;
            }
            return true;
          }
        }

        if (expr.isMethodCall && expr.args.size() == 1 &&
          (resolveExprPath(expr) == primec::collection_helpers::kRootedStringCount ||
            isSimpleCallName(expr, "count"))) {
          const Expr &stringCountTarget = expr.args.front();
          std::string stringAccessName;
          // `map.at(key)` returns the stored value (which may itself be a
          // string); a chained `.count()` on it must not be rewritten as a
          // string-character index of the map. Only treat the inner access as a
          // string index when its receiver is not a key/value map.
          const bool stringCountTargetIsKeyValueAccess =
          stringCountTarget.kind == Expr::Kind::Call &&
          stringCountTarget.args.size() == 2 &&
          ir_lowerer::resolveCollectionPairTypeInfo(
            stringCountTarget.args.front(),
            localsIn,
            {},
            semanticProgram,
            &callResolutionAdapters.semanticProductTargets.semanticIndex)
          .isKeyValueTarget;
          if (!stringCountTargetIsKeyValueAccess &&
            stringCountTarget.kind == Expr::Kind::Call &&
            stringCountTarget.args.size() == 2 &&
            getBuiltinArrayAccessName(stringCountTarget, stringAccessName) &&
            (stringAccessName == "at" || stringAccessName == "at_unsafe")) {
            Expr rewrittenStringTarget = stringCountTarget;
            rewrittenStringTarget.isMethodCall = false;
            rewrittenStringTarget.isFieldAccess = false;
            rewrittenStringTarget.namespacePrefix.clear();
            rewrittenStringTarget.name =
            canonicalKeyValueHelperPath(stringAccessName);
            if (!emitExpr(rewrittenStringTarget, localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadStringLength, 0});
            return true;
          }
          auto semanticFactTypeText = [&](SymbolId typeTextId,
            const std::string &fallback) {
            if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
              const std::string resolvedTypeText = std::string(
                semanticProgramResolveCallTargetString(*semanticProgram,
                  typeTextId));
              if (!resolvedTypeText.empty()) {
                return trimTemplateTypeText(resolvedTypeText);
              }
            }
            return trimTemplateTypeText(fallback);
          };
          auto semanticQueryReturnsString = [&]() {
            if (semanticProgram == nullptr) {
              return false;
            }
            const auto *queryFact = ir_lowerer::findSemanticProductQueryFact(
              semanticProgram,
              callResolutionAdapters.semanticProductTargets.semanticIndex,
              stringCountTarget);
            if (queryFact == nullptr) {
              return false;
            }
            const std::string queryType = semanticFactTypeText(
              queryFact->queryTypeTextId, queryFact->queryTypeText);
            const std::string bindingType = semanticFactTypeText(
              queryFact->bindingTypeTextId, queryFact->bindingTypeText);
            return queryType == "string" || queryType == primec::collection_helpers::kRootedString ||
            bindingType == "string" || bindingType == primec::collection_helpers::kRootedString;
          };
          const bool hasDirectStringCountTarget =
          ((stringCountTarget.kind == Expr::Kind::Name ||
              stringCountTarget.kind == Expr::Kind::StringLiteral ||
              stringCountTarget.kind == Expr::Kind::Call) &&
            inferExprKind(stringCountTarget, localsIn) ==
            LocalInfo::ValueKind::String) ||
          semanticQueryReturnsString();
          if (hasDirectStringCountTarget) {
            const Definition *stringCountCallee =
            resolveMethodCallDefinition(expr, localsIn);
            if (stringCountCallee == nullptr) {
              stringCountCallee = statementsExprHelpers.findDirectHelperDefinition(primec::collection_helpers::kRootedStringCount);
            }
            if (stringCountCallee != nullptr) {
              Expr directStringCountExpr = expr;
              directStringCountExpr.isMethodCall = false;
              directStringCountExpr.isFieldAccess = false;
              directStringCountExpr.namespacePrefix.clear();
              directStringCountExpr.name = primec::collection_helpers::kRootedStringCount;
              directStringCountExpr.semanticNodeId = 0;
              if (!emitInlineDefinitionCall(
                  directStringCountExpr, *stringCountCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (!emitExpr(stringCountTarget, localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::LoadStringLength, 0});
            return true;
          }
        }

        Expr countAccessExpr = expr;
        if (!expr.isMethodCall && expr.args.size() == 1) {
          auto resolveSourceQueryPath = [&](const Expr &callExpr) {
            if (semanticProgram == nullptr) {
              return std::string{};
            }
            std::vector<std::pair<int, int>> sourcePositions;
            if (callExpr.sourceLine != 0 && callExpr.sourceColumn != 0) {
              sourcePositions.emplace_back(callExpr.sourceLine,
                callExpr.sourceColumn);
            }
            if (!callExpr.args.empty() &&
              callExpr.args.front().sourceLine != 0 &&
              callExpr.args.front().sourceColumn != 0) {
              sourcePositions.emplace_back(callExpr.args.front().sourceLine,
                callExpr.args.front().sourceColumn);
            }
            for (const auto &queryFact : semanticProgram->queryFacts) {
              const bool sameSourcePosition =
              std::any_of(sourcePositions.begin(),
                sourcePositions.end(),
                [&](const auto &sourcePosition) {
                  return queryFact.sourceLine ==
                  sourcePosition.first &&
                  queryFact.sourceColumn ==
                  sourcePosition.second;
              });
              if (!sameSourcePosition) {
                continue;
              }
              const std::string_view callName =
              queryFact.callNameId != InvalidSymbolId
              ? semanticProgramResolveCallTargetString(
                *semanticProgram, queryFact.callNameId)
              : std::string_view(queryFact.callName);
              if (callName != callExpr.name &&
                (callExpr.sourceName.empty() ||
                  callName != callExpr.sourceName)) {
                continue;
              }
              if (queryFact.resolvedPathId == InvalidSymbolId) {
                return std::string{};
              }
              return std::string(semanticProgramResolveCallTargetString(
                  *semanticProgram, queryFact.resolvedPathId));
            }
            return std::string{};
          };
          if (const auto *metadata = statementsExprHelpers.keyValueHelperMetadata();
            metadata != nullptr) {
            std::string semanticHelperName;
            const std::string semanticResolvedPath =
            resolveSourceQueryPath(expr);
            if (!semanticResolvedPath.empty() &&
              resolvePublishedStdlibSurfaceMemberName(
                semanticResolvedPath, metadata->id, semanticHelperName) &&
              semanticHelperName == "count") {
              if (const Definition *semanticCountDef =
                statementsExprHelpers.findDirectHelperDefinition(semanticResolvedPath);
                semanticCountDef != nullptr) {
                Expr directCountExpr = expr;
                directCountExpr.name = semanticResolvedPath;
                directCountExpr.namespacePrefix.clear();
                directCountExpr.semanticNodeId = 0;
                if (!emitInlineDefinitionCall(
                    directCountExpr, *semanticCountDef, localsIn, true)) {
                  return false;
                }
                return true;
              }
            }
          }
          std::string keyValueCountHelperName;
