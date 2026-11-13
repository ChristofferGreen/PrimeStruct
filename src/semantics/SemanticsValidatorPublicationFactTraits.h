#pragma once

// One traits struct per publication fact kind, and the two templates that use them: the sort of a
// snapshot and the merge of a worker's facts into the merged set. A kind's traits say how its
// entries are keyed and ordered; the sequences themselves exist once.
//
// Every Traits has:
//   Entry                      the snapshot entry type
//   key(entry)                 the snapshot key; entries with equal keys are the same fact
//   order(entry)               a tuple of references: the order a snapshot is sorted in
// A kind that workers publish into the merged set also has:
//   MergeAppends = true        merge adds the fresh entries the merged set misses (false: the
//                              merge only rebinds semantic node ids)
//   mergeOrder(entry)          the order the merged set is re-sorted in after an append or prune
//   HasIdentityKey = true      merge first prunes entries a fresh entry replaces
//   identityKey(entry)         the key that finds the entry a fresh one replaces

#include "SemanticsValidatorSnapshotHelpers.h"

#include <algorithm>
#include <tuple>
#include <vector>

namespace primec::semantics::snapshot_detail {

// Stable-sorts `entries` by the tuple `order(entry)` returns, comparing it lexicographically.
template <typename Entry, typename Order>
void stableSortByOrder(std::vector<Entry> &entries, Order order) {
  std::stable_sort(entries.begin(), entries.end(), [&](const Entry &left, const Entry &right) {
    return order(left) < order(right);
  });
}

template <typename Traits> void sortSnapshotEntries(std::vector<typename Traits::Entry> &entries) {
  stableSortByOrder(entries, &Traits::order);
}

// Merges a worker pass's fresh facts into `merged`, then rebinds the semantic node ids of
// `merged` to the fresh ones that have the same key.
template <typename Traits>
void mergePublicationFactKind(std::vector<typename Traits::Entry> &merged,
                              std::vector<typename Traits::Entry> fresh) {
  if constexpr (Traits::MergeAppends) {
    bool pruned = false;
    if constexpr (Traits::HasIdentityKey) {
      pruned = pruneReplacedEntriesBySnapshotKey(merged, fresh, &Traits::identityKey, &Traits::key);
    }
    if (appendMissingEntriesBySnapshotKey(merged, fresh, &Traits::key) || pruned) {
      stableSortByOrder(merged, &Traits::mergeOrder);
    }
  }
  rebindSemanticNodeIdsBySnapshotKey(merged, std::move(fresh), &Traits::key);
}

struct DirectCallTargetFactTraits {
  using Entry = CollectedDirectCallTargetEntry;
  static constexpr bool MergeAppends = true;
  static constexpr bool HasIdentityKey = true;
  static std::string key(const Entry &entry) {
    return snapshotKey(
        entry.scopePath, entry.callName, entry.sourceLine, entry.sourceColumn, entry.resolvedPath);
  }
  static std::string identityKey(const Entry &entry) {
    return snapshotKey(entry.scopePath, entry.callName, entry.sourceLine, entry.sourceColumn);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.callName,
                    entry.resolvedPath,
                    entry.semanticNodeId);
  }
  static auto mergeOrder(const Entry &entry) {
    return order(entry);
  }
};

struct MethodCallTargetFactTraits {
  using Entry = CollectedMethodCallTargetEntry;
  static constexpr bool MergeAppends = true;
  static constexpr bool HasIdentityKey = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.scopePath,
                       entry.methodName,
                       entry.sourceLine,
                       entry.sourceColumn,
                       entry.resolvedPath);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.methodName,
                    entry.resolvedPath,
                    entry.semanticNodeId);
  }
  // The merge has never ordered by node id.
  static auto mergeOrder(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.methodName,
                    entry.resolvedPath);
  }
};

struct BridgePathChoiceFactTraits {
  using Entry = CollectedBridgePathChoiceEntry;
  static constexpr bool MergeAppends = true;
  static constexpr bool HasIdentityKey = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.scopePath,
                       entry.collectionFamily,
                       entry.sourceLine,
                       entry.sourceColumn,
                       entry.helperName,
                       entry.chosenPath);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.collectionFamily,
                    entry.helperName,
                    entry.chosenPath,
                    entry.semanticNodeId);
  }
  static auto mergeOrder(const Entry &entry) {
    return order(entry);
  }
};

struct TypeMetadataFactTraits {
  using Entry = TypeMetadataSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.fullPath, entry.category, 0, 0);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.fullPath, entry.category);
  }
};

struct StructFieldMetadataFactTraits {
  using Entry = StructFieldMetadataSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.structPath, entry.fieldName, 0, 0, std::to_string(entry.fieldIndex));
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.structPath, entry.fieldIndex, entry.fieldName);
  }
};

struct SumTypeMetadataFactTraits {
  using Entry = SumTypeMetadataSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.fullPath, std::string_view{}, 0, 0);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.fullPath);
  }
};

struct SumVariantMetadataFactTraits {
  using Entry = SumVariantMetadataSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.sumPath, entry.variantName, 0, 0, std::to_string(entry.variantIndex));
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.sumPath, entry.variantIndex, entry.variantName);
  }
};

struct BindingFactTraits {
  using Entry = BindingFactSnapshotEntry;
  static constexpr bool MergeAppends = true;
  static constexpr bool HasIdentityKey = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.scopePath,
                       entry.siteKind,
                       entry.sourceLine,
                       entry.sourceColumn,
                       entry.name,
                       entry.resolvedPath);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.siteKind,
                    entry.name,
                    entry.resolvedPath);
  }
  static auto mergeOrder(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.siteKind,
                    entry.name,
                    entry.resolvedPath,
                    entry.semanticNodeId);
  }
};

struct ArrayExtentFactTraits {
  using Entry = ArrayExtentFactSnapshotEntry;
  static constexpr bool MergeAppends = true;
  static constexpr bool HasIdentityKey = true;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.scopePath,
                       entry.siteKind,
                       entry.sourceLine,
                       entry.sourceColumn,
                       entry.targetName,
                       entry.targetResolvedPath,
                       entry.extentExpression);
  }
  static std::string identityKey(const Entry &entry) {
    return snapshotKey(
        entry.scopePath, entry.siteKind, entry.sourceLine, entry.sourceColumn, entry.targetName);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.siteKind,
                    entry.targetName,
                    entry.extentExpression,
                    entry.semanticNodeId);
  }
  static auto mergeOrder(const Entry &entry) {
    return order(entry);
  }
};

struct ReturnFactTraits {
  using Entry = ReturnFactSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.definitionPath, std::string_view{}, 0, 0);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.definitionPath);
  }
};

struct LocalAutoFactTraits {
  using Entry = LocalAutoBindingSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.scopePath, entry.bindingName, entry.sourceLine, entry.sourceColumn);
  }
};

struct QueryFactTraits {
  using Entry = QueryFactSnapshotEntry;
  static constexpr bool MergeAppends = true;
  static constexpr bool HasIdentityKey = true;
  static std::string key(const Entry &entry) {
    return snapshotKey(
        entry.scopePath, entry.callName, entry.sourceLine, entry.sourceColumn, entry.resolvedPath);
  }
  static std::string identityKey(const Entry &entry) {
    return snapshotKey(entry.scopePath, entry.callName, entry.sourceLine, entry.sourceColumn);
  }
  static auto order(const Entry &entry) {
    return std::tie(
        entry.scopePath, entry.sourceLine, entry.sourceColumn, entry.callName, entry.resolvedPath);
  }
  static auto mergeOrder(const Entry &entry) {
    return std::tie(entry.scopePath,
                    entry.sourceLine,
                    entry.sourceColumn,
                    entry.callName,
                    entry.resolvedPath,
                    entry.semanticNodeId);
  }
};

struct CallBindingFactTraits {
  using Entry = SemanticsValidator::CallBindingSnapshotEntry;
  static auto order(const Entry &entry) {
    return std::tie(entry.scopePath, entry.sourceLine, entry.sourceColumn, entry.callName);
  }
};

struct TryFactTraits {
  using Entry = TryValueSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(
        entry.scopePath, entry.operandResolvedPath, entry.sourceLine, entry.sourceColumn);
  }
  static auto order(const Entry &entry) {
    return std::tie(
        entry.scopePath, entry.sourceLine, entry.sourceColumn, entry.operandResolvedPath);
  }
};

struct OnErrorFactTraits {
  using Entry = OnErrorSnapshotEntry;
  static constexpr bool MergeAppends = false;
  static std::string key(const Entry &entry) {
    return snapshotKey(entry.definitionPath, std::string_view{}, 0, 0);
  }
  static auto order(const Entry &entry) {
    return std::tie(entry.definitionPath, entry.semanticNodeId);
  }
};

} // namespace primec::semantics::snapshot_detail
