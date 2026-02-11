#include "primec/TextFilterPipeline.h"

#include "TextFilterHelpers.h"

#include <cctype>

namespace primec {
using namespace text_filter;

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
