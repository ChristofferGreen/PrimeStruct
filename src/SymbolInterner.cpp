#include "primec/SymbolInterner.h"

#include <algorithm>
#include <limits>

namespace primec {

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
  if (const auto existing = idsByText_.find(text); existing != idsByText_.end()) {
    return existing->second;
  }
  if (storage_.size() >= static_cast<std::size_t>(std::numeric_limits<SymbolId>::max())) {
    return InvalidSymbolId;
  }
  storage_.emplace_back(text);
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
}

WorkerSymbolInternerSnapshot SymbolInterner::snapshotForWorker(uint32_t workerId) const {
  WorkerSymbolInternerSnapshot snapshot;
  snapshot.workerId = workerId;
  snapshot.symbolsByLocalId.reserve(storage_.size());
  for (const std::string &symbol : storage_) {
    snapshot.symbolsByLocalId.push_back(symbol);
  }
  return snapshot;
}

SymbolInterner SymbolInterner::mergeWorkerSnapshotsDeterministic(
    std::vector<WorkerSymbolInternerSnapshot> snapshots) {
  std::sort(snapshots.begin(), snapshots.end(), [](const WorkerSymbolInternerSnapshot &left,
                                                   const WorkerSymbolInternerSnapshot &right) {
    if (left.workerId != right.workerId) {
      return left.workerId < right.workerId;
    }
    return left.symbolsByLocalId < right.symbolsByLocalId;
  });

  SymbolInterner merged;
  for (const WorkerSymbolInternerSnapshot &snapshot : snapshots) {
    for (const std::string &symbol : snapshot.symbolsByLocalId) {
      (void)merged.intern(symbol);
    }
  }
  return merged;
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
