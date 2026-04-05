#include "SemanticsValidator.h"

#include "CondensationDag.h"
#include "TypeResolutionGraph.h"

#include <algorithm>
#include <functional>
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
  return layers;
}

} // namespace

bool SemanticsValidator::inferUnknownReturnKinds() {
  return inferUnknownReturnKindsGraph();
}

bool SemanticsValidator::inferUnknownReturnKindsGraph() {
  graphLocalAutoBindings_.clear();
  graphLocalAutoResolvedPaths_.clear();
  graphLocalAutoInitializerBindings_.clear();
  graphLocalAutoReceiverBindings_.clear();
  graphLocalAutoQueryTypeTexts_.clear();
  graphLocalAutoResultTypes_.clear();
  graphLocalAutoTryValues_.clear();
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
    if (collectDiagnostics_ && diagnosticInfo_ != nullptr) {
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
      diagnosticSink_.setRecords({std::move(record)});
      rememberFirstCollectedDiagnosticMessage(message);
      return false;
    }
    return failUncontextualizedDiagnostic(message);
  };

  const std::vector<std::vector<uint32_t>> dagLayers = buildCondensationDagLayers(dag);
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
  std::unordered_map<std::string, size_t> dependencyCountByBindingKey;
  for (const TypeResolutionGraphNode &node : graph.nodes) {
    if (node.kind != TypeResolutionNodeKind::LocalAuto) {
      continue;
    }
    dependencyCountByBindingKey.try_emplace(
        graphLocalAutoBindingKey(node.scopePath, node.sourceLine, node.sourceColumn), 0);
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
    ++dependencyCountByBindingKey[graphLocalAutoBindingKey(
        sourceNode.scopePath, sourceNode.sourceLine, sourceNode.sourceColumn)];
  }

  using ActiveLocalBindings = std::unordered_map<std::string, BindingInfo>;

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
    const std::string bindingKey =
        graphLocalAutoBindingKey(def.fullPath, sourceLine, sourceColumn);
    const auto dependencyIt = dependencyCountByBindingKey.find(bindingKey);
    if (dependencyIt == dependencyCountByBindingKey.end() || bindingExpr.args.size() != 1) {
      return false;
    }
    const Expr &initializer = bindingExpr.args.front();
    if (initializer.kind != Expr::Kind::Call || initializer.isBinding) {
      return false;
    }
    if (dependencyIt->second == 1 && !initializer.isMethodCall && !isIfCall(initializer) &&
        !isMatchCall(initializer) && !isBuiltinBlockCall(initializer)) {
      return true;
    }
    return dependencyIt->second > 0 &&
           (isIfCall(initializer) || isMatchCall(initializer) || isBuiltinBlockCall(initializer));
  };

  auto inferBindingForLocals = [&](const Definition &def,
                                   const std::vector<ParameterInfo> &defParams,
                                   const ActiveLocalBindings &activeLocals,
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
        ActiveLocalBindings argLocals = activeLocals;
        visitExpr(def, defParams, arg, argLocals);
      }
      if (!expr.bodyArguments.empty()) {
        ActiveLocalBindings bodyLocals = activeLocals;
        visitExprSequence(def, defParams, expr.bodyArguments, bodyLocals);
      }

      BindingInfo binding;
      if (!inferBindingForLocals(def, defParams, activeLocals, expr, binding)) {
        return;
      }
      if (shouldCaptureGraphBinding(def, expr)) {
        const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(expr);
        const std::string bindingKey =
            graphLocalAutoBindingKey(def.fullPath, sourceLine, sourceColumn);
        graphLocalAutoBindings_.try_emplace(bindingKey, binding);
        const Expr *initializerAnalysisExpr = expr.args.size() == 1 ? &expr.args.front() : nullptr;
        if (initializerAnalysisExpr != nullptr &&
            !initializerAnalysisExpr->isMethodCall &&
            initializerAnalysisExpr->args.size() == 1 &&
            initializerAnalysisExpr->templateArgs.empty() &&
            !initializerAnalysisExpr->hasBodyArguments &&
            initializerAnalysisExpr->bodyArguments.empty()) {
          const std::string initializerWrapperPath = resolveCalleePath(*initializerAnalysisExpr);
          if (isSimpleCallName(*initializerAnalysisExpr, "try") ||
              initializerWrapperPath == "/Result/ok") {
            initializerAnalysisExpr = &initializerAnalysisExpr->args.front();
          }
        }
        CallSnapshotData initializerCallData;
        if (initializerAnalysisExpr != nullptr &&
            inferCallSnapshotData(defParams, activeLocals, *initializerAnalysisExpr, initializerCallData)) {
          if (!initializerCallData.resolvedPath.empty()) {
            graphLocalAutoResolvedPaths_[bindingKey] = std::move(initializerCallData.resolvedPath);
          } else {
            graphLocalAutoResolvedPaths_.erase(bindingKey);
          }
          if (!initializerCallData.binding.typeName.empty()) {
            graphLocalAutoInitializerBindings_[bindingKey] = std::move(initializerCallData.binding);
          } else {
            graphLocalAutoInitializerBindings_.erase(bindingKey);
          }
        } else {
          graphLocalAutoResolvedPaths_.erase(bindingKey);
          graphLocalAutoInitializerBindings_.erase(bindingKey);
        }
        QuerySnapshotData initializerQueryData;
        if (initializerAnalysisExpr != nullptr &&
            inferQuerySnapshotData(defParams, activeLocals, *initializerAnalysisExpr, initializerQueryData)) {
          if (!initializerQueryData.receiverBinding.typeName.empty()) {
            graphLocalAutoReceiverBindings_[bindingKey] = std::move(initializerQueryData.receiverBinding);
          } else {
            graphLocalAutoReceiverBindings_.erase(bindingKey);
          }
          if (!initializerQueryData.typeText.empty()) {
            graphLocalAutoQueryTypeTexts_[bindingKey] = std::move(initializerQueryData.typeText);
          } else {
            graphLocalAutoQueryTypeTexts_.erase(bindingKey);
          }
          if (initializerQueryData.resultInfo.isResult) {
            graphLocalAutoResultTypes_[bindingKey] = std::move(initializerQueryData.resultInfo);
          } else {
            graphLocalAutoResultTypes_.erase(bindingKey);
          }
        } else {
          graphLocalAutoReceiverBindings_.erase(bindingKey);
          graphLocalAutoQueryTypeTexts_.erase(bindingKey);
          graphLocalAutoResultTypes_.erase(bindingKey);
        }
        LocalAutoTrySnapshotData initializerTryValue;
        if (expr.args.size() == 1 &&
            inferTrySnapshotData(def, defParams, activeLocals, expr.args.front(), initializerTryValue)) {
          graphLocalAutoTryValues_[bindingKey] = std::move(initializerTryValue);
        } else {
          graphLocalAutoTryValues_.erase(bindingKey);
        }
      }
      activeLocals.emplace(expr.name, std::move(binding));
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
      ActiveLocalBindings conditionLocals = activeLocals;
      visitExpr(def, defParams, expr.args.front(), conditionLocals);

      ActiveLocalBindings thenLocals = activeLocals;
      visitExpr(def, defParams, expr.args[1], thenLocals);

      ActiveLocalBindings elseLocals = activeLocals;
      visitExpr(def, defParams, expr.args[2], elseLocals);
      return;
    }

    if ((isLoopCall(expr) || isWhileCall(expr)) && expr.args.size() == 2) {
      ActiveLocalBindings loopLocals = activeLocals;
      visitExpr(def, defParams, expr.args.front(), loopLocals);
      visitExpr(def, defParams, expr.args[1], loopLocals);
      return;
    }

    if (isForCall(expr) && expr.args.size() == 4) {
      ActiveLocalBindings loopLocals = activeLocals;
      visitExpr(def, defParams, expr.args[0], loopLocals);
      if (expr.args[1].isBinding) {
        visitExpr(def, defParams, expr.args[1], loopLocals);
      } else {
        ActiveLocalBindings indexLocals = loopLocals;
        visitExpr(def, defParams, expr.args[1], indexLocals);
      }
      ActiveLocalBindings limitLocals = loopLocals;
      visitExpr(def, defParams, expr.args[2], limitLocals);
      visitExpr(def, defParams, expr.args[3], loopLocals);
      return;
    }

    if (isRepeatCall(expr)) {
      ActiveLocalBindings repeatLocals = activeLocals;
      visitExprSequence(def, defParams, expr.bodyArguments, repeatLocals);
      return;
    }

    if (isBuiltinBlockCall(expr) && expr.hasBodyArguments) {
      ActiveLocalBindings blockLocals = activeLocals;
      visitExprSequence(def, defParams, expr.bodyArguments, blockLocals);
      return;
    }

    for (const auto &arg : expr.args) {
      ActiveLocalBindings argLocals = activeLocals;
      visitExpr(def, defParams, arg, argLocals);
    }
    if (!expr.bodyArguments.empty()) {
      ActiveLocalBindings bodyLocals = activeLocals;
      visitExprSequence(def, defParams, expr.bodyArguments, bodyLocals);
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
      ActiveLocalBindings returnLocals = definitionLocals;
      visitExpr(def, defParams, *def.returnExpr, returnLocals);
    }
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
