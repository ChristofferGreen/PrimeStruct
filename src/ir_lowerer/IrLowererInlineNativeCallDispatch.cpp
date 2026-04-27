#include "IrLowererCallHelpers.h"

#include <string>
#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
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

std::string canonicalInlineMapHelperName(std::string helperName) {
  helperName = stripGeneratedInlineHelperSuffix(std::move(helperName));
  if (helperName == "mapCount") {
    return "count";
  }
  if (helperName == "mapCountRef") {
    return "count_ref";
  }
  if (helperName == "mapContains") {
    return "contains";
  }
  if (helperName == "mapContainsRef") {
    return "contains_ref";
  }
  if (helperName == "mapTryAt") {
    return "tryAt";
  }
  if (helperName == "mapTryAtRef") {
    return "tryAt_ref";
  }
  if (helperName == "mapAt") {
    return "at";
  }
  if (helperName == "mapAtRef") {
    return "at_ref";
  }
  if (helperName == "mapAtUnsafe") {
    return "at_unsafe";
  }
  if (helperName == "mapAtUnsafeRef") {
    return "at_unsafe_ref";
  }
  if (helperName == "mapInsert") {
    return "insert";
  }
  if (helperName == "mapInsertRef") {
    return "insert_ref";
  }
  return helperName;
}

bool resolvePublishedInlineMapHelperName(std::string_view resolvedPath, std::string &helperNameOut) {
  const auto *metadata = findStdlibSurfaceMetadataByResolvedPath(resolvedPath);
  if (metadata == nullptr || metadata->id != StdlibSurfaceId::CollectionsMapHelpers) {
    return false;
  }
  const size_t slash = resolvedPath.find_last_of('/');
  if (slash == std::string_view::npos || slash + 1 >= resolvedPath.size()) {
    return false;
  }
  helperNameOut = canonicalInlineMapHelperName(std::string(resolvedPath.substr(slash + 1)));
  return !helperNameOut.empty();
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

bool isExplicitSamePathPublishedMapHelperCall(const Expr &expr,
                                              const std::string &resolvedPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  if (rawPath.empty() || rawPath.front() != '/') {
    return false;
  }
  std::string helperName;
  if (!resolveMapHelperAliasName(expr, helperName) &&
      rawPath.rfind("/map/", 0) != 0 &&
      rawPath.rfind("/std/collections/map/", 0) != 0 &&
      rawPath.rfind("/std/collections/experimental_map/", 0) != 0 &&
      resolvedPath.rfind("/std/collections/map/", 0) != 0 &&
      resolvedPath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return false;
  }
  return normalizeCollectionHelperPath(rawPath) ==
         normalizeCollectionHelperPath(resolvedPath);
}

bool isSemanticBarePublishedMapHelperCall(const Expr &expr,
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

bool isExplicitSamePathMapCountLikeDefinitionCall(const Expr &expr,
                                                  const Definition &callee) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  if (rawPath.empty() || rawPath.front() != '/') {
    return false;
  }
  const std::string normalizedCalleePath =
      normalizeCollectionHelperPath(callee.fullPath);
  if (normalizedCalleePath.rfind("/map/", 0) != 0 &&
      normalizedCalleePath.rfind("/std/collections/map/", 0) != 0 &&
      normalizedCalleePath.rfind("/std/collections/experimental_map/", 0) != 0) {
    return false;
  }
  const size_t slash = callee.fullPath.find_last_of('/');
  if (slash == std::string::npos || slash + 1 >= callee.fullPath.size()) {
    return false;
  }
  const std::string helperName =
      canonicalInlineMapHelperName(callee.fullPath.substr(slash + 1));
  if (helperName != "count" && helperName != "contains" &&
      helperName != "tryAt" && helperName != "count_ref" &&
      helperName != "contains_ref" && helperName != "tryAt_ref") {
    return false;
  }
  const auto normalizeCountLikeHelperPath = [](const std::string &path) {
    std::string normalized = normalizeCollectionHelperPath(path);
    const auto rewriteCanonicalPrefix = [&](std::string_view prefix) {
      if (normalized.rfind(prefix, 0) != 0) {
        return false;
      }
      normalized = "/map/" + normalized.substr(prefix.size());
      return true;
    };
    rewriteCanonicalPrefix("/std/collections/map/");
    rewriteCanonicalPrefix("/std/collections/experimental_map/");
    return normalized;
  };
  return normalizeCountLikeHelperPath(rawPath) ==
         normalizeCountLikeHelperPath(callee.fullPath);
}

bool isDirectMapAccessHelperCall(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  std::string helperName;
  return resolveMapHelperAliasName(expr, helperName) &&
         (helperName == "at" || helperName == "at_unsafe");
}

bool isExplicitDirectMapAccessHelperCall(const Expr &expr) {
  if (!isDirectMapAccessHelperCall(expr)) {
    return false;
  }
  const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
  return !rawPath.empty() && rawPath.front() == '/';
}

bool isSemanticBarePreferredMapHelperDefinitionCall(const Expr &expr,
                                                    const Definition &callee) {
  std::string helperName;
  if (!resolvePublishedInlineMapHelperName(callee.fullPath, helperName)) {
    return false;
  }
  return isSemanticBarePublishedMapHelperCall(
      expr, canonicalInlineMapHelperName(helperName));
}

bool isBareDirectWrapperMapAccessDefinitionCall(const Expr &expr) {
  const bool isBareAccessHelper =
      expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
      expr.namespacePrefix.empty() &&
      (isSimpleCallName(expr, "at") || isSimpleCallName(expr, "at_unsafe"));
  return expr.kind == Expr::Kind::Call && !expr.isMethodCall &&
         isBareAccessHelper &&
         !isExplicitDirectMapAccessHelperCall(expr) &&
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

bool matchesInlineMapHelperFamily(std::string_view requestedHelperName,
                                  std::string_view resolvedHelperName) {
  if (requestedHelperName == resolvedHelperName) {
    return true;
  }
  return resolvedHelperName == std::string(requestedHelperName) + "_ref";
}

bool isInlineMapBuiltinHelperName(std::string_view helperName) {
  return helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
         helperName == "insert" || helperName == "insert_ref";
}

bool keepsBuiltinInlineReturnForPublishedMapHelper(std::string_view helperName,
                                                   const Definition &callee) {
  std::string declaredReturnType;
  if (!inferReceiverTypeFromDeclaredReturn(callee, declaredReturnType)) {
    return true;
  }
  if (helperName == "contains" || helperName == "contains_ref") {
    return declaredReturnType == "bool";
  }
  if (helperName == "tryAt" || helperName == "tryAt_ref") {
    return declaredReturnType == "Result";
  }
  return true;
}

bool prefersPublishedMapHelperDefinition(const Expr &expr,
                                         std::string_view helperName,
                                         const Definition &callee) {
  if (!expr.isMethodCall) {
    if (isSemanticBarePublishedMapHelperCall(expr, helperName)) {
      return true;
    }
    const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
    const bool isExplicitCanonicalMapHelperPath =
        rawPath.rfind("/std/collections/map/", 0) == 0 ||
        rawPath.rfind("std/collections/map/", 0) == 0;
    if (isExplicitCanonicalMapHelperPath &&
        (helperName == "count" || helperName == "count_ref" ||
         helperName == "contains" || helperName == "contains_ref" ||
         helperName == "tryAt" || helperName == "tryAt_ref" ||
         helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref")) {
      return true;
    }
    if (rawPath.rfind("/std/collections/experimental_map/", 0) == 0 &&
        callee.fullPath.rfind("/std/collections/experimental_map/map", 0) == 0) {
      return true;
    }
    if (isExplicitSamePathPublishedMapHelperCall(expr, callee.fullPath)) {
      return true;
    }
    if ((helperName == "at" || helperName == "at_ref" ||
         helperName == "at_unsafe" || helperName == "at_unsafe_ref") &&
        !expr.args.empty() && expr.args.front().kind == Expr::Kind::Call) {
      return true;
    }
    return false;
  }
  if (helperName == "count" || helperName == "count_ref") {
    return true;
  }
  if ((helperName == "contains" || helperName == "contains_ref" ||
       helperName == "tryAt" || helperName == "tryAt_ref" ||
       helperName == "at" || helperName == "at_ref" ||
       helperName == "at_unsafe" || helperName == "at_unsafe_ref") &&
      !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name) {
    return true;
  }
  return false;
}

bool isMapBuiltinInlinePath(const Expr &expr, const Definition &callee) {
  std::string resolvedHelperName;
  const bool hasPublishedResolvedHelper =
      resolvePublishedInlineMapHelperName(callee.fullPath, resolvedHelperName);
  if (!expr.isMethodCall) {
    const std::string scopedExprPath = resolveInlineCallPathWithoutFallbackProbes(expr);
    if (scopedExprPath.rfind("/std/collections/experimental_map/map", 0) == 0 &&
        callee.fullPath.rfind("/std/collections/experimental_map/map", 0) == 0) {
      return false;
    }
    if (prefersPublishedMapHelperDefinition(expr, resolvedHelperName, callee)) {
      return false;
    }
    if (isExplicitMapContainsOrTryAtMethodPath(scopedExprPath) &&
        normalizeCollectionHelperPath(scopedExprPath) ==
            normalizeCollectionHelperPath(callee.fullPath)) {
      return false;
    }
    std::string aliasName;
    std::string normalizedName = scopedExprPath;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      return hasPublishedResolvedHelper &&
             (resolvedHelperName == "at" || resolvedHelperName == "at_ref" ||
              resolvedHelperName == "at_unsafe" ||
              resolvedHelperName == "at_unsafe_ref");
    }
    if (resolveMapHelperAliasName(expr, aliasName)) {
      const size_t expectedArgCount =
          aliasName == "count" ? 1u : (aliasName == "insert" ? 3u : 2u);
      return hasPublishedResolvedHelper &&
             expr.args.size() == expectedArgCount &&
             matchesInlineMapHelperFamily(aliasName, resolvedHelperName);
    }
    if ((normalizedName == "contains" || normalizedName == "map/contains" ||
         normalizedName == "std/collections/map/contains") &&
        expr.args.size() == 2) {
      return hasPublishedResolvedHelper &&
             matchesInlineMapHelperFamily("contains", resolvedHelperName);
    }
    if ((normalizedName == "tryAt" || normalizedName == "map/tryAt" ||
         normalizedName == "std/collections/map/tryAt") &&
        expr.args.size() == 2) {
      return hasPublishedResolvedHelper &&
             matchesInlineMapHelperFamily("tryAt", resolvedHelperName);
    }
    if ((normalizedName == "count" || normalizedName == "map/count" ||
         normalizedName == "std/collections/map/count") &&
        expr.args.size() == 1) {
      return hasPublishedResolvedHelper &&
             matchesInlineMapHelperFamily("count", resolvedHelperName);
    }
    if ((normalizedName == "insert" || normalizedName == "map/insert" ||
         normalizedName == "std/collections/map/insert") &&
        expr.args.size() == 3) {
      return hasPublishedResolvedHelper &&
             matchesInlineMapHelperFamily("insert", resolvedHelperName);
    }
    return false;
  }
  if (hasPublishedResolvedHelper && isInlineMapBuiltinHelperName(resolvedHelperName)) {
    if (prefersPublishedMapHelperDefinition(expr, resolvedHelperName, callee)) {
      return false;
    }
    if (!keepsBuiltinInlineReturnForPublishedMapHelper(resolvedHelperName, callee)) {
      return false;
    }
    return true;
  }
  const size_t slash = callee.fullPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }
  const std::string receiverPath = callee.fullPath.substr(0, slash);
  if (receiverPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
    return false;
  }
  const std::string helperName =
      canonicalInlineMapHelperName(callee.fullPath.substr(slash + 1));
  return isInlineMapBuiltinHelperName(helperName);
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

bool isMapContainsHelperName(const Expr &expr) {
  if (isSimpleCallName(expr, "contains")) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == "contains";
}

bool isMapTryAtHelperName(const Expr &expr) {
  if (isSimpleCallName(expr, "tryAt")) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == "tryAt";
}

bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn);

bool isVectorTarget(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.kind == LocalInfo::Kind::Vector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(expr, collection) && collection == "vector") {
      return !hasNamedArguments(expr.argNames);
    }
    if (isCanonicalCollectionHelperCall(expr, "std/collections/soa_vector", "to_aos", 1)) {
      return isSoaVectorTarget(expr.args.front(), localsIn);
    }
  }
  return false;
}

bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn) {
  if (expr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(expr.name);
    return it != localsIn.end() && it->second.isSoaVector;
  }
  if (expr.kind == Expr::Kind::Call) {
    std::string collection;
    if (getBuiltinCollectionName(expr, collection) && collection == "soa_vector") {
      return true;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "to_soa") && expr.args.size() == 1) {
      return isVectorTarget(expr.args.front(), localsIn);
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
        !expr.args.empty()) {
      std::string preferredCanonicalPath;
      if (normalizedDirectCallPath ==
              "/std/collections/experimental_soa_vector_conversions/soaVectorToAos" &&
          isSoaVectorReceiverExpr(expr.args.front())) {
        preferredCanonicalPath = "/std/collections/soa_vector/to_aos";
      } else if (
          normalizedDirectCallPath ==
              "/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef" &&
          isSoaVectorReceiverExpr(expr.args.front())) {
        preferredCanonicalPath = "/std/collections/soa_vector/to_aos_ref";
      }
      if (!preferredCanonicalPath.empty()) {
        Expr canonicalExpr = expr;
        canonicalExpr.isMethodCall = false;
        canonicalExpr.name = preferredCanonicalPath;
        canonicalExpr.namespacePrefix.clear();
        if (const Definition *preferredCallee =
                resolveDefinitionCall(canonicalExpr)) {
          directCallee = preferredCallee;
        }
      }
    }
    const bool isExplicitDirectExperimentalMapAccessHelperCall =
        directCallee != nullptr &&
        directCallPath.rfind("/std/collections/experimental_map/map", 0) == 0 &&
        directCallee->fullPath.rfind("/std/collections/experimental_map/map", 0) == 0;
    if (directCallee != nullptr && isCollectionAccessReceiverExpr &&
        !expr.args.empty() && isCollectionAccessReceiverExpr(expr.args.front()) &&
        isExplicitDirectMapAccessHelperCall(expr) &&
        !isExplicitDirectExperimentalMapAccessHelperCall) {
      return InlineCallDispatchResult::NotHandled;
    }
    if (directCallee != nullptr &&
        prefersBuiltinCountFallbackOverRemovedShadow(
            expr, *directCallee, isArrayCountCall, isStringCountCall)) {
      return InlineCallDispatchResult::NotHandled;
    }
    if (directCallee != nullptr &&
        (isSemanticBarePreferredMapHelperDefinitionCall(expr, *directCallee) ||
         isBareDirectWrapperMapAccessDefinitionCall(expr) ||
         isExplicitSamePathMapCountLikeDefinitionCall(expr, *directCallee))) {
      if (isCollectionAccessReceiverExpr && !expr.args.empty() &&
          isCollectionAccessReceiverExpr(expr.args.front()) &&
          isMapBuiltinInlinePath(expr, *directCallee)) {
        return InlineCallDispatchResult::NotHandled;
      }
      const auto emitResult = emitResolvedInlineDefinitionCall(
          expr, directCallee, emitInlineDefinitionCall, error);
      return emitResult == ResolvedInlineCallResult::Emitted
                 ? InlineCallDispatchResult::Emitted
                 : InlineCallDispatchResult::Error;
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
    const bool isBuiltinMapContainsName = isMapContainsHelperName(expr) && expr.args.size() == 2;
    const bool isBuiltinMapTryAtName = isMapTryAtHelperName(expr) && expr.args.size() == 2;
    const bool isBuiltinMapInsertName = isSimpleCallName(expr, "insert") && expr.args.size() == 3;
    const bool isBuiltinCountLikeMethod =
        isBuiltinCountName || isBuiltinCapacityName || isArrayCountCall(expr) || isStringCountCall(expr) ||
        isVectorCapacityCall(expr) || isBuiltinAccessMethod || isBuiltinMapContainsName ||
        isBuiltinMapTryAtName || isBuiltinMapInsertName;
    const Definition *callee = resolveMethodCallDefinition(expr);
    if (callee != nullptr) {
      if (isCollectionAccessReceiverExpr && !expr.args.empty() &&
          isCollectionAccessReceiverExpr(expr.args.front()) &&
          isMapBuiltinInlinePath(expr, *callee)) {
        return InlineCallDispatchResult::NotHandled;
      }
      const auto emitResult = emitResolvedInlineDefinitionCall(
          expr, callee, emitInlineDefinitionCall, error);
      return emitResult == ResolvedInlineCallResult::Emitted
                 ? InlineCallDispatchResult::Emitted
                 : InlineCallDispatchResult::Error;
    }
    if (!isBuiltinCountLikeMethod) {
      return InlineCallDispatchResult::Error;
    }
    error.clear();
  }

  if (const Definition *callee =
          directCallee != nullptr ? directCallee : resolveDefinitionCall(expr)) {
    if (isCollectionAccessReceiverExpr && !expr.args.empty() &&
        isCollectionAccessReceiverExpr(expr.args.front()) &&
        isMapBuiltinInlinePath(expr, *callee)) {
      return InlineCallDispatchResult::NotHandled;
    }
    const auto emitResult = emitResolvedInlineDefinitionCall(
        expr, callee, emitInlineDefinitionCall, error);
    return emitResult == ResolvedInlineCallResult::Emitted
               ? InlineCallDispatchResult::Emitted
               : InlineCallDispatchResult::Error;
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
    std::string &error) {
  auto emitCanonicalInlineDefinitionCall = [&](const Expr &callExpr, const Definition &callee) {
    if (isTypeNamespaceMethodCallForInlineEmit(callExpr, callee, localsIn)) {
      const Expr directCallExpr = makeInlineEmitDirectTypeNamespaceCall(callExpr, callee);
      return emitInlineDefinitionCallFn(directCallExpr, callee, localsIn);
    }
    return emitInlineDefinitionCallFn(callExpr, callee, localsIn);
  };
  if (!expr.isMethodCall) {
    std::string mapHelperName;
    const std::string rawPath = resolveInlineCallPathWithoutFallbackProbes(expr);
    std::string experimentalVectorElementType;
    if (getExperimentalVectorConstructorElementTypeAlias(
            expr, experimentalVectorElementType)) {
      Expr rewrittenVectorCtor = expr;
      rewrittenVectorCtor.name = "/std/collections/experimental_vector/vector";
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
    const bool isCanonicalStdMapHelperCall =
        rawPath.rfind("/std/collections/map/", 0) == 0 ||
        rawPath.rfind("std/collections/map/", 0) == 0;
    const auto inferCallMapTargetInfo = [&](const Expr &targetExpr,
                                            MapAccessTargetInfo &targetInfoOut) {
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
        return inferForwardedMapAccessTargetInfo(
            targetExpr, *callee, localsIn, {}, targetInfoOut);
      }
      targetInfoOut.isMapTarget = true;
      targetInfoOut.mapKeyKind = valueKindFromTypeName(collectionArgs.front());
      targetInfoOut.mapValueKind = valueKindFromTypeName(collectionArgs.back());
      return targetInfoOut.mapKeyKind != LocalInfo::ValueKind::Unknown &&
             targetInfoOut.mapValueKind != LocalInfo::ValueKind::Unknown;
    };
    if (isCanonicalStdMapHelperCall && !expr.args.empty()) {
      const auto targetInfo =
          resolveMapAccessTargetInfo(expr.args.front(), localsIn, inferCallMapTargetInfo);
      std::string directHelperName = rawPath;
      const size_t lastSlash = directHelperName.find_last_of('/');
      if (lastSlash != std::string::npos) {
        directHelperName = directHelperName.substr(lastSlash + 1);
      }
      directHelperName = canonicalInlineMapHelperName(std::move(directHelperName));
      if (targetInfo.isMapTarget &&
          (directHelperName == "count" || directHelperName == "contains" ||
           directHelperName == "tryAt" || directHelperName == "at" ||
           directHelperName == "at_unsafe")) {
        return InlineCallDispatchResult::NotHandled;
      }
    }
    if (!expr.args.empty() &&
        (resolveMapHelperAliasName(expr, mapHelperName) ||
         (getBuiltinArrayAccessName(expr, mapHelperName) && expr.args.size() == 2)) &&
        resolveMapAccessTargetInfo(expr.args.front(), localsIn, inferCallMapTargetInfo).isMapTarget &&
        !isCanonicalStdMapHelperCall &&
        resolveDefinitionCallFn(expr) == nullptr) {
      return InlineCallDispatchResult::NotHandled;
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      const auto targetInfo = resolveArrayVectorAccessTargetInfo(expr.args.front(), localsIn);
      if (targetInfo.isArgsPackTarget) {
        return InlineCallDispatchResult::NotHandled;
      }
    }
  }
  auto tryEmitVectorMutatorCallFormExpr = [&]() {
    const bool isVectorMutatorCall =
        isVectorBuiltinName(expr, "push") || isVectorBuiltinName(expr, "pop") ||
        isVectorBuiltinName(expr, "reserve") || isVectorBuiltinName(expr, "clear") ||
        isVectorBuiltinName(expr, "remove_at") || isVectorBuiltinName(expr, "remove_swap");
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
      const std::string priorError = error;
      const Definition *callee = resolveMethodCallDefinitionFn(methodExpr, localsIn);
      if (callee == nullptr) {
        error = priorError;
        continue;
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
      (isSimpleCallName(expr, "count") || isSimpleCallName(expr, "capacity")) &&
      isVectorTarget(expr.args.front(), localsIn)) {
    const Definition *callee = resolveMethodCallDefinitionFn(expr, localsIn);
    if (callee != nullptr && callee->fullPath == "/vector/count") {
      return InlineCallDispatchResult::NotHandled;
    }
    if (callee != nullptr && callee->fullPath == "/vector/capacity") {
      error =
          "native backend only supports arithmetic/comparison/clamp/min/max/abs/sign/saturate/convert/pointer/assign/increment/decrement calls in expressions (call=" +
          expr.name + ", name=" + expr.name +
          ", args=" + std::to_string(expr.args.size()) +
          ", method=true)";
      return InlineCallDispatchResult::Error;
    }
  }
  const auto inlineResult = tryEmitInlineCallWithCountFallbacksImpl(
      expr,
      [&](const Expr &callExpr) { return isArrayCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isStringCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isVectorCapacityCallFn(callExpr, localsIn); },
      [&](const Expr &receiverExpr) { return isSoaVectorTarget(receiverExpr, localsIn); },
      [&](const Expr &receiverExpr) {
        if (receiverExpr.kind == Expr::Kind::StringLiteral) {
          return true;
        }
        if (receiverExpr.kind == Expr::Kind::Name) {
          auto it = localsIn.find(receiverExpr.name);
          if (it == localsIn.end()) {
            return false;
          }
          const LocalInfo &info = it->second;
          if (info.isArgsPack) {
            return false;
          }
          if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
              info.kind == LocalInfo::Kind::Map) {
            return true;
          }
          if (info.kind == LocalInfo::Kind::Reference &&
              (info.referenceToArray || info.referenceToVector || info.referenceToMap)) {
            return true;
          }
          if (info.kind == LocalInfo::Kind::Pointer &&
              (info.pointerToArray || info.pointerToVector || info.pointerToMap ||
               info.pointerToBuffer)) {
            return true;
          }
          return info.valueKind == LocalInfo::ValueKind::String;
        }
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding) {
          return false;
        }
        std::string mapHelperName;
        if (resolveMapHelperAliasName(receiverExpr, mapHelperName) &&
            (mapHelperName == "at" || mapHelperName == "at_unsafe" ||
             mapHelperName == "tryAt" || mapHelperName == "contains") &&
            receiverExpr.args.size() == 2) {
          return false;
        }
        const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(receiverExpr, localsIn);
        if (resolveMapAccessTargetInfo(receiverExpr, localsIn).isMapTarget) {
          return true;
        }
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
      [&](const Expr &callExpr) { return resolveMethodCallDefinitionFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return resolveDefinitionCallFn(callExpr); },
      [&](const Expr &callExpr, const Definition &callee) {
        return emitCanonicalInlineDefinitionCall(callExpr, callee);
      },
      error);
  return inlineResult;
}

} // namespace primec::ir_lowerer
