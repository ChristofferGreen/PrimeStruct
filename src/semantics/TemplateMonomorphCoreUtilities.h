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
  return name == "array" || name == "vector" || name == "soa_vector" || name == "map" || name == "Result" ||
         name == "File" || isBuiltinTemplateTypeName(name);
}

std::string normalizeBuiltinCollectionTemplateBase(const std::string &name) {
  if (name == "array" || name == "/array") {
    return "array";
  }
  if (name == "vector" || name == "/vector" || name == "std/collections/vector" ||
      name == "/std/collections/vector") {
    return "vector";
  }
  if (name == "soa_vector" || name == "/soa_vector") {
    return "soa_vector";
  }
  if (name == "map" || name == "/map" || name == "std/collections/map" || name == "/std/collections/map") {
    return "map";
  }
  return {};
}

bool isBuiltinCollectionTemplateBase(const std::string &name, size_t argumentCount) {
  const std::string normalized = normalizeBuiltinCollectionTemplateBase(name);
  if (normalized.empty()) {
    return false;
  }
  if ((normalized == "array" || normalized == "vector" || normalized == "soa_vector") && argumentCount == 1) {
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
  if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
}

std::string helperOverloadInternalPath(const std::string &publicPath, size_t parameterCount) {
  return publicPath + "__ov" + std::to_string(parameterCount);
}

std::string helperOverloadDisplayPath(const std::string &path, const Context &ctx) {
  auto directIt = ctx.helperOverloadInternalToPublic.find(path);
  if (directIt != ctx.helperOverloadInternalToPublic.end()) {
    return directIt->second;
  }
  for (const auto &[internalPath, publicPath] : ctx.helperOverloadInternalToPublic) {
    if (path.rfind(internalPath + "__t", 0) == 0) {
      return publicPath;
    }
  }
  return path;
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
  auto isMapEntryConstructorPath = [](const std::string &path) {
    return path == "/std/collections/map/entry" ||
           path.rfind("/std/collections/map/entry__", 0) == 0 ||
           path == "/map/entry" ||
           path.rfind("/map/entry__", 0) == 0 ||
           path == "/std/collections/experimental_map/entry" ||
           path.rfind("/std/collections/experimental_map/entry__", 0) == 0;
  };
  auto isMapEntryConstructorExpr = [&](const Expr &argExpr) {
    if (argExpr.kind != Expr::Kind::Call || argExpr.isMethodCall) {
      return false;
    }
    return isMapEntryConstructorPath(explicitCallPath(argExpr));
  };
  const bool preferMapEntryArgsPackOverload =
      !expr.isMethodCall &&
      (resolvedPath == "/std/collections/map/map" || resolvedPath == "/map") &&
      !expr.args.empty() &&
      ([&]() {
        for (const Expr &argExpr : expr.args) {
          if (!isMapEntryConstructorExpr(argExpr)) {
            return false;
          }
        }
        return true;
      })();
  if (preferMapEntryArgsPackOverload) {
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
