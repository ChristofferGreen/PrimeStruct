#include "IrLowererCallHelpers.h"

#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isExperimentalVectorStructPath(const std::string &structTypeName) {
  return isExperimentalCollectionTypeName(structTypeName, "vector", "Vector");
}

std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

std::string inferExperimentalSoaVectorStructPathFromTypeName(
    const std::string &typeName) {
  const size_t first = typeName.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = typeName.find_last_not_of(" \t\r\n");
  std::string normalizedArg = typeName.substr(first, last - first + 1);
  if (normalizedArg.empty()) {
    return "";
  }
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedExperimentalSoaVectorStructPathForElementType(normalizedArg);
}

std::string inferExperimentalVectorStructPathFromTypeName(
    const std::string &typeName) {
  const size_t first = typeName.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = typeName.find_last_not_of(" \t\r\n");
  std::string normalizedArg = typeName.substr(first, last - first + 1);
  if (normalizedArg.empty()) {
    return "";
  }
  if (!normalizedArg.empty() && normalizedArg.front() == '/') {
    normalizedArg.erase(normalizedArg.begin());
  }
  return specializedExperimentalVectorStructPathForElementType(normalizedArg);
}

bool hasInferredTypedWrappedKeyValue(const LocalInfo &localInfo, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         localInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
         localInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown;
}

const StdlibSurfaceMetadata *keyValueHelperSurfaceMetadataForAccessTargets() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

const StdlibSurfaceMetadata *keyValueConstructorSurfaceMetadataForAccessTargets() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_constructors");
}

bool isKeyValueAccessHelperName(std::string_view helperName) {
  return helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref";
}

bool resolveKeyValueAccessHelperPathMemberName(std::string_view path,
                                          std::string &helperNameOut) {
  helperNameOut.clear();
  const auto *metadata = keyValueHelperSurfaceMetadataForAccessTargets();
  if (metadata == nullptr) {
    return false;
  }
  const std::string_view helperName =
      resolveStdlibSurfaceMemberName(*metadata, path);
  if (helperName.empty()) {
    return false;
  }
  helperNameOut.assign(helperName);
  return true;
}

bool isExplicitKeyValueAccessHelperPath(std::string_view path) {
  std::string helperName;
  return resolveKeyValueAccessHelperPathMemberName(path, helperName) &&
         isKeyValueAccessHelperName(helperName);
}

bool resolveKeyValueConstructorExprMemberName(const Expr &expr,
                                              std::string &constructorNameOut) {
  constructorNameOut.clear();
  const auto *metadata = keyValueConstructorSurfaceMetadataForAccessTargets();
  return metadata != nullptr &&
         resolvePublishedStdlibSurfaceConstructorExprMemberName(
             expr, metadata->id, constructorNameOut);
}

bool resolveKeyValueConstructorPathMemberName(std::string_view path,
                                              std::string &constructorNameOut) {
  constructorNameOut.clear();
  const auto *metadata = keyValueConstructorSurfaceMetadataForAccessTargets();
  return metadata != nullptr &&
         resolvePublishedStdlibSurfaceConstructorMemberName(
             path, metadata->id, constructorNameOut);
}

bool isPublishedKeyValueConstructorExpr(const Expr &expr) {
  std::string constructorName;
  return resolveKeyValueConstructorExprMemberName(expr, constructorName);
}

std::string forwardedEmptyKeyValueConstructorMemberName() {
  const auto *metadata = keyValueConstructorSurfaceMetadataForAccessTargets();
  if (metadata != nullptr) {
    const std::string_view memberName =
        resolveStdlibSurfaceMemberName(*metadata, metadata->canonicalPath);
    if (!memberName.empty()) {
      return std::string(memberName) + "New";
    }
  }
  return std::string("map") + std::string("New");
}

std::string mapKindTypeName(LocalInfo::ValueKind kind) {
  switch (kind) {
  case LocalInfo::ValueKind::Int32:
    return "i32";
  case LocalInfo::ValueKind::Int64:
    return "i64";
  case LocalInfo::ValueKind::UInt64:
    return "u64";
  case LocalInfo::ValueKind::Bool:
    return "bool";
  case LocalInfo::ValueKind::Float32:
    return "f32";
  case LocalInfo::ValueKind::Float64:
    return "f64";
  case LocalInfo::ValueKind::String:
    return "string";
  default:
    return "";
  }
}

std::string inferExperimentalMapStructPathFromKinds(LocalInfo::ValueKind keyKind,
                                                    LocalInfo::ValueKind valueKind) {
  const std::string keyType = mapKindTypeName(keyKind);
  const std::string valueType = mapKindTypeName(valueKind);
  if (keyType.empty() || valueType.empty()) {
    return "";
  }

  std::ostringstream specializedPath;
  specializedPath << collectionTypePath("map") << "/MapValue"
                  << mangleTemplateTypeArgsSuffix({keyType, valueType});
  return specializedPath.str();
}

std::string resolveAccessSemanticTypeText(const SemanticProgram *semanticProgram,
                                          const std::string &typeText,
                                          SymbolId typeTextId) {
  if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
    const std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

std::string normalizeAccessCollectionFamily(std::string family) {
  family = normalizeCollectionBindingTypeName(trimTemplateTypeText(family));
  if (!family.empty() && family.front() == '/') {
    family.erase(family.begin());
  }
  return family;
}

bool classifySemanticArrayVectorAccessTypeText(const std::string &typeText,
                                               ArrayVectorAccessTargetInfo &targetInfoOut) {
  std::string normalizedDirectType = trimTemplateTypeText(typeText);
  if (isExperimentalVectorStructPath(normalizedDirectType)) {
    targetInfoOut = {};
    targetInfoOut.isArrayOrVectorTarget = true;
    targetInfoOut.isVectorTarget = true;
    targetInfoOut.structTypeName = normalizedDirectType;
    return true;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return false;
  }
  base = normalizeAccessCollectionFamily(base);
  std::string elementText = trimTemplateTypeText(argText);
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> wrappedArgs;
    if (!splitTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
      return false;
    }
    if (!splitTemplateTypeName(trimTemplateTypeText(wrappedArgs.front()), base, elementText)) {
      return false;
    }
    base = normalizeAccessCollectionFamily(base);
  }

  if (base != "array" && base != "vector" && base != "Buffer" && base != "soa" "_vector") {
    return false;
  }
  std::vector<std::string> elementArgs;
  if (!splitTemplateArgs(elementText, elementArgs) || elementArgs.empty()) {
    elementArgs = {elementText};
  }
  if (elementArgs.size() != 1) {
    return false;
  }

  targetInfoOut = {};
  targetInfoOut.isArrayOrVectorTarget = true;
  targetInfoOut.isVectorTarget = (base == "vector");
  targetInfoOut.isSoaVector = (base == "soa" "_vector");
  targetInfoOut.elemKind = valueKindFromTypeName(trimTemplateTypeText(elementArgs.front()));
  if (targetInfoOut.isSoaVector) {
    targetInfoOut.structTypeName =
        inferExperimentalSoaVectorStructPathFromTypeName(elementArgs.front());
  } else if (base == "vector" &&
             targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown) {
    targetInfoOut.structTypeName =
        inferExperimentalVectorStructPathFromTypeName(elementArgs.front());
  }
  return true;
}

bool classifySemanticKeyValueAccessTypeText(const std::string &typeText,
                                       KeyValueAccessTargetInfo &targetInfoOut) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return false;
  }
  base = normalizeAccessCollectionFamily(base);
  bool isWrappedKeyValue = false;
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> wrappedArgs;
    if (!splitTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
      return false;
    }
    if (!splitTemplateTypeName(trimTemplateTypeText(wrappedArgs.front()), base, argText)) {
      return false;
    }
    base = normalizeAccessCollectionFamily(base);
    isWrappedKeyValue = true;
  }
  if (base != "map") {
    return false;
  }

  std::vector<std::string> keyValueArgs;
  if (!splitTemplateArgs(argText, keyValueArgs) || keyValueArgs.size() != 2) {
    return false;
  }

  targetInfoOut = {};
  targetInfoOut.isKeyValueTarget = true;
  targetInfoOut.keyValueKeyKind = valueKindFromTypeName(trimTemplateTypeText(keyValueArgs.front()));
  targetInfoOut.keyValueValueKind = valueKindFromTypeName(trimTemplateTypeText(keyValueArgs.back()));
  targetInfoOut.isWrappedKeyValueTarget = isWrappedKeyValue;
  targetInfoOut.structTypeName =
      inferExperimentalMapStructPathFromKinds(targetInfoOut.keyValueKeyKind,
                                              targetInfoOut.keyValueValueKind);
  return true;
}

bool resolveSemanticArrayVectorAccessTargetInfo(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    ArrayVectorAccessTargetInfo &targetInfoOut,
    bool &hasSemanticFactOut) {
  hasSemanticFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || targetExpr.semanticNodeId == 0) {
    return false;
  }

  auto tryClassifyType = [&](const std::string &typeText, SymbolId typeTextId) {
    const std::string resolvedTypeText =
        resolveAccessSemanticTypeText(semanticProgram, typeText, typeTextId);
    return classifySemanticArrayVectorAccessTypeText(resolvedTypeText, targetInfoOut);
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, targetExpr);
      collectionFact != nullptr) {
    hasSemanticFactOut = true;
    const std::string rawFamily = resolveAccessSemanticTypeText(
        semanticProgram,
        collectionFact->collectionFamily,
        collectionFact->collectionFamilyId);
    const std::string family = normalizeAccessCollectionFamily(rawFamily);
    if (family != "array" && family != "vector" && family != "Buffer" &&
        family != "soa" "_vector") {
      return false;
    }
    targetInfoOut = {};
    targetInfoOut.isArrayOrVectorTarget = true;
    targetInfoOut.isVectorTarget = (family == "vector");
    targetInfoOut.isSoaVector = (family == "soa" "_vector");
    const std::string elementTypeText = resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->elementTypeText, collectionFact->elementTypeTextId);
    targetInfoOut.elemKind = valueKindFromTypeName(elementTypeText);
    if (targetInfoOut.isSoaVector) {
      targetInfoOut.structTypeName =
          inferExperimentalSoaVectorStructPathFromTypeName(elementTypeText);
    } else if (family == "vector" &&
               targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown) {
      targetInfoOut.structTypeName = elementTypeText.empty() &&
                                             isExperimentalVectorStructPath(rawFamily)
                                         ? rawFamily
                                         : inferExperimentalVectorStructPathFromTypeName(elementTypeText);
    }
    if (targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown &&
        targetInfoOut.structTypeName.empty()) {
      return false;
    }
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, targetExpr);
      queryFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(queryFact->queryTypeText, queryFact->queryTypeTextId) ||
           tryClassifyType(queryFact->bindingTypeText, queryFact->bindingTypeTextId) ||
           tryClassifyType(queryFact->receiverBindingTypeText,
                           queryFact->receiverBindingTypeTextId);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, targetExpr);
      bindingFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, targetExpr);
      localAutoFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
  }
  return false;
}

bool resolveSemanticKeyValueAccessTargetInfo(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    KeyValueAccessTargetInfo &targetInfoOut,
    bool &hasSemanticFactOut) {
  hasSemanticFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || targetExpr.semanticNodeId == 0) {
    return false;
  }

  auto tryClassifyType = [&](const std::string &typeText, SymbolId typeTextId) {
    const std::string resolvedTypeText =
        resolveAccessSemanticTypeText(semanticProgram, typeText, typeTextId);
    return classifySemanticKeyValueAccessTypeText(resolvedTypeText, targetInfoOut);
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, targetExpr);
      collectionFact != nullptr) {
    hasSemanticFactOut = true;
    const std::string family = normalizeAccessCollectionFamily(
        resolveAccessSemanticTypeText(
            semanticProgram,
            collectionFact->collectionFamily,
            collectionFact->collectionFamilyId));
    if (family != "map") {
      return false;
    }
    targetInfoOut = {};
    targetInfoOut.isKeyValueTarget = true;
    targetInfoOut.keyValueKeyKind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->keyTypeText, collectionFact->keyTypeTextId));
    targetInfoOut.keyValueValueKind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->valueTypeText, collectionFact->valueTypeTextId));
    targetInfoOut.isWrappedKeyValueTarget = collectionFact->isReference || collectionFact->isPointer;
    targetInfoOut.structTypeName =
        inferExperimentalMapStructPathFromKinds(targetInfoOut.keyValueKeyKind,
                                                targetInfoOut.keyValueValueKind);
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, targetExpr);
      queryFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(queryFact->queryTypeText, queryFact->queryTypeTextId) ||
           tryClassifyType(queryFact->bindingTypeText, queryFact->bindingTypeTextId) ||
           tryClassifyType(queryFact->receiverBindingTypeText,
                           queryFact->receiverBindingTypeTextId);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, targetExpr);
      bindingFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, targetExpr);
      localAutoFact != nullptr) {
    hasSemanticFactOut = true;
    return tryClassifyType(localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
  }
  return false;
}

LocalInfo::ValueKind resolveAccessIndexSemanticKind(
    const Expr &indexExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr || indexExpr.semanticNodeId == 0) {
    return LocalInfo::ValueKind::Unknown;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, indexExpr);
      queryFact != nullptr) {
    LocalInfo::ValueKind kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, queryFact->queryTypeText, queryFact->queryTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
    kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, queryFact->bindingTypeText, queryFact->bindingTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, indexExpr);
      bindingFact != nullptr) {
    const LocalInfo::ValueKind kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, bindingFact->bindingTypeText, bindingFact->bindingTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, indexExpr);
      localAutoFact != nullptr) {
    const LocalInfo::ValueKind kind = valueKindFromTypeName(resolveAccessSemanticTypeText(
        semanticProgram, localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId));
    if (kind != LocalInfo::ValueKind::Unknown) {
      return kind;
    }
  }
  return LocalInfo::ValueKind::Unknown;
}

const Expr *findForwardedReturnValueExpr(const Definition &definition) {
  if (definition.returnExpr.has_value()) {
    return &*definition.returnExpr;
  }
  const Expr *valueExpr = nullptr;
  for (const auto &stmt : definition.statements) {
    if (stmt.isBinding) {
      continue;
    }
    valueExpr = &stmt;
    if (isReturnCall(stmt) && stmt.args.size() == 1) {
      return &stmt.args.front();
    }
  }
  return valueExpr;
}

bool resolveForwardedReturnParameterName(const Definition &definition,
                                         std::string &parameterNameOut) {
  parameterNameOut.clear();
  const Expr *valueExpr = findForwardedReturnValueExpr(definition);
  if (valueExpr == nullptr) {
    return false;
  }
  if (isReturnCall(*valueExpr) && valueExpr->args.size() == 1) {
    valueExpr = &valueExpr->args.front();
  }
  if (valueExpr->kind != Expr::Kind::Name) {
    return false;
  }
  for (const auto &parameter : definition.parameters) {
    if (parameter.name == valueExpr->name) {
      parameterNameOut = valueExpr->name;
      return true;
    }
  }
  return false;
}

const Expr *resolveCallArgumentForParameter(const Expr &target,
                                            const Definition &callee,
                                            const std::string &parameterName) {
  for (size_t index = 0; index < target.args.size(); ++index) {
    if (index < target.argNames.size() &&
        target.argNames[index].has_value() &&
        *target.argNames[index] == parameterName) {
      return &target.args[index];
    }
  }
  for (size_t index = 0; index < callee.parameters.size(); ++index) {
    if (callee.parameters[index].name != parameterName) {
      continue;
    }
    return index < target.args.size() ? &target.args[index] : nullptr;
  }
  return nullptr;
}

bool isForwardedKeyValueNewConstructor(const Expr &expr) {
  std::string constructorName;
  return resolveKeyValueConstructorExprMemberName(expr, constructorName) &&
         constructorName == forwardedEmptyKeyValueConstructorMemberName();
}

bool inferDirectKeyValueConstructorTargetInfo(const Expr &target, KeyValueAccessTargetInfo &info) {
  info = {};
  if (target.kind != Expr::Kind::Call || target.isBinding || target.isMethodCall) {
    return false;
  }

  std::string normalizedName = target.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  auto isDirectKeyValueConstructor = [&]() {
    std::string constructorName;
    return resolveKeyValueConstructorPathMemberName(normalizedName, constructorName) ||
           isPublishedKeyValueConstructorExpr(target);
  };
  auto inferLiteralKind = [&](const Expr &valueExpr, LocalInfo::ValueKind &kindOut) {
    kindOut = LocalInfo::ValueKind::Unknown;
    if (valueExpr.kind == Expr::Kind::Literal) {
      kindOut = valueExpr.isUnsigned ? LocalInfo::ValueKind::UInt64
                                     : (valueExpr.intWidth == 64 ? LocalInfo::ValueKind::Int64
                                                                 : LocalInfo::ValueKind::Int32);
      return true;
    }
    if (valueExpr.kind == Expr::Kind::BoolLiteral) {
      kindOut = LocalInfo::ValueKind::Bool;
      return true;
    }
    if (valueExpr.kind == Expr::Kind::FloatLiteral) {
      kindOut = valueExpr.floatWidth == 64 ? LocalInfo::ValueKind::Float64 : LocalInfo::ValueKind::Float32;
      return true;
    }
    if (valueExpr.kind == Expr::Kind::StringLiteral) {
      kindOut = LocalInfo::ValueKind::String;
      return true;
    }
    return false;
  };

  if (!isDirectKeyValueConstructor()) {
    return false;
  }

  if (target.templateArgs.size() == 2) {
    info.isKeyValueTarget = true;
    info.keyValueKeyKind = valueKindFromTypeName(target.templateArgs[0]);
    info.keyValueValueKind = valueKindFromTypeName(target.templateArgs[1]);
    info.structTypeName =
        inferExperimentalMapStructPathFromKinds(info.keyValueKeyKind, info.keyValueValueKind);
    return true;
  }

  if (target.args.empty() || (target.args.size() % 2) != 0) {
    return false;
  }

  info.isKeyValueTarget = true;
  LocalInfo::ValueKind keyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  for (size_t i = 0; i < target.args.size(); i += 2) {
    LocalInfo::ValueKind currentKeyKind = LocalInfo::ValueKind::Unknown;
    LocalInfo::ValueKind currentValueKind = LocalInfo::ValueKind::Unknown;
    if (!inferLiteralKind(target.args[i], currentKeyKind) ||
        !inferLiteralKind(target.args[i + 1], currentValueKind)) {
      return true;
    }
    if (keyKind == LocalInfo::ValueKind::Unknown) {
      keyKind = currentKeyKind;
    } else if (keyKind != currentKeyKind) {
      return false;
    }
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      valueKind = currentValueKind;
    } else if (valueKind != currentValueKind) {
      return false;
    }
  }

  info.keyValueKeyKind = keyKind;
  info.keyValueValueKind = valueKind;
  info.structTypeName =
      inferExperimentalMapStructPathFromKinds(info.keyValueKeyKind, info.keyValueValueKind);
  return true;
}

} // namespace

SemanticStringAccessTargetKind classifyAccessTargetSemanticStringKind(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr || targetExpr.semanticNodeId == 0) {
    return SemanticStringAccessTargetKind::Unknown;
  }

  auto classifyTypeText = [&](const std::string &typeText,
                              SymbolId typeTextId) {
    const std::string resolvedTypeText =
        resolveAccessSemanticTypeText(semanticProgram, typeText, typeTextId);
    if (resolvedTypeText.empty()) {
      return SemanticStringAccessTargetKind::Unknown;
    }
    return valueKindFromTypeName(resolvedTypeText) == LocalInfo::ValueKind::String
               ? SemanticStringAccessTargetKind::String
               : SemanticStringAccessTargetKind::NonString;
  };

  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, targetExpr);
      collectionFact != nullptr) {
    const std::string collectionFamily = resolveAccessSemanticTypeText(
        semanticProgram, collectionFact->collectionFamily, collectionFact->collectionFamilyId);
    if (collectionFamily == "string") {
      return SemanticStringAccessTargetKind::String;
    }
    return SemanticStringAccessTargetKind::NonString;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, targetExpr);
      queryFact != nullptr) {
    SemanticStringAccessTargetKind kind =
        classifyTypeText(queryFact->queryTypeText, queryFact->queryTypeTextId);
    if (kind != SemanticStringAccessTargetKind::Unknown) {
      return kind;
    }
    return classifyTypeText(queryFact->bindingTypeText, queryFact->bindingTypeTextId);
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, targetExpr);
      bindingFact != nullptr) {
    return classifyTypeText(bindingFact->bindingTypeText, bindingFact->bindingTypeTextId);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, targetExpr);
      localAutoFact != nullptr) {
    return classifyTypeText(localAutoFact->bindingTypeText, localAutoFact->bindingTypeTextId);
  }
  return SemanticStringAccessTargetKind::Unknown;
}

KeyValueAccessTargetInfo resolveKeyValueAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallKeyValueAccessTargetInfoFn &resolveCallKeyValueAccessTargetInfo,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  KeyValueAccessTargetInfo info;
  const auto peelLocationWrappers = [&](const Expr &expr) {
    const Expr *current = &expr;
    while (current->kind == Expr::Kind::Call &&
           isSimpleCallName(*current, "location") &&
           current->args.size() == 1) {
      current = &current->args.front();
    }
    return current;
  };
  auto populateFromDirectLocal = [&](const LocalInfo &localInfo, bool dereferenced) {
    const bool inferredWrappedKeyValue = hasInferredTypedWrappedKeyValue(localInfo, localInfo.kind);
    if (localInfo.kind != LocalInfo::Kind::KeyValueCollection &&
        !(localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToKeyValueCollection) &&
        !(localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToKeyValueCollection) &&
        !inferredWrappedKeyValue) {
      return false;
    }
    auto resolvedExperimentalMapStructPath = [&](const std::string &structTypeName) {
      const std::string experimentalMapType =
          experimentalCollectionTypePath("map", "Map", false);
      const std::string rootedExperimentalMapType =
          experimentalCollectionTypePath("map", "Map");
      if (structTypeName.empty()) {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.keyValueKeyKind,
                                                    localInfo.keyValueValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      if (structTypeName == experimentalMapType ||
          structTypeName == rootedExperimentalMapType) {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.keyValueKeyKind,
                                                    localInfo.keyValueValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      return structTypeName;
    };
    info.isKeyValueTarget = true;
    info.keyValueKeyKind = localInfo.keyValueKeyKind;
    info.keyValueValueKind = localInfo.keyValueValueKind;
    const bool isWrappedKeyValue =
        (localInfo.kind == LocalInfo::Kind::Reference && localInfo.referenceToKeyValueCollection) ||
        (localInfo.kind == LocalInfo::Kind::Pointer && localInfo.pointerToKeyValueCollection) ||
        inferredWrappedKeyValue;
    info.isWrappedKeyValueTarget = isWrappedKeyValue && !dereferenced;
    const bool isDirectMapStorage = localInfo.kind == LocalInfo::Kind::KeyValueCollection;
    const std::string resolvedStructTypeName =
        resolvedExperimentalMapStructPath(localInfo.structTypeName);
    const bool preserveDirectExperimentalMapStruct =
        isDirectMapStorage && isExperimentalMapStructTypePath(resolvedStructTypeName);
    if (((!info.isWrappedKeyValueTarget || dereferenced) &&
         (!isDirectMapStorage || preserveDirectExperimentalMapStruct)) ||
        (info.isWrappedKeyValueTarget && isExperimentalMapStructTypePath(resolvedStructTypeName))) {
      info.structTypeName = resolvedStructTypeName;
    }
    return true;
  };
  auto populateFromArgsPackElement = [&](const LocalInfo &localInfo, bool dereferenced) {
    if (!localInfo.isArgsPack) {
      return false;
    }
    auto resolvedExperimentalMapStructPath = [&](const std::string &structTypeName) {
      const std::string experimentalMapType =
          experimentalCollectionTypePath("map", "Map", false);
      const std::string rootedExperimentalMapType =
          experimentalCollectionTypePath("map", "Map");
      if (structTypeName.empty()) {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.keyValueKeyKind,
                                                    localInfo.keyValueValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      if (structTypeName == experimentalMapType ||
          structTypeName == rootedExperimentalMapType) {
        const std::string specialized =
            inferExperimentalMapStructPathFromKinds(localInfo.keyValueKeyKind,
                                                    localInfo.keyValueValueKind);
        if (!specialized.empty()) {
          return specialized;
        }
      }
      return structTypeName;
    };
    const bool isDirectMap = localInfo.argsPackElementKind == LocalInfo::Kind::KeyValueCollection;
    const bool inferredWrappedKeyValue =
        hasInferredTypedWrappedKeyValue(localInfo, localInfo.argsPackElementKind);
    const bool isWrappedKeyValue =
        (localInfo.argsPackElementKind == LocalInfo::Kind::Reference && localInfo.referenceToKeyValueCollection) ||
        (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer && localInfo.pointerToKeyValueCollection) ||
        inferredWrappedKeyValue;
    if (!isDirectMap && !isWrappedKeyValue) {
      return false;
    }
    info.isKeyValueTarget = true;
    info.keyValueKeyKind = localInfo.keyValueKeyKind;
    info.keyValueValueKind = localInfo.keyValueValueKind;
    info.isWrappedKeyValueTarget = isWrappedKeyValue && !dereferenced;
    const bool isDirectMapStorage =
        localInfo.argsPackElementKind == LocalInfo::Kind::KeyValueCollection;
    const std::string resolvedStructTypeName =
        resolvedExperimentalMapStructPath(localInfo.structTypeName);
    const bool preserveDirectExperimentalMapStruct =
        isDirectMapStorage && isExperimentalMapStructTypePath(resolvedStructTypeName);
    if (((!info.isWrappedKeyValueTarget || dereferenced) &&
         (!isDirectMapStorage || preserveDirectExperimentalMapStruct)) ||
        (info.isWrappedKeyValueTarget && isExperimentalMapStructTypePath(resolvedStructTypeName))) {
      info.structTypeName = resolvedStructTypeName;
    }
    return true;
  };
  auto resolveArgsPackAccessTarget = [&](const Expr &candidate,
                                         bool dereferenced) {
    const Expr *accessReceiver = peelLocationWrappers(candidate);
    bool receiverDereferenced = dereferenced;
    while (accessReceiver->kind == Expr::Kind::Call &&
           isSimpleCallName(*accessReceiver, "dereference") &&
           accessReceiver->args.size() == 1) {
      receiverDereferenced = true;
      accessReceiver = peelLocationWrappers(accessReceiver->args.front());
    }
    if (accessReceiver->kind == Expr::Kind::Name) {
      auto it = localsIn.find(accessReceiver->name);
      if (it != localsIn.end() && populateFromArgsPackElement(it->second, false)) {
        info.isWrappedKeyValueTarget = info.isWrappedKeyValueTarget && !receiverDereferenced;
        return true;
      }
    }
    return false;
  };

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackElement(it->second, false)) {
      return info;
    }
  }
  if (target.kind == Expr::Kind::Call) {
    std::string preSemanticAccessName;
    std::string preSemanticKeyValueHelperName;
    const std::string preSemanticScopedPath = resolveScopedCallPath(target);
    const bool preSemanticAliasKeyValueAccess =
        resolveKeyValueHelperAliasName(target, preSemanticKeyValueHelperName) &&
        isKeyValueAccessHelperName(preSemanticKeyValueHelperName);
    const bool preSemanticExplicitKeyValueAccess =
        !target.isMethodCall &&
        (isExplicitKeyValueAccessHelperPath(preSemanticScopedPath) ||
         preSemanticAliasKeyValueAccess) &&
        target.args.size() == 2;
    if (((getBuiltinArrayAccessName(target, preSemanticAccessName) &&
          target.args.size() == 2) ||
         preSemanticExplicitKeyValueAccess) &&
        resolveArgsPackAccessTarget(target.args.front(), false)) {
      return info;
    }
  }

  bool hasSemanticTargetFact = false;
  if (resolveSemanticKeyValueAccessTargetInfo(
          target, semanticProgram, semanticIndex, info, hasSemanticTargetFact)) {
    return info;
  }
  if (hasSemanticTargetFact) {
    return {};
  }
  if (target.kind == Expr::Kind::Name) {
    if (target.semanticNodeId != 0 && resolveCallKeyValueAccessTargetInfo) {
      KeyValueAccessTargetInfo inferred;
      if (resolveCallKeyValueAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    auto it = localsIn.find(target.name);
    if (it != localsIn.end()) {
      populateFromDirectLocal(it->second, false);
    }
    if (!info.isKeyValueTarget && resolveCallKeyValueAccessTargetInfo) {
      KeyValueAccessTargetInfo inferred;
      if (resolveCallKeyValueAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        if (it != localsIn.end() && populateFromDirectLocal(it->second, true)) {
          return info;
        }
      }
      std::string derefAccessName;
      const std::string derefScopedPath = resolveScopedCallPath(derefTarget);
      const bool isDerefKeyValueAccess =
          derefTarget.kind == Expr::Kind::Call &&
          ((getBuiltinArrayAccessName(derefTarget, derefAccessName) &&
            derefTarget.args.size() == 2) ||
           (isExplicitKeyValueAccessHelperPath(derefScopedPath) &&
            derefTarget.args.size() == 2));
      if (isDerefKeyValueAccess &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.args.front().name);
        if (it != localsIn.end() && populateFromArgsPackElement(it->second, true)) {
          return info;
        }
      }
    }
    std::string accessName;
    std::string helperName;
    const std::string scopedTargetPath = resolveScopedCallPath(target);
    const bool isAliasKeyValueArgsPackAccess =
        resolveKeyValueHelperAliasName(target, helperName) &&
        isKeyValueAccessHelperName(helperName);
    const bool isExplicitKeyValueArgsPackAccess =
        !target.isMethodCall &&
        (isExplicitKeyValueAccessHelperPath(scopedTargetPath) ||
         isAliasKeyValueArgsPackAccess) &&
        target.args.size() == 2;
    if ((getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) ||
        isExplicitKeyValueArgsPackAccess) {
      if (resolveArgsPackAccessTarget(target.args.front(), false)) {
        return info;
      }
    }
    std::string collection;
    if (resolveCallKeyValueAccessTargetInfo) {
      KeyValueAccessTargetInfo inferred;
      if (resolveCallKeyValueAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    KeyValueAccessTargetInfo directConstructorInfo;
    const bool hasDirectConstructorInfo =
        inferDirectKeyValueConstructorTargetInfo(target, directConstructorInfo);
    if (hasDirectConstructorInfo) {
      return directConstructorInfo;
    }
    if (getBuiltinCollectionName(target, collection) && collection == "map" &&
        target.templateArgs.size() == 2) {
      info.isKeyValueTarget = true;
      info.keyValueKeyKind = valueKindFromTypeName(target.templateArgs[0]);
      info.keyValueValueKind = valueKindFromTypeName(target.templateArgs[1]);
      return info;
    }
  }
  return info;
}

KeyValueAccessTargetInfo resolveKeyValueAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallKeyValueAccessTargetInfoFn &resolveCallKeyValueAccessTargetInfo) {
  return resolveKeyValueAccessTargetInfo(
      target, localsIn, resolveCallKeyValueAccessTargetInfo, nullptr, nullptr);
}

KeyValueAccessTargetInfo resolveKeyValueAccessTargetInfo(const Expr &target, const LocalMap &localsIn) {
  return resolveKeyValueAccessTargetInfo(target, localsIn, {}, nullptr, nullptr);
}

bool inferForwardedKeyValueAccessTargetInfo(
    const Expr &target,
    const Definition &callee,
    const LocalMap &localsIn,
    const ResolveCallKeyValueAccessTargetInfoFn &resolveCallKeyValueAccessTargetInfo,
    KeyValueAccessTargetInfo &targetInfoOut) {
  targetInfoOut = {};
  if (target.kind != Expr::Kind::Call || target.isMethodCall || target.isBinding) {
    return false;
  }

  std::string parameterName;
  if (!resolveForwardedReturnParameterName(callee, parameterName)) {
    return false;
  }
  const Expr *forwardedArg =
      resolveCallArgumentForParameter(target, callee, parameterName);
  if (forwardedArg == nullptr) {
    return false;
  }
  if (isForwardedKeyValueNewConstructor(*forwardedArg)) {
    return false;
  }

  KeyValueAccessTargetInfo forwardedInfo =
      resolveKeyValueAccessTargetInfo(
          *forwardedArg, localsIn, resolveCallKeyValueAccessTargetInfo, nullptr, nullptr);
  if (!forwardedInfo.isKeyValueTarget) {
    return false;
  }
  targetInfoOut = std::move(forwardedInfo);
  return true;
}

bool validateKeyValueAccessTargetInfo(const KeyValueAccessTargetInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error) {
  if (!targetInfo.isKeyValueTarget) {
    return true;
  }
  if (targetInfo.keyValueKeyKind == LocalInfo::ValueKind::Unknown ||
      targetInfo.keyValueValueKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires typed map bindings for " + accessName;
    return false;
  }
  return true;
}

NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (targetExpr.kind == Expr::Kind::StringLiteral) {
    return NonLiteralStringAccessTargetResult::Stop;
  }
  const SemanticStringAccessTargetKind semanticTargetKind =
      classifyAccessTargetSemanticStringKind(targetExpr, semanticProgram, semanticIndex);
  if (semanticTargetKind == SemanticStringAccessTargetKind::NonString) {
    return NonLiteralStringAccessTargetResult::Continue;
  }
  if (targetExpr.kind == Expr::Kind::Name) {
    const LocalInfo::ValueKind targetKind =
        semanticTargetKind == SemanticStringAccessTargetKind::String
            ? LocalInfo::ValueKind::String
            : inferExprKind(targetExpr, localsIn);
    if (targetKind != LocalInfo::ValueKind::Unknown &&
        targetKind != LocalInfo::ValueKind::String) {
      return NonLiteralStringAccessTargetResult::Continue;
    }
    if (targetKind == LocalInfo::ValueKind::String) {
      error = "native backend only supports indexing into string literals or string bindings";
      return NonLiteralStringAccessTargetResult::Error;
    }
    auto it = localsIn.find(targetExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.valueKind == LocalInfo::ValueKind::String) {
      error = "native backend only supports indexing into string literals or string bindings";
      return NonLiteralStringAccessTargetResult::Error;
    }
  }
  if (isEntryArgsName(targetExpr, localsIn)) {
    error = "native backend only supports entry argument indexing in print calls or string bindings";
    return NonLiteralStringAccessTargetResult::Error;
  }
  return NonLiteralStringAccessTargetResult::Continue;
}

NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error) {
  return validateNonLiteralStringAccessTarget(
      targetExpr, localsIn, inferExprKind, isEntryArgsName, error, nullptr, nullptr);
}

bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  indexKindOut = normalizeIndexKind(resolveAccessIndexSemanticKind(indexExpr, semanticProgram, semanticIndex));
  if (indexKindOut == LocalInfo::ValueKind::Unknown) {
    indexKindOut = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
  }
  if (!isSupportedIndexKind(indexKindOut)) {
    error = "native backend requires integer indices for " + accessName;
    return false;
  }
  return true;
}

bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error) {
  return resolveValidatedAccessIndexKind(
      indexExpr, localsIn, accessName, inferExprKind, indexKindOut, error, nullptr, nullptr);
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  ArrayVectorAccessTargetInfo info;
  const auto elementSlotCountForLocal = [](const LocalInfo &localInfo) {
    if (localInfo.isArgsPack) {
      const bool isInlineStructPack =
          (localInfo.argsPackElementKind == LocalInfo::Kind::Value ||
           localInfo.argsPackElementKind == LocalInfo::Kind::KeyValueCollection) &&
          !localInfo.structTypeName.empty() &&
          localInfo.structSlotCount > 0;
      return isInlineStructPack ? localInfo.structSlotCount : 1;
    }
    return localInfo.structSlotCount;
  };
  const auto populateFromArgsPackLocal = [&](const LocalInfo &localInfo, bool dereferenced) {
    if (!localInfo.isArgsPack) {
      return false;
    }

    info.elemKind = localInfo.valueKind;
    info.isSoaVector = localInfo.isSoaVector;
    info.isArgsPackTarget = !dereferenced;
    info.argsPackElementKind = localInfo.argsPackElementKind;
    info.elemSlotCount = elementSlotCountForLocal(localInfo);
    info.structTypeName = localInfo.structTypeName;
    info.isKeyValueTarget = false;
    info.isWrappedKeyValueTarget = false;

    if (localInfo.argsPackElementKind == LocalInfo::Kind::Array ||
        localInfo.argsPackElementKind == LocalInfo::Kind::Buffer ||
        localInfo.argsPackElementKind == LocalInfo::Kind::Vector) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget =
          dereferenced && localInfo.argsPackElementKind == LocalInfo::Kind::Vector;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::KeyValueCollection) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      info.isKeyValueTarget = true;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
        localInfo.valueKind != LocalInfo::ValueKind::Unknown &&
        localInfo.valueKind != LocalInfo::ValueKind::String &&
        localInfo.structTypeName.empty()) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Value &&
        !localInfo.isSoaVector &&
        isExperimentalVectorStructPath(localInfo.structTypeName)) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = true;
      info.isSoaVector = false;
      info.argsPackElementKind = LocalInfo::Kind::Vector;
      info.elemSlotCount = 1;
      return true;
    }
    if (!localInfo.structTypeName.empty() &&
        (localInfo.argsPackElementKind == LocalInfo::Kind::Value ||
         localInfo.argsPackElementKind == LocalInfo::Kind::KeyValueCollection)) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
        (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToBuffer ||
         localInfo.referenceToKeyValueCollection || !localInfo.structTypeName.empty())) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = localInfo.referenceToVector;
      info.isKeyValueTarget = localInfo.referenceToKeyValueCollection && dereferenced;
      info.isWrappedKeyValueTarget = localInfo.referenceToKeyValueCollection && !dereferenced;
      if (localInfo.referenceToKeyValueCollection && dereferenced) {
        info.structTypeName = localInfo.structTypeName;
      }
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
        localInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
        (localInfo.pointerToArray || localInfo.pointerToVector || localInfo.pointerToBuffer ||
         localInfo.pointerToKeyValueCollection || !localInfo.structTypeName.empty())) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = localInfo.pointerToVector;
      info.isKeyValueTarget = localInfo.pointerToKeyValueCollection && dereferenced;
      info.isWrappedKeyValueTarget = localInfo.pointerToKeyValueCollection && !dereferenced;
      if (localInfo.pointerToKeyValueCollection && dereferenced) {
        info.structTypeName = localInfo.structTypeName;
      }
      return true;
    }
    if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
        localInfo.valueKind != LocalInfo::ValueKind::Unknown) {
      info.isArrayOrVectorTarget = true;
      info.isVectorTarget = false;
      return true;
    }
    return false;
  };

  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackLocal(it->second, false)) {
      return info;
    }
  }

  bool hasSemanticTargetFact = false;
  if (resolveSemanticArrayVectorAccessTargetInfo(
          target, semanticProgram, semanticIndex, info, hasSemanticTargetFact) &&
      (info.elemKind != LocalInfo::ValueKind::Unknown ||
       !info.structTypeName.empty())) {
    return info;
  }
  if (hasSemanticTargetFact && resolveCallArrayVectorAccessTargetInfo) {
    ArrayVectorAccessTargetInfo inferred;
    if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
      return inferred;
    }
  }
  if (hasSemanticTargetFact) {
    return {};
  }

  if (target.kind == Expr::Kind::Name) {
    if (target.semanticNodeId != 0 && resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() && populateFromArgsPackLocal(it->second, false)) {
      return info;
    }
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
         it->second.kind == LocalInfo::Kind::Buffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.isSoaVector && !it->second.structTypeName.empty()) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isSoaVector = true;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        !it->second.isSoaVector && isExperimentalVectorStructPath(it->second.structTypeName)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = true;
      info.isSoaVector = false;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference &&
        (it->second.referenceToArray || it->second.referenceToVector || it->second.referenceToBuffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = it->second.referenceToVector;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer &&
        (it->second.pointerToArray || it->second.pointerToBuffer)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = false;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = true;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = elementSlotCountForLocal(it->second);
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (!info.isArrayOrVectorTarget && resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    if (isSimpleCallName(target, "dereference") &&
        target.args.size() == 1 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.args.front().name);
      if (localIt != localsIn.end() &&
          localIt->second.isArgsPack &&
          populateFromArgsPackLocal(localIt->second, false)) {
        return info;
      }
    }
    auto resolveDereferencedArgsPackTarget = [&](const Expr &derefTarget) {
      std::string derefAccessName;
      if (!(getBuiltinArrayAccessName(derefTarget, derefAccessName) && derefTarget.args.size() == 2)) {
        return false;
      }

      const Expr &accessReceiver = derefTarget.args.front();
      if (accessReceiver.kind != Expr::Kind::Name) {
        return false;
      }

      auto localIt = localsIn.find(accessReceiver.name);
      if (localIt == localsIn.end() || !localIt->second.isArgsPack) {
        return false;
      }

      const LocalInfo &localInfo = localIt->second;
      if (populateFromArgsPackLocal(localInfo, true)) {
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Array) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Buffer) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = false;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Vector) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = true;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
          (localInfo.referenceToArray || localInfo.referenceToVector || localInfo.referenceToBuffer)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.referenceToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      if (localInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
          (localInfo.pointerToArray || localInfo.pointerToVector || localInfo.pointerToBuffer)) {
        info.isArrayOrVectorTarget = true;
        info.elemKind = localInfo.valueKind;
        info.isVectorTarget = localInfo.pointerToVector;
        info.isSoaVector = localInfo.isSoaVector;
        info.isArgsPackTarget = false;
        info.argsPackElementKind = localInfo.argsPackElementKind;
        info.elemSlotCount = elementSlotCountForLocal(localInfo);
        info.structTypeName = localInfo.structTypeName;
        return true;
      }
      return false;
    };

    std::string accessName;
    std::string helperName;
    const std::string scopedTargetPath = resolveScopedCallPath(target);
    const bool isAliasKeyValueArgsPackAccess =
        resolveKeyValueHelperAliasName(target, helperName) &&
        isKeyValueAccessHelperName(helperName);
    const bool isExplicitKeyValueArgsPackAccess =
        !target.isMethodCall &&
        (isExplicitKeyValueAccessHelperPath(scopedTargetPath) ||
         isAliasKeyValueArgsPackAccess) &&
        target.args.size() == 2;
    if ((getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) ||
        isExplicitKeyValueArgsPackAccess) {
      const Expr &accessReceiver = target.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && populateFromArgsPackLocal(localIt->second, false)) {
          return info;
        }
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array ||
              localIt->second.argsPackElementKind == LocalInfo::Kind::Buffer ||
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
               (localIt->second.referenceToArray || localIt->second.referenceToVector ||
                localIt->second.referenceToBuffer))) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget =
                localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
                localIt->second.referenceToVector;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer &&
              (localIt->second.pointerToArray || localIt->second.pointerToBuffer)) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = false;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer && localIt->second.pointerToVector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            info.isArgsPackTarget = true;
            info.argsPackElementKind = localIt->second.argsPackElementKind;
            info.elemSlotCount = elementSlotCountForLocal(localIt->second);
            info.structTypeName = localIt->second.structTypeName;
            return info;
          }
        }
      }
    }
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1 &&
        resolveDereferencedArgsPackTarget(target.args.front())) {
      return info;
    }
    std::string collection;
    if (getBuiltinCollectionName(target, collection) &&
        (collection == "array" || collection == "vector" || collection == "Buffer" ||
         collection == "soa" "_vector") &&
        target.templateArgs.size() == 1) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = valueKindFromTypeName(target.templateArgs.front());
      info.isVectorTarget = (collection == "vector");
      info.isSoaVector = (collection == "soa" "_vector");
      if (info.isSoaVector) {
        info.structTypeName =
            inferExperimentalSoaVectorStructPathFromTypeName(target.templateArgs.front());
      }
      return info;
    }
    if (resolveCallArrayVectorAccessTargetInfo) {
      ArrayVectorAccessTargetInfo inferred;
      if (resolveCallArrayVectorAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
  }
  return info;
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo) {
  return resolveArrayVectorAccessTargetInfo(
      target, localsIn, resolveCallArrayVectorAccessTargetInfo, nullptr, nullptr);
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target, const LocalMap &localsIn) {
  return resolveArrayVectorAccessTargetInfo(target, localsIn, {}, nullptr, nullptr);
}

} // namespace primec::ir_lowerer
