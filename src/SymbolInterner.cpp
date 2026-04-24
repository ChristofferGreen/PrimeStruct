#include "primec/SymbolInterner.h"

#include <algorithm>
#include <limits>

namespace primec {

namespace {

bool symbol_origin_less(const SymbolOriginKey &left, const SymbolOriginKey &right) {
  if (left.moduleOrder != right.moduleOrder) {
    return left.moduleOrder < right.moduleOrder;
  }
  if (left.declarationOrder != right.declarationOrder) {
    return left.declarationOrder < right.declarationOrder;
  }
  if (left.semanticNodeOrder != right.semanticNodeOrder) {
    return left.semanticNodeOrder < right.semanticNodeOrder;
  }
  return left.fieldOrder < right.fieldOrder;
}

bool snapshot_order_less(const WorkerSymbolInternerSnapshot *left,
                         const WorkerSymbolInternerSnapshot *right) {
  if (left->partitionKey != right->partitionKey) {
    return left->partitionKey < right->partitionKey;
  }
  if (left->workerId != right->workerId) {
    return left->workerId < right->workerId;
  }
  return left->symbolsByLocalId < right->symbolsByLocalId;
}

SymbolInterner merge_snapshots_deterministic(
    std::vector<const WorkerSymbolInternerSnapshot *> snapshots) {
  struct MergeCandidate {
    std::string text;
    SymbolOriginKey firstOrigin;
    uint32_t partitionKey = 0;
    uint32_t workerId = 0;
    std::size_t localIndex = 0;
  };

  auto candidate_less = [](const MergeCandidate &left, const MergeCandidate &right) {
    if (symbol_origin_less(left.firstOrigin, right.firstOrigin)) {
      return true;
    }
    if (symbol_origin_less(right.firstOrigin, left.firstOrigin)) {
      return false;
    }
    if (left.partitionKey != right.partitionKey) {
      return left.partitionKey < right.partitionKey;
    }
    if (left.localIndex != right.localIndex) {
      return left.localIndex < right.localIndex;
    }
    if (left.workerId != right.workerId) {
      return left.workerId < right.workerId;
    }
    return left.text < right.text;
  };

  std::sort(snapshots.begin(), snapshots.end(), snapshot_order_less);

  std::unordered_map<std::string, MergeCandidate> representativeByText;
  for (const WorkerSymbolInternerSnapshot *snapshot : snapshots) {
    const auto &symbols = snapshot->symbolsByLocalId;
    for (std::size_t localIndex = 0; localIndex < symbols.size(); ++localIndex) {
      const std::string &symbol = symbols[localIndex];
      const SymbolOriginKey origin =
          localIndex < snapshot->firstOriginByLocalId.size()
              ? snapshot->firstOriginByLocalId[localIndex]
              : SymbolOriginKey{};
      MergeCandidate candidate{
          .text = symbol,
          .firstOrigin = origin,
          .partitionKey = snapshot->partitionKey,
          .workerId = snapshot->workerId,
          .localIndex = localIndex,
      };
      const auto it = representativeByText.find(symbol);
      if (it == representativeByText.end() || candidate_less(candidate, it->second)) {
        representativeByText.insert_or_assign(symbol, std::move(candidate));
      }
    }
  }

  std::vector<MergeCandidate> candidates;
  candidates.reserve(representativeByText.size());
  for (auto &[_, candidate] : representativeByText) {
    candidates.push_back(std::move(candidate));
  }
  std::sort(candidates.begin(), candidates.end(), [](const MergeCandidate &left,
                                                     const MergeCandidate &right) {
    if (symbol_origin_less(left.firstOrigin, right.firstOrigin)) {
      return true;
    }
    if (symbol_origin_less(right.firstOrigin, left.firstOrigin)) {
      return false;
    }
    if (left.partitionKey != right.partitionKey) {
      return left.partitionKey < right.partitionKey;
    }
    if (left.localIndex != right.localIndex) {
      return left.localIndex < right.localIndex;
    }
    if (left.workerId != right.workerId) {
      return left.workerId < right.workerId;
    }
    return left.text < right.text;
  });

  SymbolInterner merged;
  for (const auto &candidate : candidates) {
    (void)merged.intern(candidate.text);
  }
  return merged;
}

} // namespace

std::size_t SymbolInterner::StringViewHash::operator()(std::string_view value) const noexcept {
  return std::hash<std::string_view>{}(value);
}

std::size_t SymbolInterner::StringViewHash::operator()(const std::string &value) const noexcept {
  return (*this)(std::string_view(value));
}

std::size_t SymbolInterner::StringViewHash::operator()(const char *value) const noexcept {
  return (*this)(std::string_view(value));
}

bool SymbolInterner::StringViewEqual::operator()(std::string_view left, std::string_view right) const noexcept {
  return left == right;
}

bool SymbolInterner::StringViewEqual::operator()(const std::string &left,
                                                 std::string_view right) const noexcept {
  return std::string_view(left) == right;
}

bool SymbolInterner::StringViewEqual::operator()(std::string_view left,
                                                 const std::string &right) const noexcept {
  return left == std::string_view(right);
}

bool SymbolInterner::StringViewEqual::operator()(const char *left, std::string_view right) const noexcept {
  return std::string_view(left) == right;
}

bool SymbolInterner::StringViewEqual::operator()(std::string_view left, const char *right) const noexcept {
  return left == std::string_view(right);
}

SymbolId SymbolInterner::intern(std::string_view text) {
  return intern(text, SymbolOriginKey{});
}

SymbolId SymbolInterner::intern(std::string_view text, const SymbolOriginKey &originKey) {
  if (const auto existing = idsByText_.find(text); existing != idsByText_.end()) {
    const std::size_t existingIndex = static_cast<std::size_t>(existing->second - 1);
    if (existingIndex < firstOriginById_.size() &&
        symbol_origin_less(originKey, firstOriginById_[existingIndex])) {
      firstOriginById_[existingIndex] = originKey;
    }
    return existing->second;
  }
  if (storage_.size() >= static_cast<std::size_t>(std::numeric_limits<SymbolId>::max())) {
    return InvalidSymbolId;
  }
  storage_.emplace_back(text);
  firstOriginById_.push_back(originKey);
  const SymbolId id = static_cast<SymbolId>(storage_.size());
  idsByText_.emplace(storage_.back(), id);
  return id;
}

std::optional<SymbolId> SymbolInterner::lookup(std::string_view text) const {
  const auto it = idsByText_.find(text);
  if (it == idsByText_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool SymbolInterner::contains(std::string_view text) const {
  return idsByText_.find(text) != idsByText_.end();
}

std::string_view SymbolInterner::resolve(SymbolId id) const {
  if (id == InvalidSymbolId || id > storage_.size()) {
    return {};
  }
  return storage_[id - 1];
}

std::size_t SymbolInterner::size() const noexcept {
  return storage_.size();
}

bool SymbolInterner::empty() const noexcept {
  return storage_.empty();
}

void SymbolInterner::clear() {
  idsByText_.clear();
  storage_.clear();
  firstOriginById_.clear();
}

WorkerSymbolInternerSnapshot SymbolInterner::snapshotForWorker(uint32_t workerId) const {
  WorkerSymbolInternerSnapshot snapshot;
  snapshot.workerId = workerId;
  snapshot.partitionKey = workerId;
  snapshot.symbolsByLocalId.reserve(storage_.size());
  snapshot.firstOriginByLocalId.reserve(firstOriginById_.size());
  for (const std::string &symbol : storage_) {
    snapshot.symbolsByLocalId.push_back(symbol);
  }
  snapshot.firstOriginByLocalId = firstOriginById_;
  return snapshot;
}

SymbolInterner SymbolInterner::mergeTwoWorkerSnapshotsDeterministic(
    const WorkerSymbolInternerSnapshot &first, const WorkerSymbolInternerSnapshot &second) {
  return merge_snapshots_deterministic({&first, &second});
}

SymbolInterner SymbolInterner::mergeWorkerSnapshotsDeterministic(
    std::vector<WorkerSymbolInternerSnapshot> snapshots) {
  std::vector<const WorkerSymbolInternerSnapshot *> snapshotPtrs;
  snapshotPtrs.reserve(snapshots.size());
  for (const WorkerSymbolInternerSnapshot &snapshot : snapshots) {
    snapshotPtrs.push_back(&snapshot);
  }
  return merge_snapshots_deterministic(std::move(snapshotPtrs));
}

std::vector<SymbolId> SymbolInterner::remapLocalIdsToMerged(
    const WorkerSymbolInternerSnapshot &snapshot, const SymbolInterner &merged) {
  std::vector<SymbolId> mergedIdsByLocalId;
  mergedIdsByLocalId.reserve(snapshot.symbolsByLocalId.size());
  for (const std::string &symbol : snapshot.symbolsByLocalId) {
    const std::optional<SymbolId> mergedId = merged.lookup(symbol);
    mergedIdsByLocalId.push_back(mergedId.value_or(InvalidSymbolId));
  }
  return mergedIdsByLocalId;
}

} // namespace primec
