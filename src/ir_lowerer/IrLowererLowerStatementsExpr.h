        auto resolveDirectHelperPath = [&](const Expr &callExpr) {
          if (!callExpr.name.empty() && callExpr.name.front() == '/') {
            return callExpr.name;
          }
          if (!callExpr.namespacePrefix.empty()) {
            std::string scoped = callExpr.namespacePrefix;
            if (!scoped.empty() && scoped.front() != '/') {
              scoped.insert(scoped.begin(), '/');
            }
            return scoped + "/" + callExpr.name;
          }
          return callExpr.name;
        };
        auto matchesDirectHelperDefinitionFamilyPath =
            [&](const std::string &candidatePath, const Definition &callee) {
              return !candidatePath.empty() &&
                     (callee.fullPath == candidatePath ||
                      callee.fullPath.rfind(candidatePath + "__", 0) == 0 ||
                      callee.fullPath.rfind(candidatePath + "<", 0) == 0 ||
                      normalizeCollectionHelperPath(candidatePath) ==
                          normalizeCollectionHelperPath(callee.fullPath));
            };
        auto isDirectHelperDefinitionFamily = [&](const Expr &callExpr,
                                                  const Definition &callee) {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          if (matchesDirectHelperDefinitionFamilyPath(rawPath, callee)) {
            return true;
          }
          const std::string resolvedPath = resolveExprPath(callExpr);
          return resolvedPath != rawPath &&
                 matchesDirectHelperDefinitionFamilyPath(resolvedPath, callee);
        };
        auto findDirectHelperDefinition = [&](const std::string &rawPath) -> const Definition * {
          auto defIt = defMap.find(rawPath);
          if (defIt != defMap.end()) {
            return defIt->second;
          }
          auto matchesGeneratedLeafDefinition = [&](const std::string &path,
                                                    const char *marker,
                                                    size_t markerSize) {
            return path.rfind(rawPath, 0) == 0 &&
                   path.compare(rawPath.size(), markerSize, marker) == 0 &&
                   path.find('/', rawPath.size() + markerSize) ==
                       std::string::npos;
          };
          for (const auto &[path, def] : defMap) {
            if (def == nullptr) {
              continue;
            }
            if (matchesGeneratedLeafDefinition(path, "__t", 3) ||
                matchesGeneratedLeafDefinition(path, "__ov", 4) ||
                matchesGeneratedLeafDefinition(path, "<", 1)) {
              return def;
            }
          }
          return nullptr;
        };
        auto isExplicitExperimentalVectorConstructorHelper =
            [](const std::string &path) {
              return path == "/std/collections/experimental_vector/vector" ||
                     path == "std/collections/experimental_vector/vector";
            };
        auto resolveDirectHelperDefinition = [&](const Expr &targetExpr) -> const Definition * {
          const std::string rawPath = resolveDirectHelperPath(targetExpr);
          if (isExplicitExperimentalVectorConstructorHelper(rawPath)) {
            if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
                rawDef != nullptr) {
              return rawDef;
            }
          }
          if (const Definition *callee = resolveDefinitionCall(targetExpr);
              callee != nullptr) {
            return callee;
          }
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr) {
            return rawDef;
          }
          const std::string resolvedPath = resolveExprPath(targetExpr);
          if (resolvedPath != rawPath) {
            return findDirectHelperDefinition(resolvedPath);
          }
          return nullptr;
        };
        auto stripGeneratedHelperSuffix = [](std::string helperPath) {
          const size_t leafStart = helperPath.find_last_of('/');
          const size_t generatedSuffix =
              helperPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
          if (generatedSuffix != std::string::npos) {
            helperPath.erase(generatedSuffix);
          }
          return helperPath;
        };
        auto extractHelperTail = [&](std::string helperPath) {
          helperPath = stripGeneratedHelperSuffix(std::move(helperPath));
          const size_t slash = helperPath.find_last_of('/');
          if (slash != std::string::npos) {
            helperPath = helperPath.substr(slash + 1);
          }
          return helperPath;
        };
        auto isDirectCollectionHelperPath = [](const std::string &path) {
          return path.rfind("/array/", 0) == 0 ||
                 path.rfind("/map/", 0) == 0 ||
                 path.rfind("/std/collections/vector/", 0) == 0 ||
                 path.rfind("/std/collections/experimental_vector/", 0) == 0 ||
                 path.rfind("/std/collections/map/", 0) == 0 ||
                 path.rfind("/std/collections/experimental_map/", 0) == 0;
        };
        auto hasMapEntryCtorArgs = [](const Expr &callExpr) {
          auto isMapEntryCallExpr = [](const Expr &candidate) {
            if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
              return false;
            }
            std::string normalizedName;
            if (!candidate.name.empty() && candidate.name.front() == '/') {
              normalizedName = candidate.name.substr(1);
            } else if (!candidate.namespacePrefix.empty()) {
              normalizedName = candidate.namespacePrefix;
              if (!normalizedName.empty() && normalizedName.front() == '/') {
                normalizedName.erase(normalizedName.begin());
              }
              normalizedName += "/" + candidate.name;
            } else {
              normalizedName = candidate.name;
            }
            const auto generatedSuffix = normalizedName.find("__");
            if (generatedSuffix != std::string::npos) {
              normalizedName.erase(generatedSuffix);
            }
            return normalizedName == "map/entry" ||
                   normalizedName == "std/collections/map/entry" ||
                   normalizedName == "std/collections/experimental_map/entry";
          };
          for (const auto &arg : callExpr.args) {
            if (isMapEntryCallExpr(arg)) {
              return true;
            }
          }
          return false;
        };
        auto isInternalSoaHelperFamilyName = [&](const std::string &helperName) {
          return helperName.rfind("soaColumn", 0) == 0 ||
                 helperName.rfind("soaColumns", 0) == 0 ||
                 helperName.rfind("SoaColumn", 0) == 0 ||
                 helperName.rfind("SoaColumns", 0) == 0;
        };
        auto isInternalSoaHelperFamilyPath = [&](const std::string &path) {
          return isInternalSoaHelperFamilyName(
              extractHelperTail(normalizeCollectionHelperPath(path)));
        };
        auto isSamePathSoaHelperPath = [&](const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath.rfind("/soa_vector/", 0) == 0 ||
                 normalizedPath == "/to_aos" ||
                 normalizedPath == "/to_aos_ref";
        };
        auto isSoaWrapperHelperFamilyPath = [&](const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath.rfind("/std/collections/soa_vector/", 0) == 0 ||
                 normalizedPath.rfind("/std/collections/experimental_soa_vector/soaVector", 0) == 0 ||
                 normalizedPath.rfind("/std/collections/experimental_soa_vector_conversions/soaVector", 0) == 0;
        };
        auto isBuiltinMapInsertFamilyPath = [&](const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath == "/std/collections/map/insert_builtin";
        };
        auto findDirectInternalSoaDefinition = [&](const std::string &rawPath)
            -> const Definition * {
          const std::string helperName = extractHelperTail(rawPath);
          if (!isInternalSoaHelperFamilyName(helperName)) {
            return nullptr;
          }
          for (const auto &[path, def] : defMap) {
            if (def == nullptr ||
                path.rfind("/std/collections/internal_soa_storage/", 0) != 0) {
              continue;
            }
            if (extractHelperTail(path) == helperName) {
              return def;
            }
          }
          return nullptr;
        };
        auto findDirectSoaWrapperDefinition = [&](const Expr &callExpr,
                                                 const std::string &rawPath)
            -> const Definition * {
          const std::string normalizedRawPath = stripGeneratedHelperSuffix(rawPath);
          const bool isExperimentalSoaToAosCall =
              normalizedRawPath ==
              "/std/collections/experimental_soa_vector_conversions/soaVectorToAos";
          if (isExperimentalSoaToAosCall && !callExpr.args.empty()) {
            const std::string receiverStruct =
                inferStructExprPath(callExpr.args.front(), localsIn);
            if (normalizeCollectionBindingTypeName(receiverStruct) == "soa_vector") {
              if (const Definition *canonicalSoaToAos =
                      findDirectHelperDefinition("/std/collections/soa_vector/to_aos")) {
                return canonicalSoaToAos;
              }
            }
          }
          return findDirectHelperDefinition(rawPath);
        };
        auto findDirectEntryMapConstructorDefinition = [&](const Expr &callExpr)
            -> const Definition * {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          const std::string normalizedRawPath =
              normalizeCollectionHelperPath(rawPath);
          if (normalizedRawPath != "/map/map") {
            return nullptr;
          }
          for (const auto &[path, def] : defMap) {
            if (def == nullptr || def->parameters.empty() ||
                !isArgsPackBinding(def->parameters.front())) {
              continue;
            }
            if (normalizeCollectionHelperPath(path) == normalizedRawPath &&
                extractHelperTail(path) == "map") {
              return def;
            }
          }
          return nullptr;
        };
        auto findDirectStructDefinition = [&](const Expr &callExpr) -> const Definition * {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr && ir_lowerer::isStructDefinition(*rawDef)) {
            return rawDef;
          }
          std::string directStructPath;
          if (!resolveStructTypeName(callExpr.name, callExpr.namespacePrefix, directStructPath)) {
            return nullptr;
          }
          if (const Definition *structDef = findDirectHelperDefinition(directStructPath);
              structDef != nullptr && ir_lowerer::isStructDefinition(*structDef)) {
            return structDef;
          }
          return nullptr;
        };
        auto resolveHelperReturnedArrayVectorAccessTargetInfo =
            [&](const Expr &targetCallExpr,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              auto resolveSpecializedVectorElementKind =
                  [&](const std::string &typeText,
                      ir_lowerer::LocalInfo::ValueKind &elemKindOut) {
                    elemKindOut = ir_lowerer::LocalInfo::ValueKind::Unknown;
                    std::string normalized =
                        ir_lowerer::trimTemplateTypeText(typeText);
                    if (!normalized.empty() && normalized.front() != '/') {
                      normalized.insert(normalized.begin(), '/');
                    }
                    if (normalized.rfind(
                            "/std/collections/experimental_vector/Vector__",
                            0) != 0) {
                      return false;
                    }
                    Expr syntheticExpr;
                    syntheticExpr.kind = Expr::Kind::Call;
                    syntheticExpr.name = normalized;
                    const Definition *structDef =
                        resolveDefinitionCall(syntheticExpr);
                    if (structDef == nullptr ||
                        !ir_lowerer::isStructDefinition(*structDef)) {
                      return false;
                    }
                    for (const auto &fieldExpr : structDef->statements) {
                      if (!fieldExpr.isBinding || fieldExpr.name != "data") {
                        continue;
                      }
                      std::string typeName;
                      std::vector<std::string> templateArgs;
                      if (!ir_lowerer::extractFirstBindingTypeTransform(
                              fieldExpr, typeName, templateArgs) ||
                          ir_lowerer::normalizeCollectionBindingTypeName(
                              typeName) != "Pointer" ||
                          templateArgs.size() != 1) {
                        continue;
                      }
                      std::string elementType =
                          ir_lowerer::trimTemplateTypeText(
                              templateArgs.front());
                      if (!ir_lowerer::extractTopLevelUninitializedTypeText(
                              elementType, elementType)) {
                        continue;
                      }
                      elemKindOut =
                          ir_lowerer::valueKindFromTypeName(elementType);
                      return elemKindOut !=
                             ir_lowerer::LocalInfo::ValueKind::Unknown;
                    }
                    return false;
                  };
              const std::string inferredReceiverStruct =
                  inferStructExprPath(targetCallExpr, localsIn);
              if (inferredReceiverStruct.rfind(
                      "/std/collections/experimental_vector/Vector__", 0) ==
                  0) {
                ir_lowerer::LocalInfo::ValueKind elemKind;
                if (!resolveSpecializedVectorElementKind(inferredReceiverStruct,
                                                        elemKind)) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = true;
                targetInfoOut.elemKind = elemKind;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              if (inferredReceiverStruct.rfind(
                      "/std/collections/experimental_soa_vector/SoaVector__", 0) == 0 ||
                  normalizeCollectionBindingTypeName(inferredReceiverStruct) ==
                      "soa_vector") {
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = false;
                targetInfoOut.isSoaVector = true;
                targetInfoOut.structTypeName = inferredReceiverStruct;
                return true;
              }
              const Definition *callee =
                  resolveDirectHelperDefinition(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee,
                                                             collectionName,
                                                             collectionArgs)) {
                return false;
              }
              if ((collectionName != "array" && collectionName != "vector" &&
                   collectionName != "soa_vector") ||
                  collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.isSoaVector = (collectionName == "soa_vector");
              targetInfoOut.elemKind =
                  ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              if (targetInfoOut.isSoaVector) {
                std::string elementTypeName =
                    trimTemplateTypeText(collectionArgs.front());
                if (!elementTypeName.empty() &&
                    elementTypeName.front() == '/') {
                  elementTypeName.erase(elementTypeName.begin());
                }
                targetInfoOut.structTypeName =
                    specializedExperimentalSoaVectorStructPathForElementType(
                        elementTypeName);
              }
              return true;
            };
        if (!expr.isMethodCall) {
          const std::string rawPath = resolveDirectHelperPath(expr);
          std::string experimentalVectorElementType;
          const bool isExperimentalVectorConstructorAlias =
              getExperimentalVectorConstructorElementTypeAlias(
                  expr, experimentalVectorElementType) ||
              getExperimentalVectorConstructorElementTypeAliasFromPath(
                  resolveExprPath(expr), experimentalVectorElementType);
          if (isExperimentalVectorConstructorAlias) {
            Expr rewrittenVectorCtor = expr;
            rewrittenVectorCtor.name = "/std/collections/experimental_vector/vector";
            rewrittenVectorCtor.namespacePrefix.clear();
            rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};
            if (const Definition *vectorCtor =
                    resolveDirectHelperDefinition(rewrittenVectorCtor)) {
              if (!emitInlineDefinitionCall(
                      rewrittenVectorCtor, *vectorCtor, localsIn, true)) {
                return false;
              }
              return true;
            }
          }
          const Definition *directCallee = resolveDefinitionCall(expr);
          if (directCallee != nullptr &&
              isSoaWrapperHelperFamilyPath(rawPath)) {
            if (const Definition *preferredSoaWrapper =
                    findDirectSoaWrapperDefinition(expr, rawPath)) {
              directCallee = preferredSoaWrapper;
            }
          }
          if (directCallee == nullptr &&
              hasMapEntryCtorArgs(expr) &&
              (rawPath.rfind("/map/", 0) == 0 ||
               rawPath.rfind("/std/collections/map/", 0) == 0 ||
               rawPath.rfind("/std/collections/experimental_map/", 0) == 0)) {
            directCallee = findDirectEntryMapConstructorDefinition(expr);
          }
          if (directCallee == nullptr &&
              isInternalSoaHelperFamilyPath(rawPath)) {
            directCallee = findDirectInternalSoaDefinition(rawPath);
          }
          if (directCallee == nullptr) {
            directCallee = findDirectStructDefinition(expr);
          }
          if (directCallee == nullptr &&
              isSoaWrapperHelperFamilyPath(rawPath)) {
            directCallee = findDirectSoaWrapperDefinition(expr, rawPath);
          }
          const std::string resolvedExprPath = resolveExprPath(expr);
          if (directCallee == nullptr && isDirectCollectionHelperPath(rawPath)) {
            directCallee = findDirectHelperDefinition(rawPath);
          }
          if (directCallee == nullptr && isDirectCollectionHelperPath(resolvedExprPath)) {
            directCallee = findDirectHelperDefinition(resolvedExprPath);
          }
          auto isExperimentalVectorReceiver = [&](const Expr &receiver) {
            if (receiver.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiver.name);
              if (localIt != localsIn.end() &&
                  (localIt->second.structTypeName == "/std/collections/experimental_vector/Vector" ||
                   localIt->second.structTypeName.rfind(
                       "/std/collections/experimental_vector/Vector__", 0) == 0)) {
                return true;
              }
            }
            const std::string structPath = inferStructExprPath(receiver, localsIn);
            return structPath == "/std/collections/experimental_vector/Vector" ||
                   structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
          };
          auto emitVectorHeaderFieldLoad = [&](int32_t ptrLocal, int32_t slotOffset) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            if (slotOffset != 0) {
              function.instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
              function.instructions.push_back({IrOpcode::AddI64, 0});
            }
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          };
          auto emitVectorHeaderFieldStore = [&](int32_t ptrLocal, int32_t slotOffset, int32_t valueLocal) {
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
            if (slotOffset != 0) {
              function.instructions.push_back(
                  {IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
              function.instructions.push_back({IrOpcode::AddI64, 0});
            }
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::StoreIndirect, 0});
            function.instructions.push_back({IrOpcode::Pop, 0});
          };
          auto emitVectorHeaderBoundsTrapIfStackTrue = [&]() {
            const size_t okJump = function.instructions.size();
            function.instructions.push_back({IrOpcode::JumpIfZero, 0});
            emitArrayIndexOutOfBounds();
            function.instructions[okJump].imm = function.instructions.size();
          };
          auto emitExperimentalVectorHeaderSetter = [&]() -> int {
            if (!expr.isMethodCall || expr.args.size() != 2 ||
                (!isSimpleCallName(expr, "set_field_count") &&
                 !isSimpleCallName(expr, "set_field_capacity")) ||
                !isExperimentalVectorReceiver(expr.args.front())) {
              return -1;
            }
            const int32_t ptrLocal = allocTempLocal();
            if (!emitExpr(expr.args.front(), localsIn)) {
              return 0;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
            const int32_t valueLocal = allocTempLocal();
            if (!emitExpr(expr.args[1], localsIn)) {
              return 0;
            }
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});

            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 0});
            function.instructions.push_back({IrOpcode::CmpLtI32, 0});
            emitVectorHeaderBoundsTrapIfStackTrue();

            if (isSimpleCallName(expr, "set_field_count")) {
              function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
              emitVectorHeaderFieldLoad(ptrLocal, 1);
              function.instructions.push_back({IrOpcode::CmpGtI32, 0});
              emitVectorHeaderBoundsTrapIfStackTrue();
              emitVectorHeaderFieldStore(ptrLocal, 0, valueLocal);
              return 1;
            }

            emitVectorHeaderFieldLoad(ptrLocal, 0);
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::CmpGtI32, 0});
            emitVectorHeaderBoundsTrapIfStackTrue();
            function.instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
            function.instructions.push_back({IrOpcode::PushI32, 1073741823});
            function.instructions.push_back({IrOpcode::CmpGtI32, 0});
            emitVectorHeaderBoundsTrapIfStackTrue();
            emitVectorHeaderFieldStore(ptrLocal, 1, valueLocal);
            return 1;
          };
          if (const auto setterResult = emitExperimentalVectorHeaderSetter();
              setterResult >= 0) {
            return setterResult != 0;
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
            constexpr std::string_view internalSoaPrefix =
                "std/collections/internal_soa_storage/";
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
                   "/std/collections/internal_soa_storage/SoaColumn", 0) == 0 ||
               directCallee->fullPath.rfind(
                   "/std/collections/internal_soa_storage/SoaFieldView", 0) == 0);
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
          if (directCallee != nullptr) {
            if (ir_lowerer::isStructDefinition(*directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (!isInternalSoaMetadataMethod &&
                directCallee->fullPath.rfind("/std/collections/internal_soa_storage/", 0) == 0 &&
                isInternalSoaHelperFamilyPath(directCallee->fullPath)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            const bool isVisibleSamePathSoaHelper =
                isSamePathSoaHelperPath(rawPath) &&
                isDirectHelperDefinitionFamily(expr, *directCallee);
            const bool isResolvedSoaWrapperHelper =
                isSoaWrapperHelperFamilyPath(directCallee->fullPath);
            if (isResolvedSoaWrapperHelper || isVisibleSamePathSoaHelper) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (isBuiltinMapInsertFamilyPath(rawPath) ||
                isBuiltinMapInsertFamilyPath(directCallee->fullPath)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if ((rawPath.rfind("/array/", 0) == 0 ||
                 resolvedExprPath.rfind("/array/", 0) == 0 ||
                 directCallee->fullPath.rfind("/array/", 0) == 0) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if ((rawPath.rfind("/std/collections/vector/", 0) == 0 ||
                 rawPath.rfind("/std/collections/experimental_vector/", 0) == 0 ||
                 directCallee->fullPath.rfind("/std/collections/vector/", 0) == 0 ||
                 directCallee->fullPath.rfind("/std/collections/experimental_vector/", 0) == 0) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if (hasMapEntryCtorArgs(expr) &&
                extractHelperTail(normalizeCollectionHelperPath(directCallee->fullPath)) ==
                    "map" &&
                (rawPath.rfind("/map/", 0) == 0 ||
                 rawPath.rfind("/std/collections/map/", 0) == 0 ||
                 rawPath.rfind("/std/collections/experimental_map/", 0) == 0 ||
                 resolvedExprPath.rfind("/map/", 0) == 0 ||
                 resolvedExprPath.rfind("/std/collections/map/", 0) == 0 ||
                 resolvedExprPath.rfind("/std/collections/experimental_map/", 0) == 0) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            std::string helperName;
            const bool hasMapHelperAlias = resolveMapHelperAliasName(expr, helperName);
            if (!hasMapHelperAlias) {
              const size_t leafStart = rawPath.find_last_of('/');
              std::string helperLeaf =
                  leafStart == std::string::npos ? rawPath : rawPath.substr(leafStart + 1);
              const size_t generatedSuffix = helperLeaf.find("__");
              if (generatedSuffix != std::string::npos) {
                helperLeaf.erase(generatedSuffix);
              }
              if (helperLeaf == "mapCount" || helperLeaf == "mapCountRef") {
                helperName = "count";
              } else if (helperLeaf == "mapContains" || helperLeaf == "mapContainsRef") {
                helperName = "contains";
              } else if (helperLeaf == "mapTryAt" || helperLeaf == "mapTryAtRef" ||
                         helperLeaf == "tryAt" || helperLeaf == "tryAt_ref") {
                helperName = "tryAt";
              } else if (helperLeaf == "mapInsert" || helperLeaf == "mapInsertRef") {
                helperName = "insert";
              }
            }
            if (!helperName.empty() &&
                (helperName == "count" || helperName == "contains" ||
                 helperName == "tryAt" || helperName == "insert" ||
                 helperName == "insert_ref") &&
                (rawPath.rfind("/map/", 0) == 0 ||
                 rawPath.rfind("/std/collections/map/", 0) == 0 ||
                 rawPath.rfind("/std/collections/experimental_map/", 0) == 0) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              const bool isExplicitCanonicalMapHelperPath =
                  rawPath.rfind("/std/collections/map/", 0) == 0;
              const auto targetInfo =
                  !expr.args.empty()
                      ? ir_lowerer::resolveMapAccessTargetInfo(expr.args.front(), localsIn)
                      : ir_lowerer::MapAccessTargetInfo{};
              if (isExplicitCanonicalMapHelperPath &&
                  targetInfo.isMapTarget &&
                  !targetInfo.isWrappedMapTarget &&
                  (helperName == "count" || helperName == "contains" ||
                   helperName == "tryAt")) {
                // Keep direct canonical map helpers on the builtin/native
                // dispatch path for flat map storage receivers.
              } else if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              } else {
                return true;
              }
            }
            std::string accessName;
            if (getBuiltinArrayAccessName(expr, accessName) &&
                expr.args.size() == 2 &&
                rawPath.rfind("/std/collections/map/", 0) == 0 &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
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
        if (getBuiltinArrayAccessName(expr, accessName)) {
          const bool isMethodCallTempReceiver =
              expr.isMethodCall &&
              !expr.args.empty() &&
              expr.args.front().kind == Expr::Kind::Call &&
              (accessName == "at" || accessName == "at_unsafe");
          bool tempReceiverSupportsBuiltinAccess = false;
          if (isMethodCallTempReceiver) {
            ir_lowerer::ArrayVectorAccessTargetInfo targetInfo;
            tempReceiverSupportsBuiltinAccess =
                resolveHelperReturnedArrayVectorAccessTargetInfo(
                    expr.args.front(), targetInfo);
          }
          if (isMethodCallTempReceiver && !tempReceiverSupportsBuiltinAccess) {
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
                  {},
                  resolveHelperReturnedArrayVectorAccessTargetInfo,
                  inferExprKind,
                  isEntryArgsName,
                  allocTempLocal,
                  [&](const Expr &valueExpr, const LocalMap &valueLocals) {
                    return emitExpr(valueExpr, valueLocals);
                  },
                  emitStringIndexOutOfBounds,
                  emitMapKeyNotFound,
                  emitArrayIndexOutOfBounds,
                  [&]() { return function.instructions.size(); },
                  [&](IrOpcode opcode, uint64_t imm) {
                    function.instructions.push_back({opcode, imm});
                  },
                  [&](size_t instructionIndex, uint64_t imm) {
                    function.instructions[instructionIndex].imm = imm;
                  },
                  error)) {
            return false;
          }
          return true;
          }
        }

        const auto countAccessResult = tryEmitCountAccessCall(
            expr,
            localsIn,
            isArrayCountCall,
            isVectorCapacityCall,
            isStringCountCall,
            isEntryArgsName,
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              const bool isExperimentalVectorTarget =
                  structPath == "/std/collections/experimental_vector/Vector" ||
                  structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
              const bool isExperimentalMapTarget =
                  structPath == "/std/collections/experimental_map/Map" ||
                  structPath.rfind("/std/collections/experimental_map/Map__", 0) == 0;
              return targetInfo.isArrayOrVectorTarget || structPath == "/array" ||
                     structPath == "/vector" || structPath == "/Buffer" || structPath == "/map" ||
                     structPath == "/soa_vector" || isExperimentalVectorTarget ||
                     isExperimentalMapTarget;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     structPath == "/std/collections/experimental_vector/Vector" ||
                     structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     structPath == "/std/collections/experimental_vector/Vector" ||
                     structPath.rfind("/std/collections/experimental_vector/Vector__", 0) == 0;
            },
            inferExprKind,
            resolveStringTableTarget,
            [&](const Expr &valueExpr, const LocalMap &valueLocals) { return emitExpr(valueExpr, valueLocals); },
            [&](IrOpcode opcode, uint64_t imm) { function.instructions.push_back({opcode, imm}); },
            error);
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
                if (localStructPath == "/std/collections/internal_soa_storage/SoaColumn" ||
                    localStructPath == "/std/collections/internal_soa_storage/SoaFieldView") {
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
            return structPath == "/std/collections/internal_soa_storage/SoaColumn" ||
                   structPath == "/std/collections/internal_soa_storage/SoaFieldView";
          };
          auto emitInternalSoaMetadataBase = [&](const Expr &receiver) {
            if (receiver.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiver.name);
              if (localIt != localsIn.end() &&
                  isInternalSoaMetadataReceiver(receiver)) {
                function.instructions.push_back(
                    {localIt->second.kind == LocalInfo::Kind::Value
                         ? IrOpcode::AddressOfLocal
                         : IrOpcode::LoadLocal,
                     static_cast<uint64_t>(localIt->second.index)});
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
            if (isSimpleCallName(expr, "field_capacity")) {
              function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
              function.instructions.push_back({IrOpcode::AddI64, 0});
            }
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
          const std::string priorError = error;
          const Definition *methodCallee =
              resolveMethodCallDefinition(expr, localsIn);
          if (methodCallee == nullptr) {
            methodCallee = findDirectHelperDefinition(resolveExprPath(expr));
          }
          if (methodCallee != nullptr && expr.args.size() == 1 &&
              (isSimpleCallName(expr, "field_count") ||
               isSimpleCallName(expr, "field_capacity")) &&
              (methodCallee->fullPath.rfind(
                   "/std/collections/internal_soa_storage/SoaColumn", 0) == 0 ||
               methodCallee->fullPath.rfind(
                   "/std/collections/internal_soa_storage/SoaFieldView", 0) == 0)) {
            if (!emitInternalSoaMetadataBase(expr.args.front())) {
              return false;
            }
            if (isSimpleCallName(expr, "field_capacity")) {
              function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
              function.instructions.push_back({IrOpcode::AddI64, 0});
            }
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            error = priorError;
            return true;
          }
          if (methodCallee != nullptr) {
            if (!emitInlineDefinitionCall(expr, *methodCallee, localsIn, true)) {
              return false;
            }
            error = priorError;
            return true;
          }
          error = priorError;
        }
        if (!expr.isMethodCall && hasMapEntryCtorArgs(expr) &&
            normalizeCollectionHelperPath(resolveExprPath(expr)) == "/map/map") {
          error = "native backend does not support variadic entry map constructors";
          return false;
        }
        if (!expr.isMethodCall && isVectorBuiltinName(expr, "capacity") &&
            expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call) {
          std::string receiverCollectionName;
          const bool isDirectVectorConstructor =
              getBuiltinCollectionName(expr.args.front(), receiverCollectionName) &&
              receiverCollectionName == "vector";
          if (!isDirectVectorConstructor) {
            if (!emitExpr(expr.args.front(), localsIn)) {
              return false;
            }
            function.instructions.push_back({IrOpcode::PushI64, IrSlotBytes});
            function.instructions.push_back({IrOpcode::AddI64, 0});
            function.instructions.push_back({IrOpcode::LoadIndirect, 0});
            return true;
          }
        }
        if (!expr.isMethodCall &&
            (resolveExprPath(expr).find("mapCount") != std::string::npos ||
             resolveExprPath(expr).find("mapContains") != std::string::npos ||
             resolveExprPath(expr).find("mapTryAt") != std::string::npos ||
             resolveExprPath(expr).find("mapAt") != std::string::npos)) {
          if (const Definition *directCallee = resolveDirectHelperDefinition(expr);
              directCallee != nullptr) {
            if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
              return false;
            }
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
    if (arg.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(arg, accessName)) {
        if (arg.args.size() != 2) {
          error = accessName + " requires exactly two arguments";
          return false;
        }
        if (isEntryArgsName(arg.args.front(), localsIn)) {
          LocalInfo::ValueKind indexKind = normalizeIndexKind(inferExprKind(arg.args[1], localsIn));
          if (!isSupportedIndexKind(indexKind)) {
            error = "native backend requires integer indices for " + accessName;
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
      if (it->second.valueKind == LocalInfo::ValueKind::String) {
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
    error = builtin.name + " requires an integer/bool or string literal/binding argument";
    return false;
  };
