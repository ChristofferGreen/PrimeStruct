#pragma once

// Helpers shared by the IrLowererInlineNativeCallDispatch*.cpp units (split out of
// IrLowererInlineNativeCallDispatch.cpp without changes).
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "primec/ir_lowerer/IrLowererHelpers.h"
#include "primec/ir_lowerer/IrLowererSemanticProductTargetAdapters.h"
#include "primec/ir_lowerer/IrLowererSetupTypeCollectionHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/ast/AstCallPathHelpers.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

namespace inline_native_call_dispatch_file_local {

inline std::string stripGeneratedInlineHelperSuffix(std::string helperName) {
  const size_t generatedSuffix = helperName.find("__");
  if (generatedSuffix != std::string::npos) {
    helperName.erase(generatedSuffix);
  }
  return helperName;
}

inline std::string experimentalCollectionMemberPath(std::string_view collectionName,
                                             std::string_view memberName,
                                             bool leadingSlash = true) {
  return experimentalCollectionMemberRoot(collectionName, leadingSlash) +
         std::string(memberName);
}

inline std::string rootCollectionMemberPath(std::string_view collectionName,
                                     std::string_view memberName) {
  return "/" + std::string(collectionName) + "/" + std::string(memberName);
}

inline bool matchesGeneratedSpecializedPath(std::string_view text,
                                     const std::string &basePath) {
  return text.rfind(basePath + "__", 0) == 0;
}

inline bool matchesCollectionTypeText(std::string_view text,
                               std::string_view collectionName) {
  const std::string bare(collectionName);
  const std::string rooted = "/" + bare;
  return text == bare ||
         text == rooted ||
         text == collectionTypePath(collectionName, false) ||
         text == collectionTypePath(collectionName);
}

inline std::string canonicalInlineKeyValueHelperName(std::string helperName) {
  helperName = stripGeneratedInlineHelperSuffix(std::move(helperName));
  return helperName;
}

inline const StdlibSurfaceMetadata *inlineKeyValueHelperMetadata() {
  return keyValueHelperSurfaceMetadata();
}

inline bool resolvePublishedInlineKeyValueHelperName(
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

inline bool resolvePublishedInlineKeyValueSurfaceMemberName(std::string_view path,
                                                     std::string &helperNameOut) {
  const auto *metadata = inlineKeyValueHelperMetadata();
  return metadata != nullptr &&
         resolvePublishedStdlibSurfaceMemberName(path, metadata->id, helperNameOut);
}

inline bool isCanonicalPublishedInlineKeyValueHelperPath(std::string_view path) {
  const auto *metadata = inlineKeyValueHelperMetadata();
  return metadata != nullptr &&
         isCanonicalPublishedStdlibSurfaceHelperPath(path, metadata->id);
}

inline std::string resolveInlineCallPathWithoutFallbackProbes(const Expr &expr) {
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

inline bool isSemanticBarePublishedKeyValueHelperCall(const Expr &expr,
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

inline bool resolveExplicitSamePathKeyValueCountLikeDefinitionCall(
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
      helperName != "tryAt" && helperName != collection_helpers::kCountRef &&
      helperName != collection_helpers::kContainsRef && helperName != collection_helpers::kTryAtRef) {
    return false;
  }
  if (normalizeCollectionHelperPath(rawPath) !=
      normalizeCollectionHelperPath(callee.fullPath)) {
    return false;
  }
  helperNameOut = std::move(helperName);
  return true;
}

inline bool isDirectKeyValueAccessHelperCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  std::string helperName;
  return resolveKeyValueHelperAliasName(expr, helperName) &&
         (helperName == "at" || helperName == "at_unsafe");
}

inline bool isExplicitDirectKeyValueAccessHelperCall(const Expr &expr) {
  if (!isDirectKeyValueAccessHelperCall(expr)) {
    return false;
  }
  const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  return !rawPath.empty() && rawPath.front() == '/';
}

inline bool isExplicitRemovedKeyValueAccessHelperCall(const Expr &expr) {
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
          helperName == collection_helpers::kAtRef || helperName == collection_helpers::kAtUnsafeRef);
}

inline bool isSemanticBarePreferredKeyValueHelperDefinitionCall(const Expr &expr,
                                                         const Definition &callee) {
  std::string helperName;
  if (!resolvePublishedInlineKeyValueHelperName(callee.fullPath, helperName)) {
    return false;
  }
  return isSemanticBarePublishedKeyValueHelperCall(
      expr, canonicalInlineKeyValueHelperName(helperName));
}

inline bool isBareDirectWrapperKeyValueAccessDefinitionCall(const Expr &expr) {
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

inline bool prefersBuiltinCountFallbackOverRemovedShadow(
    const Expr &expr,
    const Definition &callee,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || expr.name != "count" ||
      !expr.namespacePrefix.empty() || expr.args.size() != 1) {
    return false;
  }
  if (callee.fullPath == collection_helpers::kRootedArrayCount) {
    return isArrayCountCall(expr);
  }
  if (callee.fullPath == collection_helpers::kRootedStringCount) {
    return isStringCountCall(expr);
  }
  return false;
}

inline bool keepsBuiltinInlineReturnForPublishedKeyValueHelper(std::string_view helperName,
                                                   const Definition &callee) {
  std::string declaredReturnType;
  if (!inferReceiverTypeFromDeclaredReturn(callee, declaredReturnType)) {
    return true;
  }
  declaredReturnType = trimTemplateTypeText(declaredReturnType);
  if (!declaredReturnType.empty() && declaredReturnType.front() == '/') {
    declaredReturnType.erase(declaredReturnType.begin());
  }
  if (collection_helpers::isContainsHelperName(helperName)) {
    return declaredReturnType == "bool";
  }
  if (collection_helpers::isTryAtHelperName(helperName)) {
    return declaredReturnType == "Result";
  }
  if (collection_helpers::isAtHelperName(helperName) ||
      collection_helpers::isAtUnsafeHelperName(helperName)) {
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

inline bool isTypeNamespaceMethodCallForInlineEmit(const Expr &callExpr,
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

inline Expr makeInlineEmitDirectTypeNamespaceCall(const Expr &callExpr, const Definition &callee) {
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

} // namespace inline_native_call_dispatch_file_local
} // namespace primec::ir_lowerer
