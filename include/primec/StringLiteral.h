#pragma once

#include <string>

namespace primec {

enum class StringEncoding { Utf8, Ascii };

struct ParsedStringLiteral {
  std::string literalText;
  std::string decoded;
  StringEncoding encoding = StringEncoding::Utf8;
};

inline bool splitStringLiteralToken(const std::string &token, std::string &literalText, std::string &suffix) {
  if (!token.empty() && (token.front() == '"' || token.front() == '\'')) {
    char quote = token.front();
    size_t closePos = token.rfind(quote);
    if (closePos == std::string::npos || closePos == 0) {
      return false;
    }
    literalText = token.substr(0, closePos + 1);
    suffix = token.substr(closePos + 1);
    return true;
  }
  return false;
}

inline bool decodeStringLiteralText(const std::string &literal,
                                    std::string &decoded,
                                    std::string &error,
                                    bool raw) {
  if (literal.size() >= 2 &&
      ((literal.front() == '"' && literal.back() == '"') || (literal.front() == '\'' && literal.back() == '\''))) {
    if (raw) {
      decoded = literal.substr(1, literal.size() - 2);
      return true;
    }
    decoded.clear();
    decoded.reserve(literal.size() - 2);
    for (size_t i = 1; i + 1 < literal.size(); ++i) {
      char c = literal[i];
      if (c == '\\' && i + 2 < literal.size()) {
        char next = literal[++i];
        switch (next) {
          case 'n':
            decoded.push_back('\n');
            break;
          case 'r':
            decoded.push_back('\r');
            break;
          case 't':
            decoded.push_back('\t');
            break;
          case '\\':
            decoded.push_back('\\');
            break;
          case '"':
            decoded.push_back('"');
            break;
          case '\'':
            decoded.push_back('\'');
            break;
          case '0':
            decoded.push_back('\0');
            break;
          default:
            decoded.push_back(next);
            break;
        }
      } else {
        decoded.push_back(c);
      }
    }
    return true;
  }
  error = "invalid string literal";
  return false;
}

inline bool parseStringLiteralToken(const std::string &token, ParsedStringLiteral &out, std::string &error) {
  std::string literalText;
  std::string suffix;
  if (!splitStringLiteralToken(token, literalText, suffix)) {
    error = "invalid string literal";
    return false;
  }
  if (suffix.empty()) {
    error = "string literal requires utf8/ascii/raw_utf8/raw_ascii suffix";
    return false;
  }
  StringEncoding encoding = StringEncoding::Utf8;
  bool raw = false;
  if (suffix == "utf8") {
    encoding = StringEncoding::Utf8;
  } else if (suffix == "ascii") {
    encoding = StringEncoding::Ascii;
  } else if (suffix == "raw_utf8") {
    encoding = StringEncoding::Utf8;
    raw = true;
  } else if (suffix == "raw_ascii") {
    encoding = StringEncoding::Ascii;
    raw = true;
  } else {
    error = "unknown string literal suffix: " + suffix;
    return false;
  }
  std::string decoded;
  if (!decodeStringLiteralText(literalText, decoded, error, raw)) {
    return false;
  }
  out.literalText = std::move(literalText);
  out.decoded = std::move(decoded);
  out.encoding = encoding;
  return true;
}

inline bool isAsciiText(const std::string &text) {
  for (unsigned char c : text) {
    if (c > 0x7F) {
      return false;
    }
  }
  return true;
}

} // namespace primec
