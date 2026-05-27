#include "IrLowererCallHelpers.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "IrLowererHelpers.h"
#include "IrLowererSemanticProductTargetAdapters.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererTemplateTypeParseHelpers.h"
#include "primec/AstCallPathHelpers.h"
#include "primec/StdlibSurfaceRegistry.h"

namespace primec::ir_lowerer {

namespace {

std::string stripGeneratedInlineHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

std::string experimentalCollectionMemberPath(std::string_view collectionName,
                                             std::string_view memberName,
                                             bool leadingSlash = true) {
  return experimentalCollectionMemberRoot(collectionName, leadingSlash) +
         std::string(memberName);
}

std::string rootCollectionMemberPath(std::string_view collectionName,
                                     std::string_view memberName) {
  return "/" + std::string(collectionName) + "/" + std::string(memberName);
}

bool matchesGeneratedSpecializedPath(std::string_view text,
                                     const std::string &basePath) {
  return text.rfind(basePath + "__", 0) == 0;
}

bool matchesCollectionTypeText(std::string_view text,
                               std::string_view collectionName) {
  const std::string bare(collectionName);
  const std::string rooted = "/" + bare;
  return text == bare ||
         text == rooted ||
         text == collectionTypePath(collectionName, false) ||
         text == collectionTypePath(collectionName);
}

std::string canonicalInlineKeyValueHelperName(std::string helperName) {
  helperName = stripGeneratedInlineHelperSuffix(std::move(helperName));
  return helperName;
}

const StdlibSurfaceMetadata *inlineKeyValueHelperMetadata() {
  return findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
}

bool resolvePublishedInlineKeyValueHelperName(
    std::string_view resolvedPath, std::string &helperNameOut) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  const auto *keyValueMetadata = inlineKeyValueHelperMetadata();
  if (metadata == nullptr || keyValueMetadata == nullptr ||
      metadata->id != keyValueMetadata->id) {
    return false;
  }
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string_view::npos || slash + 1 >= resolvedPath.size()) {
    return false;
  }
  helperNameOut = canonicalInlineKeyValueHelperName(
      std::string(resolvedPath.substr(slash + 1)));
  return !helperNameOut.empty();
}

bool resolvePublishedInlineKeyValueSurfaceMemberName(std::string_view path,
                                                     std::string &helperNameOut) {
  const auto *metadata = inlineKeyValueHelperMetadata();
  return metadata != nullptr &&
         resolvePublishedStdlibSurfaceMemberName(path, metadata->id, helperNameOut);
}

bool isCanonicalPublishedInlineKeyValueHelperPath(std::string_view path) {
  const auto *metadata = inlineKeyValueHelperMetadata();
  return metadata != nullptr &&
         isCanonicalPublishedStdlibSurfaceHelperPath(path, metadata->id);
}

std::string resolveInlineCallPathWithoutFallbackProbes(const Expr &expr) {
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

bool isSemanticBarePublishedKeyValueHelperCall(const Expr &expr,
                                               std::string_view helperName) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.semanticNodeId == 0 ||
      !expr.namespacePrefix.empty() || expr.name.empty() || expr.name.front() == '/') {
    return false;
  }
  if (expr.name == helperName) {
    return true;
  }
  std::string accessName;
  return getBuiltinArrayAccessName(expr, accessName) && accessName == helperName;
}

bool resolveExplicitSamePathKeyValueCountLikeDefinitionCall(
    const Expr &expr,
    const Definition &callee,
    std::string &helperNameOut) {
  helperNameOut.clear();
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  if (rawPath.empty() || rawPath.front() != '/') {
    return false;
  }
  std::string helperName;
  if (!resolvePublishedInlineKeyValueSurfaceMemberName(
          normalizeCollectionHelperPath(callee.fullPath), helperName)) {
    return false;
  }
  const size_t slash = callee.fullPath.find_last_of('/');
  if (slash == std::string::npos || slash + 1 >= callee.fullPath.size()) {
    return false;
  }
  helperName = canonicalInlineKeyValueHelperName(std::move(helperName));
  if (helperName != "count" && helperName != "contains" &&
      helperName != "tryAt" && helperName != "count_ref" &&
      helperName != "contains_ref" && helperName != "tryAt_ref") {
    return false;
  }
  if (normalizeCollectionHelperPath(rawPath) !=
      normalizeCollectionHelperPath(callee.fullPath)) {
    return false;
  }
  helperNameOut = std::move(helperName);
  return true;
}

bool isDirectKeyValueAccessHelperCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  std::string helperName;
  return resolveKeyValueHelperAliasName(expr, helperName) &&
         (helperName == "at" || helperName == "at_unsafe");
}

bool isExplicitDirectKeyValueAccessHelperCall(const Expr &expr) {
  if (!isDirectKeyValueAccessHelperCall(expr)) {
    return false;
  }
  const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  return !rawPath.empty() && rawPath.front() == '/';
}

bool isExplicitRemovedKeyValueAccessHelperCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string originalPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  std::string rawPath = originalPath;
  std::string helperName;
  if (!resolvePublishedInlineKeyValueSurfaceMemberName(rawPath, helperName) &&
      !rawPath.empty() && rawPath.front() == '/') {
    rawPath.erase(rawPath.begin());
    if (!resolvePublishedInlineKeyValueSurfaceMemberName(rawPath, helperName)) {
      return false;
    }
  }
  return !isCanonicalPublishedInlineKeyValueHelperPath(originalPath) &&
         (helperName == "at" || helperName == "at_unsafe" ||
          helperName == "at_ref" || helperName == "at_unsafe_ref");
}

bool isSemanticBarePreferredKeyValueHelperDefinitionCall(const Expr &expr,
                                                         const Definition &callee) {
  std::string helperName;
  if (!resolvePublishedInlineKeyValueHelperName(callee.fullPath, helperName)) {
    return false;
  }
  return isSemanticBarePublishedKeyValueHelperCall(
      expr, canonicalInlineKeyValueHelperName(helperName));
}

bool isBareDirectWrapperKeyValueAccessDefinitionCall(const Expr &expr) {
  const bool isBareAccessHelper =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      expr.namespacePrefix.empty() &&
      (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe"));
  return expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
         isBareAccessHelper &&
         !isExplicitDirectKeyValueAccessHelperCall(expr) &&
         !expr.args.empty() &&
         expr.args.front().kind == Expr::Kind::Call;
}

bool prefersBuiltinCountFallbackOverRemovedShadow(
    const Expr &expr,
    const Definition &callee,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name != "count" ||
      !expr.namespacePrefix.empty() || expr.args.size() != 1) {
    return false;
  }
  if (callee.fullPath == "/array/count") {
    return isArrayCountCall(expr);
  }
  if (callee.fullPath == "/string/count") {
    return isStringCountCall(expr);
  }
  return false;
}

bool keepsBuiltinInlineReturnForPublishedKeyValueHelper(std::string_view helperName,
                                                   const Definition &callee) {
  std::string declaredReturnType;
  if (!inferReceiverTypeFromDeclaredReturn(callee, declaredReturnType)) {
    return true;
  }
  declaredReturnType = trimTemplateTypeText(declaredReturnType);
  if (!declaredReturnType.empty() && declaredReturnType.front() == '/') {
    declaredReturnType.erase(declaredReturnType.begin());
  }
  if (helperName == "contains" || helperName == "contains_ref") {
    return declaredReturnType == "bool";
  }
  if (helperName == "tryAt" || helperName == "tryAt_ref") {
    return declaredReturnType == "Result";
  }
  if (helperName == "at" || helperName == "at_ref" ||
      helperName == "at_unsafe" || helperName == "at_unsafe_ref") {
    return declaredReturnType == "bool" || declaredReturnType == "int" ||
           declaredReturnType == "i8" || declaredReturnType == "i16" ||
           declaredReturnType == "i32" || declaredReturnType == "i64" ||
           declaredReturnType == "u8" || declaredReturnType == "u16" ||
           declaredReturnType == "u32" || declaredReturnType == "u64" ||
           declaredReturnType == "float" || declaredReturnType == "f32" ||
           declaredReturnType == "f64" || declaredReturnType == "string";
  }
  return true;
}

bool keepsBuiltinInlineReturnForPublishedVectorHelper(std::string_view helperName,
                                                      const Definition &callee) {
  std::string declaredReturnType;
  if (!inferReceiverTypeFromDeclaredReturn(callee, declaredReturnType)) {
    return true;
  }
  declaredReturnType = trimTemplateTypeText(declaredReturnType);
  if (!declaredReturnType.empty() && declaredReturnType.front() == '/') {
    declaredReturnType.erase(declaredReturnType.begin());
  }
  if (helperName == "count" || helperName == "capacity") {
    return declaredReturnType == "int" || declaredReturnType == "i32";
  }
  if (helperName == "at" || helperName == "at_unsafe") {
    return declaredReturnType == "bool" || declaredReturnType == "int" ||
           declaredReturnType == "i8" || declaredReturnType == "i16" ||
           declaredReturnType == "i32" || declaredReturnType == "i64" ||
           declaredReturnType == "u8" || declaredReturnType == "u16" ||
           declaredReturnType == "u32" || declaredReturnType == "u64" ||
           declaredReturnType == "float" || declaredReturnType == "f32" ||
           declaredReturnType == "f64" || declaredReturnType == "string";
  }
  return true;
}

bool isTypeNamespaceMethodCallForInlineEmit(const Expr &callExpr,
                                            const Definition &callee,
                                            const LocalMap &callerLocals) {
  if (!callExpr.isMethodCall || callExpr.args.empty()) {
    return false;
  }
  const Expr &receiver = callExpr.args.front();
  if (receiver.kind != Expr::Kind::Name || callerLocals.count(receiver.name) > 0) {
    return false;
  }
  const size_t methodSlash = callee.fullPath.find_last_of('/');
  if (methodSlash == std::string::npos || methodSlash == 0) {
    return false;
  }
  const std::string receiverPath = callee.fullPath.substr(0, methodSlash);
  const size_t receiverSlash = receiverPath.find_last_of('/');
  const std::string receiverTypeName =
      receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
  return receiverTypeName == receiver.name;
}

Expr makeInlineEmitDirectTypeNamespaceCall(const Expr &callExpr, const Definition &callee) {
  Expr directCallExpr = callExpr;
  directCallExpr.name = callee.fullPath;
  directCallExpr.namespacePrefix.clear();
  directCallExpr.isMethodCall = false;
  if (!directCallExpr.args.empty()) {
    directCallExpr.args.erase(directCallExpr.args.begin());
  }
  if (!directCallExpr.argNames.empty()) {
    directCallExpr.argNames.erase(directCallExpr.argNames.begin());
  }
  return directCallExpr;
}

} // namespace

bool isKeyValueContainsHelperName(const Expr &expr) {
  if (isSimpleCallName(expr, "contains")) {
    return true;
  }
  std::string aliasName;
  return resolveKeyValueHelperAliasName(expr, aliasName) && aliasName == "contains";
}

bool isKeyValueTryAtHelperName(const Expr &expr) {
  if (isSimpleCallName(expr, "tryAt")) {
    return true;
  }
  std::string aliasName;
  return resolveKeyValueHelperAliasName(expr, aliasName) && aliasName == "tryAt";
}

bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn);

bool isPublicOrCompatibilitySoaToAosCall(const Expr &expr) {
  return isCanonicalCollectionHelperCall(expr, "std/collections/" "soa", "to" "_aos", 1) ||
         isCanonicalCollectionHelperCall(expr, "std/collections/" "soa" "_vector", "to" "_aos", 1);
}

bool isCollectionVectorRecordPath(std::string_view structTypeName) {
  const std::string vectorTypePath = experimentalCollectionTypePath("vec" "tor", "Vector");
  return structTypeName == vectorTypePath ||
         matchesGeneratedSpecializedPath(structTypeName, vectorTypePath);
}

bool isVectorTarget(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() &&
           !it->second.isSoaVector &&
           (it->second.kind == LocalInfo::Kind::Vector ||
            ((it->second.kind == LocalInfo::Kind::Value ||
              it->second.kind == LocalInfo::Kind::Reference ||
              it->second.kind == LocalInfo::Kind::Pointer) &&
             isCollectionVectorRecordPath(it->second.structTypeName)));
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(expr, collection) && collection == "vector") {
      return !hasNamedArguments(expr.argNames);
    }
    if (isPublicOrCompatibilitySoaToAosCall(expr)) {
      return isSoaVectorTarget(expr.args.front(), localsIn);
    }
  }
  return false;
}

bool isInternalSoaMetadataTarget(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(expr.name);
  if (it == localsIn.end()) {
    return false;
  }
  auto trimTypeText = [](std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
      text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
      text.pop_back();
    }
    return text;
  };
  std::string structPath = trimTypeText(it->second.structTypeName);
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
  return structPath == "/std/collections/internal_soa_storage/SoaColumn" ||
         structPath == "/std/collections/internal_soa_storage/SoaFieldView";
}

bool isInternalSoaMetadataHelperPath(std::string_view path) {
  if (path.rfind("/std/collections/internal_soa_storage/SoaColumn", 0) != 0 &&
      path.rfind("/std/collections/internal_soa_storage/SoaFieldView", 0) != 0) {
    return false;
  }
  std::string leaf(path.substr(path.find_last_of('/') == std::string_view::npos
                                   ? 0
                                   : path.find_last_of('/') + 1));
  const size_t generatedSuffix = leaf.find("__");
  if (generatedSuffix != std::string::npos) {
    leaf.erase(generatedSuffix);
  }
  return leaf == "field_count" || leaf == "field_capacity";
}

bool isCollectionVectorTarget(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind != Expr::Kind::Name) {
    return false;
  }
  auto it = localsIn.find(expr.name);
  return it != localsIn.end() &&
         (it->second.kind == LocalInfo::Kind::Value ||
          it->second.kind == LocalInfo::Kind::Reference ||
          it->second.kind == LocalInfo::Kind::Pointer) &&
         isCollectionVectorRecordPath(it->second.structTypeName);
}

bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn) {
  auto isWrappedSoaVectorLocal = [&](const Expr &candidate, bool fromArgsPack) {
    if (candidate.kind != Expr::Kind::Name) {
      return false;
    }
    auto it = localsIn.find(candidate.name);
    if (it == localsIn.end() || !it->second.isSoaVector) {
      return false;
    }
    const LocalInfo::Kind kind = fromArgsPack ? it->second.argsPackElementKind : it->second.kind;
    return (kind == LocalInfo::Kind::Reference && it->second.referenceToVector) ||
           (kind == LocalInfo::Kind::Pointer && it->second.pointerToVector);
  };
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(expr, collection) && collection == "soa" "_vector") {
      return true;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "to_soa") && expr.args.size() == 1) {
      return isVectorTarget(expr.args.front(), localsIn);
    }
    if (isSimpleCallName(expr, "dereference") && expr.args.size() == 1) {
      const Expr &derefTarget = expr.args.front();
      if (isWrappedSoaVectorLocal(derefTarget, false)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(derefTarget, accessName) &&
          derefTarget.args.size() == 2 &&
          isWrappedSoaVectorLocal(derefTarget.args.front(), true)) {
        return true;
      }
    }
  }
  return false;
}

ResolvedInlineCallResult emitResolvedInlineDefinitionCall(
    const Expr &callExpr,
    const Definition *callee,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
  if (!callee) {
    return ResolvedInlineCallResult::NoCallee;
  }
  if (isStructDefinition(*callee) &&
      !isStructConstructorCallShape(callExpr)) {
    return ResolvedInlineCallResult::NoCallee;
  }
  if (callExpr.hasBodyArguments || !callExpr.bodyArguments.empty()) {
    error = "native backend does not support block arguments on calls";
    return ResolvedInlineCallResult::Error;
  }
  if (!emitInlineDefinitionCall(callExpr, *callee)) {
    return ResolvedInlineCallResult::Error;
  }
  return ResolvedInlineCallResult::Emitted;
}

InlineCallDispatchResult tryEmitInlineCallWithCountFallbacksImpl(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<bool(const Expr &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &)> &isSoaVectorReceiverExpr,
    const std::function<bool(const Expr &)> &isCollectionAccessReceiverExpr,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error);

InlineCallDispatchResult tryEmitInlineCallWithCountFallbacks(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<bool(const Expr &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &)> &isCollectionAccessReceiverExpr,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
  return tryEmitInlineCallWithCountFallbacksImpl(
      expr,
      isArrayCountCall,
      isStringCountCall,
      isVectorCapacityCall,
      {},
      isCollectionAccessReceiverExpr,
      resolveMethodCallDefinition,
      resolveDefinitionCall,
      emitInlineDefinitionCall,
      error);
}

InlineCallDispatchResult tryEmitInlineCallWithCountFallbacks(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<bool(const Expr &)> &isVectorCapacityCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
  return tryEmitInlineCallWithCountFallbacksImpl(
      expr,
      isArrayCountCall,
      isStringCountCall,
      isVectorCapacityCall,
      {},
      {},
      resolveMethodCallDefinition,
      resolveDefinitionCall,
      emitInlineDefinitionCall,
      error);
}

InlineCallDispatchResult tryEmitInlineCallWithCountFallbacksImpl(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<bool(const Expr &)> &isVectorCapacityCall,
    const std::function<bool(const Expr &)> &isSoaVectorReceiverExpr,
    const std::function<bool(const Expr &)> &isCollectionAccessReceiverExpr,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
  const Definition *directCallee = nullptr;
  if (!expr.isMethodCall) {
    directCallee = resolveDefinitionCall(expr);
    const std::string directCallPath =
        resolveInlineCallPathWithoutFallbackProbes(expr);
    const std::string normalizedDirectCallPath =
        stripGeneratedInlineHelperSuffix(directCallPath);
    if (directCallee != nullptr &&
        isSoaVectorReceiverExpr != nullptr &&
        !expr.args.empty() &&
        (normalizedDirectCallPath ==
             "/std/collections/experimental" "_soa" "_vector_conversions/soa" "VectorToAos" ||
         normalizedDirectCallPath ==
             "/std/collections/experimental" "_soa" "_vector_conversions/soa" "VectorToAosRef") &&
        isSoaVectorReceiverExpr(expr.args.front())) {
      error = "struct parameter type mismatch: direct experimental soa" "_vector conversion "
              "helpers require Soa" "Vector receiver";
      return InlineCallDispatchResult::Error;
    }
    if (directCallee != nullptr) {
      std::string directVectorHelperName;
      if (expr.args.size() == 2 &&
          resolveVectorHelperAliasName(expr, directVectorHelperName) &&
          (directVectorHelperName == "at" ||
           directVectorHelperName == "at_unsafe")) {
        Expr methodExpr = expr;
        methodExpr.isMethodCall = true;
        methodExpr.isFieldAccess = false;
        methodExpr.semanticNodeId = 0;
        methodExpr.namespacePrefix.clear();
        methodExpr.name = directVectorHelperName;
        if (const Definition *methodCallee =
                resolveMethodCallDefinition(methodExpr)) {
          const auto emitResult = emitResolvedInlineDefinitionCall(
              methodExpr, methodCallee, emitInlineDefinitionCall, error);
          if (emitResult == ResolvedInlineCallResult::Emitted) {
            return InlineCallDispatchResult::Emitted;
          }
          if (emitResult == ResolvedInlineCallResult::Error) {
            return InlineCallDispatchResult::Error;
          }
        }
      }
    }
    if (directCallee != nullptr && isCollectionAccessReceiverExpr &&
        !expr.args.empty() && isCollectionAccessReceiverExpr(expr.args.front()) &&
        (isExplicitDirectKeyValueAccessHelperCall(expr) ||
         isExplicitRemovedKeyValueAccessHelperCall(expr))) {
      return InlineCallDispatchResult::NotHandled;
    }
    if (directCallee != nullptr &&
        prefersBuiltinCountFallbackOverRemovedShadow(
            expr, *directCallee, isArrayCountCall, isStringCountCall)) {
      return InlineCallDispatchResult::NotHandled;
    }
    std::string samePathKeyValueHelperName;
    const bool isSamePathKeyValueCountLikeCall =
        directCallee != nullptr &&
        resolveExplicitSamePathKeyValueCountLikeDefinitionCall(
            expr, *directCallee, samePathKeyValueHelperName);
    if (isSamePathKeyValueCountLikeCall &&
        (samePathKeyValueHelperName == "count" ||
         samePathKeyValueHelperName == "count_ref")) {
      return InlineCallDispatchResult::NotHandled;
    }
    if (directCallee != nullptr &&
        (isSemanticBarePreferredKeyValueHelperDefinitionCall(expr, *directCallee) ||
         isBareDirectWrapperKeyValueAccessDefinitionCall(expr) ||
         isSamePathKeyValueCountLikeCall)) {
      const auto emitResult = emitResolvedInlineDefinitionCall(
          expr, directCallee, emitInlineDefinitionCall, error);
      if (emitResult == ResolvedInlineCallResult::Emitted) {
        return InlineCallDispatchResult::Emitted;
      }
      if (emitResult == ResolvedInlineCallResult::NoCallee) {
        return InlineCallDispatchResult::NotHandled;
      }
      return InlineCallDispatchResult::Error;
    }
  }

  const auto firstCountFallbackResult = tryEmitNonMethodCountFallback(
      expr,
      isArrayCountCall,
      isStringCountCall,
      resolveMethodCallDefinition,
      emitInlineDefinitionCall,
      error,
      isCollectionAccessReceiverExpr);
  if (firstCountFallbackResult == CountMethodFallbackResult::Emitted) {
    return InlineCallDispatchResult::Emitted;
  }
  if (firstCountFallbackResult == CountMethodFallbackResult::Error) {
    return InlineCallDispatchResult::Error;
  }

  if (expr.isMethodCall) {
    std::string accessName;
    const bool isBuiltinAccessMethod = getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2;
    const bool isBuiltinCountName = isSimpleCallName(expr, "count") && expr.args.size() == 1;
    const bool isBuiltinCapacityName = isSimpleCallName(expr, "capacity") && expr.args.size() == 1;
    const bool isBuiltinKeyValueContainsName = isKeyValueContainsHelperName(expr) && expr.args.size() == 2;
    const bool isBuiltinKeyValueTryAtName = isKeyValueTryAtHelperName(expr) && expr.args.size() == 2;
    const bool isBuiltinKeyValueInsertName = isSimpleCallName(expr, "insert") && expr.args.size() == 3;
    const bool isBuiltinCountLikeMethod =
        isBuiltinCountName || isBuiltinCapacityName || isArrayCountCall(expr) || isStringCountCall(expr) ||
        isVectorCapacityCall(expr) || isBuiltinAccessMethod || isBuiltinKeyValueContainsName ||
        isBuiltinKeyValueTryAtName || isBuiltinKeyValueInsertName;
    const Definition *callee = resolveMethodCallDefinition(expr);
    if (callee != nullptr) {
      if (expr.args.size() == 1 &&
          expr.args.front().kind == Expr::Kind::Call &&
          callee->fullPath == canonicalKeyValueHelperPath("count")) {
        return InlineCallDispatchResult::NotHandled;
      }
      if (expr.args.size() == 1 &&
          isInternalSoaMetadataHelperPath(callee->fullPath)) {
        return InlineCallDispatchResult::NotHandled;
      }
      const auto emitResult = emitResolvedInlineDefinitionCall(
          expr, callee, emitInlineDefinitionCall, error);
      if (emitResult == ResolvedInlineCallResult::Emitted) {
        return InlineCallDispatchResult::Emitted;
      }
      if (emitResult == ResolvedInlineCallResult::NoCallee) {
        return InlineCallDispatchResult::NotHandled;
      }
      return InlineCallDispatchResult::Error;
    }
    if (!isBuiltinCountLikeMethod) {
      return InlineCallDispatchResult::Error;
    }
    error.clear();
  }

  if (const Definition *callee =
          directCallee != nullptr ? directCallee : resolveDefinitionCall(expr)) {
    if (expr.args.size() == 1 &&
        isInternalSoaMetadataHelperPath(callee->fullPath)) {
      return InlineCallDispatchResult::NotHandled;
    }
    const auto emitResult = emitResolvedInlineDefinitionCall(
        expr, callee, emitInlineDefinitionCall, error);
    if (emitResult == ResolvedInlineCallResult::Emitted) {
      return InlineCallDispatchResult::Emitted;
    }
    if (emitResult == ResolvedInlineCallResult::NoCallee) {
      return InlineCallDispatchResult::NotHandled;
    }
    return InlineCallDispatchResult::Error;
  }

  const auto secondCountFallbackResult = tryEmitNonMethodCountFallback(
      expr,
      isArrayCountCall,
      isStringCountCall,
      resolveMethodCallDefinition,
      emitInlineDefinitionCall,
      error,
      isCollectionAccessReceiverExpr);
  if (secondCountFallbackResult == CountMethodFallbackResult::Emitted) {
    return InlineCallDispatchResult::Emitted;
  }
  if (secondCountFallbackResult == CountMethodFallbackResult::Error) {
    return InlineCallDispatchResult::Error;
  }

  return InlineCallDispatchResult::NotHandled;
}

InlineCallDispatchResult tryEmitInlineCallDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isArrayCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isStringCountCallFn,
    const std::function<bool(const Expr &, const LocalMap &)> &isVectorCapacityCallFn,
    const std::function<const Definition *(const Expr &, const LocalMap &)> &resolveMethodCallDefinitionFn,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCallFn,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCallFn,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const SemanticProductIndex *semanticIndex) {
  std::optional<SemanticProductIndex> ownedSemanticIndex;
  if (semanticProgram != nullptr && semanticIndex == nullptr) {
    ownedSemanticIndex = buildSemanticProductIndex(semanticProgram);
    semanticIndex = &*ownedSemanticIndex;
  }
  const SemanticProductIndex *const semanticIndexPtr =
      semanticProgram == nullptr ? nullptr : semanticIndex;
  auto resolveInlineSemanticTypeText = [&](SymbolId typeTextId,
                                           const std::string &typeText) {
    if (semanticProgram != nullptr && typeTextId != InvalidSymbolId) {
      const std::string resolvedText =
          std::string(semanticProgramResolveCallTargetString(*semanticProgram,
                                                             typeTextId));
      if (!resolvedText.empty()) {
        return trimTemplateTypeText(resolvedText);
      }
    }
    return trimTemplateTypeText(typeText);
  };
  enum class InlineVectorTargetFact {
    Unknown,
    Vector,
    NonVector,
  };
  enum class InlineCollectionAccessTargetFact {
    Unknown,
    CollectionAccess,
    NonCollectionAccess,
  };
  auto isInlineExperimentalVectorTypeName = [](std::string typeName) {
    typeName = trimTemplateTypeText(typeName);
    const std::string vectorTypePath =
        experimentalCollectionTypePath("vec" "tor", "Vector", false);
    const std::string slashVectorTypePath =
        experimentalCollectionTypePath("vec" "tor", "Vector");
    return typeName == "Vector" ||
           typeName == "/Vector" ||
           typeName == vectorTypePath ||
           typeName == slashVectorTypePath ||
           matchesGeneratedSpecializedPath(typeName, vectorTypePath) ||
           matchesGeneratedSpecializedPath(typeName, slashVectorTypePath);
  };
  std::function<bool(const std::string &)> isInlineVectorTypeText;
  isInlineVectorTypeText = [&](const std::string &typeText) {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
          normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
        return isInlineVectorTypeText(argText);
      }
      return matchesCollectionTypeText(normalizedBase, "vector") ||
             isInlineExperimentalVectorTypeName(normalizedBase);
    }
    const std::string normalizedTypeText = trimTemplateTypeText(typeText);
    return matchesCollectionTypeText(normalizedTypeText, "vector") ||
           isInlineExperimentalVectorTypeName(normalizedTypeText);
  };
  auto classifyInlineVectorTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) {
    return isInlineVectorTypeText(
               resolveInlineSemanticTypeText(typeTextId, typeText))
               ? InlineVectorTargetFact::Vector
               : InlineVectorTargetFact::NonVector;
  };
  auto combineInlineVectorTargetFacts = [](InlineVectorTargetFact lhs,
                                           InlineVectorTargetFact rhs) {
    if (lhs == InlineVectorTargetFact::Vector ||
        rhs == InlineVectorTargetFact::Vector) {
      return InlineVectorTargetFact::Vector;
    }
    if (lhs == InlineVectorTargetFact::NonVector ||
        rhs == InlineVectorTargetFact::NonVector) {
      return InlineVectorTargetFact::NonVector;
    }
    return InlineVectorTargetFact::Unknown;
  };
  auto classifyInlineVectorTargetFromSemanticFacts =
      [&](const Expr &targetExpr) {
    if (semanticProgram == nullptr || semanticIndexPtr == nullptr ||
        targetExpr.semanticNodeId == 0) {
      return InlineVectorTargetFact::Unknown;
    }
    if (const auto *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr);
        collectionFact != nullptr) {
      const std::string collectionFamily =
          resolveInlineSemanticTypeText(collectionFact->collectionFamilyId,
                                        collectionFact->collectionFamily);
      return matchesCollectionTypeText(collectionFamily, "vector")
                 ? InlineVectorTargetFact::Vector
                 : InlineVectorTargetFact::NonVector;
    }
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr);
        queryFact != nullptr) {
      InlineVectorTargetFact fact =
          classifyInlineVectorTypeText(queryFact->bindingTypeTextId,
                                       queryFact->bindingTypeText);
      fact = combineInlineVectorTargetFacts(
          fact,
          classifyInlineVectorTypeText(queryFact->queryTypeTextId,
                                       queryFact->queryTypeText));
      return combineInlineVectorTargetFacts(
          fact,
          classifyInlineVectorTypeText(queryFact->receiverBindingTypeTextId,
                                       queryFact->receiverBindingTypeText));
    }
    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndexPtr, targetExpr);
        bindingFact != nullptr) {
      return classifyInlineVectorTypeText(bindingFact->bindingTypeTextId,
                                          bindingFact->bindingTypeText);
    }
    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr);
        localAutoFact != nullptr) {
      return classifyInlineVectorTypeText(localAutoFact->bindingTypeTextId,
                                          localAutoFact->bindingTypeText);
    }
    return InlineVectorTargetFact::Unknown;
  };
  auto isSemanticOrLegacyVectorTarget = [&](const Expr &targetExpr) {
    const InlineVectorTargetFact semanticFact =
        classifyInlineVectorTargetFromSemanticFacts(targetExpr);
    if (semanticFact == InlineVectorTargetFact::Vector) {
      return true;
    }
    if (semanticFact == InlineVectorTargetFact::NonVector) {
      return false;
    }
    return isVectorTarget(targetExpr, localsIn);
  };
  auto isSemanticOrLegacyExperimentalVectorTarget = [&](const Expr &targetExpr) {
    const InlineVectorTargetFact semanticFact =
        classifyInlineVectorTargetFromSemanticFacts(targetExpr);
    if (semanticFact == InlineVectorTargetFact::Vector) {
      return true;
    }
    if (semanticFact == InlineVectorTargetFact::NonVector) {
      return false;
    }
    return isCollectionVectorTarget(targetExpr, localsIn);
  };
  auto isInlineRawBuiltinSoaVectorTypeText = [&](SymbolId typeTextId,
                                                const std::string &typeText) {
    const std::string normalizedTypeText =
        trimTemplateTypeText(resolveInlineSemanticTypeText(typeTextId, typeText));
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
          normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
        const std::string normalizedArg = trimTemplateTypeText(argText);
        return normalizedArg == "soa" "_vector" ||
               normalizedArg == "/soa" "_vector" ||
               normalizedArg == "std/collections/" "soa" "_vector" ||
               normalizedArg == "/std/collections/" "soa" "_vector";
      }
      return normalizedBase == "soa" "_vector" ||
             normalizedBase == "/soa" "_vector" ||
             normalizedBase == "std/collections/" "soa" "_vector" ||
             normalizedBase == "/std/collections/" "soa" "_vector";
    }
    return normalizedTypeText == "soa" "_vector" ||
           normalizedTypeText == "/soa" "_vector" ||
           normalizedTypeText == "std/collections/" "soa" "_vector" ||
           normalizedTypeText == "/std/collections/" "soa" "_vector";
  };
  std::function<bool(const Expr &)> isRawBuiltinSoaVectorTarget;
  isRawBuiltinSoaVectorTarget = [&](const Expr &targetExpr) {
    if (semanticProgram != nullptr && semanticIndexPtr != nullptr &&
        targetExpr.semanticNodeId != 0) {
      if (const auto *collectionFact =
              findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr);
          collectionFact != nullptr &&
          isInlineRawBuiltinSoaVectorTypeText(collectionFact->collectionFamilyId,
                                             collectionFact->collectionFamily)) {
        return true;
      }
      if (const auto *queryFact =
              findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr);
          queryFact != nullptr) {
        return isInlineRawBuiltinSoaVectorTypeText(queryFact->bindingTypeTextId,
                                                   queryFact->bindingTypeText) ||
               isInlineRawBuiltinSoaVectorTypeText(queryFact->queryTypeTextId,
                                                   queryFact->queryTypeText) ||
               isInlineRawBuiltinSoaVectorTypeText(
                   queryFact->receiverBindingTypeTextId,
                   queryFact->receiverBindingTypeText);
      }
      if (const auto *bindingFact =
              findSemanticProductBindingFact(*semanticIndexPtr, targetExpr);
          bindingFact != nullptr &&
          isInlineRawBuiltinSoaVectorTypeText(bindingFact->bindingTypeTextId,
                                             bindingFact->bindingTypeText)) {
        return true;
      }
      if (const auto *localAutoFact =
              findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr);
          localAutoFact != nullptr &&
          isInlineRawBuiltinSoaVectorTypeText(localAutoFact->bindingTypeTextId,
                                             localAutoFact->bindingTypeText)) {
        return true;
      }
    }
    if (targetExpr.kind == Expr::Kind::Name) {
      const auto localIt = localsIn.find(targetExpr.name);
      return localIt != localsIn.end() &&
             localIt->second.isSoaVector &&
             localIt->second.usesBuiltinCollectionLayout;
    }
    if (targetExpr.kind == Expr::Kind::Call) {
      std::string collection;
      if (getBuiltinCollectionName(targetExpr, collection) && collection == "soa" "_vector") {
        return true;
      }
      if ((isSimpleCallName(targetExpr, "location") ||
           isSimpleCallName(targetExpr, "dereference")) &&
          targetExpr.args.size() == 1) {
        return isRawBuiltinSoaVectorTarget(targetExpr.args.front());
      }
    }
    return false;
  };
  auto emitCanonicalInlineDefinitionCall = [&](const Expr &callExpr, const Definition &callee) {
    if (isTypeNamespaceMethodCallForInlineEmit(callExpr, callee, localsIn)) {
      const Expr directCallExpr = makeInlineEmitDirectTypeNamespaceCall(callExpr, callee);
      return emitInlineDefinitionCallFn(directCallExpr, callee, localsIn);
    }
    return emitInlineDefinitionCallFn(callExpr, callee, localsIn);
  };
  if (expr.isMethodCall && expr.args.size() == 1 &&
      isStringCountCallFn(expr, localsIn)) {
    const Definition *stringCountCallee =
        resolveMethodCallDefinitionFn(expr, localsIn);
    if (stringCountCallee == nullptr) {
      Expr directStringCountExpr = expr;
      directStringCountExpr.isMethodCall = false;
      directStringCountExpr.namespacePrefix.clear();
      directStringCountExpr.name = "/string/count";
      stringCountCallee = resolveDefinitionCallFn(directStringCountExpr);
    }
    if (stringCountCallee != nullptr &&
        stringCountCallee->fullPath == "/string/count") {
      return emitCanonicalInlineDefinitionCall(expr, *stringCountCallee)
                 ? InlineCallDispatchResult::Emitted
                 : InlineCallDispatchResult::Error;
    }
  }
  if (isArrayCountCallFn(expr, localsIn) ||
      isStringCountCallFn(expr, localsIn) ||
      isVectorCapacityCallFn(expr, localsIn)) {
    return InlineCallDispatchResult::NotHandled;
  }
  const auto inferCallKeyValueTargetInfo = [&](const Expr &targetExpr,
                                          CollectionPairTypeInfo &targetInfoOut) {
    targetInfoOut = {};
    const Definition *callee = resolveDefinitionCallFn(targetExpr);
    if (callee == nullptr) {
      return false;
    }
    std::string collectionName;
    std::vector<std::string> collectionArgs;
    if (!inferDeclaredReturnCollection(*callee, collectionName, collectionArgs) ||
        collectionName != "map" ||
        collectionArgs.size() != 2) {
      return inferForwardedCollectionPairTypeInfo(
          targetExpr, *callee, localsIn, {}, targetInfoOut);
    }
    targetInfoOut.isKeyValueTarget = true;
    targetInfoOut.keyValueKeyKind = valueKindFromTypeName(collectionArgs.front());
    targetInfoOut.keyValueValueKind = valueKindFromTypeName(collectionArgs.back());
    return targetInfoOut.keyValueKeyKind != LocalInfo::ValueKind::Unknown &&
           targetInfoOut.keyValueValueKind != LocalInfo::ValueKind::Unknown;
  };
  std::function<bool(const std::string &)> isInlineCollectionAccessTypeText;
  isInlineCollectionAccessTypeText = [&](const std::string &typeText) {
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(trimTemplateTypeText(typeText), base, argText)) {
      const std::string normalizedBase = trimTemplateTypeText(base);
      if (normalizedBase == "Reference" || normalizedBase == "/Reference" ||
          normalizedBase == "Pointer" || normalizedBase == "/Pointer") {
        return isInlineCollectionAccessTypeText(argText);
      }
      return normalizedBase == "array" || normalizedBase == "/array" ||
             normalizedBase == "vector" || normalizedBase == "/vector" ||
             normalizedBase == "Array" || normalizedBase == "/Array" ||
             matchesCollectionTypeText(normalizedBase, "vector") ||
             isInlineExperimentalVectorTypeName(normalizedBase);
    }
    const std::string normalizedTypeText = trimTemplateTypeText(typeText);
    return normalizedTypeText == "string" || normalizedTypeText == "/string" ||
           normalizedTypeText == "String" || normalizedTypeText == "/String" ||
           normalizedTypeText == "array" || normalizedTypeText == "/array" ||
           normalizedTypeText == "vector" || normalizedTypeText == "/vector" ||
           normalizedTypeText == "Array" || normalizedTypeText == "/Array" ||
           matchesCollectionTypeText(normalizedTypeText, "vector") ||
           isInlineExperimentalVectorTypeName(normalizedTypeText);
  };
  auto classifyInlineCollectionAccessTypeText =
      [&](SymbolId typeTextId, const std::string &typeText) {
    return isInlineCollectionAccessTypeText(
               resolveInlineSemanticTypeText(typeTextId, typeText))
               ? InlineCollectionAccessTargetFact::CollectionAccess
               : InlineCollectionAccessTargetFact::NonCollectionAccess;
  };
  auto combineInlineCollectionAccessTargetFacts =
      [](InlineCollectionAccessTargetFact lhs,
         InlineCollectionAccessTargetFact rhs) {
    if (lhs == InlineCollectionAccessTargetFact::CollectionAccess ||
        rhs == InlineCollectionAccessTargetFact::CollectionAccess) {
      return InlineCollectionAccessTargetFact::CollectionAccess;
    }
    if (lhs == InlineCollectionAccessTargetFact::NonCollectionAccess ||
        rhs == InlineCollectionAccessTargetFact::NonCollectionAccess) {
      return InlineCollectionAccessTargetFact::NonCollectionAccess;
    }
    return InlineCollectionAccessTargetFact::Unknown;
  };
  auto classifyInlineCollectionAccessTargetFromSemanticFacts =
      [&](const Expr &targetExpr) {
    if (semanticProgram == nullptr || semanticIndexPtr == nullptr ||
        targetExpr.semanticNodeId == 0) {
      return InlineCollectionAccessTargetFact::Unknown;
    }
    if (const auto *collectionFact =
            findSemanticProductCollectionSpecialization(*semanticIndexPtr, targetExpr);
        collectionFact != nullptr) {
      const std::string collectionFamily =
          resolveInlineSemanticTypeText(collectionFact->collectionFamilyId,
                                        collectionFact->collectionFamily);
      return collectionFamily == "array" || collectionFamily == "/array" ||
             collectionFamily == "vector" || collectionFamily == "/vector" ||
             collectionFamily == "string" || collectionFamily == "/string" ||
             matchesCollectionTypeText(collectionFamily, "vector") ||
             isInlineExperimentalVectorTypeName(collectionFamily)
                 ? InlineCollectionAccessTargetFact::CollectionAccess
                 : InlineCollectionAccessTargetFact::NonCollectionAccess;
    }
    if (const auto *queryFact =
            findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr, targetExpr);
        queryFact != nullptr) {
      InlineCollectionAccessTargetFact fact =
          classifyInlineCollectionAccessTypeText(queryFact->bindingTypeTextId,
                                                 queryFact->bindingTypeText);
      fact = combineInlineCollectionAccessTargetFacts(
          fact,
          classifyInlineCollectionAccessTypeText(queryFact->queryTypeTextId,
                                                 queryFact->queryTypeText));
      return combineInlineCollectionAccessTargetFacts(
          fact,
          classifyInlineCollectionAccessTypeText(
              queryFact->receiverBindingTypeTextId,
              queryFact->receiverBindingTypeText));
    }
    if (const auto *bindingFact =
            findSemanticProductBindingFact(*semanticIndexPtr, targetExpr);
        bindingFact != nullptr) {
      return classifyInlineCollectionAccessTypeText(
          bindingFact->bindingTypeTextId,
          bindingFact->bindingTypeText);
    }
    if (const auto *localAutoFact =
            findSemanticProductLocalAutoFact(semanticProgram, *semanticIndexPtr, targetExpr);
        localAutoFact != nullptr) {
      return classifyInlineCollectionAccessTypeText(
          localAutoFact->bindingTypeTextId,
          localAutoFact->bindingTypeText);
    }
    return InlineCollectionAccessTargetFact::Unknown;
  };
  if (!expr.isMethodCall) {
    std::string keyValueHelperName;
    const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
    if (const Definition *directCallee = resolveDefinitionCallFn(expr);
        directCallee != nullptr &&
        isSemanticBarePreferredKeyValueHelperDefinitionCall(expr,
                                                            *directCallee)) {
      return emitCanonicalInlineDefinitionCall(expr, *directCallee)
                 ? InlineCallDispatchResult::Emitted
                 : InlineCallDispatchResult::Error;
    }
    if (expr.args.size() == 1 &&
        isSemanticOrLegacyVectorTarget(expr.args.front())) {
      std::string vectorHelperName;
      if (resolveVectorHelperAliasName(expr, vectorHelperName) &&
          (vectorHelperName == "count" || vectorHelperName == "capacity")) {
        return InlineCallDispatchResult::NotHandled;
      }
      const size_t rawLeafStart = rawPath.find_last_of('/');
      std::string rawLeaf = rawLeafStart == std::string::npos
                                ? rawPath
                                : rawPath.substr(rawLeafStart + 1);
      rawLeaf = stripGeneratedInlineHelperSuffix(std::move(rawLeaf));
      if (rawLeaf == collectionWrapperAlias("vector", "Count") ||
          rawLeaf == collectionWrapperAlias("vector", "Capacity")) {
        return InlineCallDispatchResult::NotHandled;
      }
      if (const Definition *callee = resolveDefinitionCallFn(expr);
          callee != nullptr) {
        const size_t leafStart = callee->fullPath.find_last_of('/');
        std::string leaf = leafStart == std::string::npos
                               ? callee->fullPath
                               : callee->fullPath.substr(leafStart + 1);
        leaf = stripGeneratedInlineHelperSuffix(std::move(leaf));
        if (leaf == collectionWrapperAlias("vector", "Count") ||
            leaf == collectionWrapperAlias("vector", "Capacity")) {
          return InlineCallDispatchResult::NotHandled;
        }
      }
    }
    if (expr.args.size() == 2 &&
        isSemanticOrLegacyVectorTarget(expr.args.front())) {
      std::string vectorHelperName;
      if (resolveVectorHelperAliasName(expr, vectorHelperName) &&
          (vectorHelperName == "at" || vectorHelperName == "at_unsafe")) {
        const bool isExplicitCanonicalVectorHelper =
            rawPath.rfind(collectionMemberRoot("vector"), 0) == 0 ||
            rawPath.rfind(collectionMemberRoot("vector", false), 0) == 0;
        if (isExplicitCanonicalVectorHelper) {
          if (const Definition *callee = resolveDefinitionCallFn(expr);
              callee != nullptr &&
              !keepsBuiltinInlineReturnForPublishedVectorHelper(vectorHelperName, *callee)) {
            return emitCanonicalInlineDefinitionCall(expr, *callee)
                       ? InlineCallDispatchResult::Emitted
                       : InlineCallDispatchResult::Error;
          }
        }
        return InlineCallDispatchResult::NotHandled;
      }
      const size_t rawLeafStart = rawPath.find_last_of('/');
      std::string rawLeaf = rawLeafStart == std::string::npos
                                ? rawPath
                                : rawPath.substr(rawLeafStart + 1);
      rawLeaf = stripGeneratedInlineHelperSuffix(std::move(rawLeaf));
      if (rawLeaf == collectionWrapperAlias("vector", "At") ||
          rawLeaf == collectionWrapperAlias("vector", "AtUnsafe") ||
          rawLeaf == "at" || rawLeaf == "at_unsafe") {
        return InlineCallDispatchResult::NotHandled;
      }
      if (const Definition *callee = resolveDefinitionCallFn(expr);
          callee != nullptr) {
        const size_t leafStart = callee->fullPath.find_last_of('/');
        std::string leaf = leafStart == std::string::npos
                               ? callee->fullPath
                               : callee->fullPath.substr(leafStart + 1);
        leaf = stripGeneratedInlineHelperSuffix(std::move(leaf));
        if (leaf == collectionWrapperAlias("vector", "At") ||
            leaf == collectionWrapperAlias("vector", "AtUnsafe") ||
            leaf == "at" || leaf == "at_unsafe") {
          return InlineCallDispatchResult::NotHandled;
        }
      }
    }
    std::string experimentalVectorElementType;
    if (getExperimentalVectorConstructorElementTypeAlias(
            expr, experimentalVectorElementType)) {
      Expr rewrittenVectorCtor = expr;
      rewrittenVectorCtor.name = experimentalCollectionMemberPath("vector", "vector");
      rewrittenVectorCtor.namespacePrefix.clear();
      rewrittenVectorCtor.templateArgs = {experimentalVectorElementType};
      const Definition *vectorCtor =
          resolveDefinitionCallFn(rewrittenVectorCtor);
      if (vectorCtor != nullptr) {
        return emitCanonicalInlineDefinitionCall(rewrittenVectorCtor,
                                                 *vectorCtor)
                   ? InlineCallDispatchResult::Emitted
                   : InlineCallDispatchResult::Error;
      }
    }
    const bool isCanonicalStdKeyValueHelperCall =
        isCanonicalPublishedInlineKeyValueHelperPath(rawPath);
    if (isCanonicalStdKeyValueHelperCall && !expr.args.empty()) {
      const auto targetInfo =
          resolveCollectionPairTypeInfo(expr.args.front(),
                                     localsIn,
                                     inferCallKeyValueTargetInfo,
                                     semanticProgram,
                                     semanticIndexPtr);
      std::string directHelperName = rawPath;
      const size_t lastSlash = directHelperName.find_last_of('/');
      if (lastSlash != std::string::npos) {
        directHelperName = directHelperName.substr(lastSlash + 1);
      }
      directHelperName = canonicalInlineKeyValueHelperName(std::move(directHelperName));
      if (targetInfo.isKeyValueTarget &&
          (directHelperName == "count" || directHelperName == "contains" ||
           directHelperName == "tryAt" || directHelperName == "at" ||
           directHelperName == "at_unsafe")) {
        if (directHelperName == "count" &&
            expr.args.front().kind == Expr::Kind::Call) {
          return InlineCallDispatchResult::NotHandled;
        }
        if (const Definition *callee = resolveDefinitionCallFn(expr);
            callee != nullptr) {
          return emitCanonicalInlineDefinitionCall(expr, *callee)
                     ? InlineCallDispatchResult::Emitted
                     : InlineCallDispatchResult::Error;
        }
        return InlineCallDispatchResult::NotHandled;
      }
      if (!targetInfo.isKeyValueTarget && expr.args.size() >= 2 &&
          (directHelperName == "contains" || directHelperName == "tryAt" ||
           directHelperName == "at" || directHelperName == "at_unsafe")) {
        const auto alternateTargetInfo =
            resolveCollectionPairTypeInfo(expr.args[1],
                                       localsIn,
                                       inferCallKeyValueTargetInfo,
                                       semanticProgram,
                                       semanticIndexPtr);
        if (alternateTargetInfo.isKeyValueTarget) {
          const LocalInfo::ValueKind keyKind =
              inferExprKind ? inferExprKind(expr.args.front(), localsIn)
                            : LocalInfo::ValueKind::Unknown;
          if (keyKind == LocalInfo::ValueKind::Unknown ||
              alternateTargetInfo.keyValueKeyKind == LocalInfo::ValueKind::Unknown ||
              keyKind != alternateTargetInfo.keyValueKeyKind) {
            return InlineCallDispatchResult::NotHandled;
          }
          if (const Definition *callee = resolveDefinitionCallFn(expr);
              callee != nullptr) {
            return emitCanonicalInlineDefinitionCall(expr, *callee)
                       ? InlineCallDispatchResult::Emitted
                       : InlineCallDispatchResult::Error;
          }
        }
      }
    }
    if (!expr.args.empty() &&
        (resolveKeyValueHelperAliasName(expr, keyValueHelperName) ||
         (getBuiltinArrayAccessName(expr, keyValueHelperName) && expr.args.size() == 2))) {
      keyValueHelperName = canonicalInlineKeyValueHelperName(std::move(keyValueHelperName));
      const auto keyValueTargetInfo =
          resolveCollectionPairTypeInfo(expr.args.front(),
                                     localsIn,
                                     inferCallKeyValueTargetInfo,
                                     semanticProgram,
                                     semanticIndexPtr);
      auto isRewrittenSlashMethodKeyValueAccess = [&]() {
        if (semanticProgram == nullptr) {
          return false;
        }
        const SemanticProgramQueryFact *queryFact = nullptr;
        if (semanticIndexPtr != nullptr) {
          queryFact =
              findSemanticProductQueryFact(semanticProgram, *semanticIndexPtr,
                                           expr);
        }
        if (queryFact == nullptr) {
          std::vector<std::pair<int, int>> sourcePositions;
          if (expr.sourceLine != 0 && expr.sourceColumn != 0) {
            sourcePositions.emplace_back(expr.sourceLine, expr.sourceColumn);
          }
          if (!expr.args.empty() && expr.args.front().sourceLine != 0 &&
              expr.args.front().sourceColumn != 0) {
            sourcePositions.emplace_back(expr.args.front().sourceLine,
                                         expr.args.front().sourceColumn);
          }
          for (const auto &candidate : semanticProgram->queryFacts) {
            const bool sameSourcePosition =
                std::any_of(sourcePositions.begin(), sourcePositions.end(),
                            [&](const auto &sourcePosition) {
                              return candidate.sourceLine == sourcePosition.first &&
                                     candidate.sourceColumn == sourcePosition.second;
                            });
            if (!sameSourcePosition) {
              continue;
            }
            const std::string_view callName =
                candidate.callNameId != InvalidSymbolId
                    ? semanticProgramResolveCallTargetString(
                          *semanticProgram, candidate.callNameId)
                    : std::string_view(candidate.callName);
            if (callName == expr.name ||
                (!expr.sourceName.empty() && callName == expr.sourceName)) {
              queryFact = &candidate;
              break;
            }
          }
        }
        if (queryFact == nullptr ||
            queryFact->resolvedPathId == InvalidSymbolId) {
          return false;
        }
        const std::string resolvedPath =
            std::string(semanticProgramResolveCallTargetString(
                *semanticProgram, queryFact->resolvedPathId));
        if (resolvedPath != "/at" && resolvedPath != "/at_unsafe") {
          return false;
        }
        const std::string queryType =
            trimTemplateTypeText(resolveInlineSemanticTypeText(
                queryFact->queryTypeTextId, queryFact->queryTypeText));
        const std::string bindingType =
            trimTemplateTypeText(resolveInlineSemanticTypeText(
                queryFact->bindingTypeTextId, queryFact->bindingTypeText));
        return queryType == "string" || queryType == "/string" ||
               bindingType == "string" || bindingType == "/string";
      };
      if (keyValueTargetInfo.isKeyValueTarget && !isCanonicalStdKeyValueHelperCall &&
          (expr.sourceIsMethodCall || isRewrittenSlashMethodKeyValueAccess() ||
           expr.name.find('/') == std::string::npos) &&
          (keyValueHelperName == "at" || keyValueHelperName == "at_unsafe")) {
        Expr canonicalKeyValueHelperExpr = expr;
        canonicalKeyValueHelperExpr.isMethodCall = false;
        canonicalKeyValueHelperExpr.isFieldAccess = false;
        canonicalKeyValueHelperExpr.namespacePrefix.clear();
        canonicalKeyValueHelperExpr.name = canonicalKeyValueHelperPath(keyValueHelperName);
        if (const Definition *callee =
                resolveDefinitionCallFn(canonicalKeyValueHelperExpr);
            callee != nullptr &&
            !keepsBuiltinInlineReturnForPublishedKeyValueHelper(
                keyValueHelperName, *callee)) {
          return emitCanonicalInlineDefinitionCall(canonicalKeyValueHelperExpr,
                                                   *callee)
                     ? InlineCallDispatchResult::Emitted
                     : InlineCallDispatchResult::Error;
        }
      }
      if (keyValueTargetInfo.isKeyValueTarget &&
          !isCanonicalStdKeyValueHelperCall &&
          resolveDefinitionCallFn(expr) == nullptr) {
        return InlineCallDispatchResult::NotHandled;
      }
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      const auto targetInfo =
          resolveArrayVectorAccessTargetInfo(expr.args.front(),
                                             localsIn,
                                             {},
                                             semanticProgram,
                                             semanticIndexPtr);
      if (targetInfo.isArgsPackTarget) {
        return InlineCallDispatchResult::NotHandled;
      }
    }
  }
  auto isVectorReturningCallTarget = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding) {
      return false;
    }
    const InlineVectorTargetFact semanticFact =
        classifyInlineVectorTargetFromSemanticFacts(receiverExpr);
    if (semanticFact == InlineVectorTargetFact::Vector) {
      return true;
    }
    const auto receiverTargetInfo =
        resolveArrayVectorAccessTargetInfo(receiverExpr,
                                           localsIn,
                                           {},
                                           semanticProgram,
                                           semanticIndexPtr);
    if (receiverTargetInfo.isArrayOrVectorTarget &&
        receiverTargetInfo.isVectorTarget &&
        !receiverTargetInfo.isArgsPackTarget) {
      return true;
    }
    if (semanticFact == InlineVectorTargetFact::NonVector) {
      return false;
    }
    const Definition *receiverDef = resolveDefinitionCallFn(receiverExpr);
    if (receiverDef == nullptr) {
      return false;
    }
    std::string collectionName;
    std::vector<std::string> collectionArgs;
    return inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs) &&
           collectionName == "vector" && collectionArgs.size() == 1;
  };

  bool deferVectorReturningMutatorCall = false;
  auto isVectorMutatorCallName = [&](const Expr &callExpr) {
    return isUnqualifiedCollectionBuiltinName(callExpr, "push") ||
           isUnqualifiedCollectionBuiltinName(callExpr, "pop") ||
           isUnqualifiedCollectionBuiltinName(callExpr, "reserve") ||
           isUnqualifiedCollectionBuiltinName(callExpr, "clear") ||
           isUnqualifiedCollectionBuiltinName(callExpr, "remove_at") ||
           isUnqualifiedCollectionBuiltinName(callExpr, "remove_swap");
  };
  auto tryEmitVectorMutatorCallFormExpr = [&]() {
    const bool isVectorMutatorCall = isVectorMutatorCallName(expr);
    if (expr.isMethodCall || !isVectorMutatorCall || expr.args.empty()) {
      return InlineCallDispatchResult::NotHandled;
    }

    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= expr.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };

    const bool hasNamedArgs = hasNamedArguments(expr.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
    }

    const bool probePositionalReorderedReceiver =
        !hasNamedArgs && expr.args.size() > 1 &&
        (expr.args.front().kind == Expr::Kind::Literal ||
         expr.args.front().kind == Expr::Kind::BoolLiteral ||
         expr.args.front().kind == Expr::Kind::FloatLiteral ||
         expr.args.front().kind == Expr::Kind::StringLiteral ||
         expr.args.front().kind == Expr::Kind::Name);
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }

    for (size_t receiverIndex : receiverIndices) {
      Expr methodExpr = expr;
      methodExpr.isMethodCall = true;
      methodExpr.semanticNodeId = 0;
      std::string normalizedHelperName;
      if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
        methodExpr.name = normalizedHelperName;
      }
      if (receiverIndex != 0) {
        std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
        if (methodExpr.argNames.size() < methodExpr.args.size()) {
          methodExpr.argNames.resize(methodExpr.args.size());
        }
        std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
      }
      if (isVectorReturningCallTarget(methodExpr.args.front())) {
        deferVectorReturningMutatorCall = true;
        return InlineCallDispatchResult::NotHandled;
      }
      const std::string priorError = error;
      const Definition *callee = resolveMethodCallDefinitionFn(methodExpr, localsIn);
      if (callee == nullptr) {
        error = priorError;
        continue;
      }
      if (methodExpr.args.size() == 1 &&
          isInternalSoaMetadataHelperPath(callee->fullPath)) {
        error = priorError;
        return InlineCallDispatchResult::NotHandled;
      }
      if (methodExpr.hasBodyArguments || !methodExpr.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return InlineCallDispatchResult::Error;
      }
      if (!emitCanonicalInlineDefinitionCall(methodExpr, *callee)) {
        return InlineCallDispatchResult::Error;
      }
      error = priorError;
      return InlineCallDispatchResult::Emitted;
    }

    return InlineCallDispatchResult::NotHandled;
  };
  const auto vectorMutatorCallFormResult = tryEmitVectorMutatorCallFormExpr();
  if (vectorMutatorCallFormResult != InlineCallDispatchResult::NotHandled) {
    return vectorMutatorCallFormResult;
  }
  if (expr.isMethodCall && expr.args.size() == 1 &&
      (isSimpleCallName(expr, "field_count") || isSimpleCallName(expr, "field_capacity")) &&
      isSemanticOrLegacyVectorTarget(expr.args.front())) {
    return InlineCallDispatchResult::NotHandled;
  }
  if (expr.isMethodCall && expr.args.size() == 1 &&
      (isSimpleCallName(expr, "field_count") ||
       isSimpleCallName(expr, "field_capacity")) &&
      isInternalSoaMetadataTarget(expr.args.front(), localsIn)) {
    return InlineCallDispatchResult::NotHandled;
  }
  if (deferVectorReturningMutatorCall) {
    return InlineCallDispatchResult::NotHandled;
  }
  if (!expr.isMethodCall && !expr.args.empty() &&
      isVectorReturningCallTarget(expr.args.front())) {
    std::string helperName;
    if (resolveVectorHelperAliasName(expr, helperName) &&
        (helperName == "count" || helperName == "capacity" ||
         helperName == "at" || helperName == "at_unsafe")) {
      return InlineCallDispatchResult::NotHandled;
    }
  }
  if (expr.isMethodCall && !expr.args.empty() &&
      isVectorReturningCallTarget(expr.args.front()) &&
      (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe"))) {
    return InlineCallDispatchResult::NotHandled;
  }
  if (expr.isMethodCall && expr.args.size() == 2) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && accessName == "at" &&
        isSemanticOrLegacyVectorTarget(expr.args.front())) {
      return InlineCallDispatchResult::NotHandled;
    }
  }
  if (expr.isMethodCall && !expr.args.empty() &&
      isVectorReturningCallTarget(expr.args.front()) &&
      isVectorMutatorCallName(expr)) {
    return InlineCallDispatchResult::NotHandled;
  }
  if (expr.isMethodCall && expr.args.size() == 1 &&
      (isSimpleCallName(expr, "count") || isSimpleCallName(expr, "capacity")) &&
      (isSemanticOrLegacyVectorTarget(expr.args.front()) ||
       isVectorReturningCallTarget(expr.args.front()))) {
    const Definition *callee = resolveMethodCallDefinitionFn(expr, localsIn);
    if (callee != nullptr &&
        callee->fullPath == rootCollectionMemberPath("vector", "count")) {
      return InlineCallDispatchResult::NotHandled;
    }
    if (callee != nullptr &&
        callee->fullPath == rootCollectionMemberPath("vector", "capacity")) {
      if (isSemanticOrLegacyExperimentalVectorTarget(expr.args.front()) ||
          isVectorReturningCallTarget(expr.args.front())) {
        return InlineCallDispatchResult::NotHandled;
      }
      error =
          "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
          expr.name + ", name=" + expr.name +
          ", args=" + std::to_string(expr.args.size()) +
          ", method=true)";
      return InlineCallDispatchResult::Error;
    }
  }
  auto isPublishedKeyValueLocalName = [&](const Expr &receiverExpr) {
    if (semanticProgram == nullptr || receiverExpr.kind != Expr::Kind::Name) {
      return false;
    }
    const auto *metadata = inlineKeyValueHelperMetadata();
    if (metadata == nullptr) {
      return false;
    }
    return std::any_of(
        semanticProgram->collectionSpecializations.begin(),
        semanticProgram->collectionSpecializations.end(),
        [&](const SemanticProgramCollectionSpecialization &collectionFact) {
          return collectionFact.name == receiverExpr.name &&
                 collectionFact.helperSurfaceId.has_value() &&
                 *collectionFact.helperSurfaceId == metadata->id;
        });
  };
  if (!expr.isMethodCall && expr.args.size() == 2 &&
      (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe")) &&
      (isPublishedKeyValueLocalName(expr.args[0]) ||
       isPublishedKeyValueLocalName(expr.args[1]))) {
    return InlineCallDispatchResult::NotHandled;
  }
  const auto inlineResult = tryEmitInlineCallWithCountFallbacksImpl(
      expr,
      [&](const Expr &callExpr) { return isArrayCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isStringCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isVectorCapacityCallFn(callExpr, localsIn); },
      [&](const Expr &receiverExpr) { return isRawBuiltinSoaVectorTarget(receiverExpr); },
      [&](const Expr &receiverExpr) {
        if (receiverExpr.kind == Expr::Kind::StringLiteral) {
          return true;
        }
        if (receiverExpr.kind == Expr::Kind::Name) {
          if (resolveCollectionPairTypeInfo(receiverExpr,
                                         localsIn,
                                         inferCallKeyValueTargetInfo,
                                         semanticProgram,
                                         semanticIndexPtr)
                  .isKeyValueTarget) {
            return true;
          }
          const InlineCollectionAccessTargetFact semanticFact =
              classifyInlineCollectionAccessTargetFromSemanticFacts(receiverExpr);
          if (semanticFact == InlineCollectionAccessTargetFact::CollectionAccess) {
            return true;
          }
          if (semanticFact == InlineCollectionAccessTargetFact::NonCollectionAccess) {
            return false;
          }
          const LocalInfo::ValueKind inferredReceiverKind =
              inferExprKind ? inferExprKind(receiverExpr, localsIn) : LocalInfo::ValueKind::Unknown;
          if (inferredReceiverKind == LocalInfo::ValueKind::String) {
            return true;
          }
          auto it = localsIn.find(receiverExpr.name);
          if (it == localsIn.end()) {
            return false;
          }
          const LocalInfo &info = it->second;
          if (info.isArgsPack) {
            return false;
          }
          if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
              (info.kind == LocalInfo::Kind::Value &&
               (isCollectionVectorRecordPath(info.structTypeName) ||
                !info.structTypeName.empty()))) {
            return true;
          }
          if (info.kind == LocalInfo::Kind::Reference &&
              (info.referenceToArray || info.referenceToVector ||
               !info.structTypeName.empty())) {
            return true;
          }
          if (info.kind == LocalInfo::Kind::Pointer &&
              (info.pointerToArray || info.pointerToVector ||
               !info.structTypeName.empty() ||
               info.pointerToBuffer)) {
            return true;
          }
          if (inferredReceiverKind != LocalInfo::ValueKind::Unknown) {
            return false;
          }
          return info.valueKind == LocalInfo::ValueKind::String;
        }
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding) {
          return false;
        }
        std::string keyValueHelperName;
        if (resolveKeyValueHelperAliasName(receiverExpr, keyValueHelperName) &&
            (keyValueHelperName == "at" || keyValueHelperName == "at_unsafe" ||
             keyValueHelperName == "tryAt" || keyValueHelperName == "contains") &&
            receiverExpr.args.size() == 2) {
          return false;
        }
        if (resolveCollectionPairTypeInfo(receiverExpr,
                                       localsIn,
                                       inferCallKeyValueTargetInfo,
                                       semanticProgram,
                                       semanticIndexPtr)
                .isKeyValueTarget) {
          return true;
        }
        const InlineCollectionAccessTargetFact semanticFact =
            classifyInlineCollectionAccessTargetFromSemanticFacts(receiverExpr);
        if (semanticFact == InlineCollectionAccessTargetFact::CollectionAccess) {
          return true;
        }
        if (semanticFact == InlineCollectionAccessTargetFact::NonCollectionAccess) {
          return false;
        }
        const auto arrayVectorTargetInfo =
            resolveArrayVectorAccessTargetInfo(receiverExpr,
                                               localsIn,
                                               {},
                                               semanticProgram,
                                               semanticIndexPtr);
        if (arrayVectorTargetInfo.isArrayOrVectorTarget) {
          return true;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(receiverExpr, collectionName)) {
          return collectionName == "array" || collectionName == "vector" ||
                 collectionName == "map" || collectionName == "string";
        }
        const Definition *receiverDef = resolveDefinitionCallFn(receiverExpr);
        if (receiverDef == nullptr) {
          return false;
        }
        std::vector<std::string> collectionArgs;
        if (inferDeclaredReturnCollection(*receiverDef, collectionName, collectionArgs)) {
          return collectionName == "array" || collectionName == "vector" ||
                 collectionName == "map" || collectionName == "string";
        }
        return false;
      },
      [&](const Expr &callExpr) {
        const Definition *callee = resolveMethodCallDefinitionFn(callExpr, localsIn);
        if (callee != nullptr && callExpr.isMethodCall && callExpr.args.size() == 1 &&
            isInternalSoaMetadataHelperPath(callee->fullPath)) {
          return static_cast<const Definition *>(nullptr);
        }
        return callee;
      },
      [&](const Expr &callExpr) { return resolveDefinitionCallFn(callExpr); },
      [&](const Expr &callExpr, const Definition &callee) {
        return emitCanonicalInlineDefinitionCall(callExpr, callee);
      },
      error);
  return inlineResult;
}

} // namespace primec::ir_lowerer
