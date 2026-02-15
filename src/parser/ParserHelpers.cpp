#include "ParserHelpers.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace primec::parser {
namespace {

bool isReservedKeyword(const std::string &text) {
  return text == "mut" || text == "return" || text == "include" || text == "import" || text == "namespace" ||
         text == "true" || text == "false" || text == "if" || text == "else" || text == "loop" || text == "while" ||
         text == "for";
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
  return validateIdentifierText(text, error);
}

bool isStructTransformName(const std::string &text) {
  return text == "struct" || text == "pod" || text == "handle" || text == "gpu_lane" || text == "no_padding" ||
         text == "platform_independent_padding";
}

bool isBuiltinName(const std::string &name, bool allowMathBare) {
  std::string candidate = name;
  if (!candidate.empty() && candidate[0] == '/') {
    candidate.erase(0, 1);
  }
  bool isMathQualified = false;
  if (candidate.rfind("math/", 0) == 0) {
    candidate.erase(0, 5);
    isMathQualified = true;
  } else if (candidate.find('/') != std::string::npos) {
    return false;
  }
  bool isMathBuiltin = candidate == "abs" || candidate == "sign" || candidate == "min" || candidate == "max" ||
                       candidate == "clamp" || candidate == "lerp" || candidate == "saturate" ||
                       candidate == "floor" || candidate == "ceil" || candidate == "round" || candidate == "trunc" ||
                       candidate == "fract" || candidate == "sqrt" || candidate == "cbrt" || candidate == "pow" ||
                       candidate == "exp" || candidate == "exp2" || candidate == "log" || candidate == "log2" ||
                       candidate == "log10" || candidate == "sin" || candidate == "cos" || candidate == "tan" ||
                       candidate == "asin" || candidate == "acos" || candidate == "atan" || candidate == "atan2" ||
                       candidate == "radians" || candidate == "degrees" || candidate == "sinh" || candidate == "cosh" ||
                       candidate == "tanh" || candidate == "asinh" || candidate == "acosh" || candidate == "atanh" ||
                       candidate == "fma" || candidate == "hypot" || candidate == "copysign" || candidate == "is_nan" ||
                       candidate == "is_inf" || candidate == "is_finite";
  if (isMathQualified && !isMathBuiltin) {
    return false;
  }
  return candidate == "assign" || candidate == "plus" || candidate == "minus" || candidate == "multiply" ||
         candidate == "divide" || candidate == "negate" || candidate == "greater_than" || candidate == "less_than" ||
         candidate == "greater_equal" || candidate == "less_equal" || candidate == "equal" || candidate == "not_equal" ||
         candidate == "and" || candidate == "or" || candidate == "not" ||
         (isMathBuiltin && (allowMathBare || isMathQualified)) ||
         candidate == "increment" || candidate == "decrement" ||
         candidate == "if" || candidate == "then" || candidate == "else" ||
         candidate == "repeat" || candidate == "return" || candidate == "array" || candidate == "vector" ||
         candidate == "map" || candidate == "count" || candidate == "capacity" || candidate == "push" ||
         candidate == "pop" || candidate == "reserve" || candidate == "clear" || candidate == "remove_at" ||
         candidate == "remove_swap" || candidate == "at" || candidate == "at_unsafe" || candidate == "convert" ||
         candidate == "location" || candidate == "dereference" || candidate == "block" || candidate == "print" ||
         candidate == "print_line" || candidate == "print_error" || candidate == "print_line_error" ||
         candidate == "notify" || candidate == "insert" || candidate == "take";
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

bool isBindingAuxTransformName(const std::string &name) {
  return name == "mut" || name == "copy" || name == "restrict" || name == "align_bytes" ||
         name == "align_kbytes" || name == "pod" || name == "handle" || name == "gpu_lane" ||
         name == "public" || name == "private" || name == "package" || name == "static";
}

bool hasExplicitBindingTypeTransform(const std::vector<Transform> &transforms) {
  for (const auto &transform : transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return true;
  }
  return false;
}

bool isBindingTransformList(const std::vector<Transform> &transforms) {
  if (transforms.empty()) {
    return false;
  }
  if (hasExplicitBindingTypeTransform(transforms)) {
    return true;
  }
  for (const auto &transform : transforms) {
    if (isBindingAuxTransformName(transform.name)) {
      return true;
    }
  }
  return false;
}

} // namespace primec::parser
