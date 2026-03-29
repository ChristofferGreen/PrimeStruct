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
    if (std::string_view(helperName) == "contains" || std::string_view(helperName) == "tryAt") {
      return std::string{};
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
