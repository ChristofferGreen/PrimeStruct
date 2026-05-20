#pragma once

bool isNonTypeTransformName(const std::string &name) {
  return name == "return" || name == "effects" || name == "capabilities" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes" || name == "on_error" ||
         name == "struct" || name == "enum" || name == "unsafe" ||
         name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding" || name == "public" || name == "private" ||
         name == "static" || name == "single_type_to_return" || name == "stack" || name == "heap" || name == "buffer" ||
         name == "Additive" || name == "Multiplicative" || name == "Comparable" || name == "Indexable";
}

bool isBuiltinTemplateContainer(const std::string &name) {
  return name == "array" || name == "vector" || name == "soa" || name == "map" || name == "Result" ||
         name == "File" || isBuiltinTemplateTypeName(name);
}

std::string templateMonomorphSoaReceiverTypeName() {
  return internalSoaCollectionTypeName();
}

bool isTemplateMonomorphSoaReceiverType(std::string_view receiverTypeName) {
  return receiverTypeName == templateMonomorphSoaReceiverTypeName();
}

std::string templateMonomorphSamePathSoaHelperPrefix(bool leadingSlash = true) {
  std::string prefix = samePathSoaHelperTargetPath("");
  if (!leadingSlash && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

std::string templateMonomorphCompatibilitySoaHelperPrefix(bool leadingSlash = true) {
  std::string prefix = compatibilitySoaHelperTargetPath("");
  prefix += "/";
  if (!leadingSlash && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

std::string templateMonomorphPublicSoaHelperPrefix(bool leadingSlash = true) {
  std::string prefix = publicSoaHelperTargetPath("");
  prefix += "/";
  if (!leadingSlash && !prefix.empty() && prefix.front() == '/') {
    prefix.erase(prefix.begin());
  }
  return prefix;
}

std::string templateMonomorphExperimentalSoaHelperPrefix() {
  std::string prefix = experimentalSoaStorageTypePath(true);
  const size_t lastSlash = prefix.find_last_of('/');
  if (lastSlash != std::string::npos) {
    prefix.erase(lastSlash + 1);
  }
  return prefix;
}

std::string templateMonomorphSoaToAosHelperName(bool borrowed = false) {
  std::string name = "to";
  name += "_";
  name += "aos";
  if (borrowed) {
    name += "_ref";
  }
  return name;
}

std::string templateMonomorphSoaToSoaHelperName() {
  std::string name = "to";
  name += "_";
  name += "soa";
  return name;
}

bool stripTemplateMonomorphSoaHelperPrefix(std::string_view path,
                                           std::string &helperNameOut,
                                           bool leadingSlash = true) {
  const std::string samePathPrefix =
      templateMonomorphSamePathSoaHelperPrefix(leadingSlash);
  if (path.rfind(samePathPrefix, 0) == 0) {
    helperNameOut = std::string(path.substr(samePathPrefix.size()));
    return true;
  }
  const std::string compatibilityPrefix =
      templateMonomorphCompatibilitySoaHelperPrefix(leadingSlash);
  if (path.rfind(compatibilityPrefix, 0) == 0) {
    helperNameOut = std::string(path.substr(compatibilityPrefix.size()));
    return true;
  }
  const std::string publicPrefix =
      templateMonomorphPublicSoaHelperPrefix(leadingSlash);
  if (path.rfind(publicPrefix, 0) == 0) {
    helperNameOut = std::string(path.substr(publicPrefix.size()));
    return true;
  }
  return false;
}

bool isTemplateMonomorphMapImportAlias(std::string_view name) {
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedName = std::string(trimLeadingSlash(name));
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (normalizedName == trimLeadingSlash(alias)) {
      return true;
    }
  }
  return false;
}

bool isTemplateMonomorphMapConstructorCallPath(std::string_view path) {
  const primec::StdlibSurfaceMetadata *metadata =
      keyValueConstructorSurfaceMetadataLocal();
  if (metadata == nullptr) {
    return false;
  }
  const std::string normalizedPath =
      stripCollectionConstructorSuffixes(std::string(path));
  std::string memberName;
  if (resolveKeyValueConstructorMemberPath(normalizedPath, memberName) &&
      memberName == "map") {
    return true;
  }
  const std::string unrootedPath = std::string(trimLeadingSlash(normalizedPath));
  for (std::string_view alias : metadata->importAliasSpellings) {
    if (unrootedPath == trimLeadingSlash(alias)) {
      return true;
    }
  }
  return false;
}

bool isTemplateMonomorphMapEntryConstructorPath(std::string path) {
  const size_t generatedSuffix = path.find("__");
  if (generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return metadataBackedKeyValueHelperMethodName(path) == "entry" ||
         isExperimentalCollectionConstructorPathLocal(path, "map", "entry");
}

std::string normalizeBuiltinCollectionTemplateBase(const std::string &name) {
  if (name == "array" || name == "/array") {
    return "array";
  }
  if (name == "vector" || name == "/vector" ||
      trimLeadingSlash(name) ==
          trimLeadingSlash(canonicalVectorCompatibilityPrefixOrFallback())) {
    return "vector";
  }
  if (name == "soa" || name == "/soa" ||
      name == publicSoaHelperTargetPath("") ||
      name == trimLeadingSlash(publicSoaHelperTargetPath(""))) {
    return templateMonomorphSoaReceiverTypeName();
  }
  if (isTemplateMonomorphMapImportAlias(name)) {
    return "map";
  }
  return {};
}

bool isBuiltinCollectionTemplateBase(const std::string &name, size_t argumentCount) {
  const std::string normalized = normalizeBuiltinCollectionTemplateBase(name);
  if (normalized.empty()) {
    return false;
  }
  if ((normalized == "array" || normalized == "vector" ||
       isTemplateMonomorphSoaReceiverType(normalized)) &&
      argumentCount == 1) {
    return true;
  }
  return normalized == "map" && argumentCount == 2;
}

bool importPathCoversTarget(const std::string &importPath, const std::string &targetPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
  }
  if (importPath == targetPath) {
    return true;
  }
  if (importPath == canonicalVectorCompatibilityPrefixOrFallback() ||
      isTemplateMonomorphMapImportAlias(importPath) ||
      importPath == publicSoaHelperTargetPath("")) {
    return targetPath.rfind(importPath + "/", 0) == 0;
  }
  if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
}

std::string helperOverloadInternalPath(const std::string &publicPath, size_t parameterCount) {
  return publicPath + "__ov" + std::to_string(parameterCount);
}

std::string genericTypeOverloadInternalPath(const std::string &publicPath,
                                            size_t templateParameterCount) {
  return publicPath + "__arity" + std::to_string(templateParameterCount);
}

std::string helperOverloadDisplayPath(const std::string &path, const Context &ctx) {
  auto directIt = ctx.helperOverloadInternalToPublic.find(path);
  if (directIt != ctx.helperOverloadInternalToPublic.end()) {
    return directIt->second;
  }
  auto typeDirectIt = ctx.genericTypeOverloadInternalToPublic.find(path);
  if (typeDirectIt != ctx.genericTypeOverloadInternalToPublic.end()) {
    return typeDirectIt->second;
  }
  for (const auto &[internalPath, publicPath] : ctx.helperOverloadInternalToPublic) {
    if (path.rfind(internalPath + "__t", 0) == 0) {
      return publicPath;
    }
  }
  for (const auto &[internalPath, publicPath] : ctx.genericTypeOverloadInternalToPublic) {
    if (path.rfind(internalPath + "__t", 0) == 0) {
      return publicPath;
    }
  }
  return path;
}

bool selectGenericTypeOverloadPath(const std::string &resolvedPath,
                                   size_t templateArgumentCount,
                                   const Context &ctx,
                                   std::string &pathOut,
                                   std::string &error) {
  pathOut = resolvedPath;
  auto familyIt = ctx.genericTypeOverloads.find(resolvedPath);
  if (familyIt == ctx.genericTypeOverloads.end()) {
    return true;
  }
  for (const auto &entry : familyIt->second) {
    if (entry.templateParameterCount == templateArgumentCount) {
      pathOut = entry.internalPath;
      return true;
    }
  }
  std::ostringstream expected;
  bool firstExpected = true;
  for (const auto &entry : familyIt->second) {
    if (!firstExpected) {
      expected << " or ";
    }
    firstExpected = false;
    expected << entry.templateParameterCount;
  }
  error = "template argument count mismatch for " + resolvedPath +
          ": expected " + expected.str() + ", got " +
          std::to_string(templateArgumentCount);
  return false;
}

std::string selectHelperOverloadPath(const Expr &expr, const std::string &resolvedPath, const Context &ctx) {
  auto familyIt = ctx.helperOverloads.find(resolvedPath);
  if (familyIt == ctx.helperOverloads.end()) {
    return resolvedPath;
  }
  auto explicitCallPath = [](const Expr &callExpr) -> std::string {
    if (callExpr.kind != Expr::Kind::Call || callExpr.isMethodCall || callExpr.name.empty()) {
      return {};
    }
    if (callExpr.name.front() == '/') {
      return callExpr.name;
    }
    std::string prefix = callExpr.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    if (prefix.empty()) {
      return "/" + callExpr.name;
    }
    return prefix + "/" + callExpr.name;
  };
  auto isKeyValueEntryConstructorExpr = [&](const Expr &argExpr) {
    if (argExpr.kind != Expr::Kind::Call || argExpr.isMethodCall) {
      return false;
    }
    return isTemplateMonomorphMapEntryConstructorPath(explicitCallPath(argExpr));
  };
  const bool preferKeyValueEntryArgsPackOverload =
      !expr.isMethodCall &&
      isTemplateMonomorphMapConstructorCallPath(resolvedPath) &&
      !expr.args.empty() &&
      ([&]() {
        for (const Expr &argExpr : expr.args) {
          if (!isKeyValueEntryConstructorExpr(argExpr)) {
            return false;
          }
        }
        return true;
      })();
  if (preferKeyValueEntryArgsPackOverload) {
    for (const auto &entry : familyIt->second) {
      if (entry.parameterCount == 1) {
        return entry.internalPath;
      }
    }
  }
  std::vector<size_t> candidateCounts;
  if (expr.isMethodCall) {
    // Most method-call paths still carry the receiver explicitly in expr.args,
    // but some monomorphized internal helper paths have already peeled it
    // away. Prefer the raw call shape first and fall back to the self-counted
    // arity for those stripped-receiver paths.
    candidateCounts.push_back(expr.args.size());
    candidateCounts.push_back(expr.args.size() + 1);
  } else {
    candidateCounts.push_back(expr.args.size());
  }
  for (size_t candidateCount : candidateCounts) {
    for (const auto &entry : familyIt->second) {
      if (entry.parameterCount == candidateCount) {
        return entry.internalPath;
      }
    }
  }
  for (size_t candidateCount : candidateCounts) {
    for (const auto &entry : familyIt->second) {
      if (entry.isVariadic && candidateCount >= entry.variadicMinArgumentCount) {
        return entry.internalPath;
      }
    }
  }
  return resolvedPath;
}

bool resolveHelperOverloadDefinitionIdentity(const Definition &def,
                                             const Context &ctx,
                                             std::string &internalPathOut,
                                             std::string &nameOut) {
  internalPathOut = def.fullPath;
  nameOut = def.name;
  auto familyIt = ctx.helperOverloads.find(def.fullPath);
  if (familyIt == ctx.helperOverloads.end()) {
    return false;
  }
  const size_t parameterCount = def.parameters.size();
  for (const auto &entry : familyIt->second) {
    if (entry.parameterCount != parameterCount) {
      continue;
    }
    internalPathOut = entry.internalPath;
    const size_t slash = internalPathOut.find_last_of('/');
    nameOut = slash == std::string::npos ? internalPathOut : internalPathOut.substr(slash + 1);
    return true;
  }
  return false;
}

bool resolveGenericTypeOverloadDefinitionIdentity(const Definition &def,
                                                  const Context &ctx,
                                                  std::string &internalPathOut,
                                                  std::string &nameOut) {
  internalPathOut = def.fullPath;
  nameOut = def.name;
  auto familyIt = ctx.genericTypeOverloads.find(def.fullPath);
  if (familyIt == ctx.genericTypeOverloads.end()) {
    return false;
  }
  const size_t templateParameterCount = def.templateArgs.size();
  for (const auto &entry : familyIt->second) {
    if (entry.templateParameterCount != templateParameterCount) {
      continue;
    }
    internalPathOut = entry.internalPath;
    const size_t slash = internalPathOut.find_last_of('/');
    nameOut = slash == std::string::npos ? internalPathOut : internalPathOut.substr(slash + 1);
    return true;
  }
  return false;
}

std::string trimWhitespace(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

std::string stripWhitespace(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      out.push_back(c);
    }
  }
  return out;
}
