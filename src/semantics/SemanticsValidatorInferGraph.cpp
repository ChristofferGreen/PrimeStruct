#include "SemanticsValidator.h"

#include "CondensationDag.h"
#include "TypeResolutionGraph.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory_resource>
#include <sstream>

namespace primec::semantics {

namespace {

std::string formatReturnInferenceCycleDiagnostic(const std::vector<const Definition *> &definitions) {
  std::ostringstream message;
  if (definitions.size() == 1) {
    message << "return type inference cycle requires explicit annotation on "
            << definitions.front()->fullPath;
    return message.str();
  }

  message << "return type inference cycle requires explicit annotations on ";
  for (size_t index = 0; index < definitions.size(); ++index) {
    if (index != 0) {
      message << ", ";
    }
    message << definitions[index]->fullPath;
  }
  return message.str();
}

std::vector<std::vector<uint32_t>> buildCondensationDagLayers(const CondensationDag &dag) {
  std::vector<uint32_t> layerByComponent(dag.nodes.size(), 0);
  uint32_t maxLayer = 0;
  for (uint32_t componentId : dag.topologicalComponentIds) {
    if (componentId >= dag.nodes.size()) {
      continue;
    }
    uint32_t layer = 0;
    for (uint32_t incoming : dag.nodes[componentId].incomingComponentIds) {
      if (incoming >= layerByComponent.size()) {
        continue;
      }
      layer = std::max(layer, static_cast<uint32_t>(layerByComponent[incoming] + 1));
    }
    layerByComponent[componentId] = layer;
    maxLayer = std::max(maxLayer, layer);
  }
  std::vector<std::vector<uint32_t>> layers;
  layers.resize(maxLayer + 1);
  for (uint32_t componentId : dag.topologicalComponentIds) {
    if (componentId >= layerByComponent.size()) {
      continue;
    }
    layers[layerByComponent[componentId]].push_back(componentId);
  }
  for (auto &layer : layers) {
    std::sort(layer.begin(), layer.end());
  }
  return layers;
}

bool useForwardGraphLayerOrder() {
  const char *value = std::getenv("PRIMESTRUCT_GRAPH_LAYER_ORDER");
  if (value == nullptr || *value == '\0') {
    return false;
  }
  return std::string_view(value) == "forward";
}

} // namespace

bool SemanticsValidator::inferUnknownReturnKinds() {
  return inferUnknownReturnKindsGraph();
}

bool SemanticsValidator::inferUnknownReturnKindsGraph() {
  graphLocalAutoScopePathInterner_.clear();
  if (graphLocalAutoBenchmarkShadow_ != nullptr) {
    graphLocalAutoBenchmarkShadow_->clear();
  }
  graphLocalAutoFacts_.clear();
  const TypeResolutionGraph graph = buildTypeResolutionGraph(program_);
  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);

  auto collectUnknownDefinitionNodes = [&](const CondensationDagNode &componentNode) {
    std::vector<const TypeResolutionGraphNode *> unresolvedNodes;
    unresolvedNodes.reserve(componentNode.memberNodeIds.size());
    for (uint32_t nodeId : componentNode.memberNodeIds) {
      if (nodeId >= graph.nodes.size()) {
        continue;
      }
      const TypeResolutionGraphNode &node = graph.nodes[nodeId];
      if (node.kind != TypeResolutionNodeKind::DefinitionReturn) {
        continue;
      }
      auto kindIt = returnKinds_.find(node.resolvedPath);
      if (kindIt == returnKinds_.end() || kindIt->second != ReturnKind::Unknown) {
        continue;
      }
      auto defIt = defMap_.find(node.resolvedPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        continue;
      }
      unresolvedNodes.push_back(&node);
    }
    sortTypeResolutionNodesForDiagnosticOrder(unresolvedNodes);
    return unresolvedNodes;
  };
  auto collectUnknownDefinitions = [&](const CondensationDagNode &componentNode) {
    std::vector<const Definition *> definitions;
    const std::vector<const TypeResolutionGraphNode *> unresolvedNodes =
        collectUnknownDefinitionNodes(componentNode);
    definitions.reserve(unresolvedNodes.size());
    for (const TypeResolutionGraphNode *node : unresolvedNodes) {
      auto defIt = defMap_.find(node->resolvedPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        continue;
      }
      definitions.push_back(defIt->second);
    }
    return definitions;
  };
  auto failInferGraphCycleDiagnostic =
      [&](const std::vector<const TypeResolutionGraphNode *> &unresolvedNodes) -> bool {
    std::vector<const Definition *> unresolvedDefinitions;
    unresolvedDefinitions.reserve(unresolvedNodes.size());
    for (const TypeResolutionGraphNode *node : unresolvedNodes) {
      auto defIt = defMap_.find(node->resolvedPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        continue;
      }
      unresolvedDefinitions.push_back(defIt->second);
    }
    const std::string message =
        formatReturnInferenceCycleDiagnostic(unresolvedDefinitions);
    if (shouldCollectStructuredDiagnostics()) {
      DiagnosticSinkRecord record;
      record.message = message;
      const Definition *primary = unresolvedDefinitions.front();
      if (primary->sourceLine > 0 && primary->sourceColumn > 0) {
        record.primarySpan.line = primary->sourceLine;
        record.primarySpan.column = primary->sourceColumn;
        record.primarySpan.endLine = primary->sourceLine;
        record.primarySpan.endColumn = primary->sourceColumn;
        record.hasPrimarySpan = true;
      }
      for (size_t index = 0; index < unresolvedDefinitions.size(); ++index) {
        const Definition *definition = unresolvedDefinitions[index];
        if (definition->sourceLine <= 0 || definition->sourceColumn <= 0) {
          continue;
        }
        DiagnosticRelatedSpan span;
        span.span.line = definition->sourceLine;
        span.span.column = definition->sourceColumn;
        span.span.endLine = definition->sourceLine;
        span.span.endColumn = definition->sourceColumn;
        span.label = "cycle member: " + unresolvedNodes[index]->resolvedPath;
        record.relatedSpans.push_back(std::move(span));
      }
      resultSink_.setRecords({std::move(record)});
      rememberFirstCollectedDiagnosticMessage(message);
      return false;
    }
    return failUncontextualizedDiagnostic(message);
  };

  const std::vector<std::vector<uint32_t>> dagLayers = buildCondensationDagLayers(dag);
  const bool forwardOrder = useForwardGraphLayerOrder();
  if (forwardOrder) {
    for (const auto &layer : dagLayers) {
      for (uint32_t componentId : layer) {
        if (componentId >= dag.nodes.size()) {
          continue;
        }
        const CondensationDagNode &componentNode = dag.nodes[componentId];
        const bool componentHasCycle = componentNode.memberNodeIds.size() > 1;
        std::vector<const Definition *> definitions = collectUnknownDefinitions(componentNode);
        if (definitions.empty()) {
          continue;
        }

        bool changed = false;
        do {
          changed = false;
          for (const Definition *definition : definitions) {
            bool definitionChanged = false;
            if (!inferDefinitionReturnKindGraphStep(
                    *definition, false, componentHasCycle, definitionChanged)) {
              return false;
            }
            changed = changed || definitionChanged;
          }
        } while (changed);

        std::vector<const TypeResolutionGraphNode *> unresolvedNodes =
            collectUnknownDefinitionNodes(componentNode);
        std::vector<const Definition *> unresolvedDefinitions;
        unresolvedDefinitions.reserve(unresolvedNodes.size());
        for (const TypeResolutionGraphNode *node : unresolvedNodes) {
          auto defIt = defMap_.find(node->resolvedPath);
          if (defIt == defMap_.end() || defIt->second == nullptr) {
            continue;
          }
          unresolvedDefinitions.push_back(defIt->second);
        }
        if (unresolvedDefinitions.empty()) {
          continue;
        }

        if (componentHasCycle) {
          return failInferGraphCycleDiagnostic(unresolvedNodes);
        }

        for (const Definition *definition : unresolvedDefinitions) {
          bool definitionChanged = false;
          if (!inferDefinitionReturnKindGraphStep(*definition, true, componentHasCycle, definitionChanged)) {
            return false;
          }
        }
      }
    }
  } else {
    for (auto layerIt = dagLayers.rbegin(); layerIt != dagLayers.rend(); ++layerIt) {
      for (uint32_t componentId : *layerIt) {
      if (componentId >= dag.nodes.size()) {
        continue;
      }
      const CondensationDagNode &componentNode = dag.nodes[componentId];
      const bool componentHasCycle = componentNode.memberNodeIds.size() > 1;
      std::vector<const Definition *> definitions = collectUnknownDefinitions(componentNode);
      if (definitions.empty()) {
        continue;
      }

      bool changed = false;
      do {
        changed = false;
        for (const Definition *definition : definitions) {
          bool definitionChanged = false;
          if (!inferDefinitionReturnKindGraphStep(
                  *definition, false, componentHasCycle, definitionChanged)) {
            return false;
          }
          changed = changed || definitionChanged;
        }
      } while (changed);

      std::vector<const TypeResolutionGraphNode *> unresolvedNodes =
          collectUnknownDefinitionNodes(componentNode);
      std::vector<const Definition *> unresolvedDefinitions;
      unresolvedDefinitions.reserve(unresolvedNodes.size());
      for (const TypeResolutionGraphNode *node : unresolvedNodes) {
        auto defIt = defMap_.find(node->resolvedPath);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          continue;
        }
        unresolvedDefinitions.push_back(defIt->second);
      }
      if (unresolvedDefinitions.empty()) {
        continue;
      }

      if (componentHasCycle) {
        return failInferGraphCycleDiagnostic(unresolvedNodes);
      }

      for (const Definition *definition : unresolvedDefinitions) {
        bool definitionChanged = false;
        if (!inferDefinitionReturnKindGraphStep(*definition, true, componentHasCycle, definitionChanged)) {
          return false;
        }
      }
    }
  }
  }

  for (const auto &def : program_.definitions) {
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt == returnKinds_.end() || kindIt->second != ReturnKind::Unknown) {
      continue;
    }
    bool hasReturnTransform = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1) {
        hasReturnTransform = true;
        break;
      }
    }
    if (hasReturnTransform || def.returnExpr.has_value()) {
      continue;
    }
    bool hasNonBindingStatement = false;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        hasNonBindingStatement = true;
        break;
      }
    }
    if (!hasNonBindingStatement) {
      kindIt->second = ReturnKind::Void;
    }
  }

  collectGraphLocalAutoBindings(graph);
  return true;
}

void SemanticsValidator::collectGraphLocalAutoBindings(const TypeResolutionGraph &graph) {
  struct GraphLocalAutoDependencyScratch {
    using PmrDependencyCountMap = std::pmr::unordered_map<GraphLocalAutoKey, size_t, GraphLocalAutoKeyHash>;

    std::array<std::byte, 16384> inlineArenaBuffer{};
    std::pmr::monotonic_buffer_resource arenaResource{
        inlineArenaBuffer.data(),
        inlineArenaBuffer.size(),
        std::pmr::new_delete_resource()};
    PmrDependencyCountMap dependencyCountByBindingKey{&arenaResource};
  };

  using StdDependencyCountMap = std::unordered_map<GraphLocalAutoKey, size_t, GraphLocalAutoKeyHash>;
  StdDependencyCountMap stdDependencyCountByBindingKey;
  GraphLocalAutoDependencyScratch dependencyScratch;
  auto &dependencyCountByBindingKey = dependencyScratch.dependencyCountByBindingKey;
  const bool usePmrDependencyScratch = benchmarkGraphLocalAutoDependencyScratchPmrEnabled_;
  if (!usePmrDependencyScratch) {
    stdDependencyCountByBindingKey.reserve(graph.nodes.size());
  }
  auto dependencyCountTryEmplaceZero = [&](const GraphLocalAutoKey &bindingKey) {
    if (usePmrDependencyScratch) {
      dependencyCountByBindingKey.try_emplace(bindingKey, 0);
      return;
    }
    stdDependencyCountByBindingKey.try_emplace(bindingKey, 0);
  };
  auto dependencyCountIncrement = [&](const GraphLocalAutoKey &bindingKey) {
    if (usePmrDependencyScratch) {
      ++dependencyCountByBindingKey[bindingKey];
      return;
    }
    ++stdDependencyCountByBindingKey[bindingKey];
  };
  auto dependencyCountFind = [&](const GraphLocalAutoKey &bindingKey) -> const size_t * {
    if (usePmrDependencyScratch) {
      const auto dependencyIt = dependencyCountByBindingKey.find(bindingKey);
      if (dependencyIt == dependencyCountByBindingKey.end()) {
        return nullptr;
      }
      return &dependencyIt->second;
    }
    const auto dependencyIt = stdDependencyCountByBindingKey.find(bindingKey);
    if (dependencyIt == stdDependencyCountByBindingKey.end()) {
      return nullptr;
    }
    return &dependencyIt->second;
  };
  for (const TypeResolutionGraphNode &node : graph.nodes) {
    if (node.kind != TypeResolutionNodeKind::LocalAuto) {
      continue;
    }
    dependencyCountTryEmplaceZero(
        graphLocalAutoBindingKey(node.scopePath, node.sourceLine, node.sourceColumn));
  }
  for (const TypeResolutionGraphEdge &edge : graph.edges) {
    if (edge.kind != TypeResolutionEdgeKind::Dependency || edge.sourceId >= graph.nodes.size() ||
        edge.targetId >= graph.nodes.size()) {
      continue;
    }
    const TypeResolutionGraphNode &sourceNode = graph.nodes[edge.sourceId];
    const TypeResolutionGraphNode &targetNode = graph.nodes[edge.targetId];
    if (sourceNode.kind != TypeResolutionNodeKind::LocalAuto ||
        targetNode.kind != TypeResolutionNodeKind::CallConstraint) {
      continue;
    }
    dependencyCountIncrement(graphLocalAutoBindingKey(
        sourceNode.scopePath, sourceNode.sourceLine, sourceNode.sourceColumn));
  }

  struct ActiveLocalBindings {
    struct Scope {
      explicit Scope(ActiveLocalBindings &localsIn) : locals(localsIn), marker(localsIn.insertedNames.size()) {}
      ~Scope() {
        while (locals.insertedNames.size() > marker) {
          locals.bindings.erase(locals.insertedNames.back());
          locals.insertedNames.pop_back();
        }
      }

      ActiveLocalBindings &locals;
      size_t marker = 0;
    };

    void addBinding(std::string name, BindingInfo binding) {
      auto [it, inserted] = bindings.emplace(std::move(name), std::move(binding));
      (void)it;
      if (!inserted) {
        return;
      }
      insertedNames.push_back(it->first);
    }

    std::unordered_map<std::string, BindingInfo> bindings;
    std::vector<std::string> insertedNames;
  };

  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = error_;
    error_.clear();
    const bool ok = fn();
    error_.clear();
    error_ = previousError;
    return ok;
  };

  auto shouldCaptureGraphBinding = [&](const Definition &def, const Expr &bindingExpr) {
    const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(bindingExpr);
    const GraphLocalAutoKey bindingKey =
        graphLocalAutoBindingKey(def.fullPath, sourceLine, sourceColumn);
    const bool isStructFieldOmittedEnvelope =
        structNames_.count(def.fullPath) > 0 &&
        !hasExplicitBindingTypeTransform(bindingExpr) &&
        bindingExpr.args.size() == 1;
    if (isStructFieldOmittedEnvelope) {
      return true;
    }
    if (bindingExpr.args.size() != 1) {
      return dependencyCountFind(bindingKey) != nullptr;
    }
    const Expr &initializer = bindingExpr.args.front();
    if (initializer.kind != Expr::Kind::Call || initializer.isBinding) {
      return true;
    }
    if (isIfCall(initializer) || isMatchCall(initializer) || isBuiltinBlockCall(initializer)) {
      return true;
    }
    if (initializer.isMethodCall && !initializer.args.empty()) {
      const Expr &receiver = initializer.args.front();
      if (receiver.kind == Expr::Kind::Name &&
          normalizeBindingTypeName(receiver.name) == "Result" &&
          (initializer.name == "map" || initializer.name == "and_then" ||
           initializer.name == "map2")) {
        return true;
      }
    }
    const size_t *dependencyCount = dependencyCountFind(bindingKey);
    if (dependencyCount == nullptr) {
      return true;
    }
    return true;
  };

  auto inferBindingForLocals = [&](const Definition &def,
                                   const std::vector<ParameterInfo> &defParams,
                                   const std::unordered_map<std::string, BindingInfo> &activeLocals,
                                   const Expr &bindingExpr,
                                   BindingInfo &bindingOut) {
    const std::string namespacePrefix =
        bindingExpr.namespacePrefix.empty() ? def.namespacePrefix : bindingExpr.namespacePrefix;
    std::optional<std::string> restrictType;
    if (!withPreservedError([&]() {
          return parseBindingInfo(
              bindingExpr, namespacePrefix, structNames_, importAliases_, bindingOut, restrictType, error_);
        })) {
      return false;
    }
    const bool hasExplicitType = hasExplicitBindingTypeTransform(bindingExpr);
    const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(bindingOut.typeName) == "auto";
    if (bindingExpr.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
      BindingInfo inferred = bindingOut;
      if (withPreservedError([&]() {
            return inferBindingTypeFromInitializer(
                bindingExpr.args.front(), defParams, activeLocals, inferred, &bindingExpr);
          })) {
        bindingOut = std::move(inferred);
      }
    }
    if (!restrictType.has_value()) {
      return true;
    }
    const bool hasTemplate = !bindingOut.typeTemplateArg.empty();
    return restrictMatchesBinding(
        *restrictType, bindingOut.typeName, bindingOut.typeTemplateArg, hasTemplate, namespacePrefix);
  };
  auto isLocalAutoCandidate = [&](const Expr &bindingExpr) {
    for (const auto &transform : bindingExpr.transforms) {
      if (transform.name == "effects" || transform.name == "capabilities") {
        continue;
      }
      if (isBindingAuxTransformName(transform.name) || !transform.arguments.empty()) {
        continue;
      }
      return normalizeBindingTypeName(transform.name) == "auto";
    }
    return true;
  };

  std::function<void(const Definition &, const std::vector<ParameterInfo> &, const Expr &, ActiveLocalBindings &)>
      visitExpr;
  std::function<void(const Definition &, const std::vector<ParameterInfo> &, const std::vector<Expr> &, ActiveLocalBindings &)>
      visitExprSequence;

  visitExprSequence = [&](const Definition &def,
                          const std::vector<ParameterInfo> &defParams,
                          const std::vector<Expr> &exprs,
                          ActiveLocalBindings &activeLocals) {
    for (const auto &expr : exprs) {
      visitExpr(def, defParams, expr, activeLocals);
    }
  };

  visitExpr = [&](const Definition &def,
                  const std::vector<ParameterInfo> &defParams,
                  const Expr &expr,
                  ActiveLocalBindings &activeLocals) {
    if (expr.isBinding) {
      for (const auto &arg : expr.args) {
        ActiveLocalBindings::Scope argScope(activeLocals);
        visitExpr(def, defParams, arg, activeLocals);
      }
      if (!expr.bodyArguments.empty()) {
        ActiveLocalBindings::Scope bodyScope(activeLocals);
        visitExprSequence(def, defParams, expr.bodyArguments, activeLocals);
      }

      BindingInfo binding;
      if (!inferBindingForLocals(def, defParams, activeLocals.bindings, expr, binding)) {
        return;
      }
      if (isLocalAutoCandidate(expr) && shouldCaptureGraphBinding(def, expr) &&
          graphBindingIsUsable(binding)) {
        const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(expr);
        const GraphLocalAutoKey bindingKey =
            graphLocalAutoBindingKey(def.fullPath, sourceLine, sourceColumn);
        auto [factIt, inserted] = graphLocalAutoFacts_.try_emplace(bindingKey);
        (void)inserted;
        GraphLocalAutoFacts &fact = factIt->second;
        fact.hasBinding = true;
        fact.binding = binding;
        const Expr *initializerAnalysisExpr = expr.args.size() == 1 ? &expr.args.front() : nullptr;
        if (initializerAnalysisExpr != nullptr &&
            !initializerAnalysisExpr->isMethodCall &&
            initializerAnalysisExpr->args.size() == 1 &&
            initializerAnalysisExpr->templateArgs.empty() &&
            !initializerAnalysisExpr->hasBodyArguments &&
            initializerAnalysisExpr->bodyArguments.empty()) {
          std::string initializerWrapperPath =
              preferredCollectionHelperResolvedPath(*initializerAnalysisExpr);
          if (initializerWrapperPath.empty()) {
            initializerWrapperPath = resolveCalleePath(*initializerAnalysisExpr);
          }
          if (!initializerWrapperPath.empty()) {
            const std::string concreteInitializerWrapperPath =
                resolveExprConcreteCallPath(defParams,
                                            activeLocals.bindings,
                                            *initializerAnalysisExpr,
                                            initializerWrapperPath);
            if (!concreteInitializerWrapperPath.empty()) {
              initializerWrapperPath = concreteInitializerWrapperPath;
            }
          }
          if (isSimpleCallName(*initializerAnalysisExpr, "try") ||
              initializerWrapperPath == "/Result/ok") {
            initializerAnalysisExpr = &initializerAnalysisExpr->args.front();
          }
        }
        CallSnapshotData initializerCallData;
        if (initializerAnalysisExpr != nullptr &&
            inferCallSnapshotData(defParams, activeLocals.bindings, *initializerAnalysisExpr, initializerCallData)) {
          if (!initializerCallData.resolvedPath.empty()) {
            fact.initializerResolvedPath = std::move(initializerCallData.resolvedPath);
          } else {
            fact.initializerResolvedPath.clear();
          }
          if (!initializerCallData.binding.typeName.empty()) {
            fact.hasInitializerBinding = true;
            fact.initializerBinding = std::move(initializerCallData.binding);
          } else {
            fact.hasInitializerBinding = false;
            fact.initializerBinding = BindingInfo{};
          }
        } else {
          fact.initializerResolvedPath.clear();
          fact.hasInitializerBinding = false;
          fact.initializerBinding = BindingInfo{};
        }
        if (initializerAnalysisExpr != nullptr &&
            initializerAnalysisExpr->kind == Expr::Kind::Call &&
            !initializerAnalysisExpr->isMethodCall) {
          std::string directCallResolvedPath =
              preferredCollectionHelperResolvedPath(*initializerAnalysisExpr);
          if (directCallResolvedPath.empty()) {
            directCallResolvedPath = resolveCalleePath(*initializerAnalysisExpr);
          }
          if (!directCallResolvedPath.empty()) {
            const std::string concreteDirectCallResolvedPath = resolveExprConcreteCallPath(
                defParams,
                activeLocals.bindings,
                *initializerAnalysisExpr,
                directCallResolvedPath);
            if (!concreteDirectCallResolvedPath.empty()) {
              directCallResolvedPath = concreteDirectCallResolvedPath;
            }
          }
          if (!directCallResolvedPath.empty()) {
            fact.directCallResolvedPath = directCallResolvedPath;
            const auto directCallReturnKindIt = returnKinds_.find(directCallResolvedPath);
            if (directCallReturnKindIt != returnKinds_.end()) {
              fact.hasDirectCallReturnKind = true;
              fact.directCallReturnKind = directCallReturnKindIt->second;
            } else {
              fact.hasDirectCallReturnKind = false;
              fact.directCallReturnKind = ReturnKind::Unknown;
            }
          } else {
            fact.directCallResolvedPath.clear();
            fact.hasDirectCallReturnKind = false;
            fact.directCallReturnKind = ReturnKind::Unknown;
          }
        } else {
          fact.directCallResolvedPath.clear();
          fact.hasDirectCallReturnKind = false;
          fact.directCallReturnKind = ReturnKind::Unknown;
        }
        if (initializerAnalysisExpr != nullptr &&
            initializerAnalysisExpr->kind == Expr::Kind::Call &&
            initializerAnalysisExpr->isMethodCall &&
            !initializerAnalysisExpr->args.empty() &&
            !initializerAnalysisExpr->name.empty()) {
          std::string resolvedMethodTarget;
          bool isBuiltinMethod = false;
          if (withPreservedError([&]() {
                return resolveMethodTarget(defParams,
                                           activeLocals.bindings,
                                           initializerAnalysisExpr->namespacePrefix,
                                           initializerAnalysisExpr->args.front(),
                                           initializerAnalysisExpr->name,
                                           resolvedMethodTarget,
                                           isBuiltinMethod);
              }) &&
              !resolvedMethodTarget.empty()) {
            const std::string concreteResolvedMethodTarget = resolveExprConcreteCallPath(
                defParams,
                activeLocals.bindings,
                *initializerAnalysisExpr,
                resolvedMethodTarget);
            if (!concreteResolvedMethodTarget.empty()) {
              resolvedMethodTarget = concreteResolvedMethodTarget;
            }
            fact.methodCallResolvedPath = resolvedMethodTarget;
            const auto methodCallReturnKindIt = returnKinds_.find(resolvedMethodTarget);
            if (methodCallReturnKindIt != returnKinds_.end()) {
              fact.hasMethodCallReturnKind = true;
              fact.methodCallReturnKind = methodCallReturnKindIt->second;
            } else {
              fact.hasMethodCallReturnKind = false;
              fact.methodCallReturnKind = ReturnKind::Unknown;
            }
          } else {
            fact.methodCallResolvedPath.clear();
            fact.hasMethodCallReturnKind = false;
            fact.methodCallReturnKind = ReturnKind::Unknown;
          }
        } else {
          fact.methodCallResolvedPath.clear();
          fact.hasMethodCallReturnKind = false;
          fact.methodCallReturnKind = ReturnKind::Unknown;
        }
        QuerySnapshotData initializerQueryData;
        if (initializerAnalysisExpr != nullptr &&
            inferQuerySnapshotData(defParams, activeLocals.bindings, *initializerAnalysisExpr, initializerQueryData)) {
          fact.hasQuerySnapshot = true;
          fact.querySnapshot = std::move(initializerQueryData);
        } else {
          fact.hasQuerySnapshot = false;
          fact.querySnapshot = QuerySnapshotData{};
        }
        LocalAutoTrySnapshotData initializerTryValue;
        if (expr.args.size() == 1 &&
            inferTrySnapshotData(def, defParams, activeLocals.bindings, expr.args.front(), initializerTryValue)) {
          fact.hasTryValue = true;
          fact.tryValue = std::move(initializerTryValue);
        } else {
          fact.hasTryValue = false;
          fact.tryValue = LocalAutoTrySnapshotData{};
        }
      }
      activeLocals.addBinding(expr.name, std::move(binding));
      return;
    }

    if (isMatchCall(expr)) {
      Expr expanded;
      if (withPreservedError([&]() { return lowerMatchToIf(expr, expanded, error_); })) {
        visitExpr(def, defParams, expanded, activeLocals);
        return;
      }
    }

    if (isIfCall(expr) && expr.args.size() == 3) {
      {
        ActiveLocalBindings::Scope conditionScope(activeLocals);
        visitExpr(def, defParams, expr.args.front(), activeLocals);
      }
      {
        ActiveLocalBindings::Scope thenScope(activeLocals);
        visitExpr(def, defParams, expr.args[1], activeLocals);
      }
      {
        ActiveLocalBindings::Scope elseScope(activeLocals);
        visitExpr(def, defParams, expr.args[2], activeLocals);
      }
      return;
    }

    if ((isLoopCall(expr) || isWhileCall(expr)) && expr.args.size() == 2) {
      ActiveLocalBindings::Scope loopScope(activeLocals);
      visitExpr(def, defParams, expr.args.front(), activeLocals);
      visitExpr(def, defParams, expr.args[1], activeLocals);
      return;
    }

    if (isForCall(expr) && expr.args.size() == 4) {
      ActiveLocalBindings::Scope loopScope(activeLocals);
      visitExpr(def, defParams, expr.args[0], activeLocals);
      if (expr.args[1].isBinding) {
        visitExpr(def, defParams, expr.args[1], activeLocals);
      } else {
        ActiveLocalBindings::Scope indexScope(activeLocals);
        visitExpr(def, defParams, expr.args[1], activeLocals);
      }
      {
        ActiveLocalBindings::Scope limitScope(activeLocals);
        visitExpr(def, defParams, expr.args[2], activeLocals);
      }
      visitExpr(def, defParams, expr.args[3], activeLocals);
      return;
    }

    if (isRepeatCall(expr)) {
      ActiveLocalBindings::Scope repeatScope(activeLocals);
      visitExprSequence(def, defParams, expr.bodyArguments, activeLocals);
      return;
    }

    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      ActiveLocalBindings::Scope blockScope(activeLocals);
      visitExprSequence(def, defParams, expr.bodyArguments, activeLocals);
      return;
    }

    for (const auto &arg : expr.args) {
      ActiveLocalBindings::Scope argScope(activeLocals);
      visitExpr(def, defParams, arg, activeLocals);
    }
    if (!expr.bodyArguments.empty()) {
      ActiveLocalBindings::Scope bodyScope(activeLocals);
      visitExprSequence(def, defParams, expr.bodyArguments, activeLocals);
    }
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));
    const auto paramsIt = paramsByDef_.find(def.fullPath);
    if (paramsIt == paramsByDef_.end()) {
      continue;
    }
    const auto &defParams = paramsIt->second;

    for (const auto &param : def.parameters) {
      for (const auto &arg : param.args) {
        ActiveLocalBindings paramLocals;
        visitExpr(def, defParams, arg, paramLocals);
      }
      if (!param.bodyArguments.empty()) {
        ActiveLocalBindings paramLocals;
        visitExprSequence(def, defParams, param.bodyArguments, paramLocals);
      }
    }

    ActiveLocalBindings definitionLocals;
    visitExprSequence(def, defParams, def.statements, definitionLocals);
    if (def.returnExpr.has_value()) {
      ActiveLocalBindings::Scope returnScope(definitionLocals);
      visitExpr(def, defParams, *def.returnExpr, definitionLocals);
    }
  }

  if (graphLocalAutoBenchmarkShadow_ != nullptr) {
    graphLocalAutoBenchmarkShadow_->rebuildFromFacts(graphLocalAutoFacts_);
  }
}

bool SemanticsValidator::ensureDefinitionReturnKindReady(const Definition &def) {
  if (!allowRecursiveReturnInference_) {
    return true;
  }
  return inferDefinitionReturnKind(def);
}

bool SemanticsValidator::inferDefinitionReturnKindGraphStep(const Definition &def,
                                                           bool finalize,
                                                           bool componentHasCycle,
                                                           bool &changed) {
  changed = false;
  auto failInferGraphDiagnostic = [&](std::string message) -> bool {
    return failUncontextualizedDiagnostic(std::move(message));
  };
  auto kindIt = returnKinds_.find(def.fullPath);
  if (kindIt == returnKinds_.end()) {
    return false;
  }

  bool hasReturnTransform = false;
  bool hasReturnAuto = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    hasReturnTransform = true;
    if (transform.templateArgs.front() == "auto") {
      hasReturnAuto = true;
    }
  }

  const bool previousAllowRecursive = allowRecursiveReturnInference_;
  const bool previousDeferUnknown = deferUnknownReturnInferenceErrors_;
  allowRecursiveReturnInference_ = false;
  deferUnknownReturnInferenceErrors_ = true;

  DefinitionReturnInferenceState inferenceState;
  bool sawExplicitReturnStmt = false;
  size_t implicitReturnStmtIndex = def.statements.size();
  const auto &defParams = paramsByDef_[def.fullPath];
  std::unordered_map<std::string, BindingInfo> locals;
  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    if (isReturnCall(def.statements[stmtIndex])) {
      sawExplicitReturnStmt = true;
      break;
    }
    if (!def.statements[stmtIndex].isBinding) {
      implicitReturnStmtIndex = stmtIndex;
    }
  }

  for (size_t stmtIndex = 0; stmtIndex < def.statements.size(); ++stmtIndex) {
    const Expr &stmt = def.statements[stmtIndex];
    if (!inferDefinitionStatementReturns(def, defParams, stmt, locals, inferenceState)) {
      allowRecursiveReturnInference_ = previousAllowRecursive;
      deferUnknownReturnInferenceErrors_ = previousDeferUnknown;
      return false;
    }
    if (hasReturnTransform && hasReturnAuto && !sawExplicitReturnStmt &&
        stmtIndex == implicitReturnStmtIndex && !inferenceState.sawReturn) {
      if (!recordDefinitionInferredReturn(def, &stmt, defParams, locals, inferenceState)) {
        allowRecursiveReturnInference_ = previousAllowRecursive;
        deferUnknownReturnInferenceErrors_ = previousDeferUnknown;
        return false;
      }
    }
  }
  if (def.returnExpr.has_value()) {
    if (!recordDefinitionInferredReturn(def, &*def.returnExpr, defParams, locals, inferenceState)) {
      allowRecursiveReturnInference_ = previousAllowRecursive;
      deferUnknownReturnInferenceErrors_ = previousDeferUnknown;
      return false;
    }
  }

  allowRecursiveReturnInference_ = previousAllowRecursive;
  deferUnknownReturnInferenceErrors_ = previousDeferUnknown;

  const auto applyKnownReturn = [&](ReturnKind kind,
                                    const std::string &structPath,
                                    const BindingInfo *binding) {
    auto normalizedBindingTypeText = [&](const BindingInfo &candidate) {
      const std::string normalizedCollectionType = normalizeCollectionTypePath(candidate.typeName);
      const std::string normalizedBase =
          normalizedCollectionType.empty() ? normalizeBindingTypeName(candidate.typeName) : normalizedCollectionType;
      if (candidate.typeTemplateArg.empty()) {
        return normalizedBase;
      }
      return normalizedBase + "<" + normalizeBindingTypeName(candidate.typeTemplateArg) + ">";
    };
    if (kindIt->second != kind) {
      kindIt->second = kind;
      changed = true;
    }

    auto structIt = returnStructs_.find(def.fullPath);
    const std::string previousStructPath = structIt == returnStructs_.end() ? std::string() : structIt->second;
    if (kind == ReturnKind::Array && !structPath.empty()) {
      if (previousStructPath != structPath) {
        returnStructs_[def.fullPath] = structPath;
        changed = true;
      }
    } else if (structIt != returnStructs_.end() && !previousStructPath.empty()) {
      returnStructs_.erase(structIt);
      changed = true;
    }

    auto bindingIt = returnBindings_.find(def.fullPath);
    if (binding != nullptr && !binding->typeName.empty()) {
      const bool bindingChanged =
          bindingIt == returnBindings_.end() ||
          normalizedBindingTypeText(bindingIt->second) != normalizedBindingTypeText(*binding);
      if (bindingChanged) {
        returnBindings_[def.fullPath] = *binding;
        changed = true;
      }
      return;
    }
    if (bindingIt != returnBindings_.end()) {
      returnBindings_.erase(bindingIt);
      changed = true;
    }
  };

  if (!inferenceState.sawReturn) {
    if (hasReturnTransform && hasReturnAuto) {
      if (!finalize) {
        return true;
      }
      return failInferGraphDiagnostic("unable to infer return type on " +
                                      def.fullPath);
    }
    applyKnownReturn(ReturnKind::Void, {}, nullptr);
    return true;
  }

  if (inferenceState.inferred == ReturnKind::Unknown || inferenceState.sawUnresolvedReturnDependency) {
    if (!finalize) {
      if (inferenceState.inferred != ReturnKind::Unknown) {
        applyKnownReturn(inferenceState.inferred,
                         inferenceState.inferredStructPath,
                         inferenceState.hasInferredBinding ? &inferenceState.inferredBinding : nullptr);
      }
      return true;
    }
    return failInferGraphDiagnostic(
        componentHasCycle
            ? "return type inference requires explicit annotation on " +
                  def.fullPath
            : "unable to infer return type on " + def.fullPath);
  }

  applyKnownReturn(inferenceState.inferred,
                   inferenceState.inferredStructPath,
                   inferenceState.hasInferredBinding ? &inferenceState.inferredBinding : nullptr);
  return true;
}

} // namespace primec::semantics
