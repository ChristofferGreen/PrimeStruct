  auto pickExplicitVectorAccessReceiverIndex = [&](const Expr &candidate) -> size_t {
    if (candidate.args.size() != 2) {
      return 0;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return i;
        }
      }
    }
    return 0;
  };
  auto isNoHelperExplicitVectorAccessCallFallback = [&](const Expr &candidate) {
    if (!isExplicitVectorAccessDirectCall(candidate) || !explicitVectorAccessResolvedTypePath(candidate).empty() ||
        candidate.args.size() != 2) {
      return false;
    }
    const size_t receiverIndex = pickExplicitVectorAccessReceiverIndex(candidate);
    return receiverIndex < candidate.args.size() && isResolvedVectorTarget(candidate.args[receiverIndex]);
  };
  auto isExplicitVectorCountCapacityDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "vector/count" || normalized == "std/collections/vector/count" ||
           normalized == "vector/capacity" || normalized == "std/collections/vector/capacity";
  };
  auto isExplicitVectorCountCapacitySlashMethod = [&](const Expr &candidate, const char *helper) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("vector/") + helper ||
           normalized == std::string("std/collections/vector/") + helper;
  };
  auto isExplicitStdlibVectorHelper = [&](const std::string &name, const char *helper) {
    if (name.empty()) {
      return false;
    }
    std::string normalized = name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("std/collections/vector/") + helper;
  };
  auto isExplicitVectorAccessSlashMethod = [&](const Expr &candidate, const char *helper) {
    if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("vector/") + helper ||
           normalized == std::string("std/collections/vector/") + helper;
  };
  auto pickExplicitVectorCountCapacityReceiverIndex = [&](const Expr &candidate) -> size_t {
    if (candidate.args.size() != 1) {
      return 0;
    }
    if (hasNamedArguments(candidate.argNames) && !candidate.argNames.empty() && candidate.argNames.front().has_value() &&
        *candidate.argNames.front() == "values") {
      return 0;
    }
    return 0;
  };
  auto isNoHelperExplicitVectorCountCapacityCallFallback = [&](const Expr &candidate) {
    if (!isExplicitVectorCountCapacityDirectCall(candidate) || candidate.args.size() != 1) {
      return false;
    }
    const size_t receiverIndex = pickExplicitVectorCountCapacityReceiverIndex(candidate);
    return receiverIndex < candidate.args.size() && isResolvedVectorTarget(candidate.args[receiverIndex]);
  };
  auto emitMissingExplicitVectorCountCapacityCall = [&](const Expr &candidate) {
    const size_t receiverIndex = pickExplicitVectorCountCapacityReceiverIndex(candidate);
    const std::string resolvedPath = resolveExprPath(candidate);
    const bool isCapacity = resolvedPath == "/vector/capacity" ||
                            resolvedPath == "/std/collections/vector/capacity";
    std::ostringstream out;
    out << "ps_missing_vector_" << (isCapacity ? "capacity" : "count") << "_call_helper("
        << emitExpr(candidate.args[receiverIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ")";
    return out.str();
  };
  auto emitMissingExplicitVectorAccessCall = [&](const Expr &candidate) {
    const size_t receiverIndex = pickExplicitVectorAccessReceiverIndex(candidate);
    const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
    const std::string resolvedPath = resolveExprPath(candidate);
    const bool isUnsafe = resolvedPath == "/vector/at_unsafe" ||
                          resolvedPath == "/std/collections/vector/at_unsafe";
    std::ostringstream out;
    out << "ps_missing_vector_" << (isUnsafe ? "at_unsafe" : "at") << "_call_helper("
        << emitExpr(candidate.args[receiverIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ", "
        << emitExpr(candidate.args[indexIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ")";
    return out.str();
  };
  auto pickExplicitMapAccessReceiverIndex = [&](const Expr &candidate) -> size_t {
    if (candidate.args.size() != 2) {
      return 0;
    }
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          return i;
        }
      }
    }
    return 0;
  };
  auto isNoHelperExplicitMapAccessCallFallback = [&](const Expr &candidate) {
    if (!isExplicitMapAccessDirectCall(candidate) || candidate.args.size() != 2) {
      return false;
    }
    const size_t receiverIndex = pickExplicitMapAccessReceiverIndex(candidate);
    if (receiverIndex >= candidate.args.size() || !isResolvedMapTarget(candidate.args[receiverIndex])) {
      return false;
    }
    return nameMap.count(resolveExprPath(candidate)) == 0;
  };
  auto emitMissingExplicitMapAccessCall = [&](const Expr &candidate) {
    const size_t receiverIndex = pickExplicitMapAccessReceiverIndex(candidate);
    const size_t indexIndex = receiverIndex == 0 ? 1 : 0;
    const std::string resolvedPath = resolveExprPath(candidate);
    const bool isUnsafe =
        resolvedPath == "/map/at_unsafe" || resolvedPath == "/std/collections/map/at_unsafe";
    std::ostringstream out;
    out << "ps_missing_map_" << (isUnsafe ? "at_unsafe" : "at") << "_call_helper("
        << emitExpr(candidate.args[receiverIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ", "
        << emitExpr(candidate.args[indexIndex],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ")";
    return out.str();
  };
  auto emitMissingExplicitMapAccessMethod = [&](const Expr &candidate) {
    const std::string resolvedPath = resolveExprPath(candidate);
    const bool isUnsafe =
        resolvedPath == "/map/at_unsafe" || resolvedPath == "/std/collections/map/at_unsafe";
    std::ostringstream out;
    out << "ps_missing_map_" << (isUnsafe ? "at_unsafe" : "at") << "_method_helper("
        << emitExpr(candidate.args[0],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ", "
        << emitExpr(candidate.args[1],
                    nameMap,
                    paramMap,
                    defMap,
                    structTypeMap,
                    importAliases,
                    localTypes,
                    returnKinds,
                    resultInfos,
                    returnStructs,
                    allowMathBare)
        << ")";
    return out.str();
  };
  auto isNoHelperExplicitMapAccessContainsReceiver = [&](const Expr &candidate) {
    return isNoHelperExplicitMapAccessCallFallback(candidate);
  };
  auto isNoHelperExplicitMapAccessMethodContainsReceiver = [&](const Expr &candidate) {
    if (!candidate.isMethodCall || !isExplicitMapAccessMethod(candidate) || candidate.args.size() != 2) {
      return false;
    }
    return nameMap.count(resolveExprPath(candidate)) == 0;
  };
  auto isNoHelperExplicitMapAccessMethodFallback = [&](const Expr &candidate) {
    if (!candidate.isMethodCall || !isExplicitMapAccessMethod(candidate) || candidate.args.size() != 2) {
      return false;
    }
    return nameMap.count(resolveExprPath(candidate)) == 0;
  };
  auto isNoHelperExplicitVectorAccessCountReceiver = [&](const Expr &candidate) {
    if (isExplicitVectorAccessDirectCall(candidate)) {
      return explicitVectorAccessResolvedTypePath(candidate).empty();
    }
    if ((isExplicitVectorAccessSlashMethod(candidate, "at") ||
         isExplicitVectorAccessSlashMethod(candidate, "at_unsafe")) &&
        candidate.isMethodCall) {
      return probedTypePathForTarget(candidate).empty();
    }
    return false;
  };
  auto pickAccessReceiverIndex = [&]() -> size_t {
    if (expr.args.size() != 2) {
      return 0;
    }
    const Expr &leading = expr.args.front();
    const bool leadingCouldBeIndex =
        leading.kind == Expr::Kind::Literal || leading.kind == Expr::Kind::BoolLiteral ||
        leading.kind == Expr::Kind::FloatLiteral || leading.kind == Expr::Kind::StringLiteral;
    const bool leadingIsKnownReceiver = isResolvedArrayLikeTarget(leading) || isResolvedMapTarget(leading) ||
                                        isResolvedStringTarget(leading);
    const bool trailingIsKnownReceiver = isResolvedArrayLikeTarget(expr.args[1]) || isResolvedMapTarget(expr.args[1]) ||
                                         isResolvedStringTarget(expr.args[1]);
    if (!leadingIsKnownReceiver && trailingIsKnownReceiver && leadingCouldBeIndex) {
      return 1;
    }
    return 0;
  };
  auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {
    if (isSimpleCallName(candidate, helper)) {
      return true;
    }
    if (candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == std::string("std/collections/vector/") + helper;
  };
  auto preferStructReturningCollectionHelperPath = [&](const std::string &path) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/' &&
        normalizedPath.rfind("std/collections/map/", 0) == 0) {
      normalizedPath.insert(normalizedPath.begin(), '/');
    }
    std::string firstExisting;
    for (const auto &candidate : collectionHelperPathCandidates(path)) {
      if (nameMap.count(candidate) == 0) {
        continue;
      }
      if (firstExisting.empty()) {
        firstExisting = candidate;
      }
      auto kindIt = returnKinds.find(candidate);
      auto structIt = returnStructs.find(candidate);
      if (kindIt != returnKinds.end() && kindIt->second == ReturnKind::Array &&
          structIt != returnStructs.end() && !structIt->second.empty()) {
        return candidate;
      }
    }
    if (!firstExisting.empty()) {
      return firstExisting;
    }
    return preferVectorStdlibHelperPath(path, nameMap);
  };
  auto preferBareMapHelperPath = [&](const Expr &candidate, const char *helperName) {
    if (candidate.isMethodCall || !isSimpleCallName(candidate, helperName) || candidate.args.size() != 2) {
      return std::string{};
    }
    if (!isResolvedMapTarget(candidate.args.front())) {
      return std::string{};
    }
    const std::string canonicalPath = std::string("/std/collections/map/") + helperName;
    if (nameMap.count(canonicalPath) > 0) {
      return canonicalPath;
    }
    const std::string compatibilityPath = std::string("/map/") + helperName;
    if (nameMap.count(compatibilityPath) > 0) {
      return compatibilityPath;
    }
    return std::string{};
  };
  auto preferExplicitVectorCountCapacityHelperPath = [&](const Expr &candidate, const std::string &path) {
    if (!isExplicitVectorCountCapacityDirectCall(candidate)) {
      return preferStructReturningCollectionHelperPath(path);
    }
    if (path == "/vector/count" || path == "/vector/capacity") {
      return path;
    }
    return preferStructReturningCollectionHelperPath(path);
  };
