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
        auto mapHelperSurfaceMetadataForLowerEmitExpr = []() {
          return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
        };
        auto mapConstructorSurfaceMetadataForLowerEmitExpr = []() {
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
              const auto *metadata = mapHelperSurfaceMetadataForLowerEmitExpr();
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
        auto resolvePublishedLateMapMemberName =
            [&](const Expr &candidate, std::string &memberNameOut) {
              const auto *metadata = mapHelperSurfaceMetadataForLowerEmitExpr();
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
        auto resolvePublishedLateMapConstructorName =
            [&](const Expr &candidate, std::string &memberNameOut) {
              const auto *metadata =
                  mapConstructorSurfaceMetadataForLowerEmitExpr();
              return metadata != nullptr &&
                     resolvePublishedLateCollectionConstructorName(
                         candidate, metadata->id, memberNameOut);
            };
        auto isEntryArgsPackMapConstructorExpr = [&](const Expr &callExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
            return false;
          }
          std::string constructorName;
          if (!resolvePublishedLateMapConstructorName(callExpr,
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
        if (isEntryArgsPackMapConstructorExpr(expr)) {
          error = "native backend does not support variadic entry map constructors";
          return false;
        }
        auto rewriteExplicitBuiltinMapHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.empty()) {
            return false;
          }
          const auto *metadata = mapHelperSurfaceMetadataForLowerEmitExpr();
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
          if ((!resolveMapHelperAliasName(callExpr, helperName) &&
               !resolvePublishedLateMapMemberName(callExpr, helperName)) ||
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
        Expr rewrittenExplicitBuiltinMapHelperExpr;
        if (rewriteExplicitBuiltinMapHelperExpr(expr, rewrittenExplicitBuiltinMapHelperExpr)) {
          return emitExpr(rewrittenExplicitBuiltinMapHelperExpr, localsIn);
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
          ir_lowerer::LocalInfo::Kind materializedMapReceiverKind = ir_lowerer::LocalInfo::Kind::KeyValueCollection;
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
                if (resolveMapHelperAliasName(candidate, helperNameOut)) {
                  return true;
                }
                return resolvePublishedLateMapMemberName(candidate,
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
            auto isMapAccessValueCall = [&](const Expr &candidateExpr) {
              if (candidateExpr.kind != Expr::Kind::Call || candidateExpr.isMethodCall) {
                return false;
              }
              std::string helperAlias;
              if (!resolveMapHelperAliasName(candidateExpr, helperAlias)) {
                return false;
              }
              return helperAlias == "at" || helperAlias == "at_unsafe" ||
                     helperAlias == "tryAt" || helperAlias == "contains";
            };
            if (isMapAccessValueCall(*receiverExpr)) {
              return std::nullopt;
            }
            const auto inferCallMapTargetInfo =
                [&](const Expr &targetExpr, ir_lowerer::MapAccessTargetInfo &out) {
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
                    return ir_lowerer::inferForwardedMapAccessTargetInfo(
                        targetExpr, *callee, localsIn, {}, out);
                  }
                  out.isMapTarget = true;
                  out.keyValueKeyKind =
                      ir_lowerer::valueKindFromTypeName(inferredCollectionArgs.front());
                  out.keyValueValueKind =
                      ir_lowerer::valueKindFromTypeName(inferredCollectionArgs.back());
                  return out.keyValueKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
                         out.keyValueValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown;
                };
            const auto mapTargetInfo =
                ir_lowerer::resolveMapAccessTargetInfo(
                    *receiverExpr,
                    localsIn,
                    inferCallMapTargetInfo,
                    lateCollectionSemanticProgram,
                    lateCollectionSemanticIndex);
            const auto arrayVectorTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    *receiverExpr,
                    localsIn,
                    {},
                    lateCollectionSemanticProgram,
                    lateCollectionSemanticIndex);
            if (mapTargetInfo.isMapTarget && arrayVectorTargetInfo.isWrappedMapTarget) {
              materializedWrappedMapReceiver = true;
              materializedMapReceiverKind = arrayVectorTargetInfo.argsPackElementKind;
            }
            if (mapTargetInfo.isMapTarget) {
              collectionName = "map";
              if (mapTargetInfo.keyValueKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
                  mapTargetInfo.keyValueValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown) {
                collectionArgs = {
                    ir_lowerer::typeNameForValueKind(mapTargetInfo.keyValueKeyKind),
                    ir_lowerer::typeNameForValueKind(mapTargetInfo.keyValueValueKind),
                };
              }
              collectionStructPath = !mapTargetInfo.structTypeName.empty()
                                         ? mapTargetInfo.structTypeName
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
            if (valueExpr.kind == Expr::Kind::Call &&
                getBuiltinArrayAccessName(valueExpr, accessName) &&
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
                  !targetInfo.isMapTarget &&
                  !targetInfo.isWrappedMapTarget;
              const bool isDirectMapArgsPackAccess =
                  targetInfo.isArgsPackTarget &&
                  targetInfo.isMapTarget &&
                  !targetInfo.isWrappedMapTarget &&
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
            materializedInfo.kind = collectionName == "map"
                                        ? ir_lowerer::LocalInfo::Kind::KeyValueCollection
                                        : ir_lowerer::LocalInfo::Kind::Value;
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
                                          : ir_lowerer::LocalInfo::Kind::KeyValueCollection;
              materializedInfo.keyValueKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              materializedInfo.keyValueValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
              materializedInfo.valueKind = materializedInfo.keyValueValueKind;
              materializedInfo.referenceToKeyValueCollection =
                  materializedWrappedMapReceiver &&
                  materializedMapReceiverKind == ir_lowerer::LocalInfo::Kind::Reference;
              materializedInfo.pointerToKeyValueCollection =
                  materializedWrappedMapReceiver &&
                  materializedMapReceiverKind == ir_lowerer::LocalInfo::Kind::Pointer;
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
        auto normalizeLateArrayVectorCollectionName =
            [&](const std::string &collectionName) {
              const std::string normalized =
                  ir_lowerer::trimTemplateTypeText(collectionName);
              if (normalized == "array" || normalized == "/array" ||
                  normalized == "args") {
                return std::string("array");
              }
              return ir_lowerer::normalizeCollectionBindingTypeName(normalized);
            };
        auto populateLateArrayVectorTargetFromElementType =
            [&](const std::string &collectionName,
                const std::string &elementTypeText,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut,
                std::vector<std::string> *collectionArgsOut) {
              const std::string normalizedCollectionName =
                  normalizeLateArrayVectorCollectionName(collectionName);
              if (normalizedCollectionName != "array" &&
                  normalizedCollectionName != "vector") {
                return false;
              }
              const std::string normalizedElementType =
                  ir_lowerer::trimTemplateTypeText(elementTypeText);
              if (normalizedElementType.empty()) {
                return false;
              }
              ir_lowerer::ArrayVectorAccessTargetInfo candidate;
              candidate.isArrayOrVectorTarget = true;
              candidate.isVectorTarget = (normalizedCollectionName == "vector");
              candidate.elemKind =
                  ir_lowerer::valueKindFromTypeName(normalizedElementType);
              if (candidate.isVectorTarget) {
                candidate.structTypeName =
                    ir_lowerer::specializedExperimentalVectorStructPathForElementType(
                        normalizedElementType);
              }
              targetInfoOut = candidate;
              if (collectionArgsOut != nullptr) {
                *collectionArgsOut = {normalizedElementType};
              }
              return true;
            };
        auto tryPopulateLateArrayVectorTargetFromTypeText =
            [&](SymbolId typeTextId,
                const std::string &typeText,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut,
                std::vector<std::string> *collectionArgsOut) {
              std::string resolvedTypeText =
                  ir_lowerer::resolveSemanticProductTypeText(
                      callResolutionAdapters.semanticProgram,
                      typeText,
                      typeTextId);
              while (true) {
                std::string base;
                std::string argText;
                if (!splitTemplateTypeName(
                        ir_lowerer::trimTemplateTypeText(resolvedTypeText),
                        base,
                        argText)) {
                  return false;
                }
                const std::string normalizedBase =
                    ir_lowerer::trimTemplateTypeText(base);
                if (normalizedBase == "Reference" ||
                    normalizedBase == "/Reference" ||
                    normalizedBase == "Pointer" ||
                    normalizedBase == "/Pointer") {
                  std::vector<std::string> wrappedArgs;
                  if (!splitTemplateArgs(argText, wrappedArgs) ||
                      wrappedArgs.size() != 1) {
                    return false;
                  }
                  resolvedTypeText = wrappedArgs.front();
                  continue;
                }
                std::vector<std::string> collectionArgs;
                if (!splitTemplateArgs(argText, collectionArgs) ||
                    collectionArgs.size() != 1) {
                  return false;
                }
                return populateLateArrayVectorTargetFromElementType(
                    normalizedBase,
                    collectionArgs.front(),
                    targetInfoOut,
                    collectionArgsOut);
              }
            };
        auto tryPopulateLateArrayVectorTargetFromSemanticCollection =
            [&](const Expr &targetExpr,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut,
                std::vector<std::string> *collectionArgsOut) {
              if (callResolutionAdapters.semanticProgram == nullptr ||
                  targetExpr.semanticNodeId == 0) {
                return false;
              }
              const auto *collectionFact =
                  ir_lowerer::findSemanticProductCollectionSpecialization(
                      callResolutionAdapters.semanticProductTargets.semanticIndex,
                      targetExpr);
              if (collectionFact == nullptr) {
                return false;
              }
              const std::string collectionName =
                  ir_lowerer::resolveSemanticProductTypeText(
                      callResolutionAdapters.semanticProgram,
                      collectionFact->collectionFamily,
                      collectionFact->collectionFamilyId);
              std::string elementType =
                  ir_lowerer::resolveSemanticProductTypeText(
                      callResolutionAdapters.semanticProgram,
                      collectionFact->elementTypeText,
                      collectionFact->elementTypeTextId);
              if (elementType.empty()) {
                elementType = ir_lowerer::resolveSemanticProductTypeText(
                    callResolutionAdapters.semanticProgram,
                    collectionFact->valueTypeText,
                    collectionFact->valueTypeTextId);
              }
              return populateLateArrayVectorTargetFromElementType(
                  collectionName,
                  elementType,
                  targetInfoOut,
                  collectionArgsOut);
            };
        auto tryPopulateLateArrayVectorTargetFromSemanticFacts =
            [&](const Expr &targetExpr,
                ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut,
                std::vector<std::string> *collectionArgsOut) {
              if (callResolutionAdapters.semanticProgram == nullptr ||
                  targetExpr.semanticNodeId == 0) {
                return false;
              }
              if (tryPopulateLateArrayVectorTargetFromSemanticCollection(
                      targetExpr, targetInfoOut, collectionArgsOut)) {
                return true;
              }
              const auto &semanticIndex =
                  callResolutionAdapters.semanticProductTargets.semanticIndex;
              if (const auto *queryFact =
                      ir_lowerer::findSemanticProductQueryFact(
                          callResolutionAdapters.semanticProgram,
                          semanticIndex,
                          targetExpr);
                  queryFact != nullptr) {
                if (tryPopulateLateArrayVectorTargetFromTypeText(
                        queryFact->bindingTypeTextId,
                        queryFact->bindingTypeText,
                        targetInfoOut,
                        collectionArgsOut) ||
                    tryPopulateLateArrayVectorTargetFromTypeText(
                        queryFact->queryTypeTextId,
                        queryFact->queryTypeText,
                        targetInfoOut,
                        collectionArgsOut) ||
                    tryPopulateLateArrayVectorTargetFromTypeText(
                        queryFact->receiverBindingTypeTextId,
                        queryFact->receiverBindingTypeText,
                        targetInfoOut,
                        collectionArgsOut)) {
                  return true;
                }
              }
              if (const auto *bindingFact =
                      ir_lowerer::findSemanticProductBindingFact(
                          semanticIndex, targetExpr);
                  bindingFact != nullptr &&
                  tryPopulateLateArrayVectorTargetFromTypeText(
                      bindingFact->bindingTypeTextId,
                      bindingFact->bindingTypeText,
                      targetInfoOut,
                      collectionArgsOut)) {
                return true;
              }
              if (const auto *localAutoFact =
                      ir_lowerer::findSemanticProductLocalAutoFact(
                          callResolutionAdapters.semanticProgram,
                          semanticIndex,
                          targetExpr);
                  localAutoFact != nullptr &&
                  tryPopulateLateArrayVectorTargetFromTypeText(
                      localAutoFact->bindingTypeTextId,
                      localAutoFact->bindingTypeText,
                      targetInfoOut,
                      collectionArgsOut)) {
                return true;
              }
              return false;
            };
        auto rewriteBareVectorHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.empty() ||
              !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
            return false;
          }
          const std::string helperName = callExpr.name;
          if (helperName != "count" && helperName != "capacity" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(
              callExpr.args.front(),
              localsIn,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                targetInfoOut = {};
                if (tryPopulateLateArrayVectorTargetFromSemanticFacts(
                        targetCallExpr, targetInfoOut, nullptr)) {
                  return true;
                }
                const Definition *callee =
                    resolveCollectionExprDirectDefinition(targetCallExpr);
                if (callee == nullptr) {
                  return false;
                }
                std::string collectionName;
                std::vector<std::string> collectionArgs;
                if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                  return false;
                }
                if ((collectionName != "array" && collectionName != "vector") || collectionArgs.size() != 1) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = (collectionName == "vector");
                targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
                return true;
              });
          if (!targetInfo.isVectorTarget) {
            return false;
          }
          std::string elemTypeName;
          if (callExpr.args.front().kind == Expr::Kind::Call) {
            ir_lowerer::ArrayVectorAccessTargetInfo semanticTargetInfo;
            std::vector<std::string> semanticCollectionArgs;
            if (tryPopulateLateArrayVectorTargetFromSemanticFacts(
                    callExpr.args.front(),
                    semanticTargetInfo,
                    &semanticCollectionArgs) &&
                semanticTargetInfo.isVectorTarget &&
                semanticCollectionArgs.size() == 1) {
              elemTypeName = semanticCollectionArgs.front();
            }
            std::string receiverCollectionName;
            if (elemTypeName.empty() &&
                getBuiltinCollectionName(callExpr.args.front(), receiverCollectionName) &&
                (receiverCollectionName == "array" || receiverCollectionName == "vector") &&
                callExpr.args.front().templateArgs.size() == 1) {
              elemTypeName = callExpr.args.front().templateArgs.front();
            }
            if (elemTypeName.empty()) {
              if (const Definition *receiverDef =
                      resolveCollectionExprDirectDefinition(callExpr.args.front())) {
                std::string receiverCollectionNameFromDef;
                std::vector<std::string> receiverCollectionArgs;
                if (ir_lowerer::inferDeclaredReturnCollection(
                        *receiverDef, receiverCollectionNameFromDef, receiverCollectionArgs) &&
                    (receiverCollectionNameFromDef == "array" ||
                     receiverCollectionNameFromDef == "vector") &&
                    receiverCollectionArgs.size() == 1) {
                  elemTypeName = receiverCollectionArgs.front();
                }
              }
            }
          }

          auto tryRewritePath = [&](const std::string &path) {
            Expr candidate = callExpr;
            candidate.name = path;
            candidate.namespacePrefix.clear();
            candidate.isMethodCall = false;
            if (candidate.templateArgs.empty() && !elemTypeName.empty()) {
              candidate.templateArgs = {elemTypeName};
            }
            if (resolveDefinitionCall(candidate) == nullptr) {
              return false;
            }
            rewrittenExpr = std::move(candidate);
            return true;
          };

          return tryRewritePath(collectionMemberPath("vector", helperName));
        };
        Expr rewrittenBareVectorHelperExpr;
        if (rewriteBareVectorHelperExpr(expr, rewrittenBareVectorHelperExpr)) {
          return emitExpr(rewrittenBareVectorHelperExpr, localsIn);
        }
        auto rewriteBareVectorMethodHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty() ||
              !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
            return false;
          }
          const std::string helperName = callExpr.name;
          if (helperName != "count" && helperName != "capacity" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          const size_t expectedArgCount =
              (helperName == "at" || helperName == "at_unsafe") ? 2u : 1u;
          if (callExpr.args.size() != expectedArgCount) {
            return false;
          }
          std::vector<std::string> receiverCollectionArgs;
          bool knownVectorCallReceiver = false;
          if (callExpr.args.front().kind == Expr::Kind::Call) {
            const Expr &receiverCallExpr = callExpr.args.front();
            ir_lowerer::ArrayVectorAccessTargetInfo semanticTargetInfo;
            if (tryPopulateLateArrayVectorTargetFromSemanticFacts(
                    receiverCallExpr, semanticTargetInfo, &receiverCollectionArgs) &&
                semanticTargetInfo.isVectorTarget &&
                receiverCollectionArgs.size() == 1) {
              knownVectorCallReceiver = true;
            }
            if (!knownVectorCallReceiver) {
              knownVectorCallReceiver =
                  receiverCallExpr.name.rfind(
                      collectionMemberPath("vector", "vector"), 0) == 0 ||
                  receiverCallExpr.name.rfind(
                      experimentalCollectionMemberPath("vector", "vector"), 0) == 0;
            }
            std::string receiverCollectionName;
            if (!knownVectorCallReceiver &&
                getBuiltinCollectionName(receiverCallExpr, receiverCollectionName) &&
                receiverCollectionName == "vector" &&
                receiverCallExpr.templateArgs.size() == 1) {
              receiverCollectionArgs = receiverCallExpr.templateArgs;
              knownVectorCallReceiver = true;
            } else if (!knownVectorCallReceiver) {
              if (const Definition *receiverDef =
                      resolveCollectionExprDirectDefinition(receiverCallExpr)) {
                if (!ir_lowerer::inferDeclaredReturnCollection(
                        *receiverDef, receiverCollectionName, receiverCollectionArgs) ||
                    receiverCollectionName != "vector" ||
                    receiverCollectionArgs.size() != 1) {
                  receiverCollectionArgs.clear();
                } else {
                  knownVectorCallReceiver = true;
                }
              }
            }
          }
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(
              callExpr.args.front(),
              localsIn,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                targetInfoOut = {};
                if (tryPopulateLateArrayVectorTargetFromSemanticFacts(
                        targetCallExpr, targetInfoOut, nullptr)) {
                  return true;
                }
                const Definition *callee =
                    resolveCollectionExprDirectDefinition(targetCallExpr);
                if (callee == nullptr) {
                  return false;
                }
                std::string collectionName;
                std::vector<std::string> collectionArgs;
                if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
                  return false;
                }
                if ((collectionName != "array" && collectionName != "vector") || collectionArgs.size() != 1) {
                  return false;
                }
                targetInfoOut.isArrayOrVectorTarget = true;
                targetInfoOut.isVectorTarget = (collectionName == "vector");
                targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
                return true;
              });
          if (!targetInfo.isVectorTarget && !knownVectorCallReceiver) {
            return false;
          }
          if (knownVectorCallReceiver) {
            auto tryRewriteNamespace = [&](const std::string &namespacePrefix) {
              Expr candidate = callExpr;
              candidate.isMethodCall = false;
              candidate.namespacePrefix = namespacePrefix;
              candidate.name = helperName;
              if (candidate.templateArgs.empty() && !receiverCollectionArgs.empty()) {
                candidate.templateArgs = receiverCollectionArgs;
              }
              rewrittenExpr = std::move(candidate);
              return true;
            };
            std::string vectorNamespace = collectionMemberRoot("vector");
            vectorNamespace.pop_back();
            return tryRewriteNamespace(vectorNamespace);
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.name = helperName;
          return true;
        };
        Expr rewrittenBareVectorMethodHelperExpr;
        if (rewriteBareVectorMethodHelperExpr(expr, rewrittenBareVectorMethodHelperExpr)) {
          const std::string priorError = error;
          if (!emitExpr(rewrittenBareVectorMethodHelperExpr, localsIn)) {
            return false;
          }
          error = priorError;
          return true;
        }
        auto rewriteBareMapAccessMethodExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.args.size() != 2) {
            return false;
          }
          std::string accessName;
          if (!getBuiltinArrayAccessName(callExpr, accessName) ||
              (accessName != "at" && accessName != "at_unsafe")) {
            return false;
          }
          if (callExpr.args.front().kind != Expr::Kind::Call) {
            return false;
          }
          if (resolveDefinitionCall(callExpr) != nullptr) {
            return false;
          }
          ir_lowerer::MapAccessTargetInfo targetInfoOut;
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
        Expr rewrittenBareMapAccessMethodExpr;
        if (rewriteBareMapAccessMethodExpr(expr, rewrittenBareMapAccessMethodExpr)) {
          const std::string priorError = error;
          if (!emitExpr(rewrittenBareMapAccessMethodExpr, localsIn)) {
            return false;
          }
          error = priorError;
          return true;
        }
