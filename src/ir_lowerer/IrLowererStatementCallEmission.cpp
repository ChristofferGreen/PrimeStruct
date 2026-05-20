#include "IrLowererStatementCallHelpers.h"

#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

#include <optional>
#include <string_view>
#include <utility>

namespace primec::ir_lowerer {

namespace {

std::string stripGeneratedHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

std::string stdCollectionsRoot() {
  return "/std/collections";
}

std::string experimentalCollectionTypePath(std::string_view collectionName,
                                           std::string_view typeName) {
  return stdCollectionsRoot() + "/experimental_" + std::string(collectionName) +
         "/" + std::string(typeName);
}

std::string collectionWrapperAlias(std::string_view collectionName,
                                   std::string_view suffix) {
  return std::string(collectionName) + std::string(suffix);
}

const StdlibSurfaceMetadata *statementVectorHelperMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
}

const StdlibSurfaceMetadata *statementKeyValueHelperMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

bool isExperimentalVectorTypePath(std::string_view path) {
  const std::string vectorTypePath = experimentalCollectionTypePath("vector", "Vector");
  return path == vectorTypePath || path.rfind(vectorTypePath + "__", 0) == 0;
}

bool matchesRegistrySpellingSet(std::span<const std::string_view> spellings,
                                std::string_view spelling) {
  for (const std::string_view candidate : spellings) {
    if (candidate == spelling) {
      return true;
    }
  }
  return false;
}

std::string resolveStatementCallPathWithoutFallbackProbes(const Expr &expr) {
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
  }
  return expr.name;
}

std::string canonicalStatementVectorHelperName(std::string helperName) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  if (helperName == collectionWrapperAlias("vector", "Count")) {
    return "count";
  }
  if (helperName == collectionWrapperAlias("vector", "Capacity")) {
    return "capacity";
  }
  if (helperName == collectionWrapperAlias("vector", "At")) {
    return "at";
  }
  if (helperName == collectionWrapperAlias("vector", "AtUnsafe")) {
    return "at_unsafe";
  }
  if (helperName == collectionWrapperAlias("vector", "Push")) {
    return "push";
  }
  if (helperName == collectionWrapperAlias("vector", "Pop")) {
    return "pop";
  }
  if (helperName == collectionWrapperAlias("vector", "Reserve")) {
    return "reserve";
  }
  if (helperName == collectionWrapperAlias("vector", "Clear")) {
    return "clear";
  }
  if (helperName == collectionWrapperAlias("vector", "RemoveAt")) {
    return "remove_at";
  }
  if (helperName == collectionWrapperAlias("vector", "RemoveSwap")) {
    return "remove_swap";
  }
  return helperName;
}

bool resolvePublishedStatementVectorHelperName(std::string_view resolvedPath,
                                               std::string &helperNameOut) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  const auto *vectorMetadata = statementVectorHelperMetadata();
  if (metadata == nullptr || vectorMetadata == nullptr || metadata->id != vectorMetadata->id) {
    return false;
  }
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string_view::npos || slash + 1 >= resolvedPath.size()) {
    return false;
  }
  helperNameOut =
      canonicalStatementVectorHelperName(std::string(resolvedPath.substr(slash + 1)));
  return !helperNameOut.empty();
}

bool isPublishedCanonicalStatementVectorHelperPath(std::string_view resolvedPath) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  const auto *vectorMetadata = statementVectorHelperMetadata();
  return metadata != nullptr &&
         vectorMetadata != nullptr &&
         metadata->id == vectorMetadata->id &&
         matchesRegistrySpellingSet(metadata->loweringSpellings, resolvedPath);
}

std::string canonicalStatementKeyValueHelperName(std::string helperName) {
  helperName = stripGeneratedHelperSuffix(std::move(helperName));
  return helperName;
}

bool resolvePublishedStatementKeyValueHelperName(std::string_view resolvedPath,
                                            std::string &helperNameOut) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  const auto *keyValueMetadata = statementKeyValueHelperMetadata();
  if (metadata == nullptr || keyValueMetadata == nullptr ||
      metadata->id != keyValueMetadata->id) {
    return false;
  }
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string_view::npos || slash + 1 >= resolvedPath.size()) {
    return false;
  }
  helperNameOut =
      canonicalStatementKeyValueHelperName(std::string(resolvedPath.substr(slash + 1)));
  return !helperNameOut.empty();
}

std::string canonicalStatementKeyValueHelperPath(std::string_view helperName) {
  const auto *metadata = statementKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
}

bool isFreeMemoryIntrinsicCall(const Expr &expr) {
  std::string builtinName;
  return expr.kind == Expr::Kind::Call && getBuiltinMemoryName(expr, builtinName) && builtinName == "free";
}

std::string resolveStatementCallSemanticTypeText(const SemanticProgram *semanticProgram,
                                                 SymbolId typeTextId,
                                                 const std::string &typeText) {
  if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
    const std::string resolvedTypeText =
        std::string(semanticProgramResolveCallTargetString(*semanticProgram, typeTextId));
    if (!resolvedTypeText.empty()) {
      return trimTemplateTypeText(resolvedTypeText);
    }
  }
  return trimTemplateTypeText(typeText);
}

static bool populateStatementVectorTargetInfoFromTypeText(
    const std::string &typeText,
    ArrayVectorAccessTargetInfo &targetInfoOut) {
  std::string base;
  std::string argText;
  const std::string normalizedTypeText =
      unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(typeText));
  if (!splitTemplateTypeName(normalizedTypeText, base, argText)) {
    return false;
  }

  const std::string normalizedBase =
      normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
    return populateStatementVectorTargetInfoFromTypeText(argText, targetInfoOut);
  }
  if (normalizedBase != "vector") {
    return false;
  }

  targetInfoOut = {};
  targetInfoOut.isArrayOrVectorTarget = true;
  targetInfoOut.isVectorTarget = true;
  std::vector<std::string> args;
  if (splitTemplateArgs(argText, args) && args.size() == 1) {
    const std::string elementType = trimTemplateTypeText(args.front());
    targetInfoOut.elemKind = valueKindFromTypeName(elementType);
    if (targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown &&
        !elementType.empty()) {
      targetInfoOut.structTypeName =
          specializedExperimentalVectorStructPathForElementType(elementType);
    }
  }
  return true;
}

static bool resolveStatementVectorReceiverTargetInfoFromSemanticFacts(
    const Expr &receiverExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    ArrayVectorAccessTargetInfo &targetInfoOut,
    bool &hasSemanticVectorFactOut) {
  hasSemanticVectorFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return false;
  }

  auto tryPopulateFromSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    return !resolvedTypeText.empty() &&
           populateStatementVectorTargetInfoFromTypeText(resolvedTypeText, targetInfoOut);
  };

  if (receiverExpr.semanticNodeId != 0) {
    if (const auto *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr);
        collectionFact != nullptr) {
      hasSemanticVectorFactOut = true;
      const std::string collectionFamily = resolveStatementCallSemanticTypeText(
          semanticProgram,
          collectionFact->collectionFamilyId,
          collectionFact->collectionFamily);
      if (normalizeCollectionBindingTypeName(collectionFamily) != "vector") {
        return false;
      }
      targetInfoOut = {};
      targetInfoOut.isArrayOrVectorTarget = true;
      targetInfoOut.isVectorTarget = true;
      const std::string elementType = resolveStatementCallSemanticTypeText(
          semanticProgram,
          collectionFact->elementTypeTextId,
          collectionFact->elementTypeText);
      targetInfoOut.elemKind = valueKindFromTypeName(elementType);
      if (targetInfoOut.elemKind == LocalInfo::ValueKind::Unknown &&
          !elementType.empty()) {
        targetInfoOut.structTypeName =
            specializedExperimentalVectorStructPathForElementType(elementType);
      }
      return true;
    }

    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr);
        queryFact != nullptr) {
      hasSemanticVectorFactOut = true;
      return tryPopulateFromSemanticTypeText(queryFact->bindingTypeTextId,
                                             queryFact->bindingTypeText) ||
             tryPopulateFromSemanticTypeText(queryFact->queryTypeTextId,
                                             queryFact->queryTypeText) ||
             tryPopulateFromSemanticTypeText(queryFact->receiverBindingTypeTextId,
                                             queryFact->receiverBindingTypeText);
    }

    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, receiverExpr);
        bindingFact != nullptr) {
      hasSemanticVectorFactOut = true;
      return tryPopulateFromSemanticTypeText(bindingFact->bindingTypeTextId,
                                             bindingFact->bindingTypeText);
    }

    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, receiverExpr);
        localAutoFact != nullptr) {
      hasSemanticVectorFactOut = true;
      return tryPopulateFromSemanticTypeText(localAutoFact->bindingTypeTextId,
                                             localAutoFact->bindingTypeText);
    }
  }

  if (receiverExpr.kind == Expr::Kind::Call && receiverExpr.args.size() == 1 &&
      (isSimpleCallName(receiverExpr, "location") ||
       isSimpleCallName(receiverExpr, "dereference"))) {
    return resolveStatementVectorReceiverTargetInfoFromSemanticFacts(
        receiverExpr.args.front(),
        semanticProgram,
        semanticIndex,
        targetInfoOut,
        hasSemanticVectorFactOut);
  }

  return false;
}

static bool resolveStatementVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  const auto *metadata = statementVectorHelperMetadata();
  if (metadata == nullptr) {
    return false;
  }
  return resolvePublishedStdlibSurfaceExprMemberName(
             expr,
             metadata->id,
             helperNameOut) ||
         resolvePublishedStatementVectorHelperName(
             resolveStatementCallPathWithoutFallbackProbes(expr),
             helperNameOut);
}

static bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveStatementVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

static bool isExplicitVectorMutatorHelperCall(const Expr &expr) {
  std::string aliasName;
  if (!resolveStatementVectorHelperAliasName(expr, aliasName)) {
    return false;
  }
  return aliasName == "push" || aliasName == "pop" || aliasName == "reserve" || aliasName == "clear" ||
         aliasName == "remove_at" || aliasName == "remove_swap";
}

static size_t explicitVectorHelperReceiverIndex(const Expr &expr) {
  for (size_t i = 0; i < expr.argNames.size() && i < expr.args.size(); ++i) {
    if (expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
      return i;
    }
  }
  return 0;
}

static bool isStatementSoaVectorTypeText(const std::string &typeText) {
  std::string base;
  std::string argText;
  const std::string normalizedTypeText =
      unwrapTopLevelUninitializedTypeText(trimTemplateTypeText(typeText));
  if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
    const std::string normalizedBase =
        normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
    if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
        normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
      return isStatementSoaVectorTypeText(argText);
    }
    return normalizedBase == "soa" "_vector";
  }
  return normalizeCollectionBindingTypeName(normalizedTypeText) == "soa" "_vector";
}

static bool resolveStatementSoaVectorReceiverFromSemanticFacts(
    const Expr &receiverExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool &hasSemanticSoaVectorFactOut) {
  hasSemanticSoaVectorFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return false;
  }

  auto tryResolveSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    return !resolvedTypeText.empty() && isStatementSoaVectorTypeText(resolvedTypeText);
  };

  if (receiverExpr.semanticNodeId != 0) {
    if (const auto *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr);
        collectionFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      const std::string collectionFamily = resolveStatementCallSemanticTypeText(
          semanticProgram,
          collectionFact->collectionFamilyId,
          collectionFact->collectionFamily);
      return normalizeCollectionBindingTypeName(collectionFamily) == "soa" "_vector";
    }

    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr);
        queryFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      return tryResolveSemanticTypeText(queryFact->bindingTypeTextId,
                                        queryFact->bindingTypeText) ||
             tryResolveSemanticTypeText(queryFact->queryTypeTextId,
                                        queryFact->queryTypeText) ||
             tryResolveSemanticTypeText(queryFact->receiverBindingTypeTextId,
                                        queryFact->receiverBindingTypeText);
    }

    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndex, receiverExpr);
        bindingFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      return tryResolveSemanticTypeText(bindingFact->bindingTypeTextId,
                                        bindingFact->bindingTypeText);
    }

    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, receiverExpr);
        localAutoFact != nullptr) {
      hasSemanticSoaVectorFactOut = true;
      return tryResolveSemanticTypeText(localAutoFact->bindingTypeTextId,
                                        localAutoFact->bindingTypeText);
    }
  }

  if (receiverExpr.kind == Expr::Kind::Call && receiverExpr.args.size() == 1 &&
      (isSimpleCallName(receiverExpr, "location") ||
       isSimpleCallName(receiverExpr, "dereference"))) {
    return resolveStatementSoaVectorReceiverFromSemanticFacts(
        receiverExpr.args.front(),
        semanticProgram,
        semanticIndex,
        hasSemanticSoaVectorFactOut);
  }

  return false;
}

static bool isSoaVectorTargetExpr(const Expr &expr,
                                  const LocalMap &localsIn,
                                  const SemanticProgram *semanticProgram,
                                  const SemanticProductIndex *semanticIndex) {
  bool hasSemanticSoaVectorFact = false;
  if (resolveStatementSoaVectorReceiverFromSemanticFacts(expr,
                                                         semanticProgram,
                                                         semanticIndex,
                                                         hasSemanticSoaVectorFact)) {
    return true;
  }
  if (hasSemanticSoaVectorFact) {
    return false;
  }
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    return getBuiltinCollectionName(expr, collection) && collection == "soa" "_vector";
  }
  return false;
}

static std::optional<LocalInfo::ValueKind> resolveBufferElementKindFromTypeText(
    const std::string &typeText,
    bool allowWrappedBuffer) {
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
    return std::nullopt;
  }

  const std::string normalizedBase = trimTemplateTypeText(base);
  if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
      normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
    if (!allowWrappedBuffer) {
      return std::nullopt;
    }
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args) || args.size() != 1) {
      return std::nullopt;
    }
    return resolveBufferElementKindFromTypeText(args.front(), true);
  }

  const bool isBufferBase =
      normalizedBase == "Buffer" || normalizedBase == "/Buffer" ||
      normalizedBase == "std/gfx/Buffer" || normalizedBase == "/std/gfx/Buffer" ||
      normalizedBase == "std/gfx/experimental/Buffer" ||
      normalizedBase == "/std/gfx/experimental/Buffer";
  if (!isBufferBase) {
    return std::nullopt;
  }

  std::vector<std::string> args;
  if (!splitTemplateArgs(argText, args) || args.size() != 1) {
    return std::nullopt;
  }
  const LocalInfo::ValueKind elementKind =
      valueKindFromTypeName(trimTemplateTypeText(args.front()));
  if (elementKind == LocalInfo::ValueKind::Unknown) {
    return std::nullopt;
  }
  return elementKind;
}

static std::optional<LocalInfo::ValueKind> resolveBufferTargetElementKindFromSemanticFacts(
    const Expr &bufferExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool allowWrappedBuffer,
    bool &hasSemanticBufferFactOut) {
  hasSemanticBufferFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr) {
    return std::nullopt;
  }

  auto tryResolveSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) -> std::optional<LocalInfo::ValueKind> {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    if (resolvedTypeText.empty()) {
      return std::nullopt;
    }
    return resolveBufferElementKindFromTypeText(resolvedTypeText, allowWrappedBuffer);
  };

  if (bufferExpr.semanticNodeId != 0) {
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, bufferExpr);
        queryFact != nullptr) {
      hasSemanticBufferFactOut = true;
      if (auto resolvedKind =
              tryResolveSemanticTypeText(queryFact->bindingTypeTextId, queryFact->bindingTypeText);
          resolvedKind.has_value()) {
        return resolvedKind;
      }
      if (auto resolvedKind =
              tryResolveSemanticTypeText(queryFact->queryTypeTextId, queryFact->queryTypeText);
          resolvedKind.has_value()) {
        return resolvedKind;
      }
      return std::nullopt;
    }

    if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, bufferExpr);
        bindingFact != nullptr) {
      hasSemanticBufferFactOut = true;
      return tryResolveSemanticTypeText(bindingFact->bindingTypeTextId,
                                        bindingFact->bindingTypeText);
    }

    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, bufferExpr);
        localAutoFact != nullptr) {
      hasSemanticBufferFactOut = true;
      return tryResolveSemanticTypeText(localAutoFact->bindingTypeTextId,
                                        localAutoFact->bindingTypeText);
    }
  }

  if (bufferExpr.kind == Expr::Kind::Call && bufferExpr.args.size() == 1 &&
      isSimpleCallName(bufferExpr, "dereference")) {
    return resolveBufferTargetElementKindFromSemanticFacts(
        bufferExpr.args.front(), semanticProgram, semanticIndex, true, hasSemanticBufferFactOut);
  }

  return std::nullopt;
}

static std::optional<LocalInfo::ValueKind> resolveBufferTargetElementKind(
    const Expr &bufferExpr,
    const LocalMap &localsIn,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  bool hasSemanticBufferFact = false;
  if (const auto semanticKind = resolveBufferTargetElementKindFromSemanticFacts(
          bufferExpr, semanticProgram, semanticIndex, false, hasSemanticBufferFact);
      semanticKind.has_value() || hasSemanticBufferFact) {
    return semanticKind;
  }

  if (bufferExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(bufferExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
  }

  if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "buffer") &&
      bufferExpr.templateArgs.size() == 1) {
    return valueKindFromTypeName(bufferExpr.templateArgs.front());
  }

  if (bufferExpr.kind == Expr::Kind::Call && isSimpleCallName(bufferExpr, "dereference") &&
      bufferExpr.args.size() == 1) {
    const Expr &targetExpr = bufferExpr.args.front();
    if (targetExpr.kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.name);
      if (it != localsIn.end() &&
          ((it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
           (it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
        return it->second.valueKind;
      }
    }

    std::string derefAccessName;
    if (targetExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(targetExpr, derefAccessName) &&
        targetExpr.args.size() == 2 && targetExpr.args.front().kind == Expr::Kind::Name) {
      auto it = localsIn.find(targetExpr.args.front().name);
      if (it != localsIn.end() && it->second.isArgsPack &&
          ((it->second.argsPackElementKind == LocalInfo::Kind::Reference && it->second.referenceToBuffer) ||
           (it->second.argsPackElementKind == LocalInfo::Kind::Pointer && it->second.pointerToBuffer))) {
        return it->second.valueKind;
      }
    }
  }

  std::string accessName;
  if (bufferExpr.kind == Expr::Kind::Call && getBuiltinArrayAccessName(bufferExpr, accessName) &&
      bufferExpr.args.size() == 2 && bufferExpr.args.front().kind == Expr::Kind::Name) {
    auto it = localsIn.find(bufferExpr.args.front().name);
    if (it != localsIn.end() && it->second.isArgsPack && it->second.argsPackElementKind == LocalInfo::Kind::Buffer) {
      return it->second.valueKind;
    }
  }

  return std::nullopt;
}

static std::optional<LocalInfo::ValueKind> resolveStatementValueKindFromSemanticFacts(
    const Expr &expr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    bool &hasSemanticValueFactOut) {
  hasSemanticValueFactOut = false;
  if (semanticProgram == nullptr || semanticIndex == nullptr || expr.semanticNodeId == 0) {
    return std::nullopt;
  }

  auto tryResolveSemanticTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) -> std::optional<LocalInfo::ValueKind> {
    const std::string resolvedTypeText =
        resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
    if (resolvedTypeText.empty()) {
      return std::nullopt;
    }
    const LocalInfo::ValueKind valueKind = valueKindFromTypeName(resolvedTypeText);
    if (valueKind == LocalInfo::ValueKind::Unknown) {
      return std::nullopt;
    }
    return valueKind;
  };

  if (const auto *bindingFact = findSemanticProductBindingFact(*semanticIndex, expr);
      bindingFact != nullptr) {
    hasSemanticValueFactOut = true;
    return tryResolveSemanticTypeText(bindingFact->bindingTypeTextId,
                                      bindingFact->bindingTypeText);
  }

  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, expr);
      localAutoFact != nullptr) {
    hasSemanticValueFactOut = true;
    return tryResolveSemanticTypeText(localAutoFact->bindingTypeTextId,
                                      localAutoFact->bindingTypeText);
  }

  if (const auto *queryFact = findSemanticProductQueryFact(semanticProgram, *semanticIndex, expr);
      queryFact != nullptr) {
    hasSemanticValueFactOut = true;
    if (auto valueKind =
            tryResolveSemanticTypeText(queryFact->bindingTypeTextId, queryFact->bindingTypeText);
        valueKind.has_value()) {
      return valueKind;
    }
    return tryResolveSemanticTypeText(queryFact->queryTypeTextId, queryFact->queryTypeText);
  }

  return std::nullopt;
}

static bool rewriteKeyValueInsertHelperStatementToCanonical(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    Expr &rewrittenStmt) {
  if (stmt.kind != Expr::Kind::Call || stmt.args.size() != 3) {
    return false;
  }

  auto isPrimeKeyValueInsertBody = [](const Definition *callee) {
    if (callee == nullptr) {
      return false;
    }
    return callee->fullPath == "/std/collections/internal_map/insertImpl" ||
           callee->fullPath.rfind("/std/collections/internal_map/insertImpl__", 0) == 0 ||
           callee->fullPath == "/std/collections/internal_map/insertRefImpl" ||
           callee->fullPath.rfind("/std/collections/internal_map/insertRefImpl__", 0) == 0;
  };
  if (stmt.isMethodCall) {
    if (isPrimeKeyValueInsertBody(resolveMethodCallDefinition(stmt, localsIn))) {
      return false;
    }
  } else {
    if (isPrimeKeyValueInsertBody(resolveDefinitionCall(stmt))) {
      return false;
    }
  }

  auto isDirectBareKeyValueInsertHelperStem = [&](const Expr &callExpr) {
    if (callExpr.isMethodCall || callExpr.name.empty()) {
      return false;
    }
    std::string helperName =
        resolveStatementCallPathWithoutFallbackProbes(callExpr);
    if (!helperName.empty() && helperName.front() == '/') {
      helperName.erase(helperName.begin());
    }
    if (helperName.find('/') != std::string::npos) {
      return false;
    }
    helperName = stripGeneratedHelperSuffix(std::move(helperName));
    return helperName == "insert" || helperName == "insert_ref" ||
           helperName == "Insert" || helperName == "InsertRef";
  };
  auto isPublishedKeyValueInsertHelperCall = [](const Expr &callExpr) {
    std::string helperName;
    return resolvePublishedStatementKeyValueHelperName(
               resolveStatementCallPathWithoutFallbackProbes(callExpr),
               helperName) &&
           (helperName == "insert" || helperName == "insert_ref");
  };

  size_t receiverIndex = 0;
  if (stmt.isMethodCall) {
    std::string normalizedName =
        resolveStatementCallPathWithoutFallbackProbes(stmt);
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    const size_t lastSlash = normalizedName.find_last_of('/');
    if (lastSlash != std::string::npos) {
      normalizedName = normalizedName.substr(lastSlash + 1);
    }
    normalizedName = stripGeneratedHelperSuffix(std::move(normalizedName));
    const bool isMethodInsertStem =
        normalizedName == "insert" || normalizedName == "insert_ref" ||
        normalizedName == "Insert" || normalizedName == "InsertRef";
    if (!isMethodInsertStem) {
      return false;
    }
  } else {
    std::string helperName;
    if ((!resolveKeyValueHelperAliasName(stmt, helperName) || helperName != "insert") &&
        !isPublishedKeyValueInsertHelperCall(stmt) &&
        !isDirectBareKeyValueInsertHelperStem(stmt)) {
      return false;
    }
    if (hasNamedArguments(stmt.argNames)) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() &&
            *stmt.argNames[i] == "values") {
          receiverIndex = i;
          break;
        }
      }
    }
  }

  if (receiverIndex >= stmt.args.size()) {
    return false;
  }
  const auto inferCallKeyValueTargetInfo = [&](const Expr &targetExpr, CollectionPairTypeInfo &targetInfoOut) {
    auto normalizeInsertHelperStem = [&](const std::string &path) {
      std::string helperName = path;
      if (!helperName.empty() && helperName.front() == '/') {
        helperName.erase(helperName.begin());
      }
      const size_t lastSlash = helperName.find_last_of('/');
      if (lastSlash != std::string::npos) {
        helperName = helperName.substr(lastSlash + 1);
      }
      return stripGeneratedHelperSuffix(std::move(helperName));
    };
    auto isKeyValueInsertLikeCallee = [&](const Definition &callee) {
      std::string publishedHelperName;
      if (resolvePublishedStatementKeyValueHelperName(callee.fullPath, publishedHelperName) &&
          (publishedHelperName == "insert" || publishedHelperName == "insert_ref")) {
        return true;
      }
      const std::string helperStem = normalizeInsertHelperStem(callee.fullPath);
      if (helperStem == "insert" || helperStem == "insert_ref" ||
          helperStem == "Insert" || helperStem == "InsertRef") {
        return true;
      }
      Expr calleeExpr;
      calleeExpr.kind = Expr::Kind::Call;
      calleeExpr.name = callee.fullPath;
      std::string helperName;
      return resolveKeyValueHelperAliasName(calleeExpr, helperName) && helperName == "insert";
    };
    std::function<bool(const std::string &, LocalInfo::ValueKind &, LocalInfo::ValueKind &)>
        inferKeyValueKindsFromTypeText;
    inferKeyValueKindsFromTypeText = [&](const std::string &typeText,
                                    LocalInfo::ValueKind &keyKindOut,
                                    LocalInfo::ValueKind &valueKindOut) {
      keyKindOut = LocalInfo::ValueKind::Unknown;
      valueKindOut = LocalInfo::ValueKind::Unknown;

      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
        return false;
      }

      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        return inferKeyValueKindsFromTypeText(argText, keyKindOut, valueKindOut);
      }

      const bool isKeyValueBase =
          isBuiltinCollectionTypeName(normalizedBase, "map") ||
          isExperimentalCollectionTypeName(normalizedBase, "map", "Map");
      if (!isKeyValueBase) {
        return false;
      }

      std::vector<std::string> keyValueArgs;
      if (!splitTemplateArgs(argText, keyValueArgs) || keyValueArgs.size() != 2) {
        return false;
      }
      keyKindOut = valueKindFromTypeName(trimTemplateTypeText(keyValueArgs.front()));
      valueKindOut = valueKindFromTypeName(trimTemplateTypeText(keyValueArgs.back()));
      return keyKindOut != LocalInfo::ValueKind::Unknown &&
             valueKindOut != LocalInfo::ValueKind::Unknown;
    };
    std::function<std::string(const std::string &)> inferKeyValueStructPathFromTypeText;
    inferKeyValueStructPathFromTypeText = [&](const std::string &typeText) {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
        return std::string{};
      }

      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
        return inferKeyValueStructPathFromTypeText(argText);
      }

      const std::string experimentalKeyValueType =
          experimentalCollectionTypePath("map", "Map");
      const std::string slashlessExperimentalKeyValueType =
          experimentalKeyValueType.substr(1);
      if (normalizedBase == slashlessExperimentalKeyValueType ||
          normalizedBase == experimentalKeyValueType) {
        return experimentalKeyValueType;
      }
      return std::string{};
    };
    auto inferKeyValueKindsFromArgsPackTypeText = [&](const std::string &typeText,
                                                 LocalInfo::ValueKind &keyKindOut,
                                                 LocalInfo::ValueKind &valueKindOut) {
      keyKindOut = LocalInfo::ValueKind::Unknown;
      valueKindOut = LocalInfo::ValueKind::Unknown;

      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
        return false;
      }

      const std::string normalizedBase = trimTemplateTypeText(base);
      const bool isArgsPackBase =
          normalizedBase == "args" || normalizedBase == "/args" ||
          normalizedBase == "Args" || normalizedBase == "/Args";
      if (!isArgsPackBase) {
        return false;
      }
      return inferKeyValueKindsFromTypeText(argText, keyKindOut, valueKindOut);
    };
    auto extractParameterTypeName = [&](const Expr &paramExpr) {
      for (const auto &transform : paramExpr.transforms) {
        if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
            transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
            transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
            transform.name == "effects" || transform.name == "capabilities") {
          continue;
        }
        if (!transform.arguments.empty()) {
          continue;
        }
        std::string typeName = transform.name;
        if (!transform.templateArgs.empty()) {
          typeName += "<";
          for (size_t index = 0; index < transform.templateArgs.size(); ++index) {
            if (index != 0) {
              typeName += ", ";
            }
            typeName += trimTemplateTypeText(transform.templateArgs[index]);
          }
          typeName += ">";
        }
        return typeName;
      }
      return std::string{};
    };
    auto tryPopulateFromSemanticTypeText = [&](SymbolId typeTextId,
                                               const std::string &typeText,
                                               CollectionPairTypeInfo &targetInfoOut) {
      const std::string resolvedTypeText =
          resolveStatementCallSemanticTypeText(semanticProgram, typeTextId, typeText);
      if (resolvedTypeText.empty()) {
        return false;
      }
      if (!inferKeyValueKindsFromTypeText(resolvedTypeText,
                                     targetInfoOut.keyValueKeyKind,
                                     targetInfoOut.keyValueValueKind)) {
        return false;
      }
      targetInfoOut.isKeyValueTarget = true;
      targetInfoOut.structTypeName = inferKeyValueStructPathFromTypeText(resolvedTypeText);
      return true;
    };
    auto tryPopulateFromSemanticCollectionSpecialization =
        [&](const Expr &receiverExpr, CollectionPairTypeInfo &targetInfoOut, bool &hasSemanticMapFactOut) {
      if (semanticIndex == nullptr || receiverExpr.semanticNodeId == 0) {
        return false;
      }
      const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, receiverExpr);
      if (collectionFact == nullptr) {
        return false;
      }
      hasSemanticMapFactOut = true;
      const std::string collectionFamily = resolveStatementCallSemanticTypeText(
          semanticProgram,
          collectionFact->collectionFamilyId,
          collectionFact->collectionFamily);
      if (!isBuiltinCollectionTypeName(collectionFamily, "map")) {
        return false;
      }
      const LocalInfo::ValueKind keyKind = valueKindFromTypeName(
          resolveStatementCallSemanticTypeText(semanticProgram,
                                               collectionFact->keyTypeTextId,
                                               collectionFact->keyTypeText));
      const LocalInfo::ValueKind valueKind = valueKindFromTypeName(
          resolveStatementCallSemanticTypeText(semanticProgram,
                                               collectionFact->valueTypeTextId,
                                               collectionFact->valueTypeText));
      if (keyKind == LocalInfo::ValueKind::Unknown ||
          valueKind == LocalInfo::ValueKind::Unknown) {
        return false;
      }
      targetInfoOut.isKeyValueTarget = true;
      targetInfoOut.keyValueKeyKind = keyKind;
      targetInfoOut.keyValueValueKind = valueKind;
      return true;
    };
    auto tryPopulateFromSemanticReceiverFact =
        [&](const Expr &receiverExpr,
            CollectionPairTypeInfo &targetInfoOut,
            bool &hasSemanticMapFactOut) {
      hasSemanticMapFactOut = false;
      if (semanticProgram == nullptr ||
          semanticIndex == nullptr ||
          receiverExpr.semanticNodeId == 0) {
        return false;
      }
      if (tryPopulateFromSemanticCollectionSpecialization(
              receiverExpr, targetInfoOut, hasSemanticMapFactOut)) {
        return true;
      }
      if (const auto *queryFact =
              findSemanticProductQueryFact(semanticProgram, *semanticIndex, receiverExpr);
          queryFact != nullptr) {
        hasSemanticMapFactOut = true;
        if (tryPopulateFromSemanticTypeText(queryFact->bindingTypeTextId,
                                            queryFact->bindingTypeText,
                                            targetInfoOut) ||
            tryPopulateFromSemanticTypeText(queryFact->queryTypeTextId,
                                            queryFact->queryTypeText,
                                            targetInfoOut) ||
            tryPopulateFromSemanticTypeText(queryFact->receiverBindingTypeTextId,
                                            queryFact->receiverBindingTypeText,
                                            targetInfoOut)) {
          return true;
        }
      }
      if (const auto *bindingFact =
              findSemanticProductBindingFact(*semanticIndex, receiverExpr);
          bindingFact != nullptr) {
        hasSemanticMapFactOut = true;
        if (tryPopulateFromSemanticTypeText(bindingFact->bindingTypeTextId,
                                            bindingFact->bindingTypeText,
                                            targetInfoOut)) {
          return true;
        }
      }
      if (const auto *localAutoFact =
              findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, receiverExpr);
          localAutoFact != nullptr) {
        hasSemanticMapFactOut = true;
        if (tryPopulateFromSemanticTypeText(localAutoFact->bindingTypeTextId,
                                            localAutoFact->bindingTypeText,
                                            targetInfoOut)) {
          return true;
        }
      }
      return false;
    };

    auto peelLocationWrappers = [&](const Expr &expr) {
      const Expr *current = &expr;
      while (current->kind == Expr::Kind::Call &&
             isSimpleCallName(*current, "location") &&
             current->args.size() == 1) {
        current = &current->args.front();
      }
      return current;
    };

    bool receiverWasDereferenced = false;
    auto peelReceiverWrappers = [&](const Expr &expr) {
      const Expr *current = peelLocationWrappers(expr);
      while (current->kind == Expr::Kind::Call &&
             isSimpleCallName(*current, "dereference") &&
             current->args.size() == 1) {
        receiverWasDereferenced = true;
        current = peelLocationWrappers(current->args.front());
      }
      return current;
    };
    auto isKeyValueArgsPackAccessCall = [&](const Expr &expr) {
      if (expr.kind != Expr::Kind::Call || expr.args.size() != 2) {
        return false;
      }
      auto isDirectKeyValueArgsPackStem = [&](const char *stem) {
        if (stem == nullptr || expr.isMethodCall || expr.name.empty()) {
          return false;
        }
        std::string directName = resolveStatementCallPathWithoutFallbackProbes(expr);
        if (!directName.empty() && directName.front() == '/') {
          directName.erase(directName.begin());
        }
        if (directName.find('/') != std::string::npos) {
          return false;
        }
        return stripGeneratedHelperSuffix(std::move(directName)) == stem;
      };
      auto isKeyValueArgsPackMethodStem = [&](const char *stem) {
        if (isSimpleCallName(expr, stem)) {
          return true;
        }
        if (!expr.isMethodCall || expr.name.empty()) {
          return false;
        }
        std::string methodName = resolveStatementCallPathWithoutFallbackProbes(expr);
        if (!methodName.empty() && methodName.front() == '/') {
          methodName.erase(methodName.begin());
        }
        if (methodName.find('/') != std::string::npos) {
          return false;
        }
        return stripGeneratedHelperSuffix(std::move(methodName)) == stem;
      };
      if (expr.isMethodCall) {
        if (isKeyValueArgsPackMethodStem("at") || isKeyValueArgsPackMethodStem("at_unsafe") ||
            isKeyValueArgsPackMethodStem("at_ref") || isKeyValueArgsPackMethodStem("at_unsafe_ref") ||
            isKeyValueArgsPackMethodStem("At") || isKeyValueArgsPackMethodStem("AtUnsafe") ||
            isKeyValueArgsPackMethodStem("AtRef") || isKeyValueArgsPackMethodStem("AtUnsafeRef")) {
          return true;
        }
      }
      if (isDirectKeyValueArgsPackStem("at") || isDirectKeyValueArgsPackStem("at_unsafe") ||
          isDirectKeyValueArgsPackStem("at_ref") || isDirectKeyValueArgsPackStem("at_unsafe_ref") ||
          isDirectKeyValueArgsPackStem("At") || isDirectKeyValueArgsPackStem("AtUnsafe") ||
          isDirectKeyValueArgsPackStem("AtRef") || isDirectKeyValueArgsPackStem("AtUnsafeRef")) {
        return true;
      }
      std::string publishedHelperName;
      const auto *metadata = statementKeyValueHelperMetadata();
      if (metadata != nullptr &&
          resolvePublishedStdlibSurfaceExprMemberName(
              expr,
              metadata->id,
              publishedHelperName) &&
          (publishedHelperName == "at" || publishedHelperName == "at_ref" ||
           publishedHelperName == "at_unsafe" ||
           publishedHelperName == "at_unsafe_ref")) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(expr, accessName) && accessName == "map") {
        return true;
      }
      std::string helperName;
      return resolveKeyValueHelperAliasName(expr, helperName) &&
             (helperName == "at" || helperName == "at_ref" ||
              helperName == "at_unsafe" || helperName == "at_unsafe_ref");
    };

    const Expr *canonicalReceiverExpr = peelReceiverWrappers(targetExpr);

    targetInfoOut = {};
    bool hasSemanticKeyValueReceiverFact = false;
    if (tryPopulateFromSemanticReceiverFact(
            *canonicalReceiverExpr, targetInfoOut, hasSemanticKeyValueReceiverFact)) {
      return true;
    }
    if (hasSemanticKeyValueReceiverFact) {
      return false;
    }

    // Reuse the core access-target resolver on the peeled receiver shape so
    // wrapped args-pack map-access forms can flow through the same typed
    // map-target inference used by direct receivers.
    auto canonicalTargetInfo =
        resolveCollectionPairTypeInfo(*canonicalReceiverExpr,
                                   localsIn,
                                   {},
                                   semanticProgram,
                                   semanticIndex);
    if (receiverWasDereferenced) {
      canonicalTargetInfo.isWrappedKeyValueTarget = false;
    }
    if (canonicalTargetInfo.isKeyValueTarget &&
        canonicalTargetInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
        canonicalTargetInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
      targetInfoOut = canonicalTargetInfo;
      return true;
    }

    const Expr *nonLocalFieldReceiver = nullptr;
    if (canonicalReceiverExpr->kind == Expr::Kind::Call &&
        canonicalReceiverExpr->isFieldAccess &&
        canonicalReceiverExpr->args.size() == 1) {
      nonLocalFieldReceiver = canonicalReceiverExpr;
    }

    targetInfoOut = {};
    if (canonicalReceiverExpr->kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(canonicalReceiverExpr->name);
      if (localIt != localsIn.end()) {
        const LocalInfo &localInfo = localIt->second;
        const bool directKeyValue =
            localInfo.kind == LocalInfo::Kind::Value && hasKeyValueKinds(localInfo);
        const bool wrappedKeyValue =
            hasWrappedKeyValueKinds(localInfo, localInfo.kind);
        const bool argsPackKeyValue =
            localInfo.isArgsPack &&
            ((localInfo.argsPackElementKind == LocalInfo::Kind::Value && hasKeyValueKinds(localInfo)) ||
             hasWrappedKeyValueKinds(localInfo, localInfo.argsPackElementKind));
        if (directKeyValue || wrappedKeyValue || argsPackKeyValue) {
          targetInfoOut.isKeyValueTarget = true;
          targetInfoOut.keyValueKeyKind = localInfo.keyValueKeyKind;
          targetInfoOut.keyValueValueKind = localInfo.keyValueValueKind;
          if (targetInfoOut.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
              targetInfoOut.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
            return true;
          }
        }
      }
    }

    auto tryPopulateFromResolvedCallee = [&](const Definition *resolvedCallee) {
      if (resolvedCallee == nullptr) {
        return false;
      }
      std::string collectionName;
      std::vector<std::string> collectionArgs;
      if (!inferDeclaredReturnCollection(*resolvedCallee, collectionName, collectionArgs) ||
          collectionName != "map" ||
          collectionArgs.size() != 2) {
        return inferForwardedCollectionPairTypeInfo(
            *canonicalReceiverExpr, *resolvedCallee, localsIn, {}, targetInfoOut);
      }
      targetInfoOut.isKeyValueTarget = true;
      targetInfoOut.keyValueKeyKind = valueKindFromTypeName(collectionArgs.front());
      targetInfoOut.keyValueValueKind = valueKindFromTypeName(collectionArgs.back());
      return targetInfoOut.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
             targetInfoOut.keyValueValueKind != LocalInfo::ValueKind::Unknown;
    };

    if (tryPopulateFromResolvedCallee(resolveDefinitionCall(*canonicalReceiverExpr))) {
      return true;
    }

    if (isKeyValueArgsPackAccessCall(*canonicalReceiverExpr) &&
        canonicalReceiverExpr->args.front().kind != Expr::Kind::Name) {
      const Definition *keyValueAccessCallee = resolveDefinitionCall(*canonicalReceiverExpr);
      if (keyValueAccessCallee != nullptr &&
          !keyValueAccessCallee->parameters.empty()) {
        const std::string receiverTypeText =
            extractParameterTypeName(keyValueAccessCallee->parameters.front());
        if (inferKeyValueKindsFromArgsPackTypeText(receiverTypeText,
                                              targetInfoOut.keyValueKeyKind,
                                              targetInfoOut.keyValueValueKind)) {
          targetInfoOut.isKeyValueTarget = true;
          return true;
        }
      }
    }

    // Direct canonical insert calls on non-local field receivers already carry
    // explicit template arguments; use those to route the receiver through the
    // builtin rewrite path even when the receiver does not resolve as a
    // standalone call target (for example `holder.values`).
    if (nonLocalFieldReceiver != nullptr &&
        stmt.templateArgs.size() == 2) {
      targetInfoOut.isKeyValueTarget = true;
      targetInfoOut.keyValueKeyKind = valueKindFromTypeName(stmt.templateArgs.front());
      targetInfoOut.keyValueValueKind = valueKindFromTypeName(stmt.templateArgs.back());
      return targetInfoOut.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
             targetInfoOut.keyValueValueKind != LocalInfo::ValueKind::Unknown;
    }

    if (!stmt.isMethodCall &&
        nonLocalFieldReceiver != nullptr) {
      const Definition *directCallee = resolveDefinitionCall(stmt);
      if (directCallee != nullptr &&
          isKeyValueInsertLikeCallee(*directCallee) &&
          !directCallee->parameters.empty()) {
        const std::string receiverTypeText = extractParameterTypeName(directCallee->parameters.front());
        if (inferKeyValueKindsFromTypeText(receiverTypeText,
                                      targetInfoOut.keyValueKeyKind,
                                      targetInfoOut.keyValueValueKind)) {
          targetInfoOut.isKeyValueTarget = true;
          return true;
        }
        if (directCallee->parameters.size() >= 3) {
          const std::string keyTypeText = extractParameterTypeName(directCallee->parameters[1]);
          const std::string valueTypeText = extractParameterTypeName(directCallee->parameters[2]);
          const LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(keyTypeText));
          const LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(valueTypeText));
          if (keyKind != LocalInfo::ValueKind::Unknown &&
              valueKind != LocalInfo::ValueKind::Unknown) {
            targetInfoOut.isKeyValueTarget = true;
            targetInfoOut.keyValueKeyKind = keyKind;
            targetInfoOut.keyValueValueKind = valueKind;
            return true;
          }
        }
      }
    }

    if (stmt.isMethodCall &&
        nonLocalFieldReceiver != nullptr) {
      const Definition *methodCallee = resolveMethodCallDefinition(stmt, localsIn);
      if (methodCallee != nullptr &&
          isKeyValueInsertLikeCallee(*methodCallee) &&
          !methodCallee->parameters.empty()) {
        const std::string receiverTypeText = extractParameterTypeName(methodCallee->parameters.front());
        if (inferKeyValueKindsFromTypeText(receiverTypeText,
                                     targetInfoOut.keyValueKeyKind,
                                     targetInfoOut.keyValueValueKind)) {
          targetInfoOut.isKeyValueTarget = true;
          return true;
        }
        if (methodCallee->parameters.size() >= 3) {
          const std::string keyTypeText = extractParameterTypeName(methodCallee->parameters[1]);
          const std::string valueTypeText = extractParameterTypeName(methodCallee->parameters[2]);
          const LocalInfo::ValueKind keyKind = valueKindFromTypeName(trimTemplateTypeText(keyTypeText));
          const LocalInfo::ValueKind valueKind = valueKindFromTypeName(trimTemplateTypeText(valueTypeText));
          if (keyKind != LocalInfo::ValueKind::Unknown &&
              valueKind != LocalInfo::ValueKind::Unknown) {
            targetInfoOut.isKeyValueTarget = true;
            targetInfoOut.keyValueKeyKind = keyKind;
            targetInfoOut.keyValueValueKind = valueKind;
            return true;
          }
        }
      }
    }
    return false;
  };

  CollectionPairTypeInfo targetInfo;
  if (!inferCallKeyValueTargetInfo(stmt.args[receiverIndex], targetInfo)) {
    return false;
  }
  if (!targetInfo.isKeyValueTarget) {
    return false;
  }
  auto isExperimentalKeyValueStructPath = [](const std::string &structPath) {
    const std::string experimentalKeyValueType =
        experimentalCollectionTypePath("map", "Map");
    return structPath == experimentalKeyValueType ||
           structPath.rfind(experimentalKeyValueType + "__", 0) == 0;
  };
  if (targetInfo.isWrappedKeyValueTarget ||
      isExperimentalKeyValueStructPath(targetInfo.structTypeName)) {
    return false;
  }

  rewrittenStmt = stmt;
  rewrittenStmt.name = canonicalStatementKeyValueHelperPath("insert");
  if (rewrittenStmt.name.empty()) {
    return false;
  }
  rewrittenStmt.namespacePrefix.clear();
  rewrittenStmt.isMethodCall = false;
  rewrittenStmt.isFieldAccess = false;
  rewrittenStmt.semanticNodeId = 0;
  if (rewrittenStmt.templateArgs.empty() &&
      targetInfo.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
      targetInfo.keyValueValueKind != LocalInfo::ValueKind::Unknown) {
    rewrittenStmt.templateArgs = {
        typeNameForValueKind(targetInfo.keyValueKeyKind),
        typeNameForValueKind(targetInfo.keyValueValueKind),
    };
  }
  return true;
}

static DirectCallStatementEmitResult tryEmitVectorHelperCallFormStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::string &error) {
  if (stmt.kind != Expr::Kind::Call || stmt.isMethodCall) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  const bool isVectorMutatorCall =
      isVectorBuiltinName(stmt, "push") || isVectorBuiltinName(stmt, "pop") || isVectorBuiltinName(stmt, "reserve") ||
      isVectorBuiltinName(stmt, "clear") || isVectorBuiltinName(stmt, "remove_at") ||
      isVectorBuiltinName(stmt, "remove_swap");
  if (!isVectorMutatorCall || stmt.args.empty()) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  std::string explicitStdlibHelperName;
  const bool isExplicitStdlibVectorHelper =
      resolveStatementVectorHelperAliasName(stmt, explicitStdlibHelperName) &&
      isPublishedCanonicalStatementVectorHelperPath(
          resolveStatementCallPathWithoutFallbackProbes(stmt));

  std::vector<size_t> receiverIndices;
  auto appendReceiverIndex = [&](size_t index) {
    if (index >= stmt.args.size()) {
      return;
    }
    for (size_t existing : receiverIndices) {
      if (existing == index) {
        return;
      }
    }
    receiverIndices.push_back(index);
  };

  const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
  if (hasNamedArgs) {
    bool hasValuesNamedReceiver = false;
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
        appendReceiverIndex(i);
        hasValuesNamedReceiver = true;
      }
    }
    if (!hasValuesNamedReceiver) {
      appendReceiverIndex(0);
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    appendReceiverIndex(0);
  }

  const bool probePositionalReorderedReceiver =
      !hasNamedArgs && stmt.args.size() > 1 &&
      (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
       stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
       stmt.args.front().kind == Expr::Kind::Name);
  if (probePositionalReorderedReceiver) {
    for (size_t i = 1; i < stmt.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }

  for (size_t receiverIndex : receiverIndices) {
    Expr methodStmt = stmt;
    methodStmt.isMethodCall = true;
    std::string normalizedHelperName;
    if (resolveStatementVectorHelperAliasName(methodStmt, normalizedHelperName)) {
      methodStmt.name = normalizedHelperName;
    }
    if (receiverIndex != 0) {
      std::swap(methodStmt.args[0], methodStmt.args[receiverIndex]);
      if (methodStmt.argNames.size() < methodStmt.args.size()) {
        methodStmt.argNames.resize(methodStmt.args.size());
      }
      std::swap(methodStmt.argNames[0], methodStmt.argNames[receiverIndex]);
    }
    const std::string priorError = error;
    const Definition *callee = resolveMethodCallDefinition(methodStmt, localsIn);
    if (!callee) {
      error = priorError;
      continue;
    }
    if (methodStmt.hasBodyArguments || !methodStmt.bodyArguments.empty()) {
      error = "native backend does not support block arguments on calls";
      return DirectCallStatementEmitResult::Error;
    }
    if (!emitInlineDefinitionCall(methodStmt, *callee, localsIn, false)) {
      return DirectCallStatementEmitResult::Error;
    }
    error = priorError;
    return DirectCallStatementEmitResult::Emitted;
  }

  if (isExplicitStdlibVectorHelper &&
      (explicitStdlibHelperName == "clear" || explicitStdlibHelperName == "remove_at" ||
       explicitStdlibHelperName == "remove_swap")) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  return DirectCallStatementEmitResult::NotMatched;
}

} // namespace

BufferStoreStatementEmitResult tryEmitBufferStoreStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "buffer_store")) {
    return BufferStoreStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() != 3) {
    error = "buffer_store requires buffer, index, and value";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind elemKind = LocalInfo::ValueKind::Unknown;
  if (const auto resolvedKind =
          resolveBufferTargetElementKind(stmt.args[0], localsIn, semanticProgram, semanticIndex);
      resolvedKind.has_value()) {
    elemKind = *resolvedKind;
  }
  if (elemKind == LocalInfo::ValueKind::Unknown || elemKind == LocalInfo::ValueKind::String) {
    error = "buffer_store requires numeric/bool buffer";
    return BufferStoreStatementEmitResult::Error;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(
          stmt.args[1],
          localsIn,
          "buffer_store",
          inferExprKind,
          indexKind,
          error,
          semanticProgram,
          semanticIndex)) {
    error = "buffer_store requires integer index";
    return BufferStoreStatementEmitResult::Error;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(stmt.args[0], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal)});
  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal)});
  const int32_t valueLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return BufferStoreStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal)});
  instructions.push_back({pushOneForIndex(indexKind), 1});
  instructions.push_back({addForIndex(indexKind), 0});
  instructions.push_back({pushOneForIndex(indexKind), IrSlotBytesI32});
  instructions.push_back({mulForIndex(indexKind), 0});
  instructions.push_back({IrOpcode::AddI64, 0});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(valueLocal)});
  instructions.push_back({IrOpcode::StoreIndirect, 0});
  instructions.push_back({IrOpcode::Pop, 0});
  return BufferStoreStatementEmitResult::Emitted;
}

DispatchStatementEmitResult tryEmitDispatchStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<std::string(const Expr &)> &resolveExprPath,
    const std::function<const Definition *(const std::string &)> &findDefinitionByPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (stmt.kind != Expr::Kind::Call || !isSimpleCallName(stmt, "dispatch")) {
    return DispatchStatementEmitResult::NotMatched;
  }
  if (stmt.args.size() < 4) {
    error = "dispatch requires kernel and three dimension arguments";
    return DispatchStatementEmitResult::Error;
  }
  if (stmt.args.front().kind != Expr::Kind::Name) {
    error = "dispatch requires kernel name as first argument";
    return DispatchStatementEmitResult::Error;
  }

  const Expr &kernelExpr = stmt.args.front();
  const std::string kernelPath = resolveExprPath(kernelExpr);
  const Definition *kernelDef = findDefinitionByPath(kernelPath);
  if (!kernelDef) {
    error = "dispatch requires known kernel: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  bool isCompute = false;
  for (const auto &transform : kernelDef->transforms) {
    if (transform.name == "compute") {
      isCompute = true;
      break;
    }
  }
  if (!isCompute) {
    error = "dispatch requires compute definition: " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  for (size_t i = 1; i <= 3; ++i) {
    bool hasSemanticDimensionFact = false;
    const std::optional<LocalInfo::ValueKind> semanticDimKind =
        resolveStatementValueKindFromSemanticFacts(
            stmt.args[i], semanticProgram, semanticIndex, hasSemanticDimensionFact);
    const LocalInfo::ValueKind dimKind =
        semanticDimKind.has_value()
            ? *semanticDimKind
            : (hasSemanticDimensionFact ? LocalInfo::ValueKind::Unknown
                                        : inferExprKind(stmt.args[i], localsIn));
    if (dimKind != LocalInfo::ValueKind::Int32) {
      error = "dispatch requires i32 dimensions";
      return DispatchStatementEmitResult::Error;
    }
  }

  const bool hasTrailingArgsPack =
      !kernelDef->parameters.empty() && isArgsPackBinding(kernelDef->parameters.back());
  const size_t fixedKernelParamCount =
      hasTrailingArgsPack ? kernelDef->parameters.size() - 1 : kernelDef->parameters.size();
  const size_t minDispatchArgs = fixedKernelParamCount + 4;
  if ((!hasTrailingArgsPack && stmt.args.size() != minDispatchArgs) ||
      (hasTrailingArgsPack && stmt.args.size() < minDispatchArgs)) {
    error = "dispatch argument count mismatch for " + kernelPath;
    return DispatchStatementEmitResult::Error;
  }

  const int32_t gxLocal = allocTempLocal();
  if (!emitExpr(stmt.args[1], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gxLocal)});
  const int32_t gyLocal = allocTempLocal();
  if (!emitExpr(stmt.args[2], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gyLocal)});
  const int32_t gzLocal = allocTempLocal();
  if (!emitExpr(stmt.args[3], localsIn)) {
    return DispatchStatementEmitResult::Error;
  }
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gzLocal)});

  const int32_t xLocal = allocTempLocal();
  const int32_t yLocal = allocTempLocal();
  const int32_t zLocal = allocTempLocal();
  const int32_t gidXLocal = allocTempLocal();
  const int32_t gidYLocal = allocTempLocal();
  const int32_t gidZLocal = allocTempLocal();

  LocalMap dispatchLocals = localsIn;
  LocalInfo gidInfo;
  gidInfo.index = gidXLocal;
  gidInfo.kind = LocalInfo::Kind::Value;
  gidInfo.valueKind = LocalInfo::ValueKind::Int32;
  dispatchLocals.emplace(kGpuGlobalIdXName, gidInfo);
  gidInfo.index = gidYLocal;
  dispatchLocals.emplace(kGpuGlobalIdYName, gidInfo);
  gidInfo.index = gidZLocal;
  dispatchLocals.emplace(kGpuGlobalIdZName, gidInfo);

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  const size_t zCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gzLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t zJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  const size_t yCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gyLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t yJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::PushI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  const size_t xCheck = instructions.size();
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(gxLocal)});
  instructions.push_back({IrOpcode::CmpLtI32, 0});
  const size_t xJumpEnd = instructions.size();
  instructions.push_back({IrOpcode::JumpIfZero, 0});

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidXLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidYLocal)});
  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(gidZLocal)});

  Expr kernelCall;
  kernelCall.kind = Expr::Kind::Call;
  kernelCall.name = kernelPath;
  kernelCall.namespacePrefix = kernelExpr.namespacePrefix;
  for (size_t i = 4; i < stmt.args.size(); ++i) {
    kernelCall.args.push_back(stmt.args[i]);
    kernelCall.argNames.push_back(std::nullopt);
  }
  if (!emitInlineDefinitionCall(kernelCall, *kernelDef, dispatchLocals, false)) {
    return DispatchStatementEmitResult::Error;
  }

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(xLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(xCheck)});
  const size_t xEnd = instructions.size();
  instructions[xJumpEnd].imm = static_cast<int32_t>(xEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(yLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(yCheck)});
  const size_t yEnd = instructions.size();
  instructions[yJumpEnd].imm = static_cast<int32_t>(yEnd);

  instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::PushI32, 1});
  instructions.push_back({IrOpcode::AddI32, 0});
  instructions.push_back({IrOpcode::StoreLocal, static_cast<uint64_t>(zLocal)});
  instructions.push_back({IrOpcode::Jump, static_cast<uint64_t>(zCheck)});
  const size_t zEnd = instructions.size();
  instructions[zJumpEnd].imm = static_cast<int32_t>(zEnd);

  return DispatchStatementEmitResult::Emitted;
}

DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (stmt.kind != Expr::Kind::Call) {
    return DirectCallStatementEmitResult::NotMatched;
  }

  bool explicitVectorMutatorHelperCall = isExplicitVectorMutatorHelperCall(stmt);
  auto explicitVectorHelperUsesBuiltinVectorReceiver = [&](const Expr &callExpr) {
    if (!explicitVectorMutatorHelperCall || callExpr.args.empty()) {
      return false;
    }
    const size_t receiverIndex = explicitVectorHelperReceiverIndex(callExpr);
    ArrayVectorAccessTargetInfo semanticTargetInfo;
    bool hasSemanticVectorFact = false;
    if (receiverIndex < callExpr.args.size() &&
        resolveStatementVectorReceiverTargetInfoFromSemanticFacts(
            callExpr.args[receiverIndex],
            semanticProgram,
            semanticIndex,
            semanticTargetInfo,
            hasSemanticVectorFact)) {
      return true;
    }
    if (hasSemanticVectorFact) {
      return false;
    }
    const auto targetInfo =
        resolveArrayVectorAccessTargetInfo(callExpr.args[receiverIndex],
                                           localsIn,
                                           {},
                                           semanticProgram,
                                           semanticIndex);
    return targetInfo.isVectorTarget;
  };
  auto isBuiltinVectorReceiverExpr = [&](const Expr &candidate) {
    ArrayVectorAccessTargetInfo semanticTargetInfo;
    bool hasSemanticVectorFact = false;
    if (resolveStatementVectorReceiverTargetInfoFromSemanticFacts(candidate,
                                                                 semanticProgram,
                                                                 semanticIndex,
                                                                 semanticTargetInfo,
                                                                 hasSemanticVectorFact)) {
      return true;
    }
    if (hasSemanticVectorFact) {
      return false;
    }
    const auto targetInfo =
        resolveArrayVectorAccessTargetInfo(candidate,
                                           localsIn,
                                           {},
                                           semanticProgram,
                                           semanticIndex);
    if (targetInfo.isVectorTarget) {
      return true;
    }
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto localIt = localsIn.find(candidate.name);
    return localIt != localsIn.end() &&
           (localIt->second.kind == LocalInfo::Kind::Vector ||
            isExperimentalVectorTypePath(localIt->second.structTypeName));
  };
  auto emitVectorHeaderFieldAddress = [&](const Expr &receiver, int32_t slotOffset) {
    if (receiver.kind == Expr::Kind::Name) {
      auto localIt = localsIn.find(receiver.name);
      if (localIt != localsIn.end() &&
          (localIt->second.kind == LocalInfo::Kind::Reference ||
           localIt->second.kind == LocalInfo::Kind::Pointer) &&
          !localIt->second.structTypeName.empty()) {
        instructions.push_back({IrOpcode::LoadLocal, static_cast<uint64_t>(localIt->second.index)});
      } else if (!emitExpr(receiver, localsIn)) {
        return false;
      }
    } else if (!emitExpr(receiver, localsIn)) {
      return false;
    }
    if (slotOffset != 0) {
      instructions.push_back({IrOpcode::PushI64, static_cast<uint64_t>(slotOffset) * IrSlotBytes});
      instructions.push_back({IrOpcode::AddI64, 0});
    }
    return true;
  };
  auto emitVectorHeaderFieldLoad = [&](const Expr &receiver, int32_t slotOffset) {
    if (!emitVectorHeaderFieldAddress(receiver, slotOffset)) {
      return false;
    }
    instructions.push_back({IrOpcode::LoadIndirect, 0});
    return true;
  };
  auto emitBoundsTrapIfStackTrue = [&]() {
    const size_t okJump = instructions.size();
    instructions.push_back({IrOpcode::JumpIfZero, 0});
    emitArrayIndexOutOfBounds();
    instructions[okJump].imm = instructions.size();
  };
  auto isInternalSoaMetadataSetterCallee = [](const Definition &callee) {
    return callee.fullPath.rfind(
               "/std/collections/internal_soa_storage/SoaColumn", 0) == 0 ||
           callee.fullPath.rfind(
               "/std/collections/internal_soa_storage/SoaFieldView", 0) == 0;
  };
  auto isInternalVectorMetadataSetterCallee = [](const Definition &callee) {
    return isExperimentalVectorTypePath(callee.fullPath);
  };
  auto metadataCountSlotOffset = [&](const Definition &callee) {
    return isInternalVectorMetadataSetterCallee(callee) ? 0 : 1;
  };
  auto metadataCapacitySlotOffset = [&](const Definition &callee) {
    return isInternalVectorMetadataSetterCallee(callee) ? 1 : 2;
  };
  auto rewriteBareVectorMethodMutatorToDirectCall = [&](const Expr &callExpr, Expr &rewrittenExpr) {
    if (callExpr.kind != Expr::Kind::Call || !callExpr.isMethodCall || callExpr.args.empty() ||
        !callExpr.namespacePrefix.empty() || callExpr.name.find('/') != std::string::npos) {
      return false;
    }
    const std::string helperName = callExpr.name;
    if (helperName != "push" && helperName != "pop" && helperName != "reserve" &&
        helperName != "clear" && helperName != "remove_at" && helperName != "remove_swap") {
      return false;
    }
    const size_t expectedArgCount =
        (helperName == "pop" || helperName == "clear") ? 1u : 2u;
    if (callExpr.args.size() != expectedArgCount) {
      return false;
    }
    if (!isBuiltinVectorReceiverExpr(callExpr.args.front())) {
      return false;
    }
    rewrittenExpr = callExpr;
    rewrittenExpr.isMethodCall = false;
    rewrittenExpr.namespacePrefix.clear();
    rewrittenExpr.name = helperName;
    return true;
  };
  Expr directStmt = stmt;
  Expr rewrittenKeyValueInsertStmt;
  if (rewriteKeyValueInsertHelperStatementToCanonical(
          stmt,
          localsIn,
          resolveMethodCallDefinition,
          resolveDefinitionCall,
          semanticProgram,
          semanticIndex,
          rewrittenKeyValueInsertStmt)) {
    directStmt = rewrittenKeyValueInsertStmt;
  }
  bool rewrittenExplicitVectorMutatorToBuiltinCall = false;
  bool rewrittenBareVectorMutatorToBuiltinCall = false;
  Expr rewrittenBareVectorMethodStmt;
  if (!explicitVectorMutatorHelperCall &&
      rewriteBareVectorMethodMutatorToDirectCall(stmt, rewrittenBareVectorMethodStmt)) {
    directStmt = rewrittenBareVectorMethodStmt;
    rewrittenBareVectorMutatorToBuiltinCall = true;
  }
  if (explicitVectorMutatorHelperCall &&
      explicitVectorHelperUsesBuiltinVectorReceiver(directStmt)) {
    std::string helperName;
    if (resolveStatementVectorHelperAliasName(directStmt, helperName)) {
      const size_t receiverIndex = explicitVectorHelperReceiverIndex(directStmt);
      directStmt.name = helperName;
      directStmt.namespacePrefix.clear();
      directStmt.templateArgs.clear();
      if (receiverIndex < directStmt.args.size() && receiverIndex != 0) {
        std::swap(directStmt.args[0], directStmt.args[receiverIndex]);
        if (directStmt.argNames.size() < directStmt.args.size()) {
          directStmt.argNames.resize(directStmt.args.size());
        }
        std::swap(directStmt.argNames[0], directStmt.argNames[receiverIndex]);
      }
      if (hasNamedArguments(directStmt.argNames)) {
        directStmt.argNames.clear();
      }
      explicitVectorMutatorHelperCall = false;
      rewrittenExplicitVectorMutatorToBuiltinCall = true;
    }
  }
  if (rewrittenExplicitVectorMutatorToBuiltinCall) {
    const size_t beforeExpr = instructions.size();
    if (!emitExpr(directStmt, localsIn)) {
      return DirectCallStatementEmitResult::Error;
    }
    if (instructions.size() > beforeExpr &&
        instructions.back().op == IrOpcode::PushI32 &&
        instructions.back().imm == 0) {
      instructions.push_back({IrOpcode::Pop, 0});
    }
    return DirectCallStatementEmitResult::Emitted;
  }
  if (rewrittenBareVectorMutatorToBuiltinCall) {
    const size_t beforeExpr = instructions.size();
    if (!emitExpr(directStmt, localsIn)) {
      return DirectCallStatementEmitResult::Error;
    }
    if (instructions.size() > beforeExpr &&
        instructions.back().op == IrOpcode::PushI32 &&
        instructions.back().imm == 0) {
      instructions.push_back({IrOpcode::Pop, 0});
    }
    return DirectCallStatementEmitResult::Emitted;
  }

  if (!directStmt.isMethodCall && directStmt.args.size() == 2) {
	if (const Definition *callee = resolveDefinitionCall(directStmt);
        callee != nullptr &&
        (isSimpleCallName(directStmt, "set_field_count") ||
         isSimpleCallName(directStmt, "set_field_capacity")) &&
        (isInternalSoaMetadataSetterCallee(*callee) ||
         isInternalVectorMetadataSetterCallee(*callee))) {
      const Expr &receiver = directStmt.args.front();
      const Expr &value = directStmt.args[1];
      const int32_t countSlotOffset = metadataCountSlotOffset(*callee);
      const int32_t capacitySlotOffset = metadataCapacitySlotOffset(*callee);
      if (!emitExpr(value, localsIn)) {
        return DirectCallStatementEmitResult::Error;
      }
      instructions.push_back({IrOpcode::PushI32, 0});
      instructions.push_back({IrOpcode::CmpLtI32, 0});
      emitBoundsTrapIfStackTrue();

      if (isSimpleCallName(directStmt, "set_field_count")) {
        if (!emitExpr(value, localsIn) ||
            !emitVectorHeaderFieldLoad(receiver, capacitySlotOffset)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitVectorHeaderFieldAddress(receiver, countSlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
      } else {
        if (!emitVectorHeaderFieldLoad(receiver, countSlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::PushI32, 1073741823});
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitVectorHeaderFieldAddress(receiver, capacitySlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
      }
      instructions.push_back({IrOpcode::StoreIndirect, 0});
      instructions.push_back({IrOpcode::Pop, 0});
      return DirectCallStatementEmitResult::Emitted;
    }
  }

  if (!explicitVectorMutatorHelperCall &&
      !directStmt.isMethodCall &&
      directStmt.args.size() == 1 &&
      isSoaVectorTargetExpr(directStmt.args.front(), localsIn, semanticProgram, semanticIndex)) {
    Expr methodStmt = directStmt;
    methodStmt.isMethodCall = true;
    const std::string priorError = error;
    if (const Definition *callee = resolveMethodCallDefinition(methodStmt, localsIn)) {
      if (methodStmt.hasBodyArguments || !methodStmt.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return DirectCallStatementEmitResult::Error;
      }
      if (!emitInlineDefinitionCall(methodStmt, *callee, localsIn, false)) {
        return DirectCallStatementEmitResult::Error;
      }
      error = priorError;
      return DirectCallStatementEmitResult::Emitted;
    }
    error = priorError;
  }

  if (directStmt.isMethodCall) {
    const bool isBuiltinCountLikeMethod =
        isArrayCountCall(directStmt, localsIn) || isStringCountCall(directStmt, localsIn) ||
        isVectorCapacityCall(directStmt, localsIn);
    const bool isVectorMetadataSetterMethod =
        directStmt.args.size() == 2 &&
        (isSimpleCallName(directStmt, "set_field_count") ||
         isSimpleCallName(directStmt, "set_field_capacity"));
    auto isStructDefinitionCallee = [](const Definition &definition) {
      for (const auto &transform : definition.transforms) {
        if (transform.name == "struct") {
          return true;
        }
      }
      return false;
    };
    const std::string priorError = error;
    const Definition *callee = resolveMethodCallDefinition(directStmt, localsIn);
    if (callee != nullptr && isVectorMetadataSetterMethod &&
        isStructDefinitionCallee(*callee) &&
        !isInternalVectorMetadataSetterCallee(*callee) &&
        !isInternalSoaMetadataSetterCallee(*callee)) {
      error = priorError;
      return DirectCallStatementEmitResult::NotMatched;
    }
    if (!callee && !isBuiltinCountLikeMethod) {
      if (isVectorMetadataSetterMethod) {
        error = priorError;
        return DirectCallStatementEmitResult::NotMatched;
      }
      return DirectCallStatementEmitResult::Error;
    }
    if (callee) {
      const bool isInternalMetadataSetter =
          directStmt.args.size() == 2 &&
          (isSimpleCallName(directStmt, "set_field_count") ||
           isSimpleCallName(directStmt, "set_field_capacity")) &&
          (isInternalSoaMetadataSetterCallee(*callee) ||
           isInternalVectorMetadataSetterCallee(*callee));
      if (isInternalMetadataSetter) {
        const Expr &receiver = directStmt.args.front();
        const Expr &value = directStmt.args[1];
        const int32_t countSlotOffset = metadataCountSlotOffset(*callee);
        const int32_t capacitySlotOffset = metadataCapacitySlotOffset(*callee);
        if (!emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::PushI32, 0});
        instructions.push_back({IrOpcode::CmpLtI32, 0});
        emitBoundsTrapIfStackTrue();

        if (isSimpleCallName(directStmt, "set_field_count")) {
          if (!emitExpr(value, localsIn) ||
              !emitVectorHeaderFieldLoad(receiver, capacitySlotOffset)) {
            return DirectCallStatementEmitResult::Error;
          }
          instructions.push_back({IrOpcode::CmpGtI32, 0});
          emitBoundsTrapIfStackTrue();
          if (!emitVectorHeaderFieldAddress(receiver, countSlotOffset) ||
              !emitExpr(value, localsIn)) {
            return DirectCallStatementEmitResult::Error;
          }
          instructions.push_back({IrOpcode::StoreIndirect, 0});
          instructions.push_back({IrOpcode::Pop, 0});
          error = priorError;
          return DirectCallStatementEmitResult::Emitted;
        }

        if (!emitVectorHeaderFieldLoad(receiver, countSlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::PushI32, 1073741823});
        instructions.push_back({IrOpcode::CmpGtI32, 0});
        emitBoundsTrapIfStackTrue();
        if (!emitVectorHeaderFieldAddress(receiver, capacitySlotOffset) ||
            !emitExpr(value, localsIn)) {
          return DirectCallStatementEmitResult::Error;
        }
        instructions.push_back({IrOpcode::StoreIndirect, 0});
        instructions.push_back({IrOpcode::Pop, 0});
        error = priorError;
        return DirectCallStatementEmitResult::Emitted;
      }
      if (directStmt.hasBodyArguments || !directStmt.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return DirectCallStatementEmitResult::Error;
      }
      if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
        return DirectCallStatementEmitResult::Error;
      }
      error = priorError;
      return DirectCallStatementEmitResult::Emitted;
    }
    error.clear();
  }

  const auto vectorHelperCallFormResult = tryEmitVectorHelperCallFormStatement(
      directStmt, localsIn, resolveMethodCallDefinition, emitInlineDefinitionCall, error);
  if (vectorHelperCallFormResult == DirectCallStatementEmitResult::Error) {
    return DirectCallStatementEmitResult::Error;
  }
  if (vectorHelperCallFormResult == DirectCallStatementEmitResult::Emitted) {
    return DirectCallStatementEmitResult::Emitted;
  }

  const std::string priorError = error;
  const Definition *callee = resolveDefinitionCall(directStmt);
  if (!callee) {
    return DirectCallStatementEmitResult::NotMatched;
  }
  if (directStmt.hasBodyArguments || !directStmt.bodyArguments.empty()) {
    error = "native backend does not support block arguments on calls";
    return DirectCallStatementEmitResult::Error;
  }

  ReturnInfo info;
  if (!getReturnInfo(callee->fullPath, info)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!emitInlineDefinitionCall(directStmt, *callee, localsIn, false)) {
    return DirectCallStatementEmitResult::Error;
  }
  if (!info.returnsVoid) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  error = priorError;
  return DirectCallStatementEmitResult::Emitted;
}

DirectCallStatementEmitResult tryEmitDirectCallStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &, bool)> &emitInlineDefinitionCall,
    std::vector<IrInstruction> &instructions,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  return tryEmitDirectCallStatement(stmt,
                                    localsIn,
                                    isArrayCountCall,
                                    isStringCountCall,
                                    isVectorCapacityCall,
                                    emitExpr,
                                    resolveMethodCallDefinition,
                                    resolveDefinitionCall,
                                    getReturnInfo,
                                    emitInlineDefinitionCall,
                                    []() {},
                                    instructions,
                                    error,
                                    semanticProgram,
                                    semanticIndex);
}

AssignOrExprStatementEmitResult emitAssignOrExprStatementWithPop(
    const Expr &stmt,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    std::vector<IrInstruction> &instructions) {
  if (stmt.kind == Expr::Kind::Call && isSimpleCallName(stmt, "assign")) {
    if (!emitExpr(stmt, localsIn)) {
      return AssignOrExprStatementEmitResult::Error;
    }
    instructions.push_back({IrOpcode::Pop, 0});
    return AssignOrExprStatementEmitResult::Emitted;
  }
  if (!emitExpr(stmt, localsIn)) {
    return AssignOrExprStatementEmitResult::Error;
  }
  if (!isFreeMemoryIntrinsicCall(stmt)) {
    instructions.push_back({IrOpcode::Pop, 0});
  }
  return AssignOrExprStatementEmitResult::Emitted;
}

} // namespace primec::ir_lowerer
