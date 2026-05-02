#include "IrLowererCallHelpers.h"

#include <optional>
#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveCallPathWithoutSemanticFallbackProbes(const Expr &expr);

std::string resolveScopedCallPathForHelperMatching(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return expr.name;
}

bool resolvesToPublishedDefinitionFamilyTarget(const SemanticProgram *semanticProgram,
                                               const std::string &resolvedPath) {
  if (semanticProgram == nullptr || resolvedPath.empty()) {
    return false;
  }
  const std::string templatedPrefix = resolvedPath + "<";
  const std::string specializedPrefix = resolvedPath + "__t";
  const std::string overloadPrefix = resolvedPath + "__ov";
  auto asStringView = [](const std::string &value) {
    return std::string_view(value.data(), value.size());
  };
  auto matchesResolvedFamily = [&](std::string_view publishedPath) {
    return publishedPath == asStringView(resolvedPath) ||
           publishedPath.starts_with(asStringView(templatedPrefix)) ||
           publishedPath.starts_with(asStringView(specializedPrefix)) ||
           publishedPath.starts_with(asStringView(overloadPrefix));
  };
  for (const auto &[pathId, definitionIndex] :
       semanticProgram->publishedRoutingLookups.definitionIndicesByPathId) {
    if (definitionIndex >= semanticProgram->definitions.size()) {
      continue;
    }
    const auto &entry = semanticProgram->definitions[definitionIndex];
    if (matchesResolvedFamily(asStringView(entry.fullPath))) {
      return true;
    }
    const std::string_view publishedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, pathId);
    if (!publishedPath.empty() && matchesResolvedFamily(publishedPath)) {
      return true;
    }
  }
  return false;
}

std::string resolveCallPathFromPublishedLookups(const Expr &expr,
                                                const SemanticProgram *semanticProgram) {
  if (semanticProgram == nullptr) {
    return resolveCallPathWithoutSemanticFallbackProbes(expr);
  }
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    scoped += "/" + expr.name;
    if (semanticProgramLookupPublishedDefinition(*semanticProgram, scoped) != nullptr) {
      return scoped;
    }
    if (const auto importAliasTargetPathId =
            semanticProgramLookupPublishedImportAliasTargetPathId(*semanticProgram, expr.name);
        importAliasTargetPathId.has_value()) {
      return normalizeMapImportAliasPath(
          std::string(semanticProgramResolveCallTargetString(*semanticProgram,
                                                             *importAliasTargetPathId)));
    }
    const std::string root = "/" + expr.name;
    if (semanticProgramLookupPublishedDefinition(*semanticProgram, root) != nullptr) {
      return root;
    }
    return scoped;
  }
  if (const auto importAliasTargetPathId =
          semanticProgramLookupPublishedImportAliasTargetPathId(*semanticProgram, expr.name);
      importAliasTargetPathId.has_value()) {
    return normalizeMapImportAliasPath(
        std::string(
            semanticProgramResolveCallTargetString(*semanticProgram, *importAliasTargetPathId)));
  }
  return "/" + expr.name;
}

bool isBridgeHelperName(std::string_view collectionFamily, std::string_view helperName) {
  if (collectionFamily == "soa_vector") {
    return helperName == "count" || helperName == "get" || helperName == "ref" ||
           helperName == "to_aos" || helperName == "push" || helperName == "reserve";
  }
  return false;
}

bool isPublishedCollectionBridgeStdlibSurfaceId(std::optional<StdlibSurfaceId> surfaceId) {
  return surfaceId.has_value() &&
         (*surfaceId == StdlibSurfaceId::CollectionsVectorHelpers ||
          *surfaceId == StdlibSurfaceId::CollectionsMapHelpers ||
          *surfaceId == StdlibSurfaceId::CollectionsSoaVectorHelpers);
}

bool isPublishedCollectionBridgeCall(const SemanticProgram *semanticProgram, const Expr &expr) {
  return isPublishedCollectionBridgeStdlibSurfaceId(
             findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)) ||
         isPublishedCollectionBridgeStdlibSurfaceId(
             findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr));
}

std::optional<std::pair<std::string, std::string>>
residualBridgeChoiceFromResolvedPath(const std::string &resolvedPath) {
  auto parsePrefixedHelper = [&](std::string_view prefix,
                                 std::string_view collectionFamily)
      -> std::optional<std::pair<std::string, std::string>> {
    if (resolvedPath.rfind(prefix, 0) != 0) {
      return std::nullopt;
    }
    std::string helperName = resolvedPath.substr(prefix.size());
    const size_t specializationSuffix = helperName.find("__t");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
    if (!isBridgeHelperName(collectionFamily, helperName)) {
      return std::nullopt;
    }
    return std::pair<std::string, std::string>(std::string(collectionFamily), std::move(helperName));
  };

  if (auto parsed = parsePrefixedHelper("/soa_vector/", "soa_vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/soa_vector/", "soa_vector")) {
    return parsed;
  }
  return std::nullopt;
}

bool isResidualBridgeHelperPath(const std::string &resolvedPath) {
  return residualBridgeChoiceFromResolvedPath(resolvedPath).has_value();
}

bool isBridgeHelperCall(const SemanticProgram *semanticProgram,
                        const Expr &expr,
                        const std::string &resolvedPath) {
  return isPublishedCollectionBridgeCall(semanticProgram, expr) ||
         isResidualBridgeHelperPath(resolvedPath);
}

bool isBuiltinPublishedMapHelperName(const Expr &expr, std::string_view helperName) {
  if (helperName == "count") {
    return expr.args.size() == 1;
  }
  if (helperName == "contains" || helperName == "tryAt" ||
      helperName == "at" || helperName == "at_unsafe") {
    return expr.args.size() == 2;
  }
  if (helperName == "insert") {
    return expr.args.size() == 3;
  }
  return false;
}

bool isMapBuiltinResolvedPath(const SemanticProgram *semanticProgram,
                              const Expr &expr,
                              const std::string &resolvedPath) {
  std::string semanticHelperName;
  if (resolvePublishedSemanticStdlibSurfaceMemberName(
          semanticProgram,
          expr,
          StdlibSurfaceId::CollectionsMapHelpers,
          semanticHelperName) &&
      isBuiltinPublishedMapHelperName(expr, semanticHelperName)) {
    return true;
  }

  auto matchesResolvedPath = [&](std::string_view basePath) {
    return resolvedPath == basePath ||
           resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0 ||
           resolvedPath.rfind(std::string(basePath) + "__ov", 0) == 0;
  };
  auto matchesExperimentalMapMethodPath = [&](std::string_view helperName) {
    const std::string prefix = "/std/collections/experimental_map/Map__";
    const std::string suffix = "/" + std::string(helperName);
    return resolvedPath.rfind(prefix, 0) == 0 &&
           resolvedPath.size() > suffix.size() &&
           resolvedPath.ends_with(suffix);
  };
  if (!expr.isMethodCall) {
    std::string aliasName;
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/map/at") ||
             matchesResolvedPath("/std/collections/mapAt") ||
             matchesResolvedPath("/std/collections/map/at_unsafe") ||
             matchesResolvedPath("/std/collections/mapAtUnsafe");
    }
    if (resolveMapHelperAliasName(expr, aliasName)) {
      if (aliasName == "count" && expr.args.size() == 1) {
        return matchesResolvedPath("/std/collections/map/count") ||
               matchesResolvedPath("/std/collections/map/count_ref") ||
               matchesResolvedPath("/std/collections/mapCount") ||
               matchesResolvedPath("/std/collections/experimental_map/mapCount") ||
               matchesResolvedPath("/std/collections/experimental_map/mapCountRef") ||
               matchesExperimentalMapMethodPath("count");
      }
      if (aliasName == "contains" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/contains") ||
               matchesResolvedPath("/std/collections/map/contains_ref") ||
               matchesResolvedPath("/std/collections/mapContains") ||
               matchesResolvedPath("/std/collections/experimental_map/mapContains") ||
               matchesResolvedPath("/std/collections/experimental_map/mapContainsRef") ||
               matchesExperimentalMapMethodPath("contains");
      }
      if (aliasName == "tryAt" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/tryAt") ||
               matchesResolvedPath("/std/collections/map/tryAt_ref") ||
               matchesResolvedPath("/std/collections/mapTryAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapTryAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapTryAtRef") ||
               matchesExperimentalMapMethodPath("tryAt");
      }
      if (aliasName == "at" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/at") ||
               matchesResolvedPath("/std/collections/map/at_ref") ||
               matchesResolvedPath("/std/collections/mapAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAtRef") ||
               matchesExperimentalMapMethodPath("at");
      }
      if (aliasName == "at_unsafe" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/at_unsafe") ||
               matchesResolvedPath("/std/collections/map/at_unsafe_ref") ||
               matchesResolvedPath("/std/collections/mapAtUnsafe") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAtUnsafe") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAtUnsafeRef") ||
               matchesExperimentalMapMethodPath("at_unsafe");
      }
      if (aliasName == "insert" && expr.args.size() == 3) {
        return matchesResolvedPath("/std/collections/map/insert") ||
               matchesResolvedPath("/std/collections/map/insert_ref");
      }
    }
    std::string normalizedName = resolveCallPathWithoutSemanticFallbackProbes(expr);
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    if ((normalizedName == "mapCount" ||
         normalizedName == "std/collections/experimental_map/mapCount" ||
         normalizedName == "mapCountRef" ||
         normalizedName == "std/collections/experimental_map/mapCountRef") &&
        expr.args.size() == 1) {
      return matchesResolvedPath("/std/collections/experimental_map/mapCount") ||
             matchesResolvedPath("/std/collections/experimental_map/mapCountRef") ||
             matchesExperimentalMapMethodPath("count");
    }
    if ((normalizedName == "mapContains" ||
         normalizedName == "std/collections/experimental_map/mapContains" ||
         normalizedName == "mapContainsRef" ||
         normalizedName == "std/collections/experimental_map/mapContainsRef") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/experimental_map/mapContains") ||
             matchesResolvedPath("/std/collections/experimental_map/mapContainsRef") ||
             matchesExperimentalMapMethodPath("contains");
    }
    if ((normalizedName == "mapTryAt" ||
         normalizedName == "std/collections/experimental_map/mapTryAt" ||
         normalizedName == "mapTryAtRef" ||
         normalizedName == "std/collections/experimental_map/mapTryAtRef") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/experimental_map/mapTryAt") ||
             matchesResolvedPath("/std/collections/experimental_map/mapTryAtRef") ||
             matchesExperimentalMapMethodPath("tryAt");
    }
    if ((normalizedName == "mapAt" ||
         normalizedName == "std/collections/experimental_map/mapAt" ||
         normalizedName == "mapAtRef" ||
         normalizedName == "std/collections/experimental_map/mapAtRef") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/experimental_map/mapAt") ||
             matchesResolvedPath("/std/collections/experimental_map/mapAtRef") ||
             matchesExperimentalMapMethodPath("at");
    }
    if ((normalizedName == "mapAtUnsafe" ||
         normalizedName == "std/collections/experimental_map/mapAtUnsafe" ||
         normalizedName == "mapAtUnsafeRef" ||
         normalizedName == "std/collections/experimental_map/mapAtUnsafeRef") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/experimental_map/mapAtUnsafe") ||
             matchesResolvedPath("/std/collections/experimental_map/mapAtUnsafeRef") ||
             matchesExperimentalMapMethodPath("at_unsafe");
    }
    if ((normalizedName == "contains" || normalizedName == "map/contains" ||
         normalizedName == "std/collections/map/contains") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/map/contains") ||
             matchesResolvedPath("/std/collections/mapContains");
    }
    if ((normalizedName == "tryAt" || normalizedName == "map/tryAt" ||
         normalizedName == "std/collections/map/tryAt") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/map/tryAt") ||
             matchesResolvedPath("/std/collections/mapTryAt");
    }
    if ((normalizedName == "count" || normalizedName == "map/count" ||
         normalizedName == "std/collections/map/count") &&
        expr.args.size() == 1) {
      return matchesResolvedPath("/std/collections/map/count") ||
             matchesResolvedPath("/std/collections/mapCount");
    }
    if ((normalizedName == "insert" || normalizedName == "map/insert" ||
         normalizedName == "std/collections/map/insert") &&
        expr.args.size() == 3) {
      return matchesResolvedPath("/std/collections/map/insert") ||
             matchesResolvedPath("/std/collections/mapInsert");
    }
  }
  return false;
}

bool isExplicitSamePathPublishedMapHelperCall(const Expr &expr,
                                              const std::string &resolvedPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);
  if (rawPath.empty() || rawPath.front() != '/') {
    return false;
  }
  std::string helperName;
  if (!resolveMapHelperAliasName(expr, helperName) &&
      rawPath.rfind("/map/", 0) != 0 &&
      rawPath.rfind("/std/collections/map/", 0) != 0 &&
      rawPath.rfind("/std/collections/experimental_map/", 0) != 0 &&
      resolvedPath.rfind("/std/collections/map/", 0) != 0 &&
      resolvedPath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return false;
  }
  return normalizeCollectionHelperPath(rawPath) ==
         normalizeCollectionHelperPath(resolvedPath);
}

bool isSemanticBarePublishedMapHelperCall(const Expr &expr,
                                          const SemanticProgram *semanticProgram,
                                          const std::string &resolvedPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.semanticNodeId == 0 ||
      !expr.namespacePrefix.empty() || expr.name.empty() || expr.name.front() == '/') {
    return false;
  }
  if (resolvedPath.rfind("/std/collections/map/", 0) != 0) {
    return false;
  }

  std::string helperName;
  if (resolvePublishedSemanticStdlibSurfaceMemberName(
          semanticProgram,
          expr,
          StdlibSurfaceId::CollectionsMapHelpers,
          helperName)) {
    return helperName == "count" || helperName == "contains" ||
           helperName == "tryAt" || helperName == "at" ||
           helperName == "at_unsafe";
  }
  if (resolveMapHelperAliasName(expr, helperName)) {
    return helperName == "count" || helperName == "contains" ||
           helperName == "tryAt" || helperName == "at" ||
           helperName == "at_unsafe";
  }
  if (expr.name == "count" || expr.name == "contains" ||
      expr.name == "tryAt" || expr.name == "at" ||
      expr.name == "at_unsafe") {
    return true;
  }

  std::string accessName;
  return getBuiltinArrayAccessName(expr, accessName) &&
         (accessName == "at" || accessName == "at_unsafe");
}

bool isExactRootedMapAliasDefinitionCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name.empty() ||
      expr.name.front() != '/') {
    return false;
  }
  std::string helperName;
  return resolveMapHelperAliasName(expr, helperName) &&
         (helperName == "count" || helperName == "contains" ||
          helperName == "tryAt" || helperName == "at" ||
          helperName == "at_unsafe" || helperName == "insert" ||
          helperName == "insert_ref");
}

bool isExplicitExperimentalMapHelperDefinitionCall(const Expr &expr,
                                                   const std::string &resolvedPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);
  if (rawPath.rfind("/std/collections/experimental_map/", 0) != 0 ||
      resolvedPath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return false;
  }
  if (resolvedPath.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    return false;
  }
  std::string helperName;
  return resolveMapHelperAliasName(expr, helperName);
}

const Definition *preferExplicitExperimentalMapHelperDefinition(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      resolvedPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
    return nullptr;
  }
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);
  if (rawPath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return nullptr;
  }
  std::string helperName;
  if (!resolveMapHelperAliasName(expr, helperName)) {
    return nullptr;
  }
  if (const Definition *rawDef = resolveDefinitionByPath(defMap, rawPath);
      rawDef != nullptr) {
    return rawDef;
  }
  const std::string specializedPrefix = rawPath + "__t";
  const std::string overloadPrefix = rawPath + "__ov";
  for (const auto &[path, def] : defMap) {
    if (def == nullptr) {
      continue;
    }
    if (path.rfind(specializedPrefix, 0) == 0 ||
        path.rfind(overloadPrefix, 0) == 0) {
      return def;
    }
  }
  return nullptr;
}

const Definition *preferExplicitExperimentalVectorHelperDefinition(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      resolvedPath.rfind("/std/collections/experimental_vector/Vector__", 0) != 0) {
    return nullptr;
  }
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);
  if (rawPath != "/std/collections/experimental_vector/vector") {
    return nullptr;
  }
  if (const Definition *rawDef = resolveDefinitionByPath(defMap, rawPath);
      rawDef != nullptr) {
    return rawDef;
  }
  const std::string specializedPrefix = rawPath + "__t";
  const std::string overloadPrefix = rawPath + "__ov";
  for (const auto &[path, def] : defMap) {
    if (def == nullptr) {
      continue;
    }
    if (path.rfind(specializedPrefix, 0) == 0 ||
        path.rfind(overloadPrefix, 0) == 0) {
      return def;
    }
  }
  return nullptr;
}

const Definition *preferExplicitRootedMapAliasDefinition(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  if (!isExactRootedMapAliasDefinitionCall(expr)) {
    return nullptr;
  }
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);
  if (const Definition *rawDef = resolveDefinitionByPath(defMap, rawPath);
      rawDef != nullptr) {
    return rawDef;
  }
  const std::string specializedPrefix = rawPath + "__t";
  const std::string overloadPrefix = rawPath + "__ov";
  for (const auto &[path, def] : defMap) {
    if (def == nullptr) {
      continue;
    }
    if (path.rfind(specializedPrefix, 0) == 0 ||
        path.rfind(overloadPrefix, 0) == 0) {
      return def;
    }
  }
  return nullptr;
}

std::string resolveCallPathWithoutSemanticFallbackProbes(const Expr &expr) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return "/" + expr.name;
}

std::string describeCallSite(const std::string &scopePath, const Expr &expr) {
  std::string displayName;
  if (!expr.name.empty() && expr.name.front() == '/') {
    displayName = expr.name;
  } else if (!expr.namespacePrefix.empty()) {
    displayName = expr.namespacePrefix;
    if (!displayName.empty() && displayName.front() != '/') {
      displayName.insert(displayName.begin(), '/');
    }
    displayName += "/" + expr.name;
  } else if (!expr.name.empty()) {
    displayName = expr.name;
  } else {
    displayName = "<unnamed>";
  }
  return scopePath + " -> " + displayName;
}

} // namespace

const Definition *resolveDefinitionCall(const Expr &callExpr,
                                        const std::unordered_map<std::string, const Definition *> &defMap,
                                        const ResolveExprPathFn &resolveExprPath,
                                        const SemanticProgram *semanticProgram) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall ||
      !resolveExprPath) {
    return nullptr;
  }
  const std::string resolved = resolveExprPath(callExpr);
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(callExpr);
  if (const Definition *rawAliasDef =
          preferExplicitRootedMapAliasDefinition(callExpr, defMap);
      rawAliasDef != nullptr &&
      normalizeCollectionHelperPath(rawAliasDef->fullPath) !=
          normalizeCollectionHelperPath(resolved)) {
    return rawAliasDef;
  }
  if (const Definition *explicitExperimentalMapHelperDef =
          preferExplicitExperimentalMapHelperDefinition(callExpr, resolved, defMap);
      explicitExperimentalMapHelperDef != nullptr) {
    return explicitExperimentalMapHelperDef;
  }
  if (const Definition *explicitExperimentalVectorHelperDef =
          preferExplicitExperimentalVectorHelperDefinition(callExpr, resolved, defMap);
      explicitExperimentalVectorHelperDef != nullptr) {
    return explicitExperimentalVectorHelperDef;
  }
  if (const Definition *resolvedDef = resolveDefinitionByPath(defMap, resolved);
      resolvedDef != nullptr) {
    if (!isMapBuiltinResolvedPath(semanticProgram, callExpr, resolved)) {
      return resolvedDef;
    }
    if (isExplicitExperimentalMapHelperDefinitionCall(callExpr, resolved)) {
      return resolvedDef;
    }
    if (isExplicitSamePathPublishedMapHelperCall(callExpr, resolved)) {
      return resolvedDef;
    }
    if (isSemanticBarePublishedMapHelperCall(callExpr, semanticProgram, resolved)) {
      return resolvedDef;
    }
    const std::string scopedCallPath =
        resolveScopedCallPathForHelperMatching(callExpr);
    if (isExplicitMapContainsOrTryAtMethodPath(scopedCallPath) &&
        normalizeCollectionHelperPath(scopedCallPath) ==
            normalizeCollectionHelperPath(resolved)) {
      return resolvedDef;
    }
    return nullptr;
  }
  const bool hasSemanticRootedRewrite =
      callExpr.semanticNodeId != 0 &&
      !rawPath.empty() &&
      !resolved.empty() &&
      rawPath.front() == '/' &&
      resolved.front() == '/' &&
      rawPath != resolved;
  const size_t rawLeafStart = rawPath.find_last_of('/');
  const bool hasGeneratedRootedRawPath =
      !rawPath.empty() &&
      rawPath.front() == '/' &&
      (rawPath.find("__t", rawLeafStart == std::string::npos ? 0 : rawLeafStart + 1) !=
           std::string::npos ||
       rawPath.find("__ov", rawLeafStart == std::string::npos ? 0 : rawLeafStart + 1) !=
           std::string::npos);
  if (!isMapBuiltinResolvedPath(semanticProgram, callExpr, resolved) &&
      rawPath != resolved &&
      (!hasSemanticRootedRewrite || hasGeneratedRootedRawPath)) {
    if (const Definition *rawDef = resolveDefinitionByPath(defMap, rawPath);
        rawDef != nullptr) {
      return rawDef;
    }
  }
  if (isMapBuiltinResolvedPath(semanticProgram, callExpr, resolved)) {
    return nullptr;
  }
  return nullptr;
}

ResolveDefinitionCallFn makeResolveDefinitionCall(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath,
    const SemanticProgram *semanticProgram) {
  return [defMap, resolveExprPath, semanticProgram](const Expr &expr) {
    return resolveDefinitionCall(expr, defMap, resolveExprPath, semanticProgram);
  };
}

CallResolutionAdapters makeCallResolutionAdapters(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return makeCallResolutionAdapters(defMap, importAliases, nullptr);
}

CallResolutionAdapters makeCallResolutionAdapters(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram) {
  CallResolutionAdapters adapters;
  adapters.semanticProgram = semanticProgram;
  adapters.semanticProductTargets = buildSemanticProductTargetAdapter(semanticProgram);
  adapters.resolveExprPath = makeResolveCallPathFromScope(
      defMap, importAliases, adapters.semanticProgram);
  adapters.isTailCallCandidate = makeIsTailCallCandidate(defMap, adapters.resolveExprPath);
  adapters.definitionExists = makeDefinitionExistsByPath(defMap);
  return adapters;
}

bool validateSemanticProductDirectCallCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramBridgePathChoice *> bridgePathChoicesByExpr;
  bridgePathChoicesByExpr.reserve(bridgePathChoices.size());
  for (const auto *entry : bridgePathChoices) {
    if (entry->semanticNodeId != 0) {
      bridgePathChoicesByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
  const auto directCallTargets = semanticProgramDirectCallTargetView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramDirectCallTarget *> directCallTargetsByExpr;
  directCallTargetsByExpr.reserve(directCallTargets.size());
  for (const auto *entry : directCallTargets) {
    if (entry->semanticNodeId != 0) {
      directCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      if (expr.semanticNodeId == 0) {
        error = "missing semantic-product direct-call semantic id: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      if (bridgePathChoicesByExpr.contains(expr.semanticNodeId)) {
        return validateExprs(scopePath, expr.args) &&
               validateExprs(scopePath, expr.bodyArguments);
      }
      if (const auto it = directCallTargetsByExpr.find(expr.semanticNodeId);
          it != directCallTargetsByExpr.end()) {
        const auto &entry = *it->second;
        const std::string siteDescription = describeCallSite(scopePath, expr);
        const std::string_view resolvedPath =
            semanticProgramDirectCallTargetResolvedPath(*semanticProgram, entry);
        if (entry.resolvedPathId == InvalidSymbolId || resolvedPath.empty()) {
          error = "missing semantic-product direct-call resolved path id: " +
                  siteDescription;
          return false;
        }
        if (entry.scopePathId != InvalidSymbolId) {
          const std::string_view resolvedScope =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.scopePathId);
          if (resolvedScope.empty()) {
            error = "missing semantic-product direct-call scope id: " +
                    siteDescription;
            return false;
          }
          if (resolvedScope != entry.scopePath) {
            error = "stale semantic-product direct-call scope metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.callNameId != InvalidSymbolId) {
          const std::string_view resolvedCallName =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.callNameId);
          if (resolvedCallName.empty()) {
            error = "missing semantic-product direct-call name id: " +
                    siteDescription;
            return false;
          }
          if (resolvedCallName != entry.callName) {
            error = "stale semantic-product direct-call name metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (const auto publishedTargetId =
                semanticProgramLookupPublishedDirectCallTargetId(*semanticProgram,
                                                                 expr.semanticNodeId);
            publishedTargetId.has_value()) {
          const std::string_view publishedTarget =
              semanticProgramResolveCallTargetString(*semanticProgram, *publishedTargetId);
          if (publishedTarget.empty()) {
            error = "missing semantic-product direct-call resolved path id: " +
                    siteDescription;
            return false;
          }
          if (publishedTarget != resolvedPath) {
            error = "stale semantic-product direct-call target metadata: " +
                    siteDescription;
            return false;
          }
        }
      }
      if (findSemanticProductBridgePathChoice(semanticProgram, expr).empty() &&
          findSemanticProductDirectCallTarget(semanticProgram, expr).empty()) {
        const std::string resolvedPath = resolveCallPathFromPublishedLookups(expr, semanticProgram);
        if (!isBridgeHelperCall(semanticProgram, expr, resolvedPath) &&
            !resolvesToPublishedDefinitionFamilyTarget(semanticProgram, resolvedPath)) {
          return validateExprs(scopePath, expr.args) &&
                 validateExprs(scopePath, expr.bodyArguments);
        }
        error = "missing semantic-product direct-call target: " +
                describeCallSite(scopePath, expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.parameters) ||
        !validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateExprs(exec.fullPath, exec.arguments) ||
        !validateExprs(exec.fullPath, exec.bodyArguments)) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductBridgePathCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramBridgePathChoice *> bridgePathChoicesByExpr;
  bridgePathChoicesByExpr.reserve(bridgePathChoices.size());
  for (const auto *entry : bridgePathChoices) {
    if (entry->semanticNodeId != 0) {
      bridgePathChoicesByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      if (const auto it = bridgePathChoicesByExpr.find(expr.semanticNodeId);
          it != bridgePathChoicesByExpr.end()) {
        const auto &entry = *it->second;
        const std::string siteDescription = describeCallSite(scopePath, expr);
        const std::string_view helperName =
            semanticProgramBridgePathChoiceHelperName(*semanticProgram, entry);
        if (entry.helperNameId == InvalidSymbolId || helperName.empty()) {
          error = "missing semantic-product bridge helper name id: " +
                  siteDescription;
          return false;
        }
        const std::string_view chosenPath =
            semanticProgramResolveCallTargetString(*semanticProgram, entry.chosenPathId);
        if (entry.chosenPathId == InvalidSymbolId || chosenPath.empty()) {
          error = "missing semantic-product bridge chosen path id: " +
                  siteDescription;
          return false;
        }
        if (entry.scopePathId != InvalidSymbolId) {
          const std::string_view resolvedScope =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.scopePathId);
          if (resolvedScope.empty()) {
            error = "missing semantic-product bridge scope id: " +
                    siteDescription;
            return false;
          }
          if (resolvedScope != entry.scopePath) {
            error = "stale semantic-product bridge scope metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.collectionFamilyId != InvalidSymbolId) {
          const std::string_view resolvedCollectionFamily =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.collectionFamilyId);
          if (resolvedCollectionFamily.empty()) {
            error = "missing semantic-product bridge collection family id: " +
                    siteDescription;
            return false;
          }
          if (resolvedCollectionFamily != entry.collectionFamily) {
            error = "stale semantic-product bridge collection family metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (const auto publishedChoiceId =
                semanticProgramLookupPublishedBridgePathChoiceId(*semanticProgram,
                                                                 expr.semanticNodeId);
            publishedChoiceId.has_value()) {
          const std::string_view publishedChoice =
              semanticProgramResolveCallTargetString(*semanticProgram, *publishedChoiceId);
          if (publishedChoice.empty()) {
            error = "missing semantic-product bridge chosen path id: " +
                    siteDescription;
            return false;
          }
          if (publishedChoice != chosenPath) {
            error = "stale semantic-product bridge target metadata: " +
                    siteDescription;
            return false;
          }
        }
      }
      if (!findSemanticProductBridgePathChoice(semanticProgram, expr).empty()) {
        return validateExprs(scopePath, expr.args) &&
               validateExprs(scopePath, expr.bodyArguments);
      }
      if (isPublishedCollectionBridgeStdlibSurfaceId(
              findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr))) {
        error = "missing semantic-product bridge-path choice: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProgram, expr);
          isResidualBridgeHelperPath(resolvedPath)) {
        error = "missing semantic-product bridge-path choice: " +
                describeCallSite(scopePath, expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.parameters) ||
        !validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateExprs(exec.fullPath, exec.arguments) ||
        !validateExprs(exec.fullPath, exec.bodyArguments)) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductMethodCallCoverage(const Program &program,
                                               const SemanticProgram *semanticProgram,
                                               std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const auto methodCallTargets = semanticProgramMethodCallTargetView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramMethodCallTarget *> methodCallTargetsByExpr;
  methodCallTargetsByExpr.reserve(methodCallTargets.size());
  for (const auto *entry : methodCallTargets) {
    if (entry->semanticNodeId != 0) {
      methodCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && expr.isMethodCall) {
      if (expr.semanticNodeId == 0) {
        error = "missing semantic-product method-call semantic id: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      if (const auto it = methodCallTargetsByExpr.find(expr.semanticNodeId);
          it != methodCallTargetsByExpr.end()) {
        const auto &entry = *it->second;
        const std::string siteDescription = describeCallSite(scopePath, expr);
        const std::string_view resolvedPath =
            semanticProgramMethodCallTargetResolvedPath(*semanticProgram, entry);
        if (entry.resolvedPathId == InvalidSymbolId || resolvedPath.empty()) {
          error = "missing semantic-product method-call resolved path id: " +
                  siteDescription;
          return false;
        }
        if (entry.scopePathId != InvalidSymbolId) {
          const std::string_view resolvedScope =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.scopePathId);
          if (resolvedScope.empty()) {
            error = "missing semantic-product method-call scope id: " +
                    siteDescription;
            return false;
          }
          if (resolvedScope != entry.scopePath) {
            error = "stale semantic-product method-call scope metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.methodNameId != InvalidSymbolId) {
          const std::string_view resolvedMethodName =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.methodNameId);
          if (resolvedMethodName.empty()) {
            error = "missing semantic-product method-call name id: " +
                    siteDescription;
            return false;
          }
          if (resolvedMethodName != entry.methodName) {
            error = "stale semantic-product method-call name metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.receiverTypeTextId != InvalidSymbolId) {
          const std::string_view resolvedReceiver =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.receiverTypeTextId);
          if (resolvedReceiver.empty()) {
            error = "missing semantic-product method-call receiver type id: " +
                    siteDescription;
            return false;
          }
          if (resolvedReceiver != entry.receiverTypeText) {
            error = "stale semantic-product method-call receiver metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (const auto publishedTargetId =
                semanticProgramLookupPublishedMethodCallTargetId(*semanticProgram,
                                                                 expr.semanticNodeId);
            publishedTargetId.has_value()) {
          const std::string_view publishedTarget =
              semanticProgramResolveCallTargetString(*semanticProgram, *publishedTargetId);
          if (publishedTarget.empty()) {
            error = "missing semantic-product method-call resolved path id: " +
                    siteDescription;
            return false;
          }
          if (publishedTarget != resolvedPath) {
            error = "stale semantic-product method-call target metadata: " +
                    siteDescription;
            return false;
          }
        }
      }
      if (findSemanticProductMethodCallTarget(semanticProgram, expr).empty()) {
        error = "missing semantic-product method-call target: " +
                describeCallSite(scopePath, expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.parameters) ||
        !validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateExprs(exec.fullPath, exec.arguments) ||
        !validateExprs(exec.fullPath, exec.bodyArguments)) {
      return false;
    }
  }

  return true;
}

EntryCallResolutionSetup buildEntryCallResolutionSetup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return buildEntryCallResolutionSetup(entryDef,
                                       definitionReturnsVoid,
                                       defMap,
                                       importAliases,
                                       nullptr);
}

EntryCallResolutionSetup buildEntryCallResolutionSetup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram) {
  EntryCallResolutionSetup setup{};
  setup.adapters = makeCallResolutionAdapters(defMap, importAliases, semanticProgram);
  setup.hasTailExecution = hasTailExecutionCandidate(
      entryDef.statements, definitionReturnsVoid, setup.adapters.isTailCallCandidate);
  return setup;
}

ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return makeResolveCallPathFromScope(defMap, importAliases, nullptr);
}

ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram) {
  return [defMap, importAliases, semanticProgram](const Expr &expr) {
    if (!expr.isMethodCall) {
      if (const std::string chosenPath =
              findSemanticProductBridgePathChoice(semanticProgram, expr);
          !chosenPath.empty()) {
        return chosenPath;
      }
    }
    if (semanticProgram != nullptr && expr.kind == Expr::Kind::Call) {
      if (expr.isMethodCall) {
        if (const std::string resolvedPath =
                findSemanticProductMethodCallTarget(semanticProgram, expr);
            !resolvedPath.empty()) {
          return resolvedPath;
        }
        return resolveCallPathWithoutSemanticFallbackProbes(expr);
      }
      if (const std::string resolvedPath =
              findSemanticProductDirectCallTarget(semanticProgram, expr);
          !resolvedPath.empty() &&
          !isPublishedCollectionBridgeStdlibSurfaceId(
              findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)) &&
          !isResidualBridgeHelperPath(resolvedPath)) {
        return resolvedPath;
      }
      return resolveCallPathWithoutSemanticFallbackProbes(expr);
    }
    if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProgram, expr);
        !resolvedPath.empty()) {
      return resolvedPath;
    }
    if (semanticProgram != nullptr) {
      return resolveCallPathWithoutSemanticFallbackProbes(expr);
    }
    return resolveCallPathFromScope(expr, defMap, importAliases);
  };
}

IsTailCallCandidateFn makeIsTailCallCandidate(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath) {
  return [defMap, resolveExprPath](const Expr &expr) {
    return isTailCallCandidate(expr, defMap, resolveExprPath);
  };
}

DefinitionExistsFn makeDefinitionExistsByPath(
    const std::unordered_map<std::string, const Definition *> &defMap) {
  return [defMap](const std::string &path) {
    return resolveDefinitionByPath(defMap, path) != nullptr;
  };
}

std::string resolveCallPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    scoped += "/" + expr.name;
    if (defMap.count(scoped) > 0) {
      return scoped;
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return normalizeMapImportAliasPath(importIt->second);
    }
    const std::string root = "/" + expr.name;
    if (defMap.count(root) > 0) {
      return root;
    }
    return scoped;
  }
  auto importIt = importAliases.find(expr.name);
  if (importIt != importAliases.end()) {
    return normalizeMapImportAliasPath(importIt->second);
  }
  return "/" + expr.name;
}

bool isTailCallCandidate(const Expr &expr,
                         const std::unordered_map<std::string, const Definition *> &defMap,
                         const ResolveExprPathFn &resolveExprPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || !resolveExprPath) {
    return false;
  }
  const std::string targetPath = resolveExprPath(expr);
  return resolveDefinitionByPath(defMap, targetPath) != nullptr;
}

bool hasTailExecutionCandidate(const std::vector<Expr> &statements,
                               bool definitionReturnsVoid,
                               const IsTailCallCandidateFn &isTailCallCandidateFn) {
  if (statements.empty() || !isTailCallCandidateFn) {
    return false;
  }
  const Expr &lastStmt = statements.back();
  if (isReturnCall(lastStmt) && lastStmt.args.size() == 1) {
    return isTailCallCandidateFn(lastStmt.args.front());
  }
  return definitionReturnsVoid && isTailCallCandidateFn(lastStmt);
}

} // namespace primec::ir_lowerer
