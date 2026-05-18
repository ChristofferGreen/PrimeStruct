  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  constexpr std::string_view VectorHelperSurfaceBridgeKey = "collections.vector_helpers";
  constexpr std::string_view KeyValueHelperSurfaceBridgeKey = "collections.map_helpers";
  auto vectorHelperPath = [&](std::string_view helperName) {
    const auto *metadata =
        findPublishedCollectionHelperSurfaceMetadataByBridgeKey(
            VectorHelperSurfaceBridgeKey);
    if (metadata == nullptr || helperName.empty()) {
      return std::string{};
    }
    return std::string(metadata->canonicalPath) + "/" + std::string(helperName);
  };
  auto keyValueHelperPath = [&](std::string_view helperName) {
    return publishedCollectionSurfaceHelperPath(
        KeyValueHelperSurfaceBridgeKey,
        helperName);
  };
  auto surfaceHelperPathForRawMethodName = [&](std::string_view bridgeKey,
                                               std::string_view rawMethodName) {
    const auto *metadata =
        findPublishedCollectionHelperSurfaceMetadataByBridgeKey(bridgeKey);
    if (metadata == nullptr || rawMethodName.empty()) {
      return std::string{};
    }
    std::string normalizedRaw(rawMethodName);
    if (!normalizedRaw.empty() && normalizedRaw.front() != '/') {
      normalizedRaw.insert(normalizedRaw.begin(), '/');
    }
    std::string memberName;
    if (!resolvePublishedCollectionSurfacePathMemberName(
            normalizedRaw,
            bridgeKey,
            true,
            memberName)) {
      return std::string{};
    }
    auto pathForRoot = [&](std::string_view root) {
      std::string normalizedRoot(root);
      if (!normalizedRoot.empty() && normalizedRoot.front() != '/') {
        normalizedRoot.insert(normalizedRoot.begin(), '/');
      }
      if (normalizedRoot.empty() ||
          normalizedRaw.size() <= normalizedRoot.size() ||
          normalizedRaw.rfind(normalizedRoot, 0) != 0 ||
          normalizedRaw[normalizedRoot.size()] != '/') {
        return std::string{};
      }
      return normalizedRoot + "/" + memberName;
    };
    if (std::string canonicalPath = pathForRoot(metadata->canonicalPath);
        !canonicalPath.empty()) {
      return canonicalPath;
    }
    for (const std::string_view alias : metadata->importAliasSpellings) {
      if (std::string aliasPath = pathForRoot(alias); !aliasPath.empty()) {
        return aliasPath;
      }
    }
    return std::string{};
  };
  auto isMapReceiverStruct = [](const std::string &receiverStruct) {
    return normalizeBindingTypeName(receiverStruct) == "map";
  };
  auto resolveVectorHelperMemberName = [&](std::string_view path,
                                           bool includeImportAliases,
                                           std::string &memberNameOut) {
    return resolvePublishedCollectionSurfacePathMemberName(
        path,
        VectorHelperSurfaceBridgeKey,
        includeImportAliases,
        memberNameOut);
  };
  auto preferCollectionHelperPath = [&](const std::string &path) -> std::string {
    std::string preferred = path;
    if (preferred.rfind("/array/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string stdlibAlias = vectorHelperPath(suffix);
        if (defMap.count(stdlibAlias) > 0) {
          return stdlibAlias;
        }
      }
    }
    return preferred;
  };
  auto normalizeCollectionMethodName = [&](const std::string &receiverStruct,
                                           std::string methodName) -> std::string {
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    if (receiverStruct == "/vector" || receiverStruct == "/array") {
      const std::string arrayPrefix = "array/";
      if (methodName.rfind(arrayPrefix, 0) == 0) {
        return methodName.substr(arrayPrefix.size());
      }
      std::string vectorMemberName;
      if (resolvePublishedCollectionSurfacePathMemberName(
              methodName,
              "collections.vector_helpers",
              true,
              vectorMemberName)) {
        return vectorMemberName;
      }
    }
    if (isMapReceiverStruct(receiverStruct)) {
      std::string keyValueMemberName;
      if (resolvePublishedCollectionSurfacePathMemberName(
              methodName,
              KeyValueHelperSurfaceBridgeKey,
              true,
              keyValueMemberName)) {
        return keyValueMemberName;
      }
    }
    return methodName;
  };
  auto collectionMethodPathCandidates = [&](const std::string &receiverStruct,
                                            const std::string &methodName,
                                            const std::string &rawMethodName)
      -> std::vector<std::string> {
    if (receiverStruct == "/vector") {
      std::vector<std::string> candidates = {
          vectorHelperPath(methodName),
      };
      if (allowsArrayVectorCompatibilitySuffix(methodName)) {
        candidates.push_back("/array/" + methodName);
      }
      return candidates;
    }
    if (receiverStruct == "/array") {
      std::vector<std::string> candidates = {
          "/array/" + methodName,
      };
      if (allowsArrayVectorCompatibilitySuffix(methodName)) {
        candidates.push_back(vectorHelperPath(methodName));
      }
      return candidates;
    }
    if (isMapReceiverStruct(receiverStruct)) {
      if (const std::string explicitMapPath =
              surfaceHelperPathForRawMethodName(
                  KeyValueHelperSurfaceBridgeKey,
                  rawMethodName);
          !explicitMapPath.empty()) {
        return {explicitMapPath};
      }
      const bool isKeyValueHelperMethod = isCanonicalMapHelperName(methodName);
      if (isKeyValueHelperMethod) {
        return {keyValueHelperPath(methodName)};
      }
      std::vector<std::string> candidates = {
          keyValueHelperPath(methodName),
      };
      return candidates;
    }
    return {receiverStruct + "/" + methodName};
  };
  auto collectionHelperPathCandidates = [&](const std::string &path) {
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
      std::string vectorMemberName;
      if (normalizedPath.rfind("array/", 0) == 0 ||
          resolveVectorHelperMemberName(normalizedPath, true, vectorMemberName) ||
          normalizedPath.rfind("std/collections/" "soa" "_vector/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        appendUnique(vectorHelperPath(suffix));
      }
    }
    return candidates;
  };
  auto pruneBuiltinVectorAccessStructReturnCandidates =
      [&](const Expr &candidate,
          const std::string &path,
          const std::vector<Expr> &params,
          const std::unordered_map<std::string, std::string> &locals,
          std::vector<std::string> &candidates) {
        std::string normalizedPath = path;
        std::string vectorMemberName;
        if (!resolveVectorHelperMemberName(
                normalizedPath,
                false,
                vectorMemberName)) {
          return;
        }
        if (vectorMemberName != "at" && vectorMemberName != "at_unsafe") {
          return;
        }
        if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
          return;
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
          return;
        }
        const std::string receiverStruct =
            inferStructReturnPath(candidate.args[receiverIndex], params, locals);
        if (receiverStruct != "/vector" && receiverStruct != "/array" &&
            receiverStruct != "/string") {
          return;
        }
        const std::string samePathCandidate = vectorHelperPath(vectorMemberName);
        for (auto it = candidates.begin(); it != candidates.end();) {
          if (*it == samePathCandidate) {
            it = candidates.erase(it);
          } else {
            ++it;
          }
        }
      };
