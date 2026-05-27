#include "IrLowererCountAccessHelpers.h"
#include "IrLowererCountAccessClassifiers.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/SemanticProduct.h"
#include "primec/SoaPathHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {
using count_access_detail::isDereferencedCollectionCountTarget;
using count_access_detail::isExplicitArrayCountName;
using count_access_detail::isVectorCountTarget;

bool resolvePublishedSemanticStdlibSurfaceMemberName(const SemanticProgram *semanticProgram,
                                                     const Expr &expr,
                                                     StdlibSurfaceId surfaceId,
                                                     std::string &memberNameOut);
bool resolvePublishedStdlibSurfaceMemberName(std::string_view path,
                                             StdlibSurfaceId surfaceId,
                                             std::string &memberNameOut);

namespace {

std::string_view resolveSemanticProductText(const SemanticProgram &semanticProgram,
                                            SymbolId id,
                                            const std::string &fallback) {
  if (id != InvalidSymbolId) {
    const std::string_view resolved = semanticProgramResolveCallTargetString(semanticProgram, id);
    if (!resolved.empty()) {
      return resolved;
    }
  }
  return fallback;
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

bool getArrayVectorAccessClassifierName(const Expr &expr,
                                        std::string &accessNameOut) {
  if (getBuiltinArrayAccessName(expr, accessNameOut)) {
    return true;
  }
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      !expr.namespacePrefix.empty() || expr.name.find('/') != std::string::npos ||
      expr.args.size() != 2) {
    return false;
  }
  if (expr.name != "at" && expr.name != "at_unsafe") {
    return false;
  }
  accessNameOut = expr.name;
  return true;
}

std::string collectionMemberPath(std::string_view collectionName,
                                 std::string_view memberName) {
  return std::string(collectionName) + "/" + std::string(memberName);
}

std::string rootedCollectionMemberPath(std::string_view collectionName,
                                       std::string_view memberName) {
  return "/" + collectionMemberPath(collectionName, memberName);
}

std::string experimentalCollectionRoot(std::string_view collectionName) {
  return "/std/collections/experimental_" + std::string(collectionName);
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return experimentalCollectionRoot(collectionName) + "/" + std::string(typeName);
}

bool isExperimentalCollectionStructPath(const std::string &structTypeName,
                                        std::string_view collectionName,
                                        std::string_view typeName) {
  const std::string basePath = experimentalCollectionTypePath(collectionName, typeName);
  return structTypeName == basePath ||
         structTypeName.rfind(basePath + "__", 0) == 0;
}

std::string experimentalVectorWrapperAlias(std::string_view suffix) {
  return std::string("vector") + std::string(suffix);
}

std::string stdCollectionsRoot() {
  return "/std/collections";
}

std::string canonicalCollectionMemberPrefix(std::string_view collectionName) {
  return stdCollectionsRoot().substr(1) + "/" +
         std::string(collectionName) + "/";
}

bool hasCanonicalCollectionMemberPrefix(std::string_view text,
                                        std::string_view collectionName) {
  const std::string prefixStorage =
      canonicalCollectionMemberPrefix(collectionName);
  const std::string_view prefix(prefixStorage);
  if (text.rfind(prefix, 0) == 0) {
    return true;
  }
  return !text.empty() && text.front() == '/' &&
         text.substr(1).rfind(prefix, 0) == 0;
}

std::string stripGeneratedHelperSuffix(std::string path) {
  const size_t leafStart = path.find_last_of('/');
  const size_t generatedSuffix =
      path.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return path;
}

std::string normalizeUnrootedHelperPath(std::string path) {
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return stripGeneratedHelperSuffix(std::move(path));
}

std::string resolveCallLeafName(const Expr &expr) {
  std::string path = resolveScopedCallPath(expr);
  const size_t leafStart = path.find_last_of('/');
  if (leafStart != std::string::npos) {
    path.erase(0, leafStart + 1);
  }
  const size_t generatedSuffix = path.find("__");
  if (generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return path;
}

bool isInternalSoaStorageMetadataCall(const Expr &expr,
                                      std::string_view fieldName) {
  return expr.kind == Expr::Kind::Call &&
         expr.args.size() == 1 &&
         resolveCallLeafName(expr) == fieldName;
}

bool isExplicitVectorCountMethodCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall) {
    return false;
  }
  const auto *metadata =
      vectorHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  std::string resolvedHelperName;
  return (resolvePublishedStdlibSurfaceExprMemberName(
              expr, metadata->id, resolvedHelperName) ||
          resolvePublishedStdlibSurfaceMemberName(
              resolveScopedCallPath(expr), metadata->id, resolvedHelperName)) &&
         resolvedHelperName == "count";
}

bool isResolvedVectorCountMethodCall(const Expr &expr,
                                     const SemanticProgram *semanticProgram) {
  if (expr.kind != Expr::Kind::Call || !expr.isMethodCall ||
      expr.args.size() != 1) {
    return false;
  }
  if (isExplicitVectorCountMethodCall(expr)) {
    return true;
  }
  const std::string canonicalVectorCountPath =
      stdlibSurfaceCanonicalHelperPath(
          StdlibSurfaceId::CollectionsManifestSurface0, "count");
  return !canonicalVectorCountPath.empty() &&
         findSemanticProductMethodCallTarget(semanticProgram, expr) ==
             canonicalVectorCountPath;
}

bool isExplicitPublishedVectorMetadataCall(const Expr &expr,
                                           std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  std::string resolvedHelperName;
  if (!count_access_detail::resolveVectorHelperAliasName(expr, resolvedHelperName) ||
      resolvedHelperName != helperName) {
    return false;
  }
  const std::string normalizedPath =
      normalizeUnrootedHelperPath(resolveScopedCallPath(expr));
  return normalizedPath.rfind(canonicalCollectionMemberPrefix("vector"), 0) == 0;
}

bool isExplicitPublishedKeyValueMetadataCall(const Expr &expr,
                                             const SemanticProgram *semanticProgram,
                                             std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const auto *metadata =
      keyValueHelperSurfaceMetadata();
  if (metadata == nullptr) {
    return false;
  }
  std::string resolvedHelperName;
  if (resolvePublishedSemanticStdlibSurfaceMemberName(
          semanticProgram, expr, metadata->id, resolvedHelperName) ||
      resolvePublishedStdlibSurfaceMemberName(
          resolveScopedCallPath(expr), metadata->id, resolvedHelperName)) {
    return resolvedHelperName == helperName;
  }
  return false;
}

bool isSemanticVectorCountBridgeCall(const Expr &expr,
                                     const SemanticProgram *semanticProgram) {
  if (semanticProgram == nullptr || expr.kind != Expr::Kind::Call ||
      expr.isMethodCall || expr.semanticNodeId == 0) {
    return false;
  }
  const std::optional<StdlibSurfaceId> surfaceId =
      findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr);
  if (!surfaceId.has_value() ||
      *surfaceId != StdlibSurfaceId::CollectionsManifestSurface0) {
    return false;
  }
  const std::string bridgePath =
      normalizeUnrootedHelperPath(findSemanticProductBridgePathChoice(semanticProgram, expr));
  return bridgePath == canonicalCollectionMemberPrefix("vector") + "count";
}

bool isSemanticArrayCountMethodTarget(const Expr &expr,
                                      const SemanticProgram *semanticProgram) {
  return semanticProgram != nullptr && expr.kind == Expr::Kind::Call &&
         expr.isMethodCall && expr.semanticNodeId != 0 &&
         findSemanticProductMethodCallTarget(semanticProgram, expr) == "/array/count";
}

std::string semanticMethodReceiverTypeText(const SemanticProgram *semanticProgram,
                                           const Expr &expr) {
  if (semanticProgram == nullptr || expr.kind != Expr::Kind::Call ||
      !expr.isMethodCall || expr.semanticNodeId == 0) {
    return {};
  }
  const auto methodCallTargets =
      semanticProgramMethodCallTargetView(*semanticProgram);
  for (const auto *entry : methodCallTargets) {
    if (entry == nullptr || entry->semanticNodeId != expr.semanticNodeId) {
      continue;
    }
    if (entry->receiverTypeTextId != InvalidSymbolId) {
      const std::string_view internedTypeText =
          semanticProgramResolveCallTargetString(*semanticProgram,
                                                 entry->receiverTypeTextId);
      if (!internedTypeText.empty()) {
        return std::string(internedTypeText);
      }
    }
    return entry->receiverTypeText;
  }
  return {};
}

bool isInternalVectorMetadataCall(const Expr &expr,
                                  std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      expr.args.size() != 1) {
    return false;
  }
  std::string scopedPath = resolveScopedCallPath(expr);
  if (!scopedPath.empty() && scopedPath.front() == '/') {
    scopedPath.erase(scopedPath.begin());
  }
  constexpr std::string_view Prefix = "std/collections/internal_vector/";
  if (scopedPath.rfind(Prefix, 0) != 0 ||
      scopedPath.find('/', Prefix.size()) != std::string::npos) {
    return false;
  }
  std::string leaf = scopedPath.substr(Prefix.size());
  const size_t generatedSuffix = leaf.find("__");
  if (generatedSuffix != std::string::npos) {
    leaf.erase(generatedSuffix);
  }
  if (helperName == "count") {
    return leaf == experimentalVectorWrapperAlias("Count");
  }
  if (helperName == "capacity") {
    return leaf == experimentalVectorWrapperAlias("Capacity");
  }
  return false;
}

bool isExplicitRemovedCountLikeAliasCall(const Expr &expr,
                                         std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string scopedPath = resolveScopedCallPath(expr);
  if (helperName == "count") {
    const auto *metadata =
        vectorHelperSurfaceMetadata();
    std::string resolvedHelperName;
    if (metadata != nullptr &&
        (resolvePublishedStdlibSurfaceExprMemberName(
             expr, metadata->id, resolvedHelperName) ||
         resolvePublishedStdlibSurfaceMemberName(
             scopedPath, metadata->id, resolvedHelperName)) &&
        resolvedHelperName == helperName) {
      return false;
    }
  }
  auto matchesCollectionRoot = [&](std::string_view collectionName) {
    return scopedPath == rootedCollectionMemberPath(collectionName, helperName) ||
           scopedPath == collectionMemberPath(collectionName, helperName);
  };
  return matchesCollectionRoot("vector") ||
         matchesCollectionRoot("array") ||
         matchesCollectionRoot(soa_paths::legacySoaFolder());
}

bool isNamedArgumentCollectionTemporary(const Expr &expr,
                                        std::string_view collectionName) {
  if (expr.kind != Expr::Kind::Call || !hasNamedArguments(expr.argNames)) {
    return false;
  }
  std::string collection;
  return getBuiltinCollectionName(expr, collection) && collection == collectionName;
}

bool isCollectionVectorStructValueLocal(const LocalInfo &info) {
  return info.kind == LocalInfo::Kind::Value &&
         isExperimentalCollectionStructPath(info.structTypeName, "vector", "Vector");
}

bool isExperimentalSoaVectorStructLocal(const LocalInfo &info) {
  const std::string backingTypePath =
      soa_paths::collectionPath(soa_paths::experimentalSoaFolder(),
                                soa_paths::soaBackingTypeName());
  return info.structTypeName == backingTypePath ||
         info.structTypeName.rfind(backingTypePath + "__", 0) == 0;
}

std::string normalizedInternalSoaStorageMetadataLeaf(std::string structPath) {
  auto trimTypeText = [](std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
      text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
      text.pop_back();
    }
    return text;
  };
  structPath = trimTypeText(std::move(structPath));
  for (std::string_view wrapper : {"Reference<", "Pointer<"}) {
    if (structPath.rfind(wrapper, 0) == 0 && structPath.size() > wrapper.size() &&
        structPath.back() == '>') {
      structPath =
          trimTypeText(structPath.substr(wrapper.size(),
                                         structPath.size() - wrapper.size() - 1));
      break;
    }
  }
  const size_t templateStart = structPath.find('<');
  if (templateStart != std::string::npos) {
    structPath.erase(templateStart);
  }
  const size_t leafStart = structPath.find_last_of('/');
  const size_t suffixStart =
      structPath.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (suffixStart != std::string::npos) {
    structPath.erase(suffixStart);
  }
  if (!structPath.empty() && structPath.front() == '/') {
    structPath.erase(structPath.begin());
  }
  constexpr std::string_view Prefix = "std/collections/internal_soa_storage/";
  if (structPath.rfind(Prefix, 0) == 0) {
    structPath.erase(0, Prefix.size());
  }
  if (structPath == "SoaColumn" || structPath == "SoaFieldView") {
    return structPath;
  }
  return {};
}

bool isInternalSoaStorageMetadataTarget(const Expr &target,
                                        const LocalMap &localsIn) {
  if (target.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(target.name);
  return it != localsIn.end() &&
         !normalizedInternalSoaStorageMetadataLeaf(it->second.structTypeName).empty();
}

bool emitInternalSoaStorageMetadataBase(const Expr &target,
                                        const LocalMap &localsIn,
                                        const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
                                        const std::function<bool(const Expr &, const LocalMap &)> &emitExpr) {
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() &&
        !normalizedInternalSoaStorageMetadataLeaf(it->second.structTypeName).empty()) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
      return true;
    }
  }
  return emitExpr(target, localsIn);
}

void emitInternalSoaStorageMetadataLoad(
    std::string_view fieldName,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const uint64_t fieldOffset =
      fieldName == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
  emitInstruction(IrOpcode::PushI64, fieldOffset);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
}

bool hasInferredTypedWrappedKeyValue(const LocalInfo &info, LocalInfo::Kind kind) {
  return (kind == LocalInfo::Kind::Reference || kind == LocalInfo::Kind::Pointer) &&
         info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
         info.keyValueValueKind != LocalInfo::ValueKind::Unknown;
}

struct SemanticCountTargetInfo {
  bool isCollection = false;
  bool isVector = false;
  bool isString = false;
};

enum class SemanticStringKeyValueAccessResolution {
  NoFact,
  StringKeyValueAccess,
  NonStringKeyValueAccess,
};

enum class SemanticDereferencedCountTargetResolution {
  NoFact,
  Collection,
  NonCollection,
};

enum class SemanticStringCountTargetResolution {
  NoFact,
  String,
  NonString,
};

bool classifySemanticCountTarget(const Expr &target,
                                 const SemanticProgram *semanticProgram,
                                 const SemanticProductIndex *semanticIndex,
                                 SemanticCountTargetInfo &infoOut);
bool hasPublishedSemanticCountTargetFact(const Expr &target,
                                         const SemanticProductIndex *semanticIndex);

bool classifySemanticCountTargetTypeText(std::string typeText,
                                         SemanticCountTargetInfo &infoOut) {
  infoOut = {};
  typeText = trimTemplateTypeText(std::move(typeText));
  if (typeText.empty()) {
    return false;
  }
  if (isStringTypeName(typeText)) {
    infoOut.isString = true;
    return true;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(typeText, base, argText)) {
    return true;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args) || args.size() != 1) {
      return true;
    }
    return classifySemanticCountTargetTypeText(args.front(), infoOut);
  }
  if (base == "array" || base == "vector" || base == soa_paths::legacySoaFolder() ||
      base == "map" || base == "Buffer") {
    infoOut.isCollection = true;
    infoOut.isVector = base == "vector";
    return true;
  }
  return true;
}

bool classifySemanticStringKeyValueTargetTypeText(std::string typeText,
                                             bool &isStringKeyValueOut) {
  isStringKeyValueOut = false;
  typeText = trimTemplateTypeText(std::move(typeText));
  if (typeText.empty()) {
    return false;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(typeText, base, argText)) {
    return true;
  }
  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args)) {
    return true;
  }
  if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
    return classifySemanticStringKeyValueTargetTypeText(args.front(), isStringKeyValueOut);
  }
  if (base != "map" || args.size() != 2) {
    return true;
  }
  isStringKeyValueOut =
      valueKindFromTypeName(args.back()) == LocalInfo::ValueKind::String;
  return true;
}

bool classifySemanticStringKeyValueTargetTypeText(const SemanticProgram &semanticProgram,
                                             const std::string &typeText,
                                             SymbolId typeTextId,
                                             bool &isStringKeyValueOut) {
  const std::string resolvedTypeText =
      std::string(resolveSemanticProductText(semanticProgram, typeTextId, typeText));
  return classifySemanticStringKeyValueTargetTypeText(resolvedTypeText, isStringKeyValueOut);
}

bool classifySemanticStringKeyValueCollectionTarget(
    const SemanticProgram &semanticProgram,
    const SemanticProgramCollectionSpecialization &collectionFact,
    bool &isStringKeyValueOut) {
  isStringKeyValueOut = false;
  const std::string collectionFamily = normalizeCollectionBindingTypeName(
      std::string(resolveSemanticProductText(semanticProgram,
                                             collectionFact.collectionFamilyId,
                                             collectionFact.collectionFamily)));
  if (collectionFamily != "map") {
    return true;
  }
  const std::string valueType =
      std::string(resolveSemanticProductText(semanticProgram,
                                             collectionFact.valueTypeTextId,
                                             collectionFact.valueTypeText));
  isStringKeyValueOut =
      valueKindFromTypeName(valueType) == LocalInfo::ValueKind::String;
  return true;
}

bool classifySemanticStringKeyValueTarget(const Expr &target,
                                     const SemanticProgram *semanticProgram,
                                     const SemanticProductIndex *semanticIndex,
                                     bool &isStringKeyValueOut) {
  isStringKeyValueOut = false;
  if (semanticProgram == nullptr ||
      semanticIndex == nullptr || target.semanticNodeId == 0) {
    return false;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, target)) {
    return classifySemanticStringKeyValueCollectionTarget(
        *semanticProgram, *collectionFact, isStringKeyValueOut);
  }
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, target)) {
    return classifySemanticStringKeyValueTargetTypeText(*semanticProgram,
                                                  bindingFact->bindingTypeText,
                                                  bindingFact->bindingTypeTextId,
                                                  isStringKeyValueOut);
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, target)) {
    return classifySemanticStringKeyValueTargetTypeText(*semanticProgram,
                                                  localAutoFact->bindingTypeText,
                                                  localAutoFact->bindingTypeTextId,
                                                  isStringKeyValueOut);
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, target)) {
    if (classifySemanticStringKeyValueTargetTypeText(*semanticProgram,
                                               queryFact->queryTypeText,
                                               queryFact->queryTypeTextId,
                                               isStringKeyValueOut)) {
      return true;
    }
    if (classifySemanticStringKeyValueTargetTypeText(*semanticProgram,
                                               queryFact->bindingTypeText,
                                               queryFact->bindingTypeTextId,
                                               isStringKeyValueOut)) {
      return true;
    }
    return classifySemanticStringKeyValueTargetTypeText(*semanticProgram,
                                                  queryFact->receiverBindingTypeText,
                                                  queryFact->receiverBindingTypeTextId,
                                                  isStringKeyValueOut);
  }
  return false;
}

SemanticStringKeyValueAccessResolution classifySemanticStringKeyValueAccess(
    const Expr &accessExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return SemanticStringKeyValueAccessResolution::NoFact;
  }
  if (hasPublishedSemanticCountTargetFact(accessExpr, semanticIndex)) {
    SemanticCountTargetInfo accessInfo;
    if (classifySemanticCountTarget(
            accessExpr, semanticProgram, semanticIndex, accessInfo) &&
        accessInfo.isString) {
      return SemanticStringKeyValueAccessResolution::StringKeyValueAccess;
    }
    return SemanticStringKeyValueAccessResolution::NonStringKeyValueAccess;
  }
  if (accessExpr.args.empty()) {
    return SemanticStringKeyValueAccessResolution::NoFact;
  }
  const Expr &accessTarget = accessExpr.args.front();
  if (!hasPublishedSemanticCountTargetFact(accessTarget, semanticIndex)) {
    return SemanticStringKeyValueAccessResolution::NoFact;
  }
  bool isStringKeyValueTarget = false;
  if (classifySemanticStringKeyValueTarget(
          accessTarget, semanticProgram, semanticIndex, isStringKeyValueTarget) &&
      isStringKeyValueTarget) {
    return SemanticStringKeyValueAccessResolution::StringKeyValueAccess;
  }
  return SemanticStringKeyValueAccessResolution::NonStringKeyValueAccess;
}

bool classifySemanticCountTargetTypeText(const SemanticProgram &semanticProgram,
                                         const std::string &typeText,
                                         SymbolId typeTextId,
                                         SemanticCountTargetInfo &infoOut) {
  const std::string resolvedTypeText =
      std::string(resolveSemanticProductText(semanticProgram, typeTextId, typeText));
  return classifySemanticCountTargetTypeText(resolvedTypeText, infoOut);
}

bool classifySemanticCollectionTarget(const SemanticProgram &semanticProgram,
                                      const SemanticProgramCollectionSpecialization &collectionFact,
                                      SemanticCountTargetInfo &infoOut) {
  infoOut = {};
  const std::string collectionFamily = normalizeCollectionBindingTypeName(
      std::string(resolveSemanticProductText(semanticProgram,
                                             collectionFact.collectionFamilyId,
                                             collectionFact.collectionFamily)));
  if (collectionFamily == "array" ||
      collectionFamily == "vector" ||
      collectionFamily == soa_paths::legacySoaFolder() ||
      collectionFamily == "map" ||
      collectionFamily == "Buffer") {
    infoOut.isCollection = true;
    infoOut.isVector = collectionFamily == "vector";
  }
  return true;
}

bool classifySemanticCountTarget(const Expr &target,
                                 const SemanticProgram *semanticProgram,
                                 const SemanticProductIndex *semanticIndex,
                                 SemanticCountTargetInfo &infoOut) {
  infoOut = {};
  if (semanticProgram == nullptr ||
      semanticIndex == nullptr || target.semanticNodeId == 0) {
    return false;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, target)) {
    return classifySemanticCollectionTarget(*semanticProgram, *collectionFact, infoOut);
  }
  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, target)) {
    if (classifySemanticCountTargetTypeText(*semanticProgram,
                                            bindingFact->bindingTypeText,
                                            bindingFact->bindingTypeTextId,
                                            infoOut)) {
      return true;
    }
    return true;
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, target)) {
    if (classifySemanticCountTargetTypeText(*semanticProgram,
                                            localAutoFact->bindingTypeText,
                                            localAutoFact->bindingTypeTextId,
                                            infoOut)) {
      return true;
    }
    return true;
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, target)) {
    if (classifySemanticCountTargetTypeText(*semanticProgram,
                                            queryFact->queryTypeText,
                                            queryFact->queryTypeTextId,
                                            infoOut) ||
        classifySemanticCountTargetTypeText(*semanticProgram,
                                            queryFact->bindingTypeText,
                                            queryFact->bindingTypeTextId,
                                            infoOut) ||
        classifySemanticCountTargetTypeText(*semanticProgram,
                                            queryFact->receiverBindingTypeText,
                                            queryFact->receiverBindingTypeTextId,
                                            infoOut)) {
      return true;
    }
    return true;
  }
  return true;
}

bool hasPublishedSemanticCountTargetFact(const Expr &target,
                                         const SemanticProductIndex *semanticIndex) {
  if (semanticIndex == nullptr || target.semanticNodeId == 0) {
    return false;
  }
  return findSemanticProductCollectionSpecialization(*semanticIndex, target) != nullptr ||
         findSemanticProductBindingFact(*semanticIndex, target) != nullptr ||
         findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, target) != nullptr ||
         findSemanticProductQueryFactBySemanticId(*semanticIndex, target) != nullptr;
}

SemanticDereferencedCountTargetResolution classifySemanticDereferencedCountTarget(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (!(target.kind == Expr::Kind::Call &&
        isSimpleCallName(target, "dereference") &&
        target.args.size() == 1)) {
    return SemanticDereferencedCountTargetResolution::NoFact;
  }
  const Expr &derefTarget = target.args.front();
  if (semanticProgram == nullptr || semanticIndex == nullptr ||
      derefTarget.semanticNodeId == 0) {
    return SemanticDereferencedCountTargetResolution::NoFact;
  }
  SemanticCountTargetInfo semanticInfo;
  if (!classifySemanticCountTarget(
          derefTarget, semanticProgram, semanticIndex, semanticInfo)) {
    return SemanticDereferencedCountTargetResolution::NoFact;
  }
  return semanticInfo.isCollection
             ? SemanticDereferencedCountTargetResolution::Collection
             : SemanticDereferencedCountTargetResolution::NonCollection;
}

SemanticStringCountTargetResolution classifySemanticStringCountTarget(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  SemanticCountTargetInfo semanticInfo;
  if (!classifySemanticCountTarget(
          target, semanticProgram, semanticIndex, semanticInfo)) {
    return SemanticStringCountTargetResolution::NoFact;
  }
  return semanticInfo.isString
             ? SemanticStringCountTargetResolution::String
             : SemanticStringCountTargetResolution::NonString;
}

const SemanticProgramQueryFact *findSourceMethodAccessQueryFact(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr) {
    return nullptr;
  }
  if (semanticIndex != nullptr) {
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, target);
        queryFact != nullptr) {
      return queryFact;
    }
  }
  std::vector<std::pair<int, int>> sourcePositions;
  if (target.sourceLine != 0 && target.sourceColumn != 0) {
    sourcePositions.emplace_back(target.sourceLine, target.sourceColumn);
  }
  if (!target.args.empty() && target.args.front().sourceLine != 0 &&
      target.args.front().sourceColumn != 0) {
    sourcePositions.emplace_back(target.args.front().sourceLine,
                                 target.args.front().sourceColumn);
  }
  for (const auto &queryFact : semanticProgram->queryFacts) {
    const bool sameSourcePosition =
        std::any_of(sourcePositions.begin(), sourcePositions.end(),
                    [&](const auto &sourcePosition) {
                      return queryFact.sourceLine == sourcePosition.first &&
                             queryFact.sourceColumn == sourcePosition.second;
                    });
    if (!sameSourcePosition) {
      continue;
    }
    const std::string_view callName =
        resolveSemanticProductText(*semanticProgram,
                                   queryFact.callNameId,
                                   queryFact.callName);
    if (callName == target.name ||
        (!target.sourceName.empty() && callName == target.sourceName)) {
      return &queryFact;
    }
  }
  return nullptr;
}

bool isSourceMethodStringKeyValueAccessTarget(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    std::string *accessNameOut = nullptr) {
  std::string accessName;
  if (target.kind != Expr::Kind::Call || target.args.size() != 2 ||
      !getBuiltinArrayAccessName(target, accessName) ||
      (accessName != "at" && accessName != "at_unsafe")) {
    return false;
  }
  const auto *queryFact =
      findSourceMethodAccessQueryFact(target, semanticProgram, semanticIndex);
  if (queryFact == nullptr || queryFact->resolvedPathId == InvalidSymbolId) {
    return false;
  }
  const std::string resolvedPath =
      std::string(semanticProgramResolveCallTargetString(
          *semanticProgram, queryFact->resolvedPathId));
  if (resolvedPath != "/at" && resolvedPath != "/at_unsafe") {
    return false;
  }
  const std::string queryType =
      trimTemplateTypeText(std::string(resolveSemanticProductText(
          *semanticProgram, queryFact->queryTypeTextId,
          queryFact->queryTypeText)));
  const std::string bindingType =
      trimTemplateTypeText(std::string(resolveSemanticProductText(
          *semanticProgram, queryFact->bindingTypeTextId,
          queryFact->bindingTypeText)));
  if (queryType != "string" && queryType != "/string" &&
      bindingType != "string" && bindingType != "/string") {
    return false;
  }
  if (accessNameOut != nullptr) {
    *accessNameOut = std::move(accessName);
  }
  return true;
}

bool publishedKeyValueAccessHelperReturnsString(const SemanticProgram *semanticProgram,
                                           std::string_view accessName) {
  if (semanticProgram == nullptr ||
      (accessName != "at" && accessName != "at_unsafe")) {
    return false;
  }
  const std::string helperPath =
      canonicalKeyValueHelperPath(accessName);
  const auto helperPathId =
      semanticProgramLookupCallTargetStringId(*semanticProgram, helperPath);
  if (!helperPathId.has_value()) {
    return false;
  }
  const auto *returnFact =
      semanticProgramLookupPublishedReturnFactByDefinitionPathId(
          *semanticProgram, *helperPathId);
  if (returnFact == nullptr) {
    return false;
  }
  const std::string structPath = trimTemplateTypeText(std::string(
      resolveSemanticProductText(*semanticProgram, returnFact->structPathId,
                                 returnFact->structPath)));
  const std::string bindingType = trimTemplateTypeText(std::string(
      resolveSemanticProductText(*semanticProgram,
                                 returnFact->bindingTypeTextId,
                                 returnFact->bindingTypeText)));
  return structPath == "/string" || structPath == "string" ||
         bindingType == "/string" || bindingType == "string";
}

bool hasExplicitStdKeyValueSourceSpelling(const Expr &expr) {
  const auto hasStdMapPrefix = [](std::string_view text) {
    return hasCanonicalCollectionMemberPrefix(text, "map");
  };
  return hasStdMapPrefix(expr.name) || hasStdMapPrefix(expr.namespacePrefix);
}

bool resolveEntryArgsParameterFromSemanticProduct(const Definition &entryDef,
                                                  const SemanticProgram *semanticProgram,
                                                  bool &hasEntryArgsOut,
                                                  std::string &entryArgsNameOut,
                                                  std::string &error) {
  hasEntryArgsOut = false;
  entryArgsNameOut.clear();
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProgramBindingFact *entryParamFact = nullptr;
  std::size_t entryParamCount = 0;
  std::vector<std::size_t> publishedBindingFactIndices;
  publishedBindingFactIndices.reserve(
      semanticProgram->publishedRoutingLookups.bindingFactIndicesByExpr.size());
  for (const auto &[semanticNodeId, entryIndex] :
       semanticProgram->publishedRoutingLookups.bindingFactIndicesByExpr) {
    (void)semanticNodeId;
    if (entryIndex < semanticProgram->bindingFacts.size()) {
      publishedBindingFactIndices.push_back(entryIndex);
    }
  }
  std::sort(publishedBindingFactIndices.begin(), publishedBindingFactIndices.end());
  publishedBindingFactIndices.erase(
      std::unique(publishedBindingFactIndices.begin(), publishedBindingFactIndices.end()),
      publishedBindingFactIndices.end());
  for (const std::size_t entryIndex : publishedBindingFactIndices) {
    const auto *entry = &semanticProgram->bindingFacts[entryIndex];
    const std::string_view scopePath =
        resolveSemanticProductText(*semanticProgram, entry->scopePathId, entry->scopePath);
    const std::string_view siteKind =
        resolveSemanticProductText(*semanticProgram, entry->siteKindId, entry->siteKind);
    if (scopePath != entryDef.fullPath || siteKind != "parameter") {
      continue;
    }
    ++entryParamCount;
    if (entryParamFact == nullptr) {
      entryParamFact = entry;
    }
  }

  if (entryParamCount == 0) {
    if (!entryDef.parameters.empty()) {
      error = "missing semantic-product entry parameter fact: " + entryDef.fullPath;
      return false;
    }
    return true;
  }
  if (entryParamCount != 1) {
    error = "native backend only supports a single array<string> entry parameter";
    return false;
  }
  const std::string_view bindingTypeText = resolveSemanticProductText(
      *semanticProgram, entryParamFact->bindingTypeTextId, entryParamFact->bindingTypeText);
  if (bindingTypeText != "array<string>") {
    error = "native backend entry parameter must be array<string>";
    return false;
  }

  hasEntryArgsOut = true;
  entryArgsNameOut = std::string(resolveSemanticProductText(
      *semanticProgram, entryParamFact->nameId, entryParamFact->name));
  return true;
}

} // namespace

bool resolveEntryArgsParameter(const Definition &entryDef,
                               const SemanticProgram *semanticProgram,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  if (semanticProgram != nullptr) {
    return resolveEntryArgsParameterFromSemanticProduct(
        entryDef, semanticProgram, hasEntryArgsOut, entryArgsNameOut, error);
  }

  hasEntryArgsOut = false;
  entryArgsNameOut.clear();
  if (entryDef.parameters.empty()) {
    return true;
  }
  if (entryDef.parameters.size() != 1) {
    error = "native backend only supports a single array<string> entry parameter";
    return false;
  }
  const Expr &param = entryDef.parameters.front();
  if (!isEntryArgsParam(param)) {
    error = "native backend entry parameter must be array<string>";
    return false;
  }
  if (!param.args.empty()) {
    error = "native backend does not allow entry parameter defaults";
    return false;
  }
  hasEntryArgsOut = true;
  entryArgsNameOut = param.name;
  return true;
}

bool resolveEntryArgsParameter(const Definition &entryDef,
                               bool &hasEntryArgsOut,
                               std::string &entryArgsNameOut,
                               std::string &error) {
  return resolveEntryArgsParameter(entryDef, nullptr, hasEntryArgsOut, entryArgsNameOut, error);
}

bool buildEntryCountAccessSetup(const Definition &entryDef,
                                const SemanticProgram *semanticProgram,
                                EntryCountAccessSetup &out,
                                std::string &error) {
  out = {};
  if (!resolveEntryArgsParameter(entryDef, semanticProgram, out.hasEntryArgs, out.entryArgsName, error)) {
    return false;
  }
  out.classifiers =
      makeCountAccessClassifiers(out.hasEntryArgs, out.entryArgsName, semanticProgram);
  return true;
}

bool buildEntryCountAccessSetup(const Definition &entryDef, EntryCountAccessSetup &out, std::string &error) {
  return buildEntryCountAccessSetup(entryDef, nullptr, out, error);
}

CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs, const std::string &entryArgsName) {
  return makeCountAccessClassifiers(hasEntryArgs, entryArgsName, nullptr);
}

CountAccessClassifiers makeCountAccessClassifiers(bool hasEntryArgs,
                                                  const std::string &entryArgsName,
                                                  const SemanticProgram *semanticProgram) {
  CountAccessClassifiers classifiers{};
  classifiers.isEntryArgsName = makeIsEntryArgsName(hasEntryArgs, entryArgsName);
  classifiers.isArrayCountCall = makeIsArrayCountCall(hasEntryArgs, entryArgsName, semanticProgram);
  classifiers.isVectorCapacityCall = makeIsVectorCapacityCall(semanticProgram);
  classifiers.isStringCountCall = makeIsStringCountCall(semanticProgram);
  return classifiers;
}

IsEntryArgsNameFn makeIsEntryArgsName(bool hasEntryArgs, const std::string &entryArgsName) {
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isEntryArgsName(expr, localsIn, hasEntryArgs, entryArgsName);
  };
}

bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram,
                      const SemanticProductIndex *semanticIndex);
bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram,
                       const SemanticProductIndex *semanticIndex);
bool isVectorCapacityCall(const Expr &expr,
                          const LocalMap &localsIn,
                          const SemanticProgram *semanticProgram,
                          const SemanticProductIndex *semanticIndex);

IsArrayCountCallFn makeIsArrayCountCall(bool hasEntryArgs, const std::string &entryArgsName) {
  return makeIsArrayCountCall(hasEntryArgs, entryArgsName, nullptr);
}

IsArrayCountCallFn makeIsArrayCountCall(bool hasEntryArgs,
                                        const std::string &entryArgsName,
                                        const SemanticProgram *semanticProgram) {
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isArrayCountCall(expr,
                            localsIn,
                            hasEntryArgs,
                            entryArgsName,
                            semanticProgram,
                            semanticIndex.get());
  };
}

IsVectorCapacityCallFn makeIsVectorCapacityCall() {
  return makeIsVectorCapacityCall(nullptr);
}

IsVectorCapacityCallFn makeIsVectorCapacityCall(const SemanticProgram *semanticProgram) {
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isVectorCapacityCall(expr, localsIn, semanticProgram, semanticIndex.get());
  };
}

IsStringCountCallFn makeIsStringCountCall() {
  return makeIsStringCountCall(nullptr);
}

IsStringCountCallFn makeIsStringCountCall(const SemanticProgram *semanticProgram) {
  auto semanticIndex = semanticProgram == nullptr
                           ? std::shared_ptr<SemanticProductIndex>{}
                           : std::make_shared<SemanticProductIndex>(
                                 buildSemanticProductIndex(semanticProgram));
  return [=](const Expr &expr, const LocalMap &localsIn) {
    return isStringCountCall(expr, localsIn, semanticProgram, semanticIndex.get());
  };
}

bool isEntryArgsName(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  if (!hasEntryArgs || expr.kind != Expr::Kind::Name || expr.name != entryArgsName) {
    return false;
  }
  return localsIn.count(entryArgsName) == 0;
}

bool isArrayCountCall(const Expr &expr, const LocalMap &localsIn, bool hasEntryArgs, const std::string &entryArgsName) {
  return isArrayCountCall(expr, localsIn, hasEntryArgs, entryArgsName, nullptr, nullptr);
}

bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram) {
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  return isArrayCountCall(expr,
                          localsIn,
                          hasEntryArgs,
                          entryArgsName,
                          semanticProgram,
                          semanticProgram == nullptr ? nullptr : &semanticIndex);
}

bool isArrayCountCall(const Expr &expr,
                      const LocalMap &localsIn,
                      bool hasEntryArgs,
                      const std::string &entryArgsName,
                      const SemanticProgram *semanticProgram,
                      const SemanticProductIndex *semanticIndex) {
  const bool isSemanticVectorCountBridge =
      isSemanticVectorCountBridgeCall(expr, semanticProgram);
  const bool isSemanticArrayCountMethod =
      isSemanticArrayCountMethodTarget(expr, semanticProgram);
  if (!(count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
        isSemanticVectorCountBridge || isSemanticArrayCountMethod) ||
      expr.args.size() != 1) {
    return false;
  }
  if (isExplicitVectorCountMethodCall(expr) && !isSemanticArrayCountMethod) {
    return false;
  }
  if (isExplicitPublishedVectorMetadataCall(expr, "count") &&
      isVectorCountTarget(expr.args.front(), localsIn)) {
    return false;
  }
  if (isExplicitRemovedCountLikeAliasCall(expr, "count")) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (isNamedArgumentCollectionTemporary(target, "vector")) {
    return false;
  }
  SemanticCountTargetInfo semanticTargetInfo;
  const bool hasSemanticTargetInfo =
      semanticProgram != nullptr &&
      semanticIndex != nullptr && target.semanticNodeId != 0 &&
      classifySemanticCountTarget(target, semanticProgram, semanticIndex, semanticTargetInfo);
  const std::string scopedExprPath = resolveScopedCallPath(expr);
  const bool isBareSimpleVectorCountCall =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      expr.namespacePrefix.empty() && isSimpleCallName(expr, "count");
  if (!hasSemanticTargetInfo && isBareSimpleVectorCountCall &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
  if (!hasSemanticTargetInfo && isExplicitArrayCountName(expr) &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
  if (isEntryArgsName(target, localsIn, hasEntryArgs, entryArgsName)) {
    return true;
  }
  if (hasSemanticTargetInfo) {
    return semanticTargetInfo.isCollection;
  }
  const SemanticDereferencedCountTargetResolution dereferencedTargetResolution =
      classifySemanticDereferencedCountTarget(target, semanticProgram, semanticIndex);
  if (dereferencedTargetResolution ==
      SemanticDereferencedCountTargetResolution::Collection) {
    return true;
  }
  if (dereferencedTargetResolution ==
      SemanticDereferencedCountTargetResolution::NonCollection) {
    return false;
  }
  if (isDereferencedCollectionCountTarget(expr, target, localsIn)) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it == localsIn.end()) {
      return false;
    }
    if (it->second.isArgsPack) {
      return true;
    }
    if (it->second.kind == LocalInfo::Kind::Reference) {
      return it->second.referenceToArray || it->second.referenceToVector ||
             it->second.referenceToBuffer ||
             hasInferredTypedWrappedKeyValue(it->second, it->second.kind);
    }
    if (it->second.kind == LocalInfo::Kind::Pointer) {
      return it->second.pointerToArray || it->second.pointerToVector ||
             it->second.pointerToBuffer ||
             hasInferredTypedWrappedKeyValue(it->second, it->second.kind);
    }
    return it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector ||
           it->second.kind == LocalInfo::Kind::Buffer || it->second.isSoaVector ||
           (it->second.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
            it->second.keyValueValueKind != LocalInfo::ValueKind::Unknown);
  }
  if (target.kind == Expr::Kind::Call) {
    std::string keyValueHelperAlias;
    const bool isNamespacedKeyValueAccessCall =
        count_access_detail::resolveKeyValueHelperAliasName(target, keyValueHelperAlias) &&
        (keyValueHelperAlias == "at" || keyValueHelperAlias == "at_unsafe") &&
        (target.name.find('/') != std::string::npos || !target.namespacePrefix.empty());
    if (isNamespacedKeyValueAccessCall) {
      return false;
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.args.front().name);
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          const LocalInfo &info = localIt->second;
          if (info.argsPackElementKind == LocalInfo::Kind::Array ||
              info.argsPackElementKind == LocalInfo::Kind::Vector ||
              info.argsPackElementKind == LocalInfo::Kind::Buffer ||
              (info.argsPackElementKind == LocalInfo::Kind::Value &&
               info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
               info.keyValueValueKind != LocalInfo::ValueKind::Unknown) ||
              (info.argsPackElementKind == LocalInfo::Kind::Reference &&
               (info.referenceToArray || info.referenceToVector || info.referenceToBuffer ||
                hasInferredTypedWrappedKeyValue(info, info.argsPackElementKind))) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToArray) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToVector) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer && info.pointerToBuffer) ||
              (info.argsPackElementKind == LocalInfo::Kind::Pointer &&
               hasInferredTypedWrappedKeyValue(info, info.argsPackElementKind)) ||
              info.isSoaVector) {
            return true;
          }
      }
    }
    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return false;
    }
    if (collection == "array" || collection == "vector" ||
        collection == soa_paths::legacySoaFolder()) {
      return target.templateArgs.size() == 1;
    }
    if (collection == "map") {
      return target.templateArgs.size() == 2;
    }
  }
  return false;
}

bool isVectorCapacityCall(const Expr &expr, const LocalMap &localsIn) {
  return isVectorCapacityCall(expr, localsIn, nullptr, nullptr);
}

bool isVectorCapacityCall(const Expr &expr,
                          const LocalMap &localsIn,
                          const SemanticProgram *semanticProgram,
                          const SemanticProductIndex *semanticIndex) {
  if (!count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity") || expr.args.size() != 1) {
    return false;
  }
  if (isExplicitRemovedCountLikeAliasCall(expr, "capacity")) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (isNamedArgumentCollectionTemporary(target, "vector")) {
    return false;
  }
  const std::string scopedExprPath = resolveScopedCallPath(expr);
  const bool isBareVectorCapacityCall =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      scopedExprPath == "capacity";
  if (isBareVectorCapacityCall &&
      isVectorCountTarget(target, localsIn)) {
    return false;
  }
  auto isSupportedVectorTarget = [&](const LocalInfo &info, bool fromArgsPack) {
    const LocalInfo::Kind kind = fromArgsPack ? info.argsPackElementKind : info.kind;
    if (info.isSoaVector) {
      return false;
    }
    return kind == LocalInfo::Kind::Vector ||
           (kind == LocalInfo::Kind::Reference && info.referenceToVector) ||
           (kind == LocalInfo::Kind::Pointer && info.pointerToVector);
  };
  const auto isSemanticVectorCapacityTarget = [&](const Expr &candidate) {
    SemanticCountTargetInfo semanticInfo;
    if (!classifySemanticCountTarget(candidate,
                                     semanticProgram,
                                     semanticIndex,
                                     semanticInfo)) {
      return false;
    }
    return semanticInfo.isVector;
  };
  const auto hasSemanticCapacityTargetFact = [&](const Expr &candidate) {
    SemanticCountTargetInfo semanticInfo;
    return classifySemanticCountTarget(candidate,
                                       semanticProgram,
                                       semanticIndex,
                                       semanticInfo);
  };

  if (target.kind == Expr::Kind::Name) {
    return false;
  }
  if (target.kind == Expr::Kind::Call) {
    if (hasPublishedSemanticCountTargetFact(target, semanticIndex) &&
        hasSemanticCapacityTargetFact(target)) {
      return isSemanticVectorCapacityTarget(target);
    }
    if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
      const Expr &derefTarget = target.args.front();
      if (derefTarget.semanticNodeId != 0 && hasSemanticCapacityTargetFact(derefTarget)) {
        return isSemanticVectorCapacityTarget(derefTarget);
      }
      if (derefTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(derefTarget.name);
        return it != localsIn.end() && isSupportedVectorTarget(it->second, false);
      }

      std::string accessName;
      if (getArrayVectorAccessClassifierName(derefTarget, accessName) && derefTarget.args.size() == 2 &&
          derefTarget.args.front().kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(derefTarget.args.front().name);
        return localIt != localsIn.end() && localIt->second.isArgsPack &&
               isSupportedVectorTarget(localIt->second, true);
      }
      return false;
    }

    std::string accessName;
    if (getArrayVectorAccessClassifierName(target, accessName) && target.args.size() == 2 &&
        target.args.front().kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(target.args.front().name);
      return localIt != localsIn.end() && localIt->second.isArgsPack &&
             isSupportedVectorTarget(localIt->second, true);
    }

    std::string collection;
    if (!getBuiltinCollectionName(target, collection)) {
      return isVectorCountTarget(target, localsIn);
    }
    return collection == "vector" && target.templateArgs.size() == 1;
  }
  return false;
}

bool isStringCountCall(const Expr &expr, const LocalMap &localsIn) {
  return isStringCountCall(expr, localsIn, nullptr, nullptr);
}

bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram) {
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  return isStringCountCall(expr,
                           localsIn,
                           semanticProgram,
                           semanticProgram == nullptr ? nullptr : &semanticIndex);
}

bool isStringCountCall(const Expr &expr,
                       const LocalMap &localsIn,
                       const SemanticProgram *semanticProgram,
                       const SemanticProductIndex *semanticIndex) {
  if (!count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
      expr.args.size() != 1) {
    return false;
  }
  if (isExplicitVectorCountMethodCall(expr)) {
    return false;
  }
  if (isExplicitRemovedCountLikeAliasCall(expr, "count")) {
    return false;
  }
  const Expr &target = expr.args.front();
  if (target.kind == Expr::Kind::StringLiteral) {
    return true;
  }
  if (target.kind == Expr::Kind::Name) {
    if (semanticProgram != nullptr && semanticIndex != nullptr &&
        target.semanticNodeId != 0) {
      SemanticCountTargetInfo semanticInfo;
      if (classifySemanticCountTarget(target, semanticProgram, semanticIndex, semanticInfo)) {
        return semanticInfo.isString;
      }
    }
    auto it = localsIn.find(target.name);
    return it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String &&
           it->second.stringSource == LocalInfo::StringSource::TableIndex;
  }
  return false;
}

StringCountCallEmitResult tryEmitStringCountCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    std::string &error) {
  if (!isStringCountCallFn(expr, localsIn)) {
    return StringCountCallEmitResult::NotHandled;
  }
  if (expr.args.size() != 1) {
    error = "count requires exactly one argument";
    return StringCountCallEmitResult::Error;
  }
  const Expr &target = expr.args.front();
  if (inferExprKind) {
    const LocalInfo::ValueKind targetKind = inferExprKind(target, localsIn);
    if (targetKind != LocalInfo::ValueKind::Unknown &&
        targetKind != LocalInfo::ValueKind::String) {
      return StringCountCallEmitResult::NotHandled;
    }
  }
  int32_t stringIndex = -1;
  size_t length = 0;
  if (!resolveStringTableTarget(target, localsIn, stringIndex, length)) {
    error = "native backend only supports count() on string literals or string bindings";
    return StringCountCallEmitResult::Error;
  }
  if (length > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    error = "native backend string too large for count()";
    return StringCountCallEmitResult::Error;
  }
  emitPushI32(static_cast<int32_t>(length));
  return StringCountCallEmitResult::Emitted;
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicCollectionCountTargetFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCountTargetFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isDynamicVectorCapacityTargetFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  const bool explicitPublishedVectorCountCall =
      isExplicitPublishedVectorMetadataCall(expr, "count");
  const bool explicitPublishedKeyValueCountCall =
      isExplicitPublishedKeyValueMetadataCall(expr, semanticProgram, "count");
  const bool semanticVectorCountBridge =
      isSemanticVectorCountBridgeCall(expr, semanticProgram);
  const bool semanticArrayCountMethod =
      isSemanticArrayCountMethodTarget(expr, semanticProgram);
  const bool explicitPublishedVectorCapacityCall =
      isExplicitPublishedVectorMetadataCall(expr, "capacity");
  const auto resolvedVectorCountMethodHasBuiltinTarget = [&]() {
    if (!isResolvedVectorCountMethodCall(expr, semanticProgram) ||
        expr.args.size() != 1) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (isDynamicVectorCountTargetFn != nullptr &&
        isDynamicVectorCountTargetFn(target, localsIn)) {
      return true;
    }
    if (isVectorCountTarget(target, localsIn)) {
      return true;
    }
    SemanticCountTargetInfo semanticTargetInfo;
    if (semanticProgram != nullptr && semanticIndex != nullptr &&
        target.semanticNodeId != 0 &&
        classifySemanticCountTarget(target,
                                    semanticProgram,
                                    semanticIndex,
                                    semanticTargetInfo) &&
        semanticTargetInfo.isVector) {
      return true;
    }
    const std::string receiverTypeText =
        semanticMethodReceiverTypeText(semanticProgram, expr);
    return !receiverTypeText.empty() &&
           classifySemanticCountTargetTypeText(receiverTypeText,
                                               semanticTargetInfo) &&
           semanticTargetInfo.isVector;
  };
  const bool resolvedVectorCountMethodBuiltinTarget =
      resolvedVectorCountMethodHasBuiltinTarget();
  const auto isCountLikeCall = [&]() {
    return (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
            explicitPublishedVectorCountCall ||
            explicitPublishedKeyValueCountCall ||
            semanticArrayCountMethod ||
            resolvedVectorCountMethodBuiltinTarget) &&
           expr.args.size() == 1;
  };
  const auto isBareSimpleCountLikeCall = [&](std::string_view helperName) {
    return expr.kind == Expr::Kind::Call &&
           !expr.isMethodCall &&
           expr.namespacePrefix.empty() &&
           expr.name == std::string(helperName) &&
           expr.args.size() == 1;
  };
  const auto shouldDeferBareSimpleCountLikeCall = [&](std::string_view helperName) {
    if (!isBareSimpleCountLikeCall(helperName)) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (helperName == "count") {
      const bool isRuntimeStringTarget =
          inferExprKind &&
          inferExprKind(target, localsIn) == LocalInfo::ValueKind::String;
      if (isArrayCountCallFn(expr, localsIn) ||
          isRuntimeStringTarget ||
          (isStringCountCallFn != nullptr &&
           isStringCountCallFn(expr, localsIn)) ||
          (isDynamicCollectionCountTargetFn != nullptr &&
           isDynamicCollectionCountTargetFn(target, localsIn)) ||
          (isDynamicVectorCountTargetFn != nullptr &&
           isDynamicVectorCountTargetFn(target, localsIn))) {
        return false;
      }
      std::string accessName;
      if (target.kind == Expr::Kind::Call &&
          target.args.size() == 2 &&
          getBuiltinArrayAccessName(target, accessName) &&
          (accessName == "at" || accessName == "at_unsafe")) {
        return false;
      }
      return true;
    }
    if ((isVectorCapacityCallFn != nullptr &&
         isVectorCapacityCallFn(expr, localsIn)) ||
        (isDynamicVectorCapacityTargetFn != nullptr &&
         isDynamicVectorCapacityTargetFn(target, localsIn))) {
      return false;
    }
    return true;
  };
  if (shouldDeferBareSimpleCountLikeCall("count") ||
      shouldDeferBareSimpleCountLikeCall("capacity")) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if (isResolvedVectorCountMethodCall(expr, semanticProgram) &&
      !semanticArrayCountMethod &&
      !resolvedVectorCountMethodBuiltinTarget) {
    return CountAccessCallEmitResult::NotHandled;
  }
  const bool namedArgVectorTemporaryCountCall =
      explicitPublishedVectorCountCall &&
      expr.args.size() == 1 &&
      isNamedArgumentCollectionTemporary(expr.args.front(), "vector");
  if (explicitPublishedVectorCountCall &&
      expr.args.size() == 1 &&
      isEntryArgsNameFn &&
      isEntryArgsNameFn(expr.args.front(), localsIn)) {
    emitInstruction(IrOpcode::PushArgc, 0);
    return CountAccessCallEmitResult::Emitted;
  }
  const bool semanticBridgeCountAccessTarget =
      semanticVectorCountBridge &&
      expr.args.size() == 1 &&
      (isArrayCountCallFn(expr, localsIn) ||
       (isDynamicCollectionCountTargetFn != nullptr &&
        isDynamicCollectionCountTargetFn(expr.args.front(), localsIn)) ||
       (isDynamicVectorCountTargetFn != nullptr &&
        isDynamicVectorCountTargetFn(expr.args.front(), localsIn)));
  const bool explicitPublishedKeyValueCountAccessTarget =
      explicitPublishedKeyValueCountCall &&
      expr.args.size() == 1 &&
      isDynamicCollectionCountTargetFn != nullptr &&
      isDynamicCollectionCountTargetFn(expr.args.front(), localsIn);
  if (explicitPublishedVectorCountCall &&
      !namedArgVectorTemporaryCountCall &&
      !semanticBridgeCountAccessTarget) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if (explicitPublishedVectorCapacityCall) {
    return CountAccessCallEmitResult::NotHandled;
  }
  if ((isExplicitRemovedCountLikeAliasCall(expr, "count") &&
       !explicitPublishedKeyValueCountAccessTarget) ||
      isExplicitRemovedCountLikeAliasCall(expr, "capacity")) {
    return CountAccessCallEmitResult::NotHandled;
  }
  const auto emitDynamicVectorHeaderBase = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && isCollectionVectorStructValueLocal(it->second)) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        return true;
      }
    }
    return emitExpr(target, localsIn);
  };
  const auto emitDynamicVectorCount = [&](const Expr &target) {
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() &&
          !it->second.isArgsPack &&
          (isExperimentalSoaVectorStructLocal(it->second) ||
           (it->second.isSoaVector && !it->second.usesBuiltinCollectionLayout))) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::PushI64, 2ull * IrSlotBytes);
        emitInstruction(IrOpcode::AddI64, 0);
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return true;
      }
    }
    if (!emitDynamicVectorHeaderBase(target)) {
      return false;
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return true;
  };
  const auto emitDynamicVectorCapacity = [&](const Expr &target) {
    if (!emitDynamicVectorHeaderBase(target)) {
      return false;
    }
    emitInstruction(IrOpcode::PushI64, IrSlotBytes);
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return true;
  };
  if (isInternalVectorMetadataCall(expr, "count") &&
      isDynamicVectorCountTargetFn != nullptr &&
      isDynamicVectorCountTargetFn(expr.args.front(), localsIn)) {
    if (!emitDynamicVectorCount(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }
  if (isInternalVectorMetadataCall(expr, "capacity") &&
      isDynamicVectorCapacityTargetFn != nullptr &&
      isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn)) {
    if (!emitDynamicVectorCapacity(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }
  if (isInternalSoaStorageMetadataCall(expr, "field_count") &&
      isInternalSoaStorageMetadataTarget(expr.args.front(), localsIn)) {
    if (!emitInternalSoaStorageMetadataBase(
            expr.args.front(), localsIn, emitInstruction, emitExpr)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInternalSoaStorageMetadataLoad("field_count", emitInstruction);
    return CountAccessCallEmitResult::Emitted;
  }
  if (isInternalSoaStorageMetadataCall(expr, "field_capacity") &&
      isInternalSoaStorageMetadataTarget(expr.args.front(), localsIn)) {
    if (!emitInternalSoaStorageMetadataBase(
            expr.args.front(), localsIn, emitInstruction, emitExpr)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInternalSoaStorageMetadataLoad("field_capacity", emitInstruction);
    return CountAccessCallEmitResult::Emitted;
  }
  const bool namedArgVectorTemporaryCountTarget =
      (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedVectorCountCall ||
       explicitPublishedKeyValueCountCall) &&
      expr.args.size() == 1 &&
      isNamedArgumentCollectionTemporary(expr.args.front(), "vector");
  if (namedArgVectorTemporaryCountTarget) {
    error = "count requires array, vector, map, or string target";
    return CountAccessCallEmitResult::Error;
  }
  const bool isFieldAccessTarget =
      expr.kind == Expr::Kind::Call && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call && expr.args.front().isFieldAccess;
  const auto isStaticallyKnownStringCountTarget = [&]() {
    if (!isCountLikeCall()) {
      return false;
    }
    const Expr &target = expr.args.front();
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(target.name);
    return it != localsIn.end() &&
           it->second.valueKind == LocalInfo::ValueKind::String &&
           it->second.stringSource == LocalInfo::StringSource::TableIndex;
  };
  const auto isRuntimeStringCountTarget = [&]() {
    if (!isCountLikeCall() || isStaticallyKnownStringCountTarget()) {
      return false;
    }
    const std::string receiverTypeText =
        semanticMethodReceiverTypeText(semanticProgram, expr);
    if (!receiverTypeText.empty()) {
      SemanticCountTargetInfo receiverInfo;
      if (classifySemanticCountTargetTypeText(receiverTypeText,
                                              receiverInfo)) {
        if (receiverInfo.isString) {
          return true;
        }
        if (receiverInfo.isCollection) {
          return false;
        }
      }
    }
    const Expr &target = expr.args.front();
    const SemanticStringCountTargetResolution semanticStringTarget =
        classifySemanticStringCountTarget(target, semanticProgram, semanticIndex);
    if (semanticStringTarget == SemanticStringCountTargetResolution::String) {
      return true;
    }
    if (semanticStringTarget == SemanticStringCountTargetResolution::NonString) {
      return false;
    }
    if (isSourceMethodStringKeyValueAccessTarget(target, semanticProgram, semanticIndex)) {
      return true;
    }
    if (inferExprKind) {
      const LocalInfo::ValueKind targetKind = inferExprKind(target, localsIn);
      if (targetKind == LocalInfo::ValueKind::String) {
        return true;
      }
      if (targetKind != LocalInfo::ValueKind::Unknown) {
        return false;
      }
    }
    if (target.kind == Expr::Kind::Name) {
      auto it = localsIn.find(target.name);
      if (it != localsIn.end() && it->second.valueKind == LocalInfo::ValueKind::String) {
        return true;
      }
    }
    return false;
  };
  const auto emitRuntimeStringCountTarget = [&]() {
    Expr rewrittenStringTarget = expr.args.front();
    std::string explicitKeyValueAccessName;
    if (hasExplicitStdKeyValueSourceSpelling(rewrittenStringTarget) &&
        getBuiltinArrayAccessName(rewrittenStringTarget, explicitKeyValueAccessName) &&
        (explicitKeyValueAccessName == "at" ||
         explicitKeyValueAccessName == "at_unsafe")) {
      rewrittenStringTarget.name = explicitKeyValueAccessName;
      rewrittenStringTarget.namespacePrefix.clear();
      rewrittenStringTarget.isMethodCall = false;
      rewrittenStringTarget.isFieldAccess = false;
      rewrittenStringTarget.semanticNodeId = 0;
      rewrittenStringTarget.templateArgs.clear();
    } else {
      std::string sourceMethodAccessName;
      if (isSourceMethodStringKeyValueAccessTarget(
              rewrittenStringTarget, semanticProgram, semanticIndex,
              &sourceMethodAccessName)) {
        rewrittenStringTarget.name = sourceMethodAccessName;
        rewrittenStringTarget.namespacePrefix.clear();
        rewrittenStringTarget.isMethodCall = true;
        rewrittenStringTarget.isFieldAccess = false;
        rewrittenStringTarget.semanticNodeId = 0;
        rewrittenStringTarget.templateArgs.clear();
      }
    }
    if (!emitExpr(rewrittenStringTarget, localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadStringLength, 0);
    return CountAccessCallEmitResult::Emitted;
  };
  if (isCountLikeCall() && expr.args.front().kind == Expr::Kind::Call) {
    const Expr &target = expr.args.front();
    std::string accessName;
    if (target.name.find('/') == std::string::npos &&
        !hasExplicitStdKeyValueSourceSpelling(target) &&
        getBuiltinArrayAccessName(target, accessName) &&
        (isSourceMethodStringKeyValueAccessTarget(target, semanticProgram, semanticIndex) ||
         (publishedKeyValueAccessHelperReturnsString(semanticProgram, accessName) &&
          !target.args.empty() &&
          !isVectorCountTarget(target.args.front(), localsIn)))) {
      return emitRuntimeStringCountTarget();
    }
  }
  if (isArrayCountCallFn(expr, localsIn)) {
    if (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") &&
        expr.args.size() == 1 &&
        expr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(expr.args.front().name);
      if (it != localsIn.end() &&
          !it->second.isArgsPack &&
          (isExperimentalSoaVectorStructLocal(it->second) ||
           (it->second.isSoaVector && !it->second.usesBuiltinCollectionLayout))) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::PushI64, 2ull * IrSlotBytes);
        emitInstruction(IrOpcode::AddI64, 0);
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return CountAccessCallEmitResult::Emitted;
      }
      if (it != localsIn.end() && it->second.isArgsPack) {
        emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(it->second.index));
        emitInstruction(IrOpcode::LoadIndirect, 0);
        return CountAccessCallEmitResult::Emitted;
      }
    }
    if (isEntryArgsNameFn(expr.args.front(), localsIn)) {
      emitInstruction(IrOpcode::PushArgc, 0);
      return CountAccessCallEmitResult::Emitted;
    }
    if (!emitExpr(expr.args.front(), localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadIndirect, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  const bool blocksBareVectorCountCall =
      isBareSimpleCountLikeCall("count") && expr.args.front().kind != Expr::Kind::Call &&
      (isDynamicVectorCountTargetFn && isDynamicVectorCountTargetFn(expr.args.front(), localsIn) &&
       !isVectorCountTarget(expr.args.front(), localsIn) &&
       !isFieldAccessTarget);
  const bool blocksLocalVectorCountCall =
      isBareSimpleCountLikeCall("count") &&
      isVectorCountTarget(expr.args.front(), localsIn) &&
      !(isDynamicCollectionCountTargetFn &&
        isDynamicCollectionCountTargetFn(expr.args.front(), localsIn));
  const bool blocksBareVectorCapacityCall =
      isBareSimpleCountLikeCall("capacity") &&
      expr.args.front().kind != Expr::Kind::Call &&
      (isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn) &&
       !(isVectorCapacityCallFn && isVectorCapacityCallFn(expr, localsIn)) &&
       !isFieldAccessTarget);
  const bool blocksLocalVectorCapacityCall =
      isBareSimpleCountLikeCall("capacity") &&
      isVectorCountTarget(expr.args.front(), localsIn) &&
      !(isDynamicVectorCapacityTargetFn &&
        isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn));
  if (blocksBareVectorCountCall || blocksLocalVectorCountCall ||
      blocksBareVectorCapacityCall || blocksLocalVectorCapacityCall) {
    return CountAccessCallEmitResult::NotHandled;
  }

  if (isVectorCapacityCallFn(expr, localsIn)) {
    if (!emitDynamicVectorCapacity(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }

  if (isCountLikeCall()) {
    const Expr &target = expr.args.front();
    std::string vectorAccessName;
    if (target.kind == Expr::Kind::Call && target.sourceIsMethodCall &&
        target.args.size() == 2 &&
        getBuiltinArrayAccessName(target, vectorAccessName) &&
        (vectorAccessName == "at" || vectorAccessName == "at_unsafe")) {
      const SemanticStringCountTargetResolution semanticStringTarget =
          classifySemanticStringCountTarget(target, semanticProgram,
                                            semanticIndex);
      if (semanticStringTarget == SemanticStringCountTargetResolution::NonString) {
        error = "native backend only supports entry argument indexing";
        return CountAccessCallEmitResult::Error;
      }
      if (semanticStringTarget == SemanticStringCountTargetResolution::String &&
          vectorAccessName == "at_unsafe") {
        return CountAccessCallEmitResult::NotHandled;
      }
    }
  }

  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedVectorCountCall ||
       explicitPublishedKeyValueCountCall ||
       resolvedVectorCountMethodBuiltinTarget) && expr.args.size() == 1 &&
      isDynamicCollectionCountTargetFn && isDynamicCollectionCountTargetFn(expr.args.front(), localsIn)) {
    if (inferExprKind &&
        inferExprKind(expr.args.front(), localsIn) == LocalInfo::ValueKind::String) {
      // String receivers can be dynamic call results; defer to string count emission.
    } else {
      if (!emitDynamicVectorCount(expr.args.front())) {
        return CountAccessCallEmitResult::Error;
      }
      return CountAccessCallEmitResult::Emitted;
    }
  }

  if ((expr.namespacePrefix.empty() || explicitPublishedVectorCapacityCall) &&
      (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity") || explicitPublishedVectorCapacityCall) &&
      expr.args.size() == 1 &&
      isDynamicVectorCapacityTargetFn && isDynamicVectorCapacityTargetFn(expr.args.front(), localsIn)) {
    if (!emitDynamicVectorCapacity(expr.args.front())) {
      return CountAccessCallEmitResult::Error;
    }
    return CountAccessCallEmitResult::Emitted;
  }

  if (isRuntimeStringCountTarget()) {
    return emitRuntimeStringCountTarget();
  }

  const auto stringCountResult = tryEmitStringCountCall(
      expr,
      localsIn,
      isStringCountCallFn,
      inferExprKind,
      resolveStringTableTarget,
      [&](int32_t length) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(length)); },
      error);
  if (stringCountResult == StringCountCallEmitResult::Error) {
    return CountAccessCallEmitResult::Error;
  }
  if (stringCountResult == StringCountCallEmitResult::Emitted) {
    return CountAccessCallEmitResult::Emitted;
  }

  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedKeyValueCountCall) && expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    const Expr &target = expr.args.front();
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const auto semanticStringKeyValueAccess =
          classifySemanticStringKeyValueAccess(target, semanticProgram, semanticIndex);
      if (semanticStringKeyValueAccess ==
          SemanticStringKeyValueAccessResolution::StringKeyValueAccess) {
        if (!emitExpr(target, localsIn)) {
          return CountAccessCallEmitResult::Error;
        }
        emitInstruction(IrOpcode::LoadStringLength, 0);
        return CountAccessCallEmitResult::Emitted;
      }
      if (semanticStringKeyValueAccess ==
          SemanticStringKeyValueAccessResolution::NonStringKeyValueAccess) {
        return CountAccessCallEmitResult::NotHandled;
      }
      if (inferExprKind) {
        const LocalInfo::ValueKind targetKind = inferExprKind(target, localsIn);
        if (targetKind == LocalInfo::ValueKind::String) {
          if (!emitExpr(target, localsIn)) {
            return CountAccessCallEmitResult::Error;
          }
          emitInstruction(IrOpcode::LoadStringLength, 0);
          return CountAccessCallEmitResult::Emitted;
        }
        if (targetKind != LocalInfo::ValueKind::Unknown) {
          return CountAccessCallEmitResult::NotHandled;
        }
      }
      const Expr &accessTarget = target.args.front();
      bool stringKeyValueAccess = false;
      if (accessTarget.kind == Expr::Kind::Name) {
        auto it = localsIn.find(accessTarget.name);
        if (it != localsIn.end()) {
          const LocalInfo &info = it->second;
          stringKeyValueAccess =
              info.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
              info.keyValueValueKind == LocalInfo::ValueKind::String;
        }
      } else if (accessTarget.kind == Expr::Kind::Call) {
        std::string collection;
        if (getBuiltinCollectionName(accessTarget, collection) && collection == "map" &&
            accessTarget.templateArgs.size() == 2) {
          stringKeyValueAccess =
              accessTarget.templateArgs[1] == "string" || accessTarget.templateArgs[1] == "/string";
        }
      }
      if (stringKeyValueAccess) {
        if (!emitExpr(target, localsIn)) {
          return CountAccessCallEmitResult::Error;
        }
        emitInstruction(IrOpcode::LoadStringLength, 0);
        return CountAccessCallEmitResult::Emitted;
      }
    }
  }

  if ((count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
       explicitPublishedKeyValueCountCall) && expr.args.size() == 1 &&
      inferExprKind) {
    const SemanticStringCountTargetResolution semanticStringTarget =
        classifySemanticStringCountTarget(expr.args.front(), semanticProgram, semanticIndex);
    if (semanticStringTarget == SemanticStringCountTargetResolution::NonString) {
      return CountAccessCallEmitResult::NotHandled;
    }
    if (semanticStringTarget != SemanticStringCountTargetResolution::String &&
        inferExprKind(expr.args.front(), localsIn) != LocalInfo::ValueKind::String) {
      return CountAccessCallEmitResult::NotHandled;
    }
    Expr rewrittenStringTarget = expr.args.front();
    std::string accessName;
    if (isSourceMethodStringKeyValueAccessTarget(
            rewrittenStringTarget, semanticProgram, semanticIndex, &accessName)) {
      rewrittenStringTarget.isMethodCall = false;
      rewrittenStringTarget.isFieldAccess = false;
      rewrittenStringTarget.namespacePrefix.clear();
      rewrittenStringTarget.name =
          canonicalKeyValueHelperPath(accessName);
    }
    if (!emitExpr(rewrittenStringTarget, localsIn)) {
      return CountAccessCallEmitResult::Error;
    }
    emitInstruction(IrOpcode::LoadStringLength, 0);
    return CountAccessCallEmitResult::Emitted;
  }

  return CountAccessCallEmitResult::NotHandled;
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  return tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCallFn,
      isVectorCapacityCallFn,
      isStringCountCallFn,
      isEntryArgsNameFn,
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return false; },
      [](const Expr &, const LocalMap &) { return LocalInfo::ValueKind::Unknown; },
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

CountAccessCallEmitResult tryEmitCountAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsNameFn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  return tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCallFn,
      isVectorCapacityCallFn,
      isStringCountCallFn,
      isEntryArgsNameFn,
      {},
      {},
      {},
      inferExprKind,
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
}

} // namespace primec::ir_lowerer
