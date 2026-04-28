#pragma once

bool initializeTemplateMonomorphSourceDefinitions(Context &ctx,
                                                  const std::string &entryPath,
                                                  std::unordered_set<std::string> &templateRoots,
                                                  std::string &error) {
  templateRoots.clear();
  std::unordered_map<std::string, std::vector<const Definition *>> definitionsByPath;
  definitionsByPath.reserve(ctx.program.definitions.size());
  std::unordered_set<std::string> occupiedPaths;
  occupiedPaths.reserve(ctx.program.definitions.size());
  for (const auto &def : ctx.program.definitions) {
    definitionsByPath[def.fullPath].push_back(&def);
    occupiedPaths.insert(def.fullPath);
  }
  for (const auto &[publicPath, family] : definitionsByPath) {
    if (family.size() == 1) {
      const Definition &def = *family.front();
      ctx.sourceDefs.emplace(def.fullPath, def);
      if (!def.templateArgs.empty()) {
        ctx.templateDefs.insert(def.fullPath);
        templateRoots.insert(def.fullPath);
      }
      continue;
    }

    auto isSumDefinitionForOverloadDiagnostic = [](const Definition &def) {
      for (const auto &transform : def.transforms) {
        if (transform.name == "sum") {
          return true;
        }
      }
      return false;
    };
    bool allSumDefinitions = true;
    for (const Definition *def : family) {
      if (def == nullptr || !isSumDefinitionForOverloadDiagnostic(*def)) {
        allSumDefinitions = false;
        break;
      }
    }
    if (allSumDefinitions) {
      std::unordered_set<size_t> seenTemplateCounts;
      bool differsOnlyByTemplateArity = true;
      for (const Definition *def : family) {
        if (!seenTemplateCounts.insert(def->templateArgs.size()).second) {
          differsOnlyByTemplateArity = false;
          break;
        }
      }
      if (differsOnlyByTemplateArity) {
        error = "sum overloads by template arity are not supported: " + publicPath;
      } else {
        error = "duplicate definition: " + publicPath;
      }
      return false;
    }

    bool allowHelperOverloadFamily = true;
    std::unordered_set<size_t> seenParameterCounts;
    std::vector<HelperOverloadEntry> overloads;
    overloads.reserve(family.size());
    for (const Definition *def : family) {
      if (isStructDefinition(*def)) {
        allowHelperOverloadFamily = false;
        break;
      }
      const size_t parameterCount = def->parameters.size();
      if (!seenParameterCounts.insert(parameterCount).second) {
        allowHelperOverloadFamily = false;
        break;
      }
      const std::string internalPath = helperOverloadInternalPath(publicPath, parameterCount);
      if (occupiedPaths.count(internalPath) > 0) {
        error = "helper overload internal path conflicts with existing definition: " + internalPath;
        return false;
      }
      overloads.push_back({internalPath, parameterCount});
    }
    if (!allowHelperOverloadFamily) {
      error = "duplicate definition: " + publicPath;
      return false;
    }
    std::stable_sort(overloads.begin(),
                     overloads.end(),
                     [](const HelperOverloadEntry &left, const HelperOverloadEntry &right) {
                       return left.parameterCount < right.parameterCount;
                     });
    ctx.helperOverloads.emplace(publicPath, overloads);
    for (const Definition *def : family) {
      std::string internalPath;
      std::string internalName;
      if (!resolveHelperOverloadDefinitionIdentity(*def, ctx, internalPath, internalName)) {
        error = "duplicate definition: " + publicPath;
        return false;
      }
      Definition clone = *def;
      clone.fullPath = internalPath;
      clone.name = internalName;
      ctx.sourceDefs.emplace(clone.fullPath, clone);
      ctx.helperOverloadInternalToPublic.emplace(clone.fullPath, publicPath);
      if (!clone.templateArgs.empty()) {
        ctx.templateDefs.insert(clone.fullPath);
        templateRoots.insert(clone.fullPath);
      }
    }
  }
  if (templateRoots.count(entryPath) > 0) {
    error = "entry definition cannot be templated: " + entryPath;
    return false;
  }
  return true;
}
