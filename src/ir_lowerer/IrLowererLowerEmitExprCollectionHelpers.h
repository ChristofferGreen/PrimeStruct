        auto resolveCollectionExprDirectPath = [&](const Expr &candidate) {
          if (!candidate.name.empty() && candidate.name.front() == '/') {
            return candidate.name;
          }
          if (!candidate.namespacePrefix.empty()) {
            std::string scoped = candidate.namespacePrefix;
            if (!scoped.empty() && scoped.front() != '/') {
              scoped.insert(scoped.begin(), '/');
            }
            return scoped + "/" + candidate.name;
          }
          return candidate.name;
        };
        auto resolveCollectionExprDirectDefinition =
            [&](const Expr &candidate) -> const Definition * {
          auto findDirectHelperDefinition = [&](const std::string &path)
              -> const Definition * {
            auto defIt = defMap.find(path);
            if (defIt != defMap.end()) {
              return defIt->second;
            }
            auto matchesGeneratedLeafDefinition = [&](const std::string &candidatePath,
                                                      const char *marker,
                                                      size_t markerSize) {
              return candidatePath.rfind(path, 0) == 0 &&
                     candidatePath.compare(path.size(), markerSize, marker) == 0 &&
                     candidatePath.find('/', path.size() + markerSize) ==
                         std::string::npos;
            };
            for (const auto &[candidatePath, def] : defMap) {
              if (def == nullptr) {
                continue;
              }
              if (matchesGeneratedLeafDefinition(candidatePath, "__t", 3) ||
                  matchesGeneratedLeafDefinition(candidatePath, "__ov", 4)) {
                return def;
              }
            }
            return nullptr;
          };
          auto isExplicitExperimentalVectorConstructorHelper =
              [&](const std::string &path) {
                const std::string slashPath =
                    experimentalCollectionMemberPath("vector", "vector");
                return path == slashPath ||
                       path == slashPath.substr(1);
              };
          const std::string rawPath = resolveCollectionExprDirectPath(candidate);
          if (isExplicitExperimentalVectorConstructorHelper(rawPath)) {
            if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
                rawDef != nullptr) {
              return rawDef;
            }
          }
          std::string experimentalVectorElementType;
          if (getExperimentalVectorConstructorElementTypeAlias(
                  candidate, experimentalVectorElementType)) {
            Expr rewrittenVectorCtor = candidate;
            rewrittenVectorCtor.name =
                experimentalCollectionMemberPath("vector", "vector");
            rewrittenVectorCtor.namespacePrefix.clear();
            rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};
            if (const Definition *vectorCtor =
                    findDirectHelperDefinition(
                        resolveCollectionExprDirectPath(rewrittenVectorCtor))) {
              return vectorCtor;
            }
          }
          if (const Definition *callee = resolveDefinitionCall(candidate);
              callee != nullptr) {
            return callee;
          }
          if (const Definition *rawDef = findDirectHelperDefinition(rawPath);
              rawDef != nullptr) {
            return rawDef;
          }
          const std::string resolvedPath = resolveExprPath(candidate);
          if (resolvedPath != rawPath) {
            return findDirectHelperDefinition(resolvedPath);
          }
          return nullptr;
        };
        auto normalizeLateCollectionHelperName = [&](std::string &helperName) {
          if (helperName == "count_ref") {
            helperName = "count";
          } else if (helperName == "contains_ref") {
            helperName = "contains";
          } else if (helperName == "tryAt_ref") {
            helperName = "tryAt";
          } else if (helperName == "at_ref") {
            helperName = "at";
          } else if (helperName == "at_unsafe_ref") {
            helperName = "at_unsafe";
          } else if (helperName == "insert_ref") {
            helperName = "insert";
          }
        };
        auto keyValueHelperSurfaceMetadataForLowerEmitExpr = []() {
          return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
        };
        auto keyValueConstructorSurfaceMetadataForLowerEmitExpr = []() {
          return findStdlibSurfaceMetadataByBridgeKey(
              "collections.map_constructors");
        };
        auto resolvePublishedLateCollectionMemberName =
            [&](const Expr &candidate,
                primec::StdlibSurfaceId surfaceId,
                std::string &memberNameOut) {
              memberNameOut.clear();
              const bool resolved =
                  ir_lowerer::resolvePublishedSemanticStdlibSurfaceMemberName(
                      callResolutionAdapters.semanticProgram,
                      candidate,
                      surfaceId,
                      memberNameOut) ||
                  ir_lowerer::resolvePublishedStdlibSurfaceExprMemberName(
                      candidate,
                      surfaceId,
                      memberNameOut) ||
                  ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                      resolveExprPath(candidate),
                      surfaceId,
                      memberNameOut) ||
                  ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                      resolveCollectionExprDirectPath(candidate),
                      surfaceId,
                      memberNameOut);
              const auto *metadata = keyValueHelperSurfaceMetadataForLowerEmitExpr();
              if (resolved && metadata != nullptr && surfaceId == metadata->id) {
                normalizeLateCollectionHelperName(memberNameOut);
              }
              return resolved;
            };
        auto resolvePublishedLateVectorMemberName =
            [&](const Expr &candidate, std::string &memberNameOut) {
              const auto *metadata =
                  findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
              return metadata != nullptr &&
                     resolvePublishedLateCollectionMemberName(
                         candidate, metadata->id, memberNameOut);
            };
        auto resolvePublishedLateKeyValueMemberName =
            [&](const Expr &candidate, std::string &memberNameOut) {
              const auto *metadata = keyValueHelperSurfaceMetadataForLowerEmitExpr();
              return metadata != nullptr &&
                     resolvePublishedLateCollectionMemberName(
                         candidate, metadata->id, memberNameOut);
            };
        auto resolvePublishedLateCollectionConstructorName =
            [&](const Expr &candidate,
                primec::StdlibSurfaceId surfaceId,
                std::string &memberNameOut) {
              memberNameOut.clear();
              return ir_lowerer::resolvePublishedStdlibSurfaceConstructorExprMemberName(
                         candidate,
                         surfaceId,
                         memberNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceConstructorMemberName(
                         resolveExprPath(candidate),
                         surfaceId,
                         memberNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceConstructorMemberName(
                         resolveCollectionExprDirectPath(candidate),
                         surfaceId,
                         memberNameOut);
            };
        auto resolvePublishedLateKeyValueConstructorName =
            [&](const Expr &candidate, std::string &memberNameOut) {
              const auto *metadata =
                  keyValueConstructorSurfaceMetadataForLowerEmitExpr();
              return metadata != nullptr &&
                     resolvePublishedLateCollectionConstructorName(
                         candidate, metadata->id, memberNameOut);
            };
        auto isEntryArgsPackKeyValueConstructorExpr = [&](const Expr &callExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
            return false;
          }
          std::string constructorName;
          if (!resolvePublishedLateKeyValueConstructorName(callExpr,
                                                           constructorName) ||
              constructorName == "entry") {
            return false;
          }
          bool hasSpreadArg = false;
          for (const auto &argExpr : callExpr.args) {
            if (argExpr.isSpread) {
              hasSpreadArg = true;
              break;
            }
          }
          if (!hasSpreadArg) {
            return false;
          }
          return true;
        };
        auto isNativeUnsupportedStringKeyValueConstructorExpr = [&](const Expr &callExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall ||
              callExpr.args.empty() || callExpr.templateArgs.size() != 2) {
            return false;
          }
          std::string constructorName;
          if (!resolvePublishedLateKeyValueConstructorName(callExpr,
                                                           constructorName) ||
              constructorName == "entry") {
            return false;
          }
          return ir_lowerer::valueKindFromTypeName(
                     ir_lowerer::trimTemplateTypeText(callExpr.templateArgs.front())) ==
                 ir_lowerer::LocalInfo::ValueKind::String;
        };
        if (isEntryArgsPackKeyValueConstructorExpr(expr)) {
          error = "native backend does not support variadic entry map constructors";
          return false;
        }
        if (isNativeUnsupportedStringKeyValueConstructorExpr(expr)) {
          error = "native backend only supports indexing into string literals or string bindings";
          return false;
        }
        auto rewriteExplicitBuiltinKeyValueHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.empty()) {
            return false;
          }
          const auto *metadata = keyValueHelperSurfaceMetadataForLowerEmitExpr();
          if (metadata == nullptr) {
            return false;
          }
          auto isRootedAliasSamePathCountLikeCall = [&](const Expr &candidate) {
            const std::string rawPath = resolveCollectionExprDirectPath(candidate);
            std::string directHelperName;
            if (!ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                    rawPath,
                    metadata->id,
                    directHelperName) ||
                ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                    rawPath,
                    metadata->id)) {
              return false;
            }
            const std::string resolvedPath = resolveExprPath(candidate);
            return normalizeCollectionHelperPath(rawPath) ==
                   normalizeCollectionHelperPath(resolvedPath);
          };
          std::string helperName;
          if ((!resolveKeyValueHelperAliasName(callExpr, helperName) &&
               !resolvePublishedLateKeyValueMemberName(callExpr, helperName)) ||
              (helperName != "count" && helperName != "contains" &&
               helperName != "tryAt" && helperName != "insert")) {
            return false;
          }
          if (callExpr.namespacePrefix.empty() &&
              callExpr.name == helperName) {
            return false;
          }
          const std::string directHelperPath = resolveCollectionExprDirectPath(callExpr);
          if (ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  directHelperPath,
                  metadata->id)) {
            return false;
          }
          if (ir_lowerer::isPublishedStdlibSurfaceLoweringPath(
                  directHelperPath,
                  metadata->id) &&
              !ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  directHelperPath,
                  metadata->id)) {
            return false;
          }
          if (helperName != "insert" && isRootedAliasSamePathCountLikeCall(callExpr)) {
            return false;
          }
          if (resolveDefinitionCall(callExpr) != nullptr) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = helperName;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.templateArgs.clear();
          return true;
        };
        Expr rewrittenExplicitBuiltinKeyValueHelperExpr;
        if (rewriteExplicitBuiltinKeyValueHelperExpr(
                expr, rewrittenExplicitBuiltinKeyValueHelperExpr)) {
          return emitExpr(rewrittenExplicitBuiltinKeyValueHelperExpr, localsIn);
        }
        auto rejectCanonicalVectorTemporaryReceiverExpr = [&](const Expr &callExpr) -> bool {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }

          std::string helperName;
          const Expr *receiverExpr = nullptr;
          if (callExpr.isMethodCall) {
            if (!resolvePublishedLateVectorMemberName(callExpr, helperName)) {
              helperName = resolveCollectionExprDirectPath(callExpr);
              if (!helperName.empty() && helperName.front() == '/') {
                helperName.erase(helperName.begin());
              }
              const size_t lastSlash = helperName.find_last_of('/');
              if (lastSlash != std::string::npos) {
                helperName = helperName.substr(lastSlash + 1);
              }
            }
            receiverExpr = &callExpr.args.front();
          } else if (callExpr.args.size() == 2 && getBuiltinArrayAccessName(callExpr, helperName)) {
            receiverExpr = &callExpr.args.front();
          } else {
            if (!resolvePublishedLateVectorMemberName(callExpr, helperName)) {
              return false;
            }
            receiverExpr = &callExpr.args.front();
          }

          if (receiverExpr == nullptr || receiverExpr->kind != Expr::Kind::Call) {
            return false;
          }
          if (helperName != "count" && helperName != "capacity" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          if (!callExpr.isMethodCall) {
            std::string publishedHelperName;
            if (!resolvePublishedLateVectorMemberName(callExpr, publishedHelperName)) {
              return false;
            }
          }

          const Definition *receiverDef =
              resolveCollectionExprDirectDefinition(*receiverExpr);
          if (receiverDef == nullptr) {
            return false;
          }
          std::string constructorName;
          if (!ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                  receiverDef->fullPath,
                  primec::StdlibSurfaceId::CollectionsVectorConstructors,
                  constructorName)) {
            return false;
          }

          error = "count requires array, vector, map, or string target";
          return true;
        };
        if (rejectCanonicalVectorTemporaryReceiverExpr(expr)) {
          return false;
        }
        auto emitMaterializedCollectionReceiverExpr = [&](const Expr &callExpr) -> std::optional<bool> {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return std::nullopt;
          }
          std::string helperName;
          const Expr *receiverExpr = nullptr;
          bool materializedWrappedMapReceiver = false;
          ir_lowerer::LocalInfo::Kind materializedMapReceiverKind = ir_lowerer::LocalInfo::Kind::Value;
          auto resolveMaterializedCollectionHelperName =
              [&](const Expr &candidate, std::string &helperNameOut) {
                helperNameOut.clear();
                if (isSimpleCallName(candidate, "count")) {
                  helperNameOut = "count";
                  return true;
                }
                if (isSimpleCallName(candidate, "contains")) {
                  helperNameOut = "contains";
                  return true;
                }
                if (isSimpleCallName(candidate, "tryAt")) {
                  helperNameOut = "tryAt";
                  return true;
                }
                if (isSimpleCallName(candidate, "at")) {
                  helperNameOut = "at";
                  return true;
                }
                if (isSimpleCallName(candidate, "at_unsafe")) {
                  helperNameOut = "at_unsafe";
                  return true;
                }
                if (isSimpleCallName(candidate, "insert")) {
                  helperNameOut = "insert";
                  return true;
                }
                if (isSimpleCallName(candidate, "capacity")) {
                  helperNameOut = "capacity";
                  return true;
                }
                if (resolveKeyValueHelperAliasName(candidate, helperNameOut)) {
                  normalizeLateCollectionHelperName(helperNameOut);
                  return true;
                }
                return resolvePublishedLateKeyValueMemberName(candidate,
                                                              helperNameOut) ||
                       resolvePublishedLateVectorMemberName(candidate, helperNameOut);
              };
          auto matchesDirectHelperName = [&](const Expr &candidate, std::string_view bareName) {
            std::string helperName;
            return resolveMaterializedCollectionHelperName(candidate, helperName) &&
                   helperName == bareName;
          };
          if (callExpr.isMethodCall) {
            if (!resolveMaterializedCollectionHelperName(callExpr, helperName)) {
              helperName = resolveCollectionExprDirectPath(callExpr);
              if (!helperName.empty() && helperName.front() == '/') {
                helperName.erase(helperName.begin());
              }
              const size_t lastSlash = helperName.find_last_of('/');
              if (lastSlash != std::string::npos) {
                helperName = helperName.substr(lastSlash + 1);
              }
            }
            receiverExpr = &callExpr.args.front();
          } else if (getBuiltinArrayAccessName(callExpr, helperName) && callExpr.args.size() == 2) {
            receiverExpr = &callExpr.args.front();
          } else if ((matchesDirectHelperName(callExpr, "count") ||
                      matchesDirectHelperName(callExpr, "at") ||
                      matchesDirectHelperName(callExpr, "at_unsafe") ||
                      matchesDirectHelperName(callExpr, "contains") ||
                      matchesDirectHelperName(callExpr, "tryAt") ||
                      matchesDirectHelperName(callExpr, "insert") ||
                      matchesDirectHelperName(callExpr, "capacity")) &&
                     !callExpr.args.empty()) {
            if (!resolveMaterializedCollectionHelperName(callExpr, helperName)) {
              helperName = resolveCollectionExprDirectPath(callExpr);
              if (!helperName.empty() && helperName.front() == '/') {
                helperName.erase(helperName.begin());
              }
              const size_t lastSlash = helperName.find_last_of('/');
              if (lastSlash != std::string::npos) {
                helperName = helperName.substr(lastSlash + 1);
              }
              const size_t generatedSuffix = helperName.find("__");
              if (generatedSuffix != std::string::npos) {
                helperName.erase(generatedSuffix);
              }
              if (helperName == "count_ref") {
                helperName = "count";
              } else if (helperName == "contains_ref") {
                helperName = "contains";
              } else if (helperName == "tryAt_ref") {
                helperName = "tryAt";
              } else if (helperName == "at_ref") {
                helperName = "at";
              } else if (helperName == "at_unsafe_ref") {
                helperName = "at_unsafe";
              } else if (helperName == "insert_ref") {
                helperName = "insert";
              }
            }
            receiverExpr = &callExpr.args.front();
          } else {
            return std::nullopt;
          }

          if (receiverExpr == nullptr || receiverExpr->kind != Expr::Kind::Call) {
            return std::nullopt;
          }
          std::string collectionName;
          std::vector<std::string> collectionArgs;
          std::string collectionStructPath;
          const Definition *receiverDef =
              resolveCollectionExprDirectDefinition(*receiverExpr);
          const bool resolvedCollectionFromDef =
              receiverDef != nullptr &&
              ir_lowerer::inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs);
          if (resolvedCollectionFromDef) {
            collectionStructPath = inferStructExprPath(*receiverExpr, localsIn);
            if (collectionStructPath.empty() &&
                receiverDef->fullPath.rfind(
                    experimentalCollectionMemberRoot("map"), 0) == 0) {
              collectionStructPath =
                  inferPublishedExperimentalMapStructPathFromConstructorPath(
                      receiverDef->fullPath);
            }
          }
          const auto *lateCollectionSemanticProgram =
              callResolutionAdapters.semanticProgram;
          const auto *lateCollectionSemanticIndex =
              &callResolutionAdapters.semanticProductTargets.semanticIndex;
          if (!resolvedCollectionFromDef) {
            auto isKeyValueAccessValueCall = [&](const Expr &candidateExpr) {
              if (candidateExpr.kind != Expr::Kind::Call || candidateExpr.isMethodCall) {
                return false;
              }
              std::string keyValueHelperAlias;
              if (!resolveKeyValueHelperAliasName(candidateExpr, keyValueHelperAlias)) {
                return false;
              }
              return keyValueHelperAlias == "at" || keyValueHelperAlias == "at_unsafe" ||
                     keyValueHelperAlias == "tryAt" || keyValueHelperAlias == "contains";
            };
            if (isKeyValueAccessValueCall(*receiverExpr)) {
              return std::nullopt;
            }
            const auto inferCallKeyValueTargetInfo =
                [&](const Expr &targetExpr, ir_lowerer::CollectionPairTypeInfo &out) {
                  out = {};
                  const Definition *callee =
                      resolveCollectionExprDirectDefinition(targetExpr);
                  if (callee == nullptr) {
                    return false;
                  }
                  std::string inferredCollectionName;
                  std::vector<std::string> inferredCollectionArgs;
                  if (!ir_lowerer::inferDeclaredReturnCollection(
                          *callee, inferredCollectionName, inferredCollectionArgs) ||
                      inferredCollectionName != "map" ||
                      inferredCollectionArgs.size() != 2) {
                    return ir_lowerer::inferForwardedCollectionPairTypeInfo(
                        targetExpr, *callee, localsIn, {}, out);
                  }
                  out.isKeyValueTarget = true;
                  out.keyValueKeyKind =
                      ir_lowerer::valueKindFromTypeName(inferredCollectionArgs.front());
                  out.keyValueValueKind =
                      ir_lowerer::valueKindFromTypeName(inferredCollectionArgs.back());
                  return out.keyValueKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
                         out.keyValueValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown;
                };
            const auto keyValueTargetInfo =
                ir_lowerer::resolveCollectionPairTypeInfo(
                    *receiverExpr,
                    localsIn,
                    inferCallKeyValueTargetInfo,
                    lateCollectionSemanticProgram,
                    lateCollectionSemanticIndex);
            const auto arrayVectorTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    *receiverExpr,
                    localsIn,
                    {},
                    lateCollectionSemanticProgram,
                    lateCollectionSemanticIndex);
            if (keyValueTargetInfo.isKeyValueTarget && arrayVectorTargetInfo.isWrappedKeyValueTarget) {
              materializedWrappedMapReceiver = true;
              materializedMapReceiverKind = arrayVectorTargetInfo.argsPackElementKind;
            }
            if (keyValueTargetInfo.isKeyValueTarget) {
              collectionName = "map";
              if (keyValueTargetInfo.keyValueKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
                  keyValueTargetInfo.keyValueValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown) {
                collectionArgs = {
                    ir_lowerer::typeNameForValueKind(keyValueTargetInfo.keyValueKeyKind),
                    ir_lowerer::typeNameForValueKind(keyValueTargetInfo.keyValueValueKind),
                };
              }
              collectionStructPath = !keyValueTargetInfo.structTypeName.empty()
                                         ? keyValueTargetInfo.structTypeName
                                         : arrayVectorTargetInfo.structTypeName;
            } else if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
              collectionName = arrayVectorTargetInfo.isVectorTarget ? "vector" : "array";
              if (arrayVectorTargetInfo.elemKind != ir_lowerer::LocalInfo::ValueKind::Unknown) {
                collectionArgs = {ir_lowerer::typeNameForValueKind(arrayVectorTargetInfo.elemKind)};
              }
              collectionStructPath = arrayVectorTargetInfo.structTypeName;
            } else {
              return std::nullopt;
            }
          }

          auto supportsHelperForCollection = [&](const std::string &name) {
            if (collectionName == "map") {
              return name == "count" || name == "contains" || name == "tryAt" ||
                     name == "at" || name == "at_unsafe" || name == "insert";
            }
            if (collectionName == "array") {
              return name == "count" || name == "capacity" ||
                     name == "at" || name == "at_unsafe";
            }
            if (collectionName == "vector") {
              return name == "count" || name == "capacity" ||
                     name == "at" || name == "at_unsafe";
            }
            return false;
          };
          if (!supportsHelperForCollection(helperName)) {
            return std::nullopt;
          }
          ir_lowerer::LocalInfo materializedInfo;
          auto emitCollectionReceiverValue = [&](const Expr &valueExpr) {
            std::string accessName;
            auto getMaterializedArgsPackAccessName = [&](const Expr &candidate,
                                                         std::string &accessNameOut) {
              if (getBuiltinArrayAccessName(candidate, accessNameOut)) {
                return true;
              }
              if (candidate.kind != Expr::Kind::Call || candidate.args.size() != 2 ||
                  candidate.args.front().kind != Expr::Kind::Name ||
                  !candidate.namespacePrefix.empty()) {
                return false;
              }
              auto localIt = localsIn.find(candidate.args.front().name);
              if (localIt == localsIn.end() || !localIt->second.isArgsPack) {
                return false;
              }
              std::string rawName = candidate.name;
              if (!rawName.empty() && rawName.front() == '/') {
                rawName.erase(rawName.begin());
              }
              if (rawName.find('/') != std::string::npos) {
                return false;
              }
              if (rawName == "at" || rawName == "at_unsafe") {
                accessNameOut = rawName;
                return true;
              }
              return false;
            };
            if (valueExpr.kind == Expr::Kind::Call &&
                getMaterializedArgsPackAccessName(valueExpr, accessName) &&
                valueExpr.args.size() == 2) {
              const auto targetInfo =
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(
                      valueExpr.args.front(),
                      localsIn,
                      {},
                      lateCollectionSemanticProgram,
                      lateCollectionSemanticIndex);
              const bool isStructArgsPackAccess =
                  targetInfo.isArgsPackTarget &&
                  !targetInfo.isVectorTarget &&
                  !targetInfo.structTypeName.empty() &&
                  targetInfo.elemSlotCount > 0 &&
                  !targetInfo.isKeyValueTarget &&
                  !targetInfo.isWrappedKeyValueTarget;
              const bool isDirectMapArgsPackAccess =
                  targetInfo.isArgsPackTarget &&
                  (targetInfo.isKeyValueTarget ||
                   targetInfo.isWrappedKeyValueTarget) &&
                  targetInfo.elemSlotCount > 0;
              if (isStructArgsPackAccess || isDirectMapArgsPackAccess) {
                return ir_lowerer::emitArrayVectorIndexedAccess(
                    accessName,
                    valueExpr.args.front(),
                    valueExpr.args[1],
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
                    [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                    [&](size_t indexToPatch, uint64_t target) {
                      function.instructions[indexToPatch].imm = target;
                    },
                    error);
              }
            }
            return emitExpr(valueExpr, localsIn);
          };
          if (!collectionStructPath.empty() && collectionName != "map") {
            StructSlotLayoutInfo layout;
            if (!resolveStructSlotLayout(collectionStructPath, layout)) {
              error = "internal error: missing struct slot layout for " + collectionStructPath;
              return false;
            }

            const int32_t baseLocal = nextLocal;
            nextLocal += layout.totalSlots;
            materializedInfo.index = nextLocal++;
            materializedInfo.kind = ir_lowerer::LocalInfo::Kind::Value;
            materializedInfo.valueKind = ir_lowerer::LocalInfo::ValueKind::Int64;
            materializedInfo.structTypeName = collectionStructPath;
            materializedInfo.structSlotCount = layout.totalSlots;
            if (collectionArgs.size() == 2) {
              materializedInfo.keyValueKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              materializedInfo.keyValueValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
            }

            if (!emitCollectionReceiverValue(*receiverExpr)) {
              return false;
            }
            const int32_t srcPtrLocal = allocTempLocal();
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(srcPtrLocal)});
            function.instructions.push_back(
                {IrOpcode::PushI32, static_cast<uint64_t>(static_cast<int32_t>(layout.totalSlots - 1))});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(baseLocal)});
            function.instructions.push_back({IrOpcode::AddressOfLocal, static_cast<uint64_t>(baseLocal)});
            function.instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(materializedInfo.index)});
            if (!emitStructCopySlots(baseLocal, srcPtrLocal, layout.totalSlots)) {
              return false;
            }
            if (shouldDisarmStructCopySourceExpr(*receiverExpr)) {
              ir_lowerer::emitDisarmTemporaryStructAfterCopy(
                  [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
                  srcPtrLocal,
                  collectionStructPath);
            }
          } else {
            materializedInfo.index = allocTempLocal();
          }

          if (collectionName == "map") {
            if (collectionArgs.size() == 2) {
              materializedInfo.kind = materializedWrappedMapReceiver
                                          ? materializedMapReceiverKind
                                          : ir_lowerer::LocalInfo::Kind::Value;
              materializedInfo.keyValueKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              materializedInfo.keyValueValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
              materializedInfo.valueKind = materializedInfo.keyValueValueKind;
              if (collectionStructPath.rfind(
                      experimentalCollectionTypePath("map", "Map") + "__",
                      0) == 0) {
                materializedInfo.structTypeName = collectionStructPath;
                if (materializedInfo.structSlotCount <= 0) {
                  StructSlotLayoutInfo layout;
                  if (!resolveStructSlotLayout(collectionStructPath, layout)) {
                    return false;
                  }
                  materializedInfo.structSlotCount = layout.totalSlots;
                }
              }
            } else {
              return std::nullopt;
            }
          } else if (collectionName == "vector" && !resolvedCollectionFromDef) {
            return std::nullopt;
          } else {
            if (collectionArgs.size() != 1) {
              return std::nullopt;
            }
            materializedInfo.kind = (collectionName == "vector")
                                        ? ir_lowerer::LocalInfo::Kind::Vector
                                        : ir_lowerer::LocalInfo::Kind::Array;
            materializedInfo.valueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
          }

          if (collectionStructPath.empty() || collectionName == "map") {
            if (!emitCollectionReceiverValue(*receiverExpr)) {
              return false;
            }
            function.instructions.push_back(
                {IrOpcode::StoreLocal, static_cast<uint64_t>(materializedInfo.index)});
          }

          Expr rewrittenExpr = callExpr;
          Expr rewrittenReceiver;
          rewrittenReceiver.kind = Expr::Kind::Name;
          rewrittenReceiver.name = "__collection_receiver_" + std::to_string(materializedInfo.index);
          rewrittenExpr.args.front() = rewrittenReceiver;

          ir_lowerer::LocalMap rewrittenLocals = localsIn;
          rewrittenLocals.emplace(rewrittenReceiver.name, materializedInfo);
          return emitExpr(rewrittenExpr, rewrittenLocals);
        };
        if (const auto materializedReceiverResult = emitMaterializedCollectionReceiverExpr(expr);
            materializedReceiverResult.has_value()) {
          return *materializedReceiverResult;
        }
        auto rewriteBareKeyValueAccessMethodExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.size() != 2) {
            return false;
          }
          std::string accessName;
          if (!getBuiltinArrayAccessName(callExpr, accessName) ||
              (accessName != "at" && accessName != "at_unsafe")) {
            return false;
          }
          if (resolveDefinitionCall(callExpr) != nullptr) {
            return false;
          }
          if (callExpr.args.front().kind != Expr::Kind::Call) {
            const auto targetInfo =
                ir_lowerer::resolveCollectionPairTypeInfo(
                    callExpr.args.front(),
                    localsIn,
                    {},
                    semanticProgram,
                    &callResolutionAdapters.semanticProductTargets.semanticIndex);
            if (!targetInfo.isKeyValueTarget) {
              return false;
            }
            rewrittenExpr = callExpr;
            rewrittenExpr.isMethodCall = true;
            return true;
          }
          ir_lowerer::CollectionPairTypeInfo targetInfoOut;
          const Definition *callee =
              resolveCollectionExprDirectDefinition(callExpr.args.front());
          if (callee == nullptr) {
            return false;
          }
          std::string collectionName;
          std::vector<std::string> collectionArgs;
          if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) ||
              collectionName != "map" || collectionArgs.size() != 2) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.isMethodCall = true;
          return true;
        };
        Expr rewrittenBareKeyValueAccessMethodExpr;
        if (rewriteBareKeyValueAccessMethodExpr(expr, rewrittenBareKeyValueAccessMethodExpr)) {
          const std::string priorError = error;
          if (!emitExpr(rewrittenBareKeyValueAccessMethodExpr, localsIn)) {
            return false;
          }
          error = priorError;
          return true;
        }
