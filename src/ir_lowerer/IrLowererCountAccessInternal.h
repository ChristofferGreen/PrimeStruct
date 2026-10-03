#pragma once

// Helpers shared by the IrLowererCountAccessHelpers*.cpp units (split out of
// IrLowererCountAccessHelpers.cpp without changes, ticket).
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
#include "primec/frontend/SemanticProduct.h"
#include "primec/ir/SoaPathHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

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

namespace count_access_internal {

inline std::string_view resolveSemanticProductText(const SemanticProgram &semanticProgram,
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

inline std::string resolveScopedCallPath(const Expr &expr) {
  if (expr.name.find('/') != std::string::npos || expr.namespacePrefix.empty()) {
    return expr.name;
  }
  if (expr.namespacePrefix == "/") {
    return "/" + expr.name;
  }
  return expr.namespacePrefix + "/" + expr.name;
}

inline bool getArrayVectorAccessClassifierName(const Expr &expr,
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

inline std::string collectionMemberPath(std::string_view collectionName,
                                 std::string_view memberName) {
  return std::string(collectionName) + "/" + std::string(memberName);
}

inline std::string rootedCollectionMemberPath(std::string_view collectionName,
                                       std::string_view memberName) {
  return "/" + collectionMemberPath(collectionName, memberName);
}

inline std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return collection_paths::memberPath(
      collection_paths::typeIdentityFolder(collectionName), typeName);
}

inline bool isExperimentalCollectionStructPath(const std::string &structTypeName,
                                        std::string_view collectionName,
                                        std::string_view typeName) {
  const std::string basePath = experimentalCollectionTypePath(collectionName, typeName);
  return structTypeName == basePath ||
         structTypeName.rfind(basePath + "__", 0) == 0;
}

inline std::string experimentalVectorWrapperAlias(std::string_view suffix) {
  return std::string("vector") + std::string(suffix);
}

inline std::string stdCollectionsRoot() {
  return "/std/collections";
}

inline std::string canonicalCollectionMemberPrefix(std::string_view collectionName) {
  return stdCollectionsRoot().substr(1) + "/" +
         std::string(collectionName) + "/";
}

inline bool hasCanonicalCollectionMemberPrefix(std::string_view text,
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

inline std::string stripGeneratedHelperSuffix(std::string path) {
  const size_t leafStart = path.find_last_of('/');
  const size_t generatedSuffix =
      path.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
  if (generatedSuffix != std::string::npos) {
    path.erase(generatedSuffix);
  }
  return path;
}

inline std::string normalizeUnrootedHelperPath(std::string path) {
  if (!path.empty() && path.front() == '/') {
    path.erase(path.begin());
  }
  return stripGeneratedHelperSuffix(std::move(path));
}

inline std::string resolveCallLeafName(const Expr &expr) {
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

inline bool isInternalSoaStorageMetadataCall(const Expr &expr,
                                      std::string_view fieldName) {
  return expr.kind == Expr::Kind::Call &&
         expr.args.size() == 1 &&
         resolveCallLeafName(expr) == fieldName;
}

inline bool isExplicitVectorCountMethodCall(const Expr &expr) {
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

inline bool isResolvedVectorCountMethodCall(const Expr &expr,
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

inline bool isExplicitPublishedVectorMetadataCall(const Expr &expr,
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

inline bool isExplicitPublishedKeyValueMetadataCall(const Expr &expr,
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

inline bool isSemanticVectorCountBridgeCall(const Expr &expr,
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

inline bool isSemanticArrayCountMethodTarget(const Expr &expr,
                                      const SemanticProgram *semanticProgram) {
  return semanticProgram != nullptr && expr.kind == Expr::Kind::Call &&
         expr.isMethodCall && expr.semanticNodeId != 0 &&
         findSemanticProductMethodCallTarget(semanticProgram, expr) == collection_helpers::kRootedArrayCount;
}

inline std::string semanticMethodReceiverTypeText(const SemanticProgram *semanticProgram,
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

inline bool isInternalVectorMetadataCall(const Expr &expr,
                                  std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      expr.args.size() != 1) {
    return false;
  }
  std::string scopedPath = resolveScopedCallPath(expr);
  if (!scopedPath.empty() && scopedPath.front() == '/') {
    scopedPath.erase(scopedPath.begin());
  }
  const std::string Prefix = collection_paths::modulePrefixBare(collection_paths::kVectorFolder);
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

inline bool isExplicitRemovedCountLikeAliasCall(const Expr &expr,
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

inline bool isNamedArgumentCollectionTemporary(const Expr &expr,
                                        std::string_view collectionName) {
  if (expr.kind != Expr::Kind::Call || !hasNamedArguments(expr.argNames)) {
    return false;
  }
  std::string collection;
  return getBuiltinCollectionName(expr, collection) && collection == collectionName;
}

inline bool isCollectionVectorStructValueLocal(const LocalInfo &info) {
  return info.kind == LocalInfo::Kind::Value &&
         isExperimentalCollectionStructPath(info.structTypeName, "vector", "Vector");
}

inline bool isExperimentalSoaVectorStructLocal(const LocalInfo &info) {
  const std::string backingTypePath =
      soa_paths::collectionPath(soa_paths::experimentalSoaFolder(),
                                soa_paths::soaBackingTypeName());
  return info.structTypeName == backingTypePath ||
         info.structTypeName.rfind(backingTypePath + "__", 0) == 0;
}

inline std::string normalizedInternalSoaStorageMetadataLeaf(std::string structPath) {
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
  const std::string Prefix = collection_paths::modulePrefixBare(collection_paths::kInternalSoaStorageFolder);
  if (structPath.rfind(Prefix, 0) == 0) {
    structPath.erase(0, Prefix.size());
  }
  if (structPath == "SoaColumn" || structPath == "SoaFieldView") {
    return structPath;
  }
  return {};
}

inline bool isInternalSoaStorageMetadataTarget(const Expr &target,
                                        const LocalMap &localsIn) {
  if (target.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(target.name);
  return it != localsIn.end() &&
         !normalizedInternalSoaStorageMetadataLeaf(it->second.structTypeName).empty();
}

inline bool emitInternalSoaStorageMetadataBase(const Expr &target,
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

inline void emitInternalSoaStorageMetadataLoad(
    std::string_view fieldName,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  const uint64_t fieldOffset =
      fieldName == "field_capacity" ? IrSlotBytes * 2 : IrSlotBytes;
  emitInstruction(IrOpcode::PushI64, fieldOffset);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
}

inline bool hasInferredTypedWrappedKeyValue(const LocalInfo &info, LocalInfo::Kind kind) {
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

inline bool classifySemanticCountTarget(const Expr &target,
                                 const SemanticProgram *semanticProgram,
                                 const SemanticProductIndex *semanticIndex,
                                 SemanticCountTargetInfo &infoOut);
inline bool hasPublishedSemanticCountTargetFact(const Expr &target,
                                         const SemanticProductIndex *semanticIndex);

inline bool classifySemanticCountTargetTypeText(std::string typeText,
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

inline bool classifySemanticStringKeyValueTargetTypeText(std::string typeText,
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

inline bool classifySemanticStringKeyValueTargetTypeText(const SemanticProgram &semanticProgram,
                                             const std::string &typeText,
                                             SymbolId typeTextId,
                                             bool &isStringKeyValueOut) {
  const std::string resolvedTypeText =
      std::string(resolveSemanticProductText(semanticProgram, typeTextId, typeText));
  return classifySemanticStringKeyValueTargetTypeText(resolvedTypeText, isStringKeyValueOut);
}

inline bool classifySemanticStringKeyValueCollectionTarget(
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

inline bool classifySemanticStringKeyValueTarget(const Expr &target,
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

inline SemanticStringKeyValueAccessResolution classifySemanticStringKeyValueAccess(
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

inline bool classifySemanticCountTargetTypeText(const SemanticProgram &semanticProgram,
                                         const std::string &typeText,
                                         SymbolId typeTextId,
                                         SemanticCountTargetInfo &infoOut) {
  const std::string resolvedTypeText =
      std::string(resolveSemanticProductText(semanticProgram, typeTextId, typeText));
  return classifySemanticCountTargetTypeText(resolvedTypeText, infoOut);
}

inline bool classifySemanticCollectionTarget(const SemanticProgram &semanticProgram,
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

inline bool classifySemanticCountTarget(const Expr &target,
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

inline bool hasPublishedSemanticCountTargetFact(const Expr &target,
                                         const SemanticProductIndex *semanticIndex) {
  if (semanticIndex == nullptr || target.semanticNodeId == 0) {
    return false;
  }
  return findSemanticProductCollectionSpecialization(*semanticIndex, target) != nullptr ||
         findSemanticProductBindingFact(*semanticIndex, target) != nullptr ||
         findSemanticProductLocalAutoFactBySemanticId(*semanticIndex, target) != nullptr ||
         findSemanticProductQueryFactBySemanticId(*semanticIndex, target) != nullptr;
}

inline SemanticDereferencedCountTargetResolution classifySemanticDereferencedCountTarget(
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

inline SemanticStringCountTargetResolution classifySemanticStringCountTarget(
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

inline const SemanticProgramQueryFact *findSourceMethodAccessQueryFact(
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

inline bool isSourceMethodStringKeyValueAccessTarget(
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
  if (resolvedPath != "/" + accessName &&
      resolvedPath != canonicalKeyValueHelperPath(accessName)) {
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
  if (queryType != "string" && !collection_helpers::isCollectionFamilyRoot(queryType, collection_helpers::CollectionFamily::String) &&
      bindingType != "string" && !collection_helpers::isCollectionFamilyRoot(bindingType, collection_helpers::CollectionFamily::String)) {
    return false;
  }
  if (accessNameOut != nullptr) {
    *accessNameOut = std::move(accessName);
  }
  return true;
}

inline bool publishedKeyValueAccessHelperReturnsString(const SemanticProgram *semanticProgram,
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
  return collection_helpers::isCollectionFamilyRoot(structPath, collection_helpers::CollectionFamily::String) || structPath == "string" ||
         collection_helpers::isCollectionFamilyRoot(bindingType, collection_helpers::CollectionFamily::String) || bindingType == "string";
}

inline bool hasExplicitStdKeyValueSourceSpelling(const Expr &expr) {
  const auto hasStdMapPrefix = [](std::string_view text) {
    return hasCanonicalCollectionMemberPrefix(text, "map");
  };
  return hasStdMapPrefix(expr.name) || hasStdMapPrefix(expr.namespacePrefix);
}

inline bool resolveEntryArgsParameterFromSemanticProduct(const Definition &entryDef,
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

} // namespace count_access_internal
} // namespace primec::ir_lowerer
