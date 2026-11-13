#pragma once

#include <cctype>

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

} // namespace primec
