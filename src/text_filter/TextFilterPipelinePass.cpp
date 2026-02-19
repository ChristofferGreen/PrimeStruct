#include "TextFilterPipelineInternal.h"

#include "TextFilterHelpers.h"
#include "primec/TextFilterPipeline.h"

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
  static const std::string_view keywords[] = {"return", "import", "include", "namespace",
                                              "if",     "else",   "for",     "while",
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

bool rewriteBinaryOperatorsWithPrecedence(const std::string &input, std::string &output, std::string &error);

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
  if (input[start] == '/' && !isCommentStart(input, start)) {
    if (start == 0 || isSeparator(input[start - 1])) {
      size_t end = start + 1;
      while (end < input.size() && !isSeparator(input[end])) {
        ++end;
      }
      out = input.substr(start, end - start);
      pos = end;
      return true;
    }
  }
  if (!isIdentifierStartChar(input[start])) {
    return false;
  }
  size_t end = start + 1;
  while (end < input.size() && isIdentifierBodyChar(input[end])) {
    ++end;
  }
  std::string token = input.substr(start, end - start);
  pos = end;
  while (true) {
    size_t scan = pos;
    while (scan < input.size() && std::isspace(static_cast<unsigned char>(input[scan]))) {
      ++scan;
    }
    if (scan < input.size() && isCommentStart(input, scan)) {
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
    if (pos >= input.size()) {
      pos = whitespaceStart;
      break;
    }
    if (isBoundaryChar(input[pos]) || isCommentStart(input, pos)) {
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

bool rewriteBinaryOperatorsWithPrecedence(const std::string &input, std::string &output, std::string &error) {
  output.clear();
  size_t pos = 0;
  while (pos < input.size()) {
    std::string_view keyword;
    if (matchNonOperandKeyword(input, pos, keyword)) {
      output.append(keyword);
      pos += keyword.size();
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
} // namespace

bool applyPass(const std::string &input,
               std::string &output,
               std::string &error,
               const primec::TextFilterOptions &options) {
  output.clear();
  output.reserve(input.size());
  error.clear();
  if (options.hasFilter("append_operators")) {
    return appendOperatorsTransform(input, output, error);
  }
  const bool enableCollections = options.hasFilter("collections");
  const bool enableOperators = options.hasFilter("operators");
  const bool enableImplicitUtf8 = options.hasFilter("implicit-utf8");
  auto isNonOperandKeyword = [](std::string_view token) -> bool {
    return token == "return" || token == "import" || token == "include" || token == "namespace";
  };
  auto hasNonOperandKeywordBefore = [&](size_t index) -> bool {
    if (index == 0) {
      return false;
    }
    size_t end = index;
    while (end > 0 && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
      --end;
    }
    size_t start = end;
    while (start > 0 && isTokenChar(input[start - 1])) {
      --start;
    }
    if (start == end) {
      return false;
    }
    return isNonOperandKeyword(std::string_view(input.data() + start, end - start));
  };

	  auto rewriteCollectionLiteral = [&](size_t &index) -> bool {
	    if (!enableCollections) {
	      return false;
	    }
    const std::string names[] = {"array", "vector", "map"};
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
	      bool sawTemplateList = false;
	      if (pos < input.size() && input[pos] == '<') {
	        size_t close = findMatchingClose(input, pos, '<', '>');
	        if (close == std::string::npos) {
	          error = "unterminated template list in collection literal";
	          return false;
	        }
	        pos = close + 1;
	        sawTemplateList = true;
	        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
	          ++pos;
	        }
	      }
	      if (pos >= input.size() || (input[pos] != '{' && input[pos] != '[')) {
	        continue;
	      }
	      const char openChar = input[pos];
	      const char closeChar = (openChar == '{') ? '}' : ']';
	      if (openChar == '[' && !sawTemplateList) {
	        continue;
	      }
	      size_t closeBrace = findMatchingClose(input, pos, openChar, closeChar);
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
        bool sawValueBoundary = false;
        bool pendingPairSeparator = false;
        int parenDepth = 0;
        int braceDepth = 0;
        int bracketDepth = 0;
        while (scan < inner.size()) {
          char c = inner[scan];
          if (std::isspace(static_cast<unsigned char>(c))) {
            if (parenDepth == 0 && braceDepth == 0 && bracketDepth == 0) {
              size_t prevIndex = scan;
              while (prevIndex > 0 && std::isspace(static_cast<unsigned char>(inner[prevIndex - 1]))) {
                --prevIndex;
              }
              char prev = prevIndex > 0 ? inner[prevIndex - 1] : '\0';
              size_t nextIndex = scan + 1;
              while (nextIndex < inner.size() && std::isspace(static_cast<unsigned char>(inner[nextIndex]))) {
                ++nextIndex;
              }
              if (nextIndex < inner.size()) {
                if (isLeftOperandEndChar(prev) && isRightOperandStartChar(inner, nextIndex)) {
                  if (entryHasPairEquals) {
                    sawValueBoundary = true;
                  } else if (pendingPairSeparator) {
                    rewritten.append(", ");
                    pendingPairSeparator = false;
                    scan = nextIndex;
                    continue;
                  } else if (inner[nextIndex] == '/' && nextIndex + 1 < inner.size() &&
                             (inner[nextIndex + 1] == '/' || inner[nextIndex + 1] == '*')) {
                    pendingPairSeparator = true;
                  } else {
                    rewritten.append(", ");
                    scan = nextIndex;
                    continue;
                  }
                }
              }
            }
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
          if (c == '/' && scan + 1 < inner.size()) {
            if (inner[scan + 1] == '/') {
              size_t end = inner.find('\n', scan + 2);
              if (end == std::string::npos) {
                end = inner.size();
              }
              rewritten.append(inner.substr(scan, end - scan));
              scan = end;
              continue;
            }
            if (inner[scan + 1] == '*') {
              size_t end = inner.find("*/", scan + 2);
              if (end == std::string::npos) {
                error = "unterminated block comment";
                return false;
              }
              end += 2;
              rewritten.append(inner.substr(scan, end - scan));
              scan = end;
              continue;
            }
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
          if ((c == ',' || c == ';') && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0) {
            entryHasPairEquals = false;
            sawValueBoundary = false;
            pendingPairSeparator = false;
            rewritten.push_back(',');
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
            if (entryHasPairEquals && !sawValueBoundary) {
              rewritten.push_back(c);
              ++scan;
              continue;
            }
            rewritten.append(", ");
            entryHasPairEquals = true;
            sawValueBoundary = false;
            pendingPairSeparator = false;
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
      if (!applyPass(inner, filteredInner, innerError, options)) {
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


  auto trySkipIncludeDirective = [&](size_t &index) -> bool {
    constexpr size_t IncludeLen = 7;
    if (index + IncludeLen > input.size()) {
      return false;
    }
    if (input.compare(index, IncludeLen, "include") != 0) {
      return false;
    }
    if (index > 0 && (isTokenChar(input[index - 1]) || input[index - 1] == '/')) {
      return false;
    }
    size_t scan = index + IncludeLen;
    if (scan < input.size() && (isTokenChar(input[scan]) || input[scan] == '/')) {
      if (!(input[scan] == '/' && scan + 1 < input.size() &&
            (input[scan + 1] == '/' || input[scan + 1] == '*'))) {
        return false;
      }
    }
    auto skipWhitespaceAndComments = [&](size_t &pos) -> bool {
      bool advanced = true;
      while (advanced) {
        advanced = false;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
          ++pos;
          advanced = true;
        }
        if (pos + 1 < input.size() && input[pos] == '/') {
          if (input[pos + 1] == '/') {
            pos += 2;
            while (pos < input.size() && input[pos] != '\n') {
              ++pos;
            }
            advanced = true;
            continue;
          }
          if (input[pos + 1] == '*') {
            size_t end = input.find("*/", pos + 2);
            if (end == std::string::npos) {
              return false;
            }
            pos = end + 2;
            advanced = true;
            continue;
          }
        }
      }
      return true;
    };
    if (!skipWhitespaceAndComments(scan)) {
      return false;
    }
    if (scan >= input.size() || input[scan] != '<') {
      return false;
    }
    size_t pos = scan + 1;
    bool inString = false;
    char quote = '\0';
    while (pos < input.size()) {
      char c = input[pos];
      if (inString) {
        if (c == quote) {
          inString = false;
        }
        ++pos;
        continue;
      }
      if (c == '/' && pos + 1 < input.size()) {
        if (input[pos + 1] == '/') {
          pos += 2;
          while (pos < input.size() && input[pos] != '\n') {
            ++pos;
          }
          continue;
        }
        if (input[pos + 1] == '*') {
          size_t end = input.find("*/", pos + 2);
          if (end == std::string::npos) {
            return false;
          }
          pos = end + 2;
          continue;
        }
      }
      if (c == '"' || c == '\'') {
        inString = true;
        quote = c;
        ++pos;
        continue;
      }
      if (c == '>') {
        ++pos;
        output.append(input.substr(index, pos - index));
        index = pos - 1;
        return true;
      }
      ++pos;
    }
    return false;
  };

  for (size_t i = 0; i < input.size(); ++i) {
    if (trySkipIncludeDirective(i)) {
      continue;
    }
    if (input[i] == '/' && i + 1 < input.size() && input[i + 1] == '/') {
      size_t end = i + 2;
      while (end < input.size() && input[end] != '\n') {
        ++end;
      }
      output.append(input.substr(i, end - i));
      i = end > 0 ? end - 1 : i;
      continue;
    }

    if (input[i] == '/' && i + 1 < input.size() && input[i + 1] == '*') {
      size_t end = input.find("*/", i + 2);
      if (end == std::string::npos) {
        output.append(input.substr(i));
        break;
      }
      end += 2;
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

    if (input[i] == '/' && i + 1 < input.size()) {
      if (input[i + 1] == '/') {
        size_t end = i + 2;
        while (end < input.size() && input[end] != '\n') {
          ++end;
        }
        output.append(input.substr(i, end - i));
        i = end > 0 ? end - 1 : i;
        continue;
      }
      if (input[i + 1] == '*') {
        size_t end = i + 2;
        bool closed = false;
        while (end + 1 < input.size()) {
          if (input[end] == '*' && input[end + 1] == '/') {
            end += 2;
            closed = true;
            break;
          }
          ++end;
        }
        if (!closed) {
          error = "unterminated block comment";
          return false;
        }
        output.append(input.substr(i, end - i));
        i = end > 0 ? end - 1 : i;
        continue;
      }
    }

    if (rewriteCollectionLiteral(i)) {
      continue;
    }
    if (!error.empty()) {
      return false;
    }

    if (input[i] == '/' && (i == 0 || isSeparator(input[i - 1]))) {
      size_t leftIndex = i;
      while (leftIndex > 0 && std::isspace(static_cast<unsigned char>(input[leftIndex - 1]))) {
        --leftIndex;
      }
      if (leftIndex == 0 || !isLeftOperandEndChar(input[leftIndex - 1]) ||
          hasNonOperandKeywordBefore(leftIndex)) {
        size_t start = i;
        size_t end = i + 1;
        while (end < input.size() && !isSeparator(input[end])) {
          ++end;
        }
        output.append(input.substr(start, end - start));
        i = end - 1;
        continue;
      }
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
          bool sawDigit = false;
          while (i < input.size()) {
            if (isHexDigitChar(input[i])) {
              sawDigit = true;
              ++i;
              continue;
            }
            if (input[i] == ',' && sawDigit && i + 1 < input.size() && isHexDigitChar(input[i + 1])) {
              ++i;
              continue;
            }
            break;
          }
        } else {
          bool sawDigit = false;
          while (i < input.size()) {
            if (isDigitChar(input[i])) {
              sawDigit = true;
              ++i;
              continue;
            }
            if (input[i] == ',' && sawDigit && i + 1 < input.size() && isDigitChar(input[i + 1])) {
              ++i;
              continue;
            }
            break;
          }
        }
        size_t digitsEnd = i;
        bool hasDot = false;
        bool hasExponent = false;
        if (!isHex && i < input.size() && input[i] == '.') {
          hasDot = true;
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
        if (hasDot || hasExponent) {
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
      if (rewriteUnaryIncrement(input, output, i, options)) {
        continue;
      }
      if (rewriteUnaryDecrement(input, output, i, options)) {
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
    }

    output.push_back(input[i]);
  }
  if (enableOperators) {
    std::string stage1 = std::move(output);
    if (!rewriteBinaryOperatorsWithPrecedence(stage1, output, error)) {
      return false;
    }
  }
  return true;
}

} // namespace primec
