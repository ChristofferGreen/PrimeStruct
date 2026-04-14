  auto isExplicitVectorCountCapacityDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "std/collections/vector/count" ||
           normalized == "std/collections/vector/capacity";
  };
  auto isExplicitVectorAccessAliasDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized == "std/collections/vector/at" ||
           normalized == "std/collections/vector/at_unsafe";
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
