#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace primec {

inline bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

inline bool isHexDigitChar(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// `text` without the leading and trailing characters std::isspace accepts.
inline std::string_view trimAsciiView(std::string_view text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.remove_suffix(1);
  }
  return text;
}

inline std::string trimAscii(std::string_view text) {
  return std::string(trimAsciiView(text));
}

} // namespace primec
