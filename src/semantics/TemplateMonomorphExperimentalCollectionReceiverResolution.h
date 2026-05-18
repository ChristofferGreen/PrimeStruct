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

std::string experimentalMapBackingLeafForReceiverResolution(std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  const size_t leafStart = typeName.find_last_of('/');
  return leafStart == std::string::npos ? typeName : typeName.substr(leafStart + 1);
}

bool isUnspecializedExperimentalMapBackingTypeForReceiverResolution(
    std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return experimentalMapBackingLeafForReceiverResolution(typeName) == "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", typeName);
}

bool isSpecializedExperimentalMapBackingTypeForReceiverResolution(
    std::string typeName) {
  typeName = normalizeBindingTypeName(std::move(typeName));
  if (!typeName.empty() && typeName.front() == '/') {
    typeName.erase(typeName.begin());
  }
  return experimentalMapBackingLeafForReceiverResolution(typeName) != "Map" &&
         isExperimentalCollectionBackingTypeName("map", "Map", typeName);
}

bool isRootMapConstructorReceiverExpr(const Expr *receiverExpr) {
  if (receiverExpr == nullptr || receiverExpr->kind != Expr::Kind::Call ||
      receiverExpr->isMethodCall || receiverExpr->name.empty()) {
    return false;
  }
  std::string normalizedName = receiverExpr->name;
  if (!receiverExpr->namespacePrefix.empty() &&
      normalizedName.find('/') == std::string::npos) {
    std::string normalizedPrefix = receiverExpr->namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    if (!normalizedPrefix.empty()) {
      normalizedName = normalizedPrefix + "/" + normalizedName;
    }
  }
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  return normalizedName == "map" || normalizedName.rfind("map__", 0) == 0;
}

bool isPublishedMapConstructorReceiverExpr(const Expr *receiverExpr,
                                           const std::string &namespacePrefix,
                                           Context &ctx) {
  if (receiverExpr == nullptr || receiverExpr->kind != Expr::Kind::Call ||
      receiverExpr->isMethodCall || receiverExpr->isBinding) {
    return false;
  }
  return isResolvedPublishedMapConstructorPath(
      resolveCalleePath(*receiverExpr, namespacePrefix, ctx));
}

bool inferPublishedMapConstructorReceiverTemplateArgs(
    const Expr *receiverExpr,
    const std::vector<ParameterInfo> &params,
    const LocalTypeMap &locals,
    bool allowMathBare,
    const std::string &namespacePrefix,
    Context &ctx,
    std::vector<std::string> &templateArgsOut) {
  templateArgsOut.clear();
  if (!isPublishedMapConstructorReceiverExpr(receiverExpr, namespacePrefix, ctx)) {
    return false;
  }
  if (receiverExpr->templateArgs.size() == 2) {
    templateArgsOut = receiverExpr->templateArgs;
    return true;
  }
  if (!receiverExpr->templateArgs.empty() || receiverExpr->args.empty() ||
      receiverExpr->args.size() % 2 != 0) {
    return false;
  }
  auto inferArgType = [&](const Expr &arg, std::string &typeTextOut) {
    BindingInfo binding;
    if (!inferBindingTypeForMonomorph(arg, params, locals, allowMathBare, ctx, binding)) {
      return false;
    }
    typeTextOut = bindingTypeToString(binding);
    return !typeTextOut.empty();
  };
  std::string keyType;
  std::string valueType;
  if (!inferArgType(receiverExpr->args[0], keyType) ||
      !inferArgType(receiverExpr->args[1], valueType)) {
    return false;
  }
  for (size_t i = 2; i + 1 < receiverExpr->args.size(); i += 2) {
    std::string nextKeyType;
    std::string nextValueType;
    if (!inferArgType(receiverExpr->args[i], nextKeyType) ||
        !inferArgType(receiverExpr->args[i + 1], nextValueType) ||
        normalizeBindingTypeName(nextKeyType) != normalizeBindingTypeName(keyType) ||
        normalizeBindingTypeName(nextValueType) != normalizeBindingTypeName(valueType)) {
      return false;
    }
  }
  templateArgsOut = {keyType, valueType};
  return true;
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
      if ((normalizedBase == "Vector" ||
           isLegacyExperimentalVectorCompatibilityPath("/" + normalizedBase)) &&
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
    if (!isLegacyExperimentalVectorCompatibilitySpecializedTypePath(
            "/" + normalizedResolvedPath)) {
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
    if (isUnspecializedExperimentalMapBackingTypeForReceiverResolution(
            normalizedBase)) {
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
  if (!isSpecializedExperimentalMapBackingTypeForReceiverResolution(
          normalizedResolvedPath)) {
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
    if ((normalizedBase == "Vector" ||
         isLegacyExperimentalVectorCompatibilityPath("/" + normalizedBase)) &&
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
  if (!isLegacyExperimentalVectorCompatibilitySpecializedTypePath(
          "/" + normalizedResolvedPath)) {
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
      if (isTemplateMonomorphSoaReceiverType(normalizedBase) &&
          !argText.empty()) {
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
    if (!isTemplateMonomorphSoaReceiverType(
            normalizeCollectionReceiverTypeName(base))) {
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
  if (isRootMapConstructorReceiverExpr(receiverExpr)) {
    return false;
  }
  std::vector<std::string> constructorTemplateArgs;
  if (inferPublishedMapConstructorReceiverTemplateArgs(receiverExpr,
                                                      params,
                                                      locals,
                                                      allowMathBare,
                                                      namespacePrefix,
                                                      ctx,
                                                      constructorTemplateArgs)) {
    return true;
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
  if (isRootMapConstructorReceiverExpr(receiverExpr)) {
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

const StdlibSurfaceMetadata *templateMonomorphKeyValueHelperSurfaceMetadata() {
  return primec::findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

bool resolveTemplateMonomorphKeyValueHelperName(
    std::string path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      templateMonomorphKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr || path.empty()) {
    return false;
  }
  if (path.find('/') != std::string::npos && path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  if (path.find('/') != std::string::npos) {
    const StdlibSurfaceMetadata *pathMetadata =
        primec::findStdlibSurfaceMetadataByResolvedPath(path);
    if (pathMetadata == nullptr || pathMetadata->id != metadata->id) {
      return false;
    }
  }
  const std::string_view helperName =
      primec::resolveStdlibSurfaceMemberName(*metadata, path);
  if (helperName.empty()) {
    return false;
  }
  helperNameOut.assign(helperName);
  return true;
}

bool isTemplateMonomorphMapImportAliasHelperPath(std::string_view path) {
  const StdlibSurfaceMetadata *metadata =
      templateMonomorphKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr || path.empty() || path.front() != '/') {
    return false;
  }
  for (const std::string_view spelling : metadata->importAliasSpellings) {
    if (spelling.empty() || spelling == metadata->canonicalPath) {
      continue;
    }
    std::string rootedSpelling(spelling);
    if (rootedSpelling.front() != '/') {
      rootedSpelling.insert(rootedSpelling.begin(), '/');
    }
    rootedSpelling.push_back('/');
    if (path.rfind(rootedSpelling, 0) == 0) {
      return true;
    }
  }
  return false;
}

std::string templateMonomorphCanonicalKeyValueHelperPath(std::string_view spelling) {
  std::string helperName;
  if (!resolveTemplateMonomorphKeyValueHelperName(std::string(spelling),
                                                  helperName)) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata =
      templateMonomorphKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return std::string(metadata->canonicalPath) + "/" + helperName;
}

bool resolveTemplateMonomorphCanonicalKeyValueHelperName(
    std::string path,
    std::string &helperNameOut) {
  helperNameOut.clear();
  const StdlibSurfaceMetadata *metadata =
      templateMonomorphKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr || path.empty()) {
    return false;
  }
  if (path.front() != '/') {
    path.insert(path.begin(), '/');
  }
  const std::string canonicalPrefix =
      std::string(metadata->canonicalPath) + "/";
  if (path.rfind(canonicalPrefix, 0) != 0) {
    return false;
  }
  return resolveTemplateMonomorphKeyValueHelperName(std::move(path),
                                                    helperNameOut);
}

bool isTemplateMonomorphCanonicalKeyValueHelperPath(const std::string &path) {
  std::string helperName;
  return resolveTemplateMonomorphCanonicalKeyValueHelperName(path, helperName) &&
         helperName != "map" && helperName != "entry";
}

bool isTemplateMonomorphCanonicalKeyValueAccessPath(const std::string &path) {
  std::string helperName;
  return resolveTemplateMonomorphCanonicalKeyValueHelperName(path, helperName) &&
         (helperName == "at" || helperName == "at_unsafe");
}

bool isTemplateMonomorphCanonicalKeyValueCountPath(const std::string &path) {
  std::string helperName;
  return resolveTemplateMonomorphCanonicalKeyValueHelperName(path, helperName) &&
         helperName == "count";
}

std::string templateMonomorphPreferredKeyValueHelperSpellingForMember(
    std::string_view spelling,
    std::string_view preferredPrefix) {
  std::string helperName;
  if (!resolveTemplateMonomorphKeyValueHelperName(std::string(spelling),
                                                  helperName)) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata =
      templateMonomorphKeyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  auto findPreferred = [&](std::span<const std::string_view> spellings) {
    for (const std::string_view candidate : spellings) {
      std::string candidateHelperName;
      if (candidate.rfind(preferredPrefix, 0) == 0 &&
          resolveTemplateMonomorphKeyValueHelperName(std::string(candidate),
                                                     candidateHelperName) &&
          candidateHelperName == helperName) {
        return std::string(candidate);
      }
    }
    return std::string{};
  };
  if (std::string preferred = findPreferred(metadata->loweringSpellings);
      !preferred.empty()) {
    return preferred;
  }
  return findPreferred(metadata->compatibilitySpellings);
}

std::string canonicalMapHelperUnknownTargetPath(const std::string &resolvedPath) {
  std::string helperName;
  if (!resolveTemplateMonomorphCanonicalKeyValueHelperName(resolvedPath,
                                                      helperName) ||
      helperName == "map" || helperName == "entry" ||
      helperName.ends_with("_ref")) {
    return {};
  }
  return templateMonomorphCanonicalKeyValueHelperPath(helperName);
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
  if (isRootMapConstructorReceiverExpr(receiverExpr)) {
    return false;
  }
  if (inferPublishedMapConstructorReceiverTemplateArgs(receiverExpr,
                                                      params,
                                                      locals,
                                                      allowMathBare,
                                                      namespacePrefix,
                                                      ctx,
                                                      templateArgsOut)) {
    return true;
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
  if (isTemplateMonomorphMapImportAliasHelperPath(path)) {
    return {};
  }
  return templateMonomorphPreferredKeyValueHelperSpellingForMember(
      path, experimentalCollectionConstructorRootLocal("map"));
}

std::string experimentalVectorHelperPathForCanonicalHelper(const std::string &path) {
  std::string helperName;
  if (!resolveCanonicalVectorHelperNameFromResolvedPath(path, helperName)) {
    return {};
  }
  const StdlibSurfaceMetadata *metadata = vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return preferredPublishedCollectionLoweringPath(
      helperName, metadata->id, legacyExperimentalVectorCompatibilityPrefix());
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
        primec::StdlibSurfaceId::CollectionsColumnarHelpers,
        candidatePath);
    return canonicalHelperPath == compatibilitySoaHelperTargetPath("count_ref") ||
           canonicalHelperPath == compatibilitySoaHelperTargetPath("get_ref") ||
           canonicalHelperPath == compatibilitySoaHelperTargetPath("ref_ref");
  };
  if (mapsToBorrowedSoaHelper(canonicalSoaCountPath) ||
      mapsToBorrowedSoaHelper(canonicalSoaGetPath) ||
      mapsToBorrowedSoaHelper(canonicalSoaRefPath)) {
    return {};
  }
  auto preferredExperimentalSoaHelper = [](const std::string &candidatePath) {
    return primec::stdlibSurfacePreferredSpellingForMember(
        primec::StdlibSurfaceId::CollectionsColumnarHelpers,
        candidatePath,
        templateMonomorphExperimentalSoaHelperPrefix());
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
  if (path.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) != 0) {
    return false;
  }
  std::string helperName;
  return resolveVectorCompatibilityHelperNameFromResolvedPath(path, helperName);
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
  if (path.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0) {
    return false;
  }
  const auto &importPaths = ctx.program.sourceImports.empty() ? ctx.program.imports : ctx.program.sourceImports;
  if (path.rfind("/std/collections/internal_vector/", 0) == 0) {
    for (const auto &importPath : importPaths) {
      if (importPath == "/std/collections/internal_vector" ||
          importPath == "/std/collections/internal_vector/*" ||
          importPath == path) {
        return true;
      }
    }
    return false;
  }
  for (const auto &importPath : importPaths) {
    if (importPathCoversTarget(importPath, path)) {
      return true;
    }
  }
  return false;
}

std::string experimentalMapHelperPathForWrapperHelper(const std::string &path) {
  if (isTemplateMonomorphMapImportAliasHelperPath(path)) {
    return {};
  }
  return templateMonomorphPreferredKeyValueHelperSpellingForMember(
      path, experimentalCollectionConstructorRootLocal("map"));
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
