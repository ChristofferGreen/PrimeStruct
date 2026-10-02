#pragma once

// Internal helpers shared by the SemanticsValidatorSnapshots*.cpp units (split
// out of SemanticsValidatorSnapshots.cpp without changes, TODO-5384).
#include "SemanticsValidator.h"

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "SemanticsWorkerSymbolMerge.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace primec::semantics::snapshot_detail {

inline bool isSemanticCollectorEnabled(const SemanticProductBuildConfig *buildConfig,
                                std::string_view collectorFamily) {
  if (buildConfig == nullptr) {
    return true;
  }
  if (buildConfig->disableAllCollectors) {
    return false;
  }
  if (!buildConfig->collectorAllowlistSpecified) {
    return true;
  }
  return std::find(buildConfig->collectorAllowlist.begin(),
                   buildConfig->collectorAllowlist.end(),
                   collectorFamily) != buildConfig->collectorAllowlist.end();
}

inline std::string fallbackSnapshotBindingResolvedPath(std::string_view scopePath,
                                                std::string_view bindingName) {
  if (scopePath.empty() || bindingName.empty()) {
    return {};
  }
  if (bindingName.front() == '/') {
    return std::string(bindingName);
  }
  std::string normalizedScope(scopePath);
  if (!normalizedScope.empty() && normalizedScope.front() != '/') {
    normalizedScope.insert(normalizedScope.begin(), '/');
  }
  if (!normalizedScope.empty() && normalizedScope.back() != '/') {
    normalizedScope.push_back('/');
  }
  normalizedScope.append(bindingName);
  return normalizedScope;
}

inline std::string snapshotBindingTypeText(const BindingInfo &binding) {
  if (binding.typeName.empty()) {
    return {};
  }
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

template <typename Entry, typename PathForEntry>
void appendEntriesForDefinitionPaths(std::vector<Entry> &out,
                                     std::vector<Entry> entries,
                                     const std::unordered_set<std::string> &definitionPaths,
                                     PathForEntry pathForEntry) {
  for (auto &entry : entries) {
    if (definitionPaths.count(pathForEntry(entry)) == 0) {
      continue;
    }
    out.push_back(std::move(entry));
  }
}

template <typename Entry, typename KeyForEntry>
void rebindSemanticNodeIdsBySnapshotKey(std::vector<Entry> &entries,
                                        std::vector<Entry> freshEntries,
                                        KeyForEntry keyForEntry) {
  std::unordered_map<std::string, std::vector<uint64_t>> semanticNodeIdsByKey;
  semanticNodeIdsByKey.reserve(freshEntries.size());
  for (const auto &entry : freshEntries) {
    semanticNodeIdsByKey[keyForEntry(entry)].push_back(entry.semanticNodeId);
  }

  std::unordered_map<std::string, std::size_t> nextSemanticNodeIdByKey;
  nextSemanticNodeIdByKey.reserve(semanticNodeIdsByKey.size());
  for (auto &entry : entries) {
    const std::string key = keyForEntry(entry);
    const auto idIt = semanticNodeIdsByKey.find(key);
    if (idIt != semanticNodeIdsByKey.end()) {
      std::size_t &nextIndex = nextSemanticNodeIdByKey[key];
      if (nextIndex < idIt->second.size()) {
        entry.semanticNodeId = idIt->second[nextIndex++];
      }
    }
  }
}

template <typename Entry, typename KeyForEntry>
bool appendMissingEntriesBySnapshotKey(std::vector<Entry> &entries,
                                       const std::vector<Entry> &freshEntries,
                                       KeyForEntry keyForEntry) {
  std::unordered_map<std::string, std::size_t> remainingEntriesByKey;
  remainingEntriesByKey.reserve(entries.size());
  for (const auto &entry : entries) {
    ++remainingEntriesByKey[keyForEntry(entry)];
  }

  bool appended = false;
  for (const auto &entry : freshEntries) {
    std::size_t &remainingCount = remainingEntriesByKey[keyForEntry(entry)];
    if (remainingCount > 0) {
      --remainingCount;
      continue;
    }
    entries.push_back(entry);
    appended = true;
  }
  return appended;
}

template <typename Entry, typename IdentityKeyForEntry, typename SnapshotKeyForEntry>
bool pruneReplacedEntriesBySnapshotKey(std::vector<Entry> &entries,
                                       const std::vector<Entry> &freshEntries,
                                       IdentityKeyForEntry identityKeyForEntry,
                                       SnapshotKeyForEntry snapshotKeyForEntry) {
  std::unordered_set<std::string> freshIdentities;
  std::unordered_set<std::string> freshSnapshotKeys;
  freshIdentities.reserve(freshEntries.size());
  freshSnapshotKeys.reserve(freshEntries.size());
  for (const auto &entry : freshEntries) {
    freshIdentities.insert(identityKeyForEntry(entry));
    freshSnapshotKeys.insert(snapshotKeyForEntry(entry));
  }

  const auto oldSize = entries.size();
  entries.erase(
      std::remove_if(entries.begin(),
                     entries.end(),
                     [&](const Entry &entry) {
                       const std::string identityKey = identityKeyForEntry(entry);
                       if (freshIdentities.count(identityKey) == 0) {
                         return false;
                       }
                       return freshSnapshotKeys.count(snapshotKeyForEntry(entry)) == 0;
                     }),
      entries.end());
  return entries.size() != oldSize;
}

inline std::string snapshotKey(std::string_view first,
                        std::string_view second,
                        int sourceLine,
                        int sourceColumn,
                        std::string_view third = {},
                        std::string_view fourth = {},
                        std::string_view fifth = {}) {
  return std::string(first) + "\x1f" + std::string(second) + "\x1f" +
         std::to_string(sourceLine) + "\x1f" + std::to_string(sourceColumn) +
         "\x1f" + std::string(third) + "\x1f" + std::string(fourth) +
         "\x1f" + std::string(fifth);
}

struct SnapshotArrayExtentShape {
  std::string elementTypeText;
  bool isReference = false;
};

inline bool classifySnapshotArrayExtentBinding(const BindingInfo &binding,
                                        SnapshotArrayExtentShape &shapeOut) {
  shapeOut = {};
  std::string typeText = normalizeBindingTypeName(snapshotBindingTypeText(binding));
  auto normalizeArrayExtentBase = [](std::string base) {
    base = normalizeBindingTypeName(std::move(base));
    return base == "args" ? std::string("array") : base;
  };
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText)) {
      return false;
    }
    base = normalizeArrayExtentBase(std::move(base));
    if (base == "Reference") {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      shapeOut.isReference = true;
      typeText = normalizeBindingTypeName(args.front());
      continue;
    }
    if (base != "array") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    shapeOut.elementTypeText = normalizeBindingTypeName(args.front());
    return true;
  }
}

inline const Expr *arrayExtentCountTarget(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.args.empty()) {
    return nullptr;
  }
  std::string_view name = expr.name;
  if (const std::size_t slash = name.find_last_of('/'); slash != std::string_view::npos) {
    name.remove_prefix(slash + 1);
  }
  if (name != "count") {
    return nullptr;
  }
  return &expr.args.front();
}

inline std::optional<int64_t> snapshotSignedLiteralValue(const Expr &expr) {
  if (expr.kind != Expr::Kind::Literal || expr.isUnsigned) {
    return std::nullopt;
  }
  if (expr.intWidth == 64) {
    return static_cast<int64_t>(expr.literalValue);
  }
  return static_cast<int32_t>(expr.literalValue);
}

inline std::string snapshotExtentExprText(const Expr &expr) {
  if (expr.kind == Expr::Kind::Name) {
    return expr.name;
  }
  if (const auto literal = snapshotSignedLiteralValue(expr)) {
    return std::to_string(*literal);
  }
  return "?";
}

inline std::optional<std::size_t> staticArrayExtentForBindingInitializer(
    const std::string &scopePath,
    const Expr &bindingExpr,
    const std::unordered_map<std::string, std::size_t> &staticExtentsByResolvedPath,
    std::string &extentExpressionOut) {
  extentExpressionOut.clear();
  if (bindingExpr.args.size() != 1) {
    return std::nullopt;
  }
  const Expr &initializer = bindingExpr.args.front();
  std::string collectionName;
  if (getBuiltinCollectionName(initializer, collectionName) &&
      collectionName == "array") {
    return initializer.args.size();
  }
  if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
      !isSimpleCallName(initializer, "slice") || initializer.args.size() != 3 ||
      initializer.args.front().kind != Expr::Kind::Name) {
    return std::nullopt;
  }
  const Expr &startExpr = initializer.args[1];
  const Expr &endExpr = initializer.args[2];
  extentExpressionOut =
      snapshotExtentExprText(endExpr) + " - " + snapshotExtentExprText(startExpr);
  const auto start = snapshotSignedLiteralValue(startExpr);
  const auto end = snapshotSignedLiteralValue(endExpr);
  if (!start.has_value() || !end.has_value() || *end < *start) {
    return std::nullopt;
  }
  const std::string targetResolvedPath =
      fallbackSnapshotBindingResolvedPath(scopePath, initializer.args.front().name);
  const auto targetExtent = staticExtentsByResolvedPath.find(targetResolvedPath);
  if (targetExtent == staticExtentsByResolvedPath.end() ||
      *start < 0 ||
      static_cast<std::size_t>(*end) > targetExtent->second) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(*end - *start);
}

inline void collectStaticArrayExtentsForBindings(
    const std::string &scopePath,
    const std::vector<Expr> &exprs,
    std::unordered_map<std::string, std::size_t> &staticExtentsByResolvedPath,
    std::unordered_map<std::string, std::string> &extentExpressionsByResolvedPath) {
  for (const Expr &expr : exprs) {
    if (expr.isBinding) {
      std::string extentExpression;
      const std::optional<std::size_t> extent =
          staticArrayExtentForBindingInitializer(
              scopePath, expr, staticExtentsByResolvedPath, extentExpression);
      if (extent.has_value() || !extentExpression.empty()) {
        const std::string resolvedPath =
            fallbackSnapshotBindingResolvedPath(scopePath, expr.name);
        if (extent.has_value()) {
          staticExtentsByResolvedPath.insert_or_assign(resolvedPath, *extent);
        }
        if (!extentExpression.empty()) {
          extentExpressionsByResolvedPath.insert_or_assign(
              resolvedPath, std::move(extentExpression));
        }
      }
    }
    collectStaticArrayExtentsForBindings(scopePath,
                                         expr.args,
                                         staticExtentsByResolvedPath,
                                         extentExpressionsByResolvedPath);
    collectStaticArrayExtentsForBindings(scopePath,
                                         expr.bodyArguments,
                                         staticExtentsByResolvedPath,
                                         extentExpressionsByResolvedPath);
  }
}

inline bool matchesStdlibSurfaceMetadata(const StdlibSurfaceMetadata &metadata,
                                  const StdlibSurfaceMetadata *expectedMetadata) {
  return expectedMetadata != nullptr && &metadata == expectedMetadata;
}

inline bool isMapCollectionSurfaceMetadata(const StdlibSurfaceMetadata &metadata) {
  return matchesStdlibSurfaceMetadata(metadata, keyValueHelperSurfaceMetadataLocal()) ||
         matchesStdlibSurfaceMetadata(metadata,
                                      keyValueConstructorSurfaceMetadataLocal());
}

inline std::optional<std::pair<std::string, std::string>>
collectionBridgeChoiceFromResolvedPath(const std::string &resolvedPath) {
  auto stripSpecializationSuffix = [](std::string_view path) {
    const std::size_t lastSlash = path.rfind('/');
    const std::size_t marker = path.rfind("__t");
    if (marker == std::string_view::npos || lastSlash == std::string_view::npos ||
        marker <= lastSlash) {
      return path;
    }
    return path.substr(0, marker);
  };
  const std::string_view normalizedResolvedPath =
      stripSpecializationSuffix(resolvedPath);
  if (isInternalSoaCollectionTypePath(normalizedResolvedPath)) {
    const std::string family = internalSoaCollectionTypeName();
    return std::pair<std::string, std::string>(family, family);
  }

  const StdlibSurfaceMetadata *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  if (metadata == nullptr) {
    auto resolveSoaHelperName = [&](std::string_view path) -> std::string_view {
      const std::string_view normalizedPath = stripSpecializationSuffix(path);
      auto matchCanonicalPrefix = [&](std::string_view prefix) -> std::string_view {
        if (!normalizedPath.starts_with(prefix)) {
          return {};
        }
        const std::string_view helperName = normalizedPath.substr(prefix.size());
        if (isSupportedCompatibilitySoaHelperName(helperName)) {
          return helperName;
        }
        return {};
      };
      if (const std::string_view helperName =
              matchCanonicalPrefix(samePathSoaHelperTargetPath(""));
          !helperName.empty()) {
        return helperName;
      }
      if (const std::string_view helperName =
              matchCanonicalPrefix(compatibilitySoaHelperTargetPath(""));
          !helperName.empty()) {
        return helperName;
      }
      const std::string experimentalSoaPrefix =
          experimentalSoaStorageTypePath(true) + "/";
      if (normalizedPath.starts_with(experimentalSoaPrefix)) {
        const std::string_view helperName = normalizedPath.substr(
            experimentalSoaPrefix.size());
        if (helperName == "soaVectorCount") {
          return "count";
        }
        if (helperName == "soaVectorCountRef") {
          return collection_helpers::kCountRef;
        }
        if (helperName == "soaVectorGet") {
          return "get";
        }
        if (helperName == "soaVectorGetRef") {
          return collection_helpers::kGetRef;
        }
        if (helperName == "soaVectorRef") {
          return "ref";
        }
        if (helperName == "soaVectorRefRef") {
          return collection_helpers::kRefRef;
        }
        if (helperName == "soaVectorPush") {
          return "push";
        }
        if (helperName == "soaVectorReserve") {
          return "reserve";
        }
      }
      const std::string experimentalSoaConversionsPrefix =
          "/std/collections/" + std::string("experimental") + "_" +
          "soa" + "_" + "vector" + "_conversions/";
      if (normalizedPath.starts_with(experimentalSoaConversionsPrefix)) {
        const std::string_view helperName = normalizedPath.substr(
            experimentalSoaConversionsPrefix.size());
        if (helperName == "soaVectorToAos") {
          return "to_aos";
        }
        if (helperName == "soaVectorToAosRef") {
          return collection_helpers::kToAosRef;
        }
      }
      return {};
    };

    if (const std::string_view helperName = resolveSoaHelperName(resolvedPath);
        !helperName.empty()) {
      return std::pair<std::string, std::string>(
          internalSoaCollectionTypeName(), std::string(helperName));
    }
    return std::nullopt;
  }

  std::string collectionFamily;
  if (metadata == vectorHelperSurfaceMetadata() ||
      metadata == vectorConstructorSurfaceMetadata()) {
    collectionFamily = "vector";
  } else if (isMapCollectionSurfaceMetadata(*metadata)) {
    collectionFamily = "map";
  } else {
    switch (metadata->id) {
      case StdlibSurfaceId::CollectionsColumnarHelpers:
      case StdlibSurfaceId::CollectionsColumnarConstructors:
        collectionFamily = internalSoaCollectionTypeName();
        break;
      default:
        return std::nullopt;
    }
  }

  const std::string_view helperName = resolveStdlibSurfaceMemberName(*metadata, resolvedPath);
  if (helperName.empty()) {
    return std::nullopt;
  }

  return std::pair<std::string, std::string>(std::move(collectionFamily),
                                             std::string(helperName));
}

template <typename ResolveFn, typename VisitorFn>
void forEachResolvedNonMethodCallSnapshot(const Program &program,
                                          ResolveFn &&resolvePath,
                                          VisitorFn &&visitor) {
  std::function<void(const std::string &, const Expr &)> visitExpr;
  auto visitExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      visitExpr(scopePath, expr);
    }
  };

  visitExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      std::string resolvedPath = resolvePath(expr);
      if (!resolvedPath.empty()) {
        visitor(scopePath, expr, std::move(resolvedPath));
      }
    }
    visitExprs(scopePath, expr.args);
    visitExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    visitExprs(def.fullPath, def.parameters);
    visitExprs(def.fullPath, def.statements);
    if (def.returnExpr.has_value()) {
      visitExpr(def.fullPath, *def.returnExpr);
    }
  }

  for (const auto &exec : program.executions) {
    visitExprs(exec.fullPath, exec.arguments);
    visitExprs(exec.fullPath, exec.bodyArguments);
  }
}

inline std::vector<std::string> snapshotCapabilities(const std::vector<Transform> &transforms) {
  std::vector<std::string> capabilities;
  for (const auto &transform : transforms) {
    if (transform.name != "capabilities") {
      continue;
    }
    capabilities = transform.arguments;
    break;
  }
  std::sort(capabilities.begin(), capabilities.end());
  capabilities.erase(std::unique(capabilities.begin(), capabilities.end()), capabilities.end());
  return capabilities;
}

inline bool isStaticFieldStatement(const Expr &stmt) {
  for (const auto &transform : stmt.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

inline bool hasSumTransformSnapshot(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name == "sum") {
      return true;
    }
  }
  return false;
}

inline bool parsePositiveIntSnapshotArg(const std::string &text, uint32_t &valueOut) {
  std::string digits = text;
  if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
    digits.resize(digits.size() - 3);
  }
  if (digits.empty()) {
    return false;
  }
  uint64_t value = 0;
  for (char c : digits) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
    value = value * 10u + static_cast<uint64_t>(c - '0');
    if (value > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
  }
  valueOut = static_cast<uint32_t>(value);
  return valueOut > 0;
}

inline bool explicitAlignmentBytesForSnapshot(const std::vector<Transform> &transforms, uint32_t &alignmentOut) {
  alignmentOut = 0;
  for (const auto &transform : transforms) {
    if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
      continue;
    }
    if (transform.arguments.empty()) {
      return false;
    }
    uint32_t value = 0;
    if (!parsePositiveIntSnapshotArg(transform.arguments.front(), value)) {
      return false;
    }
    if (transform.name == "align_kbytes") {
      if (value > std::numeric_limits<uint32_t>::max() / 1024u) {
        return false;
      }
      value *= 1024u;
    }
    alignmentOut = value;
    return true;
  }
  return false;
}

inline bool isImplicitStructDefinitionSnapshot(const Definition &def) {
  if (hasSumTransformSnapshot(def)) {
    return false;
  }
  bool hasStructTransform = false;
  bool hasReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturnTransform = true;
    }
    if (isStructTransformName(transform.name)) {
      hasStructTransform = true;
    }
  }
  if (hasStructTransform) {
    return true;
  }
  if (hasReturnTransform || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

inline std::string typeMetadataCategoryForSnapshot(const Definition &def) {
  if (hasSumTransformSnapshot(def)) {
    return "sum";
  }
  bool sawStructLikeTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "enum") {
      return "enum";
    }
    if (transform.name == "gpu_lane") {
      return "gpu_lane";
    }
    if (transform.name == "handle") {
      return "handle";
    }
    if (transform.name == "pod") {
      return "pod";
    }
    if (transform.name == "struct") {
      return "struct";
    }
    if (isStructTransformName(transform.name)) {
      sawStructLikeTransform = true;
    }
  }
  if (sawStructLikeTransform || isImplicitStructDefinitionSnapshot(def)) {
    return "struct";
  }
  return {};
}


} // namespace primec::semantics::snapshot_detail
