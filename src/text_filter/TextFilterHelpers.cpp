#include "TextFilterHelpers.h"

#include "primec/StringLiteral.h"

#include <cctype>

namespace primec::text_filter {

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

bool isCommentStart(const std::string &input, size_t index) {
  if (index + 1 >= input.size()) {
    return false;
  }
  if (input[index] != '/') {
    return false;
  }
  return input[index + 1] == '/' || input[index + 1] == '*';
}

bool isCommentEnd(const std::string &input, size_t index) {
  if (index == 0 || index >= input.size()) {
    return false;
  }
  return input[index] == '/' && input[index - 1] == '*';
}

bool isNumericSeparator(const std::string &text, size_t index) {
  if (index == 0 || index + 1 >= text.size()) {
    return false;
  }
  if (text[index] != ',') {
    return false;
  }
  if (!isDigitChar(text[index - 1]) || !isDigitChar(text[index + 1])) {
    return false;
  }
  size_t scan = index;
  while (scan > 0 && (isDigitChar(text[scan - 1]) || text[scan - 1] == ',')) {
    --scan;
  }
  if (scan > 0) {
    char prev = text[scan - 1];
    if (isTokenChar(prev) || prev == '.' || prev == 'e' || prev == 'E') {
      return false;
    }
    if ((prev == '+' || prev == '-') && scan >= 2 && (text[scan - 2] == 'e' || text[scan - 2] == 'E')) {
      return false;
    }
  }
  return true;
}

bool isUnaryPrefixPosition(const std::string &input, size_t index) {
  if (index == 0) {
    return true;
  }
  char prev = input[index - 1];
  if (prev == '/' && index >= 2 && input[index - 2] == '*') {
    return true;
  }
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
  if (isCommentStart(input, index)) {
    return false;
  }
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
  const bool allowEscape = quote == '"';
  size_t pos = endQuote;
  while (pos > 0) {
    --pos;
    if (text[pos] == quote && (!allowEscape || !isEscaped(text, pos))) {
      return pos;
    }
  }
  return std::string::npos;
}

size_t skipQuotedForward(const std::string &text, size_t start) {
  char quote = text[start];
  size_t pos = start + 1;
  while (pos < text.size()) {
    if (quote == '"' && text[pos] == '\\' && pos + 1 < text.size()) {
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
    if (isOperatorTokenChar(c) || isExponentSign(text, start - 1) || isNumericSeparator(text, start - 1)) {
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
    if (isOperatorTokenChar(text[pos]) || isExponentSign(text, pos) || isNumericSeparator(text, pos)) {
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

namespace {
bool findTemplateListClose(const std::string &input, size_t index, size_t &closeOut) {
  if (index >= input.size() || input[index] != '<') {
    return false;
  }
  auto isTemplateTokenChar = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/';
  };
  size_t pos = index + 1;
  int depth = 0;
  bool sawToken = false;
  bool expectToken = true;
  while (pos < input.size()) {
    char c = input[pos];
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++pos;
      continue;
    }
    if (c == ',') {
      expectToken = true;
      ++pos;
      continue;
    }
    if (c == '<') {
      if (expectToken) {
        return false;
      }
      ++depth;
      expectToken = true;
      ++pos;
      continue;
    }
    if (c == '>') {
      if (depth > 0) {
        --depth;
        expectToken = false;
        ++pos;
        continue;
      }
      if (sawToken && !expectToken) {
        closeOut = pos;
        return true;
      }
      return false;
    }
    if (isTemplateTokenChar(c)) {
      sawToken = true;
      expectToken = false;
      while (pos < input.size() && isTemplateTokenChar(input[pos])) {
        ++pos;
      }
      continue;
    }
    return false;
  }
  return false;
}
} // namespace

bool looksLikeTemplateList(const std::string &input, size_t index) {
  size_t close = 0;
  return findTemplateListClose(input, index, close);
}

bool looksLikeTemplateListClose(const std::string &input, size_t index) {
  if (index >= input.size() || input[index] != '>') {
    return false;
  }
  int depth = 0;
  size_t pos = index;
  while (pos > 0) {
    --pos;
    char c = input[pos];
    if (c == '>') {
      ++depth;
      continue;
    }
    if (c == '<') {
      if (depth == 0) {
        size_t close = 0;
        return findTemplateListClose(input, pos, close) && close == index;
      }
      --depth;
      continue;
    }
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
    char c = token[i];
    if (isDigitChar(c)) {
      continue;
    }
    if (c == ',' && i > start && i + 1 < token.size() && isDigitChar(token[i - 1]) && isDigitChar(token[i + 1])) {
      continue;
    }
    if (!isDigitChar(c)) {
      isDecimal = false;
      break;
    }
  }
  if (isDecimal) {
    return token + "i32";
  }
  if (token.size() > start + 2 && token[start] == '0' && (token[start + 1] == 'x' || token[start + 1] == 'X')) {
    for (size_t i = start + 2; i < token.size(); ++i) {
      char c = token[i];
      if (isHexDigitChar(c)) {
        continue;
      }
      if (c == ',' && i > start + 2 && i + 1 < token.size() && isHexDigitChar(token[i - 1]) &&
          isHexDigitChar(token[i + 1])) {
        continue;
      }
      if (!isHexDigitChar(c)) {
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

namespace {
bool rewriteUnaryMutation(const std::string &input,
                          std::string &output,
                          size_t &index,
                          const TextFilterOptions &options,
                          char op,
                          const char *name) {
  if (input[index] != op || index + 1 >= input.size() || input[index + 1] != op) {
    return false;
  }
  const size_t rightStart = index + 2;
  if (isUnaryPrefixPosition(input, index)) {
    if (rightStart >= input.size()) {
      return false;
    }
    if (input[rightStart] == '(') {
      output.append(name);
      index += 1;
      return true;
    }
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
    output.append(name);
    output.append("(");
    output.append(right);
    output.append(")");
    index = rightEnd - 1;
    return true;
  }
  if (index == 0 || !isLeftOperandEndChar(input[index - 1])) {
    return false;
  }
  if (isCommentEnd(input, index - 1)) {
    return false;
  }
  if (rightStart < input.size() && isRightOperandStartChar(input, rightStart)) {
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
  output.erase(start);
  output.append(name);
  output.append("(");
  output.append(left);
  output.append(")");
  index += 1;
  return true;
}
} // namespace

bool rewriteUnaryIncrement(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteUnaryMutation(input, output, index, options, '+', "increment");
}

bool rewriteUnaryDecrement(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteUnaryMutation(input, output, index, options, '-', "decrement");
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

} // namespace primec::text_filter
