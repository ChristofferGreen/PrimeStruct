#include "ParserHelpers.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace primec::parser {
namespace {

bool isReservedKeyword(const std::string &text) {
  return text == "mut" || text == "return" || text == "include" || text == "namespace" || text == "true" ||
         text == "false";
}

bool isAsciiAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isAsciiDigit(char c) {
  return c >= '0' && c <= '9';
}

bool isIdentifierSegmentStart(char c) {
  return isAsciiAlpha(c) || c == '_';
}

bool isIdentifierSegmentChar(char c) {
  return isAsciiAlpha(c) || isAsciiDigit(c) || c == '_';
}

bool validateSlashPath(const std::string &text, std::string &error) {
  if (text.size() < 2 || text[0] != '/') {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  size_t start = 1;
  while (start < text.size()) {
    size_t end = text.find('/', start);
    std::string segment = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (segment.empty() || !isIdentifierSegmentStart(segment[0])) {
      error = "invalid slash path identifier: " + text;
      return false;
    }
    for (size_t i = 1; i < segment.size(); ++i) {
      if (!isIdentifierSegmentChar(segment[i])) {
        error = "invalid slash path identifier: " + text;
        return false;
      }
    }
    if (isReservedKeyword(segment)) {
      error = "reserved keyword cannot be used as identifier: " + segment;
      return false;
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  if (text.back() == '/') {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  return true;
}

} // namespace

std::string describeInvalidToken(const Token &token) {
  if (token.text.size() != 1) {
    return token.text.empty() ? "<unknown>" : token.text;
  }
  const unsigned char byte = static_cast<unsigned char>(token.text[0]);
  if (byte >= 0x20 && byte <= 0x7E) {
    return std::string("'") + static_cast<char>(byte) + "'";
  }
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  return out.str();
}

bool validateIdentifierText(const std::string &text, std::string &error) {
  if (text.empty()) {
    error = "invalid identifier";
    return false;
  }
  if (text[0] == '/') {
    return validateSlashPath(text, error);
  }
  if (text.find('/') != std::string::npos) {
    error = "invalid slash path identifier: " + text;
    return false;
  }
  if (!isIdentifierSegmentStart(text[0])) {
    error = "invalid identifier: " + text;
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    if (!isIdentifierSegmentChar(text[i])) {
      error = "invalid identifier: " + text;
      return false;
    }
  }
  if (isReservedKeyword(text)) {
    error = "reserved keyword cannot be used as identifier: " + text;
    return false;
  }
  return true;
}

bool validateTransformName(const std::string &text, std::string &error) {
  if (text == "return" || text == "mut") {
    return true;
  }
  if (text.empty()) {
    error = "invalid transform identifier";
    return false;
  }
  if (text[0] == '/' || text.find('/') != std::string::npos) {
    error = "transform identifiers cannot be slash paths";
    return false;
  }
  return validateIdentifierText(text, error);
}

bool isStructTransformName(const std::string &text) {
  return text == "struct" || text == "pod" || text == "handle" || text == "gpu_lane";
}

bool isBuiltinName(const std::string &name) {
  return name == "assign" || name == "plus" || name == "minus" || name == "multiply" || name == "divide" ||
         name == "negate" || name == "greater_than" || name == "less_than" || name == "greater_equal" ||
         name == "less_equal" || name == "equal" || name == "not_equal" || name == "and" || name == "or" ||
         name == "not" || name == "clamp" || name == "min" || name == "max" || name == "abs" || name == "sign" ||
         name == "saturate" || name == "if" || name == "then" || name == "else" || name == "repeat" ||
         name == "return" || name == "array" || name == "map" || name == "count" || name == "at" ||
         name == "at_unsafe" || name == "convert" || name == "location" || name == "dereference" ||
         name == "print" || name == "print_line" || name == "print_error" || name == "print_line_error" ||
         name == "notify" || name == "insert" || name == "take";
}

bool isHexDigitChar(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

bool isValidFloatLiteral(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  size_t i = 0;
  if (text[i] == '-') {
    ++i;
  }
  if (i >= text.size()) {
    return false;
  }
  bool sawDigit = false;
  bool sawDot = false;
  while (i < text.size()) {
    char c = text[i];
    if (std::isdigit(static_cast<unsigned char>(c))) {
      sawDigit = true;
      ++i;
      continue;
    }
    if (c == '.' && !sawDot) {
      sawDot = true;
      ++i;
      continue;
    }
    break;
  }
  if (!sawDigit) {
    return false;
  }
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      ++i;
    }
    size_t expStart = i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
    if (i == expStart) {
      return false;
    }
  }
  return i == text.size();
}

} // namespace primec::parser
