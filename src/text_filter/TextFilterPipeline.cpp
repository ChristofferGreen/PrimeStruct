#include "primec/TextFilterPipeline.h"

#include "TextFilterHelpers.h"
#include "primec/TransformRegistry.h"
#include "primec/TransformRules.h"

#include <cctype>
#include <unordered_set>

namespace primec {
using namespace text_filter;

namespace {
size_t findNextTransformListStart(const std::string &input, size_t start);
size_t findMatchingCloseWithComments(const std::string &input,
                                     size_t openIndex,
                                     char openChar,
                                     char closeChar);
bool appendOperatorsTransform(const std::string &input, std::string &output, std::string &error);

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
          if (c == ',' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0) {
            entryHasPairEquals = false;
            sawValueBoundary = false;
            pendingPairSeparator = false;
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
    if (op == '/' && !left.empty() && left[0] == '/') {
      return false;
    }
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

  auto trySkipIncludeDirective = [&](size_t &index) -> bool {
    constexpr size_t IncludeLen = 7;
    if (index + IncludeLen > input.size()) {
      return false;
    }
    if (input.compare(index, IncludeLen, "include") != 0) {
      return false;
    }
    if (index > 0 && isTokenChar(input[index - 1])) {
      return false;
    }
    size_t scan = index + IncludeLen;
    if (scan < input.size() && isTokenChar(input[scan])) {
      return false;
    }
    while (scan < input.size() && std::isspace(static_cast<unsigned char>(input[scan]))) {
      ++scan;
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
        if (c == '\\' && pos + 1 < input.size()) {
          pos += 2;
          continue;
        }
        if (c == quote) {
          inString = false;
        }
        ++pos;
        continue;
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

enum class ListPhase { Auto, Text, Semantic };

bool isIdentifierStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '/';
}

bool isIdentifierBody(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/';
}

bool isTransformListBoundary(const std::string &input, size_t index) {
  if (index == 0) {
    return true;
  }
  char prev = input[index - 1];
  if (std::isspace(static_cast<unsigned char>(prev))) {
    return true;
  }
  switch (prev) {
  case '(':
  case '{':
  case ';':
  case ',':
    return true;
  default:
    return false;
  }
}

void skipWhitespaceAndComments(const std::string &input, size_t &pos) {
  while (pos < input.size()) {
    char c = input[pos];
    if (std::isspace(static_cast<unsigned char>(c))) {
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
          pos = input.size();
          return;
        }
        pos = end + 2;
        continue;
      }
    }
    return;
  }
}

size_t findMatchingCloseWithComments(const std::string &input,
                                     size_t openIndex,
                                     char openChar,
                                     char closeChar) {
  int depth = 1;
  size_t pos = openIndex + 1;
  while (pos < input.size()) {
    char c = input[pos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, pos);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end;
      continue;
    }
    if (c == 'R' && pos + 2 < input.size() && input[pos + 1] == '"' && input[pos + 2] == '(') {
      size_t end = input.find(")\"", pos + 3);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end + 2;
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
          return std::string::npos;
        }
        pos = end + 2;
        continue;
      }
    }
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
    ++pos;
  }
  return std::string::npos;
}

bool parseIdentifier(const std::string &input, size_t &pos, std::string &out) {
  if (pos >= input.size() || !isIdentifierStart(input[pos])) {
    return false;
  }
  size_t start = pos;
  ++pos;
  while (pos < input.size() && isIdentifierBody(input[pos])) {
    ++pos;
  }
  out = input.substr(start, pos - start);
  return true;
}

void appendTextTransform(const std::string &name, ListPhase phase, std::vector<std::string> &out) {
  if (phase == ListPhase::Semantic) {
    return;
  }
  if (phase == ListPhase::Text) {
    if (primec::isTextTransformName(name)) {
      out.push_back(name);
    }
    return;
  }
  if (primec::isTextTransformName(name) && !primec::isSemanticTransformName(name)) {
    out.push_back(name);
  }
}

void scanTransformEntries(const std::string &input,
                          size_t &pos,
                          size_t end,
                          ListPhase phase,
                          std::vector<std::string> &textTransforms) {
  while (pos < end) {
    skipWhitespaceAndComments(input, pos);
    while (pos < end && (input[pos] == ',' || input[pos] == ';')) {
      ++pos;
      skipWhitespaceAndComments(input, pos);
    }
    if (pos >= end) {
      return;
    }
    if (!isIdentifierStart(input[pos])) {
      ++pos;
      continue;
    }
    std::string name;
    if (!parseIdentifier(input, pos, name)) {
      ++pos;
      continue;
    }
    size_t scan = pos;
    skipWhitespaceAndComments(input, scan);
    if ((name == "text" || name == "semantic") && scan < end && input[scan] == '(') {
      size_t groupClose = findMatchingCloseWithComments(input, scan, '(', ')');
      if (groupClose != std::string::npos && groupClose <= end) {
        size_t groupPos = scan + 1;
        scanTransformEntries(input, groupPos, groupClose, name == "text" ? ListPhase::Text : ListPhase::Semantic,
                             textTransforms);
        pos = groupClose + 1;
        continue;
      }
    }
    appendTextTransform(name, phase, textTransforms);
    scan = pos;
    skipWhitespaceAndComments(input, scan);
    if (scan < end && input[scan] == '<') {
      size_t close = findMatchingCloseWithComments(input, scan, '<', '>');
      if (close != std::string::npos && close <= end) {
        scan = close + 1;
      }
    }
    pos = scan;
    skipWhitespaceAndComments(input, pos);
    if (pos < end && input[pos] == '(') {
      size_t close = findMatchingCloseWithComments(input, pos, '(', ')');
      if (close != std::string::npos && close <= end) {
        pos = close + 1;
      }
    }
  }
}

struct TransformListScan {
  size_t end = std::string::npos;
  std::vector<std::string> textTransforms;
};

bool scanTransformList(const std::string &input, size_t start, TransformListScan &out) {
  size_t close = findMatchingCloseWithComments(input, start, '[', ']');
  if (close == std::string::npos) {
    return false;
  }
  out.end = close;
  out.textTransforms.clear();
  size_t pos = start + 1;
  scanTransformEntries(input, pos, close, ListPhase::Auto, out.textTransforms);
  return true;
}

bool scanEnvelopeAfterList(const std::string &input, size_t start, size_t &envelopeEnd, std::string &nameOut) {
  size_t pos = start;
  skipWhitespaceAndComments(input, pos);
  std::string name;
  if (!parseIdentifier(input, pos, name)) {
    return false;
  }
  nameOut = name;
  skipWhitespaceAndComments(input, pos);
  if (pos < input.size() && input[pos] == '<') {
    size_t close = findMatchingCloseWithComments(input, pos, '<', '>');
    if (close == std::string::npos) {
      return false;
    }
    pos = close + 1;
    skipWhitespaceAndComments(input, pos);
  }
  if (pos >= input.size()) {
    return false;
  }
  if (input[pos] == '{') {
    size_t closeBrace = findMatchingCloseWithComments(input, pos, '{', '}');
    if (closeBrace == std::string::npos) {
      return false;
    }
    envelopeEnd = closeBrace;
    return true;
  }
  if (input[pos] != '(') {
    return false;
  }
  size_t closeParen = findMatchingCloseWithComments(input, pos, '(', ')');
  if (closeParen == std::string::npos) {
    return false;
  }
  size_t afterParams = closeParen + 1;
  skipWhitespaceAndComments(input, afterParams);
  if (afterParams < input.size() && input[afterParams] == '{') {
    size_t closeBrace = findMatchingCloseWithComments(input, afterParams, '{', '}');
    if (closeBrace == std::string::npos) {
      return false;
    }
    envelopeEnd = closeBrace;
    return true;
  }
  envelopeEnd = closeParen;
  return true;
}

size_t findNextTransformListStart(const std::string &input, size_t start) {
  size_t pos = start;
  while (pos < input.size()) {
    char c = input[pos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, pos);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end;
      continue;
    }
    if (c == 'R' && pos + 2 < input.size() && input[pos + 1] == '"' && input[pos + 2] == '(') {
      size_t end = input.find(")\"", pos + 3);
      if (end == std::string::npos) {
        return std::string::npos;
      }
      pos = end + 2;
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
          return std::string::npos;
        }
        pos = end + 2;
        continue;
      }
    }
    if (c == '[' && isTransformListBoundary(input, pos)) {
      return pos;
    }
    ++pos;
  }
  return std::string::npos;
}

bool appendOperatorsTransform(const std::string &input, std::string &output, std::string &error) {
  output = input;
  size_t listStart = findNextTransformListStart(output, 0);
  if (listStart == std::string::npos) {
    return true;
  }
  size_t listEnd = findMatchingCloseWithComments(output, listStart, '[', ']');
  if (listEnd == std::string::npos) {
    error = "unterminated transform list";
    return false;
  }
  auto isIdentifierChar = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '/';
  };
  const std::string needle = "operators";
  for (size_t pos = listStart + 1; pos + needle.size() <= listEnd; ++pos) {
    if (output.compare(pos, needle.size(), needle) != 0) {
      continue;
    }
    char prev = (pos > listStart + 1) ? output[pos - 1] : '\0';
    char next = (pos + needle.size() < listEnd) ? output[pos + needle.size()] : '\0';
    if (!isIdentifierChar(prev) && !isIdentifierChar(next)) {
      return true;
    }
  }
  bool hasContent = false;
  for (size_t pos = listStart + 1; pos < listEnd; ++pos) {
    if (!std::isspace(static_cast<unsigned char>(output[pos]))) {
      hasContent = true;
      break;
    }
  }
  std::string insertText = "operators";
  if (hasContent) {
    insertText = " " + insertText;
  }
  output.insert(listEnd, insertText);
  return true;
}

struct NamespaceBlock {
  std::string name;
  size_t end = std::string::npos;
};

bool isNamespaceKeywordAt(const std::string &input, size_t pos) {
  constexpr const char *kNamespace = "namespace";
  constexpr size_t kNamespaceLen = 9;
  if (pos + kNamespaceLen > input.size()) {
    return false;
  }
  if (input.compare(pos, kNamespaceLen, kNamespace) != 0) {
    return false;
  }
  if (pos > 0 && isIdentifierBody(input[pos - 1])) {
    return false;
  }
  size_t after = pos + kNamespaceLen;
  if (after < input.size() && isIdentifierBody(input[after])) {
    return false;
  }
  return true;
}

bool parseNamespaceBlock(const std::string &input, size_t start, size_t &afterPos, NamespaceBlock &block) {
  if (!isNamespaceKeywordAt(input, start)) {
    return false;
  }
  size_t pos = start + 9;
  skipWhitespaceAndComments(input, pos);
  std::string name;
  if (!parseIdentifier(input, pos, name)) {
    return false;
  }
  if (name.find('/') != std::string::npos) {
    return false;
  }
  skipWhitespaceAndComments(input, pos);
  if (pos >= input.size() || input[pos] != '{') {
    return false;
  }
  size_t close = findMatchingCloseWithComments(input, pos, '{', '}');
  if (close == std::string::npos) {
    return false;
  }
  block.name = name;
  block.end = close;
  afterPos = pos + 1;
  return true;
}

void advanceNamespaceScan(const std::string &input,
                          size_t &scanPos,
                          size_t targetPos,
                          std::vector<NamespaceBlock> &stack) {
  while (scanPos < targetPos) {
    char c = input[scanPos];
    if (c == '"' || c == '\'') {
      size_t end = skipQuotedForward(input, scanPos);
      if (end == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = end;
      continue;
    }
    if (c == 'R' && scanPos + 2 < input.size() && input[scanPos + 1] == '"' && input[scanPos + 2] == '(') {
      size_t end = input.find(")\"", scanPos + 3);
      if (end == std::string::npos) {
        scanPos = targetPos;
        break;
      }
      scanPos = end + 2;
      continue;
    }
    if (c == '/' && scanPos + 1 < input.size()) {
      if (input[scanPos + 1] == '/') {
        scanPos += 2;
        while (scanPos < targetPos && input[scanPos] != '\n') {
          ++scanPos;
        }
        continue;
      }
      if (input[scanPos + 1] == '*') {
        size_t end = input.find("*/", scanPos + 2);
        if (end == std::string::npos) {
          scanPos = targetPos;
          break;
        }
        scanPos = end + 2;
        continue;
      }
    }
    NamespaceBlock block;
    size_t afterPos = scanPos;
    if (parseNamespaceBlock(input, scanPos, afterPos, block)) {
      stack.push_back(std::move(block));
      scanPos = afterPos;
      continue;
    }
    ++scanPos;
  }
  while (!stack.empty() && stack.back().end < targetPos) {
    stack.pop_back();
  }
}

std::string buildNamespacePrefix(const std::string &basePrefix, const std::vector<NamespaceBlock> &stack) {
  std::string prefix = basePrefix;
  for (const auto &entry : stack) {
    if (prefix.empty()) {
      prefix = "/" + entry.name;
    } else {
      prefix.append("/");
      prefix.append(entry.name);
    }
  }
  return prefix;
}

std::string makeFullPath(const std::string &name, const std::string &prefix) {
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (prefix.empty()) {
    return "/" + name;
  }
  return prefix + "/" + name;
}

bool containsFilterName(const std::vector<std::string> &filters, const std::string &name) {
  for (const auto &filter : filters) {
    if (filter == name) {
      return true;
    }
  }
  return false;
}

bool filtersEqual(const std::vector<std::string> &left, const std::vector<std::string> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

bool scanLeadingTransformList(const std::string &input, std::vector<std::string> &out) {
  size_t listStart = findNextTransformListStart(input, 0);
  if (listStart == std::string::npos) {
    return false;
  }
  TransformListScan listScan;
  if (!scanTransformList(input, listStart, listScan)) {
    return false;
  }
  out = listScan.textTransforms;
  return true;
}

bool applyFilterToChunk(const std::string &input,
                        std::string &output,
                        std::string &error,
                        const std::string *filter) {
  if (filter == nullptr || filter->empty()) {
    output = input;
    return true;
  }
  TextFilterOptions passOptions;
  passOptions.enabledFilters = {*filter};
  return applyPass(input, output, error, passOptions);
}

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix);

bool applyPerEnvelopeFilterPass(const std::string &input,
                                std::string &output,
                                std::string &error,
                                const std::string &filter,
                                const std::vector<std::string> &activeFilters,
                                const std::vector<TextTransformRule> &rules,
                                bool suppressLeadingOverride,
                                const std::string &baseNamespacePrefix,
                                bool allowExplicitRecursion) {
  output.clear();
  size_t pos = 0;
  size_t scanPos = 0;
  bool suppress = suppressLeadingOverride;
  std::vector<NamespaceBlock> namespaceStack;
  const bool filterActive = containsFilterName(activeFilters, filter);
  while (scanPos < input.size()) {
    size_t listStart = findNextTransformListStart(input, scanPos);
    if (listStart == std::string::npos) {
      break;
    }
    advanceNamespaceScan(input, scanPos, listStart, namespaceStack);
    std::string namespacePrefix = buildNamespacePrefix(baseNamespacePrefix, namespaceStack);
    TransformListScan listScan;
    if (!scanTransformList(input, listStart, listScan)) {
      break;
    }
    if (suppress && listStart == 0) {
      scanPos = listScan.end + 1;
      suppress = false;
      continue;
    }
    size_t envelopeEnd = std::string::npos;
    std::string envelopeName;
    if (!scanEnvelopeAfterList(input, listScan.end + 1, envelopeEnd, envelopeName)) {
      scanPos = listScan.end + 1;
      suppress = false;
      continue;
    }
    std::string chunk = input.substr(pos, listStart - pos);
    std::string filteredChunk;
    if (!applyFilterToChunk(chunk, filteredChunk, error, filterActive ? &filter : nullptr)) {
      return false;
    }
    output.append(filteredChunk);
    std::string envelope = input.substr(listStart, envelopeEnd - listStart + 1);
    const std::vector<std::string> *autoFilters = &activeFilters;
    if (!rules.empty()) {
      std::string fullPath = makeFullPath(envelopeName, namespacePrefix);
      if (const auto *ruleFilters = selectRuleTransforms(rules, fullPath)) {
        autoFilters = ruleFilters;
      }
    }
    const std::vector<std::string> &envelopeFilters =
        listScan.textTransforms.empty() ? *autoFilters : listScan.textTransforms;
    const bool inheritsFilters = filtersEqual(envelopeFilters, activeFilters);
    std::string filteredEnvelope;
    if (inheritsFilters) {
      if (!applyPerEnvelopeFilterPass(envelope,
                                      filteredEnvelope,
                                      error,
                                      filter,
                                      envelopeFilters,
                                      rules,
                                      true,
                                      namespacePrefix,
                                      allowExplicitRecursion)) {
        return false;
      }
      output.append(filteredEnvelope);
    } else if (allowExplicitRecursion) {
      if (!applyPerEnvelope(envelope, filteredEnvelope, error, envelopeFilters, rules, true, namespacePrefix)) {
        return false;
      }
      output.append(filteredEnvelope);
    } else {
      output.append(envelope);
    }
    pos = envelopeEnd + 1;
    scanPos = pos;
    suppress = false;
  }
  std::string tail = input.substr(pos);
  std::string filteredTail;
  if (!applyFilterToChunk(tail, filteredTail, error, filterActive ? &filter : nullptr)) {
    return false;
  }
  output.append(filteredTail);
  return true;
}

bool applyPerEnvelope(const std::string &input,
                      std::string &output,
                      std::string &error,
                      const std::vector<std::string> &filters,
                      const std::vector<TextTransformRule> &rules,
                      bool suppressLeadingOverride,
                      const std::string &baseNamespacePrefix) {
  std::vector<std::string> activeFilters = filters;
  if (activeFilters.empty()) {
    return applyPerEnvelopeFilterPass(input,
                                      output,
                                      error,
                                      std::string(),
                                      activeFilters,
                                      rules,
                                      suppressLeadingOverride,
                                      baseNamespacePrefix,
                                      true);
  }
  std::unordered_set<std::string> applied;
  std::string current = input;
  bool allowExplicitRecursion = true;
  for (size_t i = 0; i < activeFilters.size(); ++i) {
    const std::string &filter = activeFilters[i];
    if (!applied.insert(filter).second) {
      continue;
    }
    std::string next;
    if (!applyPerEnvelopeFilterPass(current,
                                    next,
                                    error,
                                    filter,
                                    activeFilters,
                                    rules,
                                    suppressLeadingOverride,
                                    baseNamespacePrefix,
                                    allowExplicitRecursion)) {
      return false;
    }
    current.swap(next);
    allowExplicitRecursion = false;
    std::vector<std::string> listTransforms;
    if (scanLeadingTransformList(current, listTransforms)) {
      for (const auto &name : listTransforms) {
        if (applied.find(name) != applied.end()) {
          continue;
        }
        if (!containsFilterName(activeFilters, name)) {
          activeFilters.push_back(name);
        }
      }
    }
  }
  output = current;
  return true;
}

} // namespace

bool TextFilterPipeline::apply(const std::string &input,
                               std::string &output,
                               std::string &error,
                               const TextFilterOptions &options) const {
  output = input;
  error.clear();
  if (options.enabledFilters.empty() && options.rules.empty()) {
    if (!options.allowEnvelopeTransforms) {
      return true;
    }
  }
  return applyPerEnvelope(input, output, error, options.enabledFilters, options.rules, false, "");
}

} // namespace primec
