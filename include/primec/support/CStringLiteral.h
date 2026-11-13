#pragma once

#include <string>
#include <string_view>

namespace primec {

// C++ for a quoted string literal that preserves every byte of `text`. Every byte outside
// 0x20-0x7E, and `?` (which could start a trigraph), is written as a three-digit octal escape,
// which never swallows a following character; `"` and `\` get a backslash.
inline std::string cStringLiteral(std::string_view text) {
  std::string out = "\"";
  for (const unsigned char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c >= 0x20 && c < 0x7F && c != '?') {
      out += static_cast<char>(c);
    } else {
      out += '\\';
      out += static_cast<char>('0' + ((c >> 6) & 7));
      out += static_cast<char>('0' + ((c >> 3) & 7));
      out += static_cast<char>('0' + (c & 7));
    }
  }
  out += "\"";
  return out;
}

} // namespace primec
