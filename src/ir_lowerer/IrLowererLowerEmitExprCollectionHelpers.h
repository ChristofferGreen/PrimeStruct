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
          if (rewrittenExpr.templateArgs.empty()) {
            for (const auto &transform : callee->transforms) {
              if (transform.name != "return" || transform.templateArgs.size() != 1) {
                continue;
              }
              std::string base;
              std::string argList;
              if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, argList)) {
                break;
              }
              if (normalizeCollectionBindingTypeName(base) != "map") {
                break;
              }
              std::vector<std::string> templateArgs;
              if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 2) {
                break;
              }
              templateArgs[0] = trimTemplateTypeText(templateArgs[0]);
              templateArgs[1] = trimTemplateTypeText(templateArgs[1]);
              rewrittenExpr.templateArgs = std::move(templateArgs);
              break;
            }
          }
          return true;
        };
        Expr rewrittenStdlibMapConstructorExpr;
        if (rewriteStdlibMapConstructorExpr(expr, rewrittenStdlibMapConstructorExpr)) {
          return emitExpr(rewrittenStdlibMapConstructorExpr, localsIn);
        }
        auto rewriteTemporaryMapMethodToDirectHelperExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty()) {
            return false;
          }
          std::string helperName = callExpr.name;
          if (!helperName.empty() && helperName.front() == '/') {
            helperName.erase(helperName.begin());
          }
          if (helperName != "count" && helperName != "contains" && helperName != "tryAt" &&
              helperName != "at" && helperName != "at_unsafe") {
            return false;
          }
          const Expr &receiverExpr = callExpr.args.front();
          if (receiverExpr.kind != Expr::Kind::Call ||
              !ir_lowerer::resolveMapAccessTargetInfo(receiverExpr, localsIn).isMapTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.namespacePrefix = "/std/collections/map";
          rewrittenExpr.name = helperName;
          rewrittenExpr.templateArgs.clear();
          return true;
        };
        Expr rewrittenTemporaryMapMethodHelperExpr;
        if (rewriteTemporaryMapMethodToDirectHelperExpr(expr, rewrittenTemporaryMapMethodHelperExpr)) {
          return emitExpr(rewrittenTemporaryMapMethodHelperExpr, localsIn);
        }
        auto emitMaterializedCollectionReceiverExpr = [&](const Expr &callExpr) -> std::optional<bool> {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return std::nullopt;
          }

          std::string helperName;
          const Expr *receiverExpr = nullptr;
          auto matchesDirectHelperName = [&](const Expr &candidate, std::string_view bareName) {
            if (isSimpleCallName(candidate, bareName.data())) {
              return true;
            }
            std::string normalizedName = candidate.name;
            if (!normalizedName.empty() && normalizedName.front() == '/') {
              normalizedName.erase(normalizedName.begin());
            }
            return normalizedName == bareName ||
                   normalizedName == "map/" + std::string(bareName) ||
                   normalizedName == "std/collections/map/" + std::string(bareName);
          };
          if (callExpr.isMethodCall) {
            helperName = callExpr.name;
            receiverExpr = &callExpr.args.front();
          } else if (getBuiltinArrayAccessName(callExpr, helperName) && callExpr.args.size() == 2) {
            receiverExpr = &callExpr.args.front();
          } else if ((matchesDirectHelperName(callExpr, "count") ||
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
            receiverExpr = &callExpr.args.front();
          } else {
            return std::nullopt;
          }

          if (receiverExpr == nullptr || receiverExpr->kind != Expr::Kind::Call) {
            return std::nullopt;
          }

          const Definition *receiverDef = resolveDefinitionCall(*receiverExpr);
          if (receiverDef == nullptr) {
            return std::nullopt;
          }

          std::string collectionName;
          std::vector<std::string> collectionArgs;
          if (!ir_lowerer::inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs)) {
            return std::nullopt;
          }

          auto supportsHelperForCollection = [&](const std::string &name) {
            if (collectionName == "map") {
              return name == "count" || name == "contains" || name == "tryAt" ||
                     name == "at" || name == "at_unsafe";
            }
            if (collectionName == "vector" || collectionName == "array") {
              return name == "count" || name == "capacity" ||
                     name == "at" || name == "at_unsafe";
            }
            return false;
          };
          if (!supportsHelperForCollection(helperName)) {
            return std::nullopt;
          }

          ir_lowerer::LocalInfo materializedInfo;
          materializedInfo.index = allocTempLocal();
          if (collectionName == "map") {
            if (collectionArgs.size() != 2) {
              return std::nullopt;
            }
            materializedInfo.kind = ir_lowerer::LocalInfo::Kind::Map;
            materializedInfo.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
            materializedInfo.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
            materializedInfo.valueKind = materializedInfo.mapValueKind;
          } else {
            if (collectionArgs.size() != 1) {
              return std::nullopt;
            }
            materializedInfo.kind = (collectionName == "vector")
                                        ? ir_lowerer::LocalInfo::Kind::Vector
                                        : ir_lowerer::LocalInfo::Kind::Array;
            materializedInfo.valueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
          }

          if (!emitExpr(*receiverExpr, localsIn)) {
            return false;
          }
          function.instructions.push_back(
              {IrOpcode::StoreLocal, static_cast<uint64_t>(materializedInfo.index)});

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
