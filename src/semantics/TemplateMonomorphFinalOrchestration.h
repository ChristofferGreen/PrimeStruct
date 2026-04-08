#pragma once

Context makeTemplateMonomorphContext(Program &program) {
  return Context(program);
}

void buildImportAliases(Context &ctx) {
  ctx.importAliases.clear();
  const auto &importPaths = ctx.program.imports;
  auto shouldSkipWildcardAlias = [](const std::string &prefix, const std::string &remainder) {
    return prefix == "/std/collections" && (remainder == "vector" || remainder == "map");
  };
  for (const auto &importPath : importPaths) {
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
        ctx.importAliases.emplace(remainder, publicPath);
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
        ctx.importAliases.emplace(remainder, path);
      }
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
    ctx.importAliases.emplace(remainder, importPath);
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
