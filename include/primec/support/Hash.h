#pragma once

#include <cctype>
#include <cstdint>
#include <string_view>

namespace primec {

// The FNV-1a 64-bit offset basis.
inline constexpr uint64_t Fnv1a64OffsetBasis = 14695981039346656037ull;
// The basis the compiler's generated names and the stdlib symbol manifest use: the standard one
// with its last digit missing. Those hash values are part of generated names and of the checked-in
// manifests, so changing the basis renames them; hashes that need no such stability use the
// standard basis.
inline constexpr uint64_t NameHashOffsetBasis = 1469598103934665603ull;

// FNV-1a 64-bit. Not cryptographic.
constexpr uint64_t fnv1a64(std::string_view text, uint64_t offsetBasis = Fnv1a64OffsetBasis) {
  uint64_t hash = offsetBasis;
  for (const char c : text) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

// The hash of generated names and manifest entries.
constexpr uint64_t nameHash(std::string_view text) {
  return fnv1a64(text, NameHashOffsetBasis);
}

// nameHash of `text` with every whitespace character left out: the hash of a template argument
// list, which must not depend on how the source spaced it.
inline uint64_t nameHashSkippingWhitespace(std::string_view text) {
  uint64_t hash = NameHashOffsetBasis;
  for (const char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      continue;
    }
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

} // namespace primec
