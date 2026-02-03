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

bool isDigitChar(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
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
    if (isTokenChar(c)) {
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

std::string maybeAppendI32(const std::string &token) {
  if (token.empty()) {
    return token;
  }
  for (char c : token) {
    if (!isDigitChar(c)) {
      return token;
    }
  }
  return token + "i32";
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
    if (!isTokenChar(input[index - 1]) || index + 2 >= input.size() || !isTokenChar(input[index + 2])) {
      return false;
    }
    size_t end = output.size();
    size_t start = end;
    while (start > 0 && isTokenChar(output[start - 1])) {
      --start;
    }
    if (start == end) {
      return false;
    }
    std::string left = output.substr(start, end - start);
    if (options.implicitI32Suffix) {
      left = maybeAppendI32(left);
    }
    size_t rightStart = index + 2;
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
    if (!isTokenChar(input[index - 1]) || !isTokenChar(input[index + 1])) {
      return false;
    }
    size_t end = output.size();
    size_t start = end;
    while (start > 0 && isTokenChar(output[start - 1])) {
      --start;
    }
    if (start == end) {
      return false;
    }
    std::string left = output.substr(start, end - start);
    if (options.implicitI32Suffix) {
      left = maybeAppendI32(left);
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
        while (i < input.size() && isDigitChar(input[i])) {
          ++i;
        }
        size_t digitsEnd = i;
        if (digitsStart == digitsEnd) {
          output.push_back(input[start]);
          i = start;
          continue;
        }
        if (digitsEnd + 2 < input.size() && input.compare(digitsEnd, 3, "i32") == 0) {
          output.append(input.substr(start, digitsEnd - start + 3));
          i = digitsEnd + 2;
          continue;
        }
        if (digitsEnd < input.size()) {
          char next = input[digitsEnd];
          if (std::isalpha(static_cast<unsigned char>(next)) || next == '_' || next == '.') {
            output.append(input.substr(start, digitsEnd - start));
            i = digitsEnd - 1;
            continue;
          }
        }
        output.append(input.substr(start, digitsEnd - start));
        output.append("i32");
        i = digitsEnd - 1;
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
    if (rewriteBinary(i, '>', "greater_than")) {
      continue;
    }

    output.push_back(input[i]);
  }
  return true;
}

} // namespace primec
