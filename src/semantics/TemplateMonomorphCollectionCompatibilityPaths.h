#pragma once

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isRemovedBorrowedSoaCompatibilityHelper(std::string_view helperName) {
  return helperName == "count_ref" || helperName == "get_ref" ||
         helperName == "ref_ref" || helperName == "to" "_aos_ref";
}

bool isRemovedKeyValueCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "size" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

std::string_view keyValueCompatibilityHelperBase(std::string_view helperName) {
  const size_t specializationSuffix = helperName.find("__");
  if (specializationSuffix != std::string_view::npos) {
    helperName.remove_suffix(helperName.size() - specializationSuffix);
  }
  return helperName;
}

bool isTemplateMonomorphMapCollectionRoot(std::string_view value) {
  const primec::StdlibSurfaceMetadata *metadata = mapHelperSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedValue = std::string(trimLeadingSlash(value));
  auto matchesRoot = [&](std::string_view root) {
    return normalizedValue == trimLeadingSlash(root);
  };
  if (matchesRoot(metadata->canonicalPath)) {
    return true;
  }
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (matchesRoot(alias)) {
      return true;
    }
  }
  return false;
}

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverTypeName,
                                            std::string rawMethodName) {
  if (!rawMethodName.empty() && rawMethodName.front() == '/') {
    rawMethodName.erase(rawMethodName.begin());
  }

  std::string_view helperName;
  std::string helperNameString;
  if (isTemplateMonomorphSoaReceiverType(receiverTypeName)) {
    if (stripTemplateMonomorphSoaHelperPrefix(
            rawMethodName, helperNameString, false)) {
      helperName = helperNameString;
    }
    return !helperName.empty() && isRemovedBorrowedSoaCompatibilityHelper(helperName);
  }

  bool isVectorFamilyReceiver =
      receiverTypeName == "array" || receiverTypeName == "vector" ||
      isTemplateMonomorphSoaReceiverType(receiverTypeName);
  if (isVectorFamilyReceiver) {
    if (rawMethodName.rfind("array/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("array/").size());
    } else if (isUnrootedCanonicalVectorCompatibilityPath(rawMethodName)) {
      helperName = stripUnrootedCanonicalVectorCompatibilityPrefix(rawMethodName);
    } else if (stripTemplateMonomorphSoaHelperPrefix(
                   rawMethodName, helperNameString, false)) {
      helperName = helperNameString;
    }
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(std::string(helperName));
  }

  if (receiverTypeName != "map") {
    return false;
  }
  helperNameString = metadataBackedMapHelperMethodName(rawMethodName);
  if (helperNameString != rawMethodName) {
    helperName = helperNameString;
  }
  return !helperName.empty() &&
         isRemovedKeyValueCompatibilityHelper(keyValueCompatibilityHelperBase(helperName));
}

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, Definition> &defs) {
  std::string preferred = path;
  const std::string samePathSoaPrefix =
      templateMonomorphSamePathSoaHelperPrefix();
  const std::string compatibilitySoaPrefix =
      templateMonomorphCompatibilitySoaHelperPrefix();
  if (preferred.rfind(samePathSoaPrefix, 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(samePathSoaPrefix.size());
    const std::string stdlibAlias = compatibilitySoaPrefix + suffix;
    if (defs.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    }
  }
  if (preferred.rfind(compatibilitySoaPrefix, 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(compatibilitySoaPrefix.size());
    const std::string samePathAlias = samePathSoaPrefix + suffix;
    if (defs.count(samePathAlias) > 0) {
      preferred = samePathAlias;
    }
  }
  if (preferred.rfind("/array/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
      const std::string stdlibAlias =
          canonicalVectorCompatibilityHelperPathOrFallback(suffix);
      if (defs.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  return preferred;
}

std::string preferVectorStdlibTemplatePath(const std::string &path, const Context &ctx) {
  const std::string samePathSoaPrefix =
      templateMonomorphSamePathSoaHelperPrefix();
  const std::string compatibilitySoaPrefix =
      templateMonomorphCompatibilitySoaHelperPrefix();
  if (path.rfind(samePathSoaPrefix, 0) == 0) {
    const std::string suffix = path.substr(samePathSoaPrefix.size());
    const std::string stdlibPath = compatibilitySoaPrefix + suffix;
    if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
      return stdlibPath;
    }
    return path;
  }
  if (path.rfind(compatibilitySoaPrefix, 0) == 0) {
    const std::string suffix = path.substr(compatibilitySoaPrefix.size());
    const std::string aliasPath = samePathSoaPrefix + suffix;
    if (ctx.sourceDefs.count(aliasPath) > 0 && ctx.templateDefs.count(aliasPath) > 0) {
      return aliasPath;
    }
    return path;
  }
  if (path.rfind("/array/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/array/").size());
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
      const std::string stdlibPath =
          canonicalVectorCompatibilityHelperPathOrFallback(suffix);
      if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
        return stdlibPath;
      }
    }
    return path;
  }
  return path;
}

bool definitionAcceptsCallShape(const Definition &def, const Expr &expr) {
  std::vector<ParameterInfo> params;
  params.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    params.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string error;
  return buildOrderedArguments(params, expr.args, expr.argNames, ordered, error);
}

bool definitionHasArgumentCountMismatch(const Definition &def, const Expr &expr) {
  std::vector<ParameterInfo> params;
  params.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo param;
    param.name = paramExpr.name;
    extractExplicitBindingType(paramExpr, param.binding);
    if (paramExpr.args.size() == 1) {
      param.defaultExpr = &paramExpr.args.front();
    }
    params.push_back(std::move(param));
  }
  std::vector<const Expr *> ordered;
  std::string error;
  if (buildOrderedArguments(params, expr.args, expr.argNames, ordered, error)) {
    return false;
  }
  return error.find("argument count mismatch") != std::string::npos;
}

bool hasNamedCallArguments(const Expr &expr) {
  for (const auto &argName : expr.argNames) {
    if (argName.has_value()) {
      return true;
    }
  }
  return false;
}

bool isCollectionCompatibilityTemplateFallbackPath(const std::string &path) {
  (void)path;
  return false;
}

bool isExplicitCollectionCompatibilityAliasPath(std::string path) {
  if (path.empty()) {
    return false;
  }
  if (path.front() != '/' && path.rfind("array/", 0) == 0) {
    path.insert(path.begin(), '/');
  }
  return path == "/array/count" || path == "/array/capacity" ||
         path == "/array/at" || path == "/array/at_unsafe";
}

bool shouldPreserveCompatibilityTemplatePath(const std::string &path, const Context &ctx) {
  return isCollectionCompatibilityTemplateFallbackPath(path) && ctx.sourceDefs.count(path) > 0 &&
         ctx.templateDefs.count(path) == 0;
}

std::string normalizeCollectionReceiverTypeName(std::string value) {
  value = normalizeBindingTypeName(value);
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(value, base, argText) && !base.empty()) {
    value = base;
  }
  if (!value.empty() && value.front() == '/') {
    value.erase(value.begin());
  }
  if (trimLeadingSlash(value) ==
      trimLeadingSlash(canonicalVectorCompatibilityPrefixOrFallback())) {
    return "vector";
  }
  if (value == "Vector" || value.rfind("Vector__", 0) == 0) {
    return "vector";
  }
  if (isLegacyExperimentalVectorCompatibilityTypePath("/" + value)) {
    return "vector";
  }
  if (isExperimentalSoaVectorTypePath(value)) {
    return templateMonomorphSoaReceiverTypeName();
  }
  if (isTemplateMonomorphMapCollectionRoot(value)) {
    return "map";
  }
  const std::string mapBackingName = "Map";
  const bool bareGeneratedMapBacking =
      value.find('/') == std::string::npos &&
      (value == mapBackingName ||
       value.rfind(mapBackingName + "__", 0) == 0);
  if (bareGeneratedMapBacking ||
      isExperimentalCollectionBackingTypeName("map", mapBackingName, value)) {
    return "map";
  }
  return value;
}

bool isCollectionReceiverTypeName(const std::string &value) {
  return value == "array" || value == "vector" ||
         isTemplateMonomorphSoaReceiverType(value) || value == "map" ||
         value == "string";
}

std::string unwrapCollectionReceiverEnvelope(std::string typeName, const std::string &typeTemplateArg = {}) {
  std::string normalizedType = normalizeBindingTypeName(typeName);
  if ((normalizedType == "Reference" || normalizedType == "Pointer") && !typeTemplateArg.empty()) {
    const std::string innerType = normalizeBindingTypeName(typeTemplateArg);
    std::string innerBase;
    std::string innerArgText;
    if (splitTemplateTypeName(innerType, innerBase, innerArgText) && !innerBase.empty()) {
      const std::string normalizedInnerBase = normalizeCollectionReceiverTypeName(innerBase);
      if (isCollectionReceiverTypeName(normalizedInnerBase)) {
        normalizedType = innerType;
      }
    } else {
      const std::string normalizedInner = normalizeCollectionReceiverTypeName(innerType);
      if (isCollectionReceiverTypeName(normalizedInner)) {
        normalizedType = innerType;
      }
    }
  }

  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizedType, base, argText) || base.empty()) {
      return normalizeCollectionReceiverTypeName(normalizedType);
    }

    const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
    if (isCollectionReceiverTypeName(normalizedBase)) {
      return normalizedBase;
    }

    if (base != "Reference" && base != "Pointer") {
      return normalizedBase;
    }

    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return base;
    }

    const std::string innerType = normalizeBindingTypeName(args.front());
    std::string innerBase;
    std::string innerArgText;
    if (splitTemplateTypeName(innerType, innerBase, innerArgText) && !innerBase.empty()) {
      const std::string normalizedInnerBase = normalizeCollectionReceiverTypeName(innerBase);
      if (!isCollectionReceiverTypeName(normalizedInnerBase)) {
        return base;
      }
    } else {
      const std::string normalizedInner = normalizeCollectionReceiverTypeName(innerType);
      if (!isCollectionReceiverTypeName(normalizedInner)) {
        return base;
      }
    }

    normalizedType = innerType;
  }
}
