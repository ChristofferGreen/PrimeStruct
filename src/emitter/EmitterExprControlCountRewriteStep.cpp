#include "EmitterExprControlCountRewriteStep.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "EmitterHelpers.h"

namespace primec::emitter {

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
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string vectorPrefix = "vector/";
  const std::string arrayPrefix = "array/";
  const std::string stdVectorPrefix = "std/collections/vector/";
  if (normalized.rfind(vectorPrefix, 0) == 0) {
    helperNameOut = normalized.substr(vectorPrefix.size());
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
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  const std::string mapPrefix = "map/";
  const std::string stdMapPrefix = "std/collections/map/";
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

bool isNamespacedVectorHelperCall(const Expr &expr) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(0, 1);
  }
  if (normalized.rfind("vector/", 0) == 0 || normalized.rfind("std/collections/vector/", 0) == 0) {
    const size_t prefixLen = normalized.rfind("vector/", 0) == 0 ? std::string("vector/").size()
                                                                  : std::string("std/collections/vector/").size();
    const std::string helper = normalized.substr(prefixLen);
    return !isRemovedVectorCompatibilityHelper(helper);
  }
  if (normalized.rfind("array/", 0) == 0) {
    const std::string helper = normalized.substr(std::string("array/").size());
    return !isRemovedVectorCompatibilityHelper(helper);
  }
  return false;
}

bool isNamespacedMapHelperCall(const Expr &expr) {
  if (expr.name.empty()) {
    return false;
  }
  std::string normalized = expr.name;
  if (!normalized.empty() && normalized.front() == '/') {
    normalized.erase(normalized.begin());
  }
  return normalized.rfind("map/", 0) == 0 ||
         normalized.rfind("std/collections/map/", 0) == 0;
}

std::string preserveExplicitCanonicalMapHelperPath(const std::string &resolvedPath,
                                                   const std::string &candidatePath) {
  std::string normalizedResolvedPath = resolvedPath;
  if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() != '/' &&
      normalizedResolvedPath.rfind("std/collections/map/", 0) == 0) {
    normalizedResolvedPath.insert(normalizedResolvedPath.begin(), '/');
  }
  if (normalizedResolvedPath.rfind("/std/collections/map/", 0) != 0) {
    return candidatePath;
  }

  const std::string suffix =
      normalizedResolvedPath.substr(std::string("/std/collections/map/").size());
  if (suffix != "count" && suffix != "at" && suffix != "at_unsafe") {
    return candidatePath;
  }

  std::string normalizedCandidatePath = candidatePath;
  if (!normalizedCandidatePath.empty() && normalizedCandidatePath.front() != '/' &&
      normalizedCandidatePath.rfind("map/", 0) == 0) {
    normalizedCandidatePath.insert(normalizedCandidatePath.begin(), '/');
  }
  if (normalizedCandidatePath == "/map/" + suffix) {
    return normalizedResolvedPath;
  }
  return candidatePath;
}

} // namespace

std::optional<std::string> runEmitterExprControlCountRewriteStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isArrayCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isMapCountCall,
    const EmitterExprControlCountRewriteIsCountLikeCallFn &isStringCountCall,
    const EmitterExprControlCountRewriteResolveMethodPathFn &resolveMethodPath,
    const EmitterExprControlCountRewriteIsCollectionAccessReceiverFn &isCollectionAccessReceiverExpr) {
  (void)localTypes;
  (void)isArrayCountCall;
  (void)isMapCountCall;
  (void)isStringCountCall;
  const bool isNamespacedCollectionHelperCall =
      isNamespacedVectorHelperCall(expr) || isNamespacedMapHelperCall(expr);
  if (expr.isMethodCall ||
      (!isNamespacedCollectionHelperCall && nameMap.count(resolvedPath) != 0)) {
    return std::nullopt;
  }
  const bool isCountLikeCall =
      (isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) &&
      expr.args.size() == 1;
  const bool isCapacityLikeCall = isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1;
  const bool isAccessLikeCall =
      (isVectorBuiltinName(expr, "at") || isVectorBuiltinName(expr, "at_unsafe") ||
       isMapBuiltinName(expr, "at") || isMapBuiltinName(expr, "at_unsafe")) &&
      expr.args.size() == 2;
  const bool isVectorMutatorLikeCall =
      ((isVectorBuiltinName(expr, "push") || isVectorBuiltinName(expr, "reserve") ||
        isVectorBuiltinName(expr, "remove_at") || isVectorBuiltinName(expr, "remove_swap")) &&
       expr.args.size() == 2) ||
      ((isVectorBuiltinName(expr, "pop") || isVectorBuiltinName(expr, "clear")) && expr.args.size() == 1);
  if (!isCountLikeCall && !isCapacityLikeCall && !isAccessLikeCall && !isVectorMutatorLikeCall) {
    return std::nullopt;
  }
  if (!resolveMethodPath) {
    return std::nullopt;
  }
  const bool hasNamedArgs = hasNamedArguments(expr.argNames);
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
  if (hasNamedArgs) {
    bool hasValuesNamedReceiver = false;
    for (size_t i = 0; i < expr.args.size(); ++i) {
      if (i < expr.argNames.size() && expr.argNames[i].has_value() && *expr.argNames[i] == "values") {
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
  const bool probePositionalReorderedAccessReceiver =
      isAccessLikeCall && !hasNamedArgs && expr.args.size() > 1 &&
      (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
       expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
       (expr.args.front().kind == Expr::Kind::Name &&
        (!isCollectionAccessReceiverExpr || !isCollectionAccessReceiverExpr(expr.args.front()))));
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
  for (size_t receiverIndex : receiverIndices) {
    Expr methodExpr = expr;
    methodExpr.isMethodCall = true;
    std::string normalizedHelperName;
    if (resolveVectorHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    } else if (resolveMapHelperAliasName(methodExpr, normalizedHelperName)) {
      methodExpr.name = normalizedHelperName;
    }
    if (receiverIndex != 0 && receiverIndex < methodExpr.args.size()) {
      std::swap(methodExpr.args[0], methodExpr.args[receiverIndex]);
      if (methodExpr.argNames.size() < methodExpr.args.size()) {
        methodExpr.argNames.resize(methodExpr.args.size());
      }
      std::swap(methodExpr.argNames[0], methodExpr.argNames[receiverIndex]);
    }
    std::string methodPath;
    if (resolveMethodPath(methodExpr, methodPath)) {
      if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
        continue;
      }
      const std::string canonicalPreservedPath =
          preserveExplicitCanonicalMapHelperPath(resolvedPath, methodPath);
      const std::string preferredPath = preferVectorStdlibHelperPath(canonicalPreservedPath, nameMap);
      if (isVectorMutatorLikeCall && nameMap.count(preferredPath) == 0) {
        continue;
      }
      return preferredPath;
    }
  }
  return std::nullopt;
}

} // namespace primec::emitter
