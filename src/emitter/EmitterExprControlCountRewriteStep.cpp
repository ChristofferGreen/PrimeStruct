#include "EmitterExprControlCountRewriteStep.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "EmitterHelpers.h"

namespace primec::emitter {

namespace {

bool isBareCallName(const Expr &expr, const char *name) {
  if (expr.kind != Expr::Kind::Call || expr.name.empty() || name == nullptr) {
    return false;
  }
  if (!expr.namespacePrefix.empty()) {
    return false;
  }
  if (!expr.name.empty() && expr.name.front() == '/') {
    return false;
  }
  if (expr.name.find('/') != std::string::npos) {
    return false;
  }
  return expr.name == name;
}

bool isVectorBuiltinName(const Expr &expr, const char *name) {
  return isBareCallName(expr, name);
}

bool isMapBuiltinName(const Expr &expr, const char *name) {
  return isBareCallName(expr, name);
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
  if (expr.isMethodCall ||
      nameMap.count(resolvedPath) != 0) {
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
      const std::string preferredPath = preferVectorStdlibHelperPath(methodPath, nameMap);
      if (isVectorMutatorLikeCall && nameMap.count(preferredPath) == 0) {
        continue;
      }
      return preferredPath;
    }
  }
  return std::nullopt;
}

} // namespace primec::emitter
