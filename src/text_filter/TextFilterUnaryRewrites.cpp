#include "TextFilterHelpers.h"

namespace primec::text_filter {
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
  std::string right = input.substr(rightStart, rightEnd - rightStart);
  right = stripOuterParens(right);
  right = normalizeUnaryOperand(right);
  output.append("location(");
  output.append(right);
  output.append(")");
  index = rightEnd - 1;
  return true;
}

bool rewriteUnaryDeref(const std::string &input,
                       std::string &output,
                       size_t &index,
                       const TextFilterOptions &options) {
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
