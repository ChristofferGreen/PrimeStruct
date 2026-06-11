// soa-surface-audit: exempt
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
  return vectorHelperSurfaceMetadata();
}

const StdlibSurfaceMetadata *nativeTailKeyValueHelperMetadata() {
  return keyValueHelperSurfaceMetadata();
}

bool semanticSurfaceMatches(std::optional<StdlibSurfaceId> surfaceId,
                            const StdlibSurfaceMetadata *metadata) {
  return surfaceId.has_value() && metadata != nullptr && *surfaceId == metadata->id;
}

const SemanticProgramQueryFact *findSourceQueryFact(
    const SemanticProgram *semanticProgram,
    const Expr &expr) {
  if (semanticProgram == nullptr) {
    return nullptr;
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

bool resolvePublishedNativeTailHelperName(const SemanticProgram *semanticProgram,
                                          const Expr &expr,
                                          StdlibSurfaceId surfaceId,
                                          std::string &helperNameOut) {
  if (resolvePublishedSemanticStdlibSurfaceMemberName(
          semanticProgram, expr, surfaceId, helperNameOut)) {
    return true;
  }
  if (resolvePublishedStdlibSurfaceMemberName(
          resolveNativeTailCallPathWithoutFallbackProbes(expr),
          surfaceId,
          helperNameOut)) {
    return true;
  }
  const SemanticProgramQueryFact *queryFact =
      findSourceQueryFact(semanticProgram, expr);
  if (queryFact == nullptr || queryFact->resolvedPathId == InvalidSymbolId) {
    return false;
  }
  return resolvePublishedStdlibSurfaceMemberName(
      semanticProgramResolveCallTargetString(*semanticProgram,
                                             queryFact->resolvedPathId),
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

bool resolvePublishedNativeTailKeyValueHelperName(const SemanticProgram *semanticProgram,
                                                  const Expr &expr,
                                                  std::string &helperNameOut) {
  const auto *metadata = nativeTailKeyValueHelperMetadata();
  return metadata != nullptr &&
         resolvePublishedNativeTailHelperName(semanticProgram, expr, metadata->id, helperNameOut);
}

std::string nativeTailKeyValueHelperPath(std::string_view helperName) {
  const auto *metadata = nativeTailKeyValueHelperMetadata();
  if (metadata == nullptr) {
    return {};
  }
  return stdlibSurfaceCanonicalHelperPath(metadata->id, helperName);
}

bool isCanonicalPublishedNativeTailVectorHelperPath(std::string_view path) {
  const auto *metadata = nativeTailVectorHelperMetadata();
  return metadata != nullptr &&
         isCanonicalPublishedStdlibSurfaceHelperPath(path, metadata->id);
}

bool isCanonicalPublishedNativeTailKeyValueHelperPath(std::string_view path) {
  const auto *metadata = nativeTailKeyValueHelperMetadata();
  return metadata != nullptr &&
         isCanonicalPublishedStdlibSurfaceHelperPath(path, metadata->id);
}

bool hasCanonicalNativeTailKeyValueHelperPrefix(std::string_view text) {
  const auto *metadata = nativeTailKeyValueHelperMetadata();
  if (metadata == nullptr || metadata->canonicalPath.empty()) {
    return false;
  }
  auto hasPrefix = [&](std::string_view root) {
    return text.size() > root.size() && text.rfind(root, 0) == 0 &&
           text[root.size()] == '/';
  };
  if (hasPrefix(metadata->canonicalPath)) {
    return true;
  }
  std::string_view unrooted = metadata->canonicalPath;
  if (!unrooted.empty() && unrooted.front() == '/') {
    unrooted.remove_prefix(1);
  }
  return hasPrefix(unrooted);
}

bool semanticDirectCallMatchesVectorHelperSurface(const SemanticProgram *semanticProgram,
                                                  const Expr &expr) {
  return semanticSurfaceMatches(findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr),
                                nativeTailVectorHelperMetadata());
}

bool isExplicitDirectKeyValueCountContainsTryAtCall(const SemanticProgram *semanticProgram,
                                                    const Expr &expr) {
  if (expr.isMethodCall || expr.kind != Expr::Kind::Call) {
    return false;
  }
  std::string helperName;
  return resolvePublishedNativeTailKeyValueHelperName(
             semanticProgram, expr, helperName) &&
         (helperName == "count" || helperName == "contains" || helperName == "tryAt");
}

bool isKeyValueReadHelperName(std::string_view helperName) {
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
         rawPath == "/std/collections/soa/get" ||
         rawPath == "/soa_vector/get_ref" ||
         rawPath == "/std/collections/soa_vector/get_ref" ||
         rawPath == "/std/collections/soa/get_ref";
}

bool hasSemanticKeyValueAccessHelperDefinition(
    const SemanticProgram *semanticProgram,
    std::string_view accessName) {
  if (semanticProgram == nullptr ||
      (accessName != "at" && accessName != "at_unsafe")) {
    return false;
  }
  const std::string canonicalPath = nativeTailKeyValueHelperPath(accessName);
  const std::string canonicalRefPath = nativeTailKeyValueHelperPath(
      std::string(accessName) + "_ref");
  if (canonicalPath.empty() || canonicalRefPath.empty()) {
    return false;
  }
  auto importsKeyValueHelpers = [&](const std::vector<std::string> &imports) {
    for (const std::string &importPath : imports) {
      if (importPathCoversNativeTailTarget(importPath, canonicalPath) ||
          importPathCoversNativeTailTarget(importPath, canonicalRefPath)) {
        return true;
      }
    }
    return false;
  };
  if (importsKeyValueHelpers(semanticProgram->sourceImports) ||
      importsKeyValueHelpers(semanticProgram->imports)) {
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
    if (path == canonicalPath ||
        path == canonicalRefPath) {
      return true;
    }
  }
  return false;
}

bool semanticKeyValueAccessHelperKeepsBuiltinReturn(
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

const SemanticProgramQueryFact *findSourceKeyValueAccessAliasQueryFact(
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
  return findSourceQueryFact(semanticProgram, expr);
}

bool isStringReturningKeyValueAccessAlias(
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex,
    const Expr &expr) {
  const auto *queryFact =
      findSourceKeyValueAccessAliasQueryFact(semanticProgram, semanticIndex, expr);
  if (queryFact == nullptr || queryFact->resolvedPathId == InvalidSymbolId) {
    return false;
  }
  const std::string resolvedPath =
      std::string(semanticProgramResolveCallTargetString(
          *semanticProgram, queryFact->resolvedPathId));
  std::string accessName;
  if (!getBuiltinArrayAccessName(expr, accessName) ||
      (accessName != "at" && accessName != "at_unsafe")) {
    return false;
  }
  if (resolvedPath != "/" + accessName &&
      resolvedPath != nativeTailKeyValueHelperPath(accessName)) {
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

bool hasExplicitStdKeyValueSourceSpelling(const Expr &expr) {
  return hasCanonicalNativeTailKeyValueHelperPrefix(expr.name) ||
         hasCanonicalNativeTailKeyValueHelperPrefix(expr.namespacePrefix);
}

} // namespace

bool isKeyValueContainsHelperName(const Expr &expr);
bool isKeyValueTryAtHelperName(const Expr &expr);
bool isVectorTarget(const Expr &expr, const LocalMap &localsIn);
bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn);

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
  if (!expr.isMethodCall &&
      count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") &&
      expr.args.size() == 1 &&
      isDiagnosticVectorTarget(expr.args.front())) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall &&
      count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count")) {
    if (expr.name == "/count" && expr.namespacePrefix.empty() &&
        expr.args.size() == 1 && expr.args.front().kind != Expr::Kind::Call &&
        !isDiagnosticVectorTarget(expr.args.front())) {
      return UnsupportedNativeCallResult::NotHandled;
    }
    error = "count requires array, vector, map, or string target (target=" +
            diagnosticTargetName() + ")";
    return UnsupportedNativeCallResult::Error;
  }
  if (!expr.isMethodCall &&
      count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity") &&
      expr.args.size() == 1 &&
      isDiagnosticVectorTarget(expr.args.front())) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall &&
      count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity") &&
      expr.args.size() == 1 && expr.args.front().kind == Expr::Kind::Call) {
    return UnsupportedNativeCallResult::NotHandled;
  }
  if (!expr.isMethodCall &&
      count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "capacity")) {
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
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
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
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
  (void)emitMapKeyNotFound;
  std::optional<SemanticProductIndex> ownedSemanticIndex;
  if (semanticProgram != nullptr && semanticIndex == nullptr) {
    ownedSemanticIndex = buildSemanticProductIndex(semanticProgram);
    semanticIndex = &*ownedSemanticIndex;
  }
  const SemanticProductIndex *const semanticIndexPtr =
      semanticProgram == nullptr ? nullptr : semanticIndex;

  std::string mathName;
  if (tryGetMathBuiltinName(expr, mathName) && !isSupportedMathBuiltinName(mathName)) {
    error = "native backend does not support math builtin: " + mathName;
    return NativeCallTailDispatchResult::Error;
  }
  std::string directKeyValueReadHelperName;
  if (!expr.isMethodCall &&
      resolvePublishedNativeTailKeyValueHelperName(
          semanticProgram, expr, directKeyValueReadHelperName) &&
      isKeyValueReadHelperName(directKeyValueReadHelperName)) {
    return NativeCallTailDispatchResult::NotHandled;
  }
  if (!isExplicitDirectVectorCountCall(semanticProgram, expr) &&
      !expr.isMethodCall &&
      count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") &&
      expr.args.size() == 1 &&
      !isNamedArgumentVectorTemporary(expr.args.front())) {
    if (isVectorCountTarget(expr.args.front(), localsIn)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
  }
  if (!expr.isMethodCall && expr.args.size() == 1) {
    const std::string directHelperPath =
        resolveNativeTailCallPathWithoutFallbackProbes(expr);
    const bool isExplicitVectorMetadataCall =
        directHelperPath == "/std/collections/vector/count" ||
        directHelperPath == "std/collections/vector/count" ||
        directHelperPath == "/std/collections/vector/capacity" ||
        directHelperPath == "std/collections/vector/capacity" ||
        directHelperPath == "/vector/count" ||
        directHelperPath == "vector/count" ||
        directHelperPath == "/vector/capacity" ||
        directHelperPath == "vector/capacity";
    if (isExplicitVectorMetadataCall &&
        (expr.args.front().kind == Expr::Kind::Call ||
         resolveCollectionPairTypeInfo(
             expr.args.front(),
             localsIn,
             resolveCallCollectionPairTypeInfo)
             .isKeyValueTarget)) {
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
        if (resolveCollectionPairTypeInfo(targetExpr, targetLocals, resolveCallCollectionPairTypeInfo).isKeyValueTarget) {
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

  if (expr.isMethodCall && expr.args.size() == 1 &&
      (isSimpleCallName(expr, "count") ||
       resolveNativeTailCallPathWithoutFallbackProbes(expr) == "/string/count")) {
    const Expr &target = expr.args.front();
    if ((target.kind == Expr::Kind::Name ||
         target.kind == Expr::Kind::StringLiteral) &&
        inferExprKind(target, localsIn) == LocalInfo::ValueKind::String) {
      if (!emitExpr(target, localsIn)) {
        return NativeCallTailDispatchResult::Error;
      }
      emitInstruction(IrOpcode::LoadStringLength, 0);
      return NativeCallTailDispatchResult::Emitted;
    }
  }

  if (expr.args.size() == 1 &&
      (count_access_detail::isUnqualifiedCollectionBuiltinName(expr, "count") ||
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

  const auto unsupportedCallResult = emitUnsupportedNativeCallDiagnosticImpl(
      expr, localsIn, tryGetPrintBuiltinName, error, semanticProgram, semanticIndexPtr);
  if (unsupportedCallResult == UnsupportedNativeCallResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  std::string accessName;
  if (isExplicitDirectKeyValueCountContainsTryAtCall(semanticProgram, expr) &&
      !expr.args.empty() &&
      resolveCollectionPairTypeInfo(expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo).isKeyValueTarget) {
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
    const bool isCollectionPairAccessName =
        accessName == "at" || accessName == "at_ref" ||
        accessName == "at_unsafe" || accessName == "at_unsafe_ref";
    if (expr.isMethodCall && isCollectionPairAccessName && !expr.args.empty() &&
        accessName != "at" && accessName != "at_unsafe" &&
        resolveCollectionPairTypeInfo(
            expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo)
            .isKeyValueTarget) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const bool isMethodCallTempReceiver =
        expr.isMethodCall &&
        !expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Call &&
        (accessName == "at" || accessName == "at_unsafe");
    const bool isKeyValueMethodTempReceiver =
        isMethodCallTempReceiver &&
        resolveCollectionPairTypeInfo(
            expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo)
            .isKeyValueTarget;
    if (isMethodCallTempReceiver && !isKeyValueMethodTempReceiver) {
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
        (directHelperPath.rfind(vectorBackingMemberRoot(), 0) == 0 ||
         directHelperPath.rfind(vectorBackingMemberRoot(false), 0) == 0);
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
    const bool isExplicitKeyValueAccessCall =
        !expr.isMethodCall &&
        resolvePublishedNativeTailKeyValueHelperName(
            semanticProgram, expr, explicitHelperName) &&
        (explicitHelperName == "at" || explicitHelperName == "at_ref" ||
         explicitHelperName == "at_unsafe" ||
         explicitHelperName == "at_unsafe_ref");
    const bool isExplicitKeyValueAccessMethodCall =
        expr.isMethodCall &&
        resolvePublishedNativeTailKeyValueHelperName(
            semanticProgram, expr, explicitHelperName) &&
        (explicitHelperName == "at" || explicitHelperName == "at_ref" ||
         explicitHelperName == "at_unsafe" ||
         explicitHelperName == "at_unsafe_ref");
    const std::string explicitKeyValueAccessMethodPath =
        isCanonicalPublishedNativeTailKeyValueHelperPath(directHelperPath)
            ? directHelperPath
            : nativeTailKeyValueHelperPath(explicitHelperName);
    if (isExplicitKeyValueAccessMethodCall &&
        !semanticKeyValueAccessHelperKeepsBuiltinReturn(semanticProgram,
                                                        explicitKeyValueAccessMethodPath)) {
      return NativeCallTailDispatchResult::NotHandled;
    }
    const auto isKeyValueArgsPackAccessReceiver = [&](const Expr &receiver) {
      const auto isKeyValueArgsPackAccessReceiverImpl =
          [&](const Expr &candidate, const auto &self) -> bool {
        if (candidate.kind == Expr::Kind::Call &&
            isSimpleCallName(candidate, "dereference") &&
            candidate.args.size() == 1) {
          return self(candidate.args.front(), self);
        }
        const auto accessTargetInfo = resolveArrayVectorAccessTargetInfo(
            candidate, localsIn, resolveCallArrayVectorAccessTargetInfo);
        return accessTargetInfo.isArgsPackTarget &&
               (accessTargetInfo.isKeyValueTarget ||
                accessTargetInfo.isWrappedKeyValueTarget);
      };
      return isKeyValueArgsPackAccessReceiverImpl(receiver,
                                                  isKeyValueArgsPackAccessReceiverImpl);
    };
    if (isExplicitKeyValueAccessCall && !expr.args.empty() &&
        expr.args.front().kind == Expr::Kind::Call) {
      const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(
          expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo);
      if (!keyValueTargetInfo.isKeyValueTarget ||
          !isKeyValueArgsPackAccessReceiver(expr.args.front())) {
        return NativeCallTailDispatchResult::NotHandled;
      }
    }
    if (isExplicitKeyValueAccessCall && !expr.args.empty()) {
      const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(
          expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo);
      if (keyValueTargetInfo.isKeyValueTarget &&
          (expr.args.front().kind != Expr::Kind::Call ||
           !isKeyValueArgsPackAccessReceiver(expr.args.front()))) {
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
        !isExplicitKeyValueAccessCall &&
        !hasSemanticKeyValueAccessHelperDefinition(semanticProgram, accessName)) {
      const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(
          expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo);
      if (keyValueTargetInfo.isKeyValueTarget) {
        const std::string helperPath = nativeTailKeyValueHelperPath(accessName);
        error = "unknown call target: " + helperPath;
        return NativeCallTailDispatchResult::Error;
      }
    }
    if (!expr.isMethodCall && !expr.args.empty()) {
      const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(
          expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo);
      if (keyValueTargetInfo.isKeyValueTarget &&
          (isStringReturningKeyValueAccessAlias(
               semanticProgram, semanticIndexPtr, expr) ||
           (expr.name.find('/') == std::string::npos &&
            !hasExplicitStdKeyValueSourceSpelling(expr) &&
            !semanticKeyValueAccessHelperKeepsBuiltinReturn(
                semanticProgram, nativeTailKeyValueHelperPath(accessName))))) {
        return NativeCallTailDispatchResult::NotHandled;
      }
    }
    if ((accessName == "at" || accessName == "at_unsafe") &&
        expr.args.size() == 2) {
      const auto keyValueTargetInfo = resolveCollectionPairTypeInfo(
          expr.args.front(), localsIn, resolveCallCollectionPairTypeInfo);
      if (keyValueTargetInfo.isKeyValueTarget) {
        if (!emitKeyValueLookupAccess(accessName,
                                      keyValueTargetInfo.keyValueKeyKind,
                                      keyValueTargetInfo.structTypeName,
                                      expr.args.front(),
                                      expr.args[1],
                                      localsIn,
                                      allocTempLocal,
                                      emitExpr,
                                      resolveStringTableTarget,
                                      inferExprKind,
                                      emitMapKeyNotFound,
                                      instructionCount,
                                      emitInstruction,
                                      patchInstructionImm,
                                      error)) {
          return NativeCallTailDispatchResult::Error;
        }
        return NativeCallTailDispatchResult::Emitted;
      }
    }
    if (!emitBuiltinArrayAccess(accessName,
                                expr.args[0],
                                expr.args[1],
                                localsIn,
                                resolveStringTableTarget,
                                stringTableCount,
                                resolveCallArrayVectorAccessTargetInfo,
                                inferExprKind,
                                isEntryArgsName,
                                allocTempLocal,
                                emitExpr,
                                emitStringIndexOutOfBounds,
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
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
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
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
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
      resolveCallCollectionPairTypeInfo,
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
      semanticProgram,
      semanticIndex);
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
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
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
      semanticProgram,
      semanticIndex);
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
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
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
      semanticProgram,
      semanticIndex);
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
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
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
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex) {
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
      resolveCallCollectionPairTypeInfo,
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
      semanticProgram,
      semanticIndex);
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
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
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
      resolveCallCollectionPairTypeInfo,
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
