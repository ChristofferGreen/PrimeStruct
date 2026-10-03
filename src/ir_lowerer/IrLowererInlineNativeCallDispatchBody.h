                                                const std::string &typeText) {
    const std::string normalizedTypeText =
        trimTemplateTypeText(resolveInlineSemanticTypeText(typeTextId, typeText));
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
          normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
        const std::string normalizedArg = trimTemplateTypeText(argText);
        return normalizedArg == "soa" ||
               collection_helpers::isCollectionFamilyRoot(normalizedArg, collection_helpers::CollectionFamily::Soa) ||
               normalizedArg == "std/collections/soa" ||
               normalizedArg == collection_helpers::kCanonicalSoa;
      }
      return normalizedBase == "soa" ||
             collection_helpers::isCollectionFamilyRoot(normalizedBase, collection_helpers::CollectionFamily::Soa) ||
             normalizedBase == "std/collections/soa" ||
             normalizedBase == collection_helpers::kCanonicalSoa;
    }
    return normalizedTypeText == "soa" ||
           collection_helpers::isCollectionFamilyRoot(normalizedTypeText, collection_helpers::CollectionFamily::Soa) ||
           normalizedTypeText == "std/collections/soa" ||
           normalizedTypeText == collection_helpers::kCanonicalSoa;
  };
  std::function<bool(const Expr &)> isRawBuiltinSoaVectorTarget;
  isRawBuiltinSoaVectorTarget = [&](const Expr &targetExpr) {
    if (semanticProgram != nullptr && semanticIndexPtr != nullptr &&
        targetExpr.semanticNodeId != 0) {
      if (const auto *collectionFact =
              findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr);
          collectionFact != nullptr &&
          isInlineRawBuiltinSoaVectorTypeText(collectionFact->collectionFamilyId,
                                             collectionFact->collectionFamily)) {
        return true;
      }
      if (const auto *queryFact =
              findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr);
          queryFact != nullptr) {
        return isInlineRawBuiltinSoaVectorTypeText(queryFact->bindingTypeTextId,
                                                   queryFact->bindingTypeText) ||
               isInlineRawBuiltinSoaVectorTypeText(queryFact->queryTypeTextId,
                                                   queryFact->queryTypeText) ||
               isInlineRawBuiltinSoaVectorTypeText(
                   queryFact->receiverBindingTypeTextId,
                   queryFact->receiverBindingTypeText);
      }
      if (const auto *bindingFact =
              findSemanticProductBindingFact(*semanticIndexPtr, targetExpr);
          bindingFact != nullptr &&
          isInlineRawBuiltinSoaVectorTypeText(bindingFact->bindingTypeTextId,
                                             bindingFact->bindingTypeText)) {
        return true;
      }
      if (const auto *localAutoFact =
              findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr);
          localAutoFact != nullptr &&
          isInlineRawBuiltinSoaVectorTypeText(localAutoFact->bindingTypeTextId,
                                             localAutoFact->bindingTypeText)) {
        return true;
      }
    }
    if (targetExpr.kind == Expr::Kind::Name) {
      const auto localIt = localsIn.find(targetExpr.name);
      return localIt != localsIn.end() &&
             localIt->second.isSoaVector &&
             localIt->second.usesBuiltinCollectionLayout;
    }
    if (targetExpr.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(targetExpr, collection) && collection == "soa") {
        return true;
      }
      if ((isSimpleCallName(targetExpr, "location") ||
           isSimpleCallName(targetExpr, "dereference")) &&
          targetExpr.args.size() == 1) {
        return isRawBuiltinSoaVectorTarget(targetExpr.args.front());
      }
    }
    return false;
  };
  auto emitCanonicalInlineDefinitionCall = [&](const Expr &callExpr, const Definition &callee) {
    bool emitted = false;
    if (isTypeNamespaceMethodCallForInlineEmit(callExpr, callee, localsIn)) {
      const Expr directCallExpr = makeInlineEmitDirectTypeNamespaceCall(callExpr, callee);
      emitted = emitInlineDefinitionCallFn(directCallExpr, callee, localsIn);
    } else {
      emitted = emitInlineDefinitionCallFn(callExpr, callee, localsIn);
    }
    if (!emitted && error.empty()) {
      error = "canonical inline definition call failed without diagnostic: " +
              callee.fullPath;
    }
    return emitted;
  };
  const bool isSemanticStringCountMethod =
      expr.isMethodCall && expr.args.size() == 1 &&
      semanticProgram != nullptr &&
      findSemanticProductMethodCallTarget(semanticProgram, expr) ==
          collection_helpers::kRootedStringCount;
  if (expr.isMethodCall && expr.args.size() == 1 &&
      (isStringCountCallFn(expr, localsIn) || isSemanticStringCountMethod)) {
    const Definition *stringCountCallee =
        resolveMethodCallDefinitionFn(expr, localsIn);
    if (stringCountCallee == nullptr) {
      Expr directStringCountExpr = expr;
      directStringCountExpr.isMethodCall = false;
      directStringCountExpr.isFieldAccess = false;
      directStringCountExpr.namespacePrefix.clear();
      directStringCountExpr.name = collection_helpers::kRootedStringCount;
      directStringCountExpr.semanticNodeId = 0;
      stringCountCallee = resolveDefinitionCallFn(directStringCountExpr);
    }
    if (stringCountCallee != nullptr &&
        stringCountCallee->fullPath == collection_helpers::kRootedStringCount) {
      Expr directStringCountExpr = expr;
      directStringCountExpr.isMethodCall = false;
      directStringCountExpr.isFieldAccess = false;
      directStringCountExpr.namespacePrefix.clear();
      directStringCountExpr.name = collection_helpers::kRootedStringCount;
      directStringCountExpr.semanticNodeId = 0;
      return emitCanonicalInlineDefinitionCall(directStringCountExpr, *stringCountCallee)
                 ? InlineCallDispatchResult::Emitted
                 : InlineCallDispatchResult::Error;
    }
  }
  if (isArrayCountCallFn(expr, localsIn) ||
      isStringCountCallFn(expr, localsIn) ||
      isVectorCapacityCallFn(expr, localsIn)) {
    return InlineCallDispatchResult::NotHandled;
  }
  const auto inferCallKeyValueTargetInfo = [&](const Expr &targetExpr,
                                          CollectionPairTypeInfo &targetInfoOut) {
    targetInfoOut = {};
    const Definition *callee = resolveDefinitionCallFn(targetExpr);
    if (callee == nullptr) {
      return false;
    }
    std::string collectionName;
    std::vector<std::string> collectionArgs;
    if (!inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) ||
        collectionName != "map" ||
        collectionArgs.size() != 2) {
      return inferForwardedCollectionPairTypeInfo(
          targetExpr, *callee, localsIn, {}, targetInfoOut);
    }
    targetInfoOut.isKeyValueTarget = true;
    targetInfoOut.keyValueKeyKind = valueKindFromTypeName(collectionArgs.front());
    targetInfoOut.keyValueValueKind = valueKindFromTypeName(collectionArgs.back());
    return targetInfoOut.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
           targetInfoOut.keyValueValueKind != LocalInfo::ValueKind::Unknown;
  };
  std::function<bool(const std::string &)> isInlineCollectionAccessTypeText;
  isInlineCollectionAccessTypeText = [&](const std::string &typeText) {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
          normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
        return isInlineCollectionAccessTypeText(argText);
      }
      return normalizedBase == "array" || collection_helpers::isCollectionFamilyRoot(normalizedBase, collection_helpers::CollectionFamily::Array) ||
             normalizedBase == "vector" || collection_helpers::isCollectionFamilyRoot(normalizedBase, collection_helpers::CollectionFamily::Vector) ||
             normalizedBase == "Array" || normalizedBase == "/Array" ||
             matchesCollectionTypeText(normalizedBase, "vector") ||
             isInlineExperimentalVectorTypeName(normalizedBase);
    }
    const std::string normalizedTypeText = trimTemplateTypeText(typeText);
    return normalizedTypeText == "string" || collection_helpers::isCollectionFamilyRoot(normalizedTypeText, collection_helpers::CollectionFamily::String) ||
           normalizedTypeText == "String" || normalizedTypeText == "/String" ||
           normalizedTypeText == "array" || collection_helpers::isCollectionFamilyRoot(normalizedTypeText, collection_helpers::CollectionFamily::Array) ||
           normalizedTypeText == "vector" || collection_helpers::isCollectionFamilyRoot(normalizedTypeText, collection_helpers::CollectionFamily::Vector) ||
           normalizedTypeText == "Array" || normalizedTypeText == "/Array" ||
           matchesCollectionTypeText(normalizedTypeText, "vector") ||
           isInlineExperimentalVectorTypeName(normalizedTypeText);
  };
  auto classifyInlineCollectionAccessTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) {
    return isInlineCollectionAccessTypeText(
               resolveInlineSemanticTypeText(typeTextId, typeText))
               ? InlineCollectionAccessTargetFact::CollectionAccess
               : InlineCollectionAccessTargetFact::NonCollectionAccess;
  };
  auto combineInlineCollectionAccessTargetFacts =
      [](InlineCollectionAccessTargetFact lhs,
         InlineCollectionAccessTargetFact rhs) {
    if (lhs == InlineCollectionAccessTargetFact::CollectionAccess ||
        rhs == InlineCollectionAccessTargetFact::CollectionAccess) {
      return InlineCollectionAccessTargetFact::CollectionAccess;
    }
    if (lhs == InlineCollectionAccessTargetFact::NonCollectionAccess ||
        rhs == InlineCollectionAccessTargetFact::NonCollectionAccess) {
      return InlineCollectionAccessTargetFact::NonCollectionAccess;
    }
    return InlineCollectionAccessTargetFact::Unknown;
  };
  auto classifyInlineCollectionAccessTargetFromSemanticFacts =
      [&](const Expr &targetExpr) {
    if (semanticProgram == nullptr || semanticIndexPtr == nullptr ||
        targetExpr.semanticNodeId == 0) {
      return InlineCollectionAccessTargetFact::Unknown;
    }
    if (const auto *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr);
        collectionFact != nullptr) {
      const std::string collectionFamily =
          resolveInlineSemanticTypeText(collectionFact->collectionFamilyId,
                                        collectionFact->collectionFamily);
      return collectionFamily == "array" || collection_helpers::isCollectionFamilyRoot(collectionFamily, collection_helpers::CollectionFamily::Array) ||
             collectionFamily == "vector" || collection_helpers::isCollectionFamilyRoot(collectionFamily, collection_helpers::CollectionFamily::Vector) ||
             collectionFamily == "string" || collection_helpers::isCollectionFamilyRoot(collectionFamily, collection_helpers::CollectionFamily::String) ||
             matchesCollectionTypeText(collectionFamily, "vector") ||
             isInlineExperimentalVectorTypeName(collectionFamily)
                 ? InlineCollectionAccessTargetFact::CollectionAccess
                 : InlineCollectionAccessTargetFact::NonCollectionAccess;
    }
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr);
        queryFact != nullptr) {
      InlineCollectionAccessTargetFact fact =
          classifyInlineCollectionAccessTypeText(queryFact->bindingTypeTextId,
                                                 queryFact->bindingTypeText);
      fact = combineInlineCollectionAccessTargetFacts(
          fact,
          classifyInlineCollectionAccessTypeText(queryFact->queryTypeTextId,
                                                 queryFact->queryTypeText));
      return combineInlineCollectionAccessTargetFacts(
          fact,
          classifyInlineCollectionAccessTypeText(
              queryFact->receiverBindingTypeTextId,
              queryFact->receiverBindingTypeText));
    }
    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndexPtr, targetExpr);
        bindingFact != nullptr) {
      return classifyInlineCollectionAccessTypeText(
          bindingFact->bindingTypeTextId,
          bindingFact->bindingTypeText);
    }
    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr);
        localAutoFact != nullptr) {
      return classifyInlineCollectionAccessTypeText(
          localAutoFact->bindingTypeTextId,
          localAutoFact->bindingTypeText);
    }
    return InlineCollectionAccessTargetFact::Unknown;
  };
  if (!expr.isMethodCall) {
    std::string keyValueHelperName;
    const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
    if (const Definition *directCallee = resolveDefinitionCallFn(expr);
        directCallee != nullptr &&
        isSemanticBarePreferredKeyValueHelperDefinitionCall(expr,
                                                            *directCallee)) {
      return emitCanonicalInlineDefinitionCall(expr, *directCallee)
                 ? InlineCallDispatchResult::Emitted
                 : InlineCallDispatchResult::Error;
    }
    if (expr.args.size() == 1 &&
        isSemanticOrLegacyVectorTarget(expr.args.front())) {
      std::string vectorHelperName;
      if (resolveVectorHelperAliasName(expr, vectorHelperName) &&
          (vectorHelperName == "count" || vectorHelperName == "capacity")) {
        return InlineCallDispatchResult::NotHandled;
      }
      const size_t rawLeafStart = rawPath.find_last_of('/');
      std::string rawLeaf = rawLeafStart == std::string::npos
                                ? rawPath
                                : rawPath.substr(rawLeafStart + 1);
      rawLeaf = stripGeneratedInlineHelperSuffix(std::move(rawLeaf));
      if (rawLeaf == collectionWrapperAlias("vector", "Count") ||
          rawLeaf == collectionWrapperAlias("vector", "Capacity")) {
        return InlineCallDispatchResult::NotHandled;
      }
      if (const Definition *callee = resolveDefinitionCallFn(expr);
          callee != nullptr) {
        const size_t leafStart = callee->fullPath.find_last_of('/');
        std::string leaf = leafStart == std::string::npos
                               ? callee->fullPath
                               : callee->fullPath.substr(leafStart + 1);
        leaf = stripGeneratedInlineHelperSuffix(std::move(leaf));
        if (leaf == collectionWrapperAlias("vector", "Count") ||
            leaf == collectionWrapperAlias("vector", "Capacity")) {
          return InlineCallDispatchResult::NotHandled;
        }
      }
    }
    if (expr.args.size() == 2 &&
        isSemanticOrLegacyVectorTarget(expr.args.front())) {
      // A same-path user definition overriding the canonical
      // /std/collections/vector/at(_unsafe) direct-call helper must win
      // over the native builtin access path below. The compiler's own
      // generic stdlib at<T> always resolves (once specialized for a
      // concrete element type) to a generated path carrying a "__t"/"__ov"
      // suffix (see stripGeneratedInlineHelperSuffix); a real user
      // override keeps its own literal, unsuffixed declared path. Resolve
      // the callee once up front and use that identity - not just the
      // call's textual "at"/"at_unsafe" shape - to decide whether to
      // defer to the native path.
      const Definition *atCallee = resolveDefinitionCallFn(expr);
      const bool isConcreteAtOverride =
          atCallee != nullptr &&
          stripGeneratedInlineHelperSuffix(std::string(atCallee->fullPath)) ==
              atCallee->fullPath;
      if (isConcreteAtOverride) {
        return emitCanonicalInlineDefinitionCall(expr, *atCallee)
                   ? InlineCallDispatchResult::Emitted
                   : InlineCallDispatchResult::Error;
      }
      std::string vectorHelperName;
      if (resolveVectorHelperAliasName(expr, vectorHelperName) &&
          (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) {
        return InlineCallDispatchResult::NotHandled;
      }
      const size_t rawLeafStart = rawPath.find_last_of('/');
      std::string rawLeaf = rawLeafStart == std::string::npos
                                ? rawPath
                                : rawPath.substr(rawLeafStart + 1);
      rawLeaf = stripGeneratedInlineHelperSuffix(std::move(rawLeaf));
      if (rawLeaf == collectionWrapperAlias("vector", "At") ||
          rawLeaf == collectionWrapperAlias("vector", "AtUnsafe") ||
          rawLeaf == "at" || rawLeaf == "at_unsafe") {
        return InlineCallDispatchResult::NotHandled;
      }
      if (const Definition *callee = atCallee; callee != nullptr) {
        const size_t leafStart = callee->fullPath.find_last_of('/');
        std::string leaf = leafStart == std::string::npos
                               ? callee->fullPath
                               : callee->fullPath.substr(leafStart + 1);
        leaf = stripGeneratedInlineHelperSuffix(std::move(leaf));
        if (leaf == collectionWrapperAlias("vector", "At") ||
            leaf == collectionWrapperAlias("vector", "AtUnsafe") ||
            leaf == "at" || leaf == "at_unsafe") {
          return InlineCallDispatchResult::NotHandled;
        }
      }
    }
    std::string experimentalVectorElementType;
    if (getExperimentalVectorConstructorElementTypeAlias(
            expr, experimentalVectorElementType)) {
      Expr rewrittenVectorCtor = expr;
      rewrittenVectorCtor.name = experimentalCollectionMemberPath("vector", "vector");
      rewrittenVectorCtor.namespacePrefix.clear();
      rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};
      const Definition *vectorCtor =
          resolveDefinitionCallFn(rewrittenVectorCtor);
      if (vectorCtor != nullptr) {
        return emitCanonicalInlineDefinitionCall(rewrittenVectorCtor,
                                                 *vectorCtor)
                   ? InlineCallDispatchResult::Emitted
                   : InlineCallDispatchResult::Error;
      }
    }
    const bool isCanonicalStdKeyValueHelperCall =
        isCanonicalPublishedInlineKeyValueHelperPath(rawPath);
    if (isCanonicalStdKeyValueHelperCall && !expr.args.empty()) {
      const auto targetInfo =
          resolveCollectionPairTypeInfo(expr.args.front(),
                                     localsIn,
                                     inferCallKeyValueTargetInfo,
                                     semanticProgram,
                                     semanticIndexPtr);
      std::string directHelperName = rawPath;
      const size_t lastSlash = directHelperName.find_last_of('/');
      if (lastSlash != std::string::npos) {
        directHelperName = directHelperName.substr(lastSlash + 1);
      }
      directHelperName = canonicalInlineKeyValueHelperName(std::move(directHelperName));
      if (targetInfo.isKeyValueTarget &&
          (directHelperName == "count" || directHelperName == "contains" ||
           directHelperName == "tryAt" || directHelperName == "at" ||
           directHelperName == "at_unsafe")) {
        if (directHelperName == "at" || directHelperName == "at_unsafe") {
          return InlineCallDispatchResult::NotHandled;
        }
        if (directHelperName == "count" &&
            expr.args.front().kind == Expr::Kind::Call) {
          return InlineCallDispatchResult::NotHandled;
        }
        if (const Definition *callee = resolveDefinitionCallFn(expr);
            callee != nullptr) {
          return emitCanonicalInlineDefinitionCall(expr, *callee)
                     ? InlineCallDispatchResult::Emitted
                     : InlineCallDispatchResult::Error;
        }
        return InlineCallDispatchResult::NotHandled;
      }
      if (!targetInfo.isKeyValueTarget && expr.args.size() >= 2 &&
          (directHelperName == "contains" || directHelperName == "tryAt" ||
           directHelperName == "at" || directHelperName == "at_unsafe")) {
        const auto alternateTargetInfo =
            resolveCollectionPairTypeInfo(expr.args[1],
                                       localsIn,
                                       inferCallKeyValueTargetInfo,
                                       semanticProgram,
                                       semanticIndexPtr);
        if (alternateTargetInfo.isKeyValueTarget) {
          const LocalInfo::ValueKind keyKind =
              inferExprKind ? inferExprKind(expr.args.front(), localsIn)
                            : LocalInfo::ValueKind::Unknown;
          if (keyKind == LocalInfo::ValueKind::Unknown ||
              alternateTargetInfo.keyValueKeyKind == LocalInfo::ValueKind::Unknown ||
              keyKind != alternateTargetInfo.keyValueKeyKind) {
            return InlineCallDispatchResult::NotHandled;
          }
          if (const Definition *callee = resolveDefinitionCallFn(expr);
              callee != nullptr) {
            return emitCanonicalInlineDefinitionCall(expr, *callee)
                       ? InlineCallDispatchResult::Emitted
                       : InlineCallDispatchResult::Error;
          }
        }
      }
    }
    if (!expr.args.empty() &&
        (resolveKeyValueHelperAliasName(expr, keyValueHelperName) ||
         (getBuiltinArrayAccessName(expr, keyValueHelperName) && expr.args.size() == 2))) {
      keyValueHelperName = canonicalInlineKeyValueHelperName(std::move(keyValueHelperName));
      const auto keyValueTargetInfo =
          resolveCollectionPairTypeInfo(expr.args.front(),
                                     localsIn,
                                     inferCallKeyValueTargetInfo,
                                     semanticProgram,
                                     semanticIndexPtr);
      auto isRewrittenSlashMethodKeyValueAccess = [&]() {
        if (semanticProgram == nullptr) {
          return false;
        }
        const SemanticProgramQueryFact *queryFact = nullptr;
        if (semanticIndexPtr != nullptr) {
          queryFact =
              findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr,
                                           expr);
        }
        if (queryFact == nullptr) {
          std::vector<std::pair<int, int>> sourcePositions;
          if (expr.sourceLine != 0 && expr.sourceColumn != 0) {
            sourcePositions.emplace_back(expr.sourceLine, expr.sourceColumn);
          }
          if (!expr.args.empty() && expr.args.front().sourceLine != 0 &&
              expr.args.front().sourceColumn != 0) {
            sourcePositions.emplace_back(expr.args.front().sourceLine,
                                         expr.args.front().sourceColumn);
          }
          for (const auto &candidate : semanticProgram->queryFacts) {
            const bool sameSourcePosition =
                std::any_of(sourcePositions.begin(), sourcePositions.end(),
                            [&](const auto &sourcePosition) {
                              return candidate.sourceLine == sourcePosition.first &&
                                     candidate.sourceColumn == sourcePosition.second;
                            });
            if (!sameSourcePosition) {
              continue;
            }
            const std::string_view callName =
                candidate.callNameId != InvalidSymbolId
                    ? semanticProgramResolveCallTargetString(
                          *semanticProgram, candidate.callNameId)
                    : std::string_view(candidate.callName);
            if (callName == expr.name ||
                (!expr.sourceName.empty() && callName == expr.sourceName)) {
              queryFact = &candidate;
              break;
            }
          }
        }
        if (queryFact == nullptr ||
            queryFact->resolvedPathId == InvalidSymbolId) {
          return false;
        }
        const std::string resolvedPath =
            std::string(semanticProgramResolveCallTargetString(
                *semanticProgram, queryFact->resolvedPathId));
        if (resolvedPath != "/at" && resolvedPath != "/at_unsafe") {
          return false;
        }
        const std::string queryType =
            trimTemplateTypeText(resolveInlineSemanticTypeText(
                queryFact->queryTypeTextId, queryFact->queryTypeText));
        const std::string bindingType =
            trimTemplateTypeText(resolveInlineSemanticTypeText(
                queryFact->bindingTypeTextId, queryFact->bindingTypeText));
        return queryType == "string" || collection_helpers::isCollectionFamilyRoot(queryType, collection_helpers::CollectionFamily::String) ||
               bindingType == "string" || collection_helpers::isCollectionFamilyRoot(bindingType, collection_helpers::CollectionFamily::String);
      };
      if (keyValueTargetInfo.isKeyValueTarget && !isCanonicalStdKeyValueHelperCall &&
          (expr.sourceIsMethodCall || isRewrittenSlashMethodKeyValueAccess() ||
           expr.name.find('/') == std::string::npos) &&
          (keyValueHelperName == "at" || keyValueHelperName == "at_unsafe")) {
        Expr canonicalKeyValueHelperExpr = expr;
        canonicalKeyValueHelperExpr.isMethodCall = false;
        canonicalKeyValueHelperExpr.isFieldAccess = false;
        canonicalKeyValueHelperExpr.namespacePrefix.clear();
        canonicalKeyValueHelperExpr.name = canonicalKeyValueHelperPath(keyValueHelperName);
        if (const Definition *callee =
                resolveDefinitionCallFn(canonicalKeyValueHelperExpr);
            callee != nullptr &&
            !keepsBuiltinInlineReturnForPublishedKeyValueHelper(
                keyValueHelperName, *callee)) {
          return emitCanonicalInlineDefinitionCall(canonicalKeyValueHelperExpr,
                                                   *callee)
                     ? InlineCallDispatchResult::Emitted
                     : InlineCallDispatchResult::Error;
        }
      }
      if (keyValueTargetInfo.isKeyValueTarget &&
          !isCanonicalStdKeyValueHelperCall &&
          resolveDefinitionCallFn(expr) == nullptr) {
        return InlineCallDispatchResult::NotHandled;
      }
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      const auto targetInfo =
          resolveArrayVectorAccessTargetInfo(expr.args.front(),
                                             localsIn,
                                             {},
                                             semanticProgram,
                                             semanticIndexPtr);
      if (targetInfo.isArgsPackTarget) {
        return InlineCallDispatchResult::NotHandled;
      }
    }
  }
  auto isVectorReturningCallTarget = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding) {
      return false;
    }
    const InlineVectorTargetFact semanticFact =
        classifyInlineVectorTargetFromSemanticFacts(receiverExpr);
    if (semanticFact == InlineVectorTargetFact::Vector) {
      return true;
    }
    const auto receiverTargetInfo =
        resolveArrayVectorAccessTargetInfo(receiverExpr,
                                           localsIn,
                                           {},
                                           semanticProgram,
                                           semanticIndexPtr);
    if (receiverTargetInfo.isArrayOrVectorTarget &&
        receiverTargetInfo.isVectorTarget &&
        !receiverTargetInfo.isArgsPackTarget) {
      return true;
    }
    if (semanticFact == InlineVectorTargetFact::NonVector) {
      return false;
    }
    const Definition *receiverDef = resolveDefinitionCallFn(receiverExpr);
    if (receiverDef == nullptr) {
      return false;
    }
    std::string collectionName;
    std::vector<std::string> collectionArgs;
    return inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs) &&
           collectionName == "vector" && collectionArgs.size() == 1;
  };

  bool deferVectorReturningMutatorCall = false;
  auto isVectorMutatorCallName = [&](const Expr &callExpr) {
    if (isUnqualifiedCollectionBuiltinName(callExpr, "push") ||
        isUnqualifiedCollectionBuiltinName(callExpr, "pop") ||
        isUnqualifiedCollectionBuiltinName(callExpr, "reserve") ||
        isUnqualifiedCollectionBuiltinName(callExpr, "clear") ||
        isUnqualifiedCollectionBuiltinName(callExpr, "remove_at") ||
        isUnqualifiedCollectionBuiltinName(callExpr, "remove_swap")) {
      return true;
    }
    // Qualified canonical vector mutator calls (e.g. /std/collections/vector/push)
    // emitted by inlined stdlib functions like to_aos.
    std::string resolvedName;
    if (resolveVectorHelperAliasName(callExpr, resolvedName)) {
      return resolvedName == "push" || resolvedName == "pop" ||
             resolvedName == "reserve" || resolvedName == "clear" ||
             resolvedName == "remove_at" || resolvedName == "remove_swap";
    }
    return false;
  };
  auto tryEmitVectorMutatorCallFormExpr = [&]() {
    const bool isVectorMutatorCall = isVectorMutatorCallName(expr);
    if (expr.isMethodCall || !isVectorMutatorCall || expr.args.empty()) {
      return InlineCallDispatchResult::NotHandled;
    }

    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= expr.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };

    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
    }

    const bool probePositionalReorderedReceiver =
        !hasNamedArgs && expr.args.size() > 1 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         expr.args.front().kind == Expr::Kind::Name);
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      Expr methodExpr = expr;
      methodExpr.isMethodCall = true;
      methodExpr.semanticNodeId = 0;
      std::string normalizedHelperName;
      if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
        methodExpr.name = normalizedHelperName;
      }
      if (receiverIndex != 0) {
        std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
        if (methodExpr.argNames.size() < methodExpr.args.size()) {
          methodExpr.argNames.resize(methodExpr.args.size());
        }
        std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
      }
      if (isVectorReturningCallTarget(methodExpr.args.front())) {
        deferVectorReturningMutatorCall = true;
        return InlineCallDispatchResult::NotHandled;
      }
      const std::string priorError = error;
      const Definition *callee = resolveMethodCallDefinitionFn(methodExpr, localsIn);
      if (callee == nullptr) {
        error = priorError;
        if (semanticProgram != nullptr) {
          const std::string semanticTarget =
              findSemanticProductMethodCallTarget(semanticProgram, methodExpr);
          if (!semanticTarget.empty()) {
            if (semanticTarget == collection_helpers::kRootedStringCount &&
                methodExpr.args.size() == 1 &&
                isSimpleCallName(methodExpr, "count")) {
              Expr directCall = methodExpr;
              directCall.isMethodCall = false;
              directCall.isFieldAccess = false;
              directCall.namespacePrefix.clear();
              directCall.name = semanticTarget;
              directCall.semanticNodeId = 0;
              callee = resolveDefinitionCallFn(directCall);
            }
            if (callee == nullptr &&
                isBuiltinClassifiedMethodCallTarget(semanticTarget, methodExpr)) {
              continue;
            }
            if (callee != nullptr) {
              error = priorError;
            } else {
            error = "semantic-product method-call target missing lowered definition: " +
                    semanticTarget;
            return InlineCallDispatchResult::Error;
            }
          }
        }
        if (callee == nullptr) {
          continue;
        }
      }
      if (methodExpr.args.size() == 1 &&
          isInternalSoaMetadataHelperPath(callee->fullPath)) {
        error = priorError;
        return InlineCallDispatchResult::NotHandled;
      }
      if (methodExpr.hasBodyArguments || !methodExpr.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return InlineCallDispatchResult::Error;
      }
      if (!emitCanonicalInlineDefinitionCall(methodExpr, *callee)) {
        return InlineCallDispatchResult::Error;
      }
      error = priorError;
      return InlineCallDispatchResult::Emitted;
    }

    return InlineCallDispatchResult::NotHandled;
  };
  const auto vectorMutatorCallFormResult = tryEmitVectorMutatorCallFormExpr();
  if (vectorMutatorCallFormResult != InlineCallDispatchResult::NotHandled) {
    return vectorMutatorCallFormResult;
  }
  if (expr.isMethodCall && expr.args.size() == 1 &&
