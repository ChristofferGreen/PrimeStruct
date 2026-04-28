#pragma once

#include "MapConstructorHelpers.h"

ResolvedType resolveTypeStringImpl(std::string input,
                                   const SubstMap &mapping,
                                   const std::unordered_set<std::string> &allowedParams,
                                   const std::string &namespacePrefix,
                                   Context &ctx,
                                   std::string &error,
                                   std::unordered_set<std::string> &substitutionStack) {
  ResolvedType result;
  std::string trimmed = trimWhitespace(input);
  if (trimmed.empty()) {
    result.text = input;
    result.concrete = true;
    return result;
  }
  if (allowedParams.count(trimmed) > 0) {
    result.text = trimmed;
    result.concrete = false;
    return result;
  }
  auto mapIt = mapping.find(trimmed);
  if (mapIt != mapping.end()) {
    const std::string mapped = trimWhitespace(mapIt->second);
    if (mapped == trimmed || !substitutionStack.insert(trimmed).second) {
      result.text = trimmed;
      result.concrete = !isEnclosingTemplateParamName(trimmed, namespacePrefix, ctx);
      return result;
    }
    ResolvedType resolved =
        resolveTypeStringImpl(mapIt->second, mapping, allowedParams, namespacePrefix, ctx, error, substitutionStack);
    substitutionStack.erase(trimmed);
    return resolved;
  }
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimmed, base, argText)) {
    if (isEnclosingTemplateParamName(trimmed, namespacePrefix, ctx)) {
      result.text = trimmed;
      result.concrete = false;
      return result;
    }
    std::string resolvedPath =
        resolveNameToPath(trimmed, namespacePrefix, scopedImportAliasesForNamespace(namespacePrefix, ctx), ctx.sourceDefs);
    if (ctx.templateDefs.count(resolvedPath) > 0) {
      error = "template arguments required for " + resolvedPath;
      result.text.clear();
      result.concrete = false;
      return result;
    }
    result.text = trimmed;
    result.concrete = true;
    return result;
  }
  if (allowedParams.count(base) > 0) {
    error = "template parameters cannot be templated: " + base;
    result.text.clear();
    result.concrete = false;
    return result;
  }
  std::vector<std::string> args;
  if (!splitTopLevelTemplateArgs(argText, args)) {
    error = "invalid template arguments for " + base;
    result.text.clear();
    result.concrete = false;
    return result;
  }
  std::vector<std::string> resolvedArgs;
  resolvedArgs.reserve(args.size());
  bool allConcrete = true;
  for (const auto &arg : args) {
    ResolvedType resolved =
        resolveTypeStringImpl(arg, mapping, allowedParams, namespacePrefix, ctx, error, substitutionStack);
    if (!error.empty()) {
      result.text.clear();
      result.concrete = false;
      return result;
    }
    if (!resolved.concrete) {
      allConcrete = false;
    }
    resolvedArgs.push_back(resolved.text);
  }
  const auto explicitTemplateArgFactKey = [&](const std::string &targetPath) {
    return targetPath + "<" + joinTemplateArgs(resolvedArgs) + ">";
  };
  const auto consumeExplicitTemplateArgFact = [&](const std::string &targetPath, ResolvedType &resolvedOut) {
    const std::string factKey = explicitTemplateArgFactKey(targetPath);
    const auto factIt = ctx.explicitTemplateArgInferenceFacts.find(factKey);
    if (factIt == ctx.explicitTemplateArgInferenceFacts.end()) {
      return false;
    }
    resolvedOut.text = factIt->second.resolvedTypeText;
    resolvedOut.concrete = factIt->second.resolvedConcrete;
    ++ctx.explicitTemplateArgInferenceFactHitsForTesting;
    return true;
  };
  const auto publishExplicitTemplateArgFact = [&](const std::string &targetPath, const ResolvedType &resolvedType) {
    ctx.explicitTemplateArgInferenceFacts[explicitTemplateArgFactKey(targetPath)] =
        ExplicitTemplateArgInferenceFact{resolvedType.text, resolvedType.concrete};
  };
  const auto recordExplicitTemplateArgFact = [&](const std::string &targetPath,
                                                 const ResolvedType &resolvedType) {
    if (!ctx.collectExplicitTemplateArgFactsForTesting) {
      return;
    }
    ctx.explicitTemplateArgFactsForTesting.push_back(
        ExplicitTemplateArgResolutionFactForTesting{
            namespacePrefix,
            targetPath,
            joinTemplateArgs(resolvedArgs),
            resolvedType.text,
            resolvedType.concrete,
        });
  };
  std::string overloadBasePath =
      resolveNameToPath(base, namespacePrefix, scopedImportAliasesForNamespace(namespacePrefix, ctx), ctx.sourceDefs);
  if (ctx.genericTypeOverloads.count(overloadBasePath) > 0) {
    if (!selectGenericTypeOverloadPath(overloadBasePath,
                                       resolvedArgs.size(),
                                       ctx,
                                       overloadBasePath,
                                       error)) {
      result.text.clear();
      result.concrete = false;
      return result;
    }
    if (!allConcrete) {
      result.text = base + "<" + joinTemplateArgs(resolvedArgs) + ">";
      result.concrete = false;
      recordExplicitTemplateArgFact(overloadBasePath, result);
      return result;
    }
    if (consumeExplicitTemplateArgFact(overloadBasePath, result)) {
      recordExplicitTemplateArgFact(overloadBasePath, result);
      return result;
    }
    std::string specializedPath;
    if (!instantiateTemplate(overloadBasePath, resolvedArgs, ctx, error, specializedPath)) {
      result.text.clear();
      result.concrete = false;
      return result;
    }
    result.text = specializedPath;
    result.concrete = true;
    publishExplicitTemplateArgFact(overloadBasePath, result);
    recordExplicitTemplateArgFact(overloadBasePath, result);
    return result;
  }
  if (isBuiltinTemplateContainer(base) || isBuiltinCollectionTemplateBase(base, resolvedArgs.size())) {
    const std::string normalizedBase = isBuiltinTemplateContainer(base)
                                           ? base
                                           : normalizeBuiltinCollectionTemplateBase(base);
    if (allConcrete && consumeExplicitTemplateArgFact(normalizedBase, result)) {
      recordExplicitTemplateArgFact(normalizedBase, result);
      return result;
    }
    result.text = normalizedBase + "<" + joinTemplateArgs(resolvedArgs) + ">";
    result.concrete = allConcrete;
    if (allConcrete) {
      publishExplicitTemplateArgFact(normalizedBase, result);
    }
    recordExplicitTemplateArgFact(normalizedBase, result);
    return result;
  }
  std::string resolvedBasePath =
      resolveNameToPath(base, namespacePrefix, scopedImportAliasesForNamespace(namespacePrefix, ctx), ctx.sourceDefs);
  if (!selectGenericTypeOverloadPath(resolvedBasePath,
                                     resolvedArgs.size(),
                                     ctx,
                                     resolvedBasePath,
                                     error)) {
    result.text.clear();
    result.concrete = false;
    return result;
  }
  if (ctx.templateDefs.count(resolvedBasePath) == 0) {
    error = "template arguments are only supported on templated definitions: " + resolvedBasePath;
    result.text.clear();
    result.concrete = false;
    return result;
  }
  if (!allConcrete) {
    result.text = base + "<" + joinTemplateArgs(resolvedArgs) + ">";
    result.concrete = false;
    recordExplicitTemplateArgFact(resolvedBasePath, result);
    return result;
  }
  if (consumeExplicitTemplateArgFact(resolvedBasePath, result)) {
    recordExplicitTemplateArgFact(resolvedBasePath, result);
    return result;
  }
  std::string specializedPath;
  if (!instantiateTemplate(resolvedBasePath, resolvedArgs, ctx, error, specializedPath)) {
    result.text.clear();
    result.concrete = false;
    return result;
  }
  result.text = specializedPath;
  result.concrete = true;
  publishExplicitTemplateArgFact(resolvedBasePath, result);
  recordExplicitTemplateArgFact(resolvedBasePath, result);
  return result;
}

ResolvedType resolveTypeString(std::string input,
                               const SubstMap &mapping,
                               const std::unordered_set<std::string> &allowedParams,
                               const std::string &namespacePrefix,
                               Context &ctx,
                               std::string &error) {
  std::unordered_set<std::string> substitutionStack;
  return resolveTypeStringImpl(input, mapping, allowedParams, namespacePrefix, ctx, error, substitutionStack);
}

bool rewriteTransforms(std::vector<Transform> &transforms,
                       const SubstMap &mapping,
                       const std::unordered_set<std::string> &allowedParams,
                       const std::string &namespacePrefix,
                       Context &ctx,
                       std::string &error) {
  for (auto &transform : transforms) {
    if (!isNonTypeTransformName(transform.name)) {
      if (transform.templateArgs.empty()) {
        ResolvedType resolvedName = resolveTypeString(transform.name, mapping, allowedParams, namespacePrefix, ctx, error);
        if (!error.empty()) {
          return false;
        }
        if (resolvedName.text != transform.name) {
          std::string base;
          std::string argText;
          if (splitTemplateTypeName(resolvedName.text, base, argText)) {
            if (!transform.templateArgs.empty()) {
              error = "template arguments cannot be combined on " + transform.name;
              return false;
            }
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args)) {
              error = "invalid template arguments for " + resolvedName.text;
              return false;
            }
            transform.name = base;
            transform.templateArgs = std::move(args);
          } else {
            transform.name = resolvedName.text;
          }
        }
      } else {
        bool allConcreteTemplateArgs = true;
        for (auto &arg : transform.templateArgs) {
          ResolvedType resolvedArg = resolveTypeString(arg, mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }
          allConcreteTemplateArgs = allConcreteTemplateArgs && resolvedArg.concrete;
          arg = resolvedArg.text;
        }

        std::string resolvedPath =
            resolveNameToPath(transform.name,
                              namespacePrefix,
                              scopedImportAliasesForNamespace(namespacePrefix, ctx),
                              ctx.sourceDefs);
        if (!selectGenericTypeOverloadPath(resolvedPath,
                                           transform.templateArgs.size(),
                                           ctx,
                                           resolvedPath,
                                           error)) {
          return false;
        }
        const bool isImportedGfxBufferTemplate =
            resolvedPath == "/std/gfx/Buffer" || resolvedPath == "/std/gfx/experimental/Buffer";
        if (isImportedGfxBufferTemplate && allConcreteTemplateArgs) {
          std::string specializedPath;
          if (!instantiateTemplate(resolvedPath, transform.templateArgs, ctx, error, specializedPath)) {
            return false;
          }
          transform.name = resolvedPath;
          continue;
        }
        const bool canResolveTemplatedName =
            isBuiltinTemplateContainer(transform.name) || ctx.templateDefs.count(resolvedPath) > 0;
        if (canResolveTemplatedName) {
          std::string templatedName = transform.name + "<" + joinTemplateArgs(transform.templateArgs) + ">";
          ResolvedType resolvedName = resolveTypeString(templatedName, mapping, allowedParams, namespacePrefix, ctx, error);
          if (!error.empty()) {
            return false;
          }

          std::string base;
          std::string argText;
          if (splitTemplateTypeName(resolvedName.text, base, argText)) {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args)) {
              error = "invalid template arguments for " + resolvedName.text;
              return false;
            }
            transform.name = base;
            transform.templateArgs = std::move(args);
          } else {
            transform.name = resolvedName.text;
            transform.templateArgs.clear();
          }
        }
      }
    }
    for (auto &arg : transform.templateArgs) {
      ResolvedType resolvedArg = resolveTypeString(arg, mapping, allowedParams, namespacePrefix, ctx, error);
      if (!error.empty()) {
        return false;
      }
      arg = resolvedArg.text;
    }
  }
  return true;
}

std::string resolveCalleePath(const Expr &expr, const std::string &namespacePrefix, const Context &ctx) {
  auto rewriteBuiltinCollectionImportAlias = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return resolvedPath;
    }
    std::string builtinCollection;
    if (!getBuiltinCollectionName(expr, builtinCollection)) {
      return resolvedPath;
    }
    if (expr.name != builtinCollection) {
      return resolvedPath;
    }
    if (resolvedPath == "/std/collections/map") {
      return "/std/collections/map/map";
    }
    return resolvedPath;
  };
  auto rewriteCanonicalCollectionConstructorPath = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return resolvedPath;
    }
    if (resolvedPath == "/std/collections/map/map" &&
        (ctx.sourceDefs.count(resolvedPath) > 0 || ctx.helperOverloads.count(resolvedPath) > 0)) {
      return resolvedPath;
    }
    const std::string helperPath =
        metadataBackedCanonicalMapConstructorRewritePath(resolvedPath,
                                                         expr.args.size());
    if (!helperPath.empty() && ctx.sourceDefs.count(helperPath) > 0) {
      return helperPath;
    }
    return resolvedPath;
  };
  auto finalizeResolvedPath = [&](const std::string &resolvedPath) -> std::string {
    return selectHelperOverloadPath(expr, rewriteCanonicalCollectionConstructorPath(resolvedPath), ctx);
  };
  auto resolveStdlibSurfaceCompatibilityAlias = [&]() -> std::string {
    if (expr.isMethodCall || !usesStdlibScopedImportAliases(namespacePrefix, ctx)) {
      return {};
    }
    for (const StdlibSurfaceMetadata &metadata : stdlibSurfaceRegistry()) {
      if (metadata.shape == StdlibSurfaceShape::ConstructorFamily) {
        continue;
      }
      auto findVisibleSpelling = [&](std::span<const std::string_view> spellings) {
        for (const std::string_view spelling : spellings) {
          const size_t slash = spelling.find_last_of('/');
          const std::string_view spellingLeaf =
              slash == std::string_view::npos ? spelling : spelling.substr(slash + 1);
          if (spelling.empty() || spelling.front() != '/' ||
              (spellingLeaf != expr.name &&
               resolveStdlibSurfaceMemberName(metadata, spelling) != expr.name)) {
            continue;
          }
          const std::string path(spelling);
          if (ctx.sourceDefs.count(path) > 0 ||
              ctx.helperOverloads.count(path) > 0) {
            return path;
          }
        }
        return std::string{};
      };
      if (std::string path = findVisibleSpelling(metadata.compatibilitySpellings);
          !path.empty()) {
        return path;
      }
      if (std::string path = findVisibleSpelling(metadata.loweringSpellings);
          !path.empty()) {
        return path;
      }
    }
    return {};
  };
  if (expr.name.empty()) {
    return "";
  }
  std::string builtinCollection;
  if (!expr.isMethodCall &&
      getBuiltinCollectionName(expr, builtinCollection) &&
      expr.name == builtinCollection) {
    return finalizeResolvedPath("/" + builtinCollection);
  }
  if (!expr.name.empty() && expr.name[0] == '/') {
    return finalizeResolvedPath(expr.name);
  }
  if (expr.name.find('/') != std::string::npos) {
    return finalizeResolvedPath("/" + expr.name);
  }
  if (!namespacePrefix.empty()) {
    const size_t lastSlash = namespacePrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(namespacePrefix)
                                        : std::string_view(namespacePrefix).substr(lastSlash + 1);
    if (suffix == expr.name && ctx.sourceDefs.count(namespacePrefix) > 0) {
      return finalizeResolvedPath(namespacePrefix);
    }
    std::string prefix = namespacePrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
      if (ctx.sourceDefs.count(candidate) > 0) {
        return finalizeResolvedPath(candidate);
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    if (const std::string *importAlias =
            lookupScopedImportAliasForNamespace(expr.name, namespacePrefix, ctx);
        importAlias != nullptr) {
      return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(*importAlias));
    }
    if (std::string stdlibSurfaceAlias = resolveStdlibSurfaceCompatibilityAlias();
        !stdlibSurfaceAlias.empty()) {
      return finalizeResolvedPath(stdlibSurfaceAlias);
    }
    return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(namespacePrefix + "/" + expr.name));
  }
  std::string root = "/" + expr.name;
  if (ctx.sourceDefs.count(root) > 0) {
    return finalizeResolvedPath(root);
  }
  if (const std::string *importAlias =
          lookupScopedImportAliasForNamespace(expr.name, namespacePrefix, ctx);
      importAlias != nullptr) {
    return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(*importAlias));
  }
  if (std::string stdlibSurfaceAlias = resolveStdlibSurfaceCompatibilityAlias();
      !stdlibSurfaceAlias.empty()) {
    return finalizeResolvedPath(stdlibSurfaceAlias);
  }
  return finalizeResolvedPath(rewriteBuiltinCollectionImportAlias(root));
}
