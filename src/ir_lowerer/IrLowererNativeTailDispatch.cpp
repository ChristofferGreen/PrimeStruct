#include "IrLowererCallHelpers.h"

#include "IrLowererCountAccessClassifiers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererBindingTypeHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace primec::ir_lowerer {
using count_access_detail::isVectorCountTarget;

namespace {

std::string resolveNativeTailCallPathWithoutFallbackProbes(const Expr &expr) {
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

const StdlibSurfaceMetadata *nativeTailVectorHelperMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
}

bool semanticSurfaceMatches(std::optional<StdlibSurfaceId> surfaceId,
                            const StdlibSurfaceMetadata *metadata) {
  return surfaceId.has_value() && metadata != nullptr && *surfaceId == metadata->id;
}

bool resolvePublishedNativeTailHelperName(const SemanticProgram *semanticProgram,
                                          const Expr &expr,
                                          StdlibSurfaceId surfaceId,
                                          std::string &helperNameOut) {
  return resolvePublishedSemanticStdlibSurfaceMemberName(
             semanticProgram, expr, surfaceId, helperNameOut) ||
         resolvePublishedStdlibSurfaceMemberName(
             resolveNativeTailCallPathWithoutFallbackProbes(expr),
             surfaceId,
             helperNameOut);
}

bool resolvePublishedNativeTailVectorHelperName(const SemanticProgram *semanticProgram,
                                                const Expr &expr,
                                                std::string &helperNameOut) {
  const auto *metadata = nativeTailVectorHelperMetadata();
  return metadata != nullptr &&
         resolvePublishedNativeTailHelperName(semanticProgram, expr, metadata->id, helperNameOut);
}

bool isCanonicalPublishedNativeTailVectorHelperPath(std::string_view path) {
  const auto *metadata = nativeTailVectorHelperMetadata();
  return metadata != nullptr &&
         isCanonicalPublishedStdlibSurfaceHelperPath(path, metadata->id);
}

bool semanticDirectCallMatchesVectorHelperSurface(const SemanticProgram *semanticProgram,
                                                  const Expr &expr) {
  return semanticSurfaceMatches(findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr),
                                nativeTailVectorHelperMetadata());
}

bool isExplicitDirectMapCountContainsTryAtCall(const SemanticProgram *semanticProgram,
                                               const Expr &expr) {
  if (expr.isMethodCall || expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string helperName;
  return resolvePublishedNativeTailHelperName(
             semanticProgram,
             expr,
             StdlibSurfaceId::CollectionsMapHelpers,
             helperName) &&
         (helperName == "count" || helperName == "contains" || helperName == "tryAt");
}

bool shouldPreserveExplicitDirectMapHelperCall(
    const SemanticProgram *semanticProgram,
    const Expr &expr,
    const MapAccessTargetInfo &mapTargetInfo) {
  if (!isExplicitDirectMapCountContainsTryAtCall(semanticProgram, expr)) {
    return false;
  }
  return expr.args.empty() ||
         expr.args.front().kind != Expr::Kind::Call ||
         !mapTargetInfo.isMapTarget ||
         mapTargetInfo.isWrappedMapTarget;
}

bool isMapReadHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref";
}

bool importPathCoversNativeTailTarget(const std::string &importPath,
                                      const std::string &targetPath) {
  if (importPath.empty() || importPath.front() != '/') {
    return false;
  }
  if (importPath == targetPath) {
    return true;
  }
  if (importPath.size() >= 2 &&
      importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
    const std::string prefix = importPath.substr(0, importPath.size() - 2);
    return targetPath == prefix || targetPath.rfind(prefix + "/", 0) == 0;
  }
  return false;
}

bool hasSemanticMapReadHelperDefinition(const SemanticProgram *semanticProgram,
                                        std::string_view helperName) {
  if (semanticProgram == nullptr || !isMapReadHelperName(helperName)) {
    return false;
  }
  const std::string helperPath =
      "/std/collections/map/" + std::string(helperName);
  for (const auto &definition : semanticProgram->definitions) {
    if (definition.fullPath == helperPath ||
        definition.fullPath.rfind(helperPath + "__", 0) == 0) {
      return true;
    }
  }
  auto importsMapReadHelper = [&](const std::vector<std::string> &imports) {
    for (const std::string &importPath : imports) {
      if (importPathCoversNativeTailTarget(importPath, helperPath)) {
        return true;
      }
    }
    return false;
  };
  return importsMapReadHelper(semanticProgram->sourceImports) ||
         importsMapReadHelper(semanticProgram->imports);
}

bool isExplicitDirectVectorCountCall(const SemanticProgram *semanticProgram,
                                     const Expr &expr) {
  if (expr.isMethodCall || expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string helperName;
  if (!resolvePublishedNativeTailVectorHelperName(semanticProgram, expr, helperName) ||
      helperName != "count") {
    return false;
  }
  return true;
}

bool isNamedArgumentVectorTemporary(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || !hasNamedArguments(expr.argNames)) {
    return false;
  }
  std::string collectionName;
  return getBuiltinCollectionName(expr, collectionName) &&
         collectionName == "vector";
}

bool isExplicitDirectSoaAccessCall(const Expr &expr) {
  if (expr.isMethodCall || expr.kind != Expr::Kind::Call) {
    return false;
  }
  const std::string rawPath = resolveNativeTailCallPathWithoutFallbackProbes(expr);
  return rawPath == "/soa_vector/get" ||
         rawPath == "/std/collections/soa_vector/get" ||
         rawPath == "/soa_vector/get_ref" ||
         rawPath == "/std/collections/soa_vector/get_ref";
}

bool hasSemanticMapAccessHelperDefinition(const SemanticProgram *semanticProgram,
                                          std::string_view accessName) {
  if (semanticProgram == nullptr ||
      (accessName != "at" && accessName != "at_unsafe")) {
    return false;
  }
  auto importsMapHelpers = [](const std::vector<std::string> &imports) {
    for (const std::string &importPath : imports) {
      if (importPath == "/std/collections/*" ||
          importPath == "/std/collections/map/*") {
        return true;
      }
    }
    return false;
  };
  if (importsMapHelpers(semanticProgram->sourceImports) ||
      importsMapHelpers(semanticProgram->imports)) {
    return true;
  }
  for (const auto &definition : semanticProgram->definitions) {
    std::string path = definition.fullPath;
    const size_t leafStart = path.find_last_of('/');
    const size_t generatedSuffix =
        path.find("__", leafStart == std::string::npos ? 0 : leafStart + 1);
    if (generatedSuffix != std::string::npos) {
      path.erase(generatedSuffix);
    }
    const std::string canonicalPath =
        "/std/collections/map/" + std::string(accessName);
    if (path == canonicalPath ||
        path == canonicalPath + "_ref") {
      return true;
    }
  }
  return false;
}

bool semanticMapAccessHelperKeepsBuiltinReturn(
    const SemanticProgram *semanticProgram,
    std::string_view helperPath) {
  if (semanticProgram == nullptr || helperPath.empty()) {
    return true;
  }
  const auto pathId =
      semanticProgramLookupCallTargetStringId(*semanticProgram, helperPath);
  if (!pathId.has_value()) {
    return true;
  }
  const SemanticProgramReturnFact *returnFact =
      semanticProgramLookupPublishedReturnFactByDefinitionPathId(
          *semanticProgram, *pathId);
  if (returnFact == nullptr) {
    return true;
  }
  std::string structPath =
      returnFact->structPathId != InvalidSymbolId
          ? std::string(semanticProgramResolveCallTargetString(
                *semanticProgram, returnFact->structPathId))
          : returnFact->structPath;
  structPath = trimTemplateTypeText(structPath);
  if (!structPath.empty() && structPath.front() == '/') {
    structPath.erase(structPath.begin());
  }
  if (structPath.empty()) {
    return true;
  }
  return structPath == "map" || structPath == "vector" ||
         structPath == "array";
}

const SemanticProgramQueryFact *findSourceMapAccessAliasQueryFact(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    const Expr &expr) {
  if (semanticProgram == nullptr) {
    return nullptr;
  }
  if (semanticIndex != nullptr) {
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndex, expr);
        queryFact != nullptr) {
      return queryFact;
    }
  }
  std::vector<std::pair<int, int>> sourcePositions;
  if (expr.sourceLine != 0 && expr.sourceColumn != 0) {
    sourcePositions.emplace_back(expr.sourceLine, expr.sourceColumn);
  }
  if (!expr.args.empty() && expr.args.front().sourceLine != 0 &&
      expr.args.front().sourceColumn != 0) {
    sourcePositions.emplace_back(expr.args.front().sourceLine,
                                 expr.args.front().sourceColumn);
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
        queryFact.callNameId != InvalidSymbolId
            ? semanticProgramResolveCallTargetString(*semanticProgram,
                                                     queryFact.callNameId)
            : std::string_view(queryFact.callName);
    if (callName == expr.name ||
        (!expr.sourceName.empty() && callName == expr.sourceName)) {
      return &queryFact;
    }
  }
  return nullptr;
}

bool isStringReturningMapAccessAlias(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    const Expr &expr) {
  const auto *queryFact =
      findSourceMapAccessAliasQueryFact(semanticProgram, semanticIndex, expr);
  if (queryFact == nullptr || queryFact->resolvedPathId == InvalidSymbolId) {
    return false;
  }
  const std::string resolvedPath =
      std::string(semanticProgramResolveCallTargetString(
          *semanticProgram, queryFact->resolvedPathId));
  if (resolvedPath != "/at" && resolvedPath != "/at_unsafe") {
    return false;
  }
  const std::string queryType = trimTemplateTypeText(std::string(
      queryFact->queryTypeTextId != InvalidSymbolId
          ? semanticProgramResolveCallTargetString(*semanticProgram,
                                                   queryFact->queryTypeTextId)
          : std::string_view(queryFact->queryTypeText)));
  const std::string bindingType = trimTemplateTypeText(std::string(
      queryFact->bindingTypeTextId != InvalidSymbolId
          ? semanticProgramResolveCallTargetString(*semanticProgram,
                                                   queryFact->bindingTypeTextId)
          : std::string_view(queryFact->bindingTypeText)));
  return queryType == "string" || queryType == "/string" ||
         bindingType == "string" || bindingType == "/string";
}

bool hasExplicitStdMapSourceSpelling(const Expr &expr) {
  const auto hasStdMapPrefix = [](std::string_view text) {
    return text.rfind("/std/collections/map/", 0) == 0 ||
           text.rfind("std/collections/map/", 0) == 0;
  };
  return hasStdMapPrefix(expr.name) || hasStdMapPrefix(expr.namespacePrefix);
}

} // namespace

bool isMapContainsHelperName(const Expr &expr);
bool isMapTryAtHelperName(const Expr &expr);
bool isVectorTarget(const Expr &expr, const LocalMap &localsIn);
bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn);
MapAccessLookupEmitResult tryEmitMapContainsLookup(
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitMapLookupTryAt(
    LocalInfo::ValueKind mapKeyKind,
    const std::string &mapStructTypeName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);

std::string resolveNativeTailSemanticText(const SemanticProgram &semanticProgram,
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

std::optional<bool> classifyNativeTailVectorTargetType(std::string typeText) {
  typeText = trimTemplateTypeText(std::move(typeText));
  if (typeText.empty()) {
    return std::nullopt;
  }

  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(typeText, base, argText)) {
    const std::string normalizedType =
        normalizeCollectionBindingTypeName(typeText);
    if (normalizedType == "vector") {
      return true;
    }
    return false;
  }

  base = normalizeCollectionBindingTypeName(trimTemplateTypeText(base));
  if (base == "Reference" || base == "Pointer") {
    std::vector<std::string> args;
    if (!splitTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    return classifyNativeTailVectorTargetType(args.front());
  }
  return base == "vector";
}

std::optional<bool> classifyNativeTailSemanticVectorTarget(
    const Expr &target,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  if (semanticProgram == nullptr ||
      semanticIndex == nullptr ||
      target.semanticNodeId == 0) {
    return std::nullopt;
  }
  if (const auto *collectionFact =
          findSemanticProductCollectionSpecialization(*semanticIndex, target)) {
    const std::string family = normalizeCollectionBindingTypeName(
        resolveNativeTailSemanticText(*semanticProgram,
                                      collectionFact->collectionFamilyId,
                                      collectionFact->collectionFamily));
    return family == "vector";
  }
  if (const auto *bindingFact =
          findSemanticProductBindingFact(*semanticIndex, target)) {
    return classifyNativeTailVectorTargetType(
        resolveNativeTailSemanticText(*semanticProgram,
                                      bindingFact->bindingTypeTextId,
                                      bindingFact->bindingTypeText));
  }
  if (const auto *localAutoFact =
          findSemanticProductLocalAutoFact(semanticProgram, *semanticIndex, target)) {
    return classifyNativeTailVectorTargetType(
        resolveNativeTailSemanticText(*semanticProgram,
                                      localAutoFact->bindingTypeTextId,
                                      localAutoFact->bindingTypeText));
  }
  if (const auto *queryFact =
          findSemanticProductQueryFact(semanticProgram, *semanticIndex, target)) {
    const auto classifyQueryType =
        [&](SymbolId typeId, const std::string &typeText) {
          return classifyNativeTailVectorTargetType(
              resolveNativeTailSemanticText(*semanticProgram, typeId, typeText));
        };
    if (auto classified =
            classifyQueryType(queryFact->queryTypeTextId, queryFact->queryTypeText);
        classified.has_value()) {
      return classified;
    }
    if (auto classified =
            classifyQueryType(queryFact->bindingTypeTextId, queryFact->bindingTypeText);
        classified.has_value()) {
      return classified;
    }
    if (auto classified = classifyQueryType(queryFact->receiverBindingTypeTextId,
                                            queryFact->receiverBindingTypeText);
        classified.has_value()) {
      return classified;
    }
    return false;
  }
  return std::nullopt;
}

UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnosticImpl(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  const auto isBareSimpleCountLikeCall = [&](std::string_view helperName) {
    return expr.kind == Expr::Kind::Call &&
           !expr.isMethodCall &&
           expr.namespacePrefix.empty() &&
           isSimpleCallName(expr, std::string(helperName).c_str()) &&
           expr.args.size() == 1;
  };
  const auto isDiagnosticVectorTarget = [&](const Expr &target) {
    if (const auto semanticTarget =
            classifyNativeTailSemanticVectorTarget(target, semanticProgram, semanticIndex);
        semanticTarget.has_value()) {
      return *semanticTarget;
    }
    return isVectorTarget(target, localsIn);
  };
  const auto isDiagnosticPublishedVectorMetadataTarget = [&](const Expr &target) {
    if (const auto semanticTarget =
            classifyNativeTailSemanticVectorTarget(target, semanticProgram, semanticIndex);
        semanticTarget.has_value()) {
      return *semanticTarget;
    }
    return isVectorTarget(target, localsIn) || isSoaVectorTarget(target, localsIn);
  };
  const auto diagnosticTargetName = [&]() {
    if (expr.args.empty()) {
      return std::string("<none>");
    }
    if (!expr.args.front().name.empty()) {
      return expr.args.front().name;
    }
    return std::string("<expr>");
  };
  std::string publishedVectorHelperName;
  const std::string directHelperPath =
      resolveNativeTailCallPathWithoutFallbackProbes(expr);
  const auto isRemovedCollectionMetadataAlias = [&](std::string_view helperName) {
    if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
      return false;
    }
    auto matchesCollectionRoot = [&](std::string_view collectionName) {
      const std::string unrooted =
          std::string(collectionName) + "/" + std::string(helperName);
      return directHelperPath == unrooted || directHelperPath == "/" + unrooted;
    };
    return matchesCollectionRoot("vector") ||
           matchesCollectionRoot("array") ||
           matchesCollectionRoot("soa_vector");
  };
  const bool hasPublishedVectorMetadataPath =
      expr.kind == Expr::Kind::Call &&
      !expr.isMethodCall &&
      expr.args.size() == 1 &&
      resolvePublishedNativeTailVectorHelperName(
          semanticProgram, expr, publishedVectorHelperName) &&
      (isCanonicalPublishedNativeTailVectorHelperPath(directHelperPath) ||
       semanticDirectCallMatchesVectorHelperSurface(semanticProgram, expr));
  const bool isPublishedVectorMetadataCall =
      semanticProgram != nullptr && hasPublishedVectorMetadataPath;
  if (isPublishedVectorMetadataCall &&
      publishedVectorHelperName == "count") {
    if (isDiagnosticPublishedVectorMetadataTarget(expr.args.front())) {
      return UnsupportedNativeCallResult::NotHandled;
    }
    error = "count requires array, vector, map, or string target (target=" +
            diagnosticTargetName() + ")";
    return UnsupportedNativeCallResult::Error;
  }
  if (isPublishedVectorMetadataCall &&
      publishedVectorHelperName == "capacity") {
    if (isDiagnosticPublishedVectorMetadataTarget(expr.args.front())) {
      return UnsupportedNativeCallResult::NotHandled;
    }
    error = "capacity requires vector target";
    return UnsupportedNativeCallResult::Error;
  }
  if (hasPublishedVectorMetadataPath &&
      (publishedVectorHelperName == "count" ||
       publishedVectorHelperName == "capacity")) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (isRemovedCollectionMetadataAlias("count") ||
      isRemovedCollectionMetadataAlias("capacity")) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (isBareSimpleCountLikeCall("count") &&
      !isDiagnosticVectorTarget(expr.args.front())) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (isBareSimpleCountLikeCall("capacity") &&
      !isDiagnosticVectorTarget(expr.args.front())) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall && count_access_detail::isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
      isDiagnosticVectorTarget(expr.args.front())) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall &&
      (count_access_detail::isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count"))) {
    if (expr.name == "/count" && expr.namespacePrefix.empty() &&
        expr.args.size() == 1 && expr.args.front().kind != Expr::Kind::Call &&
        !isDiagnosticVectorTarget(expr.args.front())) {
      return UnsupportedNativeCallResult::NotHandled;
    }
    error = "count requires array, vector, map, or string target (target=" +
            diagnosticTargetName() + ")";
    return UnsupportedNativeCallResult::Error;
  }
  if (!expr.isMethodCall && count_access_detail::isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
      isDiagnosticVectorTarget(expr.args.front())) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall && count_access_detail::isVectorBuiltinName(expr, "capacity") &&
      expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall && count_access_detail::isVectorBuiltinName(expr, "capacity")) {
    error = "capacity requires vector target";
    return UnsupportedNativeCallResult::Error;
  }

  std::string printBuiltinName;
  if (tryGetPrintBuiltinName(expr, printBuiltinName)) {
    error = printBuiltinName + " is only supported as a statement in the native backend";
    return UnsupportedNativeCallResult::Error;
  }

  return UnsupportedNativeCallResult::NotHandled;
}

UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error) {
  static const LocalMap emptyLocals;
  return emitUnsupportedNativeCallDiagnostic(expr, emptyLocals, tryGetPrintBuiltinName, error);
}

UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error) {
  return emitUnsupportedNativeCallDiagnosticImpl(
      expr, localsIn, tryGetPrintBuiltinName, error, nullptr, nullptr);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  const SemanticProductIndex semanticIndex = buildSemanticProductIndex(semanticProgram);
  const SemanticProductIndex *const semanticIndexPtr =
      semanticProgram == nullptr ? nullptr : &semanticIndex;

  std::string mathName;
  if (tryGetMathBuiltinName(expr, mathName) && !isSupportedMathBuiltinName(mathName)) {
    error = "native backend does not support math builtin: " + mathName;
    return NativeCallTailDispatchResult::Error;
  }
  std::string directMapReadHelperName;
  if (!expr.isMethodCall &&
      resolvePublishedNativeTailHelperName(
          semanticProgram,
          expr,
          StdlibSurfaceId::CollectionsMapHelpers,
          directMapReadHelperName) &&
      isMapReadHelperName(directMapReadHelperName) &&
      hasSemanticMapReadHelperDefinition(semanticProgram,
                                         directMapReadHelperName)) {
    return NativeCallTailDispatchResult::NotHandled;
  }
  if (!isExplicitDirectVectorCountCall(semanticProgram, expr) &&
      !expr.isMethodCall && count_access_detail::isVectorBuiltinName(expr, "count") &&
      expr.args.size() == 1 &&
      !isNamedArgumentVectorTemporary(expr.args.front())) {
    if (isVectorCountTarget(expr.args.front(), localsIn)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
  }
  const auto countAccessResult = tryEmitCountAccessCall(
      expr,
      localsIn,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      [&](const Expr &targetExpr, const LocalMap &targetLocals) {
        if (resolveMapAccessTargetInfo(targetExpr, targetLocals, resolveCallMapAccessTargetInfo).isMapTarget) {
          return true;
        }
        return resolveArrayVectorAccessTargetInfo(
                   targetExpr, targetLocals, resolveCallArrayVectorAccessTargetInfo)
            .isArrayOrVectorTarget;
      },
      [&](const Expr &targetExpr, const LocalMap &targetLocals) {
        const auto targetInfo = resolveArrayVectorAccessTargetInfo(
            targetExpr, targetLocals, resolveCallArrayVectorAccessTargetInfo);
        return targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget;
      },
      [&](const Expr &targetExpr, const LocalMap &targetLocals) {
        const auto targetInfo = resolveArrayVectorAccessTargetInfo(
            targetExpr, targetLocals, resolveCallArrayVectorAccessTargetInfo);
        return targetInfo.isArrayOrVectorTarget && targetInfo.isVectorTarget;
      },
      inferExprKind,
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error,
      semanticProgram,
      semanticIndexPtr);
  if (countAccessResult == CountAccessCallEmitResult::Emitted) {
    return NativeCallTailDispatchResult::Emitted;
  }
  if (countAccessResult == CountAccessCallEmitResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  if (expr.args.size() == 1 &&
      (count_access_detail::isVectorBuiltinName(expr, "count") ||
       count_access_detail::isMapBuiltinName(expr, "count") ||
       isSimpleCallName(expr, "count") ||
       resolveNativeTailCallPathWithoutFallbackProbes(expr) == "/string/count")) {
    const Expr &target = expr.args.front();
    std::string accessName;
    if (target.kind == Expr::Kind::Call && target.sourceIsMethodCall &&
        target.args.size() == 2 &&
        getBuiltinArrayAccessName(target, accessName) &&
        accessName == "at_unsafe") {
      return NativeCallTailDispatchResult::NotHandled;
    }
  }

  if (expr.isMethodCall && expr.name == "contains") {
    if (expr.args.size() != 2) {
      error = "contains requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto mapTargetInfo = resolveMapAccessTargetInfo(
        expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
    if (expr.args.front().kind == Expr::Kind::Call && !mapTargetInfo.isMapTarget) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const auto containsResult = tryEmitMapContainsLookup(
        expr.args.front(),
        expr.args[1],
        localsIn,
        allocTempLocal,
        emitExpr,
        resolveStringTableTarget,
        resolveCallMapAccessTargetInfo,
        inferExprKind,
        instructionCount,
        emitInstruction,
        patchInstructionImm,
        error);
    if (containsResult == MapAccessLookupEmitResult::Emitted) {
      return NativeCallTailDispatchResult::Emitted;
    }
    if (containsResult == MapAccessLookupEmitResult::Error) {
      return NativeCallTailDispatchResult::Error;
    }
    error = "contains requires map target";
    return NativeCallTailDispatchResult::Error;
  }

  if (!expr.isMethodCall && isMapContainsHelperName(expr)) {
    if (expr.args.size() != 2) {
      error = "contains requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto mapTargetInfo = resolveMapAccessTargetInfo(
        expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
    if (shouldPreserveExplicitDirectMapHelperCall(semanticProgram, expr, mapTargetInfo)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (expr.args.front().kind == Expr::Kind::Call &&
        (!mapTargetInfo.isMapTarget || mapTargetInfo.isWrappedMapTarget)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const auto containsResult = tryEmitMapContainsLookup(
        expr.args.front(),
        expr.args[1],
        localsIn,
        allocTempLocal,
        emitExpr,
        resolveStringTableTarget,
        resolveCallMapAccessTargetInfo,
        inferExprKind,
        instructionCount,
        emitInstruction,
        patchInstructionImm,
        error);
    if (containsResult == MapAccessLookupEmitResult::Emitted) {
      return NativeCallTailDispatchResult::Emitted;
    }
    if (containsResult == MapAccessLookupEmitResult::Error) {
      return NativeCallTailDispatchResult::Error;
    }
    error = "contains requires map target";
    return NativeCallTailDispatchResult::Error;
  }

  if (expr.isMethodCall && expr.name == "tryAt") {
    if (expr.args.size() != 2) {
      error = "tryAt requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto mapTargetInfo = resolveMapAccessTargetInfo(
        expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
    if (expr.args.front().kind == Expr::Kind::Call && !mapTargetInfo.isMapTarget) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (!mapTargetInfo.isMapTarget) {
      error = "tryAt requires map target";
      return NativeCallTailDispatchResult::Error;
    }
    if (!validateMapAccessTargetInfo(mapTargetInfo, "tryAt", error)) {
      return NativeCallTailDispatchResult::Error;
    }
    if (!emitMapLookupTryAt(
            mapTargetInfo.mapKeyKind,
            mapTargetInfo.structTypeName,
            expr.args.front(),
            expr.args[1],
            localsIn,
            allocTempLocal,
            emitExpr,
            resolveStringTableTarget,
            inferExprKind,
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error)) {
      return NativeCallTailDispatchResult::Error;
    }
    return NativeCallTailDispatchResult::Emitted;
  }

  if (!expr.isMethodCall && isMapTryAtHelperName(expr)) {
    if (expr.args.size() != 2) {
      error = "tryAt requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    const auto mapTargetInfo = resolveMapAccessTargetInfo(
        expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
    if (shouldPreserveExplicitDirectMapHelperCall(semanticProgram, expr, mapTargetInfo)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (expr.args.front().kind == Expr::Kind::Call &&
        (!mapTargetInfo.isMapTarget || mapTargetInfo.isWrappedMapTarget)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if (!mapTargetInfo.isMapTarget) {
      error = "tryAt requires map target";
      return NativeCallTailDispatchResult::Error;
    }
    if (!validateMapAccessTargetInfo(mapTargetInfo, "tryAt", error)) {
      return NativeCallTailDispatchResult::Error;
    }
    if (!emitMapLookupTryAt(
            mapTargetInfo.mapKeyKind,
            mapTargetInfo.structTypeName,
            expr.args.front(),
            expr.args[1],
            localsIn,
            allocTempLocal,
            emitExpr,
            resolveStringTableTarget,
            inferExprKind,
            instructionCount,
            emitInstruction,
            patchInstructionImm,
            error)) {
      return NativeCallTailDispatchResult::Error;
    }
    return NativeCallTailDispatchResult::Emitted;
  }

  const auto unsupportedCallResult = emitUnsupportedNativeCallDiagnosticImpl(
      expr, localsIn, tryGetPrintBuiltinName, error, semanticProgram, semanticIndexPtr);
  if (unsupportedCallResult == UnsupportedNativeCallResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  std::string accessName;
  if (isExplicitDirectMapCountContainsTryAtCall(semanticProgram, expr) &&
      !expr.args.empty() &&
      resolveMapAccessTargetInfo(expr.args.front(), localsIn, resolveCallMapAccessTargetInfo).isMapTarget) {
    return NativeCallTailDispatchResult::NotHandled;
  }
  const bool hasBuiltinArrayAccessName =
      getBuiltinArrayAccessName(expr, accessName);
  std::string publishedVectorAccessName;
  const std::string directHelperPath =
      resolveNativeTailCallPathWithoutFallbackProbes(expr);
  const bool hasPublishedVectorAccessName =
      !expr.isMethodCall &&
      resolvePublishedNativeTailVectorHelperName(
          semanticProgram, expr, publishedVectorAccessName) &&
      (publishedVectorAccessName == "at" ||
       publishedVectorAccessName == "at_unsafe") &&
      (isCanonicalPublishedNativeTailVectorHelperPath(directHelperPath) ||
       semanticDirectCallMatchesVectorHelperSurface(semanticProgram, expr));
  if (!hasBuiltinArrayAccessName && hasPublishedVectorAccessName) {
    accessName = publishedVectorAccessName;
  }
  if (hasBuiltinArrayAccessName || hasPublishedVectorAccessName) {
    const bool isMethodCallTempReceiver =
        expr.isMethodCall &&
        !expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Call &&
        (accessName == "at" || accessName == "at_unsafe");
    const bool isMapMethodTempReceiver =
        isMethodCallTempReceiver &&
        resolveMapAccessTargetInfo(
            expr.args.front(), localsIn, resolveCallMapAccessTargetInfo)
            .isMapTarget;
    if (isMethodCallTempReceiver && !isMapMethodTempReceiver) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const auto arrayVectorTargetInfo = !expr.args.empty()
                                           ? resolveArrayVectorAccessTargetInfo(
                                                 expr.args.front(), localsIn, resolveCallArrayVectorAccessTargetInfo)
                                           : ArrayVectorAccessTargetInfo{};
    std::string explicitHelperName;
    const bool isExplicitVectorAccessCall =
        !expr.isMethodCall &&
        resolvePublishedNativeTailVectorHelperName(semanticProgram, expr, explicitHelperName) &&
        (isCanonicalPublishedNativeTailVectorHelperPath(directHelperPath) ||
         semanticDirectCallMatchesVectorHelperSurface(semanticProgram, expr)) &&
        (explicitHelperName == "at" || explicitHelperName == "at_unsafe");
    const bool isPublishedVectorAtUnsafeImplementationCall =
        !expr.isMethodCall &&
        explicitHelperName == "at_unsafe" &&
        !isCanonicalPublishedNativeTailVectorHelperPath(directHelperPath) &&
        !semanticDirectCallMatchesVectorHelperSurface(semanticProgram, expr);
    const bool isDirectExperimentalVectorAtUnsafeImplementationCall =
        !expr.isMethodCall &&
        accessName == "at_unsafe" &&
        (directHelperPath.rfind(experimentalCollectionMemberRoot("vector"), 0) == 0 ||
         directHelperPath.rfind(experimentalCollectionMemberRoot("vector", false), 0) == 0);
    if (((isExplicitVectorAccessCall && explicitHelperName == "at_unsafe") ||
         isPublishedVectorAtUnsafeImplementationCall ||
         isDirectExperimentalVectorAtUnsafeImplementationCall) &&
        arrayVectorTargetInfo.isVectorTarget) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    if ((isExplicitDirectSoaAccessCall(expr) ||
         (expr.isMethodCall &&
          (accessName == "get" || accessName == "get_ref"))) &&
        arrayVectorTargetInfo.isSoaVector) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const bool isExplicitMapAccessCall =
        !expr.isMethodCall &&
        resolvePublishedNativeTailHelperName(
            semanticProgram,
            expr,
            StdlibSurfaceId::CollectionsMapHelpers,
            explicitHelperName) &&
        (explicitHelperName == "at" || explicitHelperName == "at_ref" ||
         explicitHelperName == "at_unsafe" ||
         explicitHelperName == "at_unsafe_ref");
    const bool isExplicitMapAccessMethodCall =
        expr.isMethodCall &&
        resolvePublishedNativeTailHelperName(
            semanticProgram,
            expr,
            StdlibSurfaceId::CollectionsMapHelpers,
            explicitHelperName) &&
        (explicitHelperName == "at" || explicitHelperName == "at_ref" ||
         explicitHelperName == "at_unsafe" ||
         explicitHelperName == "at_unsafe_ref");
    const std::string explicitMapAccessMethodPath =
        (directHelperPath.rfind("/std/collections/map/", 0) == 0 ||
         directHelperPath.rfind("std/collections/map/", 0) == 0)
            ? directHelperPath
            : "/std/collections/map/" + explicitHelperName;
    if (isExplicitMapAccessMethodCall &&
        !semanticMapAccessHelperKeepsBuiltinReturn(semanticProgram,
                                                   explicitMapAccessMethodPath)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const auto isMapArgsPackAccessReceiver = [&](const Expr &receiver) {
      const auto isMapArgsPackAccessReceiverImpl =
          [&](const Expr &candidate, const auto &self) -> bool {
        if (candidate.kind == Expr::Kind::Call &&
            isSimpleCallName(candidate, "dereference") &&
            candidate.args.size() == 1) {
          return self(candidate.args.front(), self);
        }
        const auto accessTargetInfo = resolveArrayVectorAccessTargetInfo(
            candidate, localsIn, resolveCallArrayVectorAccessTargetInfo);
        return accessTargetInfo.isArgsPackTarget &&
               (accessTargetInfo.isMapTarget ||
                accessTargetInfo.isWrappedMapTarget);
      };
      return isMapArgsPackAccessReceiverImpl(receiver,
                                             isMapArgsPackAccessReceiverImpl);
    };
    if (isExplicitMapAccessCall && !expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Call) {
      const auto mapTargetInfo = resolveMapAccessTargetInfo(
          expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
      if (!mapTargetInfo.isMapTarget ||
          !isMapArgsPackAccessReceiver(expr.args.front())) {
        return NativeCallTailDispatchResult::NotHandled;
      }
    }
    if (isExplicitMapAccessCall && !expr.args.empty()) {
      const auto mapTargetInfo = resolveMapAccessTargetInfo(
          expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
      if (mapTargetInfo.isMapTarget &&
          (expr.args.front().kind != Expr::Kind::Call ||
           !isMapArgsPackAccessReceiver(expr.args.front()))) {
        return NativeCallTailDispatchResult::NotHandled;
      }
    }
    if (expr.args.size() != 2) {
      error = accessName + " requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
    }
    if (!expr.isMethodCall &&
        expr.namespacePrefix.empty() &&
        (expr.name == "at" || expr.name == "at_unsafe") &&
        !isExplicitMapAccessCall &&
        !hasSemanticMapAccessHelperDefinition(semanticProgram, accessName)) {
      const auto mapTargetInfo = resolveMapAccessTargetInfo(
          expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
      if (mapTargetInfo.isMapTarget) {
        error = "unknown call target: /std/collections/map/" + accessName;
        return NativeCallTailDispatchResult::Error;
      }
    }
    if (!expr.isMethodCall && !expr.args.empty()) {
      const auto mapTargetInfo = resolveMapAccessTargetInfo(
          expr.args.front(), localsIn, resolveCallMapAccessTargetInfo);
      if (mapTargetInfo.isMapTarget &&
          (isStringReturningMapAccessAlias(
               semanticProgram, semanticIndexPtr, expr) ||
           (expr.name.find('/') == std::string::npos &&
            !hasExplicitStdMapSourceSpelling(expr) &&
            !semanticMapAccessHelperKeepsBuiltinReturn(
                semanticProgram, "/std/collections/map/" + accessName)))) {
        return NativeCallTailDispatchResult::NotHandled;
      }
    }
    if (!emitBuiltinArrayAccess(accessName,
                                expr.args[0],
                                expr.args[1],
                                localsIn,
                                resolveStringTableTarget,
                                stringTableCount,
                                resolveCallMapAccessTargetInfo,
                                resolveCallArrayVectorAccessTargetInfo,
                                inferExprKind,
                                isEntryArgsName,
                                allocTempLocal,
                                emitExpr,
                                emitStringIndexOutOfBounds,
                                emitMapKeyNotFound,
                                emitArrayIndexOutOfBounds,
                                instructionCount,
                                emitInstruction,
                                patchInstructionImm,
                                error,
                                semanticProgram,
                                semanticIndexPtr)) {
      return NativeCallTailDispatchResult::Error;
    }
    return NativeCallTailDispatchResult::Emitted;
  }

  return NativeCallTailDispatchResult::NotHandled;
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      stringTableCount,
      emitExpr,
      {},
      {},
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatch(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      stringTableCount,
      emitExpr,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatchWithLocals(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      resolveCallMapAccessTargetInfo,
      resolveCallArrayVectorAccessTargetInfo,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatchWithLocals(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      stringTableCount,
      emitExpr,
      {},
      {},
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, std::string &)> &tryGetMathBuiltinName,
    const std::function<bool(const std::string &)> &isSupportedMathBuiltinName,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram) {
  return tryEmitNativeCallTailDispatchWithLocals(
      expr,
      localsIn,
      tryGetMathBuiltinName,
      isSupportedMathBuiltinName,
      isArrayCountCall,
      isVectorCapacityCall,
      isStringCountCall,
      isEntryArgsName,
      resolveStringTableTarget,
      0,
      emitExpr,
      tryGetPrintBuiltinName,
      inferExprKind,
      allocTempLocal,
      emitStringIndexOutOfBounds,
      emitMapKeyNotFound,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error,
      semanticProgram);
}

} // namespace primec::ir_lowerer
