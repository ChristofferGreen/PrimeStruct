#pragma once

bool validateTemplateParameterMetadataForTemplateSetup(const Definition &def,
                                                       std::string &error) {
  if (!def.templateArgIsPack.empty() &&
      def.templateArgIsPack.size() != def.templateArgs.size()) {
    error = "template parameter metadata mismatch on " + def.fullPath;
    return false;
  }
  std::unordered_set<std::string> seen;
  bool sawPack = false;
  for (std::size_t index = 0; index < def.templateArgs.size(); ++index) {
    const std::string &name = def.templateArgs[index];
    if (!seen.insert(name).second) {
      error = "duplicate template parameter: " + name;
      return false;
    }
    const bool isPack = index < def.templateArgIsPack.size() &&
                        def.templateArgIsPack[index];
    if (isPack) {
      if (sawPack) {
        error = "only one type pack parameter is supported";
        return false;
      }
      sawPack = true;
    } else if (sawPack) {
      error = "type pack parameter must be last";
      return false;
    }
  }
  return true;
}

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
    if (!validateTemplateParameterMetadataForTemplateSetup(def, error)) {
      return false;
    }
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
      std::vector<GenericTypeOverloadEntry> overloads;
      overloads.reserve(family.size());
      bool allowGenericTypeOverloadFamily = true;
      for (const Definition *def : family) {
        if (!seenTemplateCounts.insert(def->templateArgs.size()).second) {
          allowGenericTypeOverloadFamily = false;
          break;
        }
        const std::string internalPath =
            genericTypeOverloadInternalPath(publicPath, def->templateArgs.size());
        if (occupiedPaths.count(internalPath) > 0) {
          error = "generic type overload internal path conflicts with existing definition: " +
                  internalPath;
          return false;
        }
        overloads.push_back({internalPath, def->templateArgs.size()});
      }
      if (!allowGenericTypeOverloadFamily) {
        error = "duplicate definition: " + publicPath;
        return false;
      }
      std::stable_sort(overloads.begin(),
                       overloads.end(),
                       [](const GenericTypeOverloadEntry &left,
                          const GenericTypeOverloadEntry &right) {
                         return left.templateParameterCount <
                                right.templateParameterCount;
                       });
      ctx.genericTypeOverloads.emplace(publicPath, overloads);
      for (const Definition *def : family) {
        std::string internalPath;
        std::string internalName;
        if (!resolveGenericTypeOverloadDefinitionIdentity(
                *def, ctx, internalPath, internalName)) {
          error = "duplicate definition: " + publicPath;
          return false;
        }
        Definition clone = *def;
        clone.fullPath = internalPath;
        clone.name = internalName;
        ctx.sourceDefs.emplace(clone.fullPath, clone);
        ctx.genericTypeOverloadInternalToPublic.emplace(clone.fullPath,
                                                        publicPath);
        if (!clone.templateArgs.empty()) {
          ctx.templateDefs.insert(clone.fullPath);
          templateRoots.insert(clone.fullPath);
        }
      }
      continue;
    }

    bool allowHelperOverloadFamily = true;
    std::unordered_map<size_t, std::size_t> parameterCountFrequencies;
    std::unordered_map<size_t, std::size_t> parameterCountOrdinals;
    std::vector<HelperOverloadEntry> overloads;
    overloads.reserve(family.size());
    for (const Definition *def : family) {
      if (isStructDefinition(*def)) {
        allowHelperOverloadFamily = false;
        break;
      }
      ++parameterCountFrequencies[def->parameters.size()];
    }
    for (const auto &[parameterCount, frequency] : parameterCountFrequencies) {
      if (frequency <= 1) {
        continue;
      }
      bool hasRequirementConstrainedCandidate = false;
      for (const Definition *def : family) {
        if (def->parameters.size() == parameterCount &&
            definitionHasRequireTransform(*def)) {
          hasRequirementConstrainedCandidate = true;
          break;
        }
      }
      if (!hasRequirementConstrainedCandidate) {
        allowHelperOverloadFamily = false;
        break;
      }
    }
    if (!allowHelperOverloadFamily) {
      error = "duplicate definition: " + publicPath;
      return false;
    }
    for (const Definition *def : family) {
      const size_t parameterCount = def->parameters.size();
      const std::size_t ordinal = parameterCountOrdinals[parameterCount]++;
      std::string internalPath = helperOverloadInternalPath(publicPath, parameterCount);
      if (parameterCountFrequencies[parameterCount] > 1) {
        internalPath += "_" + std::to_string(ordinal);
      }
      if (occupiedPaths.count(internalPath) > 0) {
        error = "helper overload internal path conflicts with existing definition: " + internalPath;
        return false;
      }
      const bool isVariadic = definitionHasVariadicParameter(*def);
      overloads.push_back({internalPath,
                           helperOverloadDefinitionKey(*def),
                           parameterCount,
                           isVariadic && parameterCount > 0 ? parameterCount - 1 : parameterCount,
                           isVariadic,
                           definitionHasRequireTransform(*def)});
    }
    std::stable_sort(overloads.begin(),
                     overloads.end(),
                     [](const HelperOverloadEntry &left, const HelperOverloadEntry &right) {
                       if (left.parameterCount != right.parameterCount) {
                         return left.parameterCount < right.parameterCount;
                       }
                       return left.internalPath < right.internalPath;
                     });
    ctx.helperOverloads.emplace(publicPath, overloads);
    for (const auto &entry : overloads) {
      ctx.helperOverloadDefinitionIdentity.emplace(entry.sourceKey,
                                                   entry.internalPath);
    }
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
