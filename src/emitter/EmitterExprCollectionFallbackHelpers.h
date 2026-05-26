  constexpr std::string_view CollectionFallbackVectorHelperSurfaceBridgeKey =
      "collections.vector_helpers";
  auto collectionFallbackVectorHelperMemberName = [&](const Expr &candidate,
                                                      bool includeImportAliases)
      -> std::string {
    std::string memberName;
    if (!resolvePublishedCollectionSurfacePathMemberName(
            resolveExprPath(candidate),
            CollectionFallbackVectorHelperSurfaceBridgeKey,
            includeImportAliases,
            memberName)) {
      return "";
    }
    return memberName;
  };
  auto isExplicitVectorCountCapacityDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    const std::string memberName =
        collectionFallbackVectorHelperMemberName(candidate, false);
    return memberName == "count" || memberName == "capacity";
  };
  auto isExplicitVectorAccessAliasDirectCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    const std::string memberName =
        collectionFallbackVectorHelperMemberName(candidate, false);
    return memberName == "at" || memberName == "at_unsafe";
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
  auto isUnqualifiedCollectionBuiltinName = [&](const Expr &candidate,
                                                const char *helper) -> bool {
    if (candidate.kind != Expr::Kind::Call || helper == nullptr ||
        candidate.name != helper) {
      return false;
    }
    if (!candidate.namespacePrefix.empty()) {
      return false;
    }
    return candidate.name.find('/') == std::string::npos;
  };
  auto preferStructReturningCollectionHelperPath = [&](const std::string &path) {
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
