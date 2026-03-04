#include "IrLowererBindingTypeHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

BindingKindFromTransformsFn makeBindingKindFromTransforms() {
  return [](const Expr &expr) {
    return bindingKindFromTransforms(expr);
  };
}

IsBindingTypeFn makeIsStringBindingType() {
  return [](const Expr &expr) {
    return isStringBindingType(expr);
  };
}

IsBindingTypeFn makeIsFileErrorBindingType() {
  return [](const Expr &expr) {
    return isFileErrorBindingType(expr);
  };
}

BindingValueKindFromTransformsFn makeBindingValueKindFromTransforms() {
  return [](const Expr &expr, LocalInfo::Kind kind) {
    return bindingValueKindFromTransforms(expr, kind);
  };
}

SetReferenceArrayInfoFn makeSetReferenceArrayInfoFromTransforms() {
  return [](const Expr &expr, LocalInfo &info) {
    setReferenceArrayInfoFromTransforms(expr, info);
  };
}

LocalInfo::Kind bindingKindFromTransforms(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "Reference") {
      return LocalInfo::Kind::Reference;
    }
    if (transform.name == "Pointer") {
      return LocalInfo::Kind::Pointer;
    }
    if (transform.name == "array") {
      return LocalInfo::Kind::Array;
    }
    if (transform.name == "vector") {
      return LocalInfo::Kind::Vector;
    }
    if (transform.name == "map") {
      return LocalInfo::Kind::Map;
    }
    if (transform.name == "Buffer") {
      return LocalInfo::Kind::Buffer;
    }
  }
  return LocalInfo::Kind::Value;
}

bool isStringTypeName(const std::string &name) {
  return name == "string";
}

bool isStringBindingType(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (isStringTypeName(transform.name)) {
      return true;
    }
    if ((transform.name == "Pointer" || transform.name == "Reference") && transform.templateArgs.size() == 1 &&
        isStringTypeName(transform.templateArgs.front())) {
      return true;
    }
  }
  return false;
}

bool isFileErrorBindingType(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (transform.name == "FileError") {
      return true;
    }
  }
  return false;
}

LocalInfo::ValueKind bindingValueKindFromTransforms(const Expr &expr, LocalInfo::Kind kind) {
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (transform.name == "Pointer" || transform.name == "Reference") {
      if (transform.templateArgs.size() == 1) {
        return valueKindFromTypeName(transform.templateArgs.front());
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (transform.name == "array" || transform.name == "vector" || transform.name == "Buffer") {
      if (transform.templateArgs.size() == 1) {
        return valueKindFromTypeName(transform.templateArgs.front());
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (transform.name == "map") {
      if (transform.templateArgs.size() == 2) {
        return valueKindFromTypeName(transform.templateArgs[1]);
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (transform.name == "Result") {
      if (transform.templateArgs.size() == 1) {
        return LocalInfo::ValueKind::Int32;
      }
      if (transform.templateArgs.size() == 2) {
        return LocalInfo::ValueKind::Int64;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (transform.name == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    LocalInfo::ValueKind kindValue = valueKindFromTypeName(transform.name);
    if (kindValue != LocalInfo::ValueKind::Unknown) {
      return kindValue;
    }
  }
  if (kind != LocalInfo::Kind::Value) {
    return LocalInfo::ValueKind::Unknown;
  }
  return LocalInfo::ValueKind::Int32;
}

void setReferenceArrayInfoFromTransforms(const Expr &expr, LocalInfo &info) {
  if (info.kind != LocalInfo::Kind::Reference) {
    return;
  }
  for (const auto &transform : expr.transforms) {
    if (transform.name != "Reference" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg) || base != "array") {
      return;
    }
    info.referenceToArray = true;
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
    }
    return;
  }
}

} // namespace primec::ir_lowerer
