        auto rewriteStdlibMapConstructorExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall) {
            return false;
          }
          const Definition *callee = resolveDefinitionCall(callExpr);
          if (callee == nullptr) {
            return false;
          }
          auto matchesWrapperPath = [&](std::string_view basePath) {
            return callee->fullPath == basePath ||
                   callee->fullPath.rfind(std::string(basePath) + "__t", 0) == 0;
          };
          const bool isStdlibMapConstructor =
              matchesWrapperPath("/std/collections/map/map") ||
              matchesWrapperPath("/std/collections/mapNew") ||
              matchesWrapperPath("/std/collections/mapSingle") ||
              matchesWrapperPath("/std/collections/mapDouble") ||
              matchesWrapperPath("/std/collections/mapPair") ||
              matchesWrapperPath("/std/collections/mapTriple") ||
              matchesWrapperPath("/std/collections/mapQuad") ||
              matchesWrapperPath("/std/collections/mapQuint") ||
              matchesWrapperPath("/std/collections/mapSext") ||
              matchesWrapperPath("/std/collections/mapSept") ||
              matchesWrapperPath("/std/collections/mapOct");
          if (!isStdlibMapConstructor) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = "/map/map";
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.semanticNodeId = 0;
          if (rewrittenExpr.templateArgs.empty()) {
            std::string collectionName;
            std::vector<std::string> collectionArgs;
            if (ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) &&
                collectionName == "map" && collectionArgs.size() == 2) {
              collectionArgs[0] = trimTemplateTypeText(collectionArgs[0]);
              collectionArgs[1] = trimTemplateTypeText(collectionArgs[1]);
              rewrittenExpr.templateArgs = std::move(collectionArgs);
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
            }
          }
          return true;
        };
        Expr rewrittenStdlibMapConstructorExpr;
        if (rewriteStdlibMapConstructorExpr(expr, rewrittenStdlibMapConstructorExpr)) {
          return emitExpr(rewrittenStdlibMapConstructorExpr, localsIn);
        }
        auto rejectCanonicalVectorTemporaryReceiverExpr = [&](const Expr &callExpr) -> bool {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }

          std::string helperName;
          const Expr *receiverExpr = nullptr;
          if (callExpr.isMethodCall) {
            helperName = callExpr.name;
            receiverExpr = &callExpr.args.front();
          } else if (callExpr.args.size() == 2 && getBuiltinArrayAccessName(callExpr, helperName)) {
            receiverExpr = &callExpr.args.front();
          } else {
            std::string normalizedName = callExpr.name;
            if (!normalizedName.empty() && normalizedName.front() == '/') {
              normalizedName.erase(normalizedName.begin());
            }
            auto matchDirectVectorHelper = [&](std::string_view path) {
              return normalizedName == path;
            };
            if (matchDirectVectorHelper("std/collections/vector/count")) {
              helperName = "count";
            } else if (matchDirectVectorHelper("std/collections/vector/capacity")) {
              helperName = "capacity";
            } else if (matchDirectVectorHelper("std/collections/vector/at")) {
              helperName = "at";
            } else if (matchDirectVectorHelper("std/collections/vector/at_unsafe")) {
              helperName = "at_unsafe";
            } else {
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

          const Definition *receiverDef = resolveDefinitionCall(*receiverExpr);
          if (receiverDef == nullptr) {
            return false;
          }
          if (receiverDef->fullPath.rfind("/std/collections/vector/vector", 0) != 0) {
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
          ir_lowerer::LocalInfo::Kind materializedMapReceiverKind = ir_lowerer::LocalInfo::Kind::Map;
          auto matchesDirectHelperName = [&](const Expr &candidate, std::string_view bareName) {
            auto matchesResolvedPath = [&](std::string_view basePath) {
              const std::string resolvedPath = resolveExprPath(candidate);
              return resolvedPath == basePath ||
                     resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
            };
            if (isSimpleCallName(candidate, bareName.data())) {
              return true;
            }
            std::string aliasName;
            if (resolveMapHelperAliasName(candidate, aliasName) && aliasName == bareName) {
              return true;
            }
            std::string normalizedName = candidate.name;
            if (!normalizedName.empty() && normalizedName.front() == '/') {
              normalizedName.erase(normalizedName.begin());
            }
            if (normalizedName == bareName ||
                normalizedName == "map/" + std::string(bareName) ||
                normalizedName == "std/collections/map/" + std::string(bareName)) {
              return true;
            }
            if (bareName == "at") {
              return matchesResolvedPath("/std/collections/map/at") ||
                     matchesResolvedPath("/std/collections/mapAt");
            }
            if (bareName == "at_unsafe") {
              return matchesResolvedPath("/std/collections/map/at_unsafe") ||
                     matchesResolvedPath("/std/collections/mapAtUnsafe");
            }
            if (bareName == "count") {
              return matchesResolvedPath("/std/collections/map/count") ||
                     matchesResolvedPath("/std/collections/mapCount");
            }
            if (bareName == "contains") {
              return matchesResolvedPath("/std/collections/mapContains");
            }
            if (bareName == "tryAt") {
              return matchesResolvedPath("/std/collections/mapTryAt");
            }
            return false;
          };
          if (callExpr.isMethodCall) {
            helperName = callExpr.name;
            receiverExpr = &callExpr.args.front();
          } else if (getBuiltinArrayAccessName(callExpr, helperName) && callExpr.args.size() == 2) {
            receiverExpr = &callExpr.args.front();
          } else if ((matchesDirectHelperName(callExpr, "count") ||
                      matchesDirectHelperName(callExpr, "at") ||
                      matchesDirectHelperName(callExpr, "at_unsafe") ||
                      matchesDirectHelperName(callExpr, "contains") ||
                      matchesDirectHelperName(callExpr, "tryAt") ||
                      matchesDirectHelperName(callExpr, "capacity")) &&
                     !callExpr.args.empty()) {
            helperName = callExpr.name;
            if (!helperName.empty() && helperName.front() == '/') {
              helperName.erase(helperName.begin());
            }
            const size_t lastSlash = helperName.find_last_of('/');
            if (lastSlash != std::string::npos) {
              helperName = helperName.substr(lastSlash + 1);
            }
            const size_t specializationSuffix = helperName.find("__t");
            if (specializationSuffix != std::string::npos) {
              helperName.erase(specializationSuffix);
            }
            if (helperName == "mapCount") {
              helperName = "count";
            } else if (helperName == "mapCountRef") {
              helperName = "count";
            } else if (helperName == "mapContains") {
              helperName = "contains";
            } else if (helperName == "mapContainsRef") {
              helperName = "contains";
            } else if (helperName == "mapTryAt") {
              helperName = "tryAt";
            } else if (helperName == "mapTryAtRef") {
              helperName = "tryAt";
            } else if (helperName == "mapAt") {
              helperName = "at";
            } else if (helperName == "mapAtRef") {
              helperName = "at";
            } else if (helperName == "mapAtUnsafe") {
              helperName = "at_unsafe";
            } else if (helperName == "mapAtUnsafeRef") {
              helperName = "at_unsafe";
            } else if (helperName == "mapInsertRef") {
              helperName = "insert";
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
          const Definition *receiverDef = resolveDefinitionCall(*receiverExpr);
          const bool resolvedCollectionFromDef =
              receiverDef != nullptr &&
              ir_lowerer::inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs);
          if (resolvedCollectionFromDef) {
            collectionStructPath = inferStructExprPath(*receiverExpr, localsIn);
            if (collectionStructPath.empty() &&
                receiverDef->fullPath.rfind("/std/collections/experimental_map/", 0) == 0) {
              const std::string suffix =
                  receiverDef->fullPath.substr(std::string("/std/collections/experimental_map/").size());
              auto remapExperimentalMapConstructor = [&](std::string_view helperStem) -> std::string {
                const std::string helperPrefix = std::string(helperStem) + "__";
                if (suffix.rfind(helperPrefix, 0) != 0) {
                  return {};
                }
                return "/std/collections/experimental_map/Map__" + suffix.substr(helperPrefix.size());
              };
              if (std::string structPath = remapExperimentalMapConstructor("mapNew"); !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapSingle");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapDouble");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapPair");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapTriple");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapQuad");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapQuint");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapSext");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else if (std::string structPath = remapExperimentalMapConstructor("mapSept");
                         !structPath.empty()) {
                collectionStructPath = std::move(structPath);
              } else {
                collectionStructPath = remapExperimentalMapConstructor("mapOct");
              }
            }
          }
          if (!resolvedCollectionFromDef) {
            const auto mapTargetInfo = ir_lowerer::resolveMapAccessTargetInfo(*receiverExpr, localsIn);
            const auto arrayVectorTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(*receiverExpr, localsIn);
            if (mapTargetInfo.isMapTarget && arrayVectorTargetInfo.isWrappedMapTarget) {
              materializedWrappedMapReceiver = true;
              materializedMapReceiverKind = arrayVectorTargetInfo.argsPackElementKind;
            }
            if (mapTargetInfo.isMapTarget) {
              collectionName = "map";
              if (mapTargetInfo.mapKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
                  mapTargetInfo.mapValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown) {
                collectionArgs = {
                    ir_lowerer::typeNameForValueKind(mapTargetInfo.mapKeyKind),
                    ir_lowerer::typeNameForValueKind(mapTargetInfo.mapValueKind),
                };
              }
              collectionStructPath = arrayVectorTargetInfo.structTypeName;
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
                     name == "at" || name == "at_unsafe";
            }
            if (collectionName == "array") {
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
                  ir_lowerer::resolveArrayVectorAccessTargetInfo(valueExpr.args.front(), localsIn);
              const bool isStructArgsPackAccess =
                  targetInfo.isArgsPackTarget &&
                  !targetInfo.isVectorTarget &&
                  !targetInfo.structTypeName.empty() &&
                  targetInfo.elemSlotCount > 0 &&
                  !targetInfo.isMapTarget &&
                  !targetInfo.isWrappedMapTarget;
              if (isStructArgsPackAccess) {
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
                                        ? ir_lowerer::LocalInfo::Kind::Map
                                        : ir_lowerer::LocalInfo::Kind::Value;
            materializedInfo.valueKind = ir_lowerer::LocalInfo::ValueKind::Int64;
            materializedInfo.structTypeName = collectionStructPath;
            materializedInfo.structSlotCount = layout.totalSlots;
            if (collectionArgs.size() == 2) {
              materializedInfo.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              materializedInfo.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
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
            if (receiverExpr->kind == Expr::Kind::Call) {
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
                                          : ir_lowerer::LocalInfo::Kind::Map;
              materializedInfo.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              materializedInfo.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
              materializedInfo.valueKind = materializedInfo.mapValueKind;
              materializedInfo.referenceToMap =
                  materializedWrappedMapReceiver &&
                  materializedMapReceiverKind == ir_lowerer::LocalInfo::Kind::Reference;
              materializedInfo.pointerToMap =
                  materializedWrappedMapReceiver &&
                  materializedMapReceiverKind == ir_lowerer::LocalInfo::Kind::Pointer;
              if (collectionStructPath.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
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
          } else if (collectionName == "vector") {
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
                const Definition *callee = resolveDefinitionCall(targetCallExpr);
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
            std::string receiverCollectionName;
            if (getBuiltinCollectionName(callExpr.args.front(), receiverCollectionName) &&
                (receiverCollectionName == "array" || receiverCollectionName == "vector") &&
                callExpr.args.front().templateArgs.size() == 1) {
              elemTypeName = callExpr.args.front().templateArgs.front();
            } else if (const Definition *receiverDef = resolveDefinitionCall(callExpr.args.front())) {
              std::string receiverCollectionNameFromDef;
              std::vector<std::string> receiverCollectionArgs;
              if (ir_lowerer::inferDeclaredReturnCollection(
                      *receiverDef, receiverCollectionNameFromDef, receiverCollectionArgs) &&
                  (receiverCollectionNameFromDef == "array" || receiverCollectionNameFromDef == "vector") &&
                  receiverCollectionArgs.size() == 1) {
                elemTypeName = receiverCollectionArgs.front();
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

          return tryRewritePath("/std/collections/vector/" + helperName) ||
                 tryRewritePath("/vector/" + helperName);
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
            knownVectorCallReceiver =
                receiverCallExpr.name.rfind("/std/collections/vector/vector", 0) == 0 ||
                receiverCallExpr.name.rfind("/std/collections/experimental_vector/vector", 0) == 0 ||
                receiverCallExpr.name.rfind("/vector/vector", 0) == 0;
            std::string receiverCollectionName;
            if (getBuiltinCollectionName(receiverCallExpr, receiverCollectionName) &&
                receiverCollectionName == "vector" &&
                receiverCallExpr.templateArgs.size() == 1) {
              receiverCollectionArgs = receiverCallExpr.templateArgs;
              knownVectorCallReceiver = true;
            } else if (const Definition *receiverDef = resolveDefinitionCall(receiverCallExpr)) {
              if (!ir_lowerer::inferDeclaredReturnCollection(
                      *receiverDef, receiverCollectionName, receiverCollectionArgs) ||
                  receiverCollectionName != "vector" || receiverCollectionArgs.size() != 1) {
                receiverCollectionArgs.clear();
              } else {
                knownVectorCallReceiver = true;
              }
            }
          }
          const auto targetInfo = ir_lowerer::resolveArrayVectorAccessTargetInfo(
              callExpr.args.front(),
              localsIn,
              [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
                targetInfoOut = {};
                const Definition *callee = resolveDefinitionCall(targetCallExpr);
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
            return tryRewriteNamespace("/std/collections/vector") ||
                   tryRewriteNamespace("/vector");
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
          ir_lowerer::MapAccessTargetInfo targetInfoOut;
          const Definition *callee = resolveDefinitionCall(callExpr.args.front());
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
