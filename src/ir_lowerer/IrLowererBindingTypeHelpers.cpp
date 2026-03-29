#include "IrLowererBindingTypeHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

std::string normalizeCollectionBindingTypeName(const std::string &name) {
  if (name == "/vector" || name == "std/collections/vector" || name == "/std/collections/vector" ||
      name == "Vector" || name == "std/collections/experimental_vector/Vector" ||
      name == "/std/collections/experimental_vector/Vector" ||
      name.rfind("std/collections/experimental_vector/Vector__", 0) == 0 ||
      name.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
    return "vector";
  }
  if (name == "/map" || name == "std/collections/map" || name == "/std/collections/map" ||
      name == "Map" || name == "std/collections/experimental_map/Map" ||
      name == "/std/collections/experimental_map/Map" ||
      name.rfind("std/collections/experimental_map/Map__", 0) == 0 ||
      name.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
    return "map";
  }
  if (name == "Buffer" || name == "std/gfx/Buffer" || name == "/std/gfx/Buffer" ||
      name == "std/gfx/experimental/Buffer" || name == "/std/gfx/experimental/Buffer") {
    return "Buffer";
  }
  if (name == "args") {
    return "array";
  }
  return name;
}

BindingTypeAdapters makeBindingTypeAdapters() {
  BindingTypeAdapters adapters;
  adapters.bindingKind = makeBindingKindFromTransforms();
  adapters.isStringBinding = makeIsStringBindingType();
  adapters.isFileErrorBinding = makeIsFileErrorBindingType();
  adapters.bindingValueKind = makeBindingValueKindFromTransforms();
  adapters.setReferenceArrayInfo = makeSetReferenceArrayInfoFromTransforms();
  return adapters;
}

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
    const std::string normalizedName = normalizeCollectionBindingTypeName(transform.name);
    if (normalizedName == "Reference") {
      return LocalInfo::Kind::Reference;
    }
    if (normalizedName == "Pointer") {
      return LocalInfo::Kind::Pointer;
    }
    if (normalizedName == "array") {
      return LocalInfo::Kind::Array;
    }
    if (normalizedName == "vector") {
      return LocalInfo::Kind::Vector;
    }
    if (normalizedName == "map") {
      return LocalInfo::Kind::Map;
    }
    if (normalizedName == "Buffer") {
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
    if ((transform.name == "Reference" || transform.name == "Pointer") && transform.templateArgs.size() == 1 &&
        unwrapTopLevelUninitializedTypeText(transform.templateArgs.front()) == "FileError") {
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
    const std::string normalizedName = normalizeCollectionBindingTypeName(transform.name);
    if (normalizedName == "Pointer" || normalizedName == "Reference") {
      if (transform.templateArgs.size() == 1) {
        return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(transform.templateArgs.front()));
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "array" || normalizedName == "vector" || normalizedName == "Buffer") {
      if (transform.templateArgs.size() == 1) {
        return valueKindFromTypeName(transform.templateArgs.front());
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "map") {
      if (transform.templateArgs.size() == 2) {
        return valueKindFromTypeName(transform.templateArgs[1]);
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "Result") {
      if (transform.templateArgs.size() == 1) {
        return LocalInfo::ValueKind::Int32;
      }
      if (transform.templateArgs.size() == 2) {
        return LocalInfo::ValueKind::Int64;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    LocalInfo::ValueKind kindValue = valueKindFromTypeName(normalizedName);
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
  if (info.kind != LocalInfo::Kind::Reference && info.kind != LocalInfo::Kind::Pointer) {
    return;
  }
  for (const auto &transform : expr.transforms) {
    if ((info.kind == LocalInfo::Kind::Reference && transform.name != "Reference") ||
        (info.kind == LocalInfo::Kind::Pointer && transform.name != "Pointer") ||
        transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string targetType = unwrapTopLevelUninitializedTypeText(transform.templateArgs.front());
    std::string base;
    std::string arg;
    if (!splitTemplateTypeName(targetType, base, arg)) {
      return;
    }
    base = normalizeCollectionBindingTypeName(base);
    if (base == "array") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToArray = true;
      } else {
        info.pointerToArray = true;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
    if (base == "vector") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToVector = true;
      } else {
        info.pointerToVector = true;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
    if (base == "soa_vector") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToVector = true;
      } else {
        info.pointerToVector = true;
      }
      info.isSoaVector = true;
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
    if (base == "Buffer") {
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToBuffer = true;
      } else {
        info.pointerToBuffer = true;
      }
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(trimTemplateTypeText(arg));
      }
      return;
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (!splitTemplateArgs(arg, args) || args.size() != 2) {
        return;
      }
      if (info.kind == LocalInfo::Kind::Reference) {
        info.referenceToMap = true;
      } else {
        info.pointerToMap = true;
      }
      info.mapKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
      info.mapValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = info.mapValueKind;
      }
      return;
    }
    if (base == "File") {
      info.isFileHandle = true;
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = LocalInfo::ValueKind::Int64;
      }
    }
    return;
  }
}

} // namespace primec::ir_lowerer
