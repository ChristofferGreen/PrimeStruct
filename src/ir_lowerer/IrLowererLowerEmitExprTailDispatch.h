        auto resolveTailDispatchDirectHelperPath = [&](const Expr &candidate) {
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
        auto resolveTailDispatchDirectHelperDefinition =
            [&](const Expr &candidate) -> const Definition * {
          if (const Definition *callee = resolveDefinitionCall(candidate);
              callee != nullptr) {
            return callee;
          }
          auto stripGeneratedLeafSuffix = [](std::string helperPath) {
            const size_t leafStart = helperPath.find_last_of('/');
            const size_t searchStart =
                leafStart == std::string::npos ? 0 : leafStart + 1;
            const size_t generatedSuffix = helperPath.find("__", searchStart);
            if (generatedSuffix != std::string::npos) {
              helperPath.erase(generatedSuffix);
            }
            return helperPath;
          };
          auto findDirectHelperDefinition = [&](const std::string &path)
              -> const Definition * {
            auto findByPath = [&](const std::string &lookupPath)
                -> const Definition * {
              auto defIt = defMap.find(lookupPath);
              if (defIt != defMap.end()) {
                return defIt->second;
              }
              auto matchesGeneratedLeafDefinition =
                  [&](const std::string &candidatePath,
                      const char *marker,
                      size_t markerSize) {
                    return candidatePath.rfind(lookupPath, 0) == 0 &&
                           candidatePath.compare(
                               lookupPath.size(), markerSize, marker) == 0 &&
                           candidatePath.find('/',
                                              lookupPath.size() + markerSize) ==
                               std::string::npos;
                  };
              for (const auto &[candidatePath, def] : defMap) {
                if (def == nullptr) {
                  continue;
                }
                if (matchesGeneratedLeafDefinition(candidatePath, "__t", 3) ||
                    matchesGeneratedLeafDefinition(candidatePath, "__ov", 4) ||
                    matchesGeneratedLeafDefinition(candidatePath, "<", 1)) {
                  return def;
                }
              }
              return nullptr;
            };
            if (const Definition *direct = findByPath(path);
                direct != nullptr) {
              return direct;
            }
            const std::string canonicalPath = stripGeneratedLeafSuffix(path);
            if (canonicalPath != path) {
              return findByPath(canonicalPath);
            }
            return nullptr;
          };
          const std::string rawPath =
              resolveTailDispatchDirectHelperPath(candidate);
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
        auto hasPublishedSemanticMapSurface = [&](const Expr &callExpr) {
          return findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, callExpr) ==
                     primec::StdlibSurfaceId::CollectionsMapHelpers ||
                 findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, callExpr) ==
                     primec::StdlibSurfaceId::CollectionsMapHelpers;
        };
        auto resolvePublishedTailDispatchMapHelperName =
            [&](const Expr &callExpr, std::string &helperNameOut) {
              helperNameOut.clear();
              return ir_lowerer::resolvePublishedSemanticStdlibSurfaceMemberName(
                         semanticProgram,
                         callExpr,
                         primec::StdlibSurfaceId::CollectionsMapHelpers,
                         helperNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceExprMemberName(
                         callExpr,
                         primec::StdlibSurfaceId::CollectionsMapHelpers,
                         helperNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                         resolveExprPath(callExpr),
                         primec::StdlibSurfaceId::CollectionsMapHelpers,
                         helperNameOut) ||
                     ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                         resolveTailDispatchDirectHelperPath(callExpr),
                         primec::StdlibSurfaceId::CollectionsMapHelpers,
                         helperNameOut);
            };
        auto resolveBuiltinMapHelperName =
            [&](const Expr &callExpr, bool allowMethodCall, std::string &helperNameOut) {
              helperNameOut.clear();
              if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
                return false;
              }
              if (allowMethodCall && callExpr.isMethodCall) {
                if (resolvePublishedTailDispatchMapHelperName(callExpr, helperNameOut)) {
                  return true;
                }
                if (callExpr.namespacePrefix.empty() &&
                    callExpr.name.find('/') == std::string::npos &&
                    (callExpr.name == "count" || callExpr.name == "contains" ||
                    callExpr.name == "tryAt" || callExpr.name == "at" ||
                     callExpr.name == "at_unsafe")) {
                  helperNameOut = callExpr.name;
                  return true;
                }
                return false;
              }
              if (callExpr.isMethodCall) {
                return false;
              }
              if (ir_lowerer::resolveMapHelperAliasName(callExpr, helperNameOut)) {
                return true;
              }
              return resolvePublishedTailDispatchMapHelperName(callExpr, helperNameOut);
            };
        const SemanticProductIndex tailDispatchMapSemanticIndex =
            ir_lowerer::buildSemanticProductIndex(semanticProgram);
        const SemanticProductIndex *const tailDispatchMapSemanticIndexPtr =
            semanticProgram == nullptr ? nullptr : &tailDispatchMapSemanticIndex;
        auto populateTailDispatchMapStructPathFromKinds =
            [&](ir_lowerer::MapAccessTargetInfo &targetInfo) {
              if (!targetInfo.structTypeName.empty() ||
                  targetInfo.mapKeyKind == LocalInfo::ValueKind::Unknown ||
                  targetInfo.mapValueKind == LocalInfo::ValueKind::Unknown) {
                return;
              }
              const std::string typeText =
                  ir_lowerer::collectionTypePath("map", false) + "<" +
                  ir_lowerer::typeNameForValueKind(targetInfo.mapKeyKind) +
                  ", " +
                  ir_lowerer::typeNameForValueKind(targetInfo.mapValueKind) +
                  ">";
              std::string structPath;
              if (ir_lowerer::resolveSpecializedExperimentalMapStructPathForBindingType(
                      typeText, structPath)) {
                targetInfo.structTypeName = std::move(structPath);
              }
            };
        const auto inferCallMapTargetInfo = [&](const Expr &targetExpr, ir_lowerer::MapAccessTargetInfo &out) {
          out = {};
          const Definition *callee =
              resolveTailDispatchDirectHelperDefinition(targetExpr);
          if (callee == nullptr) {
            return false;
          }
          std::string collectionName;
          std::vector<std::string> collectionArgs;
          if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) ||
              collectionName != "map" ||
              collectionArgs.size() != 2) {
            return ir_lowerer::inferForwardedMapAccessTargetInfo(
                targetExpr, *callee, localsIn, {}, out);
          }
          out.isMapTarget = true;
          out.mapKeyKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
          out.mapValueKind = ir_lowerer::valueKindFromTypeName(collectionArgs.back());
          const bool resolvedKinds =
              out.mapKeyKind != ir_lowerer::LocalInfo::ValueKind::Unknown &&
              out.mapValueKind != ir_lowerer::LocalInfo::ValueKind::Unknown;
          if (resolvedKinds) {
            populateTailDispatchMapStructPathFromKinds(out);
          }
          return resolvedKinds;
        };
        auto rewriteBuiltinMapInsertBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.size() != 3) {
            return false;
          }
          auto matchesPublishedMapInsertPath = [&](const Expr &candidate) {
            std::string helperName;
            if (hasPublishedSemanticMapSurface(candidate) &&
                resolvePublishedTailDispatchMapHelperName(candidate, helperName) &&
                (helperName == "insert" || helperName == "insert_ref")) {
              return true;
            }
            const std::string resolvedPath = resolveExprPath(candidate);
            return ir_lowerer::isPublishedStdlibSurfaceLoweringPath(
                       resolvedPath,
                       primec::StdlibSurfaceId::CollectionsMapHelpers) &&
                   ir_lowerer::resolvePublishedStdlibSurfaceMemberName(
                       resolvedPath,
                       primec::StdlibSurfaceId::CollectionsMapHelpers,
                       helperName) &&
                   (helperName == "insert" || helperName == "insert_ref");
          };
          auto stripGeneratedHelperSuffix = [](std::string helperName) {
            const size_t generatedSuffix = helperName.find("__");
            if (generatedSuffix != std::string::npos) {
              helperName.erase(generatedSuffix);
            }
            return helperName;
          };
          auto isDirectBareMapInsertHelperStem = [&](const Expr &candidate) {
            if (candidate.isMethodCall || candidate.name.empty()) {
              return false;
            }
            std::string helperName =
                resolveTailDispatchDirectHelperPath(candidate);
            if (!helperName.empty() && helperName.front() == '/') {
              helperName.erase(helperName.begin());
            }
            if (helperName.find('/') != std::string::npos) {
              return false;
            }
            helperName = stripGeneratedHelperSuffix(std::move(helperName));
            return helperName == "insert" || helperName == "insert_ref" ||
                   helperName == "Insert" || helperName == "InsertRef";
          };

          size_t receiverIndex = 0;
          if (callExpr.isMethodCall) {
            std::string helperName;
            if (!resolveBuiltinMapHelperName(callExpr, true, helperName) ||
                (helperName != "insert" && helperName != "insert_ref")) {
              return false;
            }
          } else {
            std::string helperName;
            if ((!ir_lowerer::resolveMapHelperAliasName(callExpr, helperName) ||
                 (helperName != "insert" && helperName != "insert_ref")) &&
                !isDirectBareMapInsertHelperStem(callExpr)) {
              return false;
            }
            if (hasNamedArguments(callExpr.argNames)) {
              for (size_t i = 0; i < callExpr.args.size(); ++i) {
                if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value() &&
                    *callExpr.argNames[i] == "values") {
                  receiverIndex = i;
                  break;
                }
              }
            }
          }

          if (receiverIndex >= callExpr.args.size()) {
            return false;
          }
          const auto targetInfo =
              ir_lowerer::resolveMapAccessTargetInfo(
                  callExpr.args[receiverIndex],
                  localsIn,
                  inferCallMapTargetInfo,
                  semanticProgram,
                  tailDispatchMapSemanticIndexPtr);
          if (!targetInfo.isMapTarget &&
              !matchesPublishedMapInsertPath(callExpr)) {
            return false;
          }
          if (targetInfo.isWrappedMapTarget ||
              ir_lowerer::isExperimentalMapStructTypePath(targetInfo.structTypeName)) {
            return false;
          }

          rewrittenExpr = callExpr;
          rewrittenExpr.name = ir_lowerer::collectionMemberPath("map", "insert");
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.isFieldAccess = false;
          // Force rewritten rooted helper calls to resolve from their new path
          // instead of stale semantic-product call-target facts.
          rewrittenExpr.semanticNodeId = 0;
          if (rewrittenExpr.templateArgs.empty() &&
              targetInfo.mapKeyKind != LocalInfo::ValueKind::Unknown &&
              targetInfo.mapValueKind != LocalInfo::ValueKind::Unknown) {
            rewrittenExpr.templateArgs = {
                ir_lowerer::typeNameForValueKind(targetInfo.mapKeyKind),
                ir_lowerer::typeNameForValueKind(targetInfo.mapValueKind),
            };
          }
          return true;
        };
        auto rewriteCanonicalMapHelperForExperimentalReceiverExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          std::string helperName;
          if (!resolveBuiltinMapHelperName(callExpr, true, helperName)) {
            return false;
          }
          if (hasPublishedSemanticMapSurface(callExpr)) {
            return false;
          }
          if (ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  resolveTailDispatchDirectHelperPath(callExpr),
                  primec::StdlibSurfaceId::CollectionsMapHelpers) ||
              (hasPublishedSemanticMapSurface(callExpr) &&
               ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                   resolveExprPath(callExpr),
                   primec::StdlibSurfaceId::CollectionsMapHelpers))) {
            return false;
          }
          const size_t expectedArgCount = helperName == "count" ? 1u : 2u;
          if (callExpr.args.size() != expectedArgCount) {
            return false;
          }
          const SemanticProductIndex canonicalMapHelperSemanticIndex =
              ir_lowerer::buildSemanticProductIndex(semanticProgram);
          const SemanticProductIndex *const canonicalMapHelperSemanticIndexPtr =
              semanticProgram == nullptr ? nullptr : &canonicalMapHelperSemanticIndex;
          auto isSpecializedExperimentalMapStructPath =
              [](const std::string &structPath) {
                if (!ir_lowerer::isExperimentalMapStructTypePath(structPath)) {
                  return false;
                }
                return structPath.find("__") != std::string::npos;
              };
          auto hasCanonicalMapHelperSemanticReceiverFact = [&](const Expr &receiverExpr) {
            return semanticProgram != nullptr &&
                   canonicalMapHelperSemanticIndexPtr != nullptr &&
                   receiverExpr.semanticNodeId != 0 &&
                   (ir_lowerer::findSemanticProductCollectionSpecialization(
                        *canonicalMapHelperSemanticIndexPtr, receiverExpr) != nullptr ||
                    ir_lowerer::findSemanticProductQueryFact(
                        semanticProgram, *canonicalMapHelperSemanticIndexPtr, receiverExpr) != nullptr ||
                    ir_lowerer::findSemanticProductBindingFact(
                        *canonicalMapHelperSemanticIndexPtr, receiverExpr) != nullptr ||
                    ir_lowerer::findSemanticProductLocalAutoFact(
                        semanticProgram, *canonicalMapHelperSemanticIndexPtr, receiverExpr) != nullptr);
          };
          const auto receiverMapTargetInfo =
              ir_lowerer::resolveMapAccessTargetInfo(
                  callExpr.args.front(),
                  localsIn,
                  inferCallMapTargetInfo,
                  semanticProgram,
                  canonicalMapHelperSemanticIndexPtr);
          if (receiverMapTargetInfo.isWrappedMapTarget) {
            return false;
          }

          auto inferExperimentalMapStructPath = [&](const Expr &receiverExpr) {
            const auto mapTargetInfo =
                ir_lowerer::resolveMapAccessTargetInfo(
                    receiverExpr,
                    localsIn,
                    inferCallMapTargetInfo,
                    semanticProgram,
                    canonicalMapHelperSemanticIndexPtr);
            if (isSpecializedExperimentalMapStructPath(mapTargetInfo.structTypeName)) {
              return mapTargetInfo.structTypeName;
            }
            if (hasCanonicalMapHelperSemanticReceiverFact(receiverExpr)) {
              return std::string{};
            }
            if (receiverExpr.kind == Expr::Kind::Name) {
              auto it = localsIn.find(receiverExpr.name);
              if (it != localsIn.end() &&
                  !it->second.isArgsPack &&
                  it->second.kind != LocalInfo::Kind::Reference &&
                  it->second.kind != LocalInfo::Kind::Pointer &&
                  isSpecializedExperimentalMapStructPath(it->second.structTypeName)) {
                return it->second.structTypeName;
              }
            }
            const auto accessTargetInfo =
                ir_lowerer::resolveArrayVectorAccessTargetInfo(
                    receiverExpr,
                    localsIn,
                    {},
                    semanticProgram,
                    canonicalMapHelperSemanticIndexPtr);
            if (accessTargetInfo.isWrappedMapTarget) {
              if (isSpecializedExperimentalMapStructPath(mapTargetInfo.structTypeName)) {
                return mapTargetInfo.structTypeName;
              }
              return std::string{};
            }
            if (isSpecializedExperimentalMapStructPath(accessTargetInfo.structTypeName)) {
              return accessTargetInfo.structTypeName;
            }
            return std::string{};
          };
          const std::string receiverStructPath =
              inferExperimentalMapStructPath(callExpr.args.front());
          if (receiverStructPath.empty()) {
            return false;
          }

          Expr methodExpr = callExpr;
          methodExpr.isMethodCall = true;
          methodExpr.name = helperName;
          methodExpr.namespacePrefix.clear();
          const Definition *callee = resolveMethodCallDefinition(methodExpr, localsIn);
          if (callee == nullptr) {
            return false;
          }
          const bool isExperimentalMapHelper =
              isSpecializedExperimentalMapStructPath(callee->fullPath);
          if (!isExperimentalMapHelper) {
            return false;
          }

          rewrittenExpr = callExpr;
          rewrittenExpr.name = callee->fullPath;
          rewrittenExpr.namespacePrefix.clear();
          rewrittenExpr.isMethodCall = false;
          rewrittenExpr.semanticNodeId = 0;
          rewrittenExpr.templateArgs.clear();
          return true;
        };
        auto isVectorStructPath = [](const std::string &structPath) {
          const std::string vectorTypePath =
              "/std/collections/experimental_" + std::string("vector") + "/Vector";
          return structPath == "/vector" ||
                 structPath == vectorTypePath ||
                 structPath.rfind(vectorTypePath + "__", 0) == 0;
        };
        auto publishedMapAccessHelperReturnsString =
            [&](std::string_view helperName) {
          if (semanticProgram == nullptr ||
              (helperName != "at" && helperName != "at_unsafe")) {
            return false;
          }
          const std::string helperPath =
              ir_lowerer::collectionMemberPath("map", helperName);
          const auto helperPathId =
              semanticProgramLookupCallTargetStringId(*semanticProgram, helperPath);
          if (!helperPathId.has_value()) {
            return false;
          }
          const SemanticProgramReturnFact *returnFact =
              semanticProgramLookupPublishedReturnFactByDefinitionPathId(
                  *semanticProgram, *helperPathId);
          if (returnFact == nullptr) {
            return false;
          }
          auto resolveReturnTypeText = [&](const std::string &text,
                                           SymbolId textId) {
            if (textId != InvalidSymbolId) {
              const std::string_view resolved =
                  semanticProgramResolveCallTargetString(*semanticProgram,
                                                         textId);
              if (!resolved.empty()) {
                return ir_lowerer::trimTemplateTypeText(std::string(resolved));
              }
            }
            return ir_lowerer::trimTemplateTypeText(text);
          };
          const std::string structPath =
              resolveReturnTypeText(returnFact->structPath,
                                    returnFact->structPathId);
          const std::string bindingType =
              resolveReturnTypeText(returnFact->bindingTypeText,
                                    returnFact->bindingTypeTextId);
          return structPath == "string" || structPath == "/string" ||
                 bindingType == "string" || bindingType == "/string";
        };
        auto hasExplicitStdMapHelperSpelling = [](const Expr &callExpr) {
          auto hasStdMapPrefix = [](std::string_view text) {
            return text.rfind(ir_lowerer::collectionMemberRoot("map"), 0) == 0 ||
                   text.rfind(ir_lowerer::collectionMemberRoot("map", false), 0) == 0;
          };
          return hasStdMapPrefix(callExpr.name) ||
                 hasStdMapPrefix(callExpr.namespacePrefix);
        };
        auto rewriteExplicitMapHelperBuiltinExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          std::string helperName;
          if (!resolveBuiltinMapHelperName(callExpr, false, helperName) ||
              (helperName != "count" && helperName != "contains" &&
               helperName != "tryAt" && helperName != "at" &&
               helperName != "at_unsafe")) {
            return false;
          }
          if (callExpr.sourceIsMethodCall &&
              !hasExplicitStdMapHelperSpelling(callExpr) &&
              publishedMapAccessHelperReturnsString(helperName)) {
            return false;
          }
          const SemanticProductIndex explicitMapHelperSemanticIndex =
              ir_lowerer::buildSemanticProductIndex(semanticProgram);
          const SemanticProductIndex *const explicitMapHelperSemanticIndexPtr =
              semanticProgram == nullptr ? nullptr : &explicitMapHelperSemanticIndex;
          const auto mapTargetInfo =
              ir_lowerer::resolveMapAccessTargetInfo(
                  callExpr.args.front(),
                  localsIn,
                  inferCallMapTargetInfo,
                  semanticProgram,
                  explicitMapHelperSemanticIndexPtr);
          const auto receiverTargetInfo =
              ir_lowerer::resolveArrayVectorAccessTargetInfo(
                  callExpr.args.front(),
                  localsIn,
                  {},
                  semanticProgram,
                  explicitMapHelperSemanticIndexPtr);
          const bool isCanonicalStdMapHelperPath =
              ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  resolveTailDispatchDirectHelperPath(callExpr),
                  primec::StdlibSurfaceId::CollectionsMapHelpers) ||
              (hasPublishedSemanticMapSurface(callExpr) &&
               ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                   resolveExprPath(callExpr),
                   primec::StdlibSurfaceId::CollectionsMapHelpers));
          const bool explicitCanonicalDirectMapAccess =
              !callExpr.isMethodCall &&
              hasExplicitStdMapHelperSpelling(callExpr) &&
              (helperName == "at" || helperName == "at_unsafe");
          const bool preserveCanonicalStdMapHelperPath =
              isCanonicalStdMapHelperPath &&
              (!mapTargetInfo.isMapTarget ||
               (mapTargetInfo.isWrappedMapTarget &&
                !explicitCanonicalDirectMapAccess));
          if (preserveCanonicalStdMapHelperPath) {
            // Keep canonical stdlib helper calls intact so direct overrides on
            // the published map helper surface are honored.
            return false;
          }
          const std::string rawPath = resolveTailDispatchDirectHelperPath(callExpr);
          if ((helperName == "count" || helperName == "contains" ||
               helperName == "tryAt" || helperName == "at" ||
               helperName == "at_unsafe") &&
              rawPath.rfind("/" + std::string("map") + "/", 0) == 0 &&
              normalizeCollectionHelperPath(rawPath) ==
                  normalizeCollectionHelperPath(resolveExprPath(callExpr))) {
            return false;
          }
          if ((helperName == "count" || helperName == "contains" ||
               helperName == "tryAt" || helperName == "at" ||
               helperName == "at_unsafe" || helperName == "insert" ||
               helperName == "insert_ref") &&
              resolveDefinitionCall(callExpr) != nullptr &&
              !isCanonicalStdMapHelperPath) {
            return false;
          }
          const std::string resolvedHelperPath = resolveExprPath(callExpr);
          if (ir_lowerer::isCanonicalPublishedStdlibSurfaceHelperPath(
                  resolvedHelperPath,
                  primec::StdlibSurfaceId::CollectionsMapHelpers) &&
              !isCanonicalStdMapHelperPath) {
            // Keep canonical stdlib helper paths intact so custom overrides on
            // the published map helper surface retain their resolved return types.
            return false;
          }
          std::string receiverAccessName;
          const bool receiverIsIndexedArgsPackElement =
              callExpr.args.front().kind == Expr::Kind::Call &&
              getBuiltinArrayAccessName(callExpr.args.front(), receiverAccessName) &&
              callExpr.args.front().args.size() == 2;
          if (helperName == "at" &&
              receiverTargetInfo.isArgsPackTarget &&
              receiverTargetInfo.argsPackElementKind == LocalInfo::Kind::Map) {
            rewrittenExpr = callExpr;
            rewrittenExpr.name = "at";
            rewrittenExpr.namespacePrefix.clear();
            return true;
          }
          if (receiverTargetInfo.isArgsPackTarget &&
              receiverTargetInfo.argsPackElementKind == LocalInfo::Kind::Map &&
              !receiverIsIndexedArgsPackElement) {
            return false;
          }
          if (!mapTargetInfo.isMapTarget) {
            return false;
          }
          rewrittenExpr = callExpr;
          rewrittenExpr.name = helperName;
          rewrittenExpr.namespacePrefix.clear();
          // Force later call resolution to use the rewritten builtin helper
          // shape instead of stale semantic-product direct-call targets.
          rewrittenExpr.semanticNodeId = 0;
          rewrittenExpr.templateArgs.clear();
          return true;
        };
        auto resolveSemanticReceiverTypeText =
            [&](const std::string &typeText, SymbolId typeTextId) {
          if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
            std::string resolvedTypeText = std::string(
                semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
            if (!resolvedTypeText.empty()) {
              return ir_lowerer::trimTemplateTypeText(resolvedTypeText);
            }
          }
          return ir_lowerer::trimTemplateTypeText(typeText);
        };
        Expr inlineDispatchExpr = expr;
        Expr rewrittenBuiltinMapInsertBuiltinExpr;
        if (rewriteBuiltinMapInsertBuiltinExpr(expr, rewrittenBuiltinMapInsertBuiltinExpr)) {
          inlineDispatchExpr = rewrittenBuiltinMapInsertBuiltinExpr;
        }
        Expr rewrittenCanonicalExperimentalMapHelperExpr;
        if (rewriteCanonicalMapHelperForExperimentalReceiverExpr(
                inlineDispatchExpr, rewrittenCanonicalExperimentalMapHelperExpr)) {
          inlineDispatchExpr = rewrittenCanonicalExperimentalMapHelperExpr;
        }
        Expr rewrittenInlineExplicitMapHelperExpr;
        if (rewriteExplicitMapHelperBuiltinExpr(
                inlineDispatchExpr, rewrittenInlineExplicitMapHelperExpr)) {
          inlineDispatchExpr = rewrittenInlineExplicitMapHelperExpr;
        }
        auto rewriteCanonicalMapHelperDefinitionExpr =
            [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }
          std::string helperName;
          if (callExpr.isMethodCall &&
              resolveBuiltinMapHelperName(callExpr, true, helperName)) {
            if (helperName == "contains_ref") {
              helperName = "contains";
            } else if (helperName == "tryAt_ref") {
              helperName = "tryAt";
            } else if (helperName == "count_ref") {
              helperName = "count";
            }
          } else if (!callExpr.isMethodCall &&
                     callExpr.namespacePrefix.empty() &&
                     callExpr.name.find('/') == std::string::npos &&
                     (callExpr.name == "contains" ||
                      callExpr.name == "tryAt" ||
                      callExpr.name == "count") &&
                     resolveTailDispatchDirectHelperDefinition(callExpr) ==
                         nullptr) {
            helperName = callExpr.name;
          }
          if (helperName.empty()) {
            return false;
          }
          if (!((helperName == "contains" && callExpr.args.size() == 2) ||
                (helperName == "tryAt" && callExpr.args.size() == 2) ||
                (helperName == "count" && callExpr.args.size() > 1))) {
            return false;
          }
          const SemanticProductIndex canonicalMapHelperSemanticIndex =
              ir_lowerer::buildSemanticProductIndex(semanticProgram);
          const SemanticProductIndex *const canonicalMapHelperSemanticIndexPtr =
              semanticProgram == nullptr ? nullptr : &canonicalMapHelperSemanticIndex;
          const auto mapTargetInfo =
              ir_lowerer::resolveMapAccessTargetInfo(
                  callExpr.args.front(),
                  localsIn,
                  inferCallMapTargetInfo,
                  semanticProgram,
                  canonicalMapHelperSemanticIndexPtr);
          if (!mapTargetInfo.isMapTarget) {
            return false;
          }
          Expr candidate = callExpr;
          candidate.isMethodCall = false;
          candidate.isFieldAccess = false;
          candidate.namespacePrefix.clear();
          candidate.name = ir_lowerer::collectionMemberPath("map", helperName);
          candidate.semanticNodeId = 0;
          candidate.templateArgs.clear();
          const Definition *callee =
              resolveTailDispatchDirectHelperDefinition(candidate);
          if (callee == nullptr) {
            return false;
          }
          rewrittenExpr = std::move(candidate);
          return true;
        };
        Expr rewrittenMethodCanonicalMapHelperDefinitionExpr;
        if (rewriteCanonicalMapHelperDefinitionExpr(
                inlineDispatchExpr,
                rewrittenMethodCanonicalMapHelperDefinitionExpr)) {
          inlineDispatchExpr = rewrittenMethodCanonicalMapHelperDefinitionExpr;
        }
        auto emitInternalSoaMetadataBeforeInline = [&]() -> std::optional<bool> {
          auto metadataLeaf = [](const Expr &callExpr) {
            std::string path = callExpr.name;
            if (path.find('/') == std::string::npos &&
                !callExpr.namespacePrefix.empty()) {
              path = callExpr.namespacePrefix == "/"
                         ? "/" + path
                         : callExpr.namespacePrefix + "/" + path;
            }
            const size_t leafStart = path.find_last_of('/');
            std::string leaf =
                leafStart == std::string::npos ? path : path.substr(leafStart + 1);
            const size_t generatedSuffix = leaf.find("__");
            if (generatedSuffix != std::string::npos) {
              leaf.erase(generatedSuffix);
            }
            if (leaf == "field_count" || leaf == "field_capacity") {
              return leaf;
            }
            return std::string{};
          }(inlineDispatchExpr);
          if (metadataLeaf.empty() || inlineDispatchExpr.args.size() != 1) {
            return std::nullopt;
          }
          auto normalizeInternalSoaType = [](std::string typeText) {
            typeText = ir_lowerer::trimTemplateTypeText(typeText);
            for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
              if (typeText.rfind(wrapper, 0) == 0 &&
                  typeText.size() > wrapper.size() &&
                  typeText.back() == '>') {
                typeText = ir_lowerer::trimTemplateTypeText(
                    typeText.substr(wrapper.size(),
                                    typeText.size() - wrapper.size() - 1));
                break;
              }
            }
            const size_t templateStart = typeText.find('<');
            if (templateStart != std::string::npos) {
              typeText.erase(templateStart);
            }
            const size_t leafStart = typeText.find_last_of('/');
            const size_t suffixStart =
                typeText.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
            if (suffixStart != std::string::npos) {
              typeText.erase(suffixStart);
            }
            return typeText;
          };
          auto isInternalSoaType = [&](const std::string &typeText) {
            const std::string normalized = normalizeInternalSoaType(typeText);
            return normalized == "/std/collections/internal_soa_storage/SoaColumn" ||
                   normalized == "/std/collections/internal_soa_storage/SoaFieldView";
          };
          const Expr &receiverExpr = inlineDispatchExpr.args.front();
          auto classifyInternalReceiverFromSemanticFacts = [&]() -> std::optional<bool> {
            if (semanticProgram == nullptr) {
              return std::nullopt;
            }
            const SemanticProductIndex semanticIndex =
                ir_lowerer::buildSemanticProductIndex(semanticProgram);
            if (const auto *queryFact = ir_lowerer::findSemanticProductQueryFact(
                    semanticProgram, semanticIndex, inlineDispatchExpr);
                queryFact != nullptr) {
              return isInternalSoaType(resolveSemanticReceiverTypeText(
                  queryFact->receiverBindingTypeText,
                  queryFact->receiverBindingTypeTextId));
            }
            return std::nullopt;
          };
          bool hasInternalReceiver = false;
          if (const std::optional<bool> semanticInternalReceiver =
                  classifyInternalReceiverFromSemanticFacts();
              semanticInternalReceiver.has_value()) {
            hasInternalReceiver = *semanticInternalReceiver;
          } else if (receiverExpr.kind == Expr::Kind::Name) {
            auto localIt = localsIn.find(receiverExpr.name);
            hasInternalReceiver =
                localIt != localsIn.end() &&
                isInternalSoaType(localIt->second.structTypeName);
          }
          if (!hasInternalReceiver) {
            return std::nullopt;
          }
          if (!emitExpr(receiverExpr, localsIn)) {
            return false;
          }
          const uint64_t slotOffset =
              metadataLeaf == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
          function.instructions.push_back({IrOpcode::PushI64, slotOffset});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        };
        if (const std::optional<bool> metadataResult =
                emitInternalSoaMetadataBeforeInline();
            metadataResult.has_value()) {
          return *metadataResult;
        }
        const auto inlineDispatchResult = ir_lowerer::tryEmitInlineCallDispatchWithLocals(
            inlineDispatchExpr,
            localsIn,
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isArrayCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isStringCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isVectorCapacityCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return resolveMethodCallDefinition(callExpr, localMap);
            },
            [&](const Expr &callExpr) {
              return resolveTailDispatchDirectHelperDefinition(callExpr);
            },
            [&](const Expr &callExpr, const Definition &callee, const ir_lowerer::LocalMap &localMap) {
              return emitInlineDefinitionCall(callExpr, callee, localMap, true);
            },
            error,
            semanticProgram,
            [&](const Expr &candidateExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(candidateExpr, localMap);
            });
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Emitted) {
          return true;
        }
        if (inlineDispatchResult == ir_lowerer::InlineCallDispatchResult::Error) {
          return false;
        }
        Expr nativeTailExpr = inlineDispatchExpr;
        auto rewriteImplicitBorrowedMapReceiverExpr = [&](const Expr &callExpr, Expr &rewrittenExpr) {
          if (callExpr.kind != Expr::Kind::Call || callExpr.args.empty()) {
            return false;
          }

          const SemanticProductIndex borrowedMapReceiverSemanticIndex =
              ir_lowerer::buildSemanticProductIndex(semanticProgram);
          const SemanticProductIndex *const borrowedMapReceiverSemanticIndexPtr =
              semanticProgram == nullptr ? nullptr : &borrowedMapReceiverSemanticIndex;
          auto resolveBorrowedMapReceiverInfo =
              [&](const Expr &receiverExpr) {
                return ir_lowerer::resolveMapAccessTargetInfo(
                    receiverExpr,
                    localsIn,
                    inferCallMapTargetInfo,
                    semanticProgram,
                    borrowedMapReceiverSemanticIndexPtr);
              };
          auto isBorrowedOrPointerMapReceiver = [&](const Expr &receiverExpr) {
            if (receiverExpr.kind == Expr::Kind::Call && isSimpleCallName(receiverExpr, "dereference") &&
                receiverExpr.args.size() == 1) {
              return false;
            }
            const auto mapTargetInfo = resolveBorrowedMapReceiverInfo(receiverExpr);
            return mapTargetInfo.isMapTarget && mapTargetInfo.isWrappedMapTarget;
          };

          auto shouldRewriteReceiver = [&](const Expr &candidate) {
            if (candidate.isMethodCall) {
              std::string helperName;
              return resolveBuiltinMapHelperName(candidate, true, helperName) &&
                     (helperName == "count" || helperName == "contains" ||
                      helperName == "tryAt" || helperName == "at" ||
                      helperName == "at_unsafe");
            }
            if (!candidate.args.empty() &&
                candidate.namespacePrefix.empty() &&
                candidate.name.find('/') == std::string::npos &&
                (candidate.name == "count" || candidate.name == "contains" ||
                 candidate.name == "tryAt" || candidate.name == "at" ||
                 candidate.name == "at_unsafe")) {
              return resolveBorrowedMapReceiverInfo(candidate.args.front()).isMapTarget;
            }
            std::string helperName;
            return resolveBuiltinMapHelperName(candidate, false, helperName) &&
                   (helperName == "count" || helperName == "contains" ||
                    helperName == "tryAt" || helperName == "at" ||
                    helperName == "at_unsafe" || helperName == "insert" ||
                    helperName == "insert_ref");
          };

          if (!shouldRewriteReceiver(callExpr) || !isBorrowedOrPointerMapReceiver(callExpr.args.front())) {
            return false;
          }

          Expr derefExpr;
          derefExpr.kind = Expr::Kind::Call;
          derefExpr.name = "dereference";
          derefExpr.args.push_back(callExpr.args.front());

          rewrittenExpr = callExpr;
          rewrittenExpr.args.front() = derefExpr;
          return true;
        };
        Expr rewrittenExplicitMapHelperExpr;
        if (rewriteExplicitMapHelperBuiltinExpr(nativeTailExpr, rewrittenExplicitMapHelperExpr)) {
          nativeTailExpr = rewrittenExplicitMapHelperExpr;
        }
        Expr rewrittenBorrowedMapReceiverExpr;
        if (rewriteImplicitBorrowedMapReceiverExpr(nativeTailExpr, rewrittenBorrowedMapReceiverExpr)) {
          nativeTailExpr = rewrittenBorrowedMapReceiverExpr;
        }
        const auto resolveCanonicalMathBuiltinName = [&](const Expr &callExpr, std::string &mathBuiltinName) {
          if (getMathBuiltinName(callExpr, mathBuiltinName)) {
            return true;
          }

          Expr resolvedCallExpr = callExpr;
          resolvedCallExpr.name = resolveExprPath(callExpr);
          resolvedCallExpr.namespacePrefix.clear();
          return getMathBuiltinName(resolvedCallExpr, mathBuiltinName);
        };
        auto resolveSemanticQueryFactTypeText =
            [&](const SemanticProgramQueryFact &queryFact) {
              auto resolveFactTypeText = [&](const std::string &typeText,
                                             SymbolId typeTextId) {
                if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
                  std::string resolvedTypeText = std::string(
                      semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
                  if (!resolvedTypeText.empty()) {
                    return ir_lowerer::trimTemplateTypeText(resolvedTypeText);
                  }
                }
                return ir_lowerer::trimTemplateTypeText(typeText);
              };
              std::string bindingType =
                  resolveFactTypeText(queryFact.bindingTypeText, queryFact.bindingTypeTextId);
              if (bindingType.empty()) {
                bindingType =
                    resolveFactTypeText(queryFact.queryTypeText, queryFact.queryTypeTextId);
              }
              return bindingType;
            };
        auto normalizeInternalSoaMetadataType = [](std::string typeText) {
          typeText = ir_lowerer::trimTemplateTypeText(typeText);
          for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
            if (typeText.rfind(wrapper, 0) == 0 &&
                typeText.size() > wrapper.size() &&
                typeText.back() == '>') {
              typeText = ir_lowerer::trimTemplateTypeText(
                  typeText.substr(wrapper.size(),
                                  typeText.size() - wrapper.size() - 1));
              break;
            }
          }
          const size_t templateStart = typeText.find('<');
          if (templateStart != std::string::npos) {
            typeText.erase(templateStart);
          }
          const size_t leafStart = typeText.find_last_of('/');
          const size_t suffixStart =
              typeText.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
          if (suffixStart != std::string::npos) {
            typeText.erase(suffixStart);
          }
          return typeText;
        };
        auto internalSoaMetadataCallLeaf = [](const Expr &callExpr) {
          std::string path = callExpr.name;
          if (path.find('/') == std::string::npos && !callExpr.namespacePrefix.empty()) {
            path = callExpr.namespacePrefix == "/" ? "/" + path
                                                   : callExpr.namespacePrefix + "/" + path;
          }
          const size_t leafStart = path.find_last_of('/');
          std::string leaf =
              leafStart == std::string::npos ? path : path.substr(leafStart + 1);
          const size_t generatedSuffix = leaf.find("__");
          if (generatedSuffix != std::string::npos) {
            leaf.erase(generatedSuffix);
          }
          if (leaf == "field_count" || leaf == "field_capacity") {
            return leaf;
          }
          return std::string{};
        };
        auto isInternalSoaMetadataReceiver = [&](const Expr &receiverExpr,
                                                 const Expr &callExpr) {
          auto isInternalSoaType = [&](const std::string &typeText) {
            const std::string normalized =
                normalizeInternalSoaMetadataType(typeText);
            return normalized == "/std/collections/internal_soa_storage/SoaColumn" ||
                   normalized == "/std/collections/internal_soa_storage/SoaFieldView";
          };
          if (semanticProgram != nullptr) {
            const SemanticProductIndex semanticIndex =
                ir_lowerer::buildSemanticProductIndex(semanticProgram);
            const auto *queryFact =
                ir_lowerer::findSemanticProductQueryFact(semanticProgram, semanticIndex, callExpr);
            if (queryFact != nullptr) {
              const std::string receiverType = resolveSemanticReceiverTypeText(
                  queryFact->receiverBindingTypeText,
                  queryFact->receiverBindingTypeTextId);
              return isInternalSoaType(receiverType);
            }
          }
          if (receiverExpr.kind != Expr::Kind::Name) {
            return false;
          }
          auto localIt = localsIn.find(receiverExpr.name);
          return localIt != localsIn.end() &&
                 isInternalSoaType(localIt->second.structTypeName);
        };
        auto emitInternalSoaMetadataCall = [&]() -> std::optional<bool> {
          const std::string metadataLeaf =
              internalSoaMetadataCallLeaf(nativeTailExpr);
          if (metadataLeaf.empty() || nativeTailExpr.args.size() != 1 ||
              !isInternalSoaMetadataReceiver(nativeTailExpr.args.front(),
                                             nativeTailExpr)) {
            return std::nullopt;
          }
          if (!emitExpr(nativeTailExpr.args.front(), localsIn)) {
            return false;
          }
          const uint64_t slotOffset =
              metadataLeaf == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
          function.instructions.push_back({IrOpcode::PushI64, slotOffset});
          function.instructions.push_back({IrOpcode::AddI64, 0});
          function.instructions.push_back({IrOpcode::LoadIndirect, 0});
          return true;
        };
        if (const std::optional<bool> metadataResult =
                emitInternalSoaMetadataCall();
            metadataResult.has_value()) {
          return *metadataResult;
        }

        const auto nativeTailResult = ir_lowerer::tryEmitNativeCallTailDispatchWithLocals(
            nativeTailExpr,
            localsIn,
            [&](const Expr &callExpr, std::string &mathBuiltinName) {
              return resolveCanonicalMathBuiltinName(callExpr, mathBuiltinName);
            },
            [&](const std::string &mathBuiltinName) {
              return ir_lowerer::isSupportedMathBuiltinName(mathBuiltinName);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isArrayCountCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isVectorCapacityCall(callExpr, localMap);
            },
            [&](const Expr &callExpr, const ir_lowerer::LocalMap &localMap) {
              return isStringCountCall(callExpr, localMap);
            },
            [&](const Expr &targetExpr, const ir_lowerer::LocalMap &localMap) {
              return isEntryArgsName(targetExpr, localMap);
            },
            [&](const Expr &targetExpr,
                const ir_lowerer::LocalMap &localMap,
                int32_t &stringIndexOut,
                size_t &lengthOut) {
              return resolveStringTableTarget(targetExpr, localMap, stringIndexOut, lengthOut);
            },
            stringTable.size(),
            [&](const Expr &valueExpr, const ir_lowerer::LocalMap &localMap) {
              return emitExpr(valueExpr, localMap);
            },
            [&](const Expr &targetCallExpr, ir_lowerer::MapAccessTargetInfo &targetInfoOut) {
              targetInfoOut = ir_lowerer::resolveMapAccessTargetInfo(
                  targetCallExpr,
                  localsIn,
                  inferCallMapTargetInfo,
                  semanticProgram,
                  tailDispatchMapSemanticIndexPtr);
              if (targetInfoOut.isMapTarget) {
                populateTailDispatchMapStructPathFromKinds(targetInfoOut);
              }
              return targetInfoOut.isMapTarget;
            },
            [&](const Expr &targetCallExpr, ir_lowerer::ArrayVectorAccessTargetInfo &targetInfoOut) {
              targetInfoOut = {};
              auto resolveSpecializedVectorElementKind = [&](const std::string &typeText,
                                                            ir_lowerer::LocalInfo::ValueKind &elemKindOut) {
                elemKindOut = ir_lowerer::LocalInfo::ValueKind::Unknown;
                std::string normalized = ir_lowerer::trimTemplateTypeText(typeText);
                if (!normalized.empty() && normalized.front() != '/') {
                  normalized.insert(normalized.begin(), '/');
                }
                if (!matchesGeneratedSpecializedType(normalized, "vector", "Vector")) {
                  return false;
                }
                Expr syntheticExpr;
                syntheticExpr.kind = Expr::Kind::Call;
                syntheticExpr.name = normalized;
                const Definition *structDef = resolveDefinitionCall(syntheticExpr);
                if (structDef == nullptr || !ir_lowerer::isStructDefinition(*structDef)) {
                  return false;
                }
                for (const auto &fieldExpr : structDef->statements) {
                  if (!fieldExpr.isBinding || fieldExpr.name != "data") {
                    continue;
                  }
                  std::string typeName;
                  std::vector<std::string> templateArgs;
                  if (!ir_lowerer::extractFirstBindingTypeTransform(fieldExpr, typeName, templateArgs) ||
                      ir_lowerer::normalizeCollectionBindingTypeName(typeName) != "Pointer" ||
                      templateArgs.size() != 1) {
                    continue;
                  }
                  std::string elementType = ir_lowerer::trimTemplateTypeText(templateArgs.front());
                  if (!ir_lowerer::extractTopLevelUninitializedTypeText(elementType, elementType)) {
                    continue;
                  }
                  elemKindOut = ir_lowerer::valueKindFromTypeName(elementType);
                  return elemKindOut != ir_lowerer::LocalInfo::ValueKind::Unknown;
                }
                return false;
              };
              auto populateNativeTailArrayVectorTargetFromElementType =
                  [&](const std::string &collectionName,
                      const std::string &elementTypeText,
                      ir_lowerer::ArrayVectorAccessTargetInfo &targetInfo) {
                    const std::string normalizedElementType =
                        ir_lowerer::trimTemplateTypeText(elementTypeText);
                    if (normalizedElementType.empty()) {
                      return false;
                    }
                    ir_lowerer::ArrayVectorAccessTargetInfo candidate;
                    candidate.isArrayOrVectorTarget = true;
                    candidate.isVectorTarget = (collectionName == "vector");
                    candidate.isSoaVector = (collectionName == "soa_vector");
                    candidate.elemKind =
                        ir_lowerer::valueKindFromTypeName(normalizedElementType);
                    if (candidate.elemKind ==
                            ir_lowerer::LocalInfo::ValueKind::Unknown &&
                        !candidate.isSoaVector) {
                      return false;
                    }
                    if (candidate.isVectorTarget) {
                      candidate.structTypeName =
                          specializedExperimentalVectorStructPathForElementType(
                              normalizedElementType);
                    } else if (candidate.isSoaVector) {
                      candidate.structTypeName =
                          specializedExperimentalSoaVectorStructPathForElementType(
                              normalizedElementType);
                    }
                    targetInfo = candidate;
                    return true;
                  };
              std::function<bool(const std::string &,
                                 ir_lowerer::ArrayVectorAccessTargetInfo &)>
                  inferNativeTailArrayVectorTargetFromTypeText;
              inferNativeTailArrayVectorTargetFromTypeText =
                  [&](const std::string &typeText,
                      ir_lowerer::ArrayVectorAccessTargetInfo &targetInfo) {
                    const std::string normalized =
                        ir_lowerer::trimTemplateTypeText(typeText);
                    if (normalized.empty()) {
                      return false;
                    }
                    std::string normalizedPath = normalized;
                    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
                      normalizedPath.insert(normalizedPath.begin(), '/');
                    }
                    if (matchesGeneratedSpecializedType(
                            normalizedPath, "vector", "Vector")) {
                      ir_lowerer::LocalInfo::ValueKind elemKind;
                      if (!resolveSpecializedVectorElementKind(normalizedPath,
                                                              elemKind)) {
                        return false;
                      }
                      ir_lowerer::ArrayVectorAccessTargetInfo candidate;
                      candidate.isArrayOrVectorTarget = true;
                      candidate.isVectorTarget = true;
                      candidate.elemKind = elemKind;
                      candidate.structTypeName = normalizedPath;
                      targetInfo = candidate;
                      return true;
                    }
                    if (normalizedPath.rfind(
                            "/std/collections/experimental_soa_vector/SoaVector__",
                            0) == 0) {
                      ir_lowerer::ArrayVectorAccessTargetInfo candidate;
                      candidate.isArrayOrVectorTarget = true;
                      candidate.isVectorTarget = false;
                      candidate.isSoaVector = true;
                      candidate.structTypeName = normalizedPath;
                      targetInfo = candidate;
                      return true;
                    }
                    std::string base;
                    std::string argText;
                    if (!splitTemplateTypeName(normalized, base, argText)) {
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
                      return inferNativeTailArrayVectorTargetFromTypeText(
                          wrappedArgs.front(), targetInfo);
                    }
                    const std::string collectionName =
                        normalizedBase == "/array"
                            ? "array"
                            : ir_lowerer::normalizeCollectionBindingTypeName(
                                  normalizedBase);
                    if (collectionName != "array" &&
                        collectionName != "vector" &&
                        collectionName != "soa_vector") {
                      return false;
                    }
                    std::vector<std::string> args;
                    if (!splitTemplateArgs(argText, args) || args.size() != 1) {
                      return false;
                    }
                    return populateNativeTailArrayVectorTargetFromElementType(
                        collectionName, args.front(), targetInfo);
                  };
              auto tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText =
                  [&](const std::string &typeText,
                      SymbolId typeTextId,
                      ir_lowerer::ArrayVectorAccessTargetInfo &targetInfo) {
                    return inferNativeTailArrayVectorTargetFromTypeText(
                        resolveSemanticReceiverTypeText(typeText, typeTextId),
                        targetInfo);
                  };
              auto tryPopulateNativeTailArrayVectorTargetFromSemanticCollection =
                  [&](const SemanticProductIndex &semanticIndex) {
                    const auto *collectionFact =
                        ir_lowerer::findSemanticProductCollectionSpecialization(
                            semanticIndex, targetCallExpr);
                    if (collectionFact == nullptr) {
                      return false;
                    }
                    const std::string collectionFamily =
                        resolveSemanticReceiverTypeText(
                            collectionFact->collectionFamily,
                            collectionFact->collectionFamilyId);
                    const std::string collectionName =
                        collectionFamily == "/array"
                            ? "array"
                            : ir_lowerer::normalizeCollectionBindingTypeName(
                                  collectionFamily);
                    if (collectionName != "array" &&
                        collectionName != "vector" &&
                        collectionName != "soa_vector") {
                      return false;
                    }
                    std::string elementTypeText =
                        resolveSemanticReceiverTypeText(
                            collectionFact->elementTypeText,
                            collectionFact->elementTypeTextId);
                    if (elementTypeText.empty()) {
                      elementTypeText =
                          resolveSemanticReceiverTypeText(
                              collectionFact->valueTypeText,
                              collectionFact->valueTypeTextId);
                    }
                    return populateNativeTailArrayVectorTargetFromElementType(
                        collectionName, elementTypeText, targetInfoOut);
                  };
              auto tryPopulateArrayVectorFromSemanticReceiverFacts = [&]() {
                if (semanticProgram == nullptr) {
                  return false;
                }
                const SemanticProductIndex semanticIndex =
                    ir_lowerer::buildSemanticProductIndex(semanticProgram);
                if (tryPopulateNativeTailArrayVectorTargetFromSemanticCollection(
                        semanticIndex)) {
                  return true;
                }
                const auto *queryFact =
                    ir_lowerer::findSemanticProductQueryFact(semanticProgram, semanticIndex, targetCallExpr);
                if (queryFact != nullptr) {
                  if (tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          queryFact->bindingTypeText,
                          queryFact->bindingTypeTextId,
                          targetInfoOut) ||
                      tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          queryFact->queryTypeText,
                          queryFact->queryTypeTextId,
                          targetInfoOut) ||
                      tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          queryFact->receiverBindingTypeText,
                          queryFact->receiverBindingTypeTextId,
                          targetInfoOut)) {
                    return true;
                  }
                  const std::string bindingType = resolveSemanticQueryFactTypeText(*queryFact);
                  if (tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                          bindingType,
                          InvalidSymbolId,
                          targetInfoOut)) {
                    return true;
                  }
                }
                if (const auto *bindingFact =
                        ir_lowerer::findSemanticProductBindingFact(
                            semanticIndex, targetCallExpr);
                    bindingFact != nullptr &&
                    tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                        bindingFact->bindingTypeText,
                        bindingFact->bindingTypeTextId,
                        targetInfoOut)) {
                  return true;
                }
                if (const auto *localAutoFact =
                        ir_lowerer::findSemanticProductLocalAutoFact(
                            semanticProgram, semanticIndex, targetCallExpr);
                    localAutoFact != nullptr &&
                    tryPopulateNativeTailArrayVectorTargetFromSemanticTypeText(
                        localAutoFact->bindingTypeText,
                        localAutoFact->bindingTypeTextId,
                        targetInfoOut)) {
                  return true;
                }
                return false;
              };
              if (tryPopulateArrayVectorFromSemanticReceiverFacts()) {
                return true;
              }
              if (targetCallExpr.isFieldAccess && targetCallExpr.args.size() == 1) {
                const std::string receiverStruct = inferStructExprPath(targetCallExpr.args.front(), localsIn);
                if (!receiverStruct.empty()) {
                  StructSlotFieldInfo fieldInfo;
                  if (resolveStructFieldSlot(receiverStruct, targetCallExpr.name, fieldInfo) &&
                      isVectorStructPath(fieldInfo.structPath)) {
                    targetInfoOut.isArrayOrVectorTarget = true;
                    targetInfoOut.isVectorTarget = true;
                    targetInfoOut.elemKind = fieldInfo.valueKind;
                    targetInfoOut.structTypeName = fieldInfo.structPath;
                    return true;
                  }
                }
              }
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
                  resolveTailDispatchDirectHelperDefinition(targetCallExpr);
              if (callee == nullptr) {
                return false;
              }
              std::string collectionName;
              std::vector<std::string> collectionArgs;
              if (!ir_lowerer::inferDeclaredReturnCollection(*callee, collectionName, collectionArgs)) {
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
              targetInfoOut.elemKind = ir_lowerer::valueKindFromTypeName(collectionArgs.front());
              if (targetInfoOut.isSoaVector) {
                std::string elementTypeName = trimTemplateTypeText(collectionArgs.front());
                if (!elementTypeName.empty() && elementTypeName.front() == '/') {
                  elementTypeName.erase(elementTypeName.begin());
                }
                targetInfoOut.structTypeName =
                    specializedExperimentalSoaVectorStructPathForElementType(
                        elementTypeName);
              }
              return true;
            },
            [&](const Expr &callExpr, std::string &builtinName) {
              PrintBuiltin printBuiltin;
              if (!getPrintBuiltin(callExpr, printBuiltin)) {
                return false;
              }
              builtinName = printBuiltin.name;
              return true;
            },
            [&](const Expr &lookupKeyExpr, const ir_lowerer::LocalMap &localMap) {
              return inferExprKind(lookupKeyExpr, localMap);
            },
            [&]() { return allocTempLocal(); },
            [&]() { emitStringIndexOutOfBounds(); },
            [&]() { emitMapKeyNotFound(); },
            [&]() { emitArrayIndexOutOfBounds(); },
            [&]() { return function.instructions.size(); },
            [&](IrOpcode op, uint64_t imm) { function.instructions.push_back({op, imm}); },
            [&](size_t instructionIndex, uint64_t imm) { function.instructions[instructionIndex].imm = imm; },
            error,
            semanticProgram);
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Emitted) {
          return true;
        }
        if (nativeTailResult == ir_lowerer::NativeCallTailDispatchResult::Error) {
          return false;
        }
