#include "IrLowererCallHelpers.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>
#include <utility>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererIndexKindHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isRemovedVectorCompatibilityHelper(const std::string &helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" || helperName == "at_unsafe" ||
         helperName == "push" || helperName == "pop" || helperName == "reserve" || helperName == "clear" ||
         helperName == "remove_at" || helperName == "remove_swap";
}

bool resolveVectorHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  constexpr std::string_view vectorPrefix = "vector/";
  constexpr std::string_view arrayPrefix = "array/";
  constexpr std::string_view stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(vectorPrefix.size());
    if (helperNameOut == "count" || helperNameOut == "capacity") {
      return true;
    }
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(arrayPrefix, 0) == 0) {
    helperNameOut = normalized.substr(arrayPrefix.size());
    if (isRemovedVectorCompatibilityHelper(helperNameOut)) {
      return false;
    }
    return true;
  }
  if (normalized.rfind(stdVectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdVectorPrefix.size());
    return true;
  }
  return false;
}

bool resolveMapHelperAliasName(const Expr &expr, std::string &helperNameOut) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized[0] == '/') {
    normalized.erase(0, 1);
  }
  constexpr std::string_view mapPrefix = "map/";
  constexpr std::string_view stdMapPrefix = "std/collections/map/";
  if (normalized.rfind(mapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(mapPrefix.size());
    return true;
  }
  if (normalized.rfind(stdMapPrefix, 0) == 0) {
    helperNameOut = normalized.substr(stdMapPrefix.size());
    return true;
  }
  return false;
}

bool isArgsPackParam(const Expr &param) {
  for (const auto &transform : param.transforms) {
    if (transform.name == "effects" || transform.name == "capabilities") {
      continue;
    }
    if (isBindingQualifierName(transform.name)) {
      continue;
    }
    if (!transform.arguments.empty()) {
      continue;
    }
    return transform.name == "args";
  }
  return false;
}

std::string normalizeMapImportAliasPath(const std::string &path);

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveVectorHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isMapBuiltinName(const Expr &expr, const char *name) {
  if (isSimpleCallName(expr, name)) {
    return true;
  }
  std::string aliasName;
  return resolveMapHelperAliasName(expr, aliasName) && aliasName == name;
}

bool isExplicitMapHelperFallbackPath(const Expr &expr) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || expr.isMethodCall) {
    return false;
  }
  const std::string normalizedPath = normalizeMapImportAliasPath(expr.name);
  return normalizedPath == "/map/count" || normalizedPath == "/map/at" || normalizedPath == "/map/at_unsafe" ||
         normalizedPath == "/std/collections/map/count" || normalizedPath == "/std/collections/map/at" ||
         normalizedPath == "/std/collections/map/at_unsafe";
}

std::string normalizeMapImportAliasPath(const std::string &path) {
  if (path.empty() || path.front() == '/') {
    return path;
  }
  constexpr std::string_view mapPrefix = "map/";
  constexpr std::string_view stdMapPrefix = "std/collections/map/";
  if (path.rfind(mapPrefix, 0) == 0 || path.rfind(stdMapPrefix, 0) == 0) {
    return "/" + path;
  }
  return path;
}

} // namespace

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

bool emitMapLookupContains(
    LocalInfo::ValueKind mapKeyKind,
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

const Definition *resolveDefinitionCall(const Expr &callExpr,
                                        const std::unordered_map<std::string, const Definition *> &defMap,
                                        const ResolveExprPathFn &resolveExprPath) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall) {
    return nullptr;
  }
  const std::string resolved = resolveExprPath(callExpr);
  return resolveDefinitionByPath(defMap, resolved);
}

ResolveDefinitionCallFn makeResolveDefinitionCall(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath) {
  return [defMap, resolveExprPath](const Expr &expr) {
    return resolveDefinitionCall(expr, defMap, resolveExprPath);
  };
}

CallResolutionAdapters makeCallResolutionAdapters(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  CallResolutionAdapters adapters;
  adapters.resolveExprPath = makeResolveCallPathFromScope(defMap, importAliases);
  adapters.isTailCallCandidate = makeIsTailCallCandidate(defMap, adapters.resolveExprPath);
  adapters.definitionExists = makeDefinitionExistsByPath(defMap);
  return adapters;
}

EntryCallResolutionSetup buildEntryCallResolutionSetup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  EntryCallResolutionSetup setup;
  setup.adapters = makeCallResolutionAdapters(defMap, importAliases);
  setup.hasTailExecution = hasTailExecutionCandidate(
      entryDef.statements, definitionReturnsVoid, setup.adapters.isTailCallCandidate);
  return setup;
}

ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return [defMap, importAliases](const Expr &expr) {
    return resolveCallPathFromScope(expr, defMap, importAliases);
  };
}

IsTailCallCandidateFn makeIsTailCallCandidate(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath) {
  return [defMap, resolveExprPath](const Expr &expr) {
    return isTailCallCandidate(expr, defMap, resolveExprPath);
  };
}

DefinitionExistsFn makeDefinitionExistsByPath(
    const std::unordered_map<std::string, const Definition *> &defMap) {
  return [defMap](const std::string &path) {
    return resolveDefinitionByPath(defMap, path) != nullptr;
  };
}

std::string resolveCallPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix + "/" + expr.name;
    if (defMap.count(scoped) > 0) {
      return scoped;
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return normalizeMapImportAliasPath(importIt->second);
    }
    return scoped;
  }
  auto importIt = importAliases.find(expr.name);
  if (importIt != importAliases.end()) {
    return normalizeMapImportAliasPath(importIt->second);
  }
  return "/" + expr.name;
}

bool isTailCallCandidate(const Expr &expr,
                         const std::unordered_map<std::string, const Definition *> &defMap,
                         const ResolveExprPathFn &resolveExprPath) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall) {
    return false;
  }
  const std::string targetPath = resolveExprPath(expr);
  return resolveDefinitionByPath(defMap, targetPath) != nullptr;
}

bool hasTailExecutionCandidate(const std::vector<Expr> &statements,
                               bool definitionReturnsVoid,
                               const IsTailCallCandidateFn &isTailCallCandidateFn) {
  if (statements.empty()) {
    return false;
  }
  const Expr &lastStmt = statements.back();
  if (isReturnCall(lastStmt) && lastStmt.args.size() == 1) {
    return isTailCallCandidateFn(lastStmt.args.front());
  }
  return definitionReturnsVoid && isTailCallCandidateFn(lastStmt);
}

bool isVectorTarget(const Expr &expr, const LocalMap &localsIn);
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
    if (!expr.isMethodCall && isSimpleCallName(expr, "to_aos") && expr.args.size() == 1) {
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
    const bool isBuiltinCountLikeMethod =
        isBuiltinCountName || isBuiltinCapacityName || isArrayCountCall(expr) || isStringCountCall(expr) ||
        isVectorCapacityCall(expr) || isBuiltinAccessMethod;
    const Definition *callee = resolveMethodCallDefinition(expr);
    if (callee != nullptr) {
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
  if (!expr.isMethodCall && expr.args.size() == 1 &&
      isSoaVectorTarget(expr.args.front(), localsIn)) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    const std::string priorError = error;
    if (const Definition *callee = resolveMethodCallDefinitionFn(methodExpr, localsIn)) {
      if (methodExpr.hasBodyArguments || !methodExpr.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return InlineCallDispatchResult::Error;
      }
      if (!emitCanonicalInlineDefinitionCall(methodExpr, *callee)) {
        return InlineCallDispatchResult::Error;
      }
      return InlineCallDispatchResult::Emitted;
    }
    error = priorError;
  }
  if (expr.isMethodCall &&
      (isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref")) &&
      expr.args.size() == 2 &&
      isSoaVectorTarget(expr.args.front(), localsIn)) {
    if (const Definition *callee = resolveMethodCallDefinitionFn(expr, localsIn)) {
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return InlineCallDispatchResult::Error;
      }
      if (!emitCanonicalInlineDefinitionCall(expr, *callee)) {
        return InlineCallDispatchResult::Error;
      }
      return InlineCallDispatchResult::Emitted;
    }
    return InlineCallDispatchResult::NotHandled;
  }
  if (expr.isMethodCall && isSimpleCallName(expr, "count") && expr.args.size() == 1 &&
      isSoaVectorTarget(expr.args.front(), localsIn)) {
    if (const Definition *callee = resolveMethodCallDefinitionFn(expr, localsIn)) {
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error = "native backend does not support block arguments on calls";
        return InlineCallDispatchResult::Error;
      }
      if (!emitCanonicalInlineDefinitionCall(expr, *callee)) {
        return InlineCallDispatchResult::Error;
      }
      return InlineCallDispatchResult::Emitted;
    }
    return InlineCallDispatchResult::NotHandled;
  }
  return tryEmitInlineCallWithCountFallbacks(
      expr,
      [&](const Expr &callExpr) { return isArrayCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isStringCountCallFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return isVectorCapacityCallFn(callExpr, localsIn); },
      [&](const Expr &receiverExpr) {
        if (receiverExpr.kind != Expr::Kind::Name) {
          return false;
        }
        auto it = localsIn.find(receiverExpr.name);
        if (it == localsIn.end()) {
          return false;
        }
        const LocalInfo &info = it->second;
        if (info.kind == LocalInfo::Kind::Array || info.kind == LocalInfo::Kind::Vector ||
            info.kind == LocalInfo::Kind::Map) {
          return true;
        }
        if (info.kind == LocalInfo::Kind::Reference &&
            (info.referenceToArray || info.referenceToVector || info.referenceToMap)) {
          return true;
        }
        return info.valueKind == LocalInfo::ValueKind::String;
      },
      [&](const Expr &callExpr) { return resolveMethodCallDefinitionFn(callExpr, localsIn); },
      [&](const Expr &callExpr) { return resolveDefinitionCallFn(callExpr); },
      [&](const Expr &callExpr, const Definition &callee) {
        return emitCanonicalInlineDefinitionCall(callExpr, callee);
      },
      error);
}

bool getUnsupportedVectorHelperName(const Expr &expr, std::string &helperName) {
  if (isVectorBuiltinName(expr, "push")) {
    helperName = "push";
    return true;
  }
  if (isVectorBuiltinName(expr, "pop")) {
    helperName = "pop";
    return true;
  }
  if (isVectorBuiltinName(expr, "reserve")) {
    helperName = "reserve";
    return true;
  }
  if (isVectorBuiltinName(expr, "clear")) {
    helperName = "clear";
    return true;
  }
  if (isVectorBuiltinName(expr, "remove_at")) {
    helperName = "remove_at";
    return true;
  }
  if (isVectorBuiltinName(expr, "remove_swap")) {
    helperName = "remove_swap";
    return true;
  }
  return false;
}

UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(
    const Expr &expr,
    const std::function<bool(const Expr &, std::string &)> &tryGetPrintBuiltinName,
    std::string &error) {
  if (isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) {
    error = "count requires array, vector, map, or string target";
    return UnsupportedNativeCallResult::Error;
  }
  if (isVectorBuiltinName(expr, "capacity")) {
    error = "capacity requires vector target";
    return UnsupportedNativeCallResult::Error;
  }

  std::string vectorHelper;
  if (getUnsupportedVectorHelperName(expr, vectorHelper)) {
    error = "native backend does not support vector helper: " + vectorHelper;
    return UnsupportedNativeCallResult::Error;
  }

  std::string printBuiltinName;
  if (tryGetPrintBuiltinName(expr, printBuiltinName)) {
    error = printBuiltinName + " is only supported as a statement in the native backend";
    return UnsupportedNativeCallResult::Error;
  }

  return UnsupportedNativeCallResult::NotHandled;
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
    std::string &error) {
  std::string mathName;
  if (tryGetMathBuiltinName(expr, mathName) && !isSupportedMathBuiltinName(mathName)) {
    error = "native backend does not support math builtin: " + mathName;
    return NativeCallTailDispatchResult::Error;
  }
  if ((isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref")) &&
      expr.args.size() == 2 &&
      isSoaVectorTarget(expr.args.front(), localsIn)) {
    error = std::string("native backend does not support soa_vector ") + expr.name;
    return NativeCallTailDispatchResult::Error;
  }
  if (isSimpleCallName(expr, "to_soa") && expr.args.size() == 1 &&
      isVectorTarget(expr.args.front(), localsIn)) {
    error = "native backend does not support to_soa";
    return NativeCallTailDispatchResult::Error;
  }
  if (isSimpleCallName(expr, "to_aos") && expr.args.size() == 1 &&
      isSoaVectorTarget(expr.args.front(), localsIn)) {
    error = "native backend does not support to_aos";
    return NativeCallTailDispatchResult::Error;
  }
  if (isExplicitMapHelperFallbackPath(expr)) {
    return NativeCallTailDispatchResult::NotHandled;
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
      inferExprKind,
      resolveStringTableTarget,
      emitExpr,
      emitInstruction,
      error);
  if (countAccessResult == CountAccessCallEmitResult::Emitted) {
    return NativeCallTailDispatchResult::Emitted;
  }
  if (countAccessResult == CountAccessCallEmitResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  if (!expr.isMethodCall && isSimpleCallName(expr, "contains")) {
    if (expr.args.size() != 2) {
      error = "contains requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
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

  const auto unsupportedCallResult = emitUnsupportedNativeCallDiagnostic(
      expr, tryGetPrintBuiltinName, error);
  if (unsupportedCallResult == UnsupportedNativeCallResult::Error) {
    return NativeCallTailDispatchResult::Error;
  }

  std::string accessName;
  if (getBuiltinArrayAccessName(expr, accessName)) {
    if (expr.args.size() != 2) {
      error = accessName + " requires exactly two arguments";
      return NativeCallTailDispatchResult::Error;
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
                                error)) {
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
    std::string &error) {
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
      error);
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
    std::string &error) {
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
      error);
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
    std::string &error) {
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
      error);
}

BufferBuiltinDispatchResult tryEmitBufferBuiltinDispatchWithLocals(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t(int32_t)> &allocLocalRange,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error) {
  const auto result = tryEmitBufferBuiltinCall(
      expr,
      localsIn,
      valueKindFromTypeName,
      inferExprKind,
      allocLocalRange,
      allocTempLocal,
      emitExpr,
      emitInstruction,
      error);
  if (result == BufferBuiltinCallEmitResult::Emitted) {
    return BufferBuiltinDispatchResult::Emitted;
  }
  if (result == BufferBuiltinCallEmitResult::Error) {
    return BufferBuiltinDispatchResult::Error;
  }
  return BufferBuiltinDispatchResult::NotHandled;
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
    std::string &error) {
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
      error);
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
    std::string &error) {
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
      error);
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
    std::string &error) {
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
      error);
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
    std::string &error) {
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
      error);
}

MapAccessTargetInfo resolveMapAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo) {
  MapAccessTargetInfo info;
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Map ||
         (it->second.kind == LocalInfo::Kind::Reference && it->second.referenceToMap))) {
      info.isMapTarget = true;
      info.mapKeyKind = it->second.mapKeyKind;
      info.mapValueKind = it->second.mapValueKind;
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const Expr &accessReceiver = target.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && localIt->second.isArgsPack &&
            (localIt->second.argsPackElementKind == LocalInfo::Kind::Map ||
             (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
              localIt->second.referenceToMap))) {
          info.isMapTarget = true;
          info.mapKeyKind = localIt->second.mapKeyKind;
          info.mapValueKind = localIt->second.mapValueKind;
          return info;
        }
      }
    }
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && collection == "map" &&
        target.templateArgs.size() == 2) {
      info.isMapTarget = true;
      info.mapKeyKind = valueKindFromTypeName(target.templateArgs[0]);
      info.mapValueKind = valueKindFromTypeName(target.templateArgs[1]);
      return info;
    }
    if (resolveCallMapAccessTargetInfo) {
      MapAccessTargetInfo inferred;
      if (resolveCallMapAccessTargetInfo(target, inferred)) {
        return inferred;
      }
    }
  }
  return info;
}

MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target, const LocalMap &localsIn) {
  return resolveMapAccessTargetInfo(target, localsIn, {});
}

bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error) {
  if (!targetInfo.isMapTarget) {
    return true;
  }
  if (targetInfo.mapKeyKind == LocalInfo::ValueKind::Unknown ||
      targetInfo.mapValueKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires typed map bindings for " + accessName;
    return false;
  }
  return true;
}

MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto mapTargetInfo = resolveMapAccessTargetInfo(
      targetExpr, localsIn, resolveCallMapAccessTargetInfo);
  if (!mapTargetInfo.isMapTarget) {
    return MapAccessLookupEmitResult::NotHandled;
  }
  if (!validateMapAccessTargetInfo(mapTargetInfo, accessName, error)) {
    return MapAccessLookupEmitResult::Error;
  }
  if (!emitMapLookupAccess(
          accessName,
          mapTargetInfo.mapKeyKind,
          targetExpr,
          lookupKeyExpr,
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
    return MapAccessLookupEmitResult::Error;
  }
  return MapAccessLookupEmitResult::Emitted;
}

MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return tryEmitMapAccessLookup(
      accessName,
      targetExpr,
      lookupKeyExpr,
      localsIn,
      allocTempLocal,
      emitExpr,
      resolveStringTableTarget,
      {},
      inferExprKind,
      emitMapKeyNotFound,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

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
    std::string &error) {
  const auto mapTargetInfo = resolveMapAccessTargetInfo(
      targetExpr, localsIn, resolveCallMapAccessTargetInfo);
  if (!mapTargetInfo.isMapTarget) {
    return MapAccessLookupEmitResult::NotHandled;
  }
  if (!validateMapAccessTargetInfo(mapTargetInfo, "contains", error)) {
    return MapAccessLookupEmitResult::Error;
  }
  if (!emitMapLookupContains(
          mapTargetInfo.mapKeyKind,
          targetExpr,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          resolveStringTableTarget,
          inferExprKind,
          instructionCount,
          emitInstruction,
          patchInstructionImm,
          error)) {
    return MapAccessLookupEmitResult::Error;
  }
  return MapAccessLookupEmitResult::Emitted;
}

StringTableAccessEmitResult tryEmitStringTableAccessLoad(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  int32_t stringIndex = -1;
  size_t stringLength = 0;
  if (!resolveStringTableTarget(targetExpr, localsIn, stringIndex, stringLength)) {
    return StringTableAccessEmitResult::NotHandled;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(indexExpr, localsIn, accessName, inferExprKind, indexKind, error)) {
    return StringTableAccessEmitResult::Error;
  }
  if (stringLength > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
    error = "native backend string too large for indexing";
    return StringTableAccessEmitResult::Error;
  }

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(indexExpr, localsIn)) {
    return StringTableAccessEmitResult::Error;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  emitStringAccessLoad(
      accessName,
      indexLocal,
      indexKind,
      stringLength,
      stringIndex,
      emitStringIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return StringTableAccessEmitResult::Emitted;
}

enum class DynamicStringAccessEmitResult {
  NotHandled,
  Emitted,
  Error,
};

DynamicStringAccessEmitResult tryEmitDynamicStringAccessLoad(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    size_t stringTableCount,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  bool isRuntimeStringTarget = false;
  if (targetExpr.kind == Expr::Kind::Name) {
    auto it = localsIn.find(targetExpr.name);
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Value &&
        it->second.valueKind == LocalInfo::ValueKind::String) {
      if (it->second.stringSource == LocalInfo::StringSource::ArgvIndex) {
        error = "native backend only supports entry argument indexing in print calls or string bindings";
        return DynamicStringAccessEmitResult::Error;
      }
      isRuntimeStringTarget = (it->second.stringSource == LocalInfo::StringSource::RuntimeIndex);
    }
  } else {
    if (isEntryArgsName(targetExpr, localsIn)) {
      error = "native backend only supports entry argument indexing in print calls or string bindings";
      return DynamicStringAccessEmitResult::Error;
    }
    if (targetExpr.kind == Expr::Kind::Call) {
      std::string nestedAccessName;
      if (getBuiltinArrayAccessName(targetExpr, nestedAccessName) && targetExpr.args.size() == 2 &&
          isEntryArgsName(targetExpr.args.front(), localsIn)) {
        error = "native backend only supports entry argument indexing in print calls or string bindings";
        return DynamicStringAccessEmitResult::Error;
      }
    }
    isRuntimeStringTarget = inferExprKind(targetExpr, localsIn) == LocalInfo::ValueKind::String;
  }

  if (!isRuntimeStringTarget || stringTableCount == 0) {
    return DynamicStringAccessEmitResult::NotHandled;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(indexExpr, localsIn, accessName, inferExprKind, indexKind, error)) {
    return DynamicStringAccessEmitResult::Error;
  }

  const int32_t stringLocal = allocTempLocal();
  if (!emitExpr(targetExpr, localsIn)) {
    return DynamicStringAccessEmitResult::Error;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(stringLocal));

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(indexExpr, localsIn)) {
    return DynamicStringAccessEmitResult::Error;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  std::vector<size_t> jumpToEnd;
  jumpToEnd.reserve(stringTableCount);
  for (size_t stringIndex = 0; stringIndex < stringTableCount; ++stringIndex) {
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(stringLocal));
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(stringIndex));
    emitInstruction(IrOpcode::CmpEqI64, 0);
    const size_t jumpNext = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emitInstruction(IrOpcode::LoadStringByte, static_cast<uint64_t>(stringIndex));
    const size_t jumpEnd = instructionCount();
    emitInstruction(IrOpcode::Jump, 0);
    patchInstructionImm(jumpNext, static_cast<uint64_t>(instructionCount()));
    jumpToEnd.push_back(jumpEnd);
  }

  emitStringIndexOutOfBounds();
  const size_t endIndex = instructionCount();
  for (size_t jumpIndex : jumpToEnd) {
    patchInstructionImm(jumpIndex, static_cast<uint64_t>(endIndex));
  }
  return DynamicStringAccessEmitResult::Emitted;
}

NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error) {
  if (targetExpr.kind == Expr::Kind::StringLiteral) {
    return NonLiteralStringAccessTargetResult::Stop;
  }
  if (targetExpr.kind == Expr::Kind::Name) {
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

bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error) {
  indexKindOut = normalizeIndexKind(inferExprKind(indexExpr, localsIn));
  if (!isSupportedIndexKind(indexKindOut)) {
    error = "native backend requires integer indices for " + accessName;
    return false;
  }
  return true;
}

ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo) {
  ArrayVectorAccessTargetInfo info;
  if (target.kind == Expr::Kind::Name) {
    auto it = localsIn.find(target.name);
    if (it != localsIn.end() &&
        (it->second.kind == LocalInfo::Kind::Array || it->second.kind == LocalInfo::Kind::Vector)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = (it->second.kind == LocalInfo::Kind::Vector);
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = it->second.structSlotCount;
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Reference &&
        (it->second.referenceToArray || it->second.referenceToVector)) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = it->second.referenceToVector;
      info.isSoaVector = it->second.isSoaVector;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = it->second.structSlotCount;
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    if (it != localsIn.end() && it->second.kind == LocalInfo::Kind::Pointer && it->second.pointerToVector) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = it->second.valueKind;
      info.isVectorTarget = true;
      info.isArgsPackTarget = it->second.isArgsPack;
      info.argsPackElementKind = it->second.argsPackElementKind;
      info.elemSlotCount = it->second.structSlotCount;
      info.structTypeName = it->second.structTypeName;
      return info;
    }
    return info;
  }
  if (target.kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
      const Expr &accessReceiver = target.args.front();
      if (accessReceiver.kind == Expr::Kind::Name) {
        auto localIt = localsIn.find(accessReceiver.name);
        if (localIt != localsIn.end() && localIt->second.isArgsPack) {
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Vector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            info.isSoaVector = localIt->second.isSoaVector;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Array ||
              (localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
               (localIt->second.referenceToArray || localIt->second.referenceToVector))) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget =
                localIt->second.argsPackElementKind == LocalInfo::Kind::Reference &&
                localIt->second.referenceToVector;
            info.isSoaVector = localIt->second.isSoaVector;
            return info;
          }
          if (localIt->second.argsPackElementKind == LocalInfo::Kind::Pointer && localIt->second.pointerToVector) {
            info.isArrayOrVectorTarget = true;
            info.elemKind = localIt->second.valueKind;
            info.isVectorTarget = true;
            return info;
          }
        }
      }
    }
    std::string collection;
    if (getBuiltinCollectionName(target, collection) && (collection == "array" || collection == "vector") &&
        target.templateArgs.size() == 1) {
      info.isArrayOrVectorTarget = true;
      info.elemKind = valueKindFromTypeName(target.templateArgs.front());
      info.isVectorTarget = (collection == "vector");
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
    const Expr &target, const LocalMap &localsIn) {
  return resolveArrayVectorAccessTargetInfo(target, localsIn, {});
}

bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo, std::string &error) {
  const bool isStructArgsPackTarget =
      targetInfo.isArgsPackTarget && !targetInfo.isVectorTarget && !targetInfo.structTypeName.empty() &&
      targetInfo.elemSlotCount > 0;
  const bool isWrappedStructArgsPackTarget =
      targetInfo.isArgsPackTarget && !targetInfo.isVectorTarget && !targetInfo.structTypeName.empty() &&
      (targetInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
       targetInfo.argsPackElementKind == LocalInfo::Kind::Reference);
  const bool isVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Vector;
  const bool isPointerVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Pointer &&
      targetInfo.isVectorTarget;
  const bool isBorrowedSoaVectorArgsPackTarget =
      targetInfo.isArgsPackTarget && targetInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
      targetInfo.isVectorTarget && targetInfo.isSoaVector;
  if (!targetInfo.isArrayOrVectorTarget ||
      (targetInfo.elemKind == LocalInfo::ValueKind::Unknown && !isStructArgsPackTarget &&
       !isWrappedStructArgsPackTarget && !isVectorArgsPackTarget && !isPointerVectorArgsPackTarget &&
       !isBorrowedSoaVectorArgsPackTarget)) {
    error =
        "native backend only supports at() on numeric/bool/string arrays or vectors, plus args<Struct>/args<Pointer<Struct>>/args<Reference<Struct>>/args<vector<T>>/args<Pointer<vector<T>>>/args<Reference<vector<T>>>/args<Reference<soa_vector<T>>> packs";
    return false;
  }
  return true;
}

bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto arrayVectorTargetInfo = resolveArrayVectorAccessTargetInfo(
      targetExpr, localsIn, resolveCallArrayVectorAccessTargetInfo);
  if (!validateArrayVectorAccessTargetInfo(arrayVectorTargetInfo, error)) {
    return false;
  }

  LocalInfo::ValueKind indexKind = LocalInfo::ValueKind::Unknown;
  if (!resolveValidatedAccessIndexKind(indexExpr, localsIn, accessName, inferExprKind, indexKind, error)) {
    return false;
  }

  const int32_t ptrLocal = allocTempLocal();
  if (!emitExpr(targetExpr, localsIn)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(ptrLocal));

  const int32_t indexLocal = allocTempLocal();
  if (!emitExpr(indexExpr, localsIn)) {
    return false;
  }
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));

  const bool isWrappedStructArgsPackTarget =
      arrayVectorTargetInfo.isArgsPackTarget &&
      (arrayVectorTargetInfo.argsPackElementKind == LocalInfo::Kind::Pointer ||
       arrayVectorTargetInfo.argsPackElementKind == LocalInfo::Kind::Reference);
  emitArrayVectorAccessLoad(
      accessName,
      ptrLocal,
      indexLocal,
      indexKind,
      arrayVectorTargetInfo.isVectorTarget,
      1,
      (arrayVectorTargetInfo.elemSlotCount > 0) ? arrayVectorTargetInfo.elemSlotCount : 1,
      arrayVectorTargetInfo.structTypeName.empty() || isWrappedStructArgsPackTarget,
      allocTempLocal,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  if (arrayVectorTargetInfo.isArgsPackTarget &&
      arrayVectorTargetInfo.argsPackElementKind == LocalInfo::Kind::Reference &&
      !arrayVectorTargetInfo.structTypeName.empty()) {
    emitInstruction(IrOpcode::LoadIndirect, 0);
  }
  return true;
}

bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitArrayVectorIndexedAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      {},
      inferExprKind,
      allocTempLocal,
      emitExpr,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  const auto stringTableAccessResult = tryEmitStringTableAccessLoad(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      inferExprKind,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
  if (stringTableAccessResult == StringTableAccessEmitResult::Error) {
    return false;
  }
  if (stringTableAccessResult == StringTableAccessEmitResult::Emitted) {
    return true;
  }

  const auto dynamicStringAccessResult = tryEmitDynamicStringAccessLoad(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      stringTableCount,
      inferExprKind,
      isEntryArgsName,
      allocTempLocal,
      emitExpr,
      emitStringIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
  if (dynamicStringAccessResult == DynamicStringAccessEmitResult::Error) {
    return false;
  }
  if (dynamicStringAccessResult == DynamicStringAccessEmitResult::Emitted) {
    return true;
  }

  const auto nonLiteralStringTargetResult =
      validateNonLiteralStringAccessTarget(targetExpr, localsIn, isEntryArgsName, error);
  if (nonLiteralStringTargetResult == NonLiteralStringAccessTargetResult::Stop) {
    return false;
  }
  if (nonLiteralStringTargetResult == NonLiteralStringAccessTargetResult::Error) {
    return false;
  }

  const auto mapLookupResult = tryEmitMapAccessLookup(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      allocTempLocal,
      emitExpr,
      resolveStringTableTarget,
      resolveCallMapAccessTargetInfo,
      inferExprKind,
      emitMapKeyNotFound,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
  if (mapLookupResult == MapAccessLookupEmitResult::Error) {
    return false;
  }
  if (mapLookupResult == MapAccessLookupEmitResult::Emitted) {
    return true;
  }

  return emitArrayVectorIndexedAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveCallArrayVectorAccessTargetInfo,
      inferExprKind,
      allocTempLocal,
      emitExpr,
      emitArrayIndexOutOfBounds,
      instructionCount,
      emitInstruction,
      patchInstructionImm,
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitBuiltinArrayAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      0,
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
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitBuiltinArrayAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      stringTableCount,
      {},
      {},
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
      error);
}

bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  return emitBuiltinArrayAccess(
      accessName,
      targetExpr,
      indexExpr,
      localsIn,
      resolveStringTableTarget,
      0,
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
      error);
}

IrOpcode mapKeyCompareOpcode(LocalInfo::ValueKind mapKeyKind) {
  if (mapKeyKind == LocalInfo::ValueKind::Int64 || mapKeyKind == LocalInfo::ValueKind::UInt64) {
    return IrOpcode::CmpEqI64;
  }
  if (mapKeyKind == LocalInfo::ValueKind::Float64) {
    return IrOpcode::CmpEqF64;
  }
  if (mapKeyKind == LocalInfo::ValueKind::Float32) {
    return IrOpcode::CmpEqF32;
  }
  return IrOpcode::CmpEqI32;
}

MapLookupStringKeyResult tryResolveMapLookupStringKey(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    int32_t &stringIndexOut,
    std::string &error) {
  (void)error;
  if (mapKeyKind != LocalInfo::ValueKind::String) {
    return MapLookupStringKeyResult::NotHandled;
  }
  size_t length = 0;
  if (!resolveStringTableTarget(lookupKeyExpr, localsIn, stringIndexOut, length)) {
    return MapLookupStringKeyResult::NotHandled;
  }
  return MapLookupStringKeyResult::Resolved;
}

MapLookupKeyLocalEmitResult tryEmitMapLookupStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error) {
  int32_t stringIndex = -1;
  const auto resolveResult = tryResolveMapLookupStringKey(
      mapKeyKind, lookupKeyExpr, localsIn, resolveStringTableTarget, stringIndex, error);
  if (resolveResult == MapLookupStringKeyResult::NotHandled) {
    return MapLookupKeyLocalEmitResult::NotHandled;
  }
  if (resolveResult == MapLookupStringKeyResult::Error) {
    return MapLookupKeyLocalEmitResult::Error;
  }
  emitPushI32(stringIndex);
  emitStoreLocal(keyLocal);
  return MapLookupKeyLocalEmitResult::Emitted;
}

bool emitMapLookupNonStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error) {
  const LocalInfo::ValueKind lookupKeyKind = inferExprKind(lookupKeyExpr, localsIn);
  if (!validateMapLookupKeyKind(mapKeyKind, lookupKeyKind, error)) {
    return false;
  }
  if (!emitExpr(lookupKeyExpr, localsIn)) {
    return false;
  }
  emitStoreLocal(keyLocal);
  return true;
}

bool emitMapLookupKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &keyLocalOut,
    std::string &error) {
  keyLocalOut = allocTempLocal();
  const auto stringLookupKeyEmitResult = tryEmitMapLookupStringKeyLocal(
      mapKeyKind,
      lookupKeyExpr,
      localsIn,
      resolveStringTableTarget,
      emitPushI32,
      emitStoreLocal,
      keyLocalOut,
      error);
  if (stringLookupKeyEmitResult == MapLookupKeyLocalEmitResult::Error) {
    return false;
  }
  if (stringLookupKeyEmitResult == MapLookupKeyLocalEmitResult::Emitted) {
    return true;
  }
  return emitMapLookupNonStringKeyLocal(
      mapKeyKind, lookupKeyExpr, localsIn, inferExprKind, emitExpr, emitStoreLocal, keyLocalOut, error);
}

bool emitMapLookupTargetPointerLocal(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &ptrLocalOut) {
  ptrLocalOut = allocTempLocal();
  if (!emitExpr(targetExpr, localsIn)) {
    return false;
  }
  emitStoreLocal(ptrLocalOut);
  return true;
}

MapLookupLoopLocals emitMapLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  const auto loopLocals = emitMapLookupLoopLocals(ptrLocal, allocTempLocal, emitInstruction);
  const auto loopCondition = emitMapLookupLoopCondition(
      loopLocals.indexLocal, loopLocals.countLocal, instructionCount, emitInstruction);
  const auto loopMatch = emitMapLookupLoopMatchCheck(
      ptrLocal, loopLocals.indexLocal, keyLocal, mapKeyKind, instructionCount, emitInstruction);
  emitMapLookupLoopAdvanceAndPatch(
      loopMatch.jumpNotMatch,
      loopCondition.jumpLoopEnd,
      loopMatch.jumpFound,
      loopCondition.loopStart,
      loopLocals.indexLocal,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return loopLocals;
}

void emitMapLookupAccessEpilogue(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    emitMapLookupAtKeyNotFoundGuard(
        indexLocal, countLocal, emitMapKeyNotFound, instructionCount, emitInstruction, patchInstructionImm);
  }
  emitMapLookupValueLoad(ptrLocal, indexLocal, emitInstruction);
}

void emitMapLookupContainsResult(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
}

bool emitMapLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error) {
  int32_t ptrLocal = -1;
  if (!emitMapLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitMapLookupKeyLocal(
          mapKeyKind,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          resolveStringTableTarget,
          inferExprKind,
          emitExpr,
          [&](int32_t stringIndex) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)); },
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          keyLocal,
          error)) {
    return false;
  }

  const auto loopLocals = emitMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, mapKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitMapLookupAccessEpilogue(
      accessName,
      ptrLocal,
      loopLocals.indexLocal,
      loopLocals.countLocal,
      emitMapKeyNotFound,
      instructionCount,
      emitInstruction,
      patchInstructionImm);
  return true;
}

bool emitMapLookupContains(
    LocalInfo::ValueKind mapKeyKind,
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
    std::string &error) {
  int32_t ptrLocal = -1;
  if (!emitMapLookupTargetPointerLocal(
          targetExpr,
          localsIn,
          allocTempLocal,
          emitExpr,
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          ptrLocal)) {
    return false;
  }

  int32_t keyLocal = -1;
  if (!emitMapLookupKeyLocal(
          mapKeyKind,
          lookupKeyExpr,
          localsIn,
          allocTempLocal,
          resolveStringTableTarget,
          inferExprKind,
          emitExpr,
          [&](int32_t stringIndex) { emitInstruction(IrOpcode::PushI32, static_cast<uint64_t>(stringIndex)); },
          [&](int32_t localIndex) { emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(localIndex)); },
          keyLocal,
          error)) {
    return false;
  }

  const auto loopLocals = emitMapLookupLoopSearchScaffold(
      ptrLocal, keyLocal, mapKeyKind, allocTempLocal, instructionCount, emitInstruction, patchInstructionImm);
  emitMapLookupContainsResult(loopLocals.indexLocal, loopLocals.countLocal, emitInstruction);
  return true;
}

void emitStringAccessLoad(
    const std::string &accessName,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    size_t stringLength,
    int32_t stringIndex,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    if (indexKind != LocalInfo::ValueKind::UInt64) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
      emitInstruction(pushZeroForIndex(indexKind), 0);
      emitInstruction(cmpLtForIndex(indexKind), 0);
      const size_t jumpNonNegative = instructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitStringIndexOutOfBounds();
      patchInstructionImm(jumpNonNegative, static_cast<uint64_t>(instructionCount()));
    }

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    const IrOpcode lengthOp =
        (indexKind == LocalInfo::ValueKind::Int32) ? IrOpcode::PushI32 : IrOpcode::PushI64;
    emitInstruction(lengthOp, static_cast<uint64_t>(stringLength));
    emitInstruction(cmpGeForIndex(indexKind), 0);
    const size_t jumpInRange = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitStringIndexOutOfBounds();
    patchInstructionImm(jumpInRange, static_cast<uint64_t>(instructionCount()));
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadStringByte, static_cast<uint64_t>(stringIndex));
}

void emitArrayVectorAccessLoad(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    bool isVectorTarget,
    uint64_t arrayHeaderSlots,
    int32_t elementSlotCount,
    bool loadElementValue,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  if (accessName == "at") {
    const int32_t countLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(countLocal));

    if (indexKind != LocalInfo::ValueKind::UInt64) {
      emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
      emitInstruction(pushZeroForIndex(indexKind), 0);
      emitInstruction(cmpLtForIndex(indexKind), 0);
      const size_t jumpNonNegative = instructionCount();
      emitInstruction(IrOpcode::JumpIfZero, 0);
      emitArrayIndexOutOfBounds();
      patchInstructionImm(jumpNonNegative, static_cast<uint64_t>(instructionCount()));
    }

    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
    emitInstruction(cmpGeForIndex(indexKind), 0);
    const size_t jumpInRange = instructionCount();
    emitInstruction(IrOpcode::JumpIfZero, 0);
    emitArrayIndexOutOfBounds();
    patchInstructionImm(jumpInRange, static_cast<uint64_t>(instructionCount()));
  }

  int32_t basePtrLocal = ptrLocal;
  if (isVectorTarget) {
    basePtrLocal = allocTempLocal();
    emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
    emitInstruction(IrOpcode::PushI64, static_cast<uint64_t>(2 * IrSlotBytes));
    emitInstruction(IrOpcode::AddI64, 0);
    emitInstruction(IrOpcode::LoadIndirect, 0);
    emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(basePtrLocal));
  }

  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(basePtrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  if (!isVectorTarget) {
    if (elementSlotCount > 1) {
      emitInstruction(pushOneForIndex(indexKind), static_cast<uint64_t>(elementSlotCount));
      emitInstruction(mulForIndex(indexKind), 0);
    }
    emitInstruction(pushOneForIndex(indexKind), arrayHeaderSlots);
    emitInstruction(addForIndex(indexKind), 0);
  } else if (elementSlotCount > 1) {
    emitInstruction(pushOneForIndex(indexKind), static_cast<uint64_t>(elementSlotCount));
    emitInstruction(mulForIndex(indexKind), 0);
  }
  emitInstruction(pushOneForIndex(indexKind), IrSlotBytesI32);
  emitInstruction(mulForIndex(indexKind), 0);
  emitInstruction(IrOpcode::AddI64, 0);
  if (loadElementValue) {
    emitInstruction(IrOpcode::LoadIndirect, 0);
  }
}

MapLookupLoopLocals emitMapLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  MapLookupLoopLocals locals;
  locals.countLocal = allocTempLocal();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.countLocal));

  locals.indexLocal = allocTempLocal();
  emitInstruction(IrOpcode::PushI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(locals.indexLocal));
  return locals;
}

MapLookupLoopConditionAnchors emitMapLookupLoopCondition(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  MapLookupLoopConditionAnchors anchors;
  anchors.loopStart = instructionCount();
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpLtI32, 0);
  anchors.jumpLoopEnd = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  return anchors;
}

MapLookupLoopMatchAnchors emitMapLookupLoopMatchCheck(
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(keyLocal));
  emitInstruction(mapKeyCompareOpcode(mapKeyKind), 0);

  MapLookupLoopMatchAnchors anchors;
  anchors.jumpNotMatch = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  anchors.jumpFound = instructionCount();
  emitInstruction(IrOpcode::Jump, 0);
  return anchors;
}

void emitMapLookupLoopAdvanceAndPatch(
    size_t jumpNotMatch,
    size_t jumpLoopEnd,
    size_t jumpFound,
    size_t loopStart,
    int32_t indexLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  const size_t notMatchIndex = instructionCount();
  patchInstructionImm(jumpNotMatch, static_cast<uint64_t>(notMatchIndex));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 1);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::StoreLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::Jump, static_cast<uint64_t>(loopStart));

  const size_t loopEndIndex = instructionCount();
  patchInstructionImm(jumpLoopEnd, static_cast<uint64_t>(loopEndIndex));
  patchInstructionImm(jumpFound, static_cast<uint64_t>(loopEndIndex));
}

void emitMapLookupAtKeyNotFoundGuard(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(countLocal));
  emitInstruction(IrOpcode::CmpEqI32, 0);
  const size_t jumpKeyFound = instructionCount();
  emitInstruction(IrOpcode::JumpIfZero, 0);
  emitMapKeyNotFound();
  const size_t keyFoundIndex = instructionCount();
  patchInstructionImm(jumpKeyFound, static_cast<uint64_t>(keyFoundIndex));
}

void emitMapLookupValueLoad(
    int32_t ptrLocal,
    int32_t indexLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction) {
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(ptrLocal));
  emitInstruction(IrOpcode::LoadLocal, static_cast<uint64_t>(indexLocal));
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::PushI32, 2);
  emitInstruction(IrOpcode::AddI32, 0);
  emitInstruction(IrOpcode::PushI32, IrSlotBytesI32);
  emitInstruction(IrOpcode::MulI32, 0);
  emitInstruction(IrOpcode::AddI64, 0);
  emitInstruction(IrOpcode::LoadIndirect, 0);
}

bool validateMapLookupKeyKind(LocalInfo::ValueKind mapKeyKind,
                              LocalInfo::ValueKind lookupKeyKind,
                              std::string &error) {
  if (lookupKeyKind == LocalInfo::ValueKind::Unknown) {
    error = "native backend requires map lookup key type to match map key type";
    return false;
  }
  if (mapKeyKind == LocalInfo::ValueKind::String) {
    if (lookupKeyKind != LocalInfo::ValueKind::String) {
      error = "native backend requires map lookup key type to match map key type";
      return false;
    }
    return true;
  }
  if (lookupKeyKind == LocalInfo::ValueKind::String) {
    error = "native backend requires map lookup key to be numeric/bool";
    return false;
  }
  if (lookupKeyKind != mapKeyKind) {
    error = "native backend requires map lookup key type to match map key type";
    return false;
  }
  return true;
}

CountMethodFallbackResult tryEmitNonMethodCountFallback(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error,
    std::function<bool(const Expr &)> isCollectionAccessReceiverExpr) {
  if (isExplicitMapHelperFallbackPath(expr)) {
    return CountMethodFallbackResult::NotHandled;
  }
  std::string normalizedVectorHelperName;
  std::string normalizedMapHelperName;
  const bool hasVectorHelperAlias = resolveVectorHelperAliasName(expr, normalizedVectorHelperName);
  const bool hasMapHelperAlias = resolveMapHelperAliasName(expr, normalizedMapHelperName);
  const bool isCountCall = isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count");
  const bool isCapacityCall = isVectorBuiltinName(expr, "capacity");
  std::string accessName;
  const bool isCollectionAccessCall = getBuiltinArrayAccessName(expr, accessName);
  const bool isAccessCall = isCollectionAccessCall || isSimpleCallName(expr, "get") || isSimpleCallName(expr, "ref");
  const bool isVectorMutatorCall =
      isVectorBuiltinName(expr, "push") || isVectorBuiltinName(expr, "pop") || isVectorBuiltinName(expr, "reserve") ||
      isVectorBuiltinName(expr, "clear") || isVectorBuiltinName(expr, "remove_at") ||
      isVectorBuiltinName(expr, "remove_swap");
  auto expectedVectorMutatorArgCount = [&]() -> size_t {
    if (isVectorBuiltinName(expr, "pop") || isVectorBuiltinName(expr, "clear")) {
      return 1u;
    }
    return 2u;
  };
  size_t expectedArgCount = 1u;
  if (isAccessCall) {
    expectedArgCount = 2u;
  } else if (isVectorMutatorCall) {
    expectedArgCount = expectedVectorMutatorArgCount();
  }
  if (expr.isMethodCall || (!isCountCall && !isCapacityCall && !isAccessCall && !isVectorMutatorCall) ||
      expr.args.size() != expectedArgCount) {
    return CountMethodFallbackResult::NotHandled;
  }
  (void)isArrayCountCall;
  (void)isStringCountCall;

  auto hasNamedArgs = [&]() {
    for (const auto &argName : expr.argNames) {
      if (argName.has_value()) {
        return true;
      }
    }
    return false;
  };
  auto buildMethodExprForReceiverIndex = [&](size_t receiverIndex) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    if (hasVectorHelperAlias) {
      methodExpr.name = normalizedVectorHelperName;
    } else if (hasMapHelperAlias) {
      methodExpr.name = normalizedMapHelperName;
    }
    if (receiverIndex != 0 && receiverIndex < methodExpr.args.size()) {
      std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
      if (methodExpr.argNames.size() < methodExpr.args.size()) {
        methodExpr.argNames.resize(methodExpr.args.size());
      }
      std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
    }
    return methodExpr;
  };

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
  const bool hasNamedArgsValue = hasNamedArgs();
  if (hasNamedArgsValue) {
    bool hasValuesNamedReceiver = false;
    if (isVectorMutatorCall || isAccessCall) {
      for (size_t i = 0; i < expr.args.size(); ++i) {
        if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
            *expr.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
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
  const bool probePositionalReorderedVectorMutatorReceiver =
      isVectorMutatorCall && !hasNamedArgsValue && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
       expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
       expr.args.front().kind == Expr::Kind::Name);
  if (probePositionalReorderedVectorMutatorReceiver) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool probePositionalReorderedAccessReceiver =
      isAccessCall && !hasNamedArgsValue && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
       expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
       (expr.args.front().kind == Expr::Kind::Name &&
        (!isCollectionAccessReceiverExpr ||
         !isCollectionAccessReceiverExpr(expr.args.front()))));
  if (probePositionalReorderedAccessReceiver) {
    for (size_t i = 1; i < expr.args.size(); ++i) {
      appendReceiverIndex(i);
    }
  }
  const bool hasAlternativeCollectionReceiver =
      probePositionalReorderedAccessReceiver && isCollectionAccessReceiverExpr &&
      std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
        return index > 0 && index < expr.args.size() && isCollectionAccessReceiverExpr(expr.args[index]);
      });

  const std::string priorError = error;
  for (size_t receiverIndex : receiverIndices) {
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    const Definition *callee = resolveMethodCallDefinition(methodExpr);
    if (hasAlternativeCollectionReceiver && receiverIndex == 0 && callee != nullptr) {
      continue;
    }
    const auto emitResult = emitResolvedInlineDefinitionCall(
        methodExpr, callee, emitInlineDefinitionCall, error);
    if (emitResult == ResolvedInlineCallResult::Emitted) {
      return CountMethodFallbackResult::Emitted;
    }
    if (emitResult == ResolvedInlineCallResult::Error) {
      return CountMethodFallbackResult::Error;
    }
    error = priorError;
  }
  return CountMethodFallbackResult::NoCallee;
}

bool buildOrderedCallArguments(const Expr &callExpr,
                               const std::vector<Expr> &params,
                               std::vector<const Expr *> &ordered,
                               std::string &error) {
  ordered.assign(params.size(), nullptr);
  size_t positionalIndex = 0;
  for (size_t i = 0; i < callExpr.args.size(); ++i) {
    if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
      const std::string &name = *callExpr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &callExpr.args[i];
      continue;
    }
    while (positionalIndex < ordered.size() && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (positionalIndex >= ordered.size()) {
      error = "argument count mismatch";
      return false;
    }
    ordered[positionalIndex] = &callExpr.args[i];
    ++positionalIndex;
  }

  for (size_t i = 0; i < ordered.size(); ++i) {
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
      continue;
    }
    error = "argument count mismatch";
    return false;
  }
  return true;
}

bool buildOrderedCallArgumentsWithPackedArgs(const Expr &callExpr,
                                            const std::vector<Expr> &params,
                                            std::vector<const Expr *> &ordered,
                                            std::vector<const Expr *> &packedArgs,
                                            size_t &packedParamIndex,
                                            std::string &error) {
  ordered.assign(params.size(), nullptr);
  packedArgs.clear();
  packedParamIndex = params.size();
  if (!params.empty() && isArgsPackParam(params.back())) {
    packedParamIndex = params.size() - 1;
  }

  size_t positionalIndex = 0;
  for (size_t i = 0; i < callExpr.args.size(); ++i) {
    if (i < callExpr.argNames.size() && callExpr.argNames[i].has_value()) {
      const std::string &name = *callExpr.argNames[i];
      size_t index = params.size();
      for (size_t p = 0; p < params.size(); ++p) {
        if (params[p].name == name) {
          index = p;
          break;
        }
      }
      if (index >= params.size()) {
        error = "unknown named argument: " + name;
        return false;
      }
      if (index == packedParamIndex) {
        error = "named arguments cannot bind variadic parameter: " + name;
        return false;
      }
      if (ordered[index] != nullptr) {
        error = "named argument duplicates parameter: " + name;
        return false;
      }
      ordered[index] = &callExpr.args[i];
      continue;
    }

    if (callExpr.args[i].isSpread) {
      if (packedParamIndex >= params.size()) {
        error = "spread argument requires variadic parameter";
        return false;
      }
      packedArgs.push_back(&callExpr.args[i]);
      continue;
    }

    while (positionalIndex < params.size() && positionalIndex != packedParamIndex && ordered[positionalIndex] != nullptr) {
      ++positionalIndex;
    }
    if (packedParamIndex < params.size() && positionalIndex >= packedParamIndex) {
      packedArgs.push_back(&callExpr.args[i]);
      continue;
    }
    if (positionalIndex >= params.size()) {
      error = "argument count mismatch";
      return false;
    }
    ordered[positionalIndex] = &callExpr.args[i];
    ++positionalIndex;
  }

  for (size_t i = 0; i < params.size(); ++i) {
    if (i == packedParamIndex) {
      continue;
    }
    if (ordered[i] != nullptr) {
      continue;
    }
    if (!params[i].args.empty()) {
      ordered[i] = &params[i].args.front();
      continue;
    }
    error = "argument count mismatch";
    return false;
  }

  return true;
}

bool definitionHasTransform(const Definition &def, const std::string &transformName) {
  for (const auto &transform : def.transforms) {
    if (transform.name == transformName) {
      return true;
    }
  }
  return false;
}

bool isStructTransformName(const std::string &name) {
  return name == "struct" || name == "pod" || name == "handle" || name == "gpu_lane" || name == "no_padding" ||
         name == "platform_independent_padding";
}

bool isStructDefinition(const Definition &def) {
  auto hasTypeLikeName = [&]() -> bool {
    if (def.fullPath.empty()) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    const std::string_view name = (slash == std::string::npos) ? std::string_view(def.fullPath)
                                                                : std::string_view(def.fullPath).substr(slash + 1);
    if (name.empty()) {
      return false;
    }
    return std::isupper(static_cast<unsigned char>(name.front())) != 0;
  };
  bool hasStruct = false;
  bool hasReturn = false;
  for (const auto &transform : def.transforms) {
    if (transform.name == "return") {
      hasReturn = true;
    }
    if (isStructTransformName(transform.name)) {
      hasStruct = true;
    }
  }
  if (hasStruct) {
    return true;
  }
  if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
    return false;
  }
  if (def.statements.empty()) {
    return hasTypeLikeName();
  }
  for (const auto &stmt : def.statements) {
    if (!stmt.isBinding) {
      return false;
    }
  }
  return true;
}

bool isStructHelperDefinition(const Definition &def,
                              const std::unordered_set<std::string> &structNames,
                              std::string &parentStructPathOut) {
  parentStructPathOut.clear();
  if (!def.isNested) {
    return false;
  }
  if (structNames.count(def.fullPath) > 0) {
    return false;
  }
  const size_t slash = def.fullPath.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return false;
  }
  const std::string parent = def.fullPath.substr(0, slash);
  if (structNames.count(parent) == 0) {
    return false;
  }
  parentStructPathOut = parent;
  return true;
}

Expr makeStructHelperThisParam(const std::string &structPath, bool isMutable) {
  Expr param;
  param.kind = Expr::Kind::Name;
  param.isBinding = true;
  param.name = "this";
  Transform typeTransform;
  typeTransform.name = "Reference";
  typeTransform.templateArgs.push_back(structPath);
  param.transforms.push_back(std::move(typeTransform));
  if (isMutable) {
    Transform mutTransform;
    mutTransform.name = "mut";
    param.transforms.push_back(std::move(mutTransform));
  }
  return param;
}

bool isStaticFieldBinding(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

bool collectInstanceStructFieldParams(const Definition &structDef,
                                      std::vector<Expr> &paramsOut,
                                      std::string &error) {
  paramsOut.clear();
  paramsOut.reserve(structDef.statements.size());
  for (const auto &param : structDef.statements) {
    if (!param.isBinding) {
      error = "struct definitions may only contain field bindings: " + structDef.fullPath;
      return false;
    }
    if (isStaticFieldBinding(param)) {
      continue;
    }
    paramsOut.push_back(param);
  }
  return true;
}

bool buildInlineCallParameterList(const Definition &callee,
                                  const std::unordered_set<std::string> &structNames,
                                  std::vector<Expr> &paramsOut,
                                  std::string &error) {
  paramsOut.clear();
  if (isStructDefinition(callee)) {
    return collectInstanceStructFieldParams(callee, paramsOut, error);
  }

  std::string helperParent;
  if (!isStructHelperDefinition(callee, structNames, helperParent) ||
      definitionHasTransform(callee, "static")) {
    paramsOut = callee.parameters;
    return true;
  }

  paramsOut.reserve(callee.parameters.size() + 1);
  paramsOut.push_back(makeStructHelperThisParam(
      helperParent,
      definitionHasTransform(callee, "mut")));
  for (const auto &param : callee.parameters) {
    paramsOut.push_back(param);
  }
  return true;
}

bool buildInlineCallOrderedArguments(const Expr &callExpr,
                                     const Definition &callee,
                                     const std::unordered_set<std::string> &structNames,
                                     const LocalMap &callerLocals,
                                     std::vector<Expr> &paramsOut,
                                     std::vector<const Expr *> &orderedArgsOut,
                                     std::vector<const Expr *> &packedArgsOut,
                                     size_t &packedParamIndexOut,
                                     std::string &error) {
  if (!buildInlineCallParameterList(callee, structNames, paramsOut, error)) {
    return false;
  }
  if (callExpr.isMethodCall && !callExpr.args.empty()) {
    const Expr &receiver = callExpr.args.front();
    if (receiver.kind == Expr::Kind::Name && callerLocals.find(receiver.name) == callerLocals.end()) {
      const size_t methodSlash = callee.fullPath.find_last_of('/');
      if (methodSlash != std::string::npos && methodSlash > 0) {
        const std::string receiverPath = callee.fullPath.substr(0, methodSlash);
        const size_t receiverSlash = receiverPath.find_last_of('/');
        const std::string receiverTypeName =
            receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
        if (receiverTypeName == receiver.name) {
          Expr trimmedCallExpr = callExpr;
          trimmedCallExpr.args.erase(trimmedCallExpr.args.begin());
          if (!trimmedCallExpr.argNames.empty()) {
            trimmedCallExpr.argNames.erase(trimmedCallExpr.argNames.begin());
          }
          return buildOrderedCallArgumentsWithPackedArgs(
              trimmedCallExpr, paramsOut, orderedArgsOut, packedArgsOut, packedParamIndexOut, error);
        }
      }
    }
  }
  return buildOrderedCallArgumentsWithPackedArgs(
      callExpr, paramsOut, orderedArgsOut, packedArgsOut, packedParamIndexOut, error);
}

std::string resolveDefinitionNamespacePrefix(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath) {
  const Definition *definition = resolveDefinitionByPath(defMap, definitionPath);
  if (definition == nullptr) {
    return "";
  }
  return definition->namespacePrefix;
}

const Definition *resolveDefinitionByPath(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath) {
  auto defIt = defMap.find(definitionPath);
  if (defIt == defMap.end()) {
    return nullptr;
  }
  return defIt->second;
}

} // namespace primec::ir_lowerer
