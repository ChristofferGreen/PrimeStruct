#include "primec/TextFilterPipeline.h"

#include <cctype>

namespace primec {
namespace {
bool isSeparator(char c) {
  return std::isspace(static_cast<unsigned char>(c)) || c == '(' || c == ')' || c == '{' || c == '}' ||
         c == '[' || c == ']' || c == ',' || c == '<' || c == '>' || c == ';' || c == '"' || c == '\'';
}

bool isTokenChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isOperatorTokenChar(char c) {
  return isTokenChar(c) || c == '.' || c == '/';
}

bool isLeftOperandEndChar(char c) {
  return isOperatorTokenChar(c) || c == ')' || c == ']' || c == '}' || c == '"' || c == '\'';
}

bool isDigitChar(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isHexDigitChar(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

bool isUnaryPrefixPosition(const std::string &input, size_t index) {
  if (index == 0) {
    return true;
  }
  char prev = input[index - 1];
  if (prev == ')' || prev == ']' || prev == '}') {
    return false;
  }
  return isSeparator(prev);
}

bool isRightOperandStartChar(const std::string &input, size_t index) {
  char c = input[index];
  if (isOperatorTokenChar(c) || c == '(' || c == '"' || c == '\'' || c == '[' || c == '{') {
    return true;
  }
  if (c == 'R' && index + 2 < input.size() && input[index + 1] == '"' && input[index + 2] == '(') {
    return true;
  }
  if (c == '-' && isUnaryPrefixPosition(input, index)) {
    return true;
  }
  return false;
}

bool isExponentSign(const std::string &text, size_t index) {
  if (index == 0) {
    return false;
  }
  char prev = text[index - 1];
  if (prev != 'e' && prev != 'E') {
    return false;
  }
  return text[index] == '-' || text[index] == '+';
}

bool isEscaped(const std::string &text, size_t index) {
  size_t count = 0;
  while (index > 0 && text[index - 1] == '\\') {
    --index;
    ++count;
  }
  return (count % 2) == 1;
}

size_t findQuotedStart(const std::string &text, size_t endQuote) {
  char quote = text[endQuote];
  if (endQuote == 0) {
    return std::string::npos;
  }
  size_t pos = endQuote;
  while (pos > 0) {
    --pos;
    if (text[pos] == quote && !isEscaped(text, pos)) {
      return pos;
    }
  }
  return std::string::npos;
}

size_t findRawStringStart(const std::string &text, size_t endQuote) {
  if (endQuote < 2 || text[endQuote - 1] != ')') {
    return std::string::npos;
  }
  return text.rfind("R\"(", endQuote);
}

size_t skipQuotedForward(const std::string &text, size_t start) {
  char quote = text[start];
  size_t pos = start + 1;
  while (pos < text.size()) {
    if (text[pos] == '\\' && pos + 1 < text.size()) {
      pos += 2;
      continue;
    }
    if (text[pos] == quote) {
      return pos + 1;
    }
    ++pos;
  }
  return std::string::npos;
}

size_t skipRawStringForward(const std::string &text, size_t start) {
  size_t pos = start + 3;
  while (pos + 1 < text.size()) {
    if (text[pos] == ')' && text[pos + 1] == '"') {
      return pos + 2;
    }
    ++pos;
  }
  return std::string::npos;
}

size_t findMatchingClose(const std::string &text, size_t openIndex, char openChar, char closeChar) {
  int depth = 1;
  size_t pos = openIndex + 1;
  while (pos < text.size()) {
    char c = text[pos];
    if (c == openChar) {
      ++depth;
      ++pos;
      continue;
    }
    if (c == closeChar) {
      --depth;
      if (depth == 0) {
        return pos;
      }
      ++pos;
      continue;
    }
    if (c == 'R' && pos + 2 < text.size() && text[pos + 1] == '"' && text[pos + 2] == '(') {
      size_t end = skipRawStringForward(text, pos);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end;
      continue;
    }
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(text, pos);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end;
      continue;
    }
    ++pos;
  }
  return std::string::npos;
}

size_t findMatchingOpen(const std::string &text, size_t closeIndex, char openChar, char closeChar) {
  int depth = 1;
  size_t pos = closeIndex;
  while (pos > 0) {
    --pos;
    char c = text[pos];
    if (c == closeChar) {
      ++depth;
      continue;
    }
    if (c == openChar) {
      --depth;
      if (depth == 0) {
        return pos;
      }
      continue;
    }
    if (c == '"' || c == '\'') {
      size_t rawStart = (c == '"') ? findRawStringStart(text, pos) : std::string::npos;
      if (rawStart != std::string::npos) {
        pos = rawStart;
        continue;
      }
      size_t start = findQuotedStart(text, pos);
      if (start == std::string::npos || start == 0) {
        return std::string::npos;
      }
      pos = start;
      continue;
    }
  }
  return std::string::npos;
}

size_t findLeftTokenStart(const std::string &text, size_t end) {
  size_t start = end;
  if (end == 0) {
    return start;
  }
  char tail = text[end - 1];
  if (tail == ')' || tail == ']' || tail == '}') {
    char openChar = tail == ')' ? '(' : (tail == ']' ? '[' : '{');
    size_t openPos = findMatchingOpen(text, end - 1, openChar, tail);
    if (openPos == std::string::npos) {
      return end;
    }
    start = openPos;
    while (start > 0 && isOperatorTokenChar(text[start - 1])) {
      --start;
    }
    if (start > 0 && text[start - 1] == '-' && isUnaryPrefixPosition(text, start - 1)) {
      --start;
    }
    return start;
  }
  if (tail == '"' || tail == '\'') {
    size_t rawStart = (tail == '"') ? findRawStringStart(text, end - 1) : std::string::npos;
    if (rawStart != std::string::npos) {
      return rawStart;
    }
    size_t quoteStart = findQuotedStart(text, end - 1);
    if (quoteStart != std::string::npos) {
      return quoteStart;
    }
    return end;
  }
  while (start > 0) {
    char c = text[start - 1];
    if (isOperatorTokenChar(c) || isExponentSign(text, start - 1)) {
      --start;
      continue;
    }
    break;
  }
  if (start > 0 && text[start - 1] == '-' && isUnaryPrefixPosition(text, start - 1)) {
    --start;
  }
  return start;
}

size_t findRightTokenEnd(const std::string &text, size_t start) {
  if (start >= text.size()) {
    return start;
  }
  if (text[start] == '-' && isUnaryPrefixPosition(text, start)) {
    return findRightTokenEnd(text, start + 1);
  }
  char lead = text[start];
  if (lead == '(' || lead == '[' || lead == '{') {
    char closeChar = lead == '(' ? ')' : (lead == '[' ? ']' : '}');
    size_t closePos = findMatchingClose(text, start, lead, closeChar);
    return closePos == std::string::npos ? start : closePos + 1;
  }
  if (lead == 'R' && start + 2 < text.size() && text[start + 1] == '"' && text[start + 2] == '(') {
    size_t end = skipRawStringForward(text, start);
    return end == std::string::npos ? start : end;
  }
  if (lead == '"' || lead == '\'') {
    size_t end = skipQuotedForward(text, start);
    return end == std::string::npos ? start : end;
  }
  size_t pos = start;
  while (pos < text.size()) {
    if (isOperatorTokenChar(text[pos]) || isExponentSign(text, pos)) {
      ++pos;
      continue;
    }
    break;
  }
  if (pos < text.size() && text[pos] == '(') {
    size_t closePos = findMatchingClose(text, pos, '(', ')');
    if (closePos != std::string::npos) {
      pos = closePos + 1;
    }
  }
  return pos;
}

std::string stripOuterParens(const std::string &text) {
  if (text.size() < 2 || text.front() != '(' || text.back() != ')') {
    return text;
  }
  size_t closePos = findMatchingClose(text, 0, '(', ')');
  if (closePos != text.size() - 1) {
    return text;
  }
  return text.substr(1, text.size() - 2);
}

bool looksLikeTemplateList(const std::string &input, size_t index) {
  if (index >= input.size() || input[index] != '<') {
    return false;
  }
  size_t pos = index + 1;
  bool sawToken = false;
  while (pos < input.size()) {
    char c = input[pos];
    if (c == '>') {
      return sawToken;
    }
    if (isOperatorTokenChar(c)) {
      sawToken = true;
      ++pos;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
      ++pos;
      continue;
    }
    return false;
  }
  return false;
}

bool looksLikeTemplateListClose(const std::string &input, size_t index) {
  if (index >= input.size() || input[index] != '>') {
    return false;
  }
  size_t pos = index;
  bool sawToken = false;
  while (pos > 0) {
    --pos;
    char c = input[pos];
    if (c == '<') {
      return sawToken;
    }
    if (isOperatorTokenChar(c)) {
      sawToken = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
      continue;
    }
    return false;
  }
  return false;
}

std::string maybeAppendI32(const std::string &token) {
  if (token.empty()) {
    return token;
  }
  bool isDecimal = true;
  for (char c : token) {
    if (!isDigitChar(c)) {
      isDecimal = false;
      break;
    }
  }
  if (isDecimal) {
    return token + "i32";
  }
  if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
    for (size_t i = 2; i < token.size(); ++i) {
      if (!isHexDigitChar(token[i])) {
        return token;
      }
    }
    return token + "i32";
  }
  return token;
}

bool rewriteUnaryNot(const std::string &input,
                     std::string &output,
                     size_t &index,
                     const TextFilterOptions &options) {
  if (input[index] != '!') {
    return false;
  }
  if (index + 1 >= input.size()) {
    return false;
  }
  if (input[index + 1] == '=') {
    return false;
  }
  if (index > 0 && isTokenChar(input[index - 1])) {
    return false;
  }
  if (input[index + 1] == '(') {
    output.append("not");
    return true;
  }
  size_t rightStart = index + 1;
  size_t rightEnd = rightStart;
  while (rightEnd < input.size() && isTokenChar(input[rightEnd])) {
    ++rightEnd;
  }
  if (rightEnd == rightStart) {
    return false;
  }
  std::string right = input.substr(rightStart, rightEnd - rightStart);
  if (options.implicitI32Suffix) {
    right = maybeAppendI32(right);
  }
  output.append("not(");
  output.append(right);
  output.append(")");
  index = rightEnd - 1;
  return true;
}

bool rewriteUnaryMinus(const std::string &input,
                       std::string &output,
                       size_t &index,
                       const TextFilterOptions &options) {
  if (input[index] != '-') {
    return false;
  }
  if (index + 1 >= input.size()) {
    return false;
  }
  if (isDigitChar(input[index + 1])) {
    return false;
  }
  if (!isUnaryPrefixPosition(input, index)) {
    return false;
  }
  if (input[index + 1] == '(') {
    output.append("negate");
    return true;
  }
  size_t rightStart = index + 1;
  size_t rightEnd = rightStart;
  while (rightEnd < input.size() && isTokenChar(input[rightEnd])) {
    ++rightEnd;
  }
  if (rightEnd == rightStart) {
    return false;
  }
  std::string right = input.substr(rightStart, rightEnd - rightStart);
  if (options.implicitI32Suffix) {
    right = maybeAppendI32(right);
  }
  output.append("negate(");
  output.append(right);
  output.append(")");
  index = rightEnd - 1;
  return true;
}
} // namespace

bool TextFilterPipeline::apply(const std::string &input,
                               std::string &output,
                               std::string &error,
                               const TextFilterOptions &options) const {
  output.clear();
  output.reserve(input.size());
  error.clear();

  auto rewriteBinaryPair = [&](size_t &index, char first, char second, const char *name) -> bool {
    if (input[index] != first || index == 0 || index + 1 >= input.size() || input[index + 1] != second) {
      return false;
    }
    if (!isLeftOperandEndChar(input[index - 1]) || index + 2 >= input.size() ||
        !isRightOperandStartChar(input, index + 2)) {
      return false;
    }
    size_t end = output.size();
    size_t start = findLeftTokenStart(output, end);
    if (start == end) {
      return false;
    }
    std::string left = output.substr(start, end - start);
    left = stripOuterParens(left);
    if (options.implicitI32Suffix) {
      left = maybeAppendI32(left);
    }
    size_t rightStart = index + 2;
    size_t rightEnd = findRightTokenEnd(input, rightStart);
    if (rightEnd == rightStart) {
      return false;
    }
    std::string right = input.substr(rightStart, rightEnd - rightStart);
    right = stripOuterParens(right);
    if (options.implicitI32Suffix) {
      right = maybeAppendI32(right);
    }
    output.erase(start);
    output.append(name);
    output.append("(");
    output.append(left);
    output.append(", ");
    output.append(right);
    output.append(")");
    index = rightEnd - 1;
    return true;
  };

  auto rewriteBinary = [&](size_t &index, char op, const char *name) -> bool {
    if (input[index] != op || index == 0 || index + 1 >= input.size()) {
      return false;
    }
    if (!isLeftOperandEndChar(input[index - 1]) || !isRightOperandStartChar(input, index + 1)) {
      return false;
    }
    size_t end = output.size();
    size_t start = findLeftTokenStart(output, end);
    if (start == end) {
      return false;
    }
    std::string left = output.substr(start, end - start);
    left = stripOuterParens(left);
    if (options.implicitI32Suffix) {
      left = maybeAppendI32(left);
    }
    size_t rightStart = index + 1;
    size_t rightEnd = findRightTokenEnd(input, rightStart);
    if (rightEnd == rightStart) {
      return false;
    }
    std::string right = input.substr(rightStart, rightEnd - rightStart);
    right = stripOuterParens(right);
    if (options.implicitI32Suffix) {
      right = maybeAppendI32(right);
    }
    output.erase(start);
    output.append(name);
    output.append("(");
    output.append(left);
    output.append(", ");
    output.append(right);
    output.append(")");
    index = rightEnd - 1;
    return true;
  };

  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '/' && i + 1 < input.size() && input[i + 1] == '/') {
      size_t end = i + 2;
      while (end < input.size() && input[end] != '\n') {
        ++end;
      }
      output.append(input.substr(i, end - i));
      i = end > 0 ? end - 1 : i;
      continue;
    }

    if (input[i] == 'R' && i + 2 < input.size() && input[i + 1] == '"' && input[i + 2] == '(') {
      size_t end = i + 3;
      bool foundTerminator = false;
      while (end + 1 < input.size()) {
        if (input[end] == ')' && input[end + 1] == '"') {
          end += 2;
          foundTerminator = true;
          break;
        }
        ++end;
      }
      if (!foundTerminator) {
        end = input.size();
      }
      output.append(input.substr(i, end - i));
      i = end > 0 ? end - 1 : i;
      continue;
    }

    if (input[i] == '"' || input[i] == '\'') {
      char quote = input[i];
      size_t end = i + 1;
      while (end < input.size()) {
        if (input[end] == '\\' && end + 1 < input.size()) {
          end += 2;
          continue;
        }
        if (input[end] == quote) {
          ++end;
          break;
        }
        ++end;
      }
      output.append(input.substr(i, end - i));
      i = end > 0 ? end - 1 : i;
      continue;
    }

    if (input[i] == '/' && (i == 0 || isSeparator(input[i - 1]))) {
      size_t start = i;
      size_t end = i + 1;
      while (end < input.size() && !isSeparator(input[end])) {
        ++end;
      }
      output.append(input.substr(start, end - start));
      i = end - 1;
      continue;
    }

    if (options.implicitI32Suffix) {
      bool startsNumber = false;
      if (isDigitChar(input[i]) && (i == 0 || (!isTokenChar(input[i - 1]) && input[i - 1] != '.'))) {
        startsNumber = true;
      } else if (input[i] == '-' && i + 1 < input.size() && isDigitChar(input[i + 1]) &&
                 (i == 0 || isSeparator(input[i - 1]))) {
        startsNumber = true;
      }
      if (startsNumber) {
        size_t start = i;
        if (input[i] == '-') {
          ++i;
        }
        size_t digitsStart = i;
        bool isHex = false;
        if (i + 1 < input.size() && input[i] == '0' && (input[i + 1] == 'x' || input[i + 1] == 'X')) {
          isHex = true;
          i += 2;
          digitsStart = i;
          while (i < input.size() && isHexDigitChar(input[i])) {
            ++i;
          }
        } else {
          while (i < input.size() && isDigitChar(input[i])) {
            ++i;
          }
        }
        size_t digitsEnd = i;
        bool hasExponent = false;
        if (!isHex && i + 1 < input.size() && input[i] == '.' && isDigitChar(input[i + 1])) {
          ++i;
          while (i < input.size() && isDigitChar(input[i])) {
            ++i;
          }
        }
        if (!isHex && i < input.size() && (input[i] == 'e' || input[i] == 'E')) {
          size_t expPos = i + 1;
          if (expPos < input.size() && (input[expPos] == '+' || input[expPos] == '-')) {
            ++expPos;
          }
          size_t expDigits = expPos;
          while (expPos < input.size() && isDigitChar(input[expPos])) {
            ++expPos;
          }
          if (expPos > expDigits) {
            i = expPos;
            hasExponent = true;
          }
        }
        size_t literalEnd = i;
        if (digitsStart == digitsEnd) {
          output.push_back(input[start]);
          i = start;
          continue;
        }
        bool hasFloatSuffix = false;
        size_t floatSuffixLen = 0;
        if (literalEnd + 2 < input.size() && input.compare(literalEnd, 3, "f64") == 0) {
          hasFloatSuffix = true;
          floatSuffixLen = 3;
        } else if (literalEnd + 2 < input.size() && input.compare(literalEnd, 3, "f32") == 0) {
          hasFloatSuffix = true;
          floatSuffixLen = 3;
        } else if (literalEnd < input.size() && input[literalEnd] == 'f') {
          hasFloatSuffix = true;
          floatSuffixLen = 1;
        }
        if (hasFloatSuffix) {
          output.append(input.substr(start, (literalEnd + floatSuffixLen) - start));
          i = literalEnd + floatSuffixLen - 1;
          continue;
        }
        if (literalEnd > digitsEnd || hasExponent) {
          output.append(input.substr(start, literalEnd - start));
          i = literalEnd - 1;
          continue;
        }
        if (literalEnd + 2 < input.size() && input.compare(literalEnd, 3, "i32") == 0) {
          output.append(input.substr(start, literalEnd - start + 3));
          i = literalEnd + 2;
          continue;
        }
        if (literalEnd < input.size()) {
          char next = input[literalEnd];
          if (std::isalpha(static_cast<unsigned char>(next)) || next == '_' || next == '.') {
            output.append(input.substr(start, literalEnd - start));
            i = literalEnd - 1;
            continue;
          }
        }
        output.append(input.substr(start, literalEnd - start));
        output.append("i32");
        i = literalEnd - 1;
        continue;
      }
    }

    if (input[i] == '/' && rewriteBinary(i, '/', "divide")) {
      continue;
    }
    if (rewriteBinaryPair(i, '=', '=', "equal")) {
      continue;
    }
    if (rewriteBinaryPair(i, '!', '=', "not_equal")) {
      continue;
    }
    if (rewriteBinaryPair(i, '>', '=', "greater_equal")) {
      continue;
    }
    if (rewriteBinaryPair(i, '<', '=', "less_equal")) {
      continue;
    }
    if (rewriteBinaryPair(i, '&', '&', "and")) {
      continue;
    }
    if (rewriteBinaryPair(i, '|', '|', "or")) {
      continue;
    }
    if (rewriteBinary(i, '=', "assign")) {
      continue;
    }
    if (rewriteUnaryNot(input, output, i, options)) {
      continue;
    }
    if (rewriteUnaryMinus(input, output, i, options)) {
      continue;
    }
    if (input[i] == '<' && !looksLikeTemplateList(input, i) && rewriteBinary(i, '<', "less_than")) {
      continue;
    }
    if (rewriteBinary(i, '+', "plus")) {
      continue;
    }
    if (rewriteBinary(i, '-', "minus")) {
      continue;
    }
    if (rewriteBinary(i, '*', "multiply")) {
      continue;
    }
    if (!looksLikeTemplateListClose(input, i) && rewriteBinary(i, '>', "greater_than")) {
      continue;
    }

    output.push_back(input[i]);
  }
  return true;
}

} // namespace primec
