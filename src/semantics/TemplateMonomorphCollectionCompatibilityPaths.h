#pragma once

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

bool isExplicitRemovedCollectionMethodAlias(const std::string &receiverTypeName,
                                            std::string rawMethodName) {
  if (!rawMethodName.empty() && rawMethodName.front() == '/') {
    rawMethodName.erase(rawMethodName.begin());
  }

  std::string_view helperName;
  bool isVectorFamilyReceiver =
      receiverTypeName == "array" || receiverTypeName == "vector" || receiverTypeName == "soa_vector";
  if (isVectorFamilyReceiver) {
    if (rawMethodName.rfind("array/", 0) == 0) {
      helperName = std::string_view(rawMethodName).substr(std::string_view("array/").size());
    } else if (rawMethodName.rfind("std/collections/vector/", 0) == 0) {
      helperName =
          std::string_view(rawMethodName).substr(std::string_view("std/collections/vector/").size());
    }
    return !helperName.empty() && isRemovedVectorCompatibilityHelper(std::string(helperName));
  }

  if (receiverTypeName != "map") {
    return false;
  }
  if (rawMethodName.rfind("map/", 0) == 0) {
    helperName = std::string_view(rawMethodName).substr(std::string_view("map/").size());
  } else if (rawMethodName.rfind("std/collections/map/", 0) == 0) {
    helperName =
        std::string_view(rawMethodName).substr(std::string_view("std/collections/map/").size());
  }
  return !helperName.empty() && isRemovedMapCompatibilityHelper(helperName);
}

std::string preferVectorStdlibHelperPath(const std::string &path,
                                         const std::unordered_map<std::string, Definition> &defs) {
  std::string preferred = path;
  if (preferred.rfind("/soa_vector/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/soa_vector/").size());
    const std::string stdlibAlias = "/std/collections/soa_vector/" + suffix;
    if (defs.count(stdlibAlias) > 0) {
      preferred = stdlibAlias;
    }
  }
  if (preferred.rfind("/std/collections/soa_vector/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/soa_vector/").size());
    const std::string samePathAlias = "/soa_vector/" + suffix;
    if (defs.count(samePathAlias) > 0) {
      preferred = samePathAlias;
    }
  }
  if (preferred.rfind("/array/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (defs.count(stdlibAlias) > 0) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (defs.count(stdlibAlias) > 0) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && defs.count(preferred) == 0) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && !isRemovedMapCompatibilityHelper(suffix)) {
      const std::string mapAlias = "/map/" + suffix;
      if (defs.count(mapAlias) > 0) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

std::string preferVectorStdlibTemplatePath(const std::string &path, const Context &ctx) {
  if (path.rfind("/soa_vector/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/soa_vector/").size());
    const std::string stdlibPath = "/std/collections/soa_vector/" + suffix;
    if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
      return stdlibPath;
    }
    return path;
  }
  if (path.rfind("/std/collections/soa_vector/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/std/collections/soa_vector/").size());
    const std::string aliasPath = "/soa_vector/" + suffix;
    if (ctx.sourceDefs.count(aliasPath) > 0 && ctx.templateDefs.count(aliasPath) > 0) {
      return aliasPath;
    }
    return path;
  }
  if (path.rfind("/array/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/array/").size());
    if (!isRemovedVectorCompatibilityHelper(suffix)) {
      const std::string stdlibPath = "/std/collections/vector/" + suffix;
      if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
        return stdlibPath;
      }
    }
    return path;
  }
  if (path.rfind("/map/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/map/").size());
    if (!isRemovedMapCompatibilityHelper(suffix)) {
      const std::string stdlibPath = "/std/collections/map/" + suffix;
      if (ctx.sourceDefs.count(stdlibPath) > 0 && ctx.templateDefs.count(stdlibPath) > 0) {
        return stdlibPath;
      }
    }
  }
  if (path.rfind("/std/collections/map/", 0) == 0) {
    const std::string suffix = path.substr(std::string("/std/collections/map/").size());
    if (suffix != "map" && !isRemovedMapCompatibilityHelper(suffix)) {
      const std::string mapPath = "/map/" + suffix;
      if (ctx.sourceDefs.count(mapPath) > 0 && ctx.templateDefs.count(mapPath) > 0) {
        return mapPath;
      }
    }
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
  return path == "/map/map" || path == "/map/count" || path == "/map/at" ||
         path == "/map/at_unsafe";
}

bool isExplicitCollectionCompatibilityAliasPath(std::string path) {
  if (path.empty()) {
    return false;
  }
  if (path.front() != '/' &&
      (path.rfind("array/", 0) == 0 || path.rfind("map/", 0) == 0)) {
    path.insert(path.begin(), '/');
  }
  return isCollectionCompatibilityTemplateFallbackPath(path) || path == "/array/count" ||
         path == "/array/capacity" || path == "/array/at" || path == "/array/at_unsafe";
}

bool shouldPreserveCompatibilityTemplatePath(const std::string &path, const Context &ctx) {
  return isCollectionCompatibilityTemplateFallbackPath(path) && ctx.sourceDefs.count(path) > 0 &&
         ctx.templateDefs.count(path) == 0;
}

bool shouldPreserveCanonicalMapTemplatePath(const std::string &path, const Context &ctx) {
  constexpr std::string_view canonicalMapPrefix = "/std/collections/map/";
  if (path.rfind(canonicalMapPrefix, 0) != 0) {
    return false;
  }
  if (ctx.sourceDefs.count(path) == 0 || ctx.templateDefs.count(path) > 0) {
    return false;
  }
  const std::string helper = path.substr(canonicalMapPrefix.size());
  if (helper != "map" && helper != "count" && helper != "at" && helper != "at_unsafe") {
    return false;
  }
  const std::string compatibilityPath = "/map/" + helper;
  return ctx.sourceDefs.count(compatibilityPath) > 0;
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
  if (value == "std/collections/vector") {
    return "vector";
  }
  if (value == "Vector" || value.rfind("Vector__", 0) == 0) {
    return "vector";
  }
  if (value == "std/collections/experimental_vector/Vector" ||
      value.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (isExperimentalSoaVectorTypePath(value)) {
    return "soa_vector";
  }
  if (value == "std/collections/map") {
    return "map";
  }
  if (value == "Map" || value.rfind("Map__", 0) == 0) {
    return "map";
  }
  if (value == "std/collections/experimental_map/Map" ||
      value.rfind("std/collections/experimental_map/Map__", 0) == 0) {
    return "map";
  }
  return value;
}

bool isCollectionReceiverTypeName(const std::string &value) {
  return value == "array" || value == "vector" || value == "soa_vector" || value == "map" || value == "string";
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
