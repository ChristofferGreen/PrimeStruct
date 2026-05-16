#include "IrLowererCallHelpers.h"

#include <optional>
#include <string_view>

#include "IrLowererHelpers.h"
#include "IrLowererSetupTypeCollectionHelpers.h"
#include "primec/SoaPathHelpers.h"

namespace primec::ir_lowerer {

namespace {

std::string resolveCallPathWithoutSemanticFallbackProbes(const Expr &expr);

std::string stdCollectionsRoot() {
  return "/std/collections";
}

std::string experimentalCollectionMemberRoot(std::string_view collectionName) {
  return stdCollectionsRoot() + "/experimental_" + std::string(collectionName) + "/";
}

std::string experimentalCollectionMemberPath(std::string_view collectionName,
                                             std::string_view memberName) {
  return experimentalCollectionMemberRoot(collectionName) + std::string(memberName);
}

std::string experimentalCollectionSpecializedTypePrefix(std::string_view collectionName,
                                                        std::string_view typeName) {
  return experimentalCollectionMemberPath(collectionName, typeName) + "__";
}

bool resolvesToPublishedDefinitionFamilyTarget(const SemanticProgram *semanticProgram,
                                               const std::string &resolvedPath) {
  if (semanticProgram == nullptr || resolvedPath.empty()) {
    return false;
  }
  const std::string templatedPrefix = resolvedPath + "<";
  const std::string specializedPrefix = resolvedPath + "__t";
  const std::string overloadPrefix = resolvedPath + "__ov";
  auto asStringView = [](const std::string &value) {
    return std::string_view(value.data(), value.size());
  };
  auto matchesResolvedFamily = [&](std::string_view publishedPath) {
    return publishedPath == asStringView(resolvedPath) ||
           publishedPath.starts_with(asStringView(templatedPrefix)) ||
           publishedPath.starts_with(asStringView(specializedPrefix)) ||
           publishedPath.starts_with(asStringView(overloadPrefix));
  };
  for (const auto &[pathId, definitionIndex] :
       semanticProgram->publishedRoutingLookups.definitionIndicesByPathId) {
    if (definitionIndex >= semanticProgram->definitions.size()) {
      continue;
    }
    const auto &entry = semanticProgram->definitions[definitionIndex];
    if (matchesResolvedFamily(asStringView(entry.fullPath))) {
      return true;
    }
    const std::string_view publishedPath =
        semanticProgramResolveCallTargetString(*semanticProgram, pathId);
    if (!publishedPath.empty() && matchesResolvedFamily(publishedPath)) {
      return true;
    }
  }
  return false;
}

std::string resolveCallPathFromPublishedLookups(const Expr &expr,
                                                const SemanticProgram *semanticProgram) {
  if (semanticProgram == nullptr) {
    return resolveCallPathWithoutSemanticFallbackProbes(expr);
  }
  if (!expr.name.empty() && expr.name.front() == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    scoped += "/" + expr.name;
    if (semanticProgramLookupPublishedDefinition(*semanticProgram, scoped) != nullptr) {
      return scoped;
    }
    if (const auto importAliasTargetPathId =
            semanticProgramLookupPublishedImportAliasTargetPathId(*semanticProgram, expr.name);
        importAliasTargetPathId.has_value()) {
      return normalizeMapImportAliasPath(
          std::string(semanticProgramResolveCallTargetString(*semanticProgram,
                                                             *importAliasTargetPathId)));
    }
    const std::string root = "/" + expr.name;
    if (semanticProgramLookupPublishedDefinition(*semanticProgram, root) != nullptr) {
      return root;
    }
    return scoped;
  }
  if (const auto importAliasTargetPathId =
          semanticProgramLookupPublishedImportAliasTargetPathId(*semanticProgram, expr.name);
      importAliasTargetPathId.has_value()) {
    return normalizeMapImportAliasPath(
        std::string(
            semanticProgramResolveCallTargetString(*semanticProgram, *importAliasTargetPathId)));
  }
  return "/" + expr.name;
}

bool isResidualSoaBridgeHelperName(std::string_view helperName) {
  const std::string toAosHelper = std::string("to") + "_aos";
  return helperName == "count" || helperName == "get" || helperName == "ref" ||
         helperName == toAosHelper || helperName == "push" ||
         helperName == "reserve";
}

bool isPublishedCollectionBridgeStdlibSurfaceId(std::optional<StdlibSurfaceId> surfaceId) {
  if (!surfaceId.has_value()) {
    return false;
  }
  const auto *vectorMetadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.vector_helpers");
  const auto *mapMetadata =
      findStdlibSurfaceMetadataByBridgeKey("collections.map_helpers");
  return (vectorMetadata != nullptr && *surfaceId == vectorMetadata->id) ||
         (mapMetadata != nullptr && *surfaceId == mapMetadata->id) ||
         *surfaceId == StdlibSurfaceId::CollectionsColumnarHelpers;
}

bool isPublishedCollectionBridgeCall(const SemanticProgram *semanticProgram, const Expr &expr) {
  return isPublishedCollectionBridgeStdlibSurfaceId(
             findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)) ||
         isPublishedCollectionBridgeStdlibSurfaceId(
             findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr));
}

std::optional<std::string>
residualBridgeChoiceFromResolvedPath(const std::string &resolvedPath) {
  auto parsePrefixedHelper = [&](std::string_view prefix)
      -> std::optional<std::string> {
    if (resolvedPath.rfind(prefix, 0) != 0) {
      return std::nullopt;
    }
    std::string helperName = resolvedPath.substr(prefix.size());
    const size_t specializationSuffix = helperName.find("__t");
    if (specializationSuffix != std::string::npos) {
      helperName.erase(specializationSuffix);
    }
    if (!isResidualSoaBridgeHelperName(helperName)) {
      return std::nullopt;
    }
    return helperName;
  };

  const std::string residualSoaPrefix =
      soa_paths::collectionPath(soa_paths::legacySoaFolder()) + "/";
  if (auto parsed = parsePrefixedHelper(residualSoaPrefix)) {
    return parsed;
  }
  return std::nullopt;
}

bool isResidualBridgeHelperPath(const std::string &resolvedPath) {
  return residualBridgeChoiceFromResolvedPath(resolvedPath).has_value();
}

bool isBridgeHelperCall(const SemanticProgram *semanticProgram,
                        const Expr &expr,
                        const std::string &resolvedPath) {
  return isPublishedCollectionBridgeCall(semanticProgram, expr) ||
         isResidualBridgeHelperPath(resolvedPath);
}

const Definition *preferExplicitExperimentalVectorHelperDefinition(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, const Definition *> &defMap) {
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall ||
      resolvedPath.rfind(experimentalCollectionSpecializedTypePrefix("vector", "Vector"), 0) != 0) {
    return nullptr;
  }
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);
  if (rawPath != experimentalCollectionMemberPath("vector", "vector")) {
    return nullptr;
  }
  if (const Definition *rawDef = resolveDefinitionByPath(defMap, rawPath);
      rawDef != nullptr) {
    return rawDef;
  }
  const std::string specializedPrefix = rawPath + "__t";
  const std::string overloadPrefix = rawPath + "__ov";
  for (const auto &[path, def] : defMap) {
    if (def == nullptr) {
      continue;
    }
    if (path.rfind(specializedPrefix, 0) == 0 ||
        path.rfind(overloadPrefix, 0) == 0) {
      return def;
    }
  }
  return nullptr;
}

std::string resolveCallPathWithoutSemanticFallbackProbes(const Expr &expr) {
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix;
    if (!scoped.empty() && scoped.front() != '/') {
      scoped.insert(scoped.begin(), '/');
    }
    return scoped + "/" + expr.name;
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
                                        const ResolveExprPathFn &resolveExprPath,
                                        const SemanticProgram *semanticProgram) {
  (void)semanticProgram;
  if (callExpr.kind != Expr::Kind::Call || callExpr.isBinding || callExpr.isMethodCall ||
      !resolveExprPath) {
    return nullptr;
  }
  const std::string resolved = resolveExprPath(callExpr);
  const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(callExpr);
  if (const Definition *explicitExperimentalVectorHelperDef =
          preferExplicitExperimentalVectorHelperDefinition(callExpr, resolved, defMap);
      explicitExperimentalVectorHelperDef != nullptr) {
    return explicitExperimentalVectorHelperDef;
  }
  if (const Definition *resolvedDef = resolveDefinitionByPath(defMap, resolved);
      resolvedDef != nullptr) {
    if (isStructDefinition(*resolvedDef) &&
        !isStructConstructorCallShape(callExpr)) {
      return nullptr;
    }
    return resolvedDef;
  }
  const bool hasSemanticRootedRewrite =
      callExpr.semanticNodeId != 0 &&
      !rawPath.empty() &&
      !resolved.empty() &&
      rawPath.front() == '/' &&
      resolved.front() == '/' &&
      rawPath != resolved;
  const size_t rawLeafStart = rawPath.find_last_of('/');
  const bool hasGeneratedRootedRawPath =
      !rawPath.empty() &&
      rawPath.front() == '/' &&
      (rawPath.find("__t", rawLeafStart == std::string::npos ? 0 : rawLeafStart + 1) !=
           std::string::npos ||
       rawPath.find("__ov", rawLeafStart == std::string::npos ? 0 : rawLeafStart + 1) !=
           std::string::npos);
  if (rawPath != resolved &&
      (!hasSemanticRootedRewrite || hasGeneratedRootedRawPath)) {
    if (const Definition *rawDef = resolveDefinitionByPath(defMap, rawPath);
        rawDef != nullptr) {
      if (isStructDefinition(*rawDef) &&
          !isStructConstructorCallShape(callExpr)) {
        return nullptr;
      }
      return rawDef;
    }
  }
  return nullptr;
}

ResolveDefinitionCallFn makeResolveDefinitionCall(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveExprPathFn &resolveExprPath,
    const SemanticProgram *semanticProgram) {
  return [defMap, resolveExprPath, semanticProgram](const Expr &expr) {
    return resolveDefinitionCall(expr, defMap, resolveExprPath, semanticProgram);
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
  adapters.semanticProgram = semanticProgram;
  adapters.semanticProductTargets = buildSemanticProductTargetAdapter(semanticProgram);
  adapters.resolveExprPath = makeResolveCallPathFromScope(
      defMap, importAliases, adapters.semanticProgram);
  adapters.isTailCallCandidate = makeIsTailCallCandidate(defMap, adapters.resolveExprPath);
  adapters.definitionExists = makeDefinitionExistsByPath(defMap);
  return adapters;
}

bool validateSemanticProductDirectCallCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const auto directCallTargets = semanticProgramDirectCallTargetView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramDirectCallTarget *> directCallTargetsByExpr;
  directCallTargetsByExpr.reserve(directCallTargets.size());
  for (const auto *entry : directCallTargets) {
    if (entry->semanticNodeId != 0) {
      directCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
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
      const auto publishedDirectTargetId =
          semanticProgramLookupPublishedDirectCallTargetId(*semanticProgram,
                                                           expr.semanticNodeId);
      if (publishedDirectTargetId.has_value()) {
        const auto it = directCallTargetsByExpr.find(expr.semanticNodeId);
        if (it == directCallTargetsByExpr.end()) {
          error = "missing semantic-product direct-call target: " +
                  describeCallSite(scopePath, expr);
          return false;
        }
        const auto &entry = *it->second;
        const std::string siteDescription = describeCallSite(scopePath, expr);
        const std::string_view resolvedPath =
            semanticProgramDirectCallTargetResolvedPath(*semanticProgram, entry);
        if (entry.resolvedPathId == InvalidSymbolId || resolvedPath.empty()) {
          error = "missing semantic-product direct-call resolved path id: " +
                  siteDescription;
          return false;
        }
        if (entry.scopePathId != InvalidSymbolId) {
          const std::string_view resolvedScope =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.scopePathId);
          if (resolvedScope.empty()) {
            error = "missing semantic-product direct-call scope id: " +
                    siteDescription;
            return false;
          }
          if (resolvedScope != entry.scopePath) {
            error = "stale semantic-product direct-call scope metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.callNameId != InvalidSymbolId) {
          const std::string_view resolvedCallName =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.callNameId);
          if (resolvedCallName.empty()) {
            error = "missing semantic-product direct-call name id: " +
                    siteDescription;
            return false;
          }
          if (resolvedCallName != entry.callName) {
            error = "stale semantic-product direct-call name metadata: " +
                    siteDescription;
            return false;
          }
        }
        const std::string_view publishedTarget =
            semanticProgramResolveCallTargetString(*semanticProgram, *publishedDirectTargetId);
        if (publishedTarget.empty()) {
          error = "missing semantic-product direct-call resolved path id: " +
                  siteDescription;
          return false;
        }
        if (publishedTarget != resolvedPath) {
          error = "stale semantic-product direct-call target metadata: " +
                  siteDescription;
          return false;
        }
      }
      if (findSemanticProductBridgePathChoice(semanticProgram, expr).empty() &&
          findSemanticProductDirectCallTarget(semanticProgram, expr).empty()) {
        const std::string resolvedPath = resolveCallPathFromPublishedLookups(expr, semanticProgram);
        if (!isBridgeHelperCall(semanticProgram, expr, resolvedPath) &&
            !resolvesToPublishedDefinitionFamilyTarget(semanticProgram, resolvedPath)) {
          return validateExprs(scopePath, expr.args) &&
                 validateExprs(scopePath, expr.bodyArguments);
        }
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

bool validateSemanticProductBridgePathCoverage(
    const Program &program,
    const SemanticProgram *semanticProgram,
    std::string &error) {
  if (semanticProgram == nullptr) {
    return true;
  }

  const auto bridgePathChoices = semanticProgramBridgePathChoiceView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramBridgePathChoice *> bridgePathChoicesByExpr;
  bridgePathChoicesByExpr.reserve(bridgePathChoices.size());
  for (const auto *entry : bridgePathChoices) {
    if (entry->semanticNodeId != 0) {
      bridgePathChoicesByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
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
      const auto publishedChoiceId =
          semanticProgramLookupPublishedBridgePathChoiceId(*semanticProgram,
                                                           expr.semanticNodeId);
      if (publishedChoiceId.has_value()) {
        const auto it = bridgePathChoicesByExpr.find(expr.semanticNodeId);
        if (it == bridgePathChoicesByExpr.end()) {
          error = "missing semantic-product bridge-path choice: " +
                  describeCallSite(scopePath, expr);
          return false;
        }
        const auto &entry = *it->second;
        const std::string siteDescription = describeCallSite(scopePath, expr);
        const std::string_view helperName =
            semanticProgramBridgePathChoiceHelperName(*semanticProgram, entry);
        if (entry.helperNameId == InvalidSymbolId || helperName.empty()) {
          error = "missing semantic-product bridge helper name id: " +
                  siteDescription;
          return false;
        }
        const std::string_view chosenPath =
            semanticProgramResolveCallTargetString(*semanticProgram, entry.chosenPathId);
        if (entry.chosenPathId == InvalidSymbolId || chosenPath.empty()) {
          error = "missing semantic-product bridge chosen path id: " +
                  siteDescription;
          return false;
        }
        if (entry.scopePathId != InvalidSymbolId) {
          const std::string_view resolvedScope =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.scopePathId);
          if (resolvedScope.empty()) {
            error = "missing semantic-product bridge scope id: " +
                    siteDescription;
            return false;
          }
          if (resolvedScope != entry.scopePath) {
            error = "stale semantic-product bridge scope metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.collectionFamilyId != InvalidSymbolId) {
          const std::string_view resolvedCollectionFamily =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.collectionFamilyId);
          if (resolvedCollectionFamily.empty()) {
            error = "missing semantic-product bridge collection family id: " +
                    siteDescription;
            return false;
          }
          if (resolvedCollectionFamily != entry.collectionFamily) {
            error = "stale semantic-product bridge collection family metadata: " +
                    siteDescription;
            return false;
          }
        }
        const std::string_view publishedChoice =
            semanticProgramResolveCallTargetString(*semanticProgram, *publishedChoiceId);
        if (publishedChoice.empty()) {
          error = "missing semantic-product bridge chosen path id: " +
                  siteDescription;
          return false;
        }
        if (publishedChoice != chosenPath) {
          error = "stale semantic-product bridge target metadata: " +
                  siteDescription;
          return false;
        }
      }
      if (!findSemanticProductBridgePathChoice(semanticProgram, expr).empty()) {
        return validateExprs(scopePath, expr.args) &&
               validateExprs(scopePath, expr.bodyArguments);
      }
      if (isPublishedCollectionBridgeStdlibSurfaceId(
              findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr))) {
        error = "missing semantic-product bridge-path choice: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProgram, expr);
          isResidualBridgeHelperPath(resolvedPath)) {
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

  const auto methodCallTargets = semanticProgramMethodCallTargetView(*semanticProgram);
  std::unordered_map<uint64_t, const SemanticProgramMethodCallTarget *> methodCallTargetsByExpr;
  methodCallTargetsByExpr.reserve(methodCallTargets.size());
  for (const auto *entry : methodCallTargets) {
    if (entry->semanticNodeId != 0) {
      methodCallTargetsByExpr.insert_or_assign(entry->semanticNodeId, entry);
    }
  }
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
    if (expr.kind == Expr::Kind::Call && expr.isMethodCall &&
        !expr.isFieldAccess) {
      if (expr.semanticNodeId == 0) {
        error = "missing semantic-product method-call semantic id: " +
                describeCallSite(scopePath, expr);
        return false;
      }
      const auto publishedMethodTargetId =
          semanticProgramLookupPublishedMethodCallTargetId(*semanticProgram,
                                                           expr.semanticNodeId);
      if (publishedMethodTargetId.has_value()) {
        const auto it = methodCallTargetsByExpr.find(expr.semanticNodeId);
        if (it == methodCallTargetsByExpr.end()) {
          error = "missing semantic-product method-call target: " +
                  describeCallSite(scopePath, expr);
          return false;
        }
        const auto &entry = *it->second;
        const std::string siteDescription = describeCallSite(scopePath, expr);
        const std::string_view resolvedPath =
            semanticProgramMethodCallTargetResolvedPath(*semanticProgram, entry);
        if (entry.resolvedPathId == InvalidSymbolId || resolvedPath.empty()) {
          error = "missing semantic-product method-call resolved path id: " +
                  siteDescription;
          return false;
        }
        if (entry.scopePathId != InvalidSymbolId) {
          const std::string_view resolvedScope =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.scopePathId);
          if (resolvedScope.empty()) {
            error = "missing semantic-product method-call scope id: " +
                    siteDescription;
            return false;
          }
          if (resolvedScope != entry.scopePath) {
            error = "stale semantic-product method-call scope metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.methodNameId != InvalidSymbolId) {
          const std::string_view resolvedMethodName =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.methodNameId);
          if (resolvedMethodName.empty()) {
            error = "missing semantic-product method-call name id: " +
                    siteDescription;
            return false;
          }
          if (resolvedMethodName != entry.methodName) {
            error = "stale semantic-product method-call name metadata: " +
                    siteDescription;
            return false;
          }
        }
        if (entry.receiverTypeTextId != InvalidSymbolId) {
          const std::string_view resolvedReceiver =
              semanticProgramResolveCallTargetString(*semanticProgram, entry.receiverTypeTextId);
          if (resolvedReceiver.empty()) {
            error = "missing semantic-product method-call receiver type id: " +
                    siteDescription;
            return false;
          }
          if (resolvedReceiver != entry.receiverTypeText) {
            error = "stale semantic-product method-call receiver metadata: " +
                    siteDescription;
            return false;
          }
        }
        const std::string_view publishedTarget =
            semanticProgramResolveCallTargetString(*semanticProgram, *publishedMethodTargetId);
        if (publishedTarget.empty()) {
          error = "missing semantic-product method-call resolved path id: " +
                  siteDescription;
          return false;
        }
        if (publishedTarget != resolvedPath) {
          error = "stale semantic-product method-call target metadata: " +
                  siteDescription;
          return false;
        }
      }
      if (findSemanticProductMethodCallTarget(semanticProgram, expr).empty()) {
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
  return makeResolveCallPathFromScope(defMap, importAliases, nullptr);
}

ResolveExprPathFn makeResolveCallPathFromScope(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const SemanticProgram *semanticProgram) {
  return [defMap, importAliases, semanticProgram](const Expr &expr) {
    if (!expr.isMethodCall) {
      if (const std::string chosenPath =
              findSemanticProductBridgePathChoice(semanticProgram, expr);
          !chosenPath.empty()) {
        return chosenPath;
      }
    }
    if (semanticProgram != nullptr && expr.kind == Expr::Kind::Call) {
      if (expr.isMethodCall) {
        if (const std::string resolvedPath =
                findSemanticProductMethodCallTarget(semanticProgram, expr);
            !resolvedPath.empty()) {
          return resolvedPath;
        }
        return resolveCallPathWithoutSemanticFallbackProbes(expr);
      }
      if (const std::string resolvedPath =
              findSemanticProductDirectCallTarget(semanticProgram, expr);
          !resolvedPath.empty() &&
          !isPublishedCollectionBridgeStdlibSurfaceId(
              findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)) &&
          !isResidualBridgeHelperPath(resolvedPath)) {
        return resolvedPath;
      }
      return resolveCallPathWithoutSemanticFallbackProbes(expr);
    }
    if (const std::string resolvedPath = findSemanticProductDirectCallTarget(semanticProgram, expr);
        !resolvedPath.empty()) {
      return resolvedPath;
    }
    if (semanticProgram != nullptr) {
      return resolveCallPathWithoutSemanticFallbackProbes(expr);
    }
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
  if (expr.kind != Expr::Kind::Call || expr.isMethodCall || !resolveExprPath) {
    return false;
  }
  const std::string targetPath = resolveExprPath(expr);
  return resolveDefinitionByPath(defMap, targetPath) != nullptr;
}

bool hasTailExecutionCandidate(const std::vector<Expr> &statements,
                               bool definitionReturnsVoid,
                               const IsTailCallCandidateFn &isTailCallCandidateFn) {
  if (statements.empty() || !isTailCallCandidateFn) {
    return false;
  }
  const Expr &lastStmt = statements.back();
  if (isReturnCall(lastStmt) && lastStmt.args.size() == 1) {
    return isTailCallCandidateFn(lastStmt.args.front());
  }
  return definitionReturnsVoid && isTailCallCandidateFn(lastStmt);
}

} // namespace primec::ir_lowerer
