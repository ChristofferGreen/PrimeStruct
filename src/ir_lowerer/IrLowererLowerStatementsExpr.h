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
        auto stdCollectionsRoot = []() {
          return std::string("/std/collections");
        };
        auto collectionMemberRoot = [&](std::string_view collectionName) {
          return stdCollectionsRoot() + "/" + std::string(collectionName) + "/";
        };
        auto experimentalCollectionMemberRoot =
            [&](std::string_view collectionName) {
              return stdCollectionsRoot() + "/experimental_" +
                     std::string(collectionName) + "/";
            };
        auto experimentalCollectionMemberPath =
            [&](std::string_view collectionName, std::string_view memberName) {
              return experimentalCollectionMemberRoot(collectionName) +
                     std::string(memberName);
            };
        auto experimentalCollectionTypePath =
            [&](std::string_view collectionName, std::string_view typeName) {
              return experimentalCollectionMemberRoot(collectionName) +
                     std::string(typeName);
            };
        auto matchesGeneratedSpecializedType =
            [&](std::string_view path, std::string_view collectionName,
                std::string_view typeName) {
              const std::string typePath =
                  experimentalCollectionTypePath(collectionName, typeName);
              return path.rfind(typePath + "__", 0) == 0;
            };
        auto isCollectionVectorRecordTypePath = [&](std::string_view path) {
          return path == experimentalCollectionTypePath("vec" "tor", "Vector") ||
                 matchesGeneratedSpecializedType(path, "vector", "Vector");
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
            [&](const std::string &path) {
              const std::string slashPath =
                  experimentalCollectionMemberPath("vector", "vector");
              return path == slashPath || path == slashPath.substr(1);
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
        auto keyValueHelperMetadata = []() {
          return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
        };
        auto keyValueConstructorMetadata = []() {
          return findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors");
        };
        auto resolveKeyValueHelperMemberName = [&](std::string path,
                                                   std::string &helperNameOut) {
          helperNameOut.clear();
          const auto *metadata = keyValueHelperMetadata();
          if (metadata == nullptr) {
            return false;
          }
          path = normalizeCollectionHelperPath(path);
          return resolvePublishedStdlibSurfaceMemberName(
              path, metadata->id, helperNameOut);
        };
        auto keyValueHelperMethodSpelling = [&](std::string_view memberName) {
          const auto *metadata = keyValueHelperMetadata();
          if (metadata == nullptr) {
            return std::string(memberName);
          }
          for (const StdlibSurfaceMemberAlias &alias : metadata->memberAliases) {
            if (alias.memberName == memberName &&
                alias.spelling.find('/') == std::string_view::npos) {
              return std::string(alias.spelling);
            }
          }
          return std::string(memberName);
        };
        auto pascalCaseHelperMember = [](std::string_view memberName) {
          std::string result;
          bool capitalizeNext = true;
          for (const char ch : memberName) {
            if (ch == '_') {
              capitalizeNext = true;
              continue;
            }
            if (capitalizeNext) {
              result.push_back(static_cast<char>(
                  std::toupper(static_cast<unsigned char>(ch))));
              capitalizeNext = false;
            } else {
              result.push_back(ch);
            }
          }
          return result;
        };
        auto keyValueImplementationMethodSpelling =
            [&](const std::string &receiverStructPath,
                std::string_view memberName) {
          std::string receiverLeaf = extractHelperTail(receiverStructPath);
          constexpr std::string_view ValueSuffix = "Value";
          if (receiverLeaf.size() <= ValueSuffix.size() ||
              receiverLeaf.compare(receiverLeaf.size() - ValueSuffix.size(),
                                   ValueSuffix.size(),
                                   ValueSuffix) != 0) {
            return keyValueHelperMethodSpelling(memberName);
          }
          receiverLeaf.erase(receiverLeaf.size() - ValueSuffix.size());
          if (receiverLeaf.empty()) {
            return keyValueHelperMethodSpelling(memberName);
          }
          receiverLeaf.front() = static_cast<char>(
              std::tolower(static_cast<unsigned char>(receiverLeaf.front())));
          return receiverLeaf + pascalCaseHelperMember(memberName);
        };
        auto isCanonicalKeyValueHelperFamilyPath = [&](const std::string &path) {
          const auto *metadata = keyValueHelperMetadata();
          return metadata != nullptr &&
                 isCanonicalPublishedStdlibSurfaceHelperPath(
                     normalizeCollectionHelperPath(path), metadata->id);
        };
        auto isKeyValueHelperMemberPath = [&](const std::string &path,
                                              std::string_view expectedName) {
          std::string helperName;
          return resolveKeyValueHelperMemberName(path, helperName) &&
                 helperName == std::string(expectedName);
        };
        auto importPathCoversPublishedTarget =
            [](const std::string &importPath, const std::string &targetPath) {
              if (importPath == targetPath) {
                return true;
              }
              if (importPath.size() >= 2 &&
                  importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
                const std::string prefix =
                    importPath.substr(0, importPath.size() - 2);
                return targetPath == prefix ||
                       targetPath.rfind(prefix + "/", 0) == 0;
              }
              return false;
            };
        auto hasSemanticKeyValueHelperDefinition =
            [&](std::string_view helperName) {
              const auto *metadata = keyValueHelperMetadata();
              if (semanticProgram == nullptr || metadata == nullptr) {
                return false;
              }
              const std::string helperPath =
                  stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
              if (helperPath.empty()) {
                return false;
              }
              if (findDirectHelperDefinition(helperPath) != nullptr) {
                return true;
              }
              auto importsHelper = [&](const std::vector<std::string> &imports) {
                return std::any_of(
                    imports.begin(),
                    imports.end(),
                    [&](const std::string &importPath) {
                      return importPathCoversPublishedTarget(importPath,
                                                             helperPath);
                    });
              };
              if (importsHelper(semanticProgram->sourceImports) ||
                  importsHelper(semanticProgram->imports)) {
                return true;
              }
              return std::any_of(
                  semanticProgram->definitions.begin(),
                  semanticProgram->definitions.end(),
                  [&](const auto &definition) {
                    return definition.fullPath == helperPath ||
                           definition.fullPath.rfind(helperPath + "__", 0) == 0;
                  });
            };
        auto semanticCollectionFamilyForExpr = [&](const Expr &receiverExpr) {
          auto keyValueFamilyName = [&]() {
            const auto *metadata = keyValueHelperMetadata();
            if (metadata == nullptr || metadata->canonicalPath.empty()) {
              return std::string{};
            }
            const std::string canonicalPath(metadata->canonicalPath);
            const size_t slash = canonicalPath.find_last_of('/');
            return slash == std::string::npos
                       ? canonicalPath
                       : canonicalPath.substr(slash + 1);
          };
          if (semanticProgram == nullptr ||
              callResolutionAdapters.semanticProductTargets.semanticIndex
                  .collectionSpecializationsByExpr.empty()) {
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiverExpr.name);
              if (localIt != localsIn.end() &&
                  hasKeyValueKinds(localIt->second)) {
                return keyValueFamilyName();
              }
            }
            return std::string{};
          }
          const auto *collectionFact =
              ir_lowerer::findSemanticProductCollectionSpecialization(
                  callResolutionAdapters.semanticProductTargets.semanticIndex,
                  receiverExpr);
          if (collectionFact == nullptr && receiverExpr.sourceLine != 0 &&
              receiverExpr.sourceColumn != 0) {
            for (const auto &candidate :
                 semanticProgram->collectionSpecializations) {
              if (candidate.sourceLine == receiverExpr.sourceLine &&
                  candidate.sourceColumn == receiverExpr.sourceColumn) {
                collectionFact = &candidate;
                break;
              }
            }
          }
          if (collectionFact == nullptr) {
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto localIt = localsIn.find(receiverExpr.name);
              if (localIt != localsIn.end() &&
                  hasKeyValueKinds(localIt->second)) {
                return keyValueFamilyName();
              }
            }
            return std::string{};
          }
          if (collectionFact->collectionFamilyId != InvalidSymbolId) {
            return trimTemplateTypeText(std::string(
                semanticProgramResolveCallTargetString(
                    *semanticProgram, collectionFact->collectionFamilyId)));
          }
          return trimTemplateTypeText(collectionFact->collectionFamily);
        };
        auto populateKeyValueInfoFromCollectionFact =
            [&](const SemanticProgramCollectionSpecialization &collectionFact,
                CollectionPairTypeInfo &targetInfoOut) {
          const auto *metadata = keyValueHelperMetadata();
          if (metadata == nullptr ||
              !collectionFact.helperSurfaceId.has_value() ||
              *collectionFact.helperSurfaceId != metadata->id) {
            return false;
          }
          auto resolveSemanticText = [&](const std::string &text, SymbolId textId) {
            if (semanticProgram != nullptr && textId != InvalidSymbolId) {
              return std::string(
                  semanticProgramResolveCallTargetString(*semanticProgram, textId));
            }
            return text;
          };
          targetInfoOut = {};
          targetInfoOut.isKeyValueTarget = true;
          targetInfoOut.keyValueKeyKind = valueKindFromTypeName(
              resolveSemanticText(collectionFact.keyTypeText,
                                  collectionFact.keyTypeTextId));
          targetInfoOut.keyValueValueKind = valueKindFromTypeName(
              resolveSemanticText(collectionFact.valueTypeText,
                                  collectionFact.valueTypeTextId));
          targetInfoOut.isWrappedKeyValueTarget =
              collectionFact.isReference || collectionFact.isPointer;
          targetInfoOut.structTypeName = resolveSemanticText(
              collectionFact.structPath, collectionFact.structPathId);
          return true;
        };
        auto resolveLocalNameKeyValueInfo =
            [&](const Expr &receiverExpr, CollectionPairTypeInfo &targetInfoOut) {
          if (semanticProgram == nullptr || receiverExpr.kind != Expr::Kind::Name) {
            return false;
          }
          for (const auto &collectionFact :
               semanticProgram->collectionSpecializations) {
            if (collectionFact.name != receiverExpr.name) {
              continue;
            }
            if (populateKeyValueInfoFromCollectionFact(
                    collectionFact, targetInfoOut)) {
              return true;
            }
          }
          return false;
        };
        auto resolveSameFamilyKeyValueHelperMemberName =
            [&](const Expr &callExpr, const Expr &receiverExpr,
                std::string &helperNameOut) {
              helperNameOut.clear();
              const auto *metadata = keyValueHelperMetadata();
              if (metadata == nullptr) {
                return false;
              }
              std::string rawPath =
                  normalizeCollectionHelperPath(resolveDirectHelperPath(callExpr));
              if (!rawPath.empty() && rawPath.front() == '/') {
                rawPath.erase(rawPath.begin());
              }
              const size_t slash = rawPath.find('/');
              if (slash == std::string::npos || slash == 0 ||
                  slash + 1 >= rawPath.size()) {
                return false;
              }
              const std::string family = semanticCollectionFamilyForExpr(receiverExpr);
              if (family.empty() || rawPath.substr(0, slash) != family) {
                return false;
              }
              const std::string memberPath = rawPath.substr(slash + 1);
              const std::string memberLeaf = extractHelperTail(memberPath);
              const std::string_view memberName =
                  resolveStdlibSurfaceMemberName(*metadata, memberLeaf);
              if (memberName.empty()) {
                return false;
              }
              helperNameOut.assign(memberName);
              return true;
            };
        auto isCanonicalKeyValueConstructorPath = [&](const std::string &path) {
          const auto *metadata = keyValueConstructorMetadata();
          if (metadata == nullptr) {
            return false;
          }
          std::string constructorName;
          const std::string normalizedPath = normalizeCollectionHelperPath(path);
          return normalizedPath == metadata->canonicalPath &&
                 resolvePublishedStdlibSurfaceConstructorMemberName(
                     normalizedPath, metadata->id, constructorName) &&
                 constructorName == "map";
        };
        auto isDirectCollectionHelperPath = [&](const std::string &path) {
          return path.rfind("/array/", 0) == 0 ||
                 path.rfind(collectionMemberRoot("vector"), 0) == 0 ||
                 path.rfind(experimentalCollectionMemberRoot("vec" "tor"), 0) == 0 ||
                 isCanonicalKeyValueHelperFamilyPath(path);
        };
        auto hasKeyValueEntryCtorArgs = [&](const Expr &callExpr) {
          auto isKeyValueEntryCallExpr = [&](const Expr &candidate) {
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
            return isKeyValueHelperMemberPath(normalizedName, "entry");
          };
          for (const auto &arg : callExpr.args) {
            if (isKeyValueEntryCallExpr(arg)) {
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
          return normalizedPath.rfind("/soa" "_vector/", 0) == 0 ||
                 normalizedPath == "/to" "_aos" ||
                 normalizedPath == "/to" "_aos_ref";
        };
        auto isSoaWrapperHelperFamilyPath = [&](const std::string &path) {
          const std::string normalizedPath = stripGeneratedHelperSuffix(path);
          return normalizedPath.rfind("/std/collections/" "soa" "_vector/", 0) == 0 ||
                 normalizedPath.rfind("/std/collections/experimental" "_soa" "_vector/soa" "Vector", 0) == 0 ||
                 normalizedPath.rfind("/std/collections/experimental" "_soa" "_vector_conversions/soa" "Vector", 0) == 0;
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
          if (const Definition *directSoaWrapper =
                  findDirectHelperDefinition(rawPath)) {
            return directSoaWrapper;
          }
          auto canonicalSamePathSoaWrapper = [](const std::string &path) {
            if (path == "/soa" "_vector/count") {
              return std::string("/std/collections/" "soa" "_vector/count");
            }
            if (path == "/soa" "_vector/count_ref") {
              return std::string("/std/collections/" "soa" "_vector/count_ref");
            }
            if (path == "/soa" "_vector/get") {
              return std::string("/std/collections/" "soa" "_vector/get");
            }
            if (path == "/soa" "_vector/get_ref") {
              return std::string("/std/collections/" "soa" "_vector/get_ref");
            }
            if (path == "/soa" "_vector/ref") {
              return std::string("/std/collections/" "soa" "_vector/ref");
            }
            if (path == "/soa" "_vector/ref_ref") {
              return std::string("/std/collections/" "soa" "_vector/ref_ref");
            }
            if (path == "/soa" "_vector/reserve") {
              return std::string("/std/collections/" "soa" "_vector/reserve");
            }
            if (path == "/soa" "_vector/push") {
              return std::string("/std/collections/" "soa" "_vector/push");
            }
            if (path == "/to" "_aos") {
              return std::string("/std/collections/" "soa" "_vector/to" "_aos");
            }
            if (path == "/to" "_aos_ref") {
              return std::string("/std/collections/" "soa" "_vector/to" "_aos_ref");
            }
            return std::string{};
          };
          if (const std::string canonicalPath =
                  canonicalSamePathSoaWrapper(normalizedRawPath);
              !canonicalPath.empty()) {
            return findDirectHelperDefinition(canonicalPath);
          }
          const bool isExperimentalSoaToAosCall =
              normalizedRawPath ==
              "/std/collections/experimental" "_soa" "_vector_conversions/soa" "VectorToAos";
          if (isExperimentalSoaToAosCall && !callExpr.args.empty()) {
            const std::string receiverStruct =
                inferStructExprPath(callExpr.args.front(), localsIn);
            if (normalizeCollectionBindingTypeName(receiverStruct) == "soa" "_vector") {
              if (const Definition *canonicalSoaToAos =
                      findDirectHelperDefinition("/std/collections/" "soa" "_vector/to" "_aos")) {
                return canonicalSoaToAos;
              }
            }
          }
          return nullptr;
        };
        auto findDirectEntryKeyValueConstructorDefinition = [&](const Expr &callExpr)
            -> const Definition * {
          const std::string rawPath = resolveDirectHelperPath(callExpr);
          const std::string normalizedRawPath =
              normalizeCollectionHelperPath(rawPath);
          if (!isCanonicalKeyValueConstructorPath(normalizedRawPath)) {
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
          if (!ir_lowerer::isStructConstructorCallShape(callExpr)) {
            return nullptr;
          }
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
        auto resolveBuiltinAccessName = [&](const Expr &callExpr,
                                            std::string &accessNameOut) {
          if (getBuiltinArrayAccessName(callExpr, accessNameOut)) {
            return true;
          }
          const std::string resolvedAccessPath = resolveExprPath(callExpr);
          if (resolvedAccessPath == "/at") {
            accessNameOut = "at";
            return true;
          }
          if (resolvedAccessPath == "/at_unsafe") {
            accessNameOut = "at_unsafe";
            return true;
          }
          std::string vectorHelperName;
          if (resolveVectorHelperAliasName(callExpr, vectorHelperName) &&
              (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) {
            accessNameOut = vectorHelperName;
            return true;
          }
          return false;
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
                    if (!matchesGeneratedSpecializedType(
                            normalized, "vector", "Vector")) {
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
              if (matchesGeneratedSpecializedType(
                      inferredReceiverStruct, "vector", "Vector")) {
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
                      "/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0 ||
                  normalizeCollectionBindingTypeName(inferredReceiverStruct) ==
                      "soa" "_vector") {
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
                   collectionName != "soa" "_vector") ||
                  collectionArgs.size() != 1) {
                return false;
              }
              targetInfoOut.isArrayOrVectorTarget = true;
              targetInfoOut.isVectorTarget = (collectionName == "vector");
              targetInfoOut.isSoaVector = (collectionName == "soa" "_vector");
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
          const bool isCollectionVectorConstructorAlias =
              getExperimentalVectorConstructorElementTypeAlias(
                  expr, experimentalVectorElementType) ||
              getExperimentalVectorConstructorElementTypeAliasFromPath(
                  resolveExprPath(expr), experimentalVectorElementType);
          if (isCollectionVectorConstructorAlias) {
            Expr rewrittenVectorCtor = expr;
            rewrittenVectorCtor.name =
                experimentalCollectionMemberPath("vector", "vector");
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
              hasKeyValueEntryCtorArgs(expr) &&
              isCanonicalKeyValueHelperFamilyPath(rawPath)) {
            directCallee = findDirectEntryKeyValueConstructorDefinition(expr);
          }
          if (directCallee == nullptr &&
              isInternalSoaHelperFamilyPath(rawPath)) {
            directCallee = findDirectInternalSoaDefinition(rawPath);
          }
          if (directCallee == nullptr && !expr.isMethodCall) {
            directCallee = findDirectStructDefinition(expr);
          }
          if (directCallee == nullptr &&
              (isSoaWrapperHelperFamilyPath(rawPath) ||
               isSamePathSoaHelperPath(rawPath))) {
            directCallee = findDirectSoaWrapperDefinition(expr, rawPath);
          }
          const std::string resolvedExprPath = resolveExprPath(expr);
          if (directCallee == nullptr && isDirectCollectionHelperPath(rawPath)) {
            directCallee = findDirectHelperDefinition(rawPath);
          }
          if (directCallee == nullptr && isDirectCollectionHelperPath(resolvedExprPath)) {
            directCallee = findDirectHelperDefinition(resolvedExprPath);
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
            if (isCollectionVectorRecordTypePath(receiverStructPath)) {
              candidates.push_back(receiverStructPath + "/" + expr.name);
            }
            candidates.push_back(
                experimentalCollectionTypePath("vec" "tor", "Vector") + "/" +
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
                  !matchesGeneratedSpecializedType(
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
          auto internalSoaMetadataHelperLeaf =
              [](const Definition &definition) -> std::string {
            if (definition.fullPath.rfind(
                    "/std/collections/internal_soa_storage/SoaColumn", 0) != 0 &&
                definition.fullPath.rfind(
                    "/std/collections/internal_soa_storage/SoaFieldView", 0) != 0) {
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
            if ((rawPath.rfind("/array/", 0) == 0 ||
                 resolvedExprPath.rfind("/array/", 0) == 0 ||
                 directCallee->fullPath.rfind("/array/", 0) == 0) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                return false;
              }
              return true;
            }
            if ((rawPath.rfind(collectionMemberRoot("vector"), 0) == 0 ||
                 rawPath.rfind(experimentalCollectionMemberRoot("vec" "tor"), 0) == 0 ||
                 directCallee->fullPath.rfind(collectionMemberRoot("vector"), 0) == 0 ||
                 directCallee->fullPath.rfind(experimentalCollectionMemberRoot("vec" "tor"), 0) == 0) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              std::string vectorHelperName;
              const bool isMaterializableVectorMetadataReceiver =
                  resolveVectorHelperAliasName(expr, vectorHelperName) &&
                  expr.args.size() == 1 &&
                  expr.args.front().kind == Expr::Kind::Call &&
                  !expr.args.front().isFieldAccess &&
                  resolveDirectHelperDefinition(expr.args.front()) != nullptr &&
                  (vectorHelperName == "count" || vectorHelperName == "capacity");
              const bool isDirectVectorBuiltin =
                  (resolveBuiltinAccessName(expr, vectorHelperName) &&
                   expr.args.size() == 2 &&
                   (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) ||
                  isMaterializableVectorMetadataReceiver;
              if (!isDirectVectorBuiltin) {
                if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                  return false;
                }
                return true;
              }
            }
            if (hasKeyValueEntryCtorArgs(expr) &&
                extractHelperTail(normalizeCollectionHelperPath(directCallee->fullPath)) ==
                    "map" &&
                (isCanonicalKeyValueHelperFamilyPath(rawPath) ||
                 isCanonicalKeyValueHelperFamilyPath(resolvedExprPath)) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
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
                    resolveSameFamilyKeyValueHelperMemberName(
                        expr, expr.args.front(), helperName);
              }
            }
            const bool hasCanonicalKeyValueHelperFamily =
                isCanonicalKeyValueHelperFamilyPath(rawPath) ||
                isCanonicalKeyValueHelperFamilyPath(directCallee->fullPath);
            if (!helperName.empty() &&
                (helperName == "count" || helperName == "contains" ||
                 helperName == "tryAt" || helperName == "at" ||
                 helperName == "at_unsafe" || helperName == "insert" ||
                 helperName == "insert_ref") &&
                (hasCanonicalKeyValueHelperFamily ||
                 hasSameFamilyKeyValueHelperAlias) &&
                isDirectHelperDefinitionFamily(expr, *directCallee)) {
              const bool deferKeyValueCountToBuiltinEmitter =
                  helperName == "count" && expr.args.size() == 1 &&
                  hasSemanticKeyValueHelperDefinition(helperName) &&
                  ir_lowerer::resolveCollectionPairTypeInfo(
                      expr.args.front(),
                      localsIn,
                      {},
                      semanticProgram,
                      &callResolutionAdapters.semanticProductTargets.semanticIndex)
                      .isKeyValueTarget;
              if (!deferKeyValueCountToBuiltinEmitter) {
                if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
                  return false;
                } else {
                  return true;
                }
              }
            }
            std::string accessName;
            std::string explicitKeyValueAccessHelperName;
            const bool isExplicitCanonicalKeyValueAccess =
                (getBuiltinArrayAccessName(expr, accessName) &&
                 expr.args.size() == 2 &&
                 isCanonicalKeyValueHelperFamilyPath(rawPath)) ||
                (resolveKeyValueHelperAliasName(expr, explicitKeyValueAccessHelperName) &&
                 (explicitKeyValueAccessHelperName == "at" ||
                  explicitKeyValueAccessHelperName == "at_ref" ||
                  explicitKeyValueAccessHelperName == "at_unsafe" ||
                  explicitKeyValueAccessHelperName == "at_unsafe_ref") &&
                 expr.args.size() == 2 &&
                 isCanonicalKeyValueHelperFamilyPath(rawPath));
            if (isExplicitCanonicalKeyValueAccess &&
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
        if (resolveBuiltinAccessName(expr, accessName)) {
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
                  0,
                  resolveHelperReturnedArrayVectorAccessTargetInfo,
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

        Expr countAccessExpr = expr;
        if (!expr.isMethodCall && expr.args.size() == 1) {
          std::string keyValueCountHelperName;
          if (resolveSameFamilyKeyValueHelperMemberName(
                  expr, expr.args.front(), keyValueCountHelperName) &&
              keyValueCountHelperName == "count" &&
              hasSemanticKeyValueHelperDefinition(keyValueCountHelperName)) {
            if (const auto *metadata = keyValueHelperMetadata();
                metadata != nullptr) {
              const std::string canonicalCountPath =
                  stdlibSurfaceCanonicalHelperPath(metadata->id,
                                                   keyValueCountHelperName);
              if (!canonicalCountPath.empty()) {
                countAccessExpr.name = canonicalCountPath;
                countAccessExpr.namespacePrefix.clear();
                countAccessExpr.semanticNodeId = 0;
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
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              const bool isCollectionVectorTarget =
                  isCollectionVectorRecordTypePath(structPath);
              const bool isExperimentalKeyValueTarget =
                  structPath == experimentalCollectionTypePath("map", "Map") ||
                  matchesGeneratedSpecializedType(structPath, "map", "Map");
              const bool isSemanticKeyValueTarget =
                  ir_lowerer::resolveCollectionPairTypeInfo(
                      targetExpr,
                      targetLocals,
                      {},
                      semanticProgram,
                      &callResolutionAdapters.semanticProductTargets.semanticIndex)
                      .isKeyValueTarget;
              const bool isExperimentalSoaVectorTarget =
                  structPath == "/std/collections/experimental" "_soa" "_vector/Soa" "Vector" ||
                  structPath.rfind("/std/collections/experimental" "_soa" "_vector/Soa" "Vector" "__", 0) == 0;
              return targetInfo.isArrayOrVectorTarget || structPath == "/array" ||
                     structPath == "/vector" || structPath == "/Buffer" || structPath == "/map" ||
                     structPath == "/soa" "_vector" || isCollectionVectorTarget ||
                     isExperimentalKeyValueTarget || isSemanticKeyValueTarget ||
                     isExperimentalSoaVectorTarget;
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     isCollectionVectorRecordTypePath(structPath);
            },
            [&](const Expr &targetExpr, const LocalMap &targetLocals) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      targetExpr,
                      targetLocals,
                      resolveHelperReturnedArrayVectorAccessTargetInfo);
              const std::string structPath = inferStructExprPath(targetExpr, targetLocals);
              return (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
                     isCollectionVectorRecordTypePath(structPath);
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
            const uint64_t slotOffset =
                isSimpleCallName(expr, "field_capacity") ? IrSlotBytes * 2 : IrSlotBytes;
            function.instructions.push_back({IrOpcode::PushI64, slotOffset});
            function.instructions.push_back({IrOpcode::AddI64, 0});
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
        if (!expr.isMethodCall && hasKeyValueEntryCtorArgs(expr) &&
            isCanonicalKeyValueConstructorPath(resolveExprPath(expr))) {
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
                  resolveHelperReturnedArrayVectorAccessTargetInfo,
                  semanticProgram,
                  &callResolutionAdapters.semanticProductTargets.semanticIndex);
          const std::string structPath = inferStructExprPath(expr.args.front(), localsIn);
          const bool isSemanticVectorTarget =
              (targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget) ||
              isCollectionVectorRecordTypePath(structPath);
          if (!isDirectVectorConstructor &&
              (expr.args.front().kind == Expr::Kind::Call ||
               isSemanticVectorTarget)) {
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
            ((isCanonicalKeyValueHelperFamilyPath(exprPath) &&
              resolveKeyValueHelperMemberName(exprPath, resolvedKeyValueInsertHelperName) &&
              (resolvedKeyValueInsertHelperName == "insert" ||
               resolvedKeyValueInsertHelperName == "insert_ref")) ||
             exprPath.rfind("/std/collections/internal_map/insert", 0) == 0)) {
          if (const Definition *directCallee = resolveDirectHelperDefinition(expr);
              directCallee != nullptr) {
            if (!emitInlineDefinitionCall(expr, *directCallee, localsIn, true)) {
              return false;
            }
            return true;
          }
        }
        std::string bareKeyValueAccessName;
        if (!expr.isMethodCall &&
            expr.args.size() == 2 &&
            ((getBuiltinArrayAccessName(expr, bareKeyValueAccessName) &&
              (bareKeyValueAccessName == "at" ||
               bareKeyValueAccessName == "at_unsafe")) ||
             ((isSimpleCallName(expr, "at") ||
               isSimpleCallName(expr, "at_unsafe")) &&
              (bareKeyValueAccessName = expr.name, true))) &&
            (bareKeyValueAccessName == "at" ||
             bareKeyValueAccessName == "at_unsafe")) {
          auto resolveAccessTargetInfo = [&](const Expr &receiverExpr) {
            auto resolvedInfo =
                ir_lowerer::resolveCollectionPairTypeInfo(
                    receiverExpr,
                    localsIn,
                    {},
                    semanticProgram,
                    &callResolutionAdapters.semanticProductTargets.semanticIndex);
            if (!resolvedInfo.isKeyValueTarget) {
              resolveLocalNameKeyValueInfo(receiverExpr, resolvedInfo);
            }
            return resolvedInfo;
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
          if (targetInfo.isKeyValueTarget) {
            Expr accessExpr = expr;
            if (receiverArgIndex != 0 && accessExpr.args.size() > receiverArgIndex) {
              std::swap(accessExpr.args[0], accessExpr.args[receiverArgIndex]);
            }
            const std::string priorError = error;
            if (const Definition *directCallee =
                    resolveDirectHelperDefinition(accessExpr);
                directCallee != nullptr) {
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
            methodExpr.name = keyValueImplementationMethodSpelling(
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
