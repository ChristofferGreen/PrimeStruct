#include "TextFilterPipelineInternal.h"

#include "TextFilterHelpers.h"

#include <cctype>
#include <string_view>

namespace primec {
using namespace text_filter;

namespace {
struct BinaryOperatorInfo {
  std::string_view token;
  const char *name;
  int precedence;
  bool rightAssociative;
};

constexpr BinaryOperatorInfo kBinaryOperators[] = {
    {"||", "or", 2, false},
    {"&&", "and", 3, false},
    {"==", "equal", 4, false},
    {"!=", "not_equal", 4, false},
    {">=", "greater_equal", 5, false},
    {"<=", "less_equal", 5, false},
    {">", "greater_than", 5, false},
    {"<", "less_than", 5, false},
    {"+", "plus", 6, false},
    {"-", "minus", 6, false},
    {"*", "multiply", 7, false},
    {"/", "divide", 7, false},
    {"=", "assign", 1, true},
};

bool isIdentifierStartChar(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentifierBodyChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
}

bool isBoundaryChar(char c) {
  return c == ',' || c == ';' || c == ')' || c == ']' || c == '}' || c == '{' || c == ':';
}

bool matchNonOperandKeyword(const std::string &input, size_t pos, std::string_view &out) {
  static const std::string_view keywords[] = {"return", "import", "namespace", "if", "else", "for", "while",
                                              "loop"};
  for (const auto &keyword : keywords) {
    if (input.compare(pos, keyword.size(), keyword) != 0) {
      continue;
    }
    if (pos > 0 && isTokenChar(input[pos - 1])) {
      continue;
    }
    size_t end = pos + keyword.size();
    if (end < input.size() && isTokenChar(input[end])) {
      continue;
    }
    out = keyword;
    return true;
  }
  return false;
}

bool matchBinaryOperator(const std::string &input, size_t pos, BinaryOperatorInfo &out) {
  for (const auto &op : kBinaryOperators) {
    if (input.compare(pos, op.token.size(), op.token) != 0) {
      continue;
    }
    if (op.token.size() == 1 && op.token[0] == '/' && pos + 1 < input.size()) {
      if (input[pos + 1] == '/' || input[pos + 1] == '*') {
        return false;
      }
    }
    if (op.token == "<" && looksLikeTemplateList(input, pos)) {
      return false;
    }
    if (op.token == ">" && looksLikeTemplateListClose(input, pos)) {
      return false;
    }
    out = op;
    return true;
  }
  return false;
}

bool parseExpressionWithPrecedence(const std::string &input,
                                   size_t &pos,
                                   int minPrec,
                                   std::string &out,
                                   std::string &error);

bool parsePrimaryExpression(const std::string &input, size_t &pos, std::string &out, std::string &error) {
  size_t start = pos;
  if (start >= input.size()) {
    return false;
  }
  if (input[start] == '(') {
    size_t close = findMatchingClose(input, start, '(', ')');
    if (close == std::string::npos) {
      error = "unterminated parenthesis";
      return false;
    }
    std::string inner = input.substr(start + 1, close - (start + 1));
    std::string rewritten;
    if (!rewriteBinaryOperatorsWithPrecedence(inner, rewritten, error)) {
      return false;
    }
    out = "(" + rewritten + ")";
    pos = close + 1;
    return true;
  }
  if (input[start] == '"' || input[start] == '\'') {
    size_t end = skipQuotedForward(input, start);
    if (end == std::string::npos) {
      error = "unterminated string literal";
      return false;
    }
    size_t suffixStart = end;
    size_t suffixEnd = suffixStart;
    if (suffixStart < input.size() && isStringSuffixStart(input[suffixStart])) {
      ++suffixEnd;
      while (suffixEnd < input.size() && isStringSuffixBody(input[suffixEnd])) {
        ++suffixEnd;
      }
      end = suffixEnd;
    }
    out = input.substr(start, end - start);
    pos = end;
    return true;
  }
  if (input[start] == 'R' && start + 2 < input.size() && input[start + 1] == '"' && input[start + 2] == '(') {
    size_t close = input.find(")\"", start + 3);
    if (close == std::string::npos) {
      error = "unterminated raw string literal";
      return false;
    }
    size_t end = close + 2;
    size_t suffixEnd = end;
    if (suffixEnd < input.size() && isStringSuffixStart(input[suffixEnd])) {
      ++suffixEnd;
      while (suffixEnd < input.size() && isStringSuffixBody(input[suffixEnd])) {
        ++suffixEnd;
      }
      end = suffixEnd;
    }
    out = input.substr(start, end - start);
    pos = end;
    return true;
  }
  if (isDigitChar(input[start]) || (input[start] == '-' && start + 1 < input.size() && isDigitChar(input[start + 1]))) {
    size_t numberStart = start;
    size_t end = start;
    if (input[start] == '-') {
      end = findRightTokenEnd(input, start + 1);
    } else {
      end = findRightTokenEnd(input, start);
    }
    if (end == std::string::npos || end == numberStart) {
      return false;
    }
    out = input.substr(numberStart, end - numberStart);
    pos = end;
    return true;
  }
  std::string token;
  if (input[start] == '/' && !isCommentStart(input, start)) {
    if (start == 0 || isSeparator(input[start - 1]) || input[start - 1] == '.') {
      size_t end = start + 1;
      while (end < input.size() && !isSeparator(input[end])) {
        ++end;
      }
      token = input.substr(start, end - start);
      pos = end;
    } else {
      return false;
    }
  } else {
    if (!isIdentifierStartChar(input[start])) {
      return false;
    }
    size_t end = start + 1;
    while (end < input.size() && isIdentifierBodyChar(input[end])) {
      if (input[end] == '.' && end + 1 < input.size() && input[end + 1] == '/') {
        break;
      }
      ++end;
    }
    token = input.substr(start, end - start);
    pos = end;
  }
  while (true) {
    size_t scan = pos;
    while (scan < input.size() && std::isspace(static_cast<unsigned char>(input[scan]))) {
      ++scan;
    }
    const size_t lineBreak = input.find_first_of("\r\n", pos);
    const bool sawLineBreak = lineBreak != std::string::npos && lineBreak < scan;
    if (scan < input.size() && isCommentStart(input, scan)) {
      break;
    }
    if (sawLineBreak && scan < input.size() && input[scan] == '[') {
      break;
    }
    if (scan < input.size() && input[scan] == '<' && looksLikeTemplateList(input, scan)) {
      size_t close = findMatchingClose(input, scan, '<', '>');
      if (close == std::string::npos) {
        error = "unterminated template list";
        return false;
      }
      token.append(input.substr(scan, close - scan + 1));
      pos = close + 1;
      continue;
    }
    if (scan < input.size() && (input[scan] == '(' || input[scan] == '[')) {
      char openChar = input[scan];
      char closeChar = openChar == '(' ? ')' : ']';
      size_t close = findMatchingClose(input, scan, openChar, closeChar);
      if (close == std::string::npos) {
        error = openChar == '(' ? "unterminated call arguments" : "unterminated index expression";
        return false;
      }
      std::string inner = input.substr(scan + 1, close - (scan + 1));
      std::string rewritten;
      if (!rewriteBinaryOperatorsWithPrecedence(inner, rewritten, error)) {
        return false;
      }
      token.push_back(openChar);
      token.append(rewritten);
      token.push_back(closeChar);
      pos = close + 1;
      continue;
    }
    break;
  }
  out = token;
  return true;
}

bool parseExpressionWithPrecedence(const std::string &input,
                                   size_t &pos,
                                   int minPrec,
                                   std::string &out,
                                   std::string &error) {
  size_t startPos = pos;
  while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
    ++pos;
  }
  if (pos < input.size() && isCommentStart(input, pos)) {
    return false;
  }
  std::string left;
  if (!parsePrimaryExpression(input, pos, left, error)) {
    return false;
  }
  while (pos <= input.size()) {
    size_t whitespaceStart = pos;
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
      ++pos;
    }
    const bool sawLineBreak =
        input.find_first_of("\r\n", whitespaceStart) != std::string::npos &&
        input.find_first_of("\r\n", whitespaceStart) < pos;
    if (pos >= input.size()) {
      pos = whitespaceStart;
      break;
    }
    if (isBoundaryChar(input[pos]) || isCommentStart(input, pos)) {
      pos = whitespaceStart;
      break;
    }
    if (sawLineBreak && input[pos] == '/' && !isCommentStart(input, pos) && pos + 1 < input.size() &&
        isIdentifierStartChar(input[pos + 1])) {
      pos = whitespaceStart;
      break;
    }
    BinaryOperatorInfo op;
    if (!matchBinaryOperator(input, pos, op)) {
      pos = whitespaceStart;
      break;
    }
    if (op.precedence < minPrec) {
      pos = whitespaceStart;
      break;
    }
    pos += op.token.size();
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
      ++pos;
    }
    if (pos < input.size() && isCommentStart(input, pos)) {
      return false;
    }
    std::string right;
    int nextMin = op.precedence + (op.rightAssociative ? 0 : 1);
    if (!parseExpressionWithPrecedence(input, pos, nextMin, right, error)) {
      return false;
    }
    std::string leftValue = stripOuterParens(left);
    std::string rightValue = stripOuterParens(right);
    left = std::string(op.name) + "(" + leftValue + ", " + rightValue + ")";
  }
  if (pos == startPos) {
    return false;
  }
  out = left;
  return true;
}

} // namespace

bool rewriteBinaryOperatorsWithPrecedence(const std::string &input, std::string &output, std::string &error) {
  output.clear();
  size_t pos = 0;
  while (pos < input.size()) {
    std::string_view keyword;
    if (matchNonOperandKeyword(input, pos, keyword)) {
      output.append(keyword);
      pos += keyword.size();
      if (keyword == "import") {
        size_t end = pos;
        while (end < input.size() && input[end] != '\n' && input[end] != ';') {
          ++end;
        }
        output.append(input.substr(pos, end - pos));
        pos = end;
        if (pos < input.size() && input[pos] == ';') {
          output.push_back(';');
          ++pos;
        }
      }
      continue;
    }
    if (input[pos] == '/' && pos + 1 < input.size()) {
      if (input[pos + 1] == '/') {
        size_t end = pos + 2;
        while (end < input.size() && input[end] != '\n') {
          ++end;
        }
        output.append(input.substr(pos, end - pos));
        pos = end;
        continue;
      }
      if (input[pos + 1] == '*') {
        size_t end = input.find("*/", pos + 2);
        if (end == std::string::npos) {
          error = "unterminated block comment";
          return false;
        }
        end += 2;
        output.append(input.substr(pos, end - pos));
        pos = end;
        continue;
      }
    }
    if (std::isspace(static_cast<unsigned char>(input[pos]))) {
      output.push_back(input[pos]);
      ++pos;
      continue;
    }
    size_t exprPos = pos;
    std::string rewritten;
    if (parseExpressionWithPrecedence(input, exprPos, 1, rewritten, error)) {
      output.append(rewritten);
      pos = exprPos;
      continue;
    }
    output.push_back(input[pos]);
    ++pos;
  }
  return true;
}

} // namespace primec
