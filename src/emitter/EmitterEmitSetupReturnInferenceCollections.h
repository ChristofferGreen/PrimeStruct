  auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
    return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
           suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
           suffix != "remove_at" && suffix != "remove_swap";
  };
  constexpr std::string_view VectorHelperSurfaceBridgeKey = "collections.vector_helpers";
  auto vectorHelperPath = [&](std::string_view helperName) {
    const auto *metadata =
        findPublishedCollectionHelperSurfaceMetadataByBridgeKey(
            VectorHelperSurfaceBridgeKey);
    if (metadata == nullptr || helperName.empty()) {
      return std::string{};
    }
    return std::string(metadata->canonicalPath) + "/" + std::string(helperName);
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
    if (preferred.rfind("/map/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string stdlibAlias =
          "/std/collections/map/" + preferred.substr(std::string("/map/").size());
      if (defMap.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      }
    }
    if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap.count(preferred) == 0) {
      const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
      if (!isCanonicalMapHelperName(suffix)) {
        const std::string mapAlias = "/map/" + suffix;
        if (defMap.count(mapAlias) > 0) {
          preferred = mapAlias;
        }
      }
    }
    return preferred;
  };
  auto normalizeCollectionMethodName = [](const std::string &receiverStruct,
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
    if (receiverStruct == "/map") {
      const std::string mapPrefix = "map/";
      const std::string stdMapPrefix = "std/collections/map/";
      if (methodName.rfind(mapPrefix, 0) == 0) {
        return methodName.substr(mapPrefix.size());
      }
      if (methodName.rfind(stdMapPrefix, 0) == 0) {
        return methodName.substr(stdMapPrefix.size());
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
    if (receiverStruct == "/map") {
      const std::string_view explicitHelperName = mapHelperNameFromPath(rawMethodName);
      const bool isExplicitCompatibilityMethod =
          rawMethodName.rfind("map/", 0) == 0 &&
          !explicitHelperName.empty() &&
          isCanonicalMapHelperName(explicitHelperName);
      const bool isExplicitCanonicalMethod =
          rawMethodName.rfind("std/collections/map/", 0) == 0 &&
          !explicitHelperName.empty() &&
          isCanonicalMapHelperName(explicitHelperName);
      if (isExplicitCompatibilityMethod) {
        return {"/map/" + methodName};
      }
      if (isExplicitCanonicalMethod) {
        return {"/std/collections/map/" + methodName};
      }
      const bool isMapHelperMethod = isCanonicalMapHelperName(methodName);
      if (isMapHelperMethod) {
        return {"/std/collections/map/" + methodName};
      }
      std::vector<std::string> candidates = {
          "/std/collections/map/" + methodName,
      };
      const bool blocksRemovedMapAliasStructReturnForwarding =
          !explicitHelperName.empty() &&
          isCanonicalMapAccessHelperName(explicitHelperName);
      if (!blocksRemovedMapAliasStructReturnForwarding) {
        candidates.push_back("/map/" + methodName);
      }
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
          normalizedPath.rfind("std/collections/" "soa" "_vector/", 0) == 0 ||
          normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
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
    } else if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (!isCanonicalMapHelperName(suffix)) {
        appendUnique("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix =
          normalizedPath.substr(std::string("/std/collections/map/").size());
      if (!isCanonicalMapHelperName(suffix)) {
        appendUnique("/map/" + suffix);
      }
    }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [&](const std::string &path,
                                                               std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
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
      const std::string suffix =
          normalizedPath.substr(std::string("/std/collections/map/").size());
      if (isCanonicalMapAccessHelperName(suffix)) {
        eraseCandidate("/map/" + suffix);
      }
    }
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
