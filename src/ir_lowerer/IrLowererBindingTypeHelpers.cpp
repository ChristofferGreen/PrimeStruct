#include "IrLowererBindingTypeHelpers.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/SoaPathHelpers.h"

#include <cctype>
#include <utility>

namespace primec::ir_lowerer {

namespace {

bool resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
    const std::string &typeText,
    std::string &structPathOut) {
  structPathOut.clear();

  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  if (soa_paths::isExperimentalSoaVectorSpecializedTypePath(normalized)) {
    structPathOut = normalized;
    return true;
  }
  std::string base;
  std::string argList;
  if (!splitTemplateTypeName(normalized, base, argList)) {
    return false;
  }

  const std::string normalizedBase =
      normalizeDeclaredCollectionTypeBase(trimTemplateTypeText(base));
  if (normalizedBase != "soa_vector" || argList.empty()) {
    return false;
  }

  std::vector<std::string> templateArgs;
  if (!splitTemplateArgs(argList, templateArgs) || templateArgs.size() != 1) {
    return false;
  }

  std::string normalizedArg = trimTemplateTypeText(templateArgs.front());
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }

  structPathOut =
      specializedExperimentalSoaVectorStructPathForElementType(normalizedArg);
  return true;
}

std::string describeBindingSite(const std::string &scopePath,
                                const std::string &siteKind,
                                const Expr &expr) {
  const std::string displayName = expr.name.empty() ? "<unnamed>" : expr.name;
  return scopePath + " -> " + siteKind + " " + displayName;
}

bool requiresSemanticBindingFact(const SemanticProgram *semanticProgram,
                                 const Expr &expr) {
  return semanticProgram != nullptr && expr.semanticNodeId != 0;
}

bool isLocalAutoBindingCandidate(const Expr &expr) {
  std::string typeName;
  std::vector<std::string> templateArgs;
  if (!extractFirstBindingTypeTransform(expr, typeName, templateArgs)) {
    return true;
  }
  return trimTemplateTypeText(typeName) == "auto";
}

std::string compactTypeTextForComparison(const std::string &typeText) {
  std::string compact;
  for (unsigned char ch : trimTemplateTypeText(typeText)) {
    if (std::isspace(ch) == 0) {
      compact.push_back(static_cast<char>(ch));
    }
  }
  return compact;
}

bool semanticTypeTextsMatchForLocalAuto(const std::string &localAutoTypeText,
                                        const std::string &bindingTypeText) {
  const std::string localAutoType = compactTypeTextForComparison(localAutoTypeText);
  const std::string bindingType = compactTypeTextForComparison(bindingTypeText);
  if (localAutoType == bindingType) {
    return true;
  }
  const LocalInfo::ValueKind localAutoKind = valueKindFromTypeName(localAutoType);
  const LocalInfo::ValueKind bindingKind = valueKindFromTypeName(bindingType);
  return localAutoKind != LocalInfo::ValueKind::Unknown &&
         bindingKind != LocalInfo::ValueKind::Unknown &&
         localAutoKind == bindingKind;
}

struct ExpectedCollectionSpecialization {
  std::string family;
  std::vector<std::string> templateArgs;
};

std::string normalizeExpectedCollectionTemplateArg(std::string arg) {
  arg = normalizeCollectionBindingTypeName(trimTemplateTypeText(arg));
  const LocalInfo::ValueKind valueKind = valueKindFromTypeName(arg);
  const std::string normalizedValueType = typeNameForValueKind(valueKind);
  return normalizedValueType.empty() ? arg : normalizedValueType;
}

bool extractExpectedCollectionSpecialization(std::string typeText,
                                             ExpectedCollectionSpecialization &out) {
  typeText = unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(typeText));
  if (typeText.empty()) {
    return false;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(typeText, base, argText)) {
    return false;
  }

  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  argText = trimTemplateTypeText(argText);
  if (base == "Reference" || base == "Pointer") {
    return extractExpectedCollectionSpecialization(argText, out);
  }
  if (base != "vector" && base != "map" && base != "soa_vector") {
    return false;
  }

  std::vector<std::string> args;
  if (!argText.empty()) {
    if (!splitTemplateArgs(argText, args)) {
      args = {argText};
    }
    for (auto &arg : args) {
      arg = normalizeExpectedCollectionTemplateArg(arg);
    }
  }
  out = ExpectedCollectionSpecialization{base, std::move(args)};
  return true;
}

bool collectionSpecializationMatchesExpected(
    const SemanticProgramCollectionSpecialization &entry,
    const ExpectedCollectionSpecialization &expected) {
  if (entry.collectionFamily != expected.family) {
    return false;
  }
  if ((expected.family == "vector" || expected.family == "soa_vector") &&
      !expected.templateArgs.empty()) {
    return entry.elementTypeText == expected.templateArgs.front();
  }
  if (expected.family == "map" && expected.templateArgs.size() == 2) {
    return entry.keyTypeText == expected.templateArgs[0] &&
           entry.valueTypeText == expected.templateArgs[1];
  }
  return true;
}

LocalInfo::Kind bindingKindFromCollectionSpecialization(
    const SemanticProgramCollectionSpecialization &collectionFact) {
  if (collectionFact.isReference) {
    return LocalInfo::Kind::Reference;
  }
  if (collectionFact.isPointer) {
    return LocalInfo::Kind::Pointer;
  }
  if (collectionFact.collectionFamily == "vector" ||
      collectionFact.collectionFamily == "soa_vector") {
    return LocalInfo::Kind::Vector;
  }
  if (collectionFact.collectionFamily == "map") {
    return LocalInfo::Kind::Map;
  }
  return LocalInfo::Kind::Value;
}

LocalInfo::ValueKind bindingValueKindFromCollectionSpecialization(
    const SemanticProgramCollectionSpecialization &collectionFact) {
  if (collectionFact.collectionFamily == "vector" ||
      collectionFact.collectionFamily == "soa_vector") {
    return valueKindFromTypeName(collectionFact.elementTypeText);
  }
  if (collectionFact.collectionFamily == "map") {
    return valueKindFromTypeName(collectionFact.valueTypeText);
  }
  return LocalInfo::ValueKind::Unknown;
}

void setReferenceCollectionInfoFromSpecialization(
    const SemanticProgramCollectionSpecialization &collectionFact,
    LocalInfo &info) {
  if (info.kind != LocalInfo::Kind::Reference && info.kind != LocalInfo::Kind::Pointer) {
    return;
  }
  if ((info.kind == LocalInfo::Kind::Reference && !collectionFact.isReference) ||
      (info.kind == LocalInfo::Kind::Pointer && !collectionFact.isPointer)) {
    return;
  }
  if (collectionFact.collectionFamily == "vector" ||
      collectionFact.collectionFamily == "soa_vector") {
    if (info.kind == LocalInfo::Kind::Reference) {
      info.referenceToVector = true;
    } else {
      info.pointerToVector = true;
    }
    info.isSoaVector = collectionFact.collectionFamily == "soa_vector";
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = valueKindFromTypeName(collectionFact.elementTypeText);
    }
    if (info.structTypeName.empty() &&
        valueKindFromTypeName(collectionFact.elementTypeText) == LocalInfo::ValueKind::Unknown) {
      if (info.isSoaVector) {
        info.structTypeName = specializedExperimentalSoaVectorStructPathForElementType(
            collectionFact.elementTypeText);
      } else {
        info.structTypeName =
            specializedExperimentalVectorStructPathForElementType(collectionFact.elementTypeText);
      }
    }
    return;
  }
  if (collectionFact.collectionFamily == "map") {
    if (info.kind == LocalInfo::Kind::Reference) {
      info.referenceToMap = true;
    } else {
      info.pointerToMap = true;
    }
    info.mapKeyKind = valueKindFromTypeName(collectionFact.keyTypeText);
    info.mapValueKind = valueKindFromTypeName(collectionFact.valueTypeText);
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = info.mapValueKind;
    }
  }
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
  if (normalizedType == "vector" || normalizedType == "soa_vector") {
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
    if (base == "array" || base == "vector" || base == "soa_vector" || base == "Buffer") {
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
  if (kind != LocalInfo::Kind::Value || !normalizedType.empty()) {
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
  std::string normalizedTargetType = trimTemplateTypeText(targetType);
  if (!normalizedTargetType.empty() && normalizedTargetType.front() != '/') {
    normalizedTargetType.insert(normalizedTargetType.begin(), '/');
  }
  if (soa_paths::isExperimentalSoaVectorSpecializedTypePath(normalizedTargetType)) {
    if (info.kind == LocalInfo::Kind::Reference) {
      info.referenceToVector = true;
    } else {
      info.pointerToVector = true;
    }
    info.isSoaVector = true;
    if (info.structTypeName.empty()) {
      info.structTypeName = normalizedTargetType;
    }
    return;
  }
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
    if (info.structTypeName.empty() &&
        valueKindFromTypeName(normalizedArg) == LocalInfo::ValueKind::Unknown) {
      resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
          targetType, info.structTypeName);
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

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  auto validateBindingExpr = [&](const std::string &scopePath,
                                 const std::string &siteKind,
                                 const Expr &expr) {
    if (expr.semanticNodeId == 0) {
      return true;
    }
    const SemanticProgramBindingFact *bindingFact =
        findSemanticProductBindingFact(semanticIndex, expr);
    if (bindingFact == nullptr || bindingFact->bindingTypeText.empty()) {
      error = "missing semantic-product binding fact: " +
              describeBindingSite(scopePath, siteKind, expr);
      return false;
    }
    if (bindingFact->resolvedPathId == InvalidSymbolId ||
        semanticProgramBindingFactResolvedPath(*semanticProgram, *bindingFact).empty()) {
      error = "missing semantic-product binding resolved path id: " +
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

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);

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
          findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr);
      if (localAutoFact == nullptr || localAutoFact->bindingTypeText.empty()) {
        error = "missing semantic-product local-auto fact: " +
                describeBindingSite(scopePath, "local", expr);
        return false;
      }
      if (localAutoFact != nullptr &&
          localAutoFact->initializerResolvedPathId != InvalidSymbolId &&
          semanticProgramLocalAutoFactInitializerResolvedPath(*semanticProgram, *localAutoFact).empty()) {
        error = "missing semantic-product local-auto initializer path id: " +
                describeBindingSite(scopePath, "local", expr);
        return false;
      }
      auto validateInitializerCallPath = [&](SymbolId pathId,
                                             const char *missingLabel,
                                             const char *staleLabel) {
        if (pathId == InvalidSymbolId) {
          return true;
        }
        if (semanticProgramResolveCallTargetString(*semanticProgram, pathId).empty()) {
          error = std::string("missing semantic-product local-auto ") + missingLabel +
                  " path id: " + describeBindingSite(scopePath, "local", expr);
          return false;
        }
        if (localAutoFact->initializerResolvedPathId != InvalidSymbolId &&
            pathId != localAutoFact->initializerResolvedPathId) {
          error = std::string("stale semantic-product local-auto ") + staleLabel +
                  " fact: " + describeBindingSite(scopePath, "local", expr);
          return false;
        }
        return true;
      };
      if (!validateInitializerCallPath(localAutoFact->initializerDirectCallResolvedPathId,
                                       "direct-call",
                                       "direct-call")) {
        return false;
      }
      if (!validateInitializerCallPath(localAutoFact->initializerMethodCallResolvedPathId,
                                       "method-call",
                                       "method-call")) {
        return false;
      }
      const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(semanticIndex, expr);
      if (bindingFact != nullptr &&
          !bindingFact->bindingTypeText.empty() &&
          !semanticTypeTextsMatchForLocalAuto(localAutoFact->bindingTypeText,
                                              bindingFact->bindingTypeText)) {
        error = "stale semantic-product local-auto fact: " +
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

bool validateSemanticProductCollectionSpecializationCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  auto validateBindingExpr = [&](const std::string &scopePath,
                                 const std::string &siteKind,
                                 const Expr &expr) {
    if (expr.semanticNodeId == 0) {
      return true;
    }
    const SemanticProgramBindingFact *bindingFact =
        findSemanticProductBindingFact(semanticIndex, expr);
    if (bindingFact == nullptr || bindingFact->bindingTypeText.empty()) {
      return true;
    }

    ExpectedCollectionSpecialization expected;
    if (!extractExpectedCollectionSpecialization(
            bindingFact->bindingTypeText, expected)) {
      return true;
    }

    const SemanticProgramCollectionSpecialization *collectionFact =
        findSemanticProductCollectionSpecialization(semanticIndex, expr);
    if (collectionFact == nullptr) {
      error = "missing semantic-product collection specialization: " +
              describeBindingSite(scopePath, siteKind, expr);
      return false;
    }
    if (!collectionSpecializationMatchesExpected(*collectionFact, expected)) {
      error = "stale semantic-product collection specialization: " +
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
      name == "/SoaVector" ||
      name == "std/collections/experimental_soa_vector/SoaVector" ||
      name == "/std/collections/experimental_soa_vector/SoaVector" ||
      soa_paths::isExperimentalSoaVectorTypePath(name)) {
    return "soa_vector";
  }
  if (name == "Buffer" || name == "std/gfx/Buffer" || name == "/std/gfx/Buffer" ||
      name == "std/gfx/experimental/Buffer" || name == "/std/gfx/experimental/Buffer") {
    return "Buffer";
  }
  if (name == "/File" || name == "std/file/File" || name == "/std/file/File") {
    return "File";
  }
  if (name == "args") {
    return "array";
  }
  return name;
}

bool typeTextUsesRawBuiltinSoaVectorLayout(const std::string &typeText) {
  const std::string normalized = trimTemplateTypeText(typeText);
  std::string base;
  std::string arg;
  if (splitTemplateTypeName(normalized, base, arg)) {
    const std::string trimmedBase = trimTemplateTypeText(base);
    if (trimmedBase == "soa_vector" || trimmedBase == "/soa_vector" ||
        trimmedBase == "std/collections/soa_vector" ||
        trimmedBase == "/std/collections/soa_vector") {
      return true;
    }
    std::vector<std::string> templateArgs;
    if (splitTemplateArgs(arg, templateArgs)) {
      for (const std::string &nested : templateArgs) {
        if (typeTextUsesRawBuiltinSoaVectorLayout(nested)) {
          return true;
        }
      }
    } else if (!arg.empty() && typeTextUsesRawBuiltinSoaVectorLayout(arg)) {
      return true;
    }
    return false;
  }
  return normalized == "soa_vector" || normalized == "/soa_vector" ||
         normalized == "std/collections/soa_vector" ||
         normalized == "/std/collections/soa_vector";
}

bool exprUsesRawBuiltinSoaVectorLayout(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities" ||
        isBindingQualifierName(transform.name)) {
      continue;
    }
    if (typeTextUsesRawBuiltinSoaVectorLayout(transform.name)) {
      return true;
    }
    for (const std::string &templateArg : transform.templateArgs) {
      if (typeTextUsesRawBuiltinSoaVectorLayout(templateArg)) {
        return true;
      }
    }
  }
  return false;
}

BindingTypeAdapters makeBindingTypeAdapters(const SemanticProgram *semanticProgram) {
  BindingTypeAdapters adapters;
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  adapters.isBindingMutable = [](const Expr &expr) {
    return ir_lowerer::isBindingMutable(expr);
  };
  adapters.bindingKind = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramCollectionSpecialization *collectionFact =
            findSemanticProductCollectionSpecialization(semanticIndex, expr);
        collectionFact != nullptr) {
      return bindingKindFromCollectionSpecialization(*collectionFact);
    }
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticIndex, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return bindingKindFromTypeText(bindingFact->bindingTypeText);
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return LocalInfo::Kind::Value;
    }
    return bindingKindFromTransforms(expr);
  };
  adapters.hasExplicitBindingTypeTransform = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramLocalAutoFact *localAutoFact =
            findSemanticProductLocalAutoFactBySemanticId(semanticIndex, expr);
        localAutoFact != nullptr) {
      return false;
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      const SemanticProgramBindingFact *bindingFact =
          findSemanticProductBindingFact(semanticIndex, expr);
      if (bindingFact == nullptr || bindingFact->bindingTypeText.empty()) {
        return false;
      }
      return true;
    }
    return primec::ir_lowerer::hasExplicitBindingTypeTransform(expr);
  };
  adapters.isStringBinding = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticIndex, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return isStringBindingTypeText(bindingFact->bindingTypeText);
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return false;
    }
    return isStringBindingType(expr);
  };
  adapters.isFileErrorBinding = [semanticProgram, semanticIndex](const Expr &expr) {
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticIndex, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return isFileErrorBindingTypeText(bindingFact->bindingTypeText);
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return false;
    }
    return isFileErrorBindingType(expr);
  };
  adapters.bindingValueKind = [semanticProgram, semanticIndex](const Expr &expr, LocalInfo::Kind kind) {
    if (const SemanticProgramCollectionSpecialization *collectionFact =
            findSemanticProductCollectionSpecialization(semanticIndex, expr);
        collectionFact != nullptr) {
      return bindingValueKindFromCollectionSpecialization(*collectionFact);
    }
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticIndex, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      return bindingValueKindFromTypeText(bindingFact->bindingTypeText, kind);
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
      return LocalInfo::ValueKind::Unknown;
    }
    return bindingValueKindFromTransforms(expr, kind);
  };
  adapters.setReferenceArrayInfo = [semanticProgram, semanticIndex](const Expr &expr, LocalInfo &info) {
    if (const SemanticProgramCollectionSpecialization *collectionFact =
            findSemanticProductCollectionSpecialization(semanticIndex, expr);
        collectionFact != nullptr) {
      setReferenceCollectionInfoFromSpecialization(*collectionFact, info);
      return;
    }
    if (const SemanticProgramBindingFact *bindingFact =
            findSemanticProductBindingFact(semanticIndex, expr);
        bindingFact != nullptr && !bindingFact->bindingTypeText.empty()) {
      setReferenceArrayInfoFromTypeText(bindingFact->bindingTypeText, info);
      return;
    }
    if (requiresSemanticBindingFact(semanticProgram, expr)) {
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
    if (normalizedName == "vector" || normalizedName == "soa_vector") {
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
  bool sawTypeTransform = false;
  for (const auto &transform : expr.transforms) {
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    sawTypeTransform = true;
    const std::string normalizedName = normalizeCollectionBindingTypeName(transform.name);
    if (normalizedName == "Pointer" || normalizedName == "Reference") {
      if (transform.templateArgs.size() == 1) {
        return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(transform.templateArgs.front()));
      }
      return LocalInfo::ValueKind::Unknown;
    }
    if (normalizedName == "array" || normalizedName == "vector" ||
        normalizedName == "soa_vector" || normalizedName == "Buffer") {
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
  if (kind != LocalInfo::Kind::Value || sawTypeTransform) {
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
        info.structTypeName =
            specializedExperimentalVectorStructPathForElementType(elementType);
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
        info.structTypeName =
            specializedExperimentalSoaVectorStructPathForElementType(elementType);
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
