// soa-surface-audit: exempt
// collection-surface-audit: exempt
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
                       helperName == "at_ref" ||
                       helperName == "at_unsafe_ref";
              }
              auto isAccessHelperPath = [&](std::string path) {
                path = statementsExprHelpers.stripGeneratedHelperSuffix(
                    normalizeCollectionHelperPath(std::move(path)));
                return statementsExprHelpers.isKeyValueHelperMemberPath(path, "at") ||
                       statementsExprHelpers.isKeyValueHelperMemberPath(path, "at_unsafe") ||
                       path == "at" || path == "at_unsafe" ||
                       path == "/std/collections/map/at" ||
                       path == "/std/collections/map/at_unsafe";
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
            if ((rawPath.rfind("/array/", 0) == 0 ||
                 resolvedExprPath.rfind("/array/", 0) == 0 ||
                 directCallee->fullPath.rfind("/array/", 0) == 0) &&
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
              if (helperLeaf == "tryAt" || helperLeaf == "tryAt_ref") {
                helperName = "tryAt";
              } else if (helperLeaf == "at" || helperLeaf == "at_ref") {
                helperName = "at";
              } else if (helperLeaf == "at_unsafe" || helperLeaf == "at_unsafe_ref") {
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
                 helperName == "at_unsafe" || helperName == "insert" ||
                 helperName == "insert_ref") &&
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
                 (explicitKeyValueAccessHelperName == "at" ||
                  explicitKeyValueAccessHelperName == "at_ref" ||
                  explicitKeyValueAccessHelperName == "at_unsafe" ||
                  explicitKeyValueAccessHelperName == "at_unsafe_ref") &&
                 expr.args.size() == 2 &&
                 statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(rawPath)) ||
                ((canonicalKeyValueAccessLeaf == "at" ||
                  canonicalKeyValueAccessLeaf == "at_ref" ||
                  canonicalKeyValueAccessLeaf == "at_unsafe" ||
                  canonicalKeyValueAccessLeaf == "at_unsafe_ref") &&
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
                if (builtinAccessName == "at_ref") {
                  builtinAccessName = "at";
                } else if (builtinAccessName == "at_unsafe_ref") {
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
          const bool isKeyValueAccessTarget =
              (accessName == "at" || accessName == "at_unsafe") &&
              !expr.args.empty() &&
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
          }
        }

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
          return queryType == "string" || queryType == "/string" ||
                 bindingType == "string" || bindingType == "/string";
        };
        if (expr.isMethodCall && expr.args.size() == 1 &&
            (findSemanticProductMethodCallTarget(semanticProgram, expr) ==
                 "/string/count" ||
             (isSimpleCallName(expr, "count") &&
              semanticQueryExprReturnsString(expr.args.front())))) {
          if (const Definition *stringCountCallee =
                  statementsExprHelpers.findDirectHelperDefinition("/string/count");
              stringCountCallee != nullptr) {
            Expr directStringCountExpr = expr;
            directStringCountExpr.isMethodCall = false;
            directStringCountExpr.isFieldAccess = false;
            directStringCountExpr.namespacePrefix.clear();
            directStringCountExpr.name = "/string/count";
            directStringCountExpr.semanticNodeId = 0;
            if (!emitInlineDefinitionCall(
                    directStringCountExpr, *stringCountCallee, localsIn, true)) {
              return false;
            }
            return true;
          }
        }

        if (expr.isMethodCall && expr.args.size() == 1 &&
            (resolveExprPath(expr) == "/string/count" ||
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
            return queryType == "string" || queryType == "/string" ||
                   bindingType == "string" || bindingType == "/string";
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
              stringCountCallee = statementsExprHelpers.findDirectHelperDefinition("/string/count");
            }
            if (stringCountCallee != nullptr) {
              Expr directStringCountExpr = expr;
              directStringCountExpr.isMethodCall = false;
              directStringCountExpr.isFieldAccess = false;
              directStringCountExpr.namespacePrefix.clear();
              directStringCountExpr.name = "/string/count";
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
          if (statementsExprHelpers.resolveSameFamilyKeyValueHelperMemberName(
                  expr, expr.args.front(), keyValueCountHelperName, localsIn) &&
              keyValueCountHelperName == "count" &&
              statementsExprHelpers.hasSemanticKeyValueHelperDefinition(keyValueCountHelperName)) {
            if (const auto *metadata = statementsExprHelpers.keyValueHelperMetadata();
                metadata != nullptr) {
              const std::string canonicalCountPath =
                  stdlibSurfaceCanonicalHelperPath(metadata->id,
                                                   keyValueCountHelperName);
              if (!canonicalCountPath.empty()) {
                countAccessExpr.name = canonicalCountPath;
                countAccessExpr.namespacePrefix.clear();
                countAccessExpr.semanticNodeId = 0;
                if (expr.args.front().kind != Expr::Kind::Call) {
                  if (const Definition *canonicalCountDef =
                          statementsExprHelpers.findDirectHelperDefinition(canonicalCountPath);
                      canonicalCountDef != nullptr) {
                    if (!emitInlineDefinitionCall(
                            countAccessExpr, *canonicalCountDef, localsIn, true)) {
                      return false;
                    }
                    return true;
                  }
                }
              }
            }
          }
        }

        if (!countAccessExpr.isMethodCall && countAccessExpr.args.size() == 1) {
          std::string vectorMetadataHelperName;
          const std::string vectorMetadataPath = resolveExprPath(countAccessExpr);
          if ((resolveVectorHelperAliasName(
                   countAccessExpr, vectorMetadataHelperName) &&
               (vectorMetadataHelperName == "count" ||
                vectorMetadataHelperName == "capacity")) ||
              (vectorMetadataPath == "/std/collections/vector/count" &&
               (vectorMetadataHelperName = "count", true)) ||
              (vectorMetadataPath == "/std/collections/vector/capacity" &&
               (vectorMetadataHelperName = "capacity", true))) {
            if (const Definition *directVectorMetadataCallee =
                    statementsExprHelpers.resolveDirectHelperDefinition(countAccessExpr);
                directVectorMetadataCallee != nullptr &&
                !directVectorMetadataCallee->parameters.empty()) {
              std::string receiverTypeName;
              std::vector<std::string> receiverTemplateArgs;
              if (extractFirstBindingTypeTransform(
                      directVectorMetadataCallee->parameters.front(),
                      receiverTypeName,
                      receiverTemplateArgs) &&
                  normalizeCollectionBindingTypeName(receiverTypeName) ==
                      "map") {
                if (!emitInlineDefinitionCall(
                        countAccessExpr,
                        *directVectorMetadataCallee,
                        localsIn,
                        true)) {
                  return false;
                }
                return true;
              }
            }
          }
        }

        const auto countAccessResult = tryEmitCountAccessCall(
            countAccessExpr,
            localsIn,
            isArrayCountCall,
            isVectorCapacityCall,
            isStringCountCall,
            isEntryArgsName,
	            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
	              if (semanticProgram != nullptr) {
	                if (const auto *queryFact =
	                        ir_lowerer::findSemanticProductQueryFact(
	                            semanticProgram,
	                            callResolutionAdapters.semanticProductTargets
	                                .semanticIndex,
	                            targetExpr);
	                    queryFact != nullptr) {
	                  auto resolveFactTypeText = [&](SymbolId typeTextId,
	                                                  const std::string &fallback) {
	                    if (typeTextId != InvalidSymbolId) {
	                      const std::string resolvedTypeText =
	                          std::string(semanticProgramResolveCallTargetString(
	                              *semanticProgram, typeTextId));
	                      if (!resolvedTypeText.empty()) {
	                        return trimTemplateTypeText(resolvedTypeText);
	                      }
	                    }
	                    return trimTemplateTypeText(fallback);
	                  };
	                  const std::string queryType = resolveFactTypeText(
	                      queryFact->queryTypeTextId, queryFact->queryTypeText);
	                  const std::string bindingType = resolveFactTypeText(
	                      queryFact->bindingTypeTextId,
	                      queryFact->bindingTypeText);
	                  if (queryType == "string" || queryType == "/string" ||
	                      bindingType == "string" || bindingType == "/string") {
	                    return false;
	                  }
	                }
	              }
	              const auto targetInfo =
	                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
	                      targetExpr,
                      targetLocals,
                      [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                        return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                            targetCallExpr, targetInfoOut, targetLocals);
                      });
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              const bool isCollectionVectorTarget =
                  statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
              const bool isExperimentalKeyValueTarget =
                  structPath == keyValueStorageStructRootPath() ||
                  statementsExprHelpers.matchesGeneratedSpecializedType(structPath, "map", "Map");
              const bool isSemanticKeyValueTarget =
                  ir_lowerer::resolveCollectionPairTypeInfo(
                      targetExpr,
                      targetLocals,
                      {},
                      semanticProgram,
                      &callResolutionAdapters.semanticProductTargets.semanticIndex)
                      .isKeyValueTarget;
              const bool isExperimentalSoaVectorTarget =
                  structPath == collection_paths::memberPath(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName) ||
                  structPath.rfind(collection_paths::specializedTypePrefix(collection_paths::kSoaFolder, collection_paths::kSoaVectorTypeName), 0) == 0;
              return targetInfo.isArrayOrVectorTarget || structPath == "/array" ||
                     structPath == "/vector" || structPath == "/Buffer" || structPath == "/map" ||
                     structPath == "/soa" || isCollectionVectorTarget ||
                     isExperimentalKeyValueTarget || isSemanticKeyValueTarget ||
                     isExperimentalSoaVectorTarget;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                        return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                            targetCallExpr, targetInfoOut, targetLocals);
                      });
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                        return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                            targetCallExpr, targetInfoOut, targetLocals);
                      });
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
            },
            inferExprKind,
            resolveStringTableTarget,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            [&](IrOpcode opcode, uint64_t imm) { function.instructions.push_back({opcode, imm}); },
            error,
            semanticProgram,
            &callResolutionAdapters.semanticProductTargets.semanticIndex);
        if (countAccessResult == CountAccessCallEmitResult::Emitted) {
          return true;
        }
        if (countAccessResult == CountAccessCallEmitResult::Error) {
          return false;
        }
        const auto countFallbackResult = tryEmitNonMethodCountFallback(
            expr,
            [&](const Expr &callExpr) { return isArrayCountCall(callExpr, localsIn); },
            [&](const Expr &callExpr) { return isStringCountCall(callExpr, localsIn); },
            [&](const Expr &callExpr) {
              return resolveMethodCallDefinition(callExpr, localsIn);
            },
            [&](const Expr &callExpr, const Definition &callee) {
              return emitInlineDefinitionCall(callExpr, callee, localsIn, true);
            },
            error);
        if (countFallbackResult == CountMethodFallbackResult::Emitted) {
          return true;
        }
        if (countFallbackResult == CountMethodFallbackResult::Error) {
          return false;
        }
        if (expr.isMethodCall) {
          auto isInternalSoaMetadataReceiver = [&](const Expr &receiver) {
            auto unwrapInternalSoaMetadataPath = [](std::string structPath) {
              structPath = trimTemplateTypeText(structPath);
              for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
                if (structPath.rfind(wrapper, 0) == 0 &&
                    structPath.size() > wrapper.size() &&
                    structPath.back() == '>') {
                  structPath = trimTemplateTypeText(
                      structPath.substr(wrapper.size(),
                                        structPath.size() - wrapper.size() - 1));
                  break;
                }
              }
              return structPath;
            };
            if (receiver.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiver.name);
              if (localIt != localsIn.end()) {
                std::string localStructPath =
                    unwrapInternalSoaMetadataPath(localIt->second.structTypeName);
                const size_t localTemplateStart = localStructPath.find('<');
                if (localTemplateStart != std::string::npos) {
                  localStructPath.erase(localTemplateStart);
                }
                const size_t localLeafStart = localStructPath.find_last_of('/');
                const size_t localSuffixStart =
                    localStructPath.find("__",
                                         localLeafStart == std::string::npos
                                             ? 0
                                             : localLeafStart + 1);
                if (localSuffixStart != std::string::npos) {
                  localStructPath.erase(localSuffixStart);
                }
                if (localStructPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
                    localStructPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView")) {
                  return true;
                }
              }
            }
            std::string structPath =
                unwrapInternalSoaMetadataPath(inferStructExprPath(receiver, localsIn));
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
            return structPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName) ||
                   structPath == collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView");
          };
          auto emitInternalSoaMetadataBase = [&](const Expr &receiver) {
            if (receiver.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiver.name);
              if (localIt != localsIn.end() &&
                  isInternalSoaMetadataReceiver(receiver)) {
                function.instructions.push_back(
                    {IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index)});
                return true;
              }
            }
            return emitExpr(receiver, localsIn);
          };
          if (expr.args.size() == 1 &&
              (isSimpleCallName(expr, "field_count") ||
               isSimpleCallName(expr, "field_capacity")) &&
              isInternalSoaMetadataReceiver(expr.args.front())) {
            if (!emitInternalSoaMetadataBase(expr.args.front())) {
              return false;
            }
            const uint64_t slotOffset =
                isSimpleCallName(expr, "field_capacity") ? IrSlotBytes * 2 : IrSlotBytes;
            function.instructions.push_back({IrOpcode::PushI64, slotOffset});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
          const std::string priorError = error;
          const Definition *methodCallee =
              resolveMethodCallDefinition(expr, localsIn);
          if (methodCallee == nullptr) {
            methodCallee = statementsExprHelpers.findDirectHelperDefinition(resolveExprPath(expr));
          }
          if (methodCallee != nullptr && expr.args.size() == 1 &&
              (isSimpleCallName(expr, "field_count") ||
               isSimpleCallName(expr, "field_capacity")) &&
              (methodCallee->fullPath.rfind(
                   collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, collection_paths::kSoaColumnTypeName), 0) == 0 ||
               methodCallee->fullPath.rfind(
                   collection_paths::memberPath(collection_paths::kInternalSoaStorageFolder, "SoaFieldView"), 0) == 0)) {
            if (!emitInternalSoaMetadataBase(expr.args.front())) {
              return false;
            }
            const uint64_t slotOffset =
                isSimpleCallName(expr, "field_capacity") ? IrSlotBytes * 2 : IrSlotBytes;
            function.instructions.push_back({IrOpcode::PushI64, slotOffset});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            error = priorError;
            return true;
          }
          if (methodCallee != nullptr) {
            auto methodCalleeFirstParameterIsStruct = [&]() {
              if (methodCallee->parameters.empty()) {
                return false;
              }
              std::string typeName;
              std::vector<std::string> templateArgs;
              if (!extractFirstBindingTypeTransform(
                      methodCallee->parameters.front(),
                      typeName,
                      templateArgs) ||
                  !templateArgs.empty()) {
                return false;
              }
              std::string resolvedStructPath;
              return resolveStructTypeName(
                  typeName, methodCallee->namespacePrefix, resolvedStructPath);
            };
            auto isWrapperReturnedKeyValueAccessCall =
                [&](const Expr &candidate) {
              if (candidate.kind != Expr::Kind::Call ||
                  candidate.args.size() < 2 ||
                  candidate.args.front().kind != Expr::Kind::Call) {
                return false;
              }
              std::string helperName;
              if (resolveKeyValueHelperAliasName(candidate, helperName)) {
                return helperName == "at" || helperName == "at_unsafe" ||
                       helperName == "at_ref" ||
                       helperName == "at_unsafe_ref";
              }
              auto isAccessHelperPath = [&](std::string path) {
                path = statementsExprHelpers.stripGeneratedHelperSuffix(
                    normalizeCollectionHelperPath(std::move(path)));
                return statementsExprHelpers.isKeyValueHelperMemberPath(path, "at") ||
                       statementsExprHelpers.isKeyValueHelperMemberPath(path, "at_unsafe") ||
                       path == "at" || path == "at_unsafe" ||
                       path == "/std/collections/map/at" ||
                       path == "/std/collections/map/at_unsafe";
              };
              return isAccessHelperPath(candidate.name) ||
                     isAccessHelperPath(statementsExprHelpers.resolveDirectHelperPath(candidate)) ||
                     isAccessHelperPath(resolveExprPath(candidate));
            };
            if (expr.args.size() == 1 &&
                methodCalleeFirstParameterIsStruct() &&
                isWrapperReturnedKeyValueAccessCall(expr.args.front())) {
              error = "struct parameter type mismatch";
              return false;
            }
            if (!emitInlineDefinitionCall(expr, *methodCallee, localsIn, true)) {
              return false;
            }
            error = priorError;
            return true;
          }
          error = priorError;
        }
        if (!expr.isMethodCall && statementsExprHelpers.hasKeyValueEntryCtorArgs(expr) &&
            statementsExprHelpers.isCanonicalKeyValueConstructorPath(resolveExprPath(expr))) {
          error = "native backend does not support variadic entry map constructors";
          return false;
        }
        if (!expr.isMethodCall && isSimpleCallName(expr, "capacity") &&
            expr.args.size() == 1) {
          std::string receiverCollectionName;
          const bool isDirectVectorConstructor =
              expr.args.front().kind == Expr::Kind::Call &&
              getBuiltinCollectionName(expr.args.front(), receiverCollectionName) &&
              receiverCollectionName == "vector";
          const auto targetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(
                  expr.args.front(),
                  localsIn,
                  [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                    return statementsExprHelpers.resolveHelperReturnedArrayVectorAccessTargetInfo(
                        targetCallExpr, targetInfoOut, localsIn);
                  },
                  semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex);
          const std::string structPath = inferStructExprPath(expr.args.front(), localsIn);
          const bool isSemanticVectorTarget =
              (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
              statementsExprHelpers.isCollectionVectorRecordTypePath(structPath);
          if (!isDirectVectorConstructor &&
              (expr.args.front().kind == Expr::Kind::Call ||
               isSemanticVectorTarget)) {
            if (const Definition *directVectorMetadataCallee =
                    statementsExprHelpers.resolveDirectHelperDefinition(expr);
                directVectorMetadataCallee != nullptr &&
                !directVectorMetadataCallee->parameters.empty()) {
              std::string receiverTypeName;
              std::vector<std::string> receiverTemplateArgs;
              if (extractFirstBindingTypeTransform(
                      directVectorMetadataCallee->parameters.front(),
                      receiverTypeName,
                      receiverTemplateArgs) &&
                  (normalizeCollectionBindingTypeName(receiverTypeName) ==
                       "map" ||
                   normalizeCollectionBindingTypeName(receiverTypeName) ==
                       "vector")) {
                return emitInlineDefinitionCall(
                    expr, *directVectorMetadataCallee, localsIn, true);
              }
            }
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
        }
        std::string resolvedKeyValueInsertHelperName;
        const std::string exprPath = resolveExprPath(expr);
        if (!expr.isMethodCall &&
            ((statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(exprPath) &&
              statementsExprHelpers.resolveKeyValueHelperMemberName(exprPath, resolvedKeyValueInsertHelperName) &&
              (resolvedKeyValueInsertHelperName == "insert" ||
               resolvedKeyValueInsertHelperName == "insert_ref")) ||
             exprPath.rfind(collection_paths::memberPath(collection_paths::kMapFolder, "insert"), 0) == 0)) {
          if (const Definition *directCallee = statementsExprHelpers.resolveDirectHelperDefinition(expr);
              directCallee != nullptr) {
            if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
              return false;
            }
            return true;
          }
        }
        if (expr.isMethodCall && expr.args.size() == 2) {
          std::string vectorAccessName;
          if ((resolveVectorHelperAliasName(expr, vectorAccessName) ||
               getBuiltinArrayAccessName(expr, vectorAccessName) ||
               ((isSimpleCallName(expr, "at") ||
                 isSimpleCallName(expr, "at_unsafe")) &&
                (vectorAccessName = expr.name, true))) &&
              (vectorAccessName == "at" || vectorAccessName == "at_unsafe")) {
            if (const Definition *directVectorAccessCallee =
                    statementsExprHelpers.resolveDirectHelperDefinition(expr);
                directVectorAccessCallee != nullptr) {
              return emitInlineDefinitionCall(
                  expr, *directVectorAccessCallee, localsIn, true);
            }
            const auto arrayVectorTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    expr.args.front(),
                    localsIn,
                    {},
                    semanticProgram,
                    &callResolutionAdapters.semanticProductTargets.semanticIndex);
            const bool localVectorTarget =
                expr.args.front().kind == Expr::Kind::Name &&
                [&]() {
                  auto localIt = localsIn.find(expr.args.front().name);
                  return localIt != localsIn.end() &&
                         (localIt->second.kind == LocalInfo::Kind::Vector ||
                          localIt->second.referenceToVector ||
                          localIt->second.pointerToVector ||
                          statementsExprHelpers.isCollectionVectorRecordTypePath(localIt->second.structTypeName));
                }();
            if ((arrayVectorTargetInfo.isArrayOrVectorTarget &&
                 arrayVectorTargetInfo.isVectorTarget) ||
                localVectorTarget) {
              return ir_lowerer::emitArrayVectorIndexedAccess(
                  vectorAccessName,
                  expr.args.front(),
                  expr.args[1],
                  localsIn,
                  [&](const Expr &indexExpr, const LocalMap &indexLocals) {
                    return inferExprKind(indexExpr, indexLocals);
                  },
                  [&]() { return allocTempLocal(); },
                  [&](const Expr &nestedExpr, const LocalMap &nestedLocals) {
                    return emitExpr(nestedExpr, nestedLocals);
                  },
                  [&]() { emitArrayIndexOutOfBounds(); },
                  [&]() { return function.instructions.size(); },
                  [&](IrOpcode op, uint64_t imm) {
                    function.instructions.push_back({op, imm});
                  },
                  [&](size_t indexToPatch, uint64_t target) {
                    function.instructions[indexToPatch].imm = target;
                  },
                  error);
            }
          }
        }
        if (!expr.isMethodCall && expr.args.size() == 2) {
          std::string vectorAccessName;
          const std::string vectorAccessPath = resolveExprPath(expr);
          if (!statementsExprHelpers.isCanonicalKeyValueHelperFamilyPath(vectorAccessPath) &&
              (resolveVectorHelperAliasName(expr, vectorAccessName) ||
               getBuiltinArrayAccessName(expr, vectorAccessName) ||
               (vectorAccessPath == "/std/collections/vector/at" &&
                (vectorAccessName = "at", true)) ||
               (vectorAccessPath == "/std/collections/vector/at_unsafe" &&
                (vectorAccessName = "at_unsafe", true))) &&
              (vectorAccessName == "at" || vectorAccessName == "at_unsafe")) {
            // A same-path user definition overriding the canonical
            // /std/collections/vector/at(_unsafe) helper must win over the
            // native indexed-access fast path below - otherwise a
            // struct-returning override's call sites get the builtin
            // scalar-element access pattern instead of the user's own
            // body, corrupting the IR for any subsequent struct handling
            // (see TODO-4804). Mirrors the same guard already applied to
            // the method-call form immediately above.
            if (const Definition *directVectorAccessCallee =
                    statementsExprHelpers.resolveDirectHelperDefinition(expr);
                directVectorAccessCallee != nullptr) {
              return emitInlineDefinitionCall(
                  expr, *directVectorAccessCallee, localsIn, true);
            }
            const auto keyValueTargetInfo =
                statementsExprHelpers.resolveKeyValueAccessReceiverInfo(expr, expr.args.front(), localsIn);
            const auto arrayVectorTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    expr.args.front(),
                    localsIn,
                    {},
                    semanticProgram,
                    &callResolutionAdapters.semanticProductTargets.semanticIndex);
            if (!keyValueTargetInfo.isKeyValueTarget &&
                arrayVectorTargetInfo.isArrayOrVectorTarget) {
              return ir_lowerer::emitArrayVectorIndexedAccess(
                  vectorAccessName,
                  expr.args.front(),
                  expr.args[1],
                  localsIn,
                  {},
                  [&](const Expr &indexExpr, const LocalMap &indexLocals) {
                    return inferExprKind(indexExpr, indexLocals);
                  },
                  [&]() { return allocTempLocal(); },
                  [&](const Expr &nestedExpr, const LocalMap &nestedLocals) {
                    return emitExpr(nestedExpr, nestedLocals);
                  },
                  [&]() { emitArrayIndexOutOfBounds(); },
                  [&]() { return function.instructions.size(); },
                  [&](IrOpcode op, uint64_t imm) {
                    function.instructions.push_back({op, imm});
                  },
                  [&](size_t indexToPatch, uint64_t target) {
                    function.instructions[indexToPatch].imm = target;
                  },
                  error,
                  semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex);
            }
          }
        }
        std::string bareKeyValueAccessName;
        const std::string bareKeyValueAccessPath = resolveExprPath(expr);
        std::string bareKeyValueAccessLeaf = bareKeyValueAccessPath;
        if (const size_t leafStart = bareKeyValueAccessLeaf.find_last_of('/');
            leafStart != std::string::npos) {
          bareKeyValueAccessLeaf = bareKeyValueAccessLeaf.substr(leafStart + 1);
        }
        if (const size_t generatedSuffix = bareKeyValueAccessLeaf.find("__");
            generatedSuffix != std::string::npos) {
          bareKeyValueAccessLeaf.erase(generatedSuffix);
        }
        const bool isCanonicalBareKeyValueAccess =
            (bareKeyValueAccessPath.rfind("/std/collections/map/at", 0) == 0 ||
             bareKeyValueAccessPath.rfind("std/collections/map/at", 0) == 0);
        if (!expr.isMethodCall &&
            expr.args.size() == 2 &&
            ((getBuiltinArrayAccessName(expr, bareKeyValueAccessName) &&
              (bareKeyValueAccessName == "at" ||
               bareKeyValueAccessName == "at_unsafe")) ||
             (resolveKeyValueHelperAliasName(expr, bareKeyValueAccessName) &&
              (bareKeyValueAccessName == "at" ||
               bareKeyValueAccessName == "at_unsafe" ||
               bareKeyValueAccessName == "at_ref" ||
               bareKeyValueAccessName == "at_unsafe_ref")) ||
             (isCanonicalBareKeyValueAccess &&
              (bareKeyValueAccessName = bareKeyValueAccessLeaf, true)) ||
             ((isSimpleCallName(expr, "at") ||
               isSimpleCallName(expr, "at_unsafe")) &&
              (bareKeyValueAccessName = expr.name, true))) &&
            (bareKeyValueAccessName == "at" ||
             bareKeyValueAccessName == "at_ref" ||
             bareKeyValueAccessName == "at_unsafe" ||
             bareKeyValueAccessName == "at_unsafe_ref")) {
          if (bareKeyValueAccessName == "at_ref") {
            bareKeyValueAccessName = "at";
          } else if (bareKeyValueAccessName == "at_unsafe_ref") {
            bareKeyValueAccessName = "at_unsafe";
          }
          auto resolveAccessTargetInfo = [&](const Expr &receiverExpr) {
            return statementsExprHelpers.resolveKeyValueAccessReceiverInfo(expr, receiverExpr, localsIn);
          };
          auto targetInfo = resolveAccessTargetInfo(expr.args.front());
          size_t receiverArgIndex = 0;
          if (!targetInfo.isKeyValueTarget && expr.args.size() > 1) {
            auto alternateTargetInfo = resolveAccessTargetInfo(expr.args[1]);
            if (alternateTargetInfo.isKeyValueTarget) {
              targetInfo = std::move(alternateTargetInfo);
              receiverArgIndex = 1;
            }
          }
          if (isCanonicalBareKeyValueAccess && !targetInfo.isKeyValueTarget) {
            const auto arrayVectorTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    expr.args.front(),
                    localsIn,
                    {},
                    semanticProgram,
                    &callResolutionAdapters.semanticProductTargets.semanticIndex);
            if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
              return ir_lowerer::emitArrayVectorIndexedAccess(
                  bareKeyValueAccessName,
                  expr.args.front(),
                  expr.args[1],
                  localsIn,
                  [&](const Expr &indexExpr, const LocalMap &indexLocals) {
                    return inferExprKind(indexExpr, indexLocals);
                  },
                  [&]() { return allocTempLocal(); },
                  [&](const Expr &nestedExpr, const LocalMap &nestedLocals) {
                    return emitExpr(nestedExpr, nestedLocals);
                  },
                  [&]() { emitArrayIndexOutOfBounds(); },
                  [&]() { return function.instructions.size(); },
                  [&](IrOpcode op, uint64_t imm) {
                    function.instructions.push_back({op, imm});
                  },
                  [&](size_t indexToPatch, uint64_t target) {
                    function.instructions[indexToPatch].imm = target;
                  },
                  error);
            }
          }
          if (targetInfo.isKeyValueTarget) {
            Expr accessExpr = expr;
            if (receiverArgIndex != 0 && accessExpr.args.size() > receiverArgIndex) {
              std::swap(accessExpr.args[0], accessExpr.args[receiverArgIndex]);
            }
            if (bareKeyValueAccessName == "at" ||
                bareKeyValueAccessName == "at_unsafe") {
              if (accessExpr.args.front().kind == Expr::Kind::Call &&
                  !inferStructExprPath(expr, localsIn).empty()) {
                error = "struct parameter type mismatch";
                return false;
              }
              if (!ir_lowerer::emitKeyValueLookupAccess(
                      bareKeyValueAccessName,
                      targetInfo.keyValueKeyKind,
                      targetInfo.structTypeName,
                      accessExpr.args.front(),
                      accessExpr.args[1],
                      localsIn,
                      [&]() { return allocTempLocal(); },
                      [&](const Expr &nestedExpr,
                          const ir_lowerer::LocalMap &nestedLocals) {
                        return emitExpr(nestedExpr, nestedLocals);
                      },
                      resolveStringTableTarget,
                      [&](const Expr &nestedExpr,
                          const ir_lowerer::LocalMap &nestedLocals) {
                        return inferExprKind(nestedExpr, nestedLocals);
                      },
                      [&]() { emitMapKeyNotFound(); },
                      [&]() { return function.instructions.size(); },
                      [&](IrOpcode op, uint64_t imm) {
                        function.instructions.push_back({op, imm});
                      },
                      [&](size_t indexToPatch, uint64_t target) {
                        function.instructions[indexToPatch].imm = target;
                      },
                      error)) {
                return false;
              }
              return true;
            }
            const std::string priorError = error;
            if (const Definition *directCallee =
                    statementsExprHelpers.resolveDirectHelperDefinition(accessExpr);
                directCallee != nullptr && !isCanonicalBareKeyValueAccess) {
              error = priorError;
              return emitInlineDefinitionCall(
                  accessExpr, *directCallee, localsIn, true);
            }
            std::string receiverStructPath = targetInfo.structTypeName;
            if (receiverStructPath.empty()) {
              receiverStructPath =
                  inferStructExprPath(accessExpr.args.front(), localsIn);
            }
            Expr methodExpr = accessExpr;
            methodExpr.name = statementsExprHelpers.keyValueImplementationMethodSpelling(
                receiverStructPath, bareKeyValueAccessName);
            methodExpr.namespacePrefix.clear();
            methodExpr.isMethodCall = true;
            methodExpr.semanticNodeId = 0;
            auto emitAccessMethodCall =
                [&](const Definition &methodCallee) -> bool {
              if (!receiverStructPath.empty() &&
                  methodCallee.parameters.size() + 1 == methodExpr.args.size()) {
                Definition calleeWithThis = methodCallee;
                calleeWithThis.isNested = false;
                calleeWithThis.parameters.insert(
                    calleeWithThis.parameters.begin(),
                    ir_lowerer::makeStructHelperThisParam(
                        receiverStructPath,
                        definitionHasTransform(methodCallee, "mut")));
                return emitInlineDefinitionCall(
                    methodExpr, calleeWithThis, localsIn, true);
              }
              return emitInlineDefinitionCall(
                  methodExpr, methodCallee, localsIn, true);
            };
            if (const Definition *methodCallee =
                    resolveMethodCallDefinition(methodExpr, localsIn);
                methodCallee != nullptr) {
              error = priorError;
              return emitAccessMethodCall(*methodCallee);
            }
            if (!receiverStructPath.empty()) {
              Expr methodLookup;
              methodLookup.kind = Expr::Kind::Call;
              methodLookup.name = receiverStructPath + "/" + methodExpr.name;
              if (const Definition *methodCallee =
                      resolveDefinitionCall(methodLookup);
                  methodCallee != nullptr) {
                error = priorError;
                return emitAccessMethodCall(*methodCallee);
              }
            }
	            error = priorError;
	            return emitExpr(methodExpr, localsIn);
	          }
	        }
	        if (!expr.isMethodCall && expr.args.size() == 1) {
	          std::string vectorMetadataHelperName;
	          const std::string vectorMetadataPath = resolveExprPath(expr);
	          if (((resolveVectorHelperAliasName(expr, vectorMetadataHelperName) &&
	              (vectorMetadataHelperName == "count" ||
	               vectorMetadataHelperName == "capacity")) ||
		               (vectorMetadataPath == "/std/collections/vector/count" &&
		                (vectorMetadataHelperName = "count", true)) ||
		               (vectorMetadataPath == "/std/collections/vector/capacity" &&
		                (vectorMetadataHelperName = "capacity", true)))) {
              if (const Definition *directVectorMetadataCallee =
                      statementsExprHelpers.resolveDirectHelperDefinition(expr);
                  directVectorMetadataCallee != nullptr &&
                  !directVectorMetadataCallee->parameters.empty()) {
                std::string receiverTypeName;
                std::vector<std::string> receiverTemplateArgs;
                if (extractFirstBindingTypeTransform(
                        directVectorMetadataCallee->parameters.front(),
                        receiverTypeName,
                        receiverTemplateArgs) &&
                    (normalizeCollectionBindingTypeName(receiverTypeName) ==
                         "map" ||
                     normalizeCollectionBindingTypeName(receiverTypeName) ==
                         "vector")) {
                  return emitInlineDefinitionCall(
                      expr, *directVectorMetadataCallee, localsIn, true);
                }
              }
		            auto metadataTargetReturnsString = [&]() {
		              if (vectorMetadataHelperName != "count" ||
		                  semanticProgram == nullptr) {
		                return false;
		              }
		              const Expr &targetExpr = expr.args.front();
		              if (inferExprKind(targetExpr, localsIn) ==
		                  LocalInfo::ValueKind::String) {
		                return true;
		              }
		              const auto *queryFact =
		                  ir_lowerer::findSemanticProductQueryFact(
		                      semanticProgram,
		                      callResolutionAdapters.semanticProductTargets
		                          .semanticIndex,
		                      targetExpr);
		              if (queryFact == nullptr) {
		                return false;
		              }
		              auto resolveFactTypeText = [&](SymbolId typeTextId,
		                                              const std::string &fallback) {
		                if (typeTextId != InvalidSymbolId) {
		                  const std::string resolvedTypeText = std::string(
		                      semanticProgramResolveCallTargetString(
		                          *semanticProgram, typeTextId));
		                  if (!resolvedTypeText.empty()) {
		                    return trimTemplateTypeText(resolvedTypeText);
		                  }
		                }
		                return trimTemplateTypeText(fallback);
		              };
		              const std::string queryType = resolveFactTypeText(
		                  queryFact->queryTypeTextId, queryFact->queryTypeText);
		              const std::string bindingType = resolveFactTypeText(
		                  queryFact->bindingTypeTextId,
		                  queryFact->bindingTypeText);
		              return queryType == "string" || queryType == "/string" ||
		                     bindingType == "string" || bindingType == "/string";
		            };
		            if (metadataTargetReturnsString()) {
		              if (const Definition *stringCountCallee =
		                      statementsExprHelpers.findDirectHelperDefinition("/string/count");
		                  stringCountCallee != nullptr) {
		                Expr stringCountExpr = expr;
		                stringCountExpr.name = "/string/count";
		                stringCountExpr.namespacePrefix.clear();
		                stringCountExpr.semanticNodeId = 0;
		                return emitInlineDefinitionCall(
		                    stringCountExpr, *stringCountCallee, localsIn, true);
		              }
		              if (!emitExpr(expr.args.front(), localsIn)) {
		                return false;
		              }
		              function.instructions.push_back({IrOpcode::LoadStringLength, 0});
		              return true;
		            }
		            if (!emitExpr(expr.args.front(), localsIn)) {
		              return false;
		            }
	            if (vectorMetadataHelperName == "capacity") {
	              function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
	              function.instructions.push_back({IrOpcode::AddI64, 0});
	            }
	            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
	            return true;
	          }
	        }
	        error =
	            "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
	            resolveExprPath(expr) + ", name=" + expr.name +
            ", args=" + std::to_string(expr.args.size()) +
            ", method=" + std::string(expr.isMethodCall ? "true" : "false") + ")";
        return false;
      }
      default:
        error = "native backend only supports literals, names, and calls";
        return false;
    }
  };

  emitPrintArg = [&](const Expr &arg, const LocalMap &localsIn, const PrintBuiltin &builtin) -> bool {
    uint64_t flags = encodePrintFlags(builtin.newline, builtin.target == PrintTarget::Err);
    auto isEntryArgsPrintTarget = [&](const Expr &target) {
      if (isEntryArgsName(target, localsIn)) {
        return true;
      }
      if (!hasEntryArgs || semanticProgram == nullptr || target.kind != Expr::Kind::Name ||
          target.name != entryArgsName) {
        return false;
      }
      const auto *bindingFact = findSemanticProductBindingFact(
          callResolutionAdapters.semanticProductTargets.semanticIndex, target);
      if (bindingFact == nullptr) {
        return false;
      }
      return bindingFact->name == entryArgsName &&
             bindingFact->bindingTypeText == "array<string>" &&
             (bindingFact->siteKind == "parameter" ||
              bindingFact->siteKind == "parameter-reference");
    };
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      const bool isBuiltinAccess =
          getBuiltinArrayAccessName(arg, accessName) ||
          ([&]() {
            const std::string resolvedAccessPath = resolveExprPath(arg);
            if (resolvedAccessPath == "/at") {
              accessName = "at";
              return true;
            }
            if (resolvedAccessPath == "/at_unsafe") {
              accessName = "at_unsafe";
              return true;
            }
            return false;
          })();
      if (isBuiltinAccess) {
        if (arg.args.size() != 2) {
          error = accessName + " requires exactly two arguments";
          return false;
        }
        if (isEntryArgsPrintTarget(arg.args.front())) {
          LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
          if (!resolveValidatedAccessIndexKind(
                  arg.args[1],
                  localsIn,
                  accessName,
                  inferExprKind,
                  indexKind,
                  error,
                  semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex)) {
            return false;
          }

          const int32_t indexLocal = allocTempLocal();
          if (!emitExpr(arg.args[1], localsIn)) {
            return false;
          }
          function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});

          if (accessName == "at") {
            if (indexKind != LocalInfo::ValueKind::UInt64) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
              function.instructions.push_back({pushZeroForIndex(indexKind), 0});
              function.instructions.push_back({cmpLtForIndex(indexKind), 0});
              size_t jumpNonNegative = function.instructions.size();
              function.instructions.push_back({IrOpcode::JumpIfZero, 0});
              emitArrayIndexOutOfBounds();
              size_t nonNegativeIndex = function.instructions.size();
              function.instructions[jumpNonNegative].imm = static_cast<int32_t>(nonNegativeIndex);
            }

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
            function.instructions.push_back({IrOpcode::PushArgc, 0});
            function.instructions.push_back({cmpGeForIndex(indexKind), 0});
            size_t jumpInRange = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            size_t inRangeIndex = function.instructions.size();
            function.instructions[jumpInRange].imm = static_cast<int32_t>(inRangeIndex);
          }

          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
          IrOpcode printOp = (accessName == "at_unsafe") ? IrOpcode::PrintArgvUnsafe : IrOpcode::PrintArgv;
          function.instructions.push_back({printOp, flags});
          return true;
        }
      }
    }
    if (arg.kind == Expr::Kind::StringLiteral) {
      std::string decoded;
      if (!ir_lowerer::parseLowererStringLiteral(arg.stringValue, decoded, error)) {
        return false;
      }
      int32_t index = internString(decoded);
      function.instructions.push_back(
          {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(index), flags)});
      return true;
    }
    if (arg.kind == Expr::Kind::Name) {
      auto it = localsIn.find(arg.name);
      if (it == localsIn.end()) {
        error = "native backend does not know identifier: " + arg.name;
        return false;
      }
      const LocalInfo::ValueKind printNameKind = inferExprKind(arg, localsIn);
      if ((printNameKind == LocalInfo::ValueKind::Unknown ||
           printNameKind == LocalInfo::ValueKind::String) &&
          it->second.valueKind == LocalInfo::ValueKind::String) {
        if (it->second.stringSource == LocalInfo::StringSource::TableIndex) {
          if (it->second.stringIndex < 0) {
            error = "native backend missing string data for: " + arg.name;
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::PrintString, encodePrintStringImm(static_cast<uint64_t>(it->second.stringIndex), flags)});
          return true;
        }
        if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          IrOpcode printOp = it->second.argvChecked ? IrOpcode::PrintArgv : IrOpcode::PrintArgvUnsafe;
          function.instructions.push_back({printOp, flags});
          return true;
        }
        if (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex) {
          function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index)});
          function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
          return true;
        }
      }
    }
    if (!emitExpr(arg, localsIn)) {
      return false;
    }
    LocalInfo::ValueKind kind = inferExprKind(arg, localsIn);
    if (kind == LocalInfo::ValueKind::String) {
      function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int64) {
      function.instructions.push_back({IrOpcode::PrintI64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::UInt64) {
      function.instructions.push_back({IrOpcode::PrintU64, flags});
      return true;
    }
    if (kind == LocalInfo::ValueKind::Int32 || kind == LocalInfo::ValueKind::Bool) {
      function.instructions.push_back({IrOpcode::PrintI32, flags});
      return true;
    }
    if (arg.kind == Expr::Kind::Call) {
      const size_t slashPos = arg.name.find_last_of('/');
      const std::string whyLeaf =
          slashPos == std::string::npos ? arg.name : arg.name.substr(slashPos + 1);
      if (whyLeaf == "why") {
        function.instructions.push_back({IrOpcode::PrintStringDynamic, flags});
        return true;
      }
    }
    error = builtin.name + " requires an integer/bool or string literal/binding argument";
    return false;
  };
