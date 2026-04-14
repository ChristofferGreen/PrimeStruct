#include "IrLowererCallHelpers.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <utility>

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCountAccessHelpers.h"
#include "IrLowererFlowHelpers.h"
#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "IrLowererSetupTypeHelpers.h"

namespace primec::ir_lowerer {

namespace {
bool isRemovedVectorHelperDefinitionPath(const std::string &path) {
  auto matchesPath = [&](std::string_view basePath) {
    return path == basePath || path.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  return matchesPath("/vector/count") || matchesPath("/vector/capacity") ||
         matchesPath("/vector/at") || matchesPath("/vector/at_unsafe") ||
         matchesPath("/vector/push") || matchesPath("/vector/pop") ||
         matchesPath("/vector/reserve") || matchesPath("/vector/clear") ||
         matchesPath("/vector/remove_at") || matchesPath("/vector/remove_swap") ||
         matchesPath("/std/collections/vector/count") ||
         matchesPath("/std/collections/vector/capacity") ||
         matchesPath("/std/collections/vector/at") ||
         matchesPath("/std/collections/vector/at_unsafe") ||
         matchesPath("/std/collections/vector/push") ||
         matchesPath("/std/collections/vector/pop") ||
         matchesPath("/std/collections/vector/reserve") ||
         matchesPath("/std/collections/vector/clear") ||
         matchesPath("/std/collections/vector/remove_at") ||
         matchesPath("/std/collections/vector/remove_swap") ||
         matchesPath("/std/collections/vectorCount") ||
         matchesPath("/std/collections/vectorCapacity") ||
         matchesPath("/std/collections/vectorAt") ||
         matchesPath("/std/collections/vectorAtUnsafe") ||
         matchesPath("/std/collections/vectorPush") ||
         matchesPath("/std/collections/vectorPop") ||
         matchesPath("/std/collections/vectorReserve") ||
         matchesPath("/std/collections/vectorClear") ||
         matchesPath("/std/collections/vectorRemoveAt") ||
         matchesPath("/std/collections/vectorRemoveSwap");
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

} // namespace

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

CountMethodFallbackResult tryEmitNonMethodCountFallback(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
  std::string &error,
  std::function<bool(const Expr &)> isCollectionAccessReceiverExpr) {
  if (!expr.isMethodCall && expr.name.rfind("/std/collections/experimental_vector/", 0) == 0) {
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
    } else if (std::string borrowedMapHelperName; resolveBorrowedMapHelperAliasName(methodExpr, borrowedMapHelperName)) {
      methodExpr.name = borrowedMapHelperName;
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
  const bool primaryReceiverIsFallbackCandidate =
      expr.args.empty() || !isCollectionAccessReceiverExpr ||
      isCollectionAccessReceiverExpr(expr.args.front());
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
      if (primaryReceiverIsFallbackCandidate) {
        appendReceiverIndex(0);
      }
      for (size_t i = 1; i < expr.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
  } else {
    if (primaryReceiverIsFallbackCandidate) {
      appendReceiverIndex(0);
    }
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
  const bool debugDirectMapAtCountCall =
      !expr.isMethodCall &&
      (isVectorBuiltinName(expr, "count") || isMapBuiltinName(expr, "count")) &&
      expr.args.size() == 1 &&
      expr.args.front().kind == Expr::Kind::Call &&
      expr.args.front().name == "/std/collections/map/at";
  for (size_t receiverIndex : receiverIndices) {
    Expr methodExpr = buildMethodExprForReceiverIndex(receiverIndex);
    const Definition *callee = resolveMethodCallDefinition(methodExpr);
    if (hasAlternativeCollectionReceiver && receiverIndex == 0 && callee != nullptr) {
      continue;
    }
    if (callee != nullptr && isRemovedVectorHelperDefinitionPath(callee->fullPath)) {
      error = priorError;
      continue;
    }
    const auto emitResult = emitResolvedInlineDefinitionCall(
        methodExpr, callee, emitInlineDefinitionCall, error);
    if (emitResult == ResolvedInlineCallResult::Emitted) {
      if (debugDirectMapAtCountCall && callee != nullptr) {
        error = "debug: non-method count fallback emitted via " + callee->fullPath;
        return CountMethodFallbackResult::Error;
      }
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
  const std::string callName = callExpr.name.empty() ? "<anonymous>" : callExpr.name;
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
      error = "argument count mismatch for " + callName;
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
    error = "argument count mismatch for " + callName;
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
  const std::string callName = callExpr.name.empty() ? "<anonymous>" : callExpr.name;
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
      error = "argument count mismatch for " + callName;
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
    error = "argument count mismatch for " + callName;
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

bool isStructDefinition(const Definition &def, const SemanticProductTargetAdapter *semanticProductTargets) {
  if (semanticProductTargets != nullptr) {
    if (const auto *typeMetadata = findSemanticProductTypeMetadata(*semanticProductTargets, def.fullPath);
        typeMetadata != nullptr) {
      return typeMetadata->category == "struct" || typeMetadata->category == "pod" ||
             typeMetadata->category == "handle" || typeMetadata->category == "gpu_lane";
    }
  }
  return isStructDefinition(def);
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
