#pragma once

Context makeTemplateMonomorphContext(Program &program) {
  return Context(program);
}

void buildImportAliases(Context &ctx) {
  ctx.directImportAliases.clear();
  ctx.transitiveImportAliases.clear();
  ctx.stdlibScopedImportAliases.clear();
  ctx.importAliases.clear();
  const auto &directImportPaths = ctx.program.sourceImports.empty()
                                      ? ctx.program.imports
                                      : ctx.program.sourceImports;
  auto stdlibSurfaceImportAliasPriority = [](const StdlibSurfaceMetadata &metadata) {
    switch (metadata.shape) {
      case StdlibSurfaceShape::ConstructorFamily:
        return 30;
      case StdlibSurfaceShape::ErrorFamily:
        return 20;
      case StdlibSurfaceShape::HelperFamily:
        return 10;
    }
    return 0;
  };
  auto stdlibSurfaceMatchesImportAliasPath = [](const StdlibSurfaceMetadata &metadata,
                                                const std::string_view importPath) {
    return metadata.canonicalPath == importPath ||
           std::find(metadata.importAliasSpellings.begin(),
                     metadata.importAliasSpellings.end(),
                     importPath) != metadata.importAliasSpellings.end();
  };
  auto findStdlibSurfaceImportAliasMetadata =
      [&](const std::string_view importPath) -> const StdlibSurfaceMetadata * {
    const StdlibSurfaceMetadata *bestMatch = nullptr;
    int bestPriority = -1;
    for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
      if (!stdlibSurfaceMatchesImportAliasPath(metadata, importPath)) {
        continue;
      }
      const int priority = stdlibSurfaceImportAliasPriority(metadata);
      if (bestMatch == nullptr || priority > bestPriority) {
        bestMatch = &metadata;
        bestPriority = priority;
      }
    }
    return bestMatch;
  };
  auto findStdlibSurfaceWildcardAliasMetadata =
      [&](const std::string_view importRoot,
          const std::string_view aliasName) -> const StdlibSurfaceMetadata * {
    const StdlibSurfaceMetadata *bestMatch = nullptr;
    int bestPriority = -1;
    for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
      if (metadata.canonicalImportRoot != importRoot) {
        continue;
      }
      if (std::find(metadata.importAliasSpellings.begin(),
                    metadata.importAliasSpellings.end(),
                    aliasName) == metadata.importAliasSpellings.end()) {
        continue;
      }
      const int priority = stdlibSurfaceImportAliasPriority(metadata);
      if (bestMatch == nullptr || priority > bestPriority) {
        bestMatch = &metadata;
        bestPriority = priority;
      }
    }
    return bestMatch;
  };
  auto shouldSkipWildcardAlias = [](const std::string &prefix, const std::string &remainder) {
    return prefix == "/std/collections" && (remainder == "vector" || remainder == "map");
  };
  auto registerAlias = [&](std::unordered_map<std::string, std::string> &targetAliases,
                           const std::string &aliasName,
                           const std::string &targetPath) {
    targetAliases.emplace(aliasName, targetPath);
    if (!(targetPath.rfind("/std/collections/soa_vector/", 0) == 0 &&
          (aliasName == "count" || aliasName == "get" || aliasName == "ref" ||
           aliasName == "count_ref" || aliasName == "get_ref" ||
           aliasName == "ref_ref" || aliasName == "reserve" ||
           aliasName == "push" || aliasName == "to_aos" ||
           aliasName == "to_aos_ref"))) {
      ctx.importAliases.emplace(aliasName, targetPath);
    }
  };
  auto registerDefinitionAlias = [&](std::unordered_map<std::string, std::string> &targetAliases,
                                     const std::string &aliasName,
                                     const std::string &targetPath) {
    targetAliases[aliasName] = targetPath;
    if (!(targetPath.rfind("/std/collections/soa_vector/", 0) == 0 &&
          (aliasName == "count" || aliasName == "get" || aliasName == "ref" ||
           aliasName == "count_ref" || aliasName == "get_ref" ||
           aliasName == "ref_ref" || aliasName == "reserve" ||
           aliasName == "push" || aliasName == "to_aos" ||
           aliasName == "to_aos_ref"))) {
      ctx.importAliases[aliasName] = targetPath;
    }
  };
  auto registerStdlibSurfaceExactAlias =
      [&](std::unordered_map<std::string, std::string> &targetAliases,
          const std::string &importPath) {
    const StdlibSurfaceMetadata *metadata =
        findStdlibSurfaceImportAliasMetadata(importPath);
    if (metadata == nullptr) {
      return false;
    }
    const size_t slash = importPath.find_last_of('/');
    const std::string aliasName =
        slash == std::string::npos ? importPath : importPath.substr(slash + 1);
    if (aliasName.empty()) {
      return false;
    }
    registerDefinitionAlias(
        targetAliases, aliasName, std::string(metadata->canonicalPath));
    return true;
  };
  auto registerStdlibSurfaceWildcardAliases =
      [&](std::unordered_map<std::string, std::string> &targetAliases,
          const std::string &prefix) {
    for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
      if (metadata.canonicalImportRoot != prefix) {
        continue;
      }
      for (const std::string_view spelling : metadata.importAliasSpellings) {
        if (spelling.empty() || spelling.front() == '/') {
          continue;
        }
        const StdlibSurfaceMetadata *preferred =
            findStdlibSurfaceWildcardAliasMetadata(prefix, spelling);
        if (preferred != &metadata) {
          continue;
        }
        registerDefinitionAlias(
            targetAliases, std::string(spelling),
            std::string(preferred->canonicalPath));
      }
    }
  };
  for (const auto &importPath : directImportPaths) {
    if (importPath.empty() || importPath[0] != '/') {
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      registerStdlibSurfaceWildcardAliases(ctx.directImportAliases, prefix);
      const std::string scopedPrefix = prefix + "/";
      for (const auto &[publicPath, overloads] : ctx.helperOverloads) {
        (void)overloads;
        if (publicPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = publicPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (shouldSkipWildcardAlias(prefix, remainder)) {
          continue;
        }
        registerAlias(ctx.directImportAliases, remainder, publicPath);
      }
      for (const auto &entry : ctx.sourceDefs) {
        const std::string &path = entry.first;
        if (ctx.helperOverloadInternalToPublic.count(path) > 0) {
          continue;
        }
        if (path.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = path.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (shouldSkipWildcardAlias(prefix, remainder)) {
          continue;
        }
        registerDefinitionAlias(ctx.directImportAliases, remainder, path);
      }
      continue;
    }
    if (registerStdlibSurfaceExactAlias(ctx.directImportAliases, importPath)) {
      continue;
    }
    auto defIt = ctx.sourceDefs.find(importPath);
    if (defIt == ctx.sourceDefs.end()) {
      if (ctx.helperOverloads.count(importPath) == 0) {
        continue;
      }
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (defIt != ctx.sourceDefs.end()) {
      registerDefinitionAlias(ctx.directImportAliases, remainder, importPath);
    } else {
      registerAlias(ctx.directImportAliases, remainder, importPath);
    }
  }

  if (ctx.program.sourceImports.empty()) {
    return;
  }

  std::unordered_set<std::string> directImportSet(directImportPaths.begin(), directImportPaths.end());
  for (const auto &importPath : ctx.program.imports) {
    if (directImportSet.count(importPath) != 0 || importPath.empty() || importPath[0] != '/') {
      continue;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      registerStdlibSurfaceWildcardAliases(ctx.transitiveImportAliases, prefix);
      const std::string scopedPrefix = prefix + "/";
      for (const auto &[publicPath, overloads] : ctx.helperOverloads) {
        (void)overloads;
        if (publicPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = publicPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (shouldSkipWildcardAlias(prefix, remainder)) {
          continue;
        }
        registerAlias(ctx.transitiveImportAliases, remainder, publicPath);
      }
      for (const auto &entry : ctx.sourceDefs) {
        const std::string &path = entry.first;
        if (ctx.helperOverloadInternalToPublic.count(path) > 0) {
          continue;
        }
        if (path.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = path.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        if (shouldSkipWildcardAlias(prefix, remainder)) {
          continue;
        }
        registerDefinitionAlias(ctx.transitiveImportAliases, remainder, path);
      }
      continue;
    }
    if (registerStdlibSurfaceExactAlias(ctx.transitiveImportAliases, importPath)) {
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (ctx.sourceDefs.count(importPath) > 0) {
      registerDefinitionAlias(ctx.transitiveImportAliases, remainder, importPath);
    } else {
      registerAlias(ctx.transitiveImportAliases, remainder, importPath);
    }
  }

  ctx.stdlibScopedImportAliases = ctx.transitiveImportAliases;
  for (const auto &[aliasName, targetPath] : ctx.directImportAliases) {
    ctx.stdlibScopedImportAliases[aliasName] = targetPath;
  }
}

bool isPathUnderTemplateRoot(const std::string &path, const std::unordered_set<std::string> &templateRoots) {
  for (const auto &root : templateRoots) {
    if (isPathPrefix(root, path)) {
      return true;
    }
  }
  return false;
}

bool rewriteMonomorphizedDefinitions(Context &ctx,
                                     const std::unordered_set<std::string> &templateRoots,
                                     std::string &error) {
  for (const auto &def : ctx.program.definitions) {
    Definition clone = def;
    std::string overloadInternalPath;
    std::string overloadName;
    if (resolveHelperOverloadDefinitionIdentity(def, ctx, overloadInternalPath, overloadName)) {
      clone.fullPath = std::move(overloadInternalPath);
      clone.name = std::move(overloadName);
    }
    if (isPathUnderTemplateRoot(clone.fullPath, templateRoots)) {
      continue;
    }
    if (!rewriteDefinition(clone, SubstMap{}, {}, ctx, error)) {
      return false;
    }
    if (ctx.outputPaths.insert(clone.fullPath).second) {
      ctx.outputDefs.push_back(std::move(clone));
    }
  }
  return true;
}

bool rewriteMonomorphizedExecutions(Context &ctx, std::string &error) {
  ctx.outputExecs.reserve(ctx.program.executions.size());
  for (const auto &exec : ctx.program.executions) {
    Execution clone = exec;
    if (!rewriteExecution(clone, ctx, error)) {
      return false;
    }
    ctx.outputExecs.push_back(std::move(clone));
  }
  return true;
}
