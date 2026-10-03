#pragma once

// Helpers shared by the IrLowererBindingTypeHelpers*.cpp units (split out of
// IrLowererBindingTypeHelpers.cpp without changes).
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ir/SoaPathHelpers.h"
#include <cctype>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace binding_type_helpers_file_local {

inline bool resolveSpecializedExperimentalSoaVectorStructPathFromTypeText(
    const std::string &typeText,
    std::string &structPathOut) {
  structPathOut.clear();

  std::string normalized = trimTemplateTypeText(typeText);
  if (!normalized.empty() && normalized.front() != '/') {
    normalized.insert(normalized.begin(), '/');
  }
  if (soa_paths::isExperimentalColumnarVectorSpecializedTypePath(normalized)) {
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
  if (normalizedBase != "soa" || argList.empty()) {
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

inline std::string describeBindingSite(const std::string &scopePath,
                                const std::string &siteKind,
                                const Expr &expr) {
  const std::string displayName = expr.name.empty() ? "<unnamed>" : expr.name;
  return scopePath + " -> " + siteKind + " " + displayName;
}

inline bool requiresSemanticBindingFact(const SemanticProgram *semanticProgram,
                                 const Expr &expr) {
  return semanticProgram != nullptr && expr.semanticNodeId != 0;
}



inline std::string resolveSemanticBindingFactTypeText(
    const SemanticProgram *semanticProgram,
    const SemanticProgramBindingFact &bindingFact) {
  return resolveSemanticProductTypeText(
      semanticProgram, bindingFact.bindingTypeText, bindingFact.bindingTypeTextId);
}

inline std::string resolveSemanticCollectionSpecializationText(
    const SemanticProgram *semanticProgram,
    const std::string &text,
    SymbolId textId) {
  return resolveSemanticProductTypeText(semanticProgram, text, textId);
}

inline bool isLocalAutoBindingCandidate(const Expr &expr) {
  std::string typeName;
  std::vector<std::string> templateArgs;
  if (!extractFirstBindingTypeTransform(expr, typeName, templateArgs)) {
    return true;
  }
  return trimTemplateTypeText(typeName) == "auto";
}

inline std::string compactTypeTextForComparison(const std::string &typeText) {
  std::string compact;
  for (unsigned char ch : trimTemplateTypeText(typeText)) {
    if (std::isspace(ch) == 0) {
      compact.push_back(static_cast<char>(ch));
    }
  }
  return compact;
}

inline bool semanticTypeTextsMatchForLocalAuto(const std::string &localAutoTypeText,
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

inline std::string_view stripResolvedPathSpecializationSuffix(std::string_view path) {
  const std::size_t lastSlash = path.rfind('/');
  if (lastSlash == std::string_view::npos) {
    return path;
  }
  std::size_t marker = path.size();
  while (marker > lastSlash) {
    const std::size_t specializationMarker = path.rfind("__t", marker - 1);
    const std::size_t overloadMarker = path.rfind("__ov", marker - 1);
    const bool specializationInLeaf =
        specializationMarker != std::string_view::npos && specializationMarker > lastSlash;
    const bool overloadInLeaf =
        overloadMarker != std::string_view::npos && overloadMarker > lastSlash;
    if (!specializationInLeaf && !overloadInLeaf) {
      break;
    }
    if (specializationInLeaf && (!overloadInLeaf || specializationMarker > overloadMarker)) {
      marker = specializationMarker;
      continue;
    }
    marker = overloadMarker;
  }
  return marker == path.size() ? path : path.substr(0, marker);
}

struct ExpectedCollectionSpecialization {
  std::string family;
  std::vector<std::string> templateArgs;
};

struct ExpectedArrayExtent {
  std::string elementTypeText;
  bool isReference = false;
};

inline std::string normalizeExpectedCollectionTemplateArg(std::string arg) {
  arg = trimTemplateTypeText(arg);
  std::string base;
  std::string templateArgText;
  if (splitTemplateTypeName(arg, base, templateArgText)) {
    return arg;
  }
  arg = normalizeCollectionBindingTypeName(arg);
  const LocalInfo::ValueKind valueKind = valueKindFromTypeName(arg);
  const std::string normalizedValueType = typeNameForValueKind(valueKind);
  return normalizedValueType.empty() ? arg : normalizedValueType;
}

inline bool extractExpectedArrayExtent(std::string typeText,
                                ExpectedArrayExtent &out,
                                bool isReference = false) {
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
  if (base == "Reference") {
    return extractExpectedArrayExtent(argText, out, true);
  }
  if (base != "array") {
    return false;
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return false;
  }
  out.elementTypeText = normalizeExpectedCollectionTemplateArg(args.front());
  out.isReference = isReference;
  return true;
}

inline bool extractExpectedCollectionSpecialization(std::string typeText,
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
  if (base != "vector" && base != "map" && base != "soa") {
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

inline bool collectionSpecializationMatchesExpected(
    const SemanticProgramCollectionSpecialization &entry,
    const ExpectedCollectionSpecialization &expected) {
  if (entry.collectionFamily != expected.family) {
    return false;
  }
  if ((expected.family == "vector" || expected.family == "soa") &&
      !expected.templateArgs.empty()) {
    return entry.elementTypeText == expected.templateArgs.front();
  }
  if (expected.family == "map" && expected.templateArgs.size() == 2) {
    return entry.keyTypeText == expected.templateArgs[0] &&
           entry.valueTypeText == expected.templateArgs[1];
  }
  return true;
}

inline std::string resolveSemanticTextOrFallback(const SemanticProgram &semanticProgram,
                                          SymbolId textId,
                                          const std::string &fallback) {
  if (textId != InvalidSymbolId) {
    const std::string_view resolved =
        semanticProgramResolveCallTargetString(semanticProgram, textId);
    if (!resolved.empty()) {
      return std::string(resolved);
    }
  }
  return fallback;
}

inline bool arrayExtentBindingSiteRequiresFact(std::string_view siteKind,
                                        bool isReference) {
  if (siteKind == "local") {
    return !isReference;
  }
  return siteKind == "parameter" && isReference;
}

inline std::string expectedArrayExtentSiteKind(std::string_view bindingSiteKind,
                                        bool isReference) {
  if (bindingSiteKind == "local") {
    return isReference ? "local-reference" : "local-value";
  }
  if (bindingSiteKind == "parameter") {
    return isReference ? "parameter-reference" : "parameter-value";
  }
  return std::string(bindingSiteKind);
}

inline std::string describeArrayExtentSite(const std::string &scopePath,
                                    std::string_view siteKind,
                                    std::string_view targetName) {
  const std::string displayName =
      targetName.empty() ? std::string("<unnamed>") : std::string(targetName);
  return scopePath + " -> " + std::string(siteKind) + " " + displayName;
}

inline std::string arrayExtentBindingResolvedPath(std::string_view scopePath,
                                           std::string_view bindingName) {
  if (scopePath.empty() || bindingName.empty()) {
    return {};
  }
  if (bindingName.front() == '/') {
    return std::string(bindingName);
  }
  std::string normalizedScope(scopePath);
  if (!normalizedScope.empty() && normalizedScope.front() != '/') {
    normalizedScope.insert(normalizedScope.begin(), '/');
  }
  if (!normalizedScope.empty() && normalizedScope.back() != '/') {
    normalizedScope.push_back('/');
  }
  normalizedScope.append(bindingName);
  return normalizedScope;
}

inline std::optional<int64_t> arrayExtentSignedLiteralValue(const Expr &expr) {
  if (expr.kind != Expr::Kind::Literal || expr.isUnsigned) {
    return std::nullopt;
  }
  if (expr.intWidth == 64) {
    return static_cast<int64_t>(expr.literalValue);
  }
  return static_cast<int32_t>(expr.literalValue);
}

inline std::string arrayExtentOperandText(const Expr &expr) {
  if (expr.kind == Expr::Kind::Name) {
    return expr.name;
  }
  if (const auto literal = arrayExtentSignedLiteralValue(expr)) {
    return std::to_string(*literal);
  }
  return "?";
}

inline std::string sliceExtentExpressionForBindingInitializer(const Expr &bindingExpr) {
  if (bindingExpr.args.size() != 1) {
    return {};
  }
  const Expr &initializer = bindingExpr.args.front();
  if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
      !isSimpleCallName(initializer, "slice") || initializer.args.size() != 3) {
    return {};
  }
  return arrayExtentOperandText(initializer.args[2]) + " - " +
         arrayExtentOperandText(initializer.args[1]);
}

inline void collectArrayExtentExpressionsForBindings(
    const std::string &scopePath,
    const std::vector<Expr> &exprs,
    std::unordered_map<std::string, std::string> &extentExpressionsByResolvedPath) {
  for (const Expr &expr : exprs) {
    if (expr.isBinding) {
      if (std::string extentExpression =
              sliceExtentExpressionForBindingInitializer(expr);
          !extentExpression.empty()) {
        extentExpressionsByResolvedPath.insert_or_assign(
            arrayExtentBindingResolvedPath(scopePath, expr.name),
            std::move(extentExpression));
      }
    }
    collectArrayExtentExpressionsForBindings(
        scopePath, expr.args, extentExpressionsByResolvedPath);
    collectArrayExtentExpressionsForBindings(
        scopePath, expr.bodyArguments, extentExpressionsByResolvedPath);
  }
}

inline LocalInfo::Kind bindingKindFromCollectionSpecialization(
    const SemanticProgram *semanticProgram,
    const SemanticProgramCollectionSpecialization &collectionFact) {
  const std::string collectionFamily = resolveSemanticCollectionSpecializationText(
      semanticProgram, collectionFact.collectionFamily, collectionFact.collectionFamilyId);
  if (collectionFact.isReference) {
    return LocalInfo::Kind::Reference;
  }
  if (collectionFact.isPointer) {
    return LocalInfo::Kind::Pointer;
  }
  if (collectionFamily == "vector" || collectionFamily == "soa") {
    return LocalInfo::Kind::Vector;
  }
  if (collectionFamily == "map") {
    return LocalInfo::Kind::Value;
  }
  return LocalInfo::Kind::Value;
}

inline LocalInfo::ValueKind bindingValueKindFromCollectionSpecialization(
    const SemanticProgram *semanticProgram,
    const SemanticProgramCollectionSpecialization &collectionFact) {
  const std::string collectionFamily = resolveSemanticCollectionSpecializationText(
      semanticProgram, collectionFact.collectionFamily, collectionFact.collectionFamilyId);
  if (collectionFamily == "vector" || collectionFamily == "soa") {
    return valueKindFromTypeName(resolveSemanticCollectionSpecializationText(
        semanticProgram, collectionFact.elementTypeText, collectionFact.elementTypeTextId));
  }
  if (collectionFamily == "map") {
    return valueKindFromTypeName(resolveSemanticCollectionSpecializationText(
        semanticProgram, collectionFact.valueTypeText, collectionFact.valueTypeTextId));
  }
  return LocalInfo::ValueKind::Unknown;
}

inline void setReferenceCollectionInfoFromSpecialization(
    const SemanticProgram *semanticProgram,
    const SemanticProgramCollectionSpecialization &collectionFact,
    LocalInfo &info) {
  if (info.kind != LocalInfo::Kind::Reference && info.kind != LocalInfo::Kind::Pointer) {
    return;
  }
  if ((info.kind == LocalInfo::Kind::Reference && !collectionFact.isReference) ||
      (info.kind == LocalInfo::Kind::Pointer && !collectionFact.isPointer)) {
    return;
  }
  const std::string collectionFamily = resolveSemanticCollectionSpecializationText(
      semanticProgram, collectionFact.collectionFamily, collectionFact.collectionFamilyId);
  const std::string elementTypeText = resolveSemanticCollectionSpecializationText(
      semanticProgram, collectionFact.elementTypeText, collectionFact.elementTypeTextId);
  if (collectionFamily == "vector" || collectionFamily == "soa") {
    if (info.kind == LocalInfo::Kind::Reference) {
      info.referenceToVector = true;
    } else {
      info.pointerToVector = true;
    }
    info.isSoaVector = collectionFamily == "soa";
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = valueKindFromTypeName(elementTypeText);
    }
    if (info.structTypeName.empty() &&
        valueKindFromTypeName(elementTypeText) == LocalInfo::ValueKind::Unknown) {
      if (info.isSoaVector) {
        info.structTypeName = specializedExperimentalSoaVectorStructPathForElementType(
            elementTypeText);
      } else {
        info.structTypeName =
            specializedCollectionVectorRecordPathForElementType(elementTypeText);
      }
    }
    return;
  }
  if (collectionFamily == "map") {
    info.keyValueKeyKind = valueKindFromTypeName(resolveSemanticCollectionSpecializationText(
        semanticProgram, collectionFact.keyTypeText, collectionFact.keyTypeTextId));
    info.keyValueValueKind = valueKindFromTypeName(resolveSemanticCollectionSpecializationText(
        semanticProgram, collectionFact.valueTypeText, collectionFact.valueTypeTextId));
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = info.keyValueValueKind;
    }
    if (info.structTypeName.empty()) {
      info.structTypeName = resolveSemanticCollectionSpecializationText(
          semanticProgram, collectionFact.structPath, collectionFact.structPathId);
    }
  }
}

inline LocalInfo::Kind bindingKindFromTypeText(const std::string &typeText) {
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
  if (normalizedType == "vector" || normalizedType == "soa") {
    return LocalInfo::Kind::Vector;
  }
  if (normalizedType == "map") {
    return LocalInfo::Kind::Value;
  }
  if (normalizedType == "Buffer") {
    return LocalInfo::Kind::Buffer;
  }
  return LocalInfo::Kind::Value;
}

inline bool isStringBindingTypeText(const std::string &typeText) {
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

inline bool isFileErrorBindingTypeText(const std::string &typeText) {
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

inline LocalInfo::ValueKind bindingValueKindFromTypeText(const std::string &typeText, LocalInfo::Kind kind) {
  std::string base;
  std::string arg;
  const std::string normalizedType = trimTemplateTypeText(typeText);
  if (splitTemplateTypeName(normalizedType, base, arg)) {
    base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if (base == "Pointer" || base == "Reference") {
      return valueKindFromTypeName(unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(arg)));
    }
    if (base == "array" || base == "vector" || base == "soa" || base == "Buffer") {
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

inline void setReferenceArrayInfoFromTypeText(const std::string &typeText, LocalInfo &info) {
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
  if (soa_paths::isExperimentalColumnarVectorSpecializedTypePath(normalizedTargetType)) {
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
  if (base == "soa") {
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
    info.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(args[0]));
    info.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(args[1]));
    if (info.valueKind == LocalInfo::ValueKind::Unknown) {
      info.valueKind = info.keyValueValueKind;
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

inline bool validateInternedSemanticTextMetadata(const SemanticProgram &semanticProgram,
                                          SymbolId textId,
                                          std::string_view expectedText,
                                          std::string_view factLabel,
                                          std::string_view fieldLabel,
                                          const std::string &siteDescription,
                                          std::string &error,
                                          bool allowEmptyExpectedText = true) {
  if (textId == InvalidSymbolId) {
    return true;
  }
  const std::string_view resolvedText =
      semanticProgramResolveCallTargetString(semanticProgram, textId);
  if (resolvedText.empty()) {
    error = "missing semantic-product " + std::string(factLabel) + " " +
            std::string(fieldLabel) +
            " id: " + siteDescription;
    return false;
  }
  if ((expectedText.empty() && !allowEmptyExpectedText) ||
      (!expectedText.empty() && resolvedText != expectedText)) {
    error = "stale semantic-product " + std::string(factLabel) + " " +
            std::string(fieldLabel) +
            " metadata: " + siteDescription;
    return false;
  }
  return true;
}

} // namespace binding_type_helpers_file_local
} // namespace primec::ir_lowerer
