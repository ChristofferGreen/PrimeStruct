#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace primec {

using SymbolId = uint32_t;
constexpr SymbolId InvalidSymbolId = 0;

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
