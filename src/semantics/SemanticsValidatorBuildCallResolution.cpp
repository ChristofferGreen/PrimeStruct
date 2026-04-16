#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

#include <cctype>
#include <string_view>

namespace primec::semantics {

void SemanticsValidator::rebuildCallResolutionFamilyIndexes() {
  overloadFamilyBasePaths_.clear();
  uniqueSpecializationPathByBase_.clear();
  ambiguousSpecializationBasePaths_.clear();

  auto observePath = [&](const std::string &path) {
    const size_t overloadMarker = path.rfind("__ov");
    if (overloadMarker != std::string::npos && overloadMarker > 0 && overloadMarker + 4 < path.size()) {
      bool hasOnlyDigits = true;
      for (size_t index = overloadMarker + 4; index < path.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(path[index]))) {
          hasOnlyDigits = false;
          break;
        }
      }
      if (hasOnlyDigits) {
        overloadFamilyBasePaths_.insert(path.substr(0, overloadMarker));
      }
    }

    const size_t specializationMarker = path.rfind("__t");
    if (specializationMarker == std::string::npos || specializationMarker == 0 ||
        specializationMarker + 3 >= path.size()) {
      return;
    }
    const std::string basePath = path.substr(0, specializationMarker);
    auto [it, inserted] = uniqueSpecializationPathByBase_.emplace(basePath, path);
    if (!inserted && it->second != path) {
      ambiguousSpecializationBasePaths_.insert(basePath);
    }
  };

  overloadFamilyBasePaths_.reserve(defMap_.size());
  uniqueSpecializationPathByBase_.reserve(defMap_.size());
  ambiguousSpecializationBasePaths_.reserve(defMap_.size());
  for (const auto &[path, def] : defMap_) {
    (void)def;
    observePath(path);
  }
  for (const auto &[path, params] : paramsByDef_) {
    (void)params;
    observePath(path);
  }

  for (const auto &basePath : ambiguousSpecializationBasePaths_) {
    uniqueSpecializationPathByBase_.erase(basePath);
  }
}

std::string SemanticsValidator::resolveCalleePath(const Expr &expr) const {
  const Definition *activeDefinition = currentDefinitionContext_;
  const Execution *activeExecution = currentExecutionContext_;
  const bool hasScopedOwner = activeDefinition != nullptr || activeExecution != nullptr;
  if (hasScopedOwner &&
      (callTargetResolutionScratch_.definitionOwner != activeDefinition ||
       callTargetResolutionScratch_.executionOwner != activeExecution)) {
    callTargetResolutionScratch_.resetArena();
    callTargetResolutionScratch_.definitionOwner = activeDefinition;
    callTargetResolutionScratch_.executionOwner = activeExecution;
  } else if (!hasScopedOwner &&
             (!callTargetResolutionScratch_.definitionFamilyPathCache.empty() ||
              !callTargetResolutionScratch_.overloadFamilyPathCache.empty() ||
              !callTargetResolutionScratch_.overloadFamilyPrefixCache.empty() ||
              !callTargetResolutionScratch_.specializationPrefixCache.empty() ||
              !callTargetResolutionScratch_.overloadCandidatePathCache.empty() ||
              !callTargetResolutionScratch_.normalizedMethodNameCache.empty() ||
              !callTargetResolutionScratch_.explicitRemovedMethodPathCache.empty() ||
              !callTargetResolutionScratch_.concreteCallBaseCandidates.empty() ||
              callTargetResolutionScratch_.definitionOwner != nullptr ||
              callTargetResolutionScratch_.executionOwner != nullptr)) {
    callTargetResolutionScratch_.resetArena();
  }

  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathText(path);
    SymbolId pathKey = InvalidSymbolId;
    if (hasScopedOwner) {
      pathKey = callTargetResolutionScratch_.keyInterner.intern(path);
    }
    if (hasScopedOwner) {
      if (pathKey != InvalidSymbolId) {
        if (const auto cacheIt = callTargetResolutionScratch_.definitionFamilyPathCache.find(pathKey);
            cacheIt != callTargetResolutionScratch_.definitionFamilyPathCache.end()) {
          return cacheIt->second;
        }
      }
      bool hasPath = false;
      if (defMap_.count(pathText) > 0) {
        hasPath = true;
      } else {
        const std::string templatedPrefix = pathText + "<";
        const std::string specializedPrefix = pathText + "__t";
        for (const auto &def : program_.definitions) {
          if (def.fullPath == path || def.fullPath.rfind(templatedPrefix, 0) == 0 ||
              def.fullPath.rfind(specializedPrefix, 0) == 0) {
            hasPath = true;
            break;
          }
        }
      }
      if (pathKey != InvalidSymbolId) {
        callTargetResolutionScratch_.definitionFamilyPathCache.emplace(pathKey, hasPath);
      }
      return hasPath;
    }
    if (defMap_.count(pathText) > 0) {
      return true;
    }
    const std::string templatedPrefix = pathText + "<";
    const std::string specializedPrefix = pathText + "__t";
    for (const auto &def : program_.definitions) {
      if (def.fullPath == path || def.fullPath.rfind(templatedPrefix, 0) == 0 ||
          def.fullPath.rfind(specializedPrefix, 0) == 0) {
        return true;
      }
    }
    return false;
  };
  auto rootedPathForName = [&](std::string_view name) -> std::string {
    if (!hasScopedOwner) {
      return "/" + std::string(name);
    }
    const SymbolId key = callTargetResolutionScratch_.keyInterner.intern(name);
    if (key == InvalidSymbolId) {
      return "/" + std::string(name);
    }
    if (const auto cacheIt = callTargetResolutionScratch_.rootedCallNamePathCache.find(key);
        cacheIt != callTargetResolutionScratch_.rootedCallNamePathCache.end()) {
      return cacheIt->second;
    }
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.rootedCallNamePathCache.emplace(key, "/" + std::string(name));
    (void)inserted;
    return insertIt->second;
  };
  auto normalizedPrefixPath = [&](std::string_view namespacePrefix) -> std::string {
    if (!hasScopedOwner) {
      std::string normalizedPrefix(namespacePrefix);
      if (!normalizedPrefix.empty() && normalizedPrefix.front() != '/') {
        normalizedPrefix.insert(normalizedPrefix.begin(), '/');
      }
      return normalizedPrefix;
    }
    const SymbolId key = callTargetResolutionScratch_.keyInterner.intern(namespacePrefix);
    if (key == InvalidSymbolId) {
      std::string normalizedPrefix(namespacePrefix);
      if (!normalizedPrefix.empty() && normalizedPrefix.front() != '/') {
        normalizedPrefix.insert(normalizedPrefix.begin(), '/');
      }
      return normalizedPrefix;
    }
    if (const auto cacheIt = callTargetResolutionScratch_.normalizedNamespacePrefixCache.find(key);
        cacheIt != callTargetResolutionScratch_.normalizedNamespacePrefixCache.end()) {
      return cacheIt->second;
    }
    std::string normalizedPrefix(namespacePrefix);
    if (!normalizedPrefix.empty() && normalizedPrefix.front() != '/') {
      normalizedPrefix.insert(normalizedPrefix.begin(), '/');
    }
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.normalizedNamespacePrefixCache.emplace(key, std::move(normalizedPrefix));
    (void)inserted;
    return insertIt->second;
  };
  auto joinedPath = [&](std::string_view prefix, std::string_view name) -> std::string {
    if (!hasScopedOwner) {
      return std::string(prefix) + "/" + std::string(name);
    }
    const SymbolId prefixKey = callTargetResolutionScratch_.keyInterner.intern(prefix);
    const SymbolId nameKey = callTargetResolutionScratch_.keyInterner.intern(name);
    if (prefixKey == InvalidSymbolId || nameKey == InvalidSymbolId) {
      return std::string(prefix) + "/" + std::string(name);
    }
    const CallTargetResolutionScratch::SymbolPairKey key{prefixKey, nameKey};
    if (const auto cacheIt = callTargetResolutionScratch_.joinedCallPathCache.find(key);
        cacheIt != callTargetResolutionScratch_.joinedCallPathCache.end()) {
      return cacheIt->second;
    }
    std::string joined = std::string(prefix) + "/" + std::string(name);
    auto [insertIt, inserted] =
        callTargetResolutionScratch_.joinedCallPathCache.emplace(key, std::move(joined));
    (void)inserted;
    return insertIt->second;
  };
  auto rewriteCanonicalCollectionHelperPath = [&](const std::string &resolvedPath) -> std::string {
    auto canonicalMapHelperAliasPath = [&](std::string_view) -> std::string {
      return {};
    };
    auto rewriteCanonicalHelper = [&](std::string_view prefix,
                                      const auto &aliasPathBuilder,
                                      bool requirePositionalBuiltinAlias) -> std::string {
      if (resolvedPath.rfind(prefix, 0) != 0) {
        return resolvedPath;
      }
      const std::string helperName = resolvedPath.substr(prefix.size());
      const std::string aliasPath = aliasPathBuilder(helperName);
      if (aliasPath.empty()) {
        return resolvedPath;
      }
      if (defMap_.count(resolvedPath) == 0 && defMap_.count(aliasPath) > 0) {
        return aliasPath;
      }
      if (requirePositionalBuiltinAlias && !hasNamedArguments(expr.argNames) &&
          defMap_.count(resolvedPath) > 0) {
        return aliasPath;
      }
      return resolvedPath;
    };
    return rewriteCanonicalHelper("/std/collections/map/", canonicalMapHelperAliasPath, false);
  };

  auto preferredFileErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (hasDefinitionFamilyPath("/std/file/FileError/why")) {
        return "/std/file/FileError/why";
      }
      if (hasDefinitionFamilyPath("/FileError/why")) {
        return "/FileError/why";
      }
      return "/file_error/why";
    }
    if (helperName == "is_eof") {
      if (hasDefinitionFamilyPath("/std/file/FileError/is_eof")) {
        return "/std/file/FileError/is_eof";
      }
      if (hasDefinitionFamilyPath("/FileError/is_eof")) {
        return "/FileError/is_eof";
      }
      if (hasDefinitionFamilyPath("/std/file/fileErrorIsEof")) {
        return "/std/file/fileErrorIsEof";
      }
      return {};
    }
    if (helperName == "eof") {
      if (hasDefinitionFamilyPath("/std/file/FileError/eof")) {
        return "/std/file/FileError/eof";
      }
      if (hasDefinitionFamilyPath("/FileError/eof")) {
        return "/FileError/eof";
      }
      if (hasDefinitionFamilyPath("/std/file/fileReadEof")) {
        return "/std/file/fileReadEof";
      }
      return {};
    }
    if (helperName == "status" && hasDefinitionFamilyPath("/std/file/FileError/status")) {
      return "/std/file/FileError/status";
    }
    if (helperName == "result" && hasDefinitionFamilyPath("/std/file/FileError/result")) {
      return "/std/file/FileError/result";
    }
    return {};
  };

  auto preferredImageErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap_.count("/std/image/ImageError/why") > 0) {
        return "/std/image/ImageError/why";
      }
      if (defMap_.count("/ImageError/why") > 0) {
        return "/ImageError/why";
      }
      return {};
    }
    if (helperName == "status") {
      if (defMap_.count("/std/image/ImageError/status") > 0) {
        return "/std/image/ImageError/status";
      }
      if (defMap_.count("/ImageError/status") > 0) {
        return "/ImageError/status";
      }
      if (defMap_.count("/std/image/imageErrorStatus") > 0) {
        return "/std/image/imageErrorStatus";
      }
      return {};
    }
    if (helperName == "result") {
      if (defMap_.count("/std/image/ImageError/result") > 0) {
        return "/std/image/ImageError/result";
      }
      if (defMap_.count("/ImageError/result") > 0) {
        return "/ImageError/result";
      }
      if (defMap_.count("/std/image/imageErrorResult") > 0) {
        return "/std/image/imageErrorResult";
      }
      return {};
    }
    return {};
  };

  auto preferredContainerErrorHelperTarget = [&](std::string_view helperName) -> std::string {
    if (helperName == "why") {
      if (defMap_.count("/std/collections/ContainerError/why") > 0) {
        return "/std/collections/ContainerError/why";
      }
      if (defMap_.count("/ContainerError/why") > 0) {
        return "/ContainerError/why";
      }
      return {};
    }
    if (helperName == "status") {
      if (defMap_.count("/std/collections/ContainerError/status") > 0) {
        return "/std/collections/ContainerError/status";
      }
      if (defMap_.count("/ContainerError/status") > 0) {
        return "/ContainerError/status";
      }
      if (defMap_.count("/std/collections/containerErrorStatus") > 0) {
        return "/std/collections/containerErrorStatus";
      }
      return {};
    }
    if (helperName == "result") {
      if (defMap_.count("/std/collections/ContainerError/result") > 0) {
        return "/std/collections/ContainerError/result";
      }
      if (defMap_.count("/ContainerError/result") > 0) {
        return "/ContainerError/result";
      }
      if (defMap_.count("/std/collections/containerErrorResult") > 0) {
        return "/std/collections/containerErrorResult";
      }
      return {};
    }
    return {};
  };

  auto preferredGfxErrorHelperTarget = [&](std::string_view helperName,
                                           const std::string &preferredNamespacePath) -> std::string {
    const bool experimental = preferredNamespacePath.find("/std/gfx/experimental/") == 0;
    if (helperName == "why") {
      if (experimental) {
        if (defMap_.count("/std/gfx/experimental/GfxError/why") > 0) {
          return "/std/gfx/experimental/GfxError/why";
        }
      } else if (defMap_.count("/std/gfx/GfxError/why") > 0) {
        return "/std/gfx/GfxError/why";
      }
      if (defMap_.count("/GfxError/why") > 0) {
        return "/GfxError/why";
      }
      return {};
    }
    if (helperName == "status") {
      if (experimental) {
        if (defMap_.count("/std/gfx/experimental/GfxError/status") > 0) {
          return "/std/gfx/experimental/GfxError/status";
        }
      } else if (defMap_.count("/std/gfx/GfxError/status") > 0) {
        return "/std/gfx/GfxError/status";
      }
      if (defMap_.count("/GfxError/status") > 0) {
        return "/GfxError/status";
      }
      return {};
    }
    if (helperName == "result") {
      if (experimental) {
        if (defMap_.count("/std/gfx/experimental/GfxError/result") > 0) {
          return "/std/gfx/experimental/GfxError/result";
        }
      } else if (defMap_.count("/std/gfx/GfxError/result") > 0) {
        return "/std/gfx/GfxError/result";
      }
      if (defMap_.count("/GfxError/result") > 0) {
        return "/GfxError/result";
      }
      return {};
    }
    return {};
  };

  auto rewriteCanonicalCollectionConstructorPath = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return rewriteCanonicalCollectionHelperPath(resolvedPath);
    }
    auto vectorConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/vectorNew";
      case 1:
        return "/std/collections/vectorSingle";
      case 2:
        return "/std/collections/vectorPair";
      case 3:
        return "/std/collections/vectorTriple";
      case 4:
        return "/std/collections/vectorQuad";
      case 5:
        return "/std/collections/vectorQuint";
      case 6:
        return "/std/collections/vectorSext";
      case 7:
        return "/std/collections/vectorSept";
      case 8:
        return "/std/collections/vectorOct";
      default:
        return {};
      }
    };
    auto mapConstructorHelperPath = [&](size_t argumentCount) -> std::string {
      return canonicalMapConstructorHelperPath(argumentCount);
    };
    std::string helperPath;
    if (resolvedPath == "/std/collections/vector/vector") {
      if (hasDefinitionFamilyPath(resolvedPath) || hasImportedDefinitionPath(resolvedPath)) {
        return resolvedPath;
      }
      helperPath = vectorConstructorHelperPath();
    } else if (resolvedPath == "/std/collections/map/map") {
      if (hasDefinitionFamilyPath(resolvedPath) || hasImportedDefinitionPath(resolvedPath)) {
        return resolvedPath;
      }
      helperPath = mapConstructorHelperPath(expr.args.size());
    } else if (resolvedPath == "/map/map") {
      helperPath = mapConstructorHelperPath(expr.args.size());
    }
    if (!helperPath.empty()) {
      if (defMap_.count(helperPath) > 0) {
        return helperPath;
      }
      const std::string experimentalHelper = canonicalMapConstructorToExperimental(helperPath);
      if (!experimentalHelper.empty() && defMap_.count(experimentalHelper) > 0) {
        return experimentalHelper;
      }
    }
    return rewriteCanonicalCollectionHelperPath(resolvedPath);
  };

  if (expr.name.empty()) {
    return "";
  }
  if (expr.name.front() == '/') {
    return rewriteCanonicalCollectionConstructorPath(expr.name);
  }
  if (expr.name.find('/') != std::string::npos) {
    return rewriteCanonicalCollectionConstructorPath(rootedPathForName(expr.name));
  }

  std::string pointerBuiltinName;
  if (getBuiltinPointerName(expr, pointerBuiltinName)) {
    return "/" + pointerBuiltinName;
  }
  if (!expr.namespacePrefix.empty()) {
    const std::string normalizedPrefix = normalizedPrefixPath(expr.namespacePrefix);
    auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "capacity" || helperName == "at" ||
             helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
             helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
             helperName == "remove_swap";
    };
    auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "count_ref" ||
             helperName == "contains" || helperName == "contains_ref" ||
             helperName == "tryAt" || helperName == "tryAt_ref" ||
             helperName == "at" || helperName == "at_ref" ||
             helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
             helperName == "insert" || helperName == "insert_ref";
    };
    const size_t lastSlash = normalizedPrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(normalizedPrefix)
                                        : std::string_view(normalizedPrefix).substr(lastSlash + 1);
    if (normalizedPrefix == "/FileError") {
      const std::string preferred = preferredFileErrorHelperTarget(expr.name);
      if (!preferred.empty()) {
        return preferred;
      }
    }
    if (normalizedPrefix == "/ImageError") {
      const std::string preferred = preferredImageErrorHelperTarget(expr.name);
      if (!preferred.empty()) {
        return preferred;
      }
    }
    if (normalizedPrefix == "/ContainerError") {
      const std::string preferred = preferredContainerErrorHelperTarget(expr.name);
      if (!preferred.empty()) {
        return preferred;
      }
    }
    if (normalizedPrefix == "/GfxError") {
      const std::string preferred = preferredGfxErrorHelperTarget(expr.name, normalizedPrefix);
      if (!preferred.empty()) {
        return preferred;
      }
    }
    if (suffix == expr.name && defMap_.count(normalizedPrefix) > 0) {
      return normalizedPrefix;
    }
    std::string prefix = normalizedPrefix;
    while (!prefix.empty()) {
      const std::string candidate = joinedPath(prefix, expr.name);
      if (defMap_.count(candidate) > 0) {
        return candidate;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    if (normalizedPrefix.rfind("/std/collections/vector", 0) == 0 &&
        isRemovedVectorCompatibilityHelper(expr.name)) {
      return normalizedPrefix + "/" + expr.name;
    }
    if (normalizedPrefix.rfind("/std/collections/map", 0) == 0 &&
        isRemovedMapCompatibilityHelper(expr.name)) {
      return normalizedPrefix + "/" + expr.name;
    }
    auto it = importAliases_.find(expr.name);
    if (it != importAliases_.end()) {
      return rewriteCanonicalCollectionConstructorPath(it->second);
    }
    const std::string root = rootedPathForName(expr.name);
    if (defMap_.count(root) > 0) {
      return rewriteCanonicalCollectionConstructorPath(root);
    }
    return rewriteCanonicalCollectionConstructorPath(joinedPath(normalizedPrefix, expr.name));
  }

  const std::string root = rootedPathForName(expr.name);
  if (defMap_.count(root) > 0) {
    return rewriteCanonicalCollectionConstructorPath(root);
  }
  auto it = importAliases_.find(expr.name);
  if (it != importAliases_.end()) {
    return rewriteCanonicalCollectionConstructorPath(it->second);
  }
  return rewriteCanonicalCollectionConstructorPath(root);
}

}  // namespace primec::semantics
