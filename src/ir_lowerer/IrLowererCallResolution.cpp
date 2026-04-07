#include "IrLowererCallHelpers.h"

#include <optional>
#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"

namespace primec::ir_lowerer {

namespace {

bool isBridgeHelperName(std::string_view collectionFamily, std::string_view helperName) {
  if (collectionFamily == "vector") {
    return helperName == "count" || helperName == "capacity" || helperName == "at" ||
           helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
           helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
           helperName == "remove_swap";
  }
  if (collectionFamily == "map") {
    return helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
           helperName == "at" || helperName == "at_unsafe" || helperName == "insert" ||
           helperName == "mapCountRef" || helperName == "mapContainsRef" ||
           helperName == "mapTryAtRef" || helperName == "mapAtRef" ||
           helperName == "mapAtUnsafeRef" || helperName == "mapInsertRef";
  }
  if (collectionFamily == "soa_vector") {
    return helperName == "count" || helperName == "get" || helperName == "ref" ||
           helperName == "to_aos" || helperName == "push" || helperName == "reserve";
  }
  return false;
}

std::optional<std::pair<std::string, std::string>>
collectionBridgeChoiceFromResolvedPath(const std::string &resolvedPath) {
  auto parsePrefixedHelper = [&](std::string_view prefix,
                                 std::string_view collectionFamily)
      -> std::optional<std::pair<std::string, std::string>> {
    if (resolvedPath.rfind(prefix, 0) != 0) {
      return std::nullopt;
    }
    std::string helperName = resolvedPath.substr(prefix.size());
    const size_t specializationSuffix = helperName.find("__t");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
    if (!isBridgeHelperName(collectionFamily, helperName)) {
      return std::nullopt;
    }
    return std::pair<std::string, std::string>(std::string(collectionFamily), std::move(helperName));
  };

  if (auto parsed = parsePrefixedHelper("/vector/", "vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/vector/", "vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/experimental_vector/", "vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/map/", "map")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/map/", "map")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/experimental_map/", "map")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/soa_vector/", "soa_vector")) {
    return parsed;
  }
  if (auto parsed = parsePrefixedHelper("/std/collections/soa_vector/", "soa_vector")) {
    return parsed;
  }
  return std::nullopt;
}

bool isResolvedBridgeHelperPath(const std::string &resolvedPath) {
  return collectionBridgeChoiceFromResolvedPath(resolvedPath).has_value();
}

bool isMapBuiltinResolvedPath(const Expr &expr, const std::string &resolvedPath) {
  auto matchesResolvedPath = [&](std::string_view basePath) {
    return resolvedPath == basePath ||
           resolvedPath.rfind(std::string(basePath) + "__t", 0) == 0;
  };
  if (!expr.isMethodCall) {
    std::string aliasName;
    std::string accessName;
    if (getBuiltinArrayAccessName(expr, accessName) && expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/map/at") ||
             matchesResolvedPath("/std/collections/mapAt") ||
             matchesResolvedPath("/std/collections/map/at_unsafe") ||
             matchesResolvedPath("/std/collections/mapAtUnsafe");
    }
    if (resolveMapHelperAliasName(expr, aliasName)) {
      if (aliasName == "count" && expr.args.size() == 1) {
        return matchesResolvedPath("/std/collections/map/count") ||
               matchesResolvedPath("/std/collections/map/count_ref") ||
               matchesResolvedPath("/std/collections/mapCount") ||
               matchesResolvedPath("/std/collections/experimental_map/mapCount") ||
               matchesResolvedPath("/std/collections/experimental_map/mapCountRef");
      }
      if (aliasName == "contains" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/contains") ||
               matchesResolvedPath("/std/collections/map/contains_ref") ||
               matchesResolvedPath("/std/collections/mapContains") ||
               matchesResolvedPath("/std/collections/experimental_map/mapContains") ||
               matchesResolvedPath("/std/collections/experimental_map/mapContainsRef");
      }
      if (aliasName == "tryAt" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/tryAt") ||
               matchesResolvedPath("/std/collections/map/tryAt_ref") ||
               matchesResolvedPath("/std/collections/mapTryAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapTryAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapTryAtRef");
      }
      if (aliasName == "at" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/at") ||
               matchesResolvedPath("/std/collections/map/at_ref") ||
               matchesResolvedPath("/std/collections/mapAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAt") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAtRef");
      }
      if (aliasName == "at_unsafe" && expr.args.size() == 2) {
        return matchesResolvedPath("/std/collections/map/at_unsafe") ||
               matchesResolvedPath("/std/collections/map/at_unsafe_ref") ||
               matchesResolvedPath("/std/collections/mapAtUnsafe") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAtUnsafe") ||
               matchesResolvedPath("/std/collections/experimental_map/mapAtUnsafeRef");
      }
      if (aliasName == "insert" && expr.args.size() == 3) {
        return matchesResolvedPath("/std/collections/map/insert") ||
               matchesResolvedPath("/std/collections/map/insert_ref") ||
               matchesResolvedPath("/std/collections/mapInsert") ||
               matchesResolvedPath("/std/collections/experimental_map/mapInsert") ||
               matchesResolvedPath("/std/collections/experimental_map/mapInsertRef");
      }
    }
    std::string normalizedName = expr.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    if ((normalizedName == "contains" || normalizedName == "map/contains" ||
         normalizedName == "std/collections/map/contains") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/mapContains");
    }
    if ((normalizedName == "tryAt" || normalizedName == "map/tryAt" ||
         normalizedName == "std/collections/map/tryAt") &&
        expr.args.size() == 2) {
      return matchesResolvedPath("/std/collections/mapTryAt");
    }
    if ((normalizedName == "count" || normalizedName == "map/count" ||
         normalizedName == "std/collections/map/count") &&
        expr.args.size() == 1) {
      return matchesResolvedPath("/std/collections/map/count") ||
             matchesResolvedPath("/std/collections/mapCount");
    }
  }
  return false;
}

std::string resolveCallPathFromScopeWithoutImportAliases(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    scoped += "/" + expr.name;
    if (defMap.count(scoped) > 0) {
      return scoped;
    }
    const std::string root = "/" + expr.name;
    if (defMap.count(root) > 0) {
      return root;
    }
    return scoped;
  }
  return "/" + expr.name;
}

std::string describeCallSite(const std::string &scopePath, const Expr &expr) {
  std::string displayName;
  if (!expr.name.empty() && expr.name.front() == '/') {
    displayName = expr.name;
  } else if (!expr.namespacePrefix.empty()) {
    displayName = expr.namespacePrefix;
    if (!displayName.empty() && displayName.front() != '/') {
      displayName.insert(displayName.begin(), '/');
    }
    displayName += "/" + expr.name;
  } else if (!expr.name.empty()) {
    displayName = expr.name;
  } else {
    displayName = "<unnamed>";
  }
  return scopePath + " -> " + displayName;
}

} // namespace

const Definition *resolveDefinitionCall(const Expr &callExpr,
                                        const std::unordered_map<std::string, const Definition *> &defMap,
                                        const ResolveExprPathFn &resolveExprPath) {
  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall) {
    return nullptr;
  }
  const std::string resolved = resolveExprPath(callExpr);
  if (isMapBuiltinResolvedPath(callExpr, resolved)) {
    return nullptr;
  }
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
  return makeCallResolutionAdapters(defMap, importAliases, nullptr);
}

CallResolutionAdapters makeCallResolutionAdapters(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram) {
  CallResolutionAdapters adapters;
  adapters.semanticProductTargets = buildSemanticProductTargetAdapter(semanticProgram);
  adapters.resolveExprPath = makeResolveCallPathFromScope(
      defMap, importAliases, adapters.semanticProductTargets);
  adapters.isTailCallCandidate = makeIsTailCallCandidate(defMap, adapters.resolveExprPath);
  adapters.definitionExists = makeDefinitionExistsByPath(defMap);
  return adapters;
}

bool validateSemanticProductDirectCallCoverage(const Program &program,
                                               const SemanticProgram *semanticProgram,
                                               std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);
  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      if (expr.semanticNodeId == 0) {
        error = "missing semantic-product direct-call semantic id: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      if (findSemanticProductBridgePathChoice(semanticProductTargets, expr).empty() &&
          findSemanticProductDirectCallTarget(semanticProductTargets, expr).empty()) {
        error = "missing semantic-product direct-call target: " +
                describeCallSite(scopePath, expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.parameters) ||
        !validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateExprs(exec.fullPath, exec.arguments) ||
        !validateExprs(exec.fullPath, exec.bodyArguments)) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductBridgePathCoverage(const Program &program,
                                               const SemanticProgram *semanticProgram,
                                               std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);
  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      if (!findSemanticProductBridgePathChoice(semanticProductTargets, expr).empty()) {
        return validateExprs(scopePath, expr.args) &&
               validateExprs(scopePath, expr.bodyArguments);
      }
      if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProductTargets, expr);
          isResolvedBridgeHelperPath(resolvedPath)) {
        error = "missing semantic-product bridge-path choice: " +
                describeCallSite(scopePath, expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.parameters) ||
        !validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateExprs(exec.fullPath, exec.arguments) ||
        !validateExprs(exec.fullPath, exec.bodyArguments)) {
      return false;
    }
  }

  return true;
}

bool validateSemanticProductMethodCallCoverage(const Program &program,
                                               const SemanticProgram *semanticProgram,
                                               std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const SemanticProductTargetAdapter semanticProductTargets =
      buildSemanticProductTargetAdapter(semanticProgram);
  std::function<bool(const std::string &, const Expr &)> validateExpr;
  auto validateExprs = [&](const std::string &scopePath, const std::vector<Expr> &exprs) {
    for (const auto &expr : exprs) {
      if (!validateExpr(scopePath, expr)) {
        return false;
      }
    }
    return true;
  };

  validateExpr = [&](const std::string &scopePath, const Expr &expr) {
    if (expr.kind == Expr::Kind::Call && expr.isMethodCall) {
      if (expr.semanticNodeId == 0) {
        error = "missing semantic-product method-call semantic id: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      if (findSemanticProductMethodCallTarget(semanticProductTargets, expr).empty()) {
        error = "missing semantic-product method-call target: " +
                describeCallSite(scopePath, expr);
        return false;
      }
    }
    return validateExprs(scopePath, expr.args) &&
           validateExprs(scopePath, expr.bodyArguments);
  };

  for (const auto &def : program.definitions) {
    if (!validateExprs(def.fullPath, def.parameters) ||
        !validateExprs(def.fullPath, def.statements) ||
        (def.returnExpr.has_value() && !validateExpr(def.fullPath, *def.returnExpr))) {
      return false;
    }
  }

  for (const auto &exec : program.executions) {
    if (!validateExprs(exec.fullPath, exec.arguments) ||
        !validateExprs(exec.fullPath, exec.bodyArguments)) {
      return false;
    }
  }

  return true;
}

EntryCallResolutionSetup buildEntryCallResolutionSetup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return buildEntryCallResolutionSetup(entryDef,
                                       definitionReturnsVoid,
                                       defMap,
                                       importAliases,
                                       nullptr);
}

EntryCallResolutionSetup buildEntryCallResolutionSetup(
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram) {
  EntryCallResolutionSetup setup{};
  setup.adapters = makeCallResolutionAdapters(defMap, importAliases, semanticProgram);
  setup.hasTailExecution = hasTailExecutionCandidate(
      entryDef.statements, definitionReturnsVoid, setup.adapters.isTailCallCandidate);
  return setup;
}

ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases) {
  return makeResolveCallPathFromScope(defMap, importAliases, {});
}

ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProductTargetAdapter &semanticProductTargets) {
  static const std::unordered_map<std::string, std::string> noImportAliases;
  const auto &semanticAwareImportAliases =
      semanticProductTargets.hasSemanticProduct ? noImportAliases : importAliases;
  return [defMap, semanticProductTargets, semanticAwareImportAliases](const Expr &expr) {
    if (const std::string chosenPath = findSemanticProductBridgePathChoice(semanticProductTargets, expr);
        !chosenPath.empty()) {
      return chosenPath;
    }
    if (semanticProductTargets.hasSemanticProduct &&
        expr.kind == Expr::Kind::Call && !expr.isMethodCall) {
      if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProductTargets, expr);
          !resolvedPath.empty() && !isResolvedBridgeHelperPath(resolvedPath)) {
        if (resolveDefinitionByPath(defMap, resolvedPath) != nullptr ||
            expr.name.empty() || expr.name.front() != '/') {
          return resolvedPath;
        }
        return resolveCallPathFromScopeWithoutImportAliases(expr, defMap);
      }
      // Lowering sometimes rewrites validated operators/helpers into rooted direct
      // calls that do not have their own semantic-product direct-call fact.
      if (!expr.name.empty() && expr.name.front() == '/') {
        return resolveCallPathFromScopeWithoutImportAliases(expr, defMap);
      }
      return std::string{};
    }
    if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProductTargets, expr);
        !resolvedPath.empty()) {
      return resolvedPath;
    }
    if (semanticProductTargets.hasSemanticProduct) {
      return resolveCallPathFromScopeWithoutImportAliases(expr, defMap);
    }
    return resolveCallPathFromScope(expr, defMap, semanticAwareImportAliases);
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
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    scoped += "/" + expr.name;
    if (defMap.count(scoped) > 0) {
      return scoped;
    }
    auto importIt = importAliases.find(expr.name);
    if (importIt != importAliases.end()) {
      return normalizeMapImportAliasPath(importIt->second);
    }
    const std::string root = "/" + expr.name;
    if (defMap.count(root) > 0) {
      return root;
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

} // namespace primec::ir_lowerer
