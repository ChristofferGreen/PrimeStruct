#include "SemanticsValidator.h"

#include <string_view>

namespace primec::semantics {

std::string SemanticsValidator::resolveCalleePath(const Expr &expr) const {
  auto rewriteCanonicalCollectionHelperPath = [&](const std::string &resolvedPath) -> std::string {
    auto canonicalMapHelperAliasPath = [&](std::string_view helperName) -> std::string {
      if (helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
          helperName == "at" || helperName == "at_unsafe") {
        return "/map/" + std::string(helperName);
      }
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

  auto rewriteCanonicalCollectionConstructorPath = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return rewriteCanonicalCollectionHelperPath(resolvedPath);
    }
    auto hasDefinitionFamilyPath = [&](const std::string &path) {
      if (defMap_.count(path) > 0) {
        return true;
      }
      for (const auto &def : program_.definitions) {
        if (def.fullPath == path) {
          return true;
        }
      }
      return false;
    };
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
    auto mapConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/mapNew";
      case 2:
        return "/std/collections/mapSingle";
      case 4:
        return "/std/collections/mapPair";
      case 6:
        return "/std/collections/mapTriple";
      case 8:
        return "/std/collections/mapQuad";
      case 10:
        return "/std/collections/mapQuint";
      case 12:
        return "/std/collections/mapSext";
      case 14:
        return "/std/collections/mapSept";
      case 16:
        return "/std/collections/mapOct";
      default:
        return {};
      }
    };
    std::string helperPath;
    if (resolvedPath == "/std/collections/vector/vector") {
      if (hasDefinitionFamilyPath(resolvedPath) || hasImportedDefinitionPath(resolvedPath)) {
        return resolvedPath;
      }
      helperPath = vectorConstructorHelperPath();
    } else if (resolvedPath == "/std/collections/map/map") {
      helperPath = mapConstructorHelperPath();
    }
    if (!helperPath.empty() && defMap_.count(helperPath) > 0) {
      return helperPath;
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
    return rewriteCanonicalCollectionConstructorPath("/" + expr.name);
  }

  std::string pointerBuiltinName;
  if (getBuiltinPointerName(expr, pointerBuiltinName)) {
    return "/" + pointerBuiltinName;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string normalizedPrefix = expr.namespacePrefix;
    if (normalizedPrefix.front() != '/') {
      normalizedPrefix.insert(normalizedPrefix.begin(), '/');
    }
    auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "capacity" || helperName == "at" ||
             helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
             helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
             helperName == "remove_swap";
    };
    auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
    };
    const size_t lastSlash = normalizedPrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(normalizedPrefix)
                                        : std::string_view(normalizedPrefix).substr(lastSlash + 1);
    if (suffix == expr.name && defMap_.count(normalizedPrefix) > 0) {
      return normalizedPrefix;
    }
    std::string prefix = normalizedPrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
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
    return rewriteCanonicalCollectionConstructorPath(normalizedPrefix + "/" + expr.name);
  }

  std::string root = "/" + expr.name;
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
