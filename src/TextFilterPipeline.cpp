#include "primec/TextFilterPipeline.h"
#include "primec/StringLiteral.h"

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

bool isStringSuffixStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isStringSuffixBody(char c) {
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
  if (isSeparator(prev)) {
    return true;
  }
  switch (prev) {
  case '+':
  case '-':
  case '*':
  case '/':
  case '=':
  case '<':
  case '>':
  case '&':
  case '|':
  case '!':
    return true;
  default:
    return false;
  }
}

bool isRightOperandStartChar(const std::string &input, size_t index) {
  char c = input[index];
  if (isOperatorTokenChar(c) || c == '(' || c == '"' || c == '\'' || c == '[' || c == '{') {
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
  char sign = text[index];
  if (sign != '-' && sign != '+') {
    return false;
  }
  char prev = text[index - 1];
  if (prev != 'e' && prev != 'E') {
    return false;
  }
  if (index < 2) {
    return false;
  }
  char beforeExp = text[index - 2];
  if (!isDigitChar(beforeExp)) {
    return false;
  }
  return true;
}

bool looksLikeTemplateList(const std::string &input, size_t index);

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
  size_t suffixStart = end;
  while (suffixStart > 0 && isStringSuffixBody(text[suffixStart - 1])) {
    --suffixStart;
  }
  if (suffixStart < end && isStringSuffixStart(text[suffixStart])) {
    if (suffixStart > 0 && (text[suffixStart - 1] == '"' || text[suffixStart - 1] == '\'')) {
      size_t quoteEnd = suffixStart - 1;
      size_t quoteStart = findQuotedStart(text, quoteEnd);
      if (quoteStart != std::string::npos) {
        return quoteStart;
      }
    }
  }
  char tail = text[end - 1];
  if (tail == ')' || tail == ']' || tail == '}') {
    char openChar = tail == ')' ? '(' : (tail == ']' ? '[' : '{');
    size_t openPos = findMatchingOpen(text, end - 1, openChar, tail);
    if (openPos == std::string::npos) {
      return end;
    }
    start = openPos;
    if (tail == ')' && start > 0 && text[start - 1] == '>') {
      size_t templateStart = findMatchingOpen(text, start - 1, '<', '>');
      if (templateStart != std::string::npos) {
        start = templateStart;
      }
    }
    while (start > 0 && isOperatorTokenChar(text[start - 1])) {
      --start;
    }
    if (start > 0 && text[start - 1] == '-' && isUnaryPrefixPosition(text, start - 1)) {
      --start;
    }
    return start;
  }
  if (tail == '"' || tail == '\'') {
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
  if (lead == '"' || lead == '\'') {
    size_t end = skipQuotedForward(text, start);
    if (end == std::string::npos) {
      return start;
    }
    if (end < text.size() && isStringSuffixStart(text[end])) {
      size_t suffixEnd = end + 1;
      while (suffixEnd < text.size() && isStringSuffixBody(text[suffixEnd])) {
        ++suffixEnd;
      }
      end = suffixEnd;
    }
    return end;
  }
  size_t pos = start;
  while (pos < text.size()) {
    if (isOperatorTokenChar(text[pos]) || isExponentSign(text, pos)) {
      ++pos;
      continue;
    }
    break;
  }
  if (pos < text.size() && text[pos] == '<' && looksLikeTemplateList(text, pos)) {
    size_t closePos = findMatchingClose(text, pos, '<', '>');
    if (closePos != std::string::npos) {
      pos = closePos + 1;
    }
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

std::string normalizeUnaryOperand(const std::string &operand) {
  if (operand.size() > 1 && operand[0] == '-' && !isDigitChar(operand[1])) {
    std::string inner = operand.substr(1);
    inner = stripOuterParens(inner);
    return "negate(" + inner + ")";
  }
  return operand;
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
  size_t start = 0;
  if (token[0] == '-') {
    if (token.size() == 1) {
      return token;
    }
    start = 1;
  }
  bool isDecimal = true;
  for (size_t i = start; i < token.size(); ++i) {
    if (!isDigitChar(token[i])) {
      isDecimal = false;
      break;
    }
  }
  if (isDecimal) {
    return token + "i32";
  }
  if (token.size() > start + 2 && token[start] == '0' && (token[start + 1] == 'x' || token[start + 1] == 'X')) {
    for (size_t i = start + 2; i < token.size(); ++i) {
      if (!isHexDigitChar(token[i])) {
        return token;
      }
    }
    return token + "i32";
  }
  return token;
}

std::string maybeAppendUtf8(const std::string &token) {
  if (token.empty()) {
    return token;
  }
  std::string literalText;
  std::string suffix;
  if (!splitStringLiteralToken(token, literalText, suffix)) {
    return token;
  }
  if (!suffix.empty()) {
    return token;
  }
  return token + "utf8";
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
  size_t bangEnd = index;
  while (bangEnd + 1 < input.size() && input[bangEnd + 1] == '!') {
    if (bangEnd + 2 < input.size() && input[bangEnd + 2] == '=') {
      break;
    }
    if (!isUnaryPrefixPosition(input, bangEnd + 1)) {
      break;
    }
    ++bangEnd;
  }
  size_t rightStart = bangEnd + 1;
  if (!isRightOperandStartChar(input, rightStart)) {
    return false;
  }
  size_t rightEnd = findRightTokenEnd(input, rightStart);
  if (rightEnd == rightStart) {
    return false;
  }
  std::string right = input.substr(rightStart, rightEnd - rightStart);
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  if (options.hasFilter("implicit-i32")) {
    right = maybeAppendI32(right);
  }
  if (options.hasFilter("implicit-utf8")) {
    right = maybeAppendUtf8(right);
  }
  for (size_t i = index; i <= bangEnd; ++i) {
    output.append("not(");
  }
  output.append(right);
  for (size_t i = index; i <= bangEnd; ++i) {
    output.append(")");
  }
  index = rightEnd - 1;
  return true;
}

bool rewriteUnaryAddressOf(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  (void)options;
  if (input[index] != '&' || index + 1 >= input.size() || input[index + 1] == '&') {
    return false;
  }
  if (!isUnaryPrefixPosition(input, index)) {
    return false;
  }
  if (input[index + 1] == '(') {
    output.append("location");
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
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  output.append("location(");
  output.append(right);
  output.append(")");
  index = rightEnd - 1;
  return true;
}

bool rewriteUnaryDeref(const std::string &input, std::string &output, size_t &index, const TextFilterOptions &options) {
  (void)options;
  if (input[index] != '*' || index + 1 >= input.size()) {
    return false;
  }
  if (!isUnaryPrefixPosition(input, index)) {
    return false;
  }
  if (input[index + 1] == '(') {
    output.append("dereference");
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
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  output.append("dereference(");
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
  if (options.hasFilter("implicit-i32")) {
    right = maybeAppendI32(right);
  }
  if (options.hasFilter("implicit-utf8")) {
    right = maybeAppendUtf8(right);
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
  const bool enableCollections = options.hasFilter("collections");
  const bool enableOperators = options.hasFilter("operators");
  const bool enableImplicitUtf8 = options.hasFilter("implicit-utf8");

  auto rewriteCollectionLiteral = [&](size_t &index) -> bool {
    if (!enableCollections) {
      return false;
    }
    const std::string names[] = {"array", "map"};
    for (const auto &name : names) {
      const size_t nameLen = name.size();
      if (index + nameLen > input.size()) {
        continue;
      }
      if (input.compare(index, nameLen, name) != 0) {
        continue;
      }
      if (index > 0 && (isTokenChar(input[index - 1]) || input[index - 1] == '/')) {
        continue;
      }
      if (index + nameLen < input.size() && isTokenChar(input[index + nameLen])) {
        continue;
      }
      size_t pos = index + nameLen;
      while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
        ++pos;
      }
      if (pos < input.size() && input[pos] == '<') {
        size_t close = findMatchingClose(input, pos, '<', '>');
        if (close == std::string::npos) {
          error = "unterminated template list in collection literal";
          return false;
        }
        pos = close + 1;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
          ++pos;
        }
      }
      if (pos >= input.size() || input[pos] != '{') {
        continue;
      }
      size_t closeBrace = findMatchingClose(input, pos, '{', '}');
      if (closeBrace == std::string::npos) {
        error = "unterminated collection literal";
        return false;
      }
      std::string inner = input.substr(pos + 1, closeBrace - (pos + 1));
      if (name == "map") {
        std::string rewritten;
        rewritten.reserve(inner.size());
        size_t scan = 0;
        bool entryHasPairEquals = false;
        int parenDepth = 0;
        int braceDepth = 0;
        int bracketDepth = 0;
        while (scan < inner.size()) {
          char c = inner[scan];
          if (std::isspace(static_cast<unsigned char>(c))) {
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == '"' || c == '\'') {
            size_t end = skipQuotedForward(inner, scan);
            if (end == std::string::npos) {
              error = "unterminated string in map literal";
              return false;
            }
            rewritten.append(inner.substr(scan, end - scan));
            scan = end;
            continue;
          }
          if (c == '(') {
            ++parenDepth;
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == ')') {
            if (parenDepth > 0) {
              --parenDepth;
            }
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == '{') {
            ++braceDepth;
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == '}') {
            if (braceDepth > 0) {
              --braceDepth;
            }
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == '[') {
            ++bracketDepth;
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == ']') {
            if (bracketDepth > 0) {
              --bracketDepth;
            }
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == ',' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0) {
            entryHasPairEquals = false;
            rewritten.push_back(c);
            ++scan;
            continue;
          }
          if (c == '=') {
            char prev = scan > 0 ? inner[scan - 1] : '\0';
            char next = scan + 1 < inner.size() ? inner[scan + 1] : '\0';
            size_t prevIndex = scan;
            while (prevIndex > 0 && std::isspace(static_cast<unsigned char>(inner[prevIndex - 1]))) {
              --prevIndex;
            }
            if (prevIndex > 0) {
              prev = inner[prevIndex - 1];
            }
            size_t nextIndex = scan + 1;
            while (nextIndex < inner.size() && std::isspace(static_cast<unsigned char>(inner[nextIndex]))) {
              ++nextIndex;
            }
            if (nextIndex < inner.size()) {
              next = inner[nextIndex];
            }
            if (prev == '=' || prev == '!' || prev == '<' || prev == '>' || next == '=') {
              rewritten.push_back(c);
              ++scan;
              continue;
            }
            if (parenDepth > 0 || braceDepth > 0 || bracketDepth > 0) {
              rewritten.push_back(c);
              ++scan;
              continue;
            }
            if (!entryHasPairEquals) {
              rewritten.append(", ");
              entryHasPairEquals = true;
            } else {
              rewritten.push_back(c);
            }
            ++scan;
            continue;
          }
          rewritten.push_back(c);
          ++scan;
        }
        inner.swap(rewritten);
      }
      std::string filteredInner;
      std::string innerError;
      if (!apply(inner, filteredInner, innerError, options)) {
        error = innerError;
        return false;
      }
      output.append(input.substr(index, pos - index));
      output.append("(");
      output.append(filteredInner);
      output.append(")");
      index = closeBrace;
      return true;
    }
    return false;
  };

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
    left = normalizeUnaryOperand(left);
    if (options.hasFilter("implicit-i32")) {
      left = maybeAppendI32(left);
    }
    if (options.hasFilter("implicit-utf8")) {
      left = maybeAppendUtf8(left);
    }
    size_t rightStart = index + 2;
    size_t rightEnd = findRightTokenEnd(input, rightStart);
    if (rightEnd == rightStart) {
      return false;
    }
    std::string right = input.substr(rightStart, rightEnd - rightStart);
    right = stripOuterParens(right);
    right = normalizeUnaryOperand(right);
    if (options.hasFilter("implicit-i32")) {
      right = maybeAppendI32(right);
    }
    if (options.hasFilter("implicit-utf8")) {
      right = maybeAppendUtf8(right);
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
    if (isExponentSign(input, index)) {
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
    left = normalizeUnaryOperand(left);
    if (options.hasFilter("implicit-i32")) {
      left = maybeAppendI32(left);
    }
    if (options.hasFilter("implicit-utf8")) {
      left = maybeAppendUtf8(left);
    }
    size_t rightStart = index + 1;
    size_t rightEnd = findRightTokenEnd(input, rightStart);
    if (rightEnd == rightStart) {
      return false;
    }
    std::string right = input.substr(rightStart, rightEnd - rightStart);
    right = stripOuterParens(right);
    right = normalizeUnaryOperand(right);
    if (options.hasFilter("implicit-i32")) {
      right = maybeAppendI32(right);
    }
    if (options.hasFilter("implicit-utf8")) {
      right = maybeAppendUtf8(right);
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
      size_t suffixStart = end;
      size_t suffixEnd = suffixStart;
      if (foundTerminator && suffixStart < input.size() && isStringSuffixStart(input[suffixStart])) {
        ++suffixEnd;
        while (suffixEnd < input.size() && isStringSuffixBody(input[suffixEnd])) {
          ++suffixEnd;
        }
        output.append(input.substr(i, suffixEnd - i));
        i = suffixEnd > 0 ? suffixEnd - 1 : i;
        continue;
      }
      output.append(input.substr(i, end - i));
      if (foundTerminator && enableImplicitUtf8) {
        output.append("utf8");
      }
      i = end > 0 ? end - 1 : i;
      continue;
    }

    if (input[i] == '"' || input[i] == '\'') {
      char quote = input[i];
      size_t end = i + 1;
      bool foundTerminator = false;
      while (end < input.size()) {
        if (input[end] == '\\' && end + 1 < input.size()) {
          end += 2;
          continue;
        }
        if (input[end] == quote) {
          ++end;
          foundTerminator = true;
          break;
        }
        ++end;
      }
      size_t suffixStart = end;
      size_t suffixEnd = suffixStart;
      if (foundTerminator && suffixStart < input.size() && isStringSuffixStart(input[suffixStart])) {
        ++suffixEnd;
        while (suffixEnd < input.size() && isStringSuffixBody(input[suffixEnd])) {
          ++suffixEnd;
        }
        output.append(input.substr(i, suffixEnd - i));
        i = suffixEnd > 0 ? suffixEnd - 1 : i;
        continue;
      }
      output.append(input.substr(i, end - i));
      if (foundTerminator && enableImplicitUtf8) {
        output.append("utf8");
      }
      i = end > 0 ? end - 1 : i;
      continue;
    }

    if (rewriteCollectionLiteral(i)) {
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

    if (options.hasFilter("implicit-i32")) {
      bool startsNumber = false;
      if (isDigitChar(input[i]) && (i == 0 || (!isTokenChar(input[i - 1]) && input[i - 1] != '.'))) {
        startsNumber = true;
      } else if (input[i] == '-' && i + 1 < input.size() && isDigitChar(input[i + 1]) &&
                 isUnaryPrefixPosition(input, i)) {
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
        if (literalEnd + 2 < input.size() &&
            (input.compare(literalEnd, 3, "i32") == 0 || input.compare(literalEnd, 3, "i64") == 0 ||
             input.compare(literalEnd, 3, "u64") == 0)) {
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

    if (enableOperators) {
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
      if (rewriteUnaryAddressOf(input, output, i, options)) {
        continue;
      }
      if (rewriteUnaryDeref(input, output, i, options)) {
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
    }

    output.push_back(input[i]);
  }
  return true;
}

} // namespace primec
