#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec {

using SymbolId = uint32_t;
constexpr SymbolId InvalidSymbolId = 0;

// Immutable transfer object for crossing worker boundaries.
// symbolsByLocalId[i] corresponds to worker-local SymbolId i+1.
struct WorkerSymbolInternerSnapshot {
  uint32_t workerId = 0;
  std::vector<std::string> symbolsByLocalId;
};

class SymbolInterner {
public:
  // Assigns deterministic IDs in first-seen order for this interner instance.
  SymbolId intern(std::string_view text);

  // Returns the existing ID without inserting when present.
  std::optional<SymbolId> lookup(std::string_view text) const;
  bool contains(std::string_view text) const;

  // Returns the interned text for a valid non-zero SymbolId; otherwise empty view.
  std::string_view resolve(SymbolId id) const;

  std::size_t size() const noexcept;
  bool empty() const noexcept;
  void clear();

  // Captures worker-local symbols in deterministic local-id order.
  WorkerSymbolInternerSnapshot snapshotForWorker(uint32_t workerId) const;

  // Deterministic merge hook for worker-local snapshots.
  // Snapshots are sorted by workerId, then lexicographically by symbolsByLocalId.
  static SymbolInterner
  mergeWorkerSnapshotsDeterministic(std::vector<WorkerSymbolInternerSnapshot> snapshots);

  // Deterministic two-worker merge helper that uses the shared merge policy.
  // Assignment is independent of argument order.
  static SymbolInterner
  mergeTwoWorkerSnapshotsDeterministic(const WorkerSymbolInternerSnapshot &first,
                                       const WorkerSymbolInternerSnapshot &second);

  // Maps worker-local SymbolIds to merged SymbolIds by local-id order.
  // Missing symbols map to InvalidSymbolId.
  static std::vector<SymbolId>
  remapLocalIdsToMerged(const WorkerSymbolInternerSnapshot &snapshot,
                        const SymbolInterner &merged);

private:
  struct StringViewHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept;
    std::size_t operator()(const std::string &value) const noexcept;
    std::size_t operator()(const char *value) const noexcept;
  };

  struct StringViewEqual {
    using is_transparent = void;
    bool operator()(std::string_view left, std::string_view right) const noexcept;
    bool operator()(const std::string &left, std::string_view right) const noexcept;
    bool operator()(std::string_view left, const std::string &right) const noexcept;
    bool operator()(const char *left, std::string_view right) const noexcept;
    bool operator()(std::string_view left, const char *right) const noexcept;
  };

  // Deque keeps element addresses stable so map keys remain valid.
  std::deque<std::string> storage_;
  std::unordered_map<std::string_view, SymbolId, StringViewHash, StringViewEqual> idsByText_;
};

} // namespace primec
