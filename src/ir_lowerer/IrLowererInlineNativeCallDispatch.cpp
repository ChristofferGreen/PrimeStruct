#include "IrLowererCallHelpers.h"

#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "primec/AstCallPathHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isMapBuiltinInlinePath(const Expr &expr, const Definition &callee) {
  auto matchesHelper = [&](std::string_view basePath) {
    return callee.fullPath == basePath ||
           callee.fullPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  if (!expr.isMethodCall) {
    std::string aliasName;
    std::string normalizedName = expr.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      return matchesHelper("/std/collections/map/at") ||
             matchesHelper("/std/collections/mapAt") ||
             matchesHelper("/std/collections/map/at_unsafe") ||
             matchesHelper("/std/collections/mapAtUnsafe");
    }
    if (resolveMapHelperAliasName(expr, aliasName)) {
      if (aliasName == "count" && expr.args.size() == 1) {
        return matchesHelper("/std/collections/map/count") ||
               matchesHelper("/std/collections/map/count_ref") ||
               matchesHelper("/std/collections/mapCount") ||
               matchesHelper("/std/collections/experimental_map/mapCount") ||
               matchesHelper("/std/collections/experimental_map/mapCountRef");
      }
      if (aliasName == "contains" && expr.args.size() == 2) {
        return matchesHelper("/std/collections/map/contains") ||
               matchesHelper("/std/collections/map/contains_ref") ||
               matchesHelper("/std/collections/mapContains") ||
               matchesHelper("/std/collections/experimental_map/mapContains") ||
               matchesHelper("/std/collections/experimental_map/mapContainsRef");
      }
      if (aliasName == "tryAt" && expr.args.size() == 2) {
        return matchesHelper("/std/collections/map/tryAt") ||
               matchesHelper("/std/collections/map/tryAt_ref") ||
               matchesHelper("/std/collections/mapTryAt") ||
               matchesHelper("/std/collections/experimental_map/mapTryAt") ||
               matchesHelper("/std/collections/experimental_map/mapTryAtRef");
      }
      if (aliasName == "at" && expr.args.size() == 2) {
        return matchesHelper("/std/collections/map/at") ||
               matchesHelper("/std/collections/map/at_ref") ||
               matchesHelper("/std/collections/mapAt") ||
               matchesHelper("/std/collections/experimental_map/mapAt") ||
               matchesHelper("/std/collections/experimental_map/mapAtRef");
      }
      if (aliasName == "at_unsafe" && expr.args.size() == 2) {
        return matchesHelper("/std/collections/map/at_unsafe") ||
               matchesHelper("/std/collections/map/at_unsafe_ref") ||
               matchesHelper("/std/collections/mapAtUnsafe") ||
               matchesHelper("/std/collections/experimental_map/mapAtUnsafe") ||
               matchesHelper("/std/collections/experimental_map/mapAtUnsafeRef");
      }
      if (aliasName == "insert" && expr.args.size() == 3) {
        return matchesHelper("/std/collections/map/insert") ||
               matchesHelper("/std/collections/map/insert_ref") ||
               matchesHelper("/std/collections/mapInsert") ||
               matchesHelper("/std/collections/experimental_map/mapInsert") ||
               matchesHelper("/std/collections/experimental_map/mapInsertRef");
      }
    }
    if ((normalizedName == "contains" || normalizedName == "map/contains" ||
         normalizedName == "std/collections/map/contains") &&
        expr.args.size() == 2) {
      return matchesHelper("/std/collections/mapContains");
    }
    if ((normalizedName == "tryAt" || normalizedName == "map/tryAt" ||
         normalizedName == "std/collections/map/tryAt") &&
        expr.args.size() == 2) {
      return matchesHelper("/std/collections/mapTryAt");
    }
    if ((normalizedName == "count" || normalizedName == "map/count" ||
         normalizedName == "std/collections/map/count") &&
        expr.args.size() == 1) {
      return matchesHelper("/std/collections/map/count") ||
             matchesHelper("/std/collections/mapCount");
    }
    return false;
  }
  const size_t slash = callee.fullPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }
  const std::string receiverPath = callee.fullPath.substr(0, slash);
  if (receiverPath.rfind("/std/collections/experimental_map/Map__", 0) != 0) {
    return false;
  }
  const std::string helperName = callee.fullPath.substr(slash + 1);
  return helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
         helperName == "at" || helperName == "at_unsafe" || helperName == "insert";
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
      return true;
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

InlineCallDispatchResult tryEmitInlineCallWithCountFallbacks(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<bool(const Expr &)> &isVectorCapacityCall,
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
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
  return tryEmitInlineCallWithCountFallbacks(
      expr,
      isArrayCountCall,
      isStringCountCall,
      isVectorCapacityCall,
      {},
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
    const std::function<bool(const Expr &)> &isCollectionAccessReceiverExpr,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
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
    const bool isBuiltinCountLikeMethod =
        isBuiltinCountName || isBuiltinCapacityName || isArrayCountCall(expr) || isStringCountCall(expr) ||
        isVectorCapacityCall(expr) || isBuiltinAccessMethod || isBuiltinMapContainsName ||
        isBuiltinMapTryAtName;
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

  if (const Definition *callee = resolveDefinitionCall(expr)) {
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
    if (!expr.args.empty() &&
        (resolveMapHelperAliasName(expr, mapHelperName) ||
         (getBuiltinArrayAccessName(expr, mapHelperName) && expr.args.size() == 2)) &&
        resolveMapAccessTargetInfo(expr.args.front(), localsIn).isMapTarget) {
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
  return tryEmitInlineCallWithCountFallbacks(
      expr,
      [&](const Expr &callExpr) { return isArrayCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isStringCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isVectorCapacityCallFn(callExpr, localsIn); },
      [&](const Expr &receiverExpr) {
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
}

} // namespace primec::ir_lowerer
