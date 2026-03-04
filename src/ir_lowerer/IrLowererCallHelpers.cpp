#include "IrLowererCallHelpers.h"

#include <utility>

#include "IrLowererHelpers.h"

namespace primec::ir_lowerer {

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
      return importIt->second;
    }
    return scoped;
  }
  auto importIt = importAliases.find(expr.name);
  if (importIt != importAliases.end()) {
    return importIt->second;
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
      error);
  if (firstCountFallbackResult == CountMethodFallbackResult::Emitted) {
    return InlineCallDispatchResult::Emitted;
  }
  if (firstCountFallbackResult == CountMethodFallbackResult::Error) {
    return InlineCallDispatchResult::Error;
  }

  if (expr.isMethodCall && !isArrayCountCall(expr) && !isStringCountCall(expr) &&
      !isVectorCapacityCall(expr)) {
    const Definition *callee = resolveMethodCallDefinition(expr);
    const auto emitResult = emitResolvedInlineDefinitionCall(
        expr, callee, emitInlineDefinitionCall, error);
    if (emitResult == ResolvedInlineCallResult::NoCallee) {
      return InlineCallDispatchResult::Error;
    }
    return emitResult == ResolvedInlineCallResult::Emitted
               ? InlineCallDispatchResult::Emitted
               : InlineCallDispatchResult::Error;
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
      error);
  if (secondCountFallbackResult == CountMethodFallbackResult::Emitted) {
    return InlineCallDispatchResult::Emitted;
  }
  if (secondCountFallbackResult == CountMethodFallbackResult::Error) {
    return InlineCallDispatchResult::Error;
  }

  return InlineCallDispatchResult::NotHandled;
}

bool getUnsupportedVectorHelperName(const Expr &expr, std::string &helperName) {
  if (isSimpleCallName(expr, "push")) {
    helperName = "push";
    return true;
  }
  if (isSimpleCallName(expr, "pop")) {
    helperName = "pop";
    return true;
  }
  if (isSimpleCallName(expr, "reserve")) {
    helperName = "reserve";
    return true;
  }
  if (isSimpleCallName(expr, "clear")) {
    helperName = "clear";
    return true;
  }
  if (isSimpleCallName(expr, "remove_at")) {
    helperName = "remove_at";
    return true;
  }
  if (isSimpleCallName(expr, "remove_swap")) {
    helperName = "remove_swap";
    return true;
  }
  return false;
}

CountMethodFallbackResult tryEmitNonMethodCountFallback(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error) {
  if (expr.isMethodCall || !isSimpleCallName(expr, "count") || expr.args.size() != 1 ||
      isArrayCountCall(expr) || isStringCountCall(expr)) {
    return CountMethodFallbackResult::NotHandled;
  }

  Expr methodExpr = expr;
  methodExpr.isMethodCall = true;
  const Definition *callee = resolveMethodCallDefinition(methodExpr);
  const auto emitResult = emitResolvedInlineDefinitionCall(
      methodExpr, callee, emitInlineDefinitionCall, error);
  if (emitResult == ResolvedInlineCallResult::NoCallee) {
    return CountMethodFallbackResult::NoCallee;
  }
  if (emitResult == ResolvedInlineCallResult::Error) {
    return CountMethodFallbackResult::Error;
  }
  return CountMethodFallbackResult::Emitted;
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
    return false;
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
                                     std::vector<Expr> &paramsOut,
                                     std::vector<const Expr *> &orderedArgsOut,
                                     std::string &error) {
  if (!buildInlineCallParameterList(callee, structNames, paramsOut, error)) {
    return false;
  }
  return buildOrderedCallArguments(callExpr, paramsOut, orderedArgsOut, error);
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
