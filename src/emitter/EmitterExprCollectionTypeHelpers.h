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
  auto collectionHelperPathCandidates = [](const std::string &path) {
    auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
      return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
             suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
             suffix != "remove_at" && suffix != "remove_swap";
    };
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      if (candidate.empty()) {
        return;
      }
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (!isCanonicalMapHelperName(suffix)) {
        appendUnique("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (!isCanonicalMapHelperName(suffix)) {
        appendUnique("/map/" + suffix);
      }
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [](const std::string &path,
                                                              std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (isCanonicalMapAccessHelperName(suffix)) {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (isCanonicalMapAccessHelperName(suffix)) {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto isExplicitVectorAccessDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "std/collections/vector/at" ||
           normalized == "std/collections/vector/at_unsafe";
  };
  auto explicitVectorAccessResolvedTypePath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
      return "";
    }
    const std::string resolvedExprPath = resolveExprPath(candidate);
    if (resolvedExprPath != "/std/collections/vector/at" &&
        resolvedExprPath != "/std/collections/vector/at_unsafe") {
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
    pruneMapAccessStructReturnCompatibilityCandidates(resolvedPath, resolvedCandidates);
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
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/vector/at" && normalized != "std/collections/vector/at_unsafe") {
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
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized.rfind("std/collections/map/", 0) != 0 ||
        !isCanonicalMapAccessHelperName(
            std::string_view(normalized).substr(std::string_view("std/collections/map/").size()))) {
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
    std::string normalized = resolveExprPath(candidate);
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "at" && normalized != "at_unsafe" &&
        normalized != "std/collections/vector/at" &&
        normalized != "std/collections/vector/at_unsafe") {
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
    if (!isCanonicalMapAccessHelperName(normalized)) {
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
    return isCanonicalMapAccessHelperPath(normalized);
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
