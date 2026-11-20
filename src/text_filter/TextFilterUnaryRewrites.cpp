#include "TextFilterUnaryRewrites.h"

#include "TextFilterHelpers.h"

namespace primec::text_filter {
namespace {

// Extends the end of a unary operand over the postfix forms that bind tighter than any prefix or binary
// operator: call arguments, index expressions, `.member` names and the `?` try suffix (`-f(x)`, `!v[i]`,
// `-f()?.field`).
size_t extendPostfixOperandEnd(const std::string &text, size_t end) {
  while (end < text.size()) {
    const char c = text[end];
    if (c == '(' || c == '[') {
      const size_t close = findMatchingClose(text, end, c, c == '(' ? ')' : ']');
      if (close == std::string::npos) {
        break;
      }
      end = close + 1;
      continue;
    }
    if (c == '?') {
      ++end;
      continue;
    }
    if (c == '.' && end + 1 < text.size() && isTokenChar(text[end + 1]) &&
        !isDigitChar(text[end + 1])) {
      end += 2;
      while (end < text.size() && isTokenChar(text[end])) {
        ++end;
      }
      continue;
    }
    break;
  }
  return end;
}

// A unary operand is copied into the output whole, so prefix operators nested inside it (`!(f(!x))`,
// `-f(-x)`) are rewritten here, by running the same operator pass over the operand text.
Text rewriteNestedOperators(const Text &operand, const TextFilterOptions &options) {
  if (!options.hasFilter("operators") || operand.find_first_of("!&*-+") == std::string::npos) {
    return operand;
  }
  Text rewritten;
  std::string error;
  if (!primec::applyPass(operand, rewritten, error, options)) {
    return operand;
  }
  return rewritten;
}

bool rewriteUnaryMutation(const Text &input,
                          Text &output,
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
    Text right = input.substr(rightStart, rightEnd - rightStart);
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
  Text left = output.substr(start, end - start);
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

bool rewriteUnaryNot(const Text &input,
                     Text &output,
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
  size_t rightEnd = extendPostfixOperandEnd(input, findRightTokenEnd(input, rightStart));
  if (rightEnd == rightStart) {
    return false;
  }
  Text right = input.substr(rightStart, rightEnd - rightStart);
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  right = rewriteNestedOperators(right, options);
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

bool rewriteUnaryAddressOf(const Text &input,
                           Text &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  if (input[index] != '&' || index + 1 >= input.size() || input[index + 1] == '&') {
    return false;
  }
  if (index > 0 && input[index - 1] == '&') {
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
  rightEnd = extendPostfixOperandEnd(input, rightEnd);
  Text right = input.substr(rightStart, rightEnd - rightStart);
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  right = rewriteNestedOperators(right, options);
  output.append("location(");
  output.append(right);
  output.append(")");
  index = rightEnd - 1;
  return true;
}

bool rewriteUnaryDeref(const Text &input,
                       Text &output,
                       size_t &index,
                       const TextFilterOptions &options) {
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
  rightEnd = extendPostfixOperandEnd(input, rightEnd);
  Text right = input.substr(rightStart, rightEnd - rightStart);
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  right = rewriteNestedOperators(right, options);
  output.append("dereference(");
  output.append(right);
  output.append(")");
  index = rightEnd - 1;
  return true;
}

bool rewriteUnaryIncrement(const Text &input,
                           Text &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteUnaryMutation(input, output, index, options, '+', "increment");
}

bool rewriteUnaryDecrement(const Text &input,
                           Text &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteUnaryMutation(input, output, index, options, '-', "decrement");
}

bool rewriteUnaryMinus(const Text &input,
                       Text &output,
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
  rightEnd = extendPostfixOperandEnd(input, rightEnd);
  Text right = input.substr(rightStart, rightEnd - rightStart);
  right = rewriteNestedOperators(right, options);
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

// The `std::string` forms of the rewrites (the testing API): the same rewrite over text that tracks nothing.
namespace {

template <typename Rewrite>
bool rewriteStringText(const std::string &input,
                       std::string &output,
                       size_t &index,
                       Rewrite rewrite) {
  Text textOutput(output);
  const bool rewrote = rewrite(Text(input), textOutput, index);
  output = std::move(static_cast<std::string &>(textOutput));
  return rewrote;
}

} // namespace

bool rewriteUnaryNot(const std::string &input,
                     std::string &output,
                     size_t &index,
                     const TextFilterOptions &options) {
  return rewriteStringText(input, output, index, [&](const Text &in, Text &out, size_t &at) {
    return rewriteUnaryNot(in, out, at, options);
  });
}

bool rewriteUnaryAddressOf(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteStringText(input, output, index, [&](const Text &in, Text &out, size_t &at) {
    return rewriteUnaryAddressOf(in, out, at, options);
  });
}

bool rewriteUnaryDeref(const std::string &input,
                       std::string &output,
                       size_t &index,
                       const TextFilterOptions &options) {
  return rewriteStringText(input, output, index, [&](const Text &in, Text &out, size_t &at) {
    return rewriteUnaryDeref(in, out, at, options);
  });
}

bool rewriteUnaryIncrement(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteStringText(input, output, index, [&](const Text &in, Text &out, size_t &at) {
    return rewriteUnaryIncrement(in, out, at, options);
  });
}

bool rewriteUnaryDecrement(const std::string &input,
                           std::string &output,
                           size_t &index,
                           const TextFilterOptions &options) {
  return rewriteStringText(input, output, index, [&](const Text &in, Text &out, size_t &at) {
    return rewriteUnaryDecrement(in, out, at, options);
  });
}

bool rewriteUnaryMinus(const std::string &input,
                       std::string &output,
                       size_t &index,
                       const TextFilterOptions &options) {
  return rewriteStringText(input, output, index, [&](const Text &in, Text &out, size_t &at) {
    return rewriteUnaryMinus(in, out, at, options);
  });
}

} // namespace primec::text_filter
