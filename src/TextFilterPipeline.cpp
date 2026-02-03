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
} // namespace

bool TextFilterPipeline::apply(const std::string &input, std::string &output, std::string &error) const {
  output.clear();
  output.reserve(input.size());
  error.clear();

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
    size_t rightStart = index + 1;
    size_t rightEnd = rightStart;
    while (rightEnd < input.size() && isTokenChar(input[rightEnd])) {
      ++rightEnd;
    }
    if (rightEnd == rightStart) {
      return false;
    }
    std::string right = input.substr(rightStart, rightEnd - rightStart);
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

    if (input[i] == '/' && rewriteBinary(i, '/', "divide")) {
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

    output.push_back(input[i]);
  }
  return true;
}

} // namespace primec
