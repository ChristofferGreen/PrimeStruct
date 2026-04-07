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
  std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
  if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
    normalizedResolvedPath.erase(normalizedResolvedPath.begin());
  }
  if (normalizedResolvedPath.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) != 0) {
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
         normalizedFieldType != "std/collections/experimental_soa_storage/SoaColumn")) {
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
  if (path == "/std/collections/map/count") {
    return "/std/collections/experimental_map/mapCount";
  }
  if (path == "/std/collections/map/count_ref") {
    return "/std/collections/experimental_map/mapCountRef";
  }
  if (path == "/std/collections/map/contains") {
    return "/std/collections/experimental_map/mapContains";
  }
  if (path == "/std/collections/map/contains_ref") {
    return "/std/collections/experimental_map/mapContainsRef";
  }
  if (path == "/std/collections/map/tryAt") {
    return "/std/collections/experimental_map/mapTryAt";
  }
  if (path == "/std/collections/map/tryAt_ref") {
    return "/std/collections/experimental_map/mapTryAtRef";
  }
  if (path == "/std/collections/map/at") {
    return "/std/collections/experimental_map/mapAt";
  }
  if (path == "/std/collections/map/at_ref") {
    return "/std/collections/experimental_map/mapAtRef";
  }
  if (path == "/std/collections/map/at_unsafe") {
    return "/std/collections/experimental_map/mapAtUnsafe";
  }
  if (path == "/std/collections/map/at_unsafe_ref") {
    return "/std/collections/experimental_map/mapAtUnsafeRef";
  }
  if (path == "/std/collections/map/insert") {
    return "/std/collections/experimental_map/mapInsert";
  }
  if (path == "/std/collections/map/insert_ref") {
    return "/std/collections/experimental_map/mapInsertRef";
  }
  return {};
}

std::string experimentalVectorHelperPathForCanonicalHelper(const std::string &path) {
  if (path == "/std/collections/vector/count" || path == "/vector/count" ||
      path == "/std/collections/vectorCount") {
    return "/std/collections/experimental_vector/vectorCount";
  }
  if (path == "/std/collections/vector/capacity" || path == "/vector/capacity" ||
      path == "/std/collections/vectorCapacity") {
    return "/std/collections/experimental_vector/vectorCapacity";
  }
  if (path == "/std/collections/vector/push" || path == "/vector/push" ||
      path == "/std/collections/vectorPush") {
    return "/std/collections/experimental_vector/vectorPush";
  }
  if (path == "/std/collections/vector/pop" || path == "/vector/pop" ||
      path == "/std/collections/vectorPop") {
    return "/std/collections/experimental_vector/vectorPop";
  }
  if (path == "/std/collections/vector/reserve" || path == "/vector/reserve" ||
      path == "/std/collections/vectorReserve") {
    return "/std/collections/experimental_vector/vectorReserve";
  }
  if (path == "/std/collections/vector/clear" || path == "/vector/clear" ||
      path == "/std/collections/vectorClear") {
    return "/std/collections/experimental_vector/vectorClear";
  }
  if (path == "/std/collections/vector/remove_at" || path == "/vector/remove_at" ||
      path == "/std/collections/vectorRemoveAt") {
    return "/std/collections/experimental_vector/vectorRemoveAt";
  }
  if (path == "/std/collections/vector/remove_swap" || path == "/vector/remove_swap" ||
      path == "/std/collections/vectorRemoveSwap") {
    return "/std/collections/experimental_vector/vectorRemoveSwap";
  }
  if (path == "/std/collections/vector/at" || path == "/vector/at" ||
      path == "/std/collections/vectorAt") {
    return "/std/collections/experimental_vector/vectorAt";
  }
  if (path == "/std/collections/vector/at_unsafe" || path == "/vector/at_unsafe" ||
      path == "/std/collections/vectorAtUnsafe") {
    return "/std/collections/experimental_vector/vectorAtUnsafe";
  }
  return {};
}

std::string experimentalSoaVectorHelperPathForCanonicalHelper(const std::string &path) {
  auto matchesPath = [&](std::string_view basePath) {
    return path == basePath || path.rfind(std::string(basePath) + "__", 0) == 0;
  };
  if (matchesPath("/std/collections/count") ||
      matchesPath("/std/collections/soa_vector/count")) {
    return "/std/collections/experimental_soa_vector/soaVectorCount";
  }
  if (matchesPath("/std/collections/soa_vector/count_ref")) {
    return "/std/collections/experimental_soa_vector/soaVectorCountRef";
  }
  if (matchesPath("/std/collections/get") ||
      matchesPath("/std/collections/soa_vector/get")) {
    return "/std/collections/experimental_soa_vector/soaVectorGet";
  }
  if (matchesPath("/std/collections/soa_vector/get_ref")) {
    return "/std/collections/experimental_soa_vector/soaVectorGetRef";
  }
  if (matchesPath("/std/collections/ref") ||
      matchesPath("/std/collections/soa_vector/ref")) {
    return "/std/collections/experimental_soa_vector/soaVectorRef";
  }
  if (matchesPath("/std/collections/soa_vector/ref_ref")) {
    return "/std/collections/experimental_soa_vector/soaVectorRefRef";
  }
  if (matchesPath("/std/collections/reserve") ||
      matchesPath("/std/collections/soa_vector/reserve")) {
    return "/std/collections/experimental_soa_vector/soaVectorReserve";
  }
  if (matchesPath("/std/collections/push") ||
      matchesPath("/std/collections/soa_vector/push")) {
    return "/std/collections/experimental_soa_vector/soaVectorPush";
  }
  if (matchesPath("/std/collections/to_aos") ||
      matchesPath("/std/collections/soa_vector/to_aos")) {
    return "/std/collections/experimental_soa_vector_conversions/soaVectorToAos";
  }
  if (matchesPath("/std/collections/soa_vector/to_aos_ref")) {
    return "/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef";
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

bool hasVisibleStdCollectionsImportForPath(const Context &ctx, const std::string &path) {
  if (path.rfind("/std/collections/", 0) != 0) {
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
  if (path == "/std/collections/mapCount") {
    return "/std/collections/experimental_map/mapCount";
  }
  if (path == "/std/collections/mapContains") {
    return "/std/collections/experimental_map/mapContains";
  }
  if (path == "/std/collections/mapTryAt") {
    return "/std/collections/experimental_map/mapTryAt";
  }
  if (path == "/std/collections/mapAt") {
    return "/std/collections/experimental_map/mapAt";
  }
  if (path == "/std/collections/mapAtUnsafe") {
    return "/std/collections/experimental_map/mapAtUnsafe";
  }
  if (path == "/std/collections/mapInsert") {
    return "/std/collections/experimental_map/mapInsert";
  }
  return {};
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
