#include "IrLowererBindingTransformHelpers.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

bool isBindingMutable(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "mut") {
      return true;
    }
  }
  return false;
}

bool isBindingQualifierName(const std::string &name) {
  return name == "public" || name == "private" || name == "static" || name == "mut" || name == "copy" ||
         name == "restrict" || name == "align_bytes" || name == "align_kbytes";
}

bool hasExplicitBindingTypeTransform(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return true;
  }
  return false;
}

bool extractFirstBindingTypeTransform(const Expr &expr,
                                      std::string &typeNameOut,
                                      std::vector<std::string> &templateArgsOut) {
  typeNameOut.clear();
  templateArgsOut.clear();
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    typeNameOut = transform.name;
    templateArgsOut = transform.templateArgs;
    return true;
  }
  return false;
}

bool extractUninitializedTemplateArg(const Expr &expr, std::string &typeTextOut) {
  typeTextOut.clear();
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    if (transform.name != "uninitialized") {
      continue;
    }
    if (transform.templateArgs.size() != 1) {
      return false;
    }
    typeTextOut = transform.templateArgs.front();
    return true;
  }
  return false;
}

bool extractTopLevelUninitializedTypeText(const std::string &typeText, std::string &typeTextOut) {
  const std::string trimmedType = trimTemplateTypeText(typeText);
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimmedType, base, argText)) {
    return false;
  }
  if (normalizeCollectionBindingTypeName(base) != "uninitialized") {
    return false;
  }
  typeTextOut = trimTemplateTypeText(argText);
  return !typeTextOut.empty();
}

std::string unwrapTopLevelUninitializedTypeText(const std::string &typeText) {
  std::string innerType;
  if (extractTopLevelUninitializedTypeText(typeText, innerType)) {
    return innerType;
  }
  return trimTemplateTypeText(typeText);
}

bool isArgsPackBinding(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return transform.name == "args";
  }
  return false;
}

bool isEntryArgsParam(const Expr &param) {
  std::string typeName;
  std::string templateArg;
  bool hasTemplateArg = false;
  for (const auto &transform : param.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    typeName = transform.name;
    if (transform.templateArgs.size() == 1) {
      templateArg = transform.templateArgs.front();
      hasTemplateArg = true;
    } else {
      hasTemplateArg = false;
    }
  }
  return typeName == "array" && hasTemplateArg && templateArg == "string";
}

} // namespace primec::ir_lowerer
