#include "IrLowererTemplateTypeParseHelpers.h"

#include <cctype>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

std::string trimTemplateTypeText(const std::string &text) {
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
    start++;
  }
  size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    end--;
  }
  return text.substr(start, end - start);
}

bool splitTemplateArgs(const std::string &text, std::vector<std::string> &out) {
  out.clear();
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (c == '<') {
      depth++;
      continue;
    }
    if (c == '>') {
      if (depth == 0) {
        return false;
      }
      depth--;
      continue;
    }
    if (c == ',' && depth == 0) {
      out.push_back(trimTemplateTypeText(text.substr(start, i - start)));
      start = i + 1;
    }
  }
  if (depth != 0) {
    return false;
  }
  out.push_back(trimTemplateTypeText(text.substr(start)));
  return true;
}

bool isResultTemplateTypeBaseName(const std::string &base) {
  const std::string trimmed = trimTemplateTypeText(base);
  return trimmed == "Result" || trimmed == "/std/result/Result" ||
         trimmed == "std/result/Result";
}

bool parseResultTypeName(const std::string &typeName,
                         bool &hasValue,
                         LocalInfo::ValueKind &valueKind,
                         std::string &errorType) {
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(typeName, base, arg) || !isResultTemplateTypeBaseName(base)) {
    return false;
  }
  std::vector<std::string> args;
  if (!splitTemplateArgs(arg, args)) {
    return false;
  }
  if (args.size() == 1) {
    hasValue = false;
    valueKind = LocalInfo::ValueKind::Unknown;
    errorType = args.front();
    return true;
  }
  if (args.size() == 2) {
    hasValue = true;
    valueKind = valueKindFromTypeName(args.front());
    errorType = args.back();
    return true;
  }
  return false;
}

} // namespace primec::ir_lowerer
