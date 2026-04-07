#include "IrLowererBindingTypeHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string describeBindingSite(const std::string &scopePath,
                                const std::string &siteKind,
                                const Expr &expr) {
  const std::string displayName = expr.name.empty() ? "<unnamed>" : expr.name;
  return scopePath + " -> " + siteKind + " " + displayName;
}

bool requiresSemanticBindingFact(const SemanticProductTargetAdapter &semanticProductTargets,
                                 const Expr &expr) {
  return semanticProductTargets.hasSemanticProduct && expr.semanticNodeId != 0;
}

bool isLocalAutoBindingCandidate(const Expr &expr) {
  std::string typeName;
  std::vector<std::string> templateArgs;
  if (!extractFirstBindingTypeTransform(expr, typeName, templateArgs)) {
    return true;
  }
  return trimTemplateTypeText(typeName) == "auto";
}

LocalInfo::Kind bindingKindFromTypeText(const std::string &typeText) {
  std::string base;
  std::string arg;
  std::string normalizedType = trimTemplateTypeText(typeText);
  if (splitTemplateTypeName(normalizedType, base, arg)) {
    normalizedType = trimTemplateTypeText(base);
  }
  normalizedType = normalizeCollectionBindingTypeName(normalizedType);
  if (normalizedType == "Reference") {
    return LocalInfo::Kind::Reference;
  }
  if (normalizedType == "Pointer") {
    return LocalInfo::Kind::Pointer;
  }
  if (normalizedType == "array") {
    return LocalInfo::Kind::Array;
  }
  if (normalizedType == "vector") {
    return LocalInfo::Kind::Vector;
  }
  if (normalizedType == "map") {
    return LocalInfo::Kind::Map;
  }
  if (normalizedType == "Buffer") {
    return LocalInfo::Kind::Buffer;
  }
  return LocalInfo::Kind::Value;
}

bool isStringBindingTypeText(const std::string &typeText) {
  std::string base;
  std::string arg;
  const std::string normalizedType = trimTemplateTypeText(typeText);
  if (isStringTypeName(normalizedType)) {
    return true;
  }
  if (splitTemplateTypeName(normalizedType, base, arg)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    return (base == "Pointer" || base == "Reference") && isStringTypeName(trimTemplateTypeText(arg));
  }
  return false;
}

bool isFileErrorBindingTypeText(const std::string &typeText) {
  std::string base;
  std::string arg;
  const std::string normalizedType = trimTemplateTypeText(typeText);
  if (normalizedType == "FileError") {
    return true;
  }
  if (splitTemplateTypeName(normalizedType, base, arg)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    return (base == "Reference" || base == "Pointer") &&
           unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(arg)) == "FileError";
  }
  return false;
}

LocalInfo::ValueKind bindingValueKindFromTypeText(const std::string &typeText, LocalInfo::Kind kind) {
  std::string base;
  std::string arg;
  const std::string normalizedType = trimTemplateTypeText(typeText);
  if (splitTemplateTypeName(normalizedType, base, arg)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if (base == "Pointer" || base == "Reference") {
      return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(arg)));
    }
    if (base == "array" || base == "vector" || base == "Buffer") {
      return valueKindFromTypeName(trimTemplateTypeText(arg));
    }
    if (base == "map") {
      std::vector<std::string> args;
      if (splitTemplateArgs(arg, args) && args.size() == 2) {
        return valueKindFromTypeName(trimTemplateTypeText(args[1]));
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (base == "Result") {
      std::vector<std::string> args;
      if (splitTemplateArgs(arg, args)) {
        return args.size() == 2 ? LocalInfo::ValueKind::Int64 : LocalInfo::ValueKind::Int32;
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (base == "File") {
      return LocalInfo::ValueKind::Int64;
    }
    LocalInfo::ValueKind kindValue = valueKindFromTypeName(base);
    if (kindValue != LocalInfo::ValueKind::Unknown) {
      return kindValue;
    }
  } else {
    LocalInfo::ValueKind kindValue = valueKindFromTypeName(normalizeCollectionBindingTypeName(normalizedType));
    if (kindValue != LocalInfo::ValueKind::Unknown) {
      return kindValue;
    }
  }
  if (kind != LocalInfo::Kind::Value) {
    return LocalInfo::ValueKind::Unknown;
  }
  return LocalInfo::ValueKind::Int32;
}

void setReferenceArrayInfoFromTypeText(const std::string &typeText, LocalInfo &info) {
  if (info.kind != LocalInfo::Kind::Reference && info.kind != LocalInfo::Kind::Pointer) {
    return;
  }
  std::string base;
  std::string arg;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, arg)) {
    return;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (base != "Reference" && base != "Pointer") {
    return;
  }
  const std::string targetType = unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(arg));
  if (!splitTemplateTypeName(targetType, base, arg)) {
    return;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  const std::string normalizedArg = trimTemplateTypeText(arg);
  if (base == "array") {
    if (info.kind == LocalInfo::Kind::Reference) {
      info.referenceToArray = true;
    } else {
      info.pointerToArray = true;
    }
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = valueKindFromTypeName(normalizedArg);
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
      info.valueKind = valueKindFromTypeName(normalizedArg);
    }
    if (info.structTypeName.empty() && valueKindFromTypeName(normalizedArg) == LocalInfo::ValueKind::Unknown) {
      info.structTypeName = normalizedArg;
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
      info.valueKind = valueKindFromTypeName(normalizedArg);
    }
    if (info.structTypeName.empty() && valueKindFromTypeName(normalizedArg) == LocalInfo::ValueKind::Unknown) {
      info.structTypeName = normalizedArg;
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
      info.valueKind = valueKindFromTypeName(normalizedArg);
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
}

} // namespace

bool validateSemanticProductBindingCoverage(const Program &program,
                                           const SemanticProgram *semanticProgram,
                                           std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);
  auto validateBindingExpr = [&](const std::string &scopePath,
                                 const std::string &siteKind,
                                 const Expr &expr) {
    if (expr.semanticNodeId == 0) {
      return true;
    }
    const SemanticProgramBindingFact *bindingFact =
        findSemanticProductBindingFact(semanticProductTargets, expr);
    if (bindingFact == nullptr || bindingFact->bindingTypeText.empty()) {
      error = "missing semantic-product binding fact: " +
              describeBindingSite(scopePath, siteKind, expr);
      return false;
    }
    return true;
  };

  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding && !validateBindingExpr(scopePath, "local", expr)) {
      return false;
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      if (!validateBindingExpr(def.fullPath, "parameter", param)) {
        return false;
      }
    }
    if (!validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductLocalAutoCoverage(const Program &program,
                                              const SemanticProgram *semanticProgram,
                                              std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);

  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.isBinding && expr.args.size() == 1 && expr.semanticNodeId != 0 &&
        isLocalAutoBindingCandidate(expr)) {
      const SemanticProgramLocalAutoFact *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProductTargets, expr);
      const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(semanticProductTargets, expr);
      const bool hasPublishedBindingType =
          (localAutoFact != nullptr && !localAutoFact->bindingTypeText.empty()) ||
          (bindingFact != nullptr && !bindingFact->bindingTypeText.empty());
      if (!hasPublishedBindingType) {
        error = "missing semantic-product local-auto fact: " +
                describeBindingSite(scopePath, "local", expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  return true;
}

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
  if (name == "/soa_vector" || name == "std/collections/soa_vector" ||
      name == "/std/collections/soa_vector" || name == "SoaVector" ||
      name == "std/collections/experimental_soa_vector/SoaVector" ||
      name == "/std/collections/experimental_soa_vector/SoaVector" ||
      name.rfind("std/collections/experimental_soa_vector/SoaVector__", 0) == 0 ||
      name.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0) {
    return "soa_vector";
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

BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram) {
  BindingTypeAdapters adapters;
  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);
  adapters.bindingKind = [semanticProductTargets](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticProductTargets, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return bindingKindFromTypeText(bindingFact->bindingTypeText);
    }
    if (requiresSemanticBindingFact(semanticProductTargets, expr)) {
      return LocalInfo::Kind::Value;
    }
    return bindingKindFromTransforms(expr);
  };
  adapters.hasExplicitBindingTypeTransform = [semanticProductTargets](const Expr &expr) {
    if (const SemanticProgramLocalAutoFact *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProductTargets, expr);
        localAutoFact != nullptr) {
      return false;
    }
    if (requiresSemanticBindingFact(semanticProductTargets, expr)) {
      const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(semanticProductTargets, expr);
      if (bindingFact == nullptr || bindingFact->bindingTypeText.empty()) {
        return false;
      }
      return true;
    }
    return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
  };
  adapters.isStringBinding = [semanticProductTargets](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticProductTargets, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return isStringBindingTypeText(bindingFact->bindingTypeText);
    }
    if (requiresSemanticBindingFact(semanticProductTargets, expr)) {
      return false;
    }
    return isStringBindingType(expr);
  };
  adapters.isFileErrorBinding = [semanticProductTargets](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticProductTargets, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return isFileErrorBindingTypeText(bindingFact->bindingTypeText);
    }
    if (requiresSemanticBindingFact(semanticProductTargets, expr)) {
      return false;
    }
    return isFileErrorBindingType(expr);
  };
  adapters.bindingValueKind = [semanticProductTargets](const Expr &expr, LocalInfo::Kind kind) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticProductTargets, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return bindingValueKindFromTypeText(bindingFact->bindingTypeText, kind);
    }
    if (requiresSemanticBindingFact(semanticProductTargets, expr)) {
      return LocalInfo::ValueKind::Unknown;
    }
    return bindingValueKindFromTransforms(expr, kind);
  };
  adapters.setReferenceArrayInfo = [semanticProductTargets](const Expr &expr, LocalInfo &info) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticProductTargets, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      setReferenceArrayInfoFromTypeText(bindingFact->bindingTypeText, info);
      return;
    }
    if (requiresSemanticBindingFact(semanticProductTargets, expr)) {
      return;
    }
    setReferenceArrayInfoFromTransforms(expr, info);
  };
  return adapters;
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
      const std::string elementType = trimTemplateTypeText(arg);
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(elementType);
      }
      if (info.structTypeName.empty() && valueKindFromTypeName(elementType) == LocalInfo::ValueKind::Unknown) {
        info.structTypeName = elementType;
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
      const std::string elementType = trimTemplateTypeText(arg);
      if (info.valueKind == LocalInfo::ValueKind::Unknown) {
        info.valueKind = valueKindFromTypeName(elementType);
      }
      if (info.structTypeName.empty() && valueKindFromTypeName(elementType) == LocalInfo::ValueKind::Unknown) {
        info.structTypeName = elementType;
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
