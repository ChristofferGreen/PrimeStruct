#pragma once

#include "SemanticPublicationSurface.h"

#include "primec/SymbolInterner.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace primec::semantics {

struct GraphLocalAutoKey {
  SymbolId scopePathId = InvalidSymbolId;
  int32_t sourceLine = 0;
  int32_t sourceColumn = 0;

  bool operator==(const GraphLocalAutoKey &other) const {
    return scopePathId == other.scopePathId && sourceLine == other.sourceLine &&
           sourceColumn == other.sourceColumn;
  }
};

struct GraphLocalAutoKeyHash {
  std::size_t operator()(const GraphLocalAutoKey &key) const {
    std::size_t hash = std::hash<SymbolId>{}(key.scopePathId);
    hash ^= std::hash<int32_t>{}(key.sourceLine) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    hash ^= std::hash<int32_t>{}(key.sourceColumn) + 0x9e3779b9 + (hash << 6) +
            (hash >> 2);
    return hash;
  }
};

struct GraphLocalAutoFacts {
  bool hasBinding = false;
  BindingInfo binding;
  std::string initializerResolvedPath;
  bool hasInitializerBinding = false;
  BindingInfo initializerBinding;
  bool hasQuerySnapshot = false;
  QuerySnapshotData querySnapshot;
  bool hasTryValue = false;
  LocalAutoTrySnapshotData tryValue;
  std::string directCallResolvedPath;
  bool hasDirectCallReturnKind = false;
  ReturnKind directCallReturnKind = ReturnKind::Unknown;
  std::string methodCallResolvedPath;
  bool hasMethodCallReturnKind = false;
  ReturnKind methodCallReturnKind = ReturnKind::Unknown;
};

class GraphLocalAutoBenchmarkShadow {
public:
  GraphLocalAutoBenchmarkShadow(bool legacyKeyShadowEnabled,
                                bool legacySideChannelShadowEnabled);

  void clear();
  void noteLegacyKey(std::string_view scopePath, int sourceLine, int sourceColumn);
  void rebuildFromFacts(
      const std::unordered_map<GraphLocalAutoKey, GraphLocalAutoFacts,
                               GraphLocalAutoKeyHash> &graphLocalAutoFacts);

  size_t legacyBindingCount() const;
  size_t legacyBindingBucketCount() const;
  size_t legacyQueryCount() const;
  size_t legacyQueryBucketCount() const;
  size_t legacyTryCount() const;
  size_t legacyTryBucketCount() const;

private:
  struct LegacyShadowState {
    std::unordered_set<std::string> keyShadow;
    std::unordered_map<GraphLocalAutoKey, BindingInfo, GraphLocalAutoKeyHash>
        bindingShadow;
    std::unordered_map<GraphLocalAutoKey, std::string, GraphLocalAutoKeyHash>
        initializerResolvedPathShadow;
    std::unordered_map<GraphLocalAutoKey, BindingInfo, GraphLocalAutoKeyHash>
        initializerBindingShadow;
    std::unordered_map<GraphLocalAutoKey, QuerySnapshotData, GraphLocalAutoKeyHash>
        querySnapshotShadow;
    std::unordered_map<GraphLocalAutoKey, LocalAutoTrySnapshotData,
                       GraphLocalAutoKeyHash>
        tryValueShadow;
    std::unordered_map<GraphLocalAutoKey, std::string, GraphLocalAutoKeyHash>
        directCallPathShadow;
    std::unordered_map<GraphLocalAutoKey, ReturnKind, GraphLocalAutoKeyHash>
        directCallReturnKindShadow;
    std::unordered_map<GraphLocalAutoKey, std::string, GraphLocalAutoKeyHash>
        methodCallPathShadow;
    std::unordered_map<GraphLocalAutoKey, ReturnKind, GraphLocalAutoKeyHash>
        methodCallReturnKindShadow;

    void clear();
  };

  bool legacyKeyShadowEnabled_ = false;
  bool legacySideChannelShadowEnabled_ = false;
  LegacyShadowState legacyState_;
};

} // namespace primec::semantics
