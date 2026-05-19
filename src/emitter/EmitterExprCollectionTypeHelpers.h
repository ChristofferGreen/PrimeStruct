  auto normalizedTypePath = [](const std::string &typePath) -> std::string {
    if (typePath == "/array" || typePath == "array") {
      return "/array";
    }
    if (typePath == "/vector" || typePath == "vector") {
      return "/vector";
    }
    if (typePath == "/map" || typePath == "map") {
      return "/map";
    }
    if (typePath == "/string" || typePath == "string") {
      return "/string";
    }
    return "";
  };
  constexpr std::string_view CollectionTypeVectorHelperSurfaceBridgeKey =
      "collections.vector_helpers";
  constexpr std::string_view CollectionTypeKeyValueHelperSurfaceBridgeKey =
      "collections.map_helpers";
  auto collectionTypeVectorHelperMemberName = [&](std::string_view path,
                                                  bool includeImportAliases)
      -> std::string {
    std::string memberName;
    if (!resolvePublishedCollectionSurfacePathMemberName(
            path,
            CollectionTypeVectorHelperSurfaceBridgeKey,
            includeImportAliases,
            memberName)) {
      return "";
    }
    return memberName;
  };
  auto collectionTypeVectorHelperPath = [&](std::string_view memberName) {
    return publishedCollectionSurfaceHelperPath(
        CollectionTypeVectorHelperSurfaceBridgeKey,
        memberName);
  };
  auto isCollectionTypeVectorAccessHelper = [](std::string_view memberName) {
    return memberName == "at" || memberName == "at_unsafe";
  };
  auto isExplicitVectorAccessDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    return isCollectionTypeVectorAccessHelper(
        collectionTypeVectorHelperMemberName(resolveExprPath(candidate), false));
  };
  auto collectionTypeKeyValueHelperMemberName = [&](std::string_view path,
                                                    bool includeImportAliases)
      -> std::string {
    std::string memberName;
    if (!resolvePublishedCollectionSurfacePathMemberName(
            path,
            CollectionTypeKeyValueHelperSurfaceBridgeKey,
            includeImportAliases,
            memberName)) {
      return "";
    }
    return memberName;
  };
  auto isCollectionTypeKeyValueAccessHelper = [](std::string_view memberName) {
    return isCanonicalKeyValueAccessHelperName(memberName);
  };
  auto explicitVectorAccessResolvedTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    const std::string memberName =
        collectionTypeVectorHelperMemberName(resolvedExprPath, false);
    if (!isCollectionTypeVectorAccessHelper(memberName) ||
        resolvedExprPath != collectionTypeVectorHelperPath(memberName)) {
      return "";
    }
    std::vector<std::string> resolvedCandidates =
        collectionHelperPathCandidates(resolvedExprPath);
    for (const auto &resolvedCandidate : resolvedCandidates) {
      auto structIt = returnStructs.find(resolvedCandidate);
      if (structIt != returnStructs.end()) {
        const std::string normalizedStruct = normalizedTypePath(structIt->second);
        if (!normalizedStruct.empty()) {
          return normalizedStruct;
        }
      }
      auto kindIt = returnKinds.find(resolvedCandidate);
      if (kindIt != returnKinds.end()) {
        const std::string normalizedKindType = normalizedTypePath(typeNameForReturnKind(kindIt->second));
        if (!normalizedKindType.empty()) {
          return normalizedKindType;
        }
        return "";
      }
    }
    return "";
  };
  auto probedTypePathForTarget = [&](const Expr &targetExpr) -> std::string {
    Expr probeCall;
    probeCall.kind = Expr::Kind::Call;
    probeCall.isMethodCall = true;
    probeCall.name = "__primec_type_probe";
    probeCall.args.push_back(targetExpr);
    probeCall.argNames.push_back(std::nullopt);

    std::string methodPath;
    if (!resolveMethodCallPath(
            probeCall, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
      return "";
    }
    const size_t suffixPos = methodPath.rfind('/');
    if (suffixPos == std::string::npos || suffixPos == 0) {
      return "";
    }
    return methodPath.substr(0, suffixPos);
  };
  auto resolvedTypePathForResolvedCall = [&](const std::string &resolvedPath) -> std::string {
    auto resolvedCandidates = collectionHelperPathCandidates(resolvedPath);
    for (const auto &candidate : resolvedCandidates) {
      auto structIt = returnStructs.find(candidate);
      if (structIt != returnStructs.end()) {
        std::string normalized = normalizedTypePath(structIt->second);
        if (!normalized.empty()) {
          return normalized;
        }
      }
    }
    for (const auto &candidate : resolvedCandidates) {
      auto kindIt = returnKinds.find(candidate);
      if (kindIt == returnKinds.end()) {
        continue;
      }
      if (kindIt->second == ReturnKind::String) {
        return "/string";
      }
      if (kindIt->second == ReturnKind::Array) {
        return "/array";
      }
    }
    return "";
  };
  auto builtinCanonicalVectorAccessReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    if (!isCollectionTypeVectorAccessHelper(
            collectionTypeVectorHelperMemberName(resolveExprPath(candidate), false))) {
      return "";
    }
    if (candidate.args.empty()) {
      return "";
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    const Expr &receiver = candidate.args[receiverIndex];
    if (receiver.kind == Expr::Kind::StringLiteral || isStringValue(receiver, localTypes)) {
      return "/string";
    }
    if (isVectorValue(receiver, localTypes)) {
      return "/vector";
    }
    if (isArrayValue(receiver, localTypes)) {
      return "/array";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName)) {
        if (collectionName == "vector" || collectionName == "array") {
          return "/" + collectionName;
        }
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/vector" || receiverTypePath == "/array" || receiverTypePath == "/string") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto builtinCanonicalMapAccessReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    if (!isCollectionTypeKeyValueAccessHelper(
            collectionTypeKeyValueHelperMemberName(resolveExprPath(candidate), false))) {
      return "";
    }
    if (candidate.args.empty()) {
      return "";
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return "";
    }
    const Expr &receiver = candidate.args[receiverIndex];
    if (isMapValue(receiver, localTypes)) {
      return "/map";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName) && collectionName == "map") {
        return "/map";
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/map") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto builtinVectorAccessMethodReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
        candidate.args.empty()) {
      return "";
    }
    std::string memberName;
    if (!resolvePublishedCollectionSurfacePathMemberName(
            resolveExprPath(candidate),
            CollectionTypeVectorHelperSurfaceBridgeKey,
            true,
            memberName)) {
      memberName = resolveExprPath(candidate);
      if (!memberName.empty() && memberName.front() == '/') {
        memberName.erase(memberName.begin());
      }
    }
    if (!isCollectionTypeVectorAccessHelper(memberName)) {
      return "";
    }
    const Expr &receiver = candidate.args.front();
    if (receiver.kind == Expr::Kind::StringLiteral || isStringValue(receiver, localTypes)) {
      return "/string";
    }
    if (isVectorValue(receiver, localTypes)) {
      return "/vector";
    }
    if (isArrayValue(receiver, localTypes)) {
      return "/array";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName)) {
        if (collectionName == "vector" || collectionName == "array") {
          return "/" + collectionName;
        }
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/vector" || receiverTypePath == "/array" || receiverTypePath == "/string") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto builtinMapAccessMethodReceiverTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
        candidate.args.empty()) {
      return "";
    }
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (!isCanonicalKeyValueAccessHelperName(normalized)) {
      return "";
    }
    const Expr &receiver = candidate.args.front();
    if (isMapValue(receiver, localTypes)) {
      return "/map";
    }
    if (receiver.kind == Expr::Kind::Call) {
      std::string collectionName;
      if (getBuiltinCollectionName(receiver, collectionName) && collectionName == "map") {
        return "/map";
      }
      const std::string receiverTypePath = resolvedTypePathForResolvedCall(resolveExprPath(receiver));
      if (receiverTypePath == "/map") {
        return receiverTypePath;
      }
    }
    return "";
  };
  auto isExplicitMapAccessMethod = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return isCanonicalKeyValueAccessHelperPath(normalized);
  };
  auto resolvedTypePathForTarget = [&](const Expr &targetExpr) -> std::string {
    if (isStringValue(targetExpr, localTypes)) {
      return "/string";
    }
    if (isMapValue(targetExpr, localTypes)) {
      return "/map";
    }
    if (isVectorValue(targetExpr, localTypes)) {
      return "/vector";
    }
    if (isArrayValue(targetExpr, localTypes)) {
      return "/array";
    }
    if (targetExpr.kind != Expr::Kind::Call) {
      return "";
    }
    std::string collectionName;
    if (getBuiltinCollectionName(targetExpr, collectionName)) {
      if (collectionName == "array" || collectionName == "vector" || collectionName == "map") {
        return "/" + collectionName;
      }
    }
    if (!targetExpr.isMethodCall) {
      const std::string resolvedExplicitVectorAccessType =
          explicitVectorAccessResolvedTypePath(targetExpr);
      if (!resolvedExplicitVectorAccessType.empty()) {
        return resolvedExplicitVectorAccessType;
      }
      const bool shouldProbeBuiltinVectorAccessType =
          !isExplicitVectorAccessDirectCall(targetExpr) &&
          !builtinCanonicalVectorAccessReceiverTypePath(targetExpr).empty();
      const bool shouldProbeBuiltinMapAccessType =
          !builtinCanonicalMapAccessReceiverTypePath(targetExpr).empty();
      if (shouldProbeBuiltinVectorAccessType || shouldProbeBuiltinMapAccessType) {
        const std::string probedTypePath = probedTypePathForTarget(targetExpr);
        if (probedTypePath == "/string" || probedTypePath == "/array" || probedTypePath == "/vector" ||
            probedTypePath == "/map") {
          return probedTypePath;
        }
        if (!probedTypePath.empty()) {
          return "";
        }
      }
      return resolvedTypePathForResolvedCall(resolveExprPath(targetExpr));
    }
    if (isExplicitMapAccessMethod(targetExpr)) {
      const std::string probedTypePath = probedTypePathForTarget(targetExpr);
      if (!probedTypePath.empty()) {
        return probedTypePath;
      }
    }
    if (!builtinMapAccessMethodReceiverTypePath(targetExpr).empty()) {
      const std::string probedTypePath = probedTypePathForTarget(targetExpr);
      if (probedTypePath == "/map" || probedTypePath == "/string") {
        return probedTypePath;
      }
      if (!probedTypePath.empty()) {
        return "";
      }
    }
    if (!builtinVectorAccessMethodReceiverTypePath(targetExpr).empty()) {
      const std::string probedTypePath = probedTypePathForTarget(targetExpr);
      if (probedTypePath == "/string" || probedTypePath == "/array" || probedTypePath == "/vector" ||
          probedTypePath == "/map") {
        return probedTypePath;
      }
      if (!probedTypePath.empty()) {
        return "";
      }
    }
    std::string methodPath;
    if (!resolveMethodCallPath(
            targetExpr, defMap, localTypes, importAliases, structTypeMap, returnKinds, returnStructs, methodPath)) {
      return "";
    }
    return resolvedTypePathForResolvedCall(methodPath);
  };
  auto isResolvedMapTarget = [&](const Expr &targetExpr) -> bool {
    return resolvedTypePathForTarget(targetExpr) == "/map";
  };
  auto isResolvedStringTarget = [&](const Expr &targetExpr) -> bool {
    return resolvedTypePathForTarget(targetExpr) == "/string";
  };
  auto isResolvedArrayLikeTarget = [&](const Expr &targetExpr) -> bool {
    const std::string typePath = resolvedTypePathForTarget(targetExpr);
    return typePath == "/array" || typePath == "/vector";
  };
  auto isResolvedVectorTarget = [&](const Expr &targetExpr) -> bool {
    return resolvedTypePathForTarget(targetExpr) == "/vector";
  };
