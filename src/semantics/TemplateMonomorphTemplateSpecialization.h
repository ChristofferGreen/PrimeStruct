#pragma once

std::vector<Definition> collectTemplateSpecializationFamily(const std::string &basePath, const Context &ctx) {
  std::vector<Definition> family;
  family.reserve(ctx.sourceDefs.size());
  for (const auto &entry : ctx.sourceDefs) {
    if (isPathPrefix(basePath, entry.first)) {
      family.push_back(entry.second);
    }
  }
  return family;
}

bool canReplaceGeneratedTemplateShell(const Definition &existingDef, const Definition &clone) {
  if (existingDef.templateArgs.empty()) {
    return false;
  }
  const size_t slash = clone.fullPath.find_last_of('/');
  if (slash == std::string::npos) {
    return false;
  }
  return isGeneratedTemplateSpecializationPath(std::string_view(clone.fullPath).substr(0, slash));
}

std::string parentPathForDefinition(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return {};
  }
  return path.substr(0, slash);
}

std::vector<TemplateRootInfo> collectNestedTemplateRoots(const std::string &basePath,
                                                         const std::vector<Definition> &family) {
  std::vector<TemplateRootInfo> nestedTemplates;
  nestedTemplates.reserve(family.size());
  for (const auto &def : family) {
    if (def.fullPath == basePath) {
      continue;
    }
    if (!def.templateArgs.empty()) {
      nestedTemplates.push_back({def.fullPath, def.templateArgs});
    }
  }
  return nestedTemplates;
}

std::optional<size_t> finalTypePackParameterIndex(const Definition &def) {
  for (size_t i = 0; i < def.templateArgIsPack.size(); ++i) {
    if (def.templateArgIsPack[i]) {
      return i;
    }
  }
  return std::nullopt;
}

bool definitionAllowsEmptyTypePackSpecialization(const Definition &def) {
  const std::optional<size_t> packIndex = finalTypePackParameterIndex(def);
  return packIndex.has_value() && *packIndex == 0;
}

bool bindTemplateArguments(const Definition &baseDef,
                           const std::vector<std::string> &resolvedArgs,
                           const std::string &displayPath,
                           TemplateArgumentBinding &bindingOut,
                           std::string &error) {
  bindingOut = {};
  const std::optional<size_t> packIndex = finalTypePackParameterIndex(baseDef);
  if (!packIndex.has_value()) {
    if (baseDef.templateArgs.size() != resolvedArgs.size()) {
      std::ostringstream out;
      out << "template argument count mismatch for " << displayPath
          << ": expected " << baseDef.templateArgs.size()
          << ", got " << resolvedArgs.size();
      error = out.str();
      return false;
    }
    bindingOut.mapping.reserve(baseDef.templateArgs.size());
    for (size_t i = 0; i < baseDef.templateArgs.size(); ++i) {
      bindingOut.mapping.emplace(baseDef.templateArgs[i], resolvedArgs[i]);
    }
    return true;
  }

  const size_t ordinaryParameterCount = *packIndex;
  if (resolvedArgs.size() < ordinaryParameterCount) {
    std::ostringstream out;
    out << "template argument count mismatch for " << displayPath
        << ": expected at least " << ordinaryParameterCount
        << ", got " << resolvedArgs.size();
    error = out.str();
    return false;
  }
  bindingOut.mapping.reserve(ordinaryParameterCount);
  for (size_t i = 0; i < ordinaryParameterCount; ++i) {
    bindingOut.mapping.emplace(baseDef.templateArgs[i], resolvedArgs[i]);
  }
  TemplatePackBinding packBinding;
  packBinding.parameterName = baseDef.templateArgs[*packIndex];
  packBinding.arguments.assign(resolvedArgs.begin() +
                                   static_cast<std::ptrdiff_t>(ordinaryParameterCount),
                               resolvedArgs.end());
  bindingOut.packBindings.push_back(std::move(packBinding));
  return true;
}

std::unordered_set<std::string> collectShadowedTemplateParams(const std::string &definitionPath,
                                                              const std::vector<TemplateRootInfo> &nestedTemplates) {
  std::unordered_set<std::string> shadowedParams;
  for (const auto &nested : nestedTemplates) {
    if (isPathPrefix(nested.fullPath, definitionPath)) {
      for (const auto &param : nested.params) {
        shadowedParams.insert(param);
      }
    }
  }
  return shadowedParams;
}

bool isUnderNestedTemplateRoot(const std::string &basePath,
                               const std::string &definitionPath,
                               const std::vector<TemplateRootInfo> &nestedTemplates) {
  if (definitionPath == basePath) {
    return false;
  }
  for (const auto &nested : nestedTemplates) {
    if (isPathPrefix(nested.fullPath, definitionPath)) {
      return true;
    }
  }
  return false;
}

bool specializeTemplateDefinitionFamily(const std::string &basePath,
                                        const TemplateArgumentBinding &templateBinding,
                                        const std::string &specializedBasePath,
                                        const std::string &specializedName,
                                        const std::string &cacheKey,
                                        Context &ctx,
                                        std::string &error) {
  const std::vector<Definition> family = collectTemplateSpecializationFamily(basePath, ctx);
  const std::vector<TemplateRootInfo> nestedTemplates = collectNestedTemplateRoots(basePath, family);
  std::vector<Definition> clones;
  clones.reserve(family.size());
  std::vector<bool> cloneOutputs;
  cloneOutputs.reserve(family.size());
  std::unordered_set<std::string> clonePaths;
  clonePaths.reserve(family.size());
  std::vector<bool> replaceExistingShells;
  replaceExistingShells.reserve(family.size());

  for (const auto &def : family) {
    const bool shouldOutputClone =
        !isUnderNestedTemplateRoot(basePath, def.fullPath, nestedTemplates);
    Definition clone = def;
    clone.fullPath = replacePathPrefix(def.fullPath, basePath, specializedBasePath);
    clone.namespacePrefix = replacePathPrefix(def.namespacePrefix, basePath, specializedBasePath);
    if (def.fullPath == basePath) {
      clone.name = specializedName;
      clone.namespacePrefix = parentPathForDefinition(specializedBasePath);
      clone.templateArgs.clear();
      clone.templateArgIsPack.clear();
    }
    if (!templateBinding.packBindings.empty() && shouldOutputClone) {
      clone.templatePackBindings = templateBinding.packBindings;
    }
    if (!clonePaths.insert(clone.fullPath).second) {
      error = "template specialization conflicts with existing definition: " + clone.fullPath;
      ctx.specializationCache.erase(cacheKey);
      return false;
    }
    const auto existingIt = ctx.sourceDefs.find(clone.fullPath);
    const bool replaceExistingShell = existingIt != ctx.sourceDefs.end() &&
                                      canReplaceGeneratedTemplateShell(existingIt->second, clone);
    if (existingIt != ctx.sourceDefs.end() && !replaceExistingShell) {
      error = "template specialization conflicts with existing definition: " + clone.fullPath;
      ctx.specializationCache.erase(cacheKey);
      return false;
    }

    const std::unordered_set<std::string> shadowedParams =
        collectShadowedTemplateParams(def.fullPath, nestedTemplates);
    SubstMap mapping = templateBinding.mapping;
    for (const auto &param : shadowedParams) {
      mapping.erase(param);
    }
    if (!rewriteDefinition(clone, mapping, shadowedParams, ctx, error)) {
      ctx.specializationCache.erase(cacheKey);
      return false;
    }
    clones.push_back(std::move(clone));
    cloneOutputs.push_back(shouldOutputClone);
    replaceExistingShells.push_back(replaceExistingShell);
  }

  for (size_t cloneIndex = 0; cloneIndex < clones.size(); ++cloneIndex) {
    const Definition &clone = clones[cloneIndex];
    if (replaceExistingShells[cloneIndex]) {
      ctx.sourceDefs[clone.fullPath] = clone;
    } else {
      ctx.sourceDefs.emplace(clone.fullPath, clone);
    }
    if (!clone.templateArgs.empty()) {
      ctx.templateDefs.insert(clone.fullPath);
    } else {
      ctx.templateDefs.erase(clone.fullPath);
    }

    if (cloneOutputs[cloneIndex] && clone.templateArgs.empty()) {
      if (ctx.outputPaths.insert(clone.fullPath).second) {
        ctx.outputDefs.push_back(clone);
      }
    }
  }

  return true;
}
