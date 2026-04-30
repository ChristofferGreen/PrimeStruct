#pragma once

std::string experimentalCollectionValueBindingTypeText(const BindingInfo &binding) {
  const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
  if (normalizedType == "Reference" || normalizedType == "Pointer") {
    return {};
  }
  return bindingTypeToString(binding);
}

std::string experimentalCollectionBorrowedBindingTypeText(const BindingInfo &binding) {
  const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
  if ((normalizedType != "Reference" && normalizedType != "Pointer") || binding.typeTemplateArg.empty()) {
    return {};
  }
  return binding.typeTemplateArg;
}

bool extractExperimentalVectorElementTypeFromTypeText(const std::string &typeText,
                                                      const Context &ctx,
                                                      std::string &valueTypeOut) {
  valueTypeOut.clear();
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      std::string normalizedBase = normalizeBindingTypeName(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
          return false;
        }
        normalizedType = normalizeBindingTypeName(args.front());
        continue;
      }
      if (!normalizedBase.empty() && normalizedBase.front() == '/') {
        normalizedBase.erase(normalizedBase.begin());
      }
      if ((normalizedBase == "Vector" || normalizedBase == "std/collections/experimental_vector/Vector") &&
          !argText.empty()) {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
          return false;
        }
        valueTypeOut = args.front();
        return true;
      }
    }

    std::string resolvedPath = normalizedType;
    if (!resolvedPath.empty() && resolvedPath.front() != '/') {
      resolvedPath.insert(resolvedPath.begin(), '/');
    }
    std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
    if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
      normalizedResolvedPath.erase(normalizedResolvedPath.begin());
    }
    if (normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) != 0) {
      return false;
    }
    auto defIt = ctx.sourceDefs.find(resolvedPath);
    if (defIt == ctx.sourceDefs.end()) {
      return false;
    }
    for (const auto &fieldExpr : defIt->second.statements) {
      if (!fieldExpr.isBinding || fieldExpr.name != "data") {
        continue;
      }
      BindingInfo fieldBinding;
      if (!extractExplicitBindingType(fieldExpr, fieldBinding)) {
        continue;
      }
      if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" || fieldBinding.typeTemplateArg.empty()) {
        continue;
      }
      std::string pointeeBase;
      std::string pointeeArgText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg), pointeeBase, pointeeArgText) ||
          normalizeBindingTypeName(pointeeBase) != "uninitialized") {
        continue;
      }
      std::vector<std::string> pointeeArgs;
      if (!splitTopLevelTemplateArgs(pointeeArgText, pointeeArgs) || pointeeArgs.size() != 1) {
        continue;
      }
      valueTypeOut = pointeeArgs.front();
      return true;
    }
    return false;
  }
}

bool extractExperimentalMapValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                 const Context &ctx,
                                                                 std::vector<std::string> &templateArgsOut) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText)) {
      break;
    }
    std::string normalizedBase = normalizeBindingTypeName(base);
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      normalizedType = normalizeBindingTypeName(args.front());
      continue;
    }
    if (normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map") {
      return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 2;
    }
    break;
  }
  std::string resolvedPath = normalizedType;
  if (!resolvedPath.empty() && resolvedPath.front() != '/') {
    resolvedPath.insert(resolvedPath.begin(), '/');
  }
  std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
  if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
    normalizedResolvedPath.erase(normalizedResolvedPath.begin());
  }
  if (normalizedResolvedPath.rfind("std/collections/experimental_map/Map__", 0) != 0) {
    return false;
  }
  auto defIt = ctx.sourceDefs.find(resolvedPath);
  if (defIt == ctx.sourceDefs.end()) {
    return false;
  }
  std::string keyType;
  std::string valueType;
  for (const auto &fieldExpr : defIt->second.statements) {
    if (!fieldExpr.isBinding) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!extractExplicitBindingType(fieldExpr, fieldBinding)) {
      continue;
    }
    std::string fieldValueType;
    if (!extractExperimentalVectorElementTypeFromTypeText(bindingTypeToString(fieldBinding), ctx, fieldValueType)) {
      continue;
    }
    if (fieldExpr.name == "keys") {
      keyType = fieldValueType;
    } else if (fieldExpr.name == "payloads") {
      valueType = fieldValueType;
    }
  }
  if (keyType.empty() || valueType.empty()) {
    return false;
  }
  templateArgsOut = {keyType, valueType};
  return true;
}

bool extractExperimentalVectorValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                    const Context &ctx,
                                                                    std::vector<std::string> &templateArgsOut) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText)) {
    std::string normalizedBase = normalizeBindingTypeName(base);
    if (!normalizedBase.empty() && normalizedBase.front() == '/') {
      normalizedBase.erase(normalizedBase.begin());
    }
    if ((normalizedBase == "Vector" || normalizedBase == "std/collections/experimental_vector/Vector") &&
        !argText.empty()) {
      return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
    }
  }
  std::string resolvedPath = normalizedType;
  if (!resolvedPath.empty() && resolvedPath.front() != '/') {
    resolvedPath.insert(resolvedPath.begin(), '/');
  }
  std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
  if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
    normalizedResolvedPath.erase(normalizedResolvedPath.begin());
  }
  if (normalizedResolvedPath.rfind("std/collections/experimental_vector/Vector__", 0) != 0) {
    return false;
  }
  auto defIt = ctx.sourceDefs.find(resolvedPath);
  if (defIt == ctx.sourceDefs.end()) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "data") {
      continue;
    }
    BindingInfo fieldBinding;
    if (!extractExplicitBindingType(fieldExpr, fieldBinding)) {
      continue;
    }
    if (normalizeBindingTypeName(fieldBinding.typeName) != "Pointer" || fieldBinding.typeTemplateArg.empty()) {
      continue;
    }
    std::string pointeeBase;
    std::string pointeeArgText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(fieldBinding.typeTemplateArg), pointeeBase, pointeeArgText) ||
        normalizeBindingTypeName(pointeeBase) != "uninitialized") {
      continue;
    }
    return splitTopLevelTemplateArgs(pointeeArgText, templateArgsOut) && templateArgsOut.size() == 1;
  }
  return false;
}

bool extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(const std::string &typeText,
                                                                       const Context &ctx,
                                                                       std::vector<std::string> &templateArgsOut) {
  std::string normalizedType = normalizeBindingTypeName(typeText);
  while (true) {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
          return false;
        }
        normalizedType = normalizeBindingTypeName(args.front());
        continue;
      }
      if (normalizedBase == "soa_vector" && !argText.empty()) {
        return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
      }
    }
    break;
  }
  std::string resolvedPath = normalizedType;
  if (!resolvedPath.empty() && resolvedPath.front() != '/') {
    resolvedPath.insert(resolvedPath.begin(), '/');
  }
  const std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
  if (!isExperimentalSoaVectorSpecializedTypePath(normalizedResolvedPath)) {
    return false;
  }
  for (const auto &[cacheKey, specializedPath] : ctx.specializationCache) {
    if (normalizeBindingTypeName(specializedPath) != normalizeBindingTypeName(resolvedPath)) {
      continue;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(cacheKey, base, argText) || base.empty()) {
      continue;
    }
    if (normalizeCollectionReceiverTypeName(base) != "soa_vector") {
      continue;
    }
    return splitTopLevelTemplateArgs(argText, templateArgsOut) && templateArgsOut.size() == 1;
  }
  auto defIt = ctx.sourceDefs.find(resolvedPath);
  if (defIt == ctx.sourceDefs.end()) {
    return false;
  }
  for (const auto &fieldExpr : defIt->second.statements) {
    if (!fieldExpr.isBinding || fieldExpr.name != "storage") {
      continue;
    }
    BindingInfo fieldBinding;
    if (!extractExplicitBindingType(fieldExpr, fieldBinding)) {
      continue;
    }
    std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
    if (!normalizedFieldType.empty() && normalizedFieldType.front() == '/') {
      normalizedFieldType.erase(normalizedFieldType.begin());
    }
    if (fieldBinding.typeTemplateArg.empty() ||
        (normalizedFieldType != "SoaColumn" &&
         normalizedFieldType != "std/collections/internal_soa_storage/SoaColumn")) {
      continue;
    }
    return splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, templateArgsOut) &&
           templateArgsOut.size() == 1;
  }
  return false;
}

bool resolvesExperimentalMapValueReceiver(const Expr *receiverExpr,
                                          const std::vector<ParameterInfo> &params,
                                          const LocalTypeMap &locals,
                                          bool allowMathBare,
                                          const SubstMap &mapping,
                                          const std::unordered_set<std::string> &allowedParams,
                                          const std::string &namespacePrefix,
                                          Context &ctx) {
  if (receiverExpr == nullptr) {
    return false;
  }
  BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      resolvesExperimentalMapValueTypeText(experimentalCollectionValueBindingTypeText(receiverInfo),
                                           mapping,
                                           allowedParams,
                                           namespacePrefix,
                                           ctx)) {
    return true;
  }
  const std::string inferredReceiverType =
      inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
  return resolvesExperimentalMapValueTypeText(inferredReceiverType, mapping, allowedParams, namespacePrefix, ctx);
}

bool resolvesExperimentalMapBorrowedReceiver(const Expr *receiverExpr,
                                             const std::vector<ParameterInfo> &params,
                                             const LocalTypeMap &locals,
                                             bool allowMathBare,
                                             const SubstMap &mapping,
                                             const std::unordered_set<std::string> &allowedParams,
                                             const std::string &namespacePrefix,
                                             Context &ctx) {
  if (receiverExpr == nullptr) {
    return false;
  }
  BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      resolvesExperimentalMapValueTypeText(experimentalCollectionBorrowedBindingTypeText(receiverInfo),
                                           mapping,
                                           allowedParams,
                                           namespacePrefix,
                                           ctx)) {
    return true;
  }
  std::string inferredReceiverType =
      inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(inferredReceiverType, base, argText)) {
    return false;
  }
  base = normalizeBindingTypeName(base);
  if (base != "Reference" && base != "Pointer") {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  return resolvesExperimentalMapValueTypeText(args.front(), mapping, allowedParams, namespacePrefix, ctx);
}

bool resolvesExperimentalVectorValueReceiver(const Expr *receiverExpr,
                                             const std::vector<ParameterInfo> &params,
                                             const LocalTypeMap &locals,
                                             bool allowMathBare,
                                             const std::string &namespacePrefix,
                                             Context &ctx) {
  if (receiverExpr == nullptr) {
    return false;
  }
  if (receiverExpr->kind == Expr::Kind::Name) {
    if (auto localIt = locals.find(receiverExpr->name); localIt != locals.end()) {
      if (resolvesExperimentalVectorValueTypeText(bindingTypeToString(localIt->second))) {
        return true;
      }
    }
    for (const auto &param : params) {
      if (param.name == receiverExpr->name &&
          resolvesExperimentalVectorValueTypeText(bindingTypeToString(param.binding))) {
        return true;
      }
    }
  }
  BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      resolvesExperimentalVectorValueTypeText(experimentalCollectionValueBindingTypeText(receiverInfo))) {
    return true;
  }
  const std::string inferredReceiverType =
      inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare);
  return resolvesExperimentalVectorValueTypeText(inferredReceiverType);
}

std::string canonicalMapHelperUnknownTargetPath(const std::string &resolvedPath) {
  if (resolvedPath == "/std/collections/map/count" || resolvedPath == "/map/count" ||
      resolvedPath == "/std/collections/mapCount") {
    return "/std/collections/map/count";
  }
  if (resolvedPath == "/std/collections/map/contains" || resolvedPath == "/map/contains" ||
      resolvedPath == "/std/collections/mapContains") {
    return "/std/collections/map/contains";
  }
  if (resolvedPath == "/std/collections/map/tryAt" || resolvedPath == "/map/tryAt" ||
      resolvedPath == "/std/collections/mapTryAt") {
    return "/std/collections/map/tryAt";
  }
  if (resolvedPath == "/std/collections/map/at" || resolvedPath == "/map/at" ||
      resolvedPath == "/std/collections/mapAt") {
    return "/std/collections/map/at";
  }
  if (resolvedPath == "/std/collections/map/at_unsafe" || resolvedPath == "/map/at_unsafe" ||
      resolvedPath == "/std/collections/mapAtUnsafe") {
    return "/std/collections/map/at_unsafe";
  }
  if (resolvedPath == "/std/collections/map/insert" || resolvedPath == "/map/insert" ||
      resolvedPath == "/std/collections/mapInsert") {
    return "/std/collections/map/insert";
  }
  return {};
}

bool resolveExperimentalMapValueReceiverTemplateArgs(const Expr *receiverExpr,
                                                     const std::vector<ParameterInfo> &params,
                                                     const LocalTypeMap &locals,
                                                     bool allowMathBare,
                                                     const std::string &namespacePrefix,
                                                     Context &ctx,
                                                     std::vector<std::string> &templateArgsOut) {
  templateArgsOut.clear();
  if (receiverExpr == nullptr) {
    return false;
  }
  BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      normalizeBindingTypeName(receiverInfo.typeName) != "Reference" &&
      normalizeBindingTypeName(receiverInfo.typeName) != "Pointer") {
    std::string keyType;
    std::string valueType;
    if (extractMapKeyValueTypesFromTypeText(bindingTypeToString(receiverInfo), keyType, valueType)) {
      templateArgsOut = {keyType, valueType};
      return true;
    }
    if (extractExperimentalMapValueReceiverTemplateArgsFromTypeText(bindingTypeToString(receiverInfo),
                                                                    ctx,
                                                                    templateArgsOut)) {
      return true;
    }
  }
  return extractExperimentalMapValueReceiverTemplateArgsFromTypeText(
      inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare),
      ctx,
      templateArgsOut);
}

std::string experimentalMapHelperPathForCanonicalHelper(const std::string &path) {
  return primec::stdlibSurfacePreferredSpellingForMember(
      primec::StdlibSurfaceId::CollectionsMapHelpers,
      path,
      "/std/collections/experimental_map/");
}

std::string experimentalVectorHelperPathForCanonicalHelper(const std::string &path) {
  return primec::stdlibSurfacePreferredSpellingForMember(
      primec::StdlibSurfaceId::CollectionsVectorHelpers,
      path,
      "/std/collections/experimental_vector/");
}

std::string experimentalSoaVectorHelperPathForCanonicalHelper(const std::string &path) {
  auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
  const std::string canonicalSoaGetPath =
      canonicalizeSoaHelperPath(canonicalizeLegacySoaGetHelperPath(path));
  const std::string canonicalSoaRefPath =
      canonicalizeSoaHelperPath(canonicalizeLegacySoaRefHelperPath(path));
  if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, "count_ref") ||
      isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get_ref") ||
      isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref_ref")) {
    return {};
  }
  auto mapsToBorrowedSoaHelper = [](const std::string &candidatePath) {
    const std::string canonicalHelperPath = primec::stdlibSurfaceCanonicalHelperPath(
        primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
        candidatePath);
    return canonicalHelperPath == "/std/collections/soa_vector/count_ref" ||
           canonicalHelperPath == "/std/collections/soa_vector/get_ref" ||
           canonicalHelperPath == "/std/collections/soa_vector/ref_ref";
  };
  if (mapsToBorrowedSoaHelper(canonicalSoaCountPath) ||
      mapsToBorrowedSoaHelper(canonicalSoaGetPath) ||
      mapsToBorrowedSoaHelper(canonicalSoaRefPath)) {
    return {};
  }
  auto preferredExperimentalSoaHelper = [](const std::string &candidatePath) {
    return primec::stdlibSurfacePreferredSpellingForMember(
        primec::StdlibSurfaceId::CollectionsSoaVectorHelpers,
        candidatePath,
        "/std/collections/experimental_soa_vector/");
  };
  if (std::string preferredPath = preferredExperimentalSoaHelper(canonicalSoaCountPath);
      !preferredPath.empty()) {
    return preferredPath;
  }
  if (std::string preferredPath = preferredExperimentalSoaHelper(canonicalSoaGetPath);
      !preferredPath.empty()) {
    return preferredPath;
  }
  if (std::string preferredPath = preferredExperimentalSoaHelper(canonicalSoaRefPath);
      !preferredPath.empty()) {
    return preferredPath;
  }
  return {};
}

bool isExperimentalVectorPublicHelperPath(const std::string &path) {
  auto matchesHelper = [&](std::string_view helperName) {
    const std::string base = "/std/collections/experimental_vector/" + std::string(helperName);
    return path == base || path.rfind(base + "__t", 0) == 0;
  };
  return matchesHelper("vectorCount") || matchesHelper("vectorCapacity") || matchesHelper("vectorPush") ||
         matchesHelper("vectorPop") || matchesHelper("vectorReserve") || matchesHelper("vectorClear") ||
         matchesHelper("vectorRemoveAt") || matchesHelper("vectorRemoveSwap") || matchesHelper("vectorAt") ||
         matchesHelper("vectorAtUnsafe");
}

bool isExperimentalSoaVectorPublicHelperPath(const std::string &path) {
  return isExperimentalSoaVectorHelperFamilyPath(path);
}

bool hasVisibleStdCollectionsImportForPath(const Context &ctx, const std::string &path) {
  if (path.rfind("/std/collections/", 0) != 0) {
    return true;
  }
  if (usesStdlibScopedImportAliases("", ctx)) {
    return true;
  }
  const auto &importPaths = ctx.program.sourceImports.empty() ? ctx.program.imports : ctx.program.sourceImports;
  for (const auto &importPath : importPaths) {
    if (importPathCoversTarget(importPath, path)) {
      return true;
    }
  }
  return false;
}

std::string experimentalMapHelperPathForWrapperHelper(const std::string &path) {
  return primec::stdlibSurfacePreferredSpellingForMember(
      primec::StdlibSurfaceId::CollectionsMapHelpers,
      path,
      "/std/collections/experimental_map/");
}

bool resolveExperimentalVectorValueReceiverTemplateArgs(const Expr *receiverExpr,
                                                        const std::vector<ParameterInfo> &params,
                                                        const LocalTypeMap &locals,
                                                        bool allowMathBare,
                                                        const std::string &namespacePrefix,
                                                        Context &ctx,
                                                        std::vector<std::string> &templateArgsOut) {
  templateArgsOut.clear();
  if (receiverExpr == nullptr) {
    return false;
  }
  if (receiverExpr->kind == Expr::Kind::Name) {
    if (auto localIt = locals.find(receiverExpr->name); localIt != locals.end()) {
      if (extractExperimentalVectorValueReceiverTemplateArgsFromTypeText(
              bindingTypeToString(localIt->second), ctx, templateArgsOut)) {
        return true;
      }
    }
    for (const auto &param : params) {
      if (param.name == receiverExpr->name &&
          extractExperimentalVectorValueReceiverTemplateArgsFromTypeText(
              bindingTypeToString(param.binding), ctx, templateArgsOut)) {
        return true;
      }
    }
  }
  BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
      extractExperimentalVectorValueReceiverTemplateArgsFromTypeText(
          experimentalCollectionValueBindingTypeText(receiverInfo), ctx, templateArgsOut)) {
    return true;
  }
  return extractExperimentalVectorValueReceiverTemplateArgsFromTypeText(
      inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare),
      ctx,
      templateArgsOut);
}
