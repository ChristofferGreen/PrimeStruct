#include "SemanticsValidator.h"

#include "CondensationDag.h"
#include "TypeResolutionGraph.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <sstream>

namespace primec::semantics {

namespace {

void sortDefinitionsForDeterministicDiagnostics(std::vector<const Definition *> &definitions) {
  std::stable_sort(definitions.begin(), definitions.end(), [](const Definition *left, const Definition *right) {
    const int leftLine = left->sourceLine > 0 ? left->sourceLine : std::numeric_limits<int>::max();
    const int rightLine = right->sourceLine > 0 ? right->sourceLine : std::numeric_limits<int>::max();
    if (leftLine != rightLine) {
      return leftLine < rightLine;
    }
    const int leftColumn = left->sourceColumn > 0 ? left->sourceColumn : std::numeric_limits<int>::max();
    const int rightColumn = right->sourceColumn > 0 ? right->sourceColumn : std::numeric_limits<int>::max();
    if (leftColumn != rightColumn) {
      return leftColumn < rightColumn;
    }
    return left->fullPath < right->fullPath;
  });
}

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

} // namespace

bool SemanticsValidator::inferUnknownReturnKinds() {
  if (graphTypeResolverEnabled_) {
    return inferUnknownReturnKindsGraph();
  }
  return inferUnknownReturnKindsLegacy();
}

bool SemanticsValidator::inferUnknownReturnKindsLegacy() {
  graphLocalAutoBindings_.clear();
  for (const auto &def : program_.definitions) {
    if (returnKinds_[def.fullPath] == ReturnKind::Unknown) {
      if (!inferDefinitionReturnKind(def)) {
        return false;
      }
    }
  }
  return true;
}

bool SemanticsValidator::inferUnknownReturnKindsGraph() {
  graphLocalAutoBindings_.clear();
  const TypeResolutionGraph graph = buildTypeResolutionGraph(program_);

  std::vector<DirectedGraphEdge> dependencyEdges;
  dependencyEdges.reserve(graph.edges.size());
  for (const TypeResolutionGraphEdge &edge : graph.edges) {
    if (edge.kind != TypeResolutionEdgeKind::Dependency) {
      continue;
    }
    dependencyEdges.push_back(DirectedGraphEdge{edge.sourceId, edge.targetId});
  }

  const CondensationDag dag =
      computeCondensationDag(static_cast<uint32_t>(graph.nodes.size()), dependencyEdges);

  auto collectUnknownDefinitions = [&](const CondensationDagNode &componentNode) {
    std::vector<const Definition *> definitions;
    definitions.reserve(componentNode.memberNodeIds.size());
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
      definitions.push_back(defIt->second);
    }
    sortDefinitionsForDeterministicDiagnostics(definitions);
    return definitions;
  };

  for (auto componentIt = dag.topologicalComponentIds.rbegin();
       componentIt != dag.topologicalComponentIds.rend();
       ++componentIt) {
    const CondensationDagNode &componentNode = dag.nodes[*componentIt];
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

    std::vector<const Definition *> unresolvedDefinitions = collectUnknownDefinitions(componentNode);
    if (unresolvedDefinitions.empty()) {
      continue;
    }

    if (componentHasCycle) {
      error_ = formatReturnInferenceCycleDiagnostic(unresolvedDefinitions);
      if (collectDiagnostics_ && diagnosticInfo_ != nullptr) {
        DiagnosticSinkRecord record;
        record.message = error_;
        const Definition *primary = unresolvedDefinitions.front();
        if (primary->sourceLine > 0 && primary->sourceColumn > 0) {
          record.primarySpan.line = primary->sourceLine;
          record.primarySpan.column = primary->sourceColumn;
          record.primarySpan.endLine = primary->sourceLine;
          record.primarySpan.endColumn = primary->sourceColumn;
          record.hasPrimarySpan = true;
        }
        for (const Definition *definition : unresolvedDefinitions) {
          if (definition->sourceLine <= 0 || definition->sourceColumn <= 0) {
            continue;
          }
          DiagnosticRelatedSpan span;
          span.span.line = definition->sourceLine;
          span.span.column = definition->sourceColumn;
          span.span.endLine = definition->sourceLine;
          span.span.endColumn = definition->sourceColumn;
          span.label = "cycle member: " + definition->fullPath;
          record.relatedSpans.push_back(std::move(span));
        }
        diagnosticSink_.setRecords({std::move(record)});
      }
      return false;
    }

    for (const Definition *definition : unresolvedDefinitions) {
      bool definitionChanged = false;
      if (!inferDefinitionReturnKindGraphStep(*definition, true, componentHasCycle, definitionChanged)) {
        return false;
      }
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
    const std::string bindingKey =
        graphLocalAutoBindingKey(def.fullPath, bindingExpr.sourceLine, bindingExpr.sourceColumn);
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
    if (!hasExplicitBindingTypeTransform(bindingExpr) && bindingExpr.args.size() == 1) {
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
        const std::string bindingKey =
            graphLocalAutoBindingKey(def.fullPath, expr.sourceLine, expr.sourceColumn);
        graphLocalAutoBindings_.try_emplace(bindingKey, binding);
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
    ValidationContextScope validationContextScope(*this, buildDefinitionValidationContext(def));
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

  const auto applyKnownReturn = [&](ReturnKind kind, const std::string &structPath) {
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
      return;
    }
    if (structIt != returnStructs_.end() && !previousStructPath.empty()) {
      returnStructs_.erase(structIt);
      changed = true;
    }
  };

  if (!inferenceState.sawReturn) {
    if (hasReturnTransform && hasReturnAuto) {
      if (!finalize) {
        return true;
      }
      error_ = "unable to infer return type on " + def.fullPath;
      return false;
    }
    applyKnownReturn(ReturnKind::Void, {});
    return true;
  }

  if (inferenceState.inferred == ReturnKind::Unknown || inferenceState.sawUnresolvedReturnDependency) {
    if (!finalize) {
      if (inferenceState.inferred != ReturnKind::Unknown) {
        applyKnownReturn(inferenceState.inferred, inferenceState.inferredStructPath);
      }
      return true;
    }
    error_ = componentHasCycle ? "return type inference requires explicit annotation on " + def.fullPath
                               : "unable to infer return type on " + def.fullPath;
    return false;
  }

  applyKnownReturn(inferenceState.inferred, inferenceState.inferredStructPath);
  return true;
}

ReturnKind SemanticsValidator::inferExprReturnKind(const Expr &expr,
                                                   const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals) {
  auto combineNumeric = [&](ReturnKind left, ReturnKind right) -> ReturnKind {
    if (left == ReturnKind::Unknown || right == ReturnKind::Unknown) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Bool || right == ReturnKind::Bool || left == ReturnKind::String ||
        right == ReturnKind::String || left == ReturnKind::Void || right == ReturnKind::Void ||
        left == ReturnKind::Array || right == ReturnKind::Array) {
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Float64 || right == ReturnKind::Float64) {
      return ReturnKind::Float64;
    }
    if (left == ReturnKind::Float32 || right == ReturnKind::Float32) {
      return ReturnKind::Float32;
    }
    if (left == ReturnKind::UInt64 || right == ReturnKind::UInt64) {
      return (left == ReturnKind::UInt64 && right == ReturnKind::UInt64) ? ReturnKind::UInt64 : ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int64 || right == ReturnKind::Int64) {
      if ((left == ReturnKind::Int64 || left == ReturnKind::Int) &&
          (right == ReturnKind::Int64 || right == ReturnKind::Int)) {
        return ReturnKind::Int64;
      }
      return ReturnKind::Unknown;
    }
    if (left == ReturnKind::Int && right == ReturnKind::Int) {
      return ReturnKind::Int;
    }
    return ReturnKind::Unknown;
  };
  auto isStructTypeName = [&](const std::string &typeName, const std::string &namespacePrefix) -> bool {
    if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
      return false;
    }
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    if (structNames_.count(resolved) > 0) {
      return true;
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end()) {
      return structNames_.count(importIt->second) > 0;
    }
    return false;
  };
  auto referenceTargetKind = [&](const std::string &templateArg, const std::string &namespacePrefix) -> ReturnKind {
    if (templateArg.empty()) {
      return ReturnKind::Unknown;
    }
    ReturnKind kind = returnKindForTypeName(templateArg);
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (isStructTypeName(templateArg, namespacePrefix)) {
      return ReturnKind::Array;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    if (splitTemplateTypeName(templateArg, base, arg) && isMapCollectionTypeName(base)) {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
        return ReturnKind::Array;
      }
    }
    return ReturnKind::Unknown;
  };
  auto uninitializedTargetKind = [&](const std::string &templateArg, const std::string &namespacePrefix) -> ReturnKind {
    if (templateArg.empty()) {
      return ReturnKind::Unknown;
    }
    ReturnKind kind = returnKindForTypeName(templateArg);
    if (kind != ReturnKind::Unknown) {
      return kind;
    }
    if (isStructTypeName(templateArg, namespacePrefix)) {
      return ReturnKind::Array;
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(templateArg, base, arg) && normalizeBindingTypeName(base) == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    if (splitTemplateTypeName(templateArg, base, arg) && isMapCollectionTypeName(base)) {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 2) {
        return ReturnKind::Array;
      }
    }
    return ReturnKind::Unknown;
  };

  if (expr.isLambda) {
    return ReturnKind::Unknown;
  }

  if (expr.kind == Expr::Kind::Literal) {
    if (expr.isUnsigned) {
      return ReturnKind::UInt64;
    }
    if (expr.intWidth == 64) {
      return ReturnKind::Int64;
    }
    return ReturnKind::Int;
  }
  if (expr.kind == Expr::Kind::BoolLiteral) {
    return ReturnKind::Bool;
  }
  if (expr.kind == Expr::Kind::FloatLiteral) {
    return expr.floatWidth == 64 ? ReturnKind::Float64 : ReturnKind::Float32;
  }
  if (expr.kind == Expr::Kind::StringLiteral) {
    return ReturnKind::String;
  }
  if (expr.kind == Expr::Kind::Name) {
    if (isBuiltinMathConstant(expr.name, allowMathBareName(expr.name))) {
      return ReturnKind::Float64;
    }
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
        ReturnKind refKind = referenceTargetKind(paramBinding->typeTemplateArg, expr.namespacePrefix);
        if (refKind != ReturnKind::Unknown) {
          return refKind;
        }
      }
      const std::string normalizedTypeName = normalizeBindingTypeName(paramBinding->typeName);
      if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
           normalizedTypeName == "soa_vector" || isMapCollectionTypeName(normalizedTypeName)) &&
          !paramBinding->typeTemplateArg.empty()) {
        return ReturnKind::Array;
      }
      if (isStructTypeName(paramBinding->typeName, expr.namespacePrefix)) {
        return ReturnKind::Array;
      }
      return returnKindForTypeName(paramBinding->typeName);
    }
    auto it = locals.find(expr.name);
    if (it == locals.end()) {
      return ReturnKind::Unknown;
    }
    if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
      ReturnKind refKind = referenceTargetKind(it->second.typeTemplateArg, expr.namespacePrefix);
      if (refKind != ReturnKind::Unknown) {
        return refKind;
      }
    }
    const std::string normalizedTypeName = normalizeBindingTypeName(it->second.typeName);
    if ((normalizedTypeName == "array" || normalizedTypeName == "vector" ||
         normalizedTypeName == "soa_vector" || isMapCollectionTypeName(normalizedTypeName)) &&
        !it->second.typeTemplateArg.empty()) {
      return ReturnKind::Array;
    }
    if (isStructTypeName(it->second.typeName, expr.namespacePrefix)) {
      return ReturnKind::Array;
    }
    return returnKindForTypeName(it->second.typeName);
  }
  if (expr.kind == Expr::Kind::Call) {
    if (expr.isFieldAccess) {
      auto resolveStructFieldKind = [&](const Expr &receiver, const std::string &fieldName) -> ReturnKind {
        auto isStaticField = [](const Expr &stmt) -> bool {
          for (const auto &transform : stmt.transforms) {
            if (transform.name == "static") {
              return true;
            }
          }
          return false;
        };
        auto resolveStructPathFromType = [&](const std::string &typeName,
                                             const std::string &namespacePrefix,
                                             std::string &structPathOut) -> bool {
          if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
            return false;
          }
          if (!typeName.empty() && typeName[0] == '/') {
            if (structNames_.count(typeName) > 0) {
              structPathOut = typeName;
              return true;
            }
            return false;
          }
          std::string current = namespacePrefix;
          while (true) {
            if (!current.empty()) {
              std::string scoped = current + "/" + typeName;
              if (structNames_.count(scoped) > 0) {
                structPathOut = scoped;
                return true;
              }
              if (current.size() > typeName.size()) {
                const size_t start = current.size() - typeName.size();
                if (start > 0 && current[start - 1] == '/' &&
                    current.compare(start, typeName.size(), typeName) == 0 &&
                    structNames_.count(current) > 0) {
                  structPathOut = current;
                  return true;
                }
              }
            } else {
              std::string root = "/" + typeName;
              if (structNames_.count(root) > 0) {
                structPathOut = root;
                return true;
              }
            }
            if (current.empty()) {
              break;
            }
            const size_t slash = current.find_last_of('/');
            if (slash == std::string::npos || slash == 0) {
              current.clear();
            } else {
              current.erase(slash);
            }
          }
          auto importIt = importAliases_.find(typeName);
          if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
            structPathOut = importIt->second;
            return true;
          }
          return false;
        };
        std::string structPath;
        const bool isTypeReceiver =
            receiver.kind == Expr::Kind::Name &&
            findParamBinding(params, receiver.name) == nullptr &&
            locals.find(receiver.name) == locals.end() &&
            resolveStructPathFromType(receiver.name, receiver.namespacePrefix, structPath);
        if (receiver.kind == Expr::Kind::Name) {
          const BindingInfo *binding = findParamBinding(params, receiver.name);
          if (!binding) {
            auto it = locals.find(receiver.name);
            if (it != locals.end()) {
              binding = &it->second;
            }
          }
          if (binding) {
            std::string typeName = binding->typeName;
            if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
              typeName = binding->typeTemplateArg;
            }
            (void)resolveStructPathFromType(typeName, receiver.namespacePrefix, structPath);
          }
        } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
          structPath = inferStructReturnPath(receiver, params, locals);
        }
        if (structPath.empty()) {
          ReturnKind receiverKind = inferExprReturnKind(receiver, params, locals);
          if (receiverKind != ReturnKind::Unknown && receiverKind != ReturnKind::Array) {
            return receiverKind;
          }
          return ReturnKind::Unknown;
        }
        auto defIt = defMap_.find(structPath);
        if (defIt == defMap_.end() || !defIt->second) {
          return ReturnKind::Unknown;
        }
        for (const auto &stmt : defIt->second->statements) {
          if (!stmt.isBinding) {
            continue;
          }
          if (isTypeReceiver ? !isStaticField(stmt) : isStaticField(stmt)) {
            continue;
          }
          if (stmt.name != fieldName) {
            continue;
          }
          BindingInfo fieldBinding;
          if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
            return ReturnKind::Unknown;
          }
          std::string typeName = fieldBinding.typeName;
          if ((typeName == "Reference" || typeName == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
            typeName = fieldBinding.typeTemplateArg;
          }
          ReturnKind kind = returnKindForTypeName(typeName);
          if (kind != ReturnKind::Unknown) {
            return kind;
          }
          std::string fieldStructPath;
          if (resolveStructPathFromType(typeName, stmt.namespacePrefix, fieldStructPath)) {
            return ReturnKind::Array;
          }
          return ReturnKind::Unknown;
        }
        return ReturnKind::Unknown;
      };
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return resolveStructFieldKind(expr.args.front(), expr.name);
    }
    if (isLoopCall(expr) || isWhileCall(expr) || isForCall(expr) || isRepeatCall(expr)) {
      return ReturnKind::Void;
    }
    if (!expr.isMethodCall &&
        (isSimpleCallName(expr, "take") || isSimpleCallName(expr, "borrow")) && expr.args.size() == 1) {
      const Expr &storage = expr.args.front();
      BindingInfo binding;
      bool resolved = false;
      if (!resolveUninitializedStorageBinding(params, locals, storage, binding, resolved)) {
        return ReturnKind::Unknown;
      }
      if (resolved && binding.typeName == "uninitialized" && !binding.typeTemplateArg.empty()) {
        ReturnKind kind = uninitializedTargetKind(binding.typeTemplateArg, storage.namespacePrefix);
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
    }
    auto preferVectorStdlibHelperPathForCall = [&](const std::string &path) -> std::string {
      auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
        return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
               suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
               suffix != "remove_at" && suffix != "remove_swap";
      };
      auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
        return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
               suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
               suffix != "remove_at" && suffix != "remove_swap";
      };
      std::string preferred = path;
      if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/array/").size());
        if (allowsArrayVectorCompatibilitySuffix(suffix)) {
          const std::string vectorAlias = "/vector/" + suffix;
          if (defMap_.count(vectorAlias) > 0) {
            return vectorAlias;
          }
          const std::string stdlibAlias = "/std/collections/vector/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            return stdlibAlias;
          }
        }
      }
      if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/vector/").size());
        if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
          const std::string stdlibAlias = "/std/collections/vector/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          } else {
            if (allowsArrayVectorCompatibilitySuffix(suffix)) {
              const std::string arrayAlias = "/array/" + suffix;
              if (defMap_.count(arrayAlias) > 0) {
                preferred = arrayAlias;
              }
            }
          }
        }
      }
      if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
        if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
          const std::string vectorAlias = "/vector/" + suffix;
          if (defMap_.count(vectorAlias) > 0) {
            preferred = vectorAlias;
          } else {
            if (allowsArrayVectorCompatibilitySuffix(suffix)) {
              const std::string arrayAlias = "/array/" + suffix;
              if (defMap_.count(arrayAlias) > 0) {
                preferred = arrayAlias;
              }
            }
          }
        }
      }
      if (preferred.rfind("/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/map/").size());
        if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
          const std::string stdlibAlias = "/std/collections/map/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          }
        }
      }
      if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
        if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
            suffix != "at" && suffix != "at_unsafe") {
          const std::string mapAlias = "/map/" + suffix;
          if (defMap_.count(mapAlias) > 0) {
            preferred = mapAlias;
          }
        }
      }
      return preferred;
    };
    const std::string resolvedCalleePath = preferVectorStdlibHelperPathForCall(resolveCalleePath(expr));
    std::string collection;
    if (defMap_.find(resolvedCalleePath) == defMap_.end() && getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") &&
          expr.templateArgs.size() == 1) {
        return ReturnKind::Array;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return ReturnKind::Array;
      }
    }
    bool handledControlFlow = false;
    ReturnKind controlFlowKind = inferControlFlowExprReturnKind(expr, params, locals, handledControlFlow);
    if (handledControlFlow) {
      return controlFlowKind;
    }
    auto resolveBuiltinAccessReceiverExprInline = [&](const Expr &accessExpr) -> const Expr * {
      if (accessExpr.kind != Expr::Kind::Call || accessExpr.args.size() != 2) {
        return nullptr;
      }
      if (accessExpr.isMethodCall) {
        return accessExpr.args.empty() ? nullptr : &accessExpr.args.front();
      }
      size_t receiverIndex = 0;
      if (hasNamedArguments(accessExpr.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < accessExpr.args.size(); ++i) {
          if (i < accessExpr.argNames.size() && accessExpr.argNames[i].has_value() &&
              *accessExpr.argNames[i] == "values") {
            receiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          receiverIndex = 0;
        }
      }
      return receiverIndex < accessExpr.args.size() ? &accessExpr.args[receiverIndex] : nullptr;
    };
    auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      std::string accessName;
      if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
        return false;
      }
      const Expr *accessReceiver = resolveBuiltinAccessReceiverExprInline(target);
      if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
        return false;
      }
      auto resolveBinding = [&](const BindingInfo &binding) {
        return getArgsPackElementType(binding, elemTypeOut);
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(accessReceiver->name);
      return it != locals.end() && resolveBinding(it->second);
    };
    auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {
      pointeeTypeOut.clear();
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      if (base != "Reference" && base != "Pointer") {
        return false;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      pointeeTypeOut = args.front();
      return !pointeeTypeOut.empty();
    };
    auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
        return false;
      }
      std::string wrappedType;
      if (!resolveIndexedArgsPackElementType(target.args.front(), wrappedType)) {
        return false;
      }
      return extractWrappedPointeeType(wrappedType, elemTypeOut);
    };
    auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      std::string wrappedType;
      if (!resolveIndexedArgsPackElementType(target, wrappedType)) {
        return false;
      }
      return extractWrappedPointeeType(wrappedType, elemTypeOut);
    };
    auto extractCollectionElementType = [&](const std::string &typeText,
                                            const std::string &expectedBase,
                                            std::string &elemTypeOut) -> bool {
      elemTypeOut.clear();
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      if (base != expectedBase) {
        return false;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    };
    auto resolveArgsPackCountTarget = [&](const Expr &target, std::string &elemType) -> bool {
      elemType.clear();
      auto resolveBinding = [&](const BindingInfo &binding) {
        return getArgsPackElementType(binding, elemType);
      };
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return resolveBinding(*paramBinding);
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          return resolveBinding(it->second);
        }
      }
      return false;
    };
    const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
        makeBuiltinCollectionDispatchResolvers(params, locals);
    const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
    const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
    const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
    const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
    const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;
    auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName != "soa_vector" || paramBinding->typeTemplateArg.empty()) {
            return false;
          }
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName != "soa_vector" || it->second.typeTemplateArg.empty()) {
            return false;
          }
          elemType = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        std::string indexedElemType;
        if (resolveIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
          return true;
        }
        if (resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
          return true;
        }
        if (resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
            extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
          return true;
        }
        std::string collectionTypePath;
        if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
            collectionTypePath == "/soa_vector") {
          std::vector<std::string> args;
          if (resolveCallCollectionTemplateArgs(target, "soa_vector", params, locals, args) && args.size() == 1) {
            elemType = args.front();
          }
          return true;
        }
        if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
          return resolveVectorTarget(target.args.front(), elemType);
        }
      }
      return false;
    };
    auto resolveBufferTarget = [&](const Expr &target, std::string &elemType) -> bool {
      auto resolveReferenceBufferType = [&](const std::string &typeName,
                                            const std::string &typeTemplateArg,
                                            std::string &elemTypeOut) -> bool {
        if ((typeName != "Reference" && typeName != "Pointer") || typeTemplateArg.empty()) {
          return false;
        }
        std::string base;
        std::string nestedArg;
        if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) || normalizeBindingTypeName(base) != "Buffer" ||
            nestedArg.empty()) {
          return false;
        }
        elemTypeOut = nestedArg;
        return true;
      };
      auto resolveArgsPackReferenceBufferType = [&](const std::string &typeName,
                                                    const std::string &typeTemplateArg,
                                                    std::string &elemTypeOut) -> bool {
        if (typeName != "args" || typeTemplateArg.empty()) {
          return false;
        }
        std::string base;
        std::string nestedArg;
        if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
            (normalizeBindingTypeName(base) != "Reference" && normalizeBindingTypeName(base) != "Pointer") ||
            nestedArg.empty()) {
          return false;
        }
        return resolveReferenceBufferType(base, nestedArg, elemTypeOut);
      };
      auto resolveIndexedArgsPackReferenceBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
        std::string accessName;
        if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
            targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
          return false;
        }
        const std::string &targetName = targetExpr.args.front().name;
        if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
          if (resolveArgsPackReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemTypeOut)) {
            return true;
          }
        }
        auto it = locals.find(targetName);
        return it != locals.end() &&
               resolveArgsPackReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemTypeOut);
      };
      auto resolveIndexedArgsPackValueBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
        std::string accessName;
        if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
            targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
          return false;
        }
        const std::string &targetName = targetExpr.args.front().name;
        auto resolveBinding = [&](const BindingInfo &binding) {
          std::string packElemType;
          std::string base;
          return getArgsPackElementType(binding, packElemType) &&
                 splitTemplateTypeName(normalizeBindingTypeName(packElemType), base, elemTypeOut) &&
                 normalizeBindingTypeName(base) == "Buffer";
        };
        if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
          return resolveBinding(*paramBinding);
        }
        auto it = locals.find(targetName);
        return it != locals.end() && resolveBinding(it->second);
      };
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName == "Buffer" && !paramBinding->typeTemplateArg.empty()) {
            elemType = paramBinding->typeTemplateArg;
            return true;
          }
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          if (it->second.typeName == "Buffer" && !it->second.typeTemplateArg.empty()) {
            elemType = it->second.typeTemplateArg;
            return true;
          }
        }
        return false;
      }
      if (target.kind == Expr::Kind::Call) {
        if (resolveIndexedArgsPackValueBuffer(target, elemType)) {
          return true;
        }
        if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
          const Expr &innerTarget = target.args.front();
          if (innerTarget.kind == Expr::Kind::Name) {
            if (const BindingInfo *paramBinding = findParamBinding(params, innerTarget.name)) {
              if (resolveReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemType)) {
                return true;
              }
            }
            auto it = locals.find(innerTarget.name);
            if (it != locals.end() &&
                resolveReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemType)) {
              return true;
            }
          }
          if (resolveIndexedArgsPackReferenceBuffer(innerTarget, elemType)) {
            return true;
          }
        }
        if (isSimpleCallName(target, "buffer") && target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
        if (isSimpleCallName(target, "upload") && target.args.size() == 1) {
          return resolveArrayTarget(target.args.front(), elemType);
        }
      }
      return false;
    };
    auto extractExperimentalMapFieldTypes = [&](const BindingInfo &binding,
                                                std::string &keyTypeOut,
                                                std::string &valueTypeOut) -> bool {
      auto extractFromTypeText = [&](std::string normalizedType) -> bool {
        while (true) {
          std::string base;
          std::string argText;
          if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
            base = normalizeBindingTypeName(base);
            if (base == "Reference" || base == "Pointer") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
                return false;
              }
              normalizedType = normalizeBindingTypeName(args.front());
              continue;
            }
            std::string normalizedBase = base;
            if (!normalizedBase.empty() && normalizedBase.front() == '/') {
              normalizedBase.erase(normalizedBase.begin());
            }
            if (normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map") {
              std::vector<std::string> args;
              if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
                return false;
              }
              keyTypeOut = args[0];
              valueTypeOut = args[1];
              return true;
            }
          }

          std::string resolvedPath = normalizedType;
          if (!resolvedPath.empty() && resolvedPath.front() != '/') {
            resolvedPath.insert(resolvedPath.begin(), '/');
          }
          std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
          if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
            normalizedResolvedPath.erase(normalizedResolvedPath.begin());
          }
          if (normalizedResolvedPath.rfind("std/collections/experimental_map/Map__", 0) != 0) {
            return false;
          }
          auto defIt = defMap_.find(resolvedPath);
          if (defIt == defMap_.end() || !defIt->second) {
            return false;
          }
          std::string keysElementType;
          std::string payloadsElementType;
          for (const auto &fieldExpr : defIt->second->statements) {
            if (!fieldExpr.isBinding) {
              continue;
            }
            BindingInfo fieldBinding;
            std::optional<std::string> restrictType;
            std::string parseError;
            if (!parseBindingInfo(fieldExpr,
                                  defIt->second->namespacePrefix,
                                  structNames_,
                                  importAliases_,
                                  fieldBinding,
                                  restrictType,
                                  parseError)) {
              continue;
            }
            if (normalizeBindingTypeName(fieldBinding.typeName) != "vector" || fieldBinding.typeTemplateArg.empty()) {
              continue;
            }
            std::vector<std::string> fieldArgs;
            if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, fieldArgs) || fieldArgs.size() != 1) {
              continue;
            }
            if (fieldExpr.name == "keys") {
              keysElementType = fieldArgs.front();
            } else if (fieldExpr.name == "payloads") {
              payloadsElementType = fieldArgs.front();
            }
          }
          if (keysElementType.empty() || payloadsElementType.empty()) {
            return false;
          }
          keyTypeOut = keysElementType;
          valueTypeOut = payloadsElementType;
          return true;
        }
      };

      keyTypeOut.clear();
      valueTypeOut.clear();
      if (binding.typeTemplateArg.empty()) {
        return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
      }
      return extractFromTypeText(normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
    };
    auto resolveExperimentalMapTarget = [&](const Expr &target,
                                            std::string &keyTypeOut,
                                            std::string &valueTypeOut) -> bool {
      keyTypeOut.clear();
      valueTypeOut.clear();
      auto isDirectMapConstructorCall = [&](const Expr &candidate) {
        if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
          return false;
        }
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto matchesPath = [&](std::string_view basePath) {
          return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
        };
        return matchesPath("/std/collections/map/map") ||
               matchesPath("/std/collections/mapNew") ||
               matchesPath("/std/collections/mapSingle") ||
               matchesPath("/std/collections/mapDouble") ||
               matchesPath("/std/collections/mapPair") ||
               matchesPath("/std/collections/mapTriple") ||
               matchesPath("/std/collections/mapQuad") ||
               matchesPath("/std/collections/mapQuint") ||
               matchesPath("/std/collections/mapSext") ||
               matchesPath("/std/collections/mapSept") ||
               matchesPath("/std/collections/mapOct") ||
               matchesPath("/std/collections/experimental_map/mapNew") ||
               matchesPath("/std/collections/experimental_map/mapSingle") ||
               matchesPath("/std/collections/experimental_map/mapDouble") ||
               matchesPath("/std/collections/experimental_map/mapPair") ||
               matchesPath("/std/collections/experimental_map/mapTriple") ||
               matchesPath("/std/collections/experimental_map/mapQuad") ||
               matchesPath("/std/collections/experimental_map/mapQuint") ||
               matchesPath("/std/collections/experimental_map/mapSext") ||
               matchesPath("/std/collections/experimental_map/mapSept") ||
               matchesPath("/std/collections/experimental_map/mapOct");
      };
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractExperimentalMapFieldTypes(*paramBinding, keyTypeOut, valueTypeOut);
        }
        auto it = locals.find(target.name);
        return it != locals.end() &&
               extractExperimentalMapFieldTypes(it->second, keyTypeOut, valueTypeOut);
      }
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      if (isDirectMapConstructorCall(target)) {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
          keyTypeOut = args[0];
          valueTypeOut = args[1];
          return true;
        }
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferredReturn;
      return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
             extractExperimentalMapFieldTypes(inferredReturn, keyTypeOut, valueTypeOut);
    };
    auto resolveFieldBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
      if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
        return false;
      }
      std::string structPath;
      const Expr &receiver = target.args.front();
      if (receiver.kind == Expr::Kind::Name) {
        const BindingInfo *receiverBinding = findParamBinding(params, receiver.name);
        if (!receiverBinding) {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            receiverBinding = &it->second;
          }
        }
        if (receiverBinding != nullptr) {
          std::string receiverType = receiverBinding->typeName;
          if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverBinding->typeTemplateArg.empty()) {
            receiverType = receiverBinding->typeTemplateArg;
          }
          structPath = resolveStructTypePath(receiverType, receiver.namespacePrefix, structNames_);
          if (structPath.empty()) {
            auto importIt = importAliases_.find(receiverType);
            if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
              structPath = importIt->second;
            }
          }
        }
      } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
        std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
        if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
          structPath = inferredStruct;
        } else {
          const std::string resolvedType = resolveCalleePath(receiver);
          if (structNames_.count(resolvedType) > 0) {
            structPath = resolvedType;
          }
        }
      }
      if (structPath.empty()) {
        return false;
      }
      auto defIt = defMap_.find(structPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &fieldStmt : defIt->second->statements) {
        bool isStaticField = false;
        for (const auto &transform : fieldStmt.transforms) {
          if (transform.name == "static") {
            isStaticField = true;
            break;
          }
        }
        if (!fieldStmt.isBinding || isStaticField || fieldStmt.name != target.name) {
          continue;
        }
        return resolveStructFieldBinding(*defIt->second, fieldStmt, bindingOut);
      }
      return false;
    };
    auto resolveExperimentalMapValueTarget = [&](const Expr &target,
                                                 std::string &keyTypeOut,
                                                 std::string &valueTypeOut) -> bool {
      auto extractValueBinding = [&](const BindingInfo &binding) {
        const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
        if (normalizedType == "Reference" || normalizedType == "Pointer") {
          return false;
        }
        return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
      };
      keyTypeOut.clear();
      valueTypeOut.clear();
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return extractValueBinding(*paramBinding);
        }
        auto it = locals.find(target.name);
        return it != locals.end() && extractValueBinding(it->second);
      }
      BindingInfo fieldBinding;
      if (resolveFieldBindingTarget(target, fieldBinding)) {
        return extractValueBinding(fieldBinding);
      }
      if (target.kind != Expr::Kind::Call) {
        return false;
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferredReturn;
      return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
             extractValueBinding(inferredReturn);
    };
    auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {
      if (helperName == "count") {
        return std::string("mapCount");
      }
      if (helperName == "contains") {
        return std::string("mapContains");
      }
      if (helperName == "tryAt") {
        return std::string("mapTryAt");
      }
      if (helperName == "at") {
        return std::string("mapAt");
      }
      if (helperName == "at_unsafe") {
        return std::string("mapAtUnsafe");
      }
      return std::string(helperName);
    };
    auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {
      return "/std/collections/experimental_map/" + preferredExperimentalMapHelperTarget(helperName);
    };
    std::function<ReturnKind(const Expr &)> pointerTargetKind = [&](const Expr &pointerExpr) -> ReturnKind {
      if (pointerExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
          if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
              !paramBinding->typeTemplateArg.empty()) {
            return referenceTargetKind(paramBinding->typeTemplateArg, pointerExpr.namespacePrefix);
          }
          return ReturnKind::Unknown;
        }
        auto it = locals.find(pointerExpr.name);
        if (it != locals.end()) {
          if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
              !it->second.typeTemplateArg.empty()) {
            return referenceTargetKind(it->second.typeTemplateArg, pointerExpr.namespacePrefix);
          }
        }
        return ReturnKind::Unknown;
      }
      if (pointerExpr.kind == Expr::Kind::Call) {
        std::string pointerName;
        if (getBuiltinPointerName(pointerExpr, pointerName) && pointerName == "location" && pointerExpr.args.size() == 1) {
          const Expr &target = pointerExpr.args.front();
          if (target.kind != Expr::Kind::Name) {
            return ReturnKind::Unknown;
          }
          if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
            if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
              return referenceTargetKind(paramBinding->typeTemplateArg, target.namespacePrefix);
            }
            return returnKindForTypeName(paramBinding->typeName);
          }
          auto it = locals.find(target.name);
          if (it != locals.end()) {
            if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
              return referenceTargetKind(it->second.typeTemplateArg, target.namespacePrefix);
            }
            return returnKindForTypeName(it->second.typeName);
          }
        }
        if (getBuiltinMemoryName(pointerExpr, pointerName)) {
          if (pointerName == "alloc" && pointerExpr.templateArgs.size() == 1 && pointerExpr.args.size() == 1) {
            return referenceTargetKind(pointerExpr.templateArgs.front(), pointerExpr.namespacePrefix);
          }
          if (pointerName == "realloc" && pointerExpr.args.size() == 2) {
            return pointerTargetKind(pointerExpr.args.front());
          }
          if (pointerName == "at" && pointerExpr.templateArgs.empty() && pointerExpr.args.size() == 3) {
            return pointerTargetKind(pointerExpr.args.front());
          }
          if (pointerName == "at_unsafe" && pointerExpr.templateArgs.empty() && pointerExpr.args.size() == 2) {
            return pointerTargetKind(pointerExpr.args.front());
          }
        }
        std::string opName;
        if (getBuiltinOperatorName(pointerExpr, opName) && (opName == "plus" || opName == "minus") &&
            pointerExpr.args.size() == 2) {
          ReturnKind leftKind = pointerTargetKind(pointerExpr.args[0]);
          if (leftKind != ReturnKind::Unknown) {
            return leftKind;
          }
          return pointerTargetKind(pointerExpr.args[1]);
        }
      }
      return ReturnKind::Unknown;
    };
    auto resolveMethodCallPath = [&](const std::string &methodName, std::string &resolvedOut) -> bool {
      auto resolveStructTypePath = [&](const std::string &typeName,
                                       const std::string &namespacePrefix) -> std::string {
        if (typeName.empty()) {
          return "";
        }
        if (!typeName.empty() && typeName[0] == '/') {
          return typeName;
        }
        std::string current = namespacePrefix;
        while (true) {
          if (!current.empty()) {
            std::string scoped = current + "/" + typeName;
            if (structNames_.count(scoped) > 0) {
              return scoped;
            }
            if (current.size() > typeName.size()) {
              const size_t start = current.size() - typeName.size();
              if (start > 0 && current[start - 1] == '/' &&
                  current.compare(start, typeName.size(), typeName) == 0 &&
                  structNames_.count(current) > 0) {
                return current;
              }
            }
          } else {
            std::string root = "/" + typeName;
            if (structNames_.count(root) > 0) {
              return root;
            }
          }
          if (current.empty()) {
            break;
          }
          const size_t slash = current.find_last_of('/');
          if (slash == std::string::npos || slash == 0) {
            current.clear();
          } else {
            current.erase(slash);
          }
        }
        auto importIt = importAliases_.find(typeName);
        if (importIt != importAliases_.end()) {
          return importIt->second;
        }
        return "";
      };
      if (expr.args.empty()) {
        return false;
      }
      const Expr &receiver = expr.args.front();
      std::string typeName;
      std::string typeTemplateArg;
      std::string normalizedMethodName = methodName;
      if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
        normalizedMethodName.erase(normalizedMethodName.begin());
      }
      if (normalizedMethodName.rfind("vector/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
      } else if (normalizedMethodName.rfind("array/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
      } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
      } else if (normalizedMethodName.rfind("map/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
      } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
        normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
      }
      auto preferVectorStdlibHelperPathForCall = [&](const std::string &path) -> std::string {
        auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
          return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
                 suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
                 suffix != "remove_at" && suffix != "remove_swap";
        };
        auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
          return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
                 suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
                 suffix != "remove_at" && suffix != "remove_swap";
        };
        std::string preferred = path;
        if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
          const std::string suffix = preferred.substr(std::string("/array/").size());
          if (allowsArrayVectorCompatibilitySuffix(suffix)) {
            const std::string vectorAlias = "/vector/" + suffix;
            if (defMap_.count(vectorAlias) > 0) {
              return vectorAlias;
            }
            const std::string stdlibAlias = "/std/collections/vector/" + suffix;
            if (defMap_.count(stdlibAlias) > 0) {
              return stdlibAlias;
            }
          }
        }
        if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
          const std::string suffix = preferred.substr(std::string("/vector/").size());
          if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
            const std::string stdlibAlias = "/std/collections/vector/" + suffix;
            if (defMap_.count(stdlibAlias) > 0) {
              preferred = stdlibAlias;
            } else {
              if (allowsArrayVectorCompatibilitySuffix(suffix)) {
                const std::string arrayAlias = "/array/" + suffix;
                if (defMap_.count(arrayAlias) > 0) {
                  preferred = arrayAlias;
                }
              }
            }
          }
        }
        if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
          const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
          if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
            const std::string vectorAlias = "/vector/" + suffix;
            if (defMap_.count(vectorAlias) > 0) {
              preferred = vectorAlias;
            } else {
              if (allowsArrayVectorCompatibilitySuffix(suffix)) {
                const std::string arrayAlias = "/array/" + suffix;
                if (defMap_.count(arrayAlias) > 0) {
                  preferred = arrayAlias;
                }
              }
            }
          }
        }
        if (preferred.rfind("/map/", 0) == 0 && defMap_.count(preferred) == 0) {
          const std::string suffix = preferred.substr(std::string("/map/").size());
          if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
            const std::string stdlibAlias = "/std/collections/map/" + suffix;
            if (defMap_.count(stdlibAlias) > 0) {
              preferred = stdlibAlias;
            }
          }
        }
        if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
          const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
          if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
              suffix != "at" && suffix != "at_unsafe") {
            const std::string mapAlias = "/map/" + suffix;
            if (defMap_.count(mapAlias) > 0) {
              preferred = mapAlias;
            }
          }
        }
        return preferred;
      };
      auto inferPointerLikeCallReturnType = [&](const Expr &receiverExpr) -> std::string {
        if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
          return "";
        }
        const std::string callPath = preferVectorStdlibHelperPathForCall(resolveCalleePath(receiverExpr));
        if (callPath.empty()) {
          return "";
        }
        auto defIt = defMap_.find(callPath);
        if (defIt == defMap_.end() || defIt->second == nullptr) {
          return "";
        }
        for (const auto &transform : defIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          std::string base;
          std::string arg;
          if (!splitTemplateTypeName(transform.templateArgs.front(), base, arg)) {
            return "";
          }
          if (base == "Pointer") {
            return "Pointer";
          }
          if (base == "Reference") {
            return "Reference";
          }
          return "";
        }
        return "";
      };
      auto preferredMapMethodTargetForCall = [&](const Expr &receiverExpr, const std::string &helperName) {
        std::string keyType;
        std::string valueType;
        if (resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
          const std::string canonical = "/std/collections/map/" + helperName;
          if (defMap_.count(canonical) > 0 || hasImportedDefinitionPath(canonical)) {
            return canonical;
          }
          return preferredCanonicalExperimentalMapHelperTarget(helperName);
        }
        const std::string canonical = "/std/collections/map/" + helperName;
        const std::string alias = "/map/" + helperName;
        if (defMap_.count(canonical) > 0 || hasImportedDefinitionPath(canonical)) {
          return canonical;
        }
        if (defMap_.count(alias) > 0) {
          return alias;
        }
        return canonical;
      };

      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        const std::string resolvedType = resolveCalleePath(receiver);
        if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Name &&
          findParamBinding(params, receiver.name) == nullptr &&
          locals.find(receiver.name) == locals.end()) {
        std::string resolvedReceiverPath;
        const std::string rootReceiverPath = "/" + receiver.name;
        if (defMap_.find(rootReceiverPath) != defMap_.end()) {
          resolvedReceiverPath = rootReceiverPath;
        } else {
          auto importIt = importAliases_.find(receiver.name);
          if (importIt != importAliases_.end()) {
            resolvedReceiverPath = importIt->second;
          }
        }
        if (!resolvedReceiverPath.empty() &&
            (structNames_.count(resolvedReceiverPath) > 0 ||
             defMap_.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap_.end())) {
          resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
          return true;
        }
        const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix);
        if (!resolvedType.empty()) {
          resolvedOut = resolvedType + "/" + normalizedMethodName;
          return true;
        }
      }
      {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (normalizedMethodName == "count") {
          if (resolveArgsPackCountTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/array/count");
            return true;
          }
          if (resolveVectorTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/vector/count");
            return true;
          }
          if (resolveSoaVectorTarget(receiver, elemType)) {
            resolvedOut = "/soa_vector/count";
            return true;
          }
          if (resolveArrayTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/array/count");
            return true;
          }
          if (resolveStringTarget(receiver)) {
            resolvedOut = "/string/count";
            return true;
          }
        }
        if (normalizedMethodName == "capacity" && resolveVectorTarget(receiver, elemType)) {
          resolvedOut = preferVectorStdlibHelperPathForCall("/vector/capacity");
          return true;
        }
        if (normalizedMethodName == "count" && resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferredMapMethodTargetForCall(receiver, "count");
          return true;
        }
        if ((normalizedMethodName == "contains" || normalizedMethodName == "tryAt") &&
            resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
          return true;
        }
        if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
          if (resolveArgsPackAccessTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/array/" + normalizedMethodName);
            return true;
          }
          if (resolveVectorTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/vector/" + normalizedMethodName);
            return true;
          }
          if (resolveArrayTarget(receiver, elemType)) {
            resolvedOut = preferVectorStdlibHelperPathForCall("/array/" + normalizedMethodName);
            return true;
          }
          if (resolveStringTarget(receiver)) {
            resolvedOut = "/string/" + normalizedMethodName;
            return true;
          }
        }
        if ((normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
            resolveMapTarget(receiver, keyType, valueType)) {
          resolvedOut = preferredMapMethodTargetForCall(receiver, normalizedMethodName);
          return true;
        }
        if ((normalizedMethodName == "get" || normalizedMethodName == "ref") &&
            resolveSoaVectorTarget(receiver, elemType)) {
          resolvedOut = "/soa_vector/" + normalizedMethodName;
          return true;
        }
      }
      if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
          typeTemplateArg = paramBinding->typeTemplateArg;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
            typeTemplateArg = it->second.typeTemplateArg;
          }
        }
      }
      if (typeName.empty()) {
        typeName = inferPointerLikeCallReturnType(receiver);
      }
      if (typeName.empty()) {
        ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
        std::string inferred;
        if (inferredKind == ReturnKind::Array) {
          inferred = inferStructReturnPath(receiver, params, locals);
          if (inferred.empty()) {
            inferred = typeNameForReturnKind(inferredKind);
          }
        } else {
          inferred = typeNameForReturnKind(inferredKind);
        }
        if (!inferred.empty()) {
          typeName = inferred;
        }
      }
      if (typeName.empty()) {
        return false;
      }
      if (typeName == "FileError" && normalizedMethodName == "why") {
        resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
        return true;
      }
      if (typeName == "Pointer" || typeName == "Reference") {
        if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
          resolvedOut = "/" + typeName + "/" + normalizedMethodName;
          return true;
        }
        return false;
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        resolvedOut = "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName;
        return true;
      }
      std::string resolvedType = resolveStructTypePath(typeName, expr.namespacePrefix);
      if (resolvedType.empty()) {
        resolvedType = resolveTypePath(typeName, expr.namespacePrefix);
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    };
    auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {
      if (isSimpleCallName(candidate, helper)) {
        return true;
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (!getNamespacedCollectionHelperName(candidate, namespacedCollection, namespacedHelper)) {
        return false;
      }
      if (!(namespacedCollection == "vector" && namespacedHelper == helper)) {
        return false;
      }
      if ((namespacedHelper != "count" && namespacedHelper != "capacity") ||
          resolveCalleePath(candidate) != "/std/collections/vector/" + namespacedHelper) {
        return true;
      }
      return defMap_.find("/std/collections/vector/" + namespacedHelper) != defMap_.end() ||
             defMap_.find("/vector/" + namespacedHelper) != defMap_.end();
    };
    auto isArrayNamespacedVectorCountCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      const bool spellsArrayCount = (normalized == "array/count");
      const bool resolvesArrayCount = (resolveCalleePath(candidate) == "/array/count");
      if (!spellsArrayCount && !resolvesArrayCount) {
        return false;
      }
      for (const Expr &arg : candidate.args) {
        std::string elemType;
        if (resolveVectorTarget(arg, elemType)) {
          return true;
        }
      }
      return false;
    };
    auto isMapNamespacedCountCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      const bool spellsMapCount = (normalized == "map/count");
      const bool resolvesMapCount = (resolveCalleePath(candidate) == "/map/count");
      if (!spellsMapCount && !resolvesMapCount) {
        return false;
      }
      if (defMap_.find("/map/count") != defMap_.end()) {
        return false;
      }
      for (const Expr &arg : candidate.args) {
        std::string keyType;
        std::string valueType;
        if (resolveMapTarget(arg, keyType, valueType)) {
          return true;
        }
      }
      return false;
    };
    const bool shouldInferBuiltinBareMapCountCall = true;
    auto isUnnamespacedMapCountBuiltinFallbackCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      const bool spellsCount = (normalized == "count");
      const bool resolvesCount = (resolveCalleePath(candidate) == "/count");
      if (!spellsCount && !resolvesCount) {
        return false;
      }
      if (defMap_.find("/count") != defMap_.end()) {
        return false;
      }
      if (candidate.args.empty()) {
        return false;
      }
      size_t receiverIndex = 0;
      if (hasNamedArguments(candidate.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            receiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          receiverIndex = 0;
        }
      }
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      if (!shouldInferBuiltinBareMapCountCall) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
    };
    auto isMapNamespacedAccessCompatibilityCall = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.name.empty()) {
        return false;
      }
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      if (normalized != "map/at" && normalized != "map/at_unsafe") {
        std::string namespacePrefix = candidate.namespacePrefix;
        if (!namespacePrefix.empty() && namespacePrefix.front() == '/') {
          namespacePrefix.erase(namespacePrefix.begin());
        }
        if (namespacePrefix == "map" &&
            (normalized == "at" || normalized == "at_unsafe")) {
          normalized = "map/" + normalized;
        }
      }
      if (normalized != "map/at" && normalized != "map/at_unsafe") {
        const std::string resolvedPath = resolveCalleePath(candidate);
        if (resolvedPath == "/map/at") {
          normalized = "map/at";
        } else if (resolvedPath == "/map/at_unsafe") {
          normalized = "map/at_unsafe";
        } else {
          return false;
        }
      }
      return defMap_.find("/" + normalized) == defMap_.end();
    };
    auto hasDeclaredDefinitionPath = [&](const std::string &path) {
      for (const auto &def : program_.definitions) {
        if (def.fullPath == path) {
          return true;
        }
      }
      return false;
    };
    auto explicitRemovedCollectionMethodPath = [&](const Expr &candidate) -> std::string {
      if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name.empty() ||
          candidate.args.empty()) {
        return "";
      }
      auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
        return helperName == "count" || helperName == "capacity" || helperName == "at" ||
               helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
               helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
               helperName == "remove_swap";
      };
      auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
        return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
      };
      std::string normalized = candidate.name;
      if (!normalized.empty() && normalized.front() == '/') {
        normalized.erase(normalized.begin());
      }
      std::string normalizedPrefix = candidate.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      std::string_view helperName;
      bool isStdNamespacedVectorHelper = false;
      std::string compatibilityCollection;
      if (normalizedPrefix == "array") {
        helperName = normalized;
        compatibilityCollection = "array";
      } else if (normalizedPrefix == "vector") {
        helperName = normalized;
        compatibilityCollection = "vector";
      } else if (normalizedPrefix == "std/collections/vector") {
        helperName = normalized;
        isStdNamespacedVectorHelper = true;
        compatibilityCollection = "vector";
      } else if (normalizedPrefix == "map") {
        helperName = normalized;
        compatibilityCollection = "map";
      } else if (normalizedPrefix == "std/collections/map") {
        helperName = normalized;
        compatibilityCollection = "map";
      } else if (normalized.rfind("array/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("array/").size());
        compatibilityCollection = "array";
      } else if (normalized.rfind("vector/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("vector/").size());
        compatibilityCollection = "vector";
      } else if (normalized.rfind("std/collections/vector/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("std/collections/vector/").size());
        isStdNamespacedVectorHelper = true;
        compatibilityCollection = "vector";
      } else if (normalized.rfind("map/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("map/").size());
        compatibilityCollection = "map";
      } else if (normalized.rfind("std/collections/map/", 0) == 0) {
        helperName = std::string_view(normalized).substr(std::string_view("std/collections/map/").size());
        compatibilityCollection = "map";
      }
      if (helperName.empty()) {
        return "";
      }
      if (compatibilityCollection == "map") {
        if (!isRemovedMapCompatibilityHelper(helperName)) {
          return "";
        }
        const std::string removedPath = "/map/" + std::string(helperName);
        if (defMap_.find(removedPath) != defMap_.end()) {
          return "";
        }
        std::string keyType;
        std::string valueType;
        if (!resolveMapTarget(candidate.args.front(), keyType, valueType)) {
          return "";
        }
        return removedPath;
      }
      if (!isRemovedVectorCompatibilityHelper(helperName)) {
        return "";
      }
      std::string elemType;
      if (!resolveVectorTarget(candidate.args.front(), elemType) &&
          !resolveArrayTarget(candidate.args.front(), elemType) &&
          !resolveSoaVectorTarget(candidate.args.front(), elemType)) {
        return "";
      }
      if (isStdNamespacedVectorHelper) {
        return "/vector/" + std::string(helperName);
      }
      return "/" + normalized;
    };
    auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      auto matchesHelper = [&](const char *helper) -> bool {
        return isVectorBuiltinName(candidate, helper);
      };
      if (matchesHelper("push")) {
        nameOut = "push";
        return true;
      }
      if (matchesHelper("pop")) {
        nameOut = "pop";
        return true;
      }
      if (matchesHelper("reserve")) {
        nameOut = "reserve";
        return true;
      }
      if (matchesHelper("clear")) {
        nameOut = "clear";
        return true;
      }
      if (matchesHelper("remove_at")) {
        nameOut = "remove_at";
        return true;
      }
      if (matchesHelper("remove_swap")) {
        nameOut = "remove_swap";
        return true;
      }
      return false;
    };
    auto resolveVectorHelperMethodTarget = [&](const Expr &receiver,
                                               const std::string &helperName,
                                               std::string &resolvedOut) -> bool {
      resolvedOut.clear();
      auto resolveReceiverTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
        if (typeName.empty()) {
          return "";
        }
        if (isPrimitiveBindingTypeName(typeName)) {
          return "/" + normalizeBindingTypeName(typeName);
        }
        std::string resolvedType = resolveTypePath(typeName, namespacePrefix);
        if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
          auto importIt = importAliases_.find(typeName);
          if (importIt != importAliases_.end()) {
            resolvedType = importIt->second;
          }
        }
        return resolvedType;
      };
      if (receiver.kind == Expr::Kind::Name) {
        std::string typeName;
        if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
          typeName = paramBinding->typeName;
        } else {
          auto it = locals.find(receiver.name);
          if (it != locals.end()) {
            typeName = it->second.typeName;
          }
        }
        if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
          return false;
        }
        const std::string resolvedType = resolveReceiverTypePath(typeName, receiver.namespacePrefix);
        if (resolvedType.empty()) {
          return false;
        }
        resolvedOut = resolvedType + "/" + helperName;
        return true;
      }
      if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
        std::string resolvedType = resolveCalleePath(receiver);
        if (resolvedType.empty() || structNames_.count(resolvedType) == 0) {
          resolvedType = inferStructReturnPath(receiver, params, locals);
        }
        if (resolvedType.empty()) {
          return false;
        }
        resolvedOut = resolvedType + "/" + helperName;
        return true;
      }
      return false;
    };
    auto inferResolvedPathReturnKind = [&](const std::string &resolvedPath, ReturnKind &kindOut) -> bool {
      auto methodIt = defMap_.find(resolvedPath);
      if (methodIt == defMap_.end()) {
        return false;
      }
      if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
        return false;
      }
      auto kindIt = returnKinds_.find(resolvedPath);
      if (kindIt == returnKinds_.end()) {
        return false;
      }
      kindOut = kindIt->second;
      return true;
    };
    auto preferVectorStdlibHelperPath = [&](const std::string &path) -> std::string {
      auto allowsArrayVectorCompatibilitySuffix = [](const std::string &suffix) {
        return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
               suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
               suffix != "remove_at" && suffix != "remove_swap";
      };
      auto allowsVectorStdlibCompatibilitySuffix = [](const std::string &suffix) {
        return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
               suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
               suffix != "remove_at" && suffix != "remove_swap";
      };
      std::string preferred = path;
      if (preferred.rfind("/array/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/array/").size());
        if (allowsArrayVectorCompatibilitySuffix(suffix)) {
          const std::string vectorAlias = "/vector/" + suffix;
          if (defMap_.count(vectorAlias) > 0) {
            return vectorAlias;
          }
          const std::string stdlibAlias = "/std/collections/vector/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            return stdlibAlias;
          }
        }
      }
      if (preferred.rfind("/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/vector/").size());
        if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
          const std::string stdlibAlias = "/std/collections/vector/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          } else {
            if (allowsArrayVectorCompatibilitySuffix(suffix)) {
              const std::string arrayAlias = "/array/" + suffix;
              if (defMap_.count(arrayAlias) > 0) {
                preferred = arrayAlias;
              }
            }
          }
        }
      }
      if (preferred.rfind("/std/collections/vector/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
        if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
          const std::string vectorAlias = "/vector/" + suffix;
          if (defMap_.count(vectorAlias) > 0) {
            preferred = vectorAlias;
          } else {
            if (allowsArrayVectorCompatibilitySuffix(suffix)) {
              const std::string arrayAlias = "/array/" + suffix;
              if (defMap_.count(arrayAlias) > 0) {
                preferred = arrayAlias;
              }
            }
          }
        }
      }
      if (preferred.rfind("/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/map/").size());
        if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
          const std::string stdlibAlias = "/std/collections/map/" + suffix;
          if (defMap_.count(stdlibAlias) > 0) {
            preferred = stdlibAlias;
          }
        }
      }
      if (preferred.rfind("/std/collections/map/", 0) == 0 && defMap_.count(preferred) == 0) {
        const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
        if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
            suffix != "at" && suffix != "at_unsafe") {
          const std::string mapAlias = "/map/" + suffix;
          if (defMap_.count(mapAlias) > 0) {
            preferred = mapAlias;
          }
        }
      }
      return preferred;
    };
    auto collectionHelperPathCandidates = [&](const std::string &path) {
      std::vector<std::string> candidates;
      auto appendUnique = [&](const std::string &candidate) {
        if (candidate.empty()) {
          return;
        }
        for (const auto &existing : candidates) {
          if (existing == candidate) {
            return;
          }
        }
        candidates.push_back(candidate);
      };

      std::string normalizedPath = path;
      if (!normalizedPath.empty() && normalizedPath.front() != '/') {
        if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
            normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
            normalizedPath.rfind("std/collections/map/", 0) == 0) {
          normalizedPath.insert(normalizedPath.begin(), '/');
        }
      }

      appendUnique(path);
      appendUnique(normalizedPath);
      if (normalizedPath.rfind("/array/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/array/").size());
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/vector/" + suffix);
          appendUnique("/std/collections/vector/" + suffix);
        }
      } else if (normalizedPath.rfind("/vector/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
            suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
            suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/std/collections/vector/" + suffix);
        }
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/array/" + suffix);
        }
      } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
        appendUnique("/vector/" + suffix);
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/array/" + suffix);
        }
      } else if (normalizedPath.rfind("/map/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/map/").size());
        if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
          appendUnique("/std/collections/map/" + suffix);
        }
      } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
        if (suffix != "map" && suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
            suffix != "at" && suffix != "at_unsafe") {
          appendUnique("/map/" + suffix);
        }
      }
      return candidates;
    };
    auto isTemplateCompatibleDefinition = [&](const Definition &def) -> bool {
      if (expr.templateArgs.empty()) {
        return true;
      }
      return !def.templateArgs.empty() && def.templateArgs.size() == expr.templateArgs.size();
    };
    auto explicitMapHelperReceiverIndex = [&](const Expr &candidate) -> size_t {
      size_t receiverIndex = 0;
      if (hasNamedArguments(candidate.argNames)) {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            return i;
          }
        }
      }
      return receiverIndex;
    };
    auto canonicalExperimentalMapHelperName = [&](const std::string &resolvedPath, std::string &helperNameOut) {
      auto matchHelper = [&](std::string_view canonicalPath,
                             std::string_view aliasPath,
                             std::string_view wrapperPath,
                             std::string_view helperName) {
        const std::string canonicalPrefix = std::string(canonicalPath) + "__t";
        const std::string aliasPrefix = std::string(aliasPath) + "__t";
        const std::string wrapperPrefix = std::string(wrapperPath) + "__t";
        if (resolvedPath == canonicalPath || resolvedPath.rfind(canonicalPrefix, 0) == 0 ||
            resolvedPath == aliasPath || resolvedPath.rfind(aliasPrefix, 0) == 0 ||
            resolvedPath == wrapperPath || resolvedPath.rfind(wrapperPrefix, 0) == 0) {
          helperNameOut = std::string(helperName);
          return true;
        }
        return false;
      };
      return matchHelper("/std/collections/map/count", "/map/count", "/std/collections/mapCount", "count") ||
             matchHelper("/std/collections/map/contains", "/map/contains", "/std/collections/mapContains", "contains") ||
             matchHelper("/std/collections/map/tryAt", "/map/tryAt", "/std/collections/mapTryAt", "tryAt") ||
             matchHelper("/std/collections/map/at", "/map/at", "/std/collections/mapAt", "at") ||
             matchHelper("/std/collections/map/at_unsafe", "/map/at_unsafe", "/std/collections/mapAtUnsafe", "at_unsafe");
    };
    auto canonicalizeExperimentalMapHelperResolvedPath = [&](const std::string &resolvedPath,
                                                             std::string &canonicalPathOut) {
      auto matchExperimentalHelper = [&](std::string_view experimentalPath, std::string_view canonicalPath) {
        const std::string experimentalPrefix = std::string(experimentalPath) + "__t";
        if (resolvedPath == experimentalPath || resolvedPath.rfind(experimentalPrefix, 0) == 0) {
          canonicalPathOut = std::string(canonicalPath);
          return true;
        }
        return false;
      };
      return matchExperimentalHelper("/std/collections/experimental_map/mapCount", "/std/collections/map/count") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapContains", "/std/collections/map/contains") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapTryAt", "/std/collections/map/tryAt") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapAt", "/std/collections/map/at") ||
             matchExperimentalHelper("/std/collections/experimental_map/mapAtUnsafe",
                                     "/std/collections/map/at_unsafe");
    };
    auto tryRewriteExplicitCanonicalExperimentalMapHelperCall = [&](const Expr &candidate, Expr &rewrittenOut) {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      const std::string resolvedPath = resolveCalleePath(candidate);
      std::string helperName;
      if (!canonicalExperimentalMapHelperName(resolvedPath, helperName)) {
        return false;
      }
      const size_t receiverIndex = explicitMapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      if (!resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType)) {
        return false;
      }
      rewrittenOut = candidate;
      rewrittenOut.name = preferredCanonicalExperimentalMapHelperTarget(helperName);
      rewrittenOut.namespacePrefix.clear();
      if (rewrittenOut.templateArgs.empty()) {
        rewrittenOut.templateArgs = {keyType, valueType};
      }
      return true;
    };
    auto isExplicitCanonicalExperimentalMapBorrowedHelperCall = [&](const Expr &candidate) {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      std::string helperName;
      if (!canonicalExperimentalMapHelperName(resolveCalleePath(candidate), helperName)) {
        return false;
      }
      const size_t receiverIndex = explicitMapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      return resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType) &&
             !resolveExperimentalMapValueTarget(candidate.args[receiverIndex], keyType, valueType);
    };
    std::string earlyPointerBuiltin;
    if (getBuiltinPointerName(expr, earlyPointerBuiltin) && expr.args.size() == 1) {
      if (earlyPointerBuiltin == "dereference") {
        ReturnKind targetKind = pointerTargetKind(expr.args.front());
        if (targetKind != ReturnKind::Unknown) {
          return targetKind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (expr.isMethodCall && !expr.args.empty() && expr.args.front().kind == Expr::Kind::Name &&
        expr.args.front().name == "Result") {
      if (expr.name == "error" && expr.args.size() == 2) {
        return ReturnKind::Bool;
      }
      if (expr.name == "why" && expr.args.size() == 2) {
        return ReturnKind::String;
      }
    }

    std::string resolvedCallee = resolveCalleePath(expr);
    std::string canonicalExperimentalMapHelperResolved;
    if (!expr.isMethodCall && defMap_.count(resolvedCallee) == 0 &&
        canonicalizeExperimentalMapHelperResolvedPath(resolvedCallee, canonicalExperimentalMapHelperResolved)) {
      resolvedCallee = canonicalExperimentalMapHelperResolved;
    }
    Expr rewrittenCanonicalExperimentalMapHelperCall;
    if (!expr.isMethodCall &&
        tryRewriteExplicitCanonicalExperimentalMapHelperCall(expr, rewrittenCanonicalExperimentalMapHelperCall)) {
      return inferExprReturnKind(rewrittenCanonicalExperimentalMapHelperCall, params, locals);
    }
    if (!expr.isMethodCall && isExplicitCanonicalExperimentalMapBorrowedHelperCall(expr)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isMapNamespacedCountCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isMapNamespacedAccessCompatibilityCall(expr)) {
      return ReturnKind::Unknown;
    }
    const std::string removedCollectionMethodCompatibilityPath =
        expr.isMethodCall ? explicitRemovedCollectionMethodPath(expr) : "";
    if (!removedCollectionMethodCompatibilityPath.empty()) {
      if (removedCollectionMethodCompatibilityPath == "/vector/at" ||
          removedCollectionMethodCompatibilityPath == "/vector/at_unsafe") {
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionMethodReturnKind(
                removedCollectionMethodCompatibilityPath,
                expr.args.front(),
                builtinCollectionDispatchResolvers,
                builtinMethodKind)) {
          return builtinMethodKind;
        }
      }
      return ReturnKind::Unknown;
    }
    const auto resolvedCandidates = collectionHelperPathCandidates(resolvedCallee);
    std::string resolved = resolvedCandidates.empty() ? preferVectorStdlibHelperPath(resolvedCallee)
                                                      : resolvedCandidates.front();
    bool hasResolvedPath = !expr.isMethodCall;
    const bool prefersCanonicalVectorCountAliasDefinition =
        !expr.isMethodCall && resolved == "/vector/count" && defMap_.find(resolved) == defMap_.end() &&
        defMap_.find("/std/collections/vector/count") != defMap_.end();
    if (prefersCanonicalVectorCountAliasDefinition) {
      resolved = "/std/collections/vector/count";
    }
    if (!expr.isMethodCall && !resolvedCandidates.empty()) {
      std::string firstTemplateCompatibleCandidate;
      for (const auto &candidate : resolvedCandidates) {
        auto candidateIt = defMap_.find(candidate);
        if (candidateIt == defMap_.end() || !candidateIt->second) {
          continue;
        }
        if (!isTemplateCompatibleDefinition(*candidateIt->second)) {
          continue;
        }
        if (firstTemplateCompatibleCandidate.empty()) {
          firstTemplateCompatibleCandidate = candidate;
        }
        if (!ensureDefinitionReturnKindReady(*candidateIt->second)) {
          return ReturnKind::Unknown;
        }
        auto candidateKindIt = returnKinds_.find(candidate);
        const bool candidateReturnsStruct =
            candidateKindIt != returnKinds_.end() && candidateKindIt->second == ReturnKind::Array;
        if (candidateReturnsStruct && returnStructs_.find(candidate) != returnStructs_.end()) {
          resolved = candidate;
          hasResolvedPath = true;
          break;
        }
      }
      if (!firstTemplateCompatibleCandidate.empty() && !expr.templateArgs.empty() &&
          returnStructs_.find(resolved) == returnStructs_.end()) {
        resolved = firstTemplateCompatibleCandidate;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall && expr.name == "ok" && expr.args.size() >= 1) {
      const Expr &receiver = expr.args.front();
      if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        return (expr.args.size() > 1) ? ReturnKind::Int64 : ReturnKind::Int;
      }
    }
    if (expr.isMethodCall && expr.name == "why" && expr.args.size() == 2) {
      const Expr &receiver = expr.args.front();
      if (receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
        return ReturnKind::String;
      }
    }
    if (expr.isMethodCall) {
      const std::string removedCollectionMethodPath = explicitRemovedCollectionMethodPath(expr);
      if (!removedCollectionMethodPath.empty()) {
        error_ = "unknown method: " + removedCollectionMethodPath;
        return ReturnKind::Unknown;
      }
    }
    if (expr.isMethodCall && isVectorBuiltinName(expr, "count") && expr.args.size() == 1 &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr)) {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      if (resolveArgsPackCountTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/vector/count");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveArrayTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/array/count");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      } else if (resolveStringTarget(expr.args.front())) {
        if (defMap_.find("/string/count") == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = "/string/count";
        hasResolvedPath = true;
      } else if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        const std::string methodPath =
            defMap_.find("/std/collections/map/count") != defMap_.end()
                ? "/std/collections/map/count"
                : (defMap_.find("/map/count") != defMap_.end() ? "/map/count"
                                                               : "/std/collections/map/count");
        resolved = methodPath;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall && isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1) {
      std::string elemType;
      if (resolveVectorTarget(expr.args.front(), elemType)) {
        const std::string methodPath = preferVectorStdlibHelperPath("/vector/capacity");
        if (defMap_.find(methodPath) == defMap_.end()) {
          return ReturnKind::Int;
        }
        resolved = methodPath;
        hasResolvedPath = true;
      }
    }
    if (expr.isMethodCall) {
      std::string methodResolved;
      if (resolveMethodCallPath(expr.name, methodResolved)) {
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        if (methodResolved == "/file_error/why" || methodResolved == "/FileError/why") {
          return ReturnKind::String;
        }
        if ((methodResolved == "/std/collections/map/count" &&
             hasImportedDefinitionPath("/std/collections/map/count")) ||
            (methodResolved == "/std/collections/map/contains" &&
             hasImportedDefinitionPath("/std/collections/map/contains")) ||
            (methodResolved == "/std/collections/map/tryAt" &&
             hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
            (methodResolved == "/std/collections/map/at" &&
             hasImportedDefinitionPath("/std/collections/map/at")) ||
            (methodResolved == "/std/collections/map/at_unsafe" &&
             hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) {
          ReturnKind builtinMethodKind = ReturnKind::Unknown;
          if (resolveBuiltinCollectionMethodReturnKind(
                  methodResolved, expr.args.front(), builtinCollectionDispatchResolvers, builtinMethodKind)) {
            return builtinMethodKind;
          }
        }
        std::string builtinMapKeyType;
        std::string builtinMapValueType;
        if (((methodResolved == "/std/collections/map/count" &&
              !hasDeclaredDefinitionPath("/map/count") &&
              !hasImportedDefinitionPath("/std/collections/map/count")) ||
             (methodResolved == "/std/collections/map/contains" &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains")) ||
             (methodResolved == "/std/collections/map/tryAt" &&
              !hasDeclaredDefinitionPath("/map/tryAt") &&
              !hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
             (methodResolved == "/std/collections/map/at" &&
              !hasDeclaredDefinitionPath("/map/at") &&
              !hasImportedDefinitionPath("/std/collections/map/at")) ||
             (methodResolved == "/std/collections/map/at_unsafe" &&
              !hasDeclaredDefinitionPath("/map/at_unsafe") &&
              !hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) &&
            !hasDeclaredDefinitionPath(methodResolved) &&
            !resolveMapTarget(expr.args.front(), builtinMapKeyType, builtinMapValueType)) {
          error_ = "unknown method: " + methodResolved;
          return ReturnKind::Unknown;
        }
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (defMap_.find(methodResolved) == defMap_.end() &&
            resolveBuiltinCollectionMethodReturnKind(
                methodResolved, expr.args.front(), builtinCollectionDispatchResolvers, builtinMethodKind)) {
          return builtinMethodKind;
        }
        if (defMap_.find(methodResolved) == defMap_.end() && !hasImportedDefinitionPath(methodResolved)) {
          error_ = "unknown method: " + methodResolved;
          return ReturnKind::Unknown;
        }
        resolved = methodResolved;
        hasResolvedPath = true;
      }
    }
    if (!expr.isMethodCall) {
      std::string vectorHelper;
      if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {
        std::string namespacedCollection;
        std::string namespacedHelper;
        const bool isNamespacedCollectionHelperCall =
            getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
        const bool isNamespacedVectorHelperCall =
            isNamespacedCollectionHelperCall && namespacedCollection == "vector";
        const bool isStdNamespacedVectorCanonicalHelperCall =
            !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/", 0) == 0 &&
            (namespacedHelper == "push" || namespacedHelper == "pop" || namespacedHelper == "reserve" ||
             namespacedHelper == "clear" || namespacedHelper == "remove_at" ||
             namespacedHelper == "remove_swap");
        const bool shouldAllowStdNamespacedVectorHelperCompatibilityFallback =
            isStdNamespacedVectorCanonicalHelperCall && !namespacedHelper.empty() &&
            defMap_.find("/vector/" + namespacedHelper) != defMap_.end();
        if ((defMap_.find(resolved) == defMap_.end() || isNamespacedVectorHelperCall) &&
            !(isStdNamespacedVectorCanonicalHelperCall && defMap_.find(resolved) == defMap_.end() &&
              !shouldAllowStdNamespacedVectorHelperCompatibilityFallback)) {
          auto isVectorHelperReceiverName = [&](const Expr &candidate) -> bool {
            if (candidate.kind != Expr::Kind::Name) {
              return false;
            }
            std::string typeName;
            if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
              typeName = normalizeBindingTypeName(paramBinding->typeName);
            } else {
              auto it = locals.find(candidate.name);
              if (it != locals.end()) {
                typeName = normalizeBindingTypeName(it->second.typeName);
              }
            }
            return typeName == "vector" || typeName == "soa_vector";
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
               (expr.args.front().kind == Expr::Kind::Name &&
                !isVectorHelperReceiverName(expr.args.front())));
          if (probePositionalReorderedReceiver) {
            for (size_t i = 1; i < expr.args.size(); ++i) {
              appendReceiverIndex(i);
            }
          }
          for (size_t receiverIndex : receiverIndices) {
            if (receiverIndex >= expr.args.size()) {
              continue;
            }
            const Expr &receiverCandidate = expr.args[receiverIndex];
            std::string methodResolved;
            if (!resolveVectorHelperMethodTarget(receiverCandidate, vectorHelper, methodResolved)) {
              continue;
            }
            methodResolved = preferVectorStdlibHelperPath(methodResolved);
            ReturnKind helperReturnKind = ReturnKind::Unknown;
            if (inferResolvedPathReturnKind(methodResolved, helperReturnKind)) {
              return helperReturnKind;
            }
          }
        }
      }
    }
    std::string namespacedCollection;
    std::string namespacedHelper;
    const bool isNamespacedCollectionHelperCall =
        getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper);
    const bool isNamespacedVectorHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "vector";
    const bool isNamespacedMapHelperCall =
        isNamespacedCollectionHelperCall && namespacedCollection == "map";
    const bool isStdNamespacedVectorCountCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/count", 0) == 0;
    const bool hasStdNamespacedVectorCountDefinition =
        hasImportedDefinitionPath("/std/collections/vector/count");
    const bool shouldBuiltinValidateStdNamespacedVectorCountCall =
        isStdNamespacedVectorCountCall && hasStdNamespacedVectorCountDefinition;
    const bool isStdNamespacedMapCountCall =
        !expr.isMethodCall && resolveCalleePath(expr) == "/std/collections/map/count";
    const bool isNamespacedVectorCountCall =
        !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "count" &&
        isVectorBuiltinName(expr, "count") && !isArrayNamespacedVectorCountCompatibilityCall(expr);
    const bool isNamespacedMapCountCall =
        !expr.isMethodCall && isNamespacedMapHelperCall && namespacedHelper == "count" &&
        !isMapNamespacedCountCompatibilityCall(expr) && !isStdNamespacedMapCountCall &&
        defMap_.find(resolved) == defMap_.end();
    const bool prefersExplicitDirectMapAccessAliasDefinition =
        !expr.isMethodCall &&
        (((isNamespacedMapHelperCall && (namespacedHelper == "at" || namespacedHelper == "at_unsafe")) ||
          (((expr.namespacePrefix == "map") || (expr.namespacePrefix == "/map")) &&
           (expr.name == "at" || expr.name == "at_unsafe")))) &&
        defMap_.find("/map/" +
                     ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : namespacedHelper)) !=
            defMap_.end();
    if (prefersExplicitDirectMapAccessAliasDefinition) {
      resolved = "/map/" + ((expr.name == "at" || expr.name == "at_unsafe") ? expr.name : namespacedHelper);
      hasResolvedPath = true;
    }
    const bool isUnnamespacedMapCountFallbackCall =
        !expr.isMethodCall && isUnnamespacedMapCountBuiltinFallbackCall(expr);
    const bool isResolvedMapCountCall =
        !expr.isMethodCall && resolved == "/map/count" &&
        !isMapNamespacedCountCompatibilityCall(expr) &&
        !isUnnamespacedMapCountFallbackCall;
    const bool isNamespacedVectorCapacityCall =
        !expr.isMethodCall && isNamespacedVectorHelperCall && namespacedHelper == "capacity" &&
        isVectorBuiltinName(expr, "capacity");
    const bool isStdNamespacedVectorCapacityCall =
        !expr.isMethodCall && resolveCalleePath(expr).rfind("/std/collections/vector/capacity", 0) == 0;
    const bool hasStdNamespacedVectorCapacityDefinition =
        hasImportedDefinitionPath("/std/collections/vector/capacity");
    const bool shouldBuiltinValidateStdNamespacedVectorCapacityCall =
        isStdNamespacedVectorCapacityCall && hasStdNamespacedVectorCapacityDefinition;
    std::string builtinAccessName;
    const bool hasBuiltinAccessSpelling = getBuiltinArrayAccessName(expr, builtinAccessName);
    const bool isStdNamespacedVectorAccessSpelling =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        resolveCalleePath(expr).rfind("/std/collections/vector/at", 0) == 0;
    const bool hasStdNamespacedVectorAccessDefinition =
        isStdNamespacedVectorAccessSpelling &&
        hasImportedDefinitionPath(resolveCalleePath(expr));
    const bool isStdNamespacedMapAccessSpelling =
        hasBuiltinAccessSpelling && !expr.isMethodCall &&
        (resolveCalleePath(expr) == "/std/collections/map/at" ||
         resolveCalleePath(expr) == "/std/collections/map/at_unsafe");
    const bool hasStdNamespacedMapAccessDefinition =
        isStdNamespacedMapAccessSpelling &&
        hasImportedDefinitionPath(resolveCalleePath(expr));
    const bool isResolvedMapAccessCall =
        !expr.isMethodCall && (resolved == "/map/at" || resolved == "/map/at_unsafe") &&
        !isMapNamespacedAccessCompatibilityCall(expr);
    const bool shouldAllowStdAccessCompatibilityFallback =
        isStdNamespacedVectorAccessSpelling && !builtinAccessName.empty() &&
        defMap_.find("/vector/" + builtinAccessName) != defMap_.end();
    const bool isBuiltinAccess =
        hasBuiltinAccessSpelling &&
        (!isStdNamespacedVectorAccessSpelling || shouldAllowStdAccessCompatibilityFallback ||
         hasStdNamespacedVectorAccessDefinition) &&
        !isStdNamespacedMapAccessSpelling && !isResolvedMapAccessCall;
    const bool isNamespacedVectorAccessCall =
        !expr.isMethodCall && isBuiltinAccess && isNamespacedVectorHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe");
    const bool isNamespacedMapAccessCall =
        !expr.isMethodCall && isBuiltinAccess && isNamespacedMapHelperCall &&
        (namespacedHelper == "at" || namespacedHelper == "at_unsafe") &&
        !prefersExplicitDirectMapAccessAliasDefinition &&
        !isMapNamespacedAccessCompatibilityCall(expr) &&
        defMap_.find(resolved) == defMap_.end();
    auto mapHelperReceiverIndex = [&](const Expr &candidate) -> size_t {
      if (hasNamedArguments(candidate.argNames)) {
        for (size_t i = 0; i < candidate.args.size(); ++i) {
          if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
              *candidate.argNames[i] == "values") {
            return i;
          }
        }
      }
      return 0;
    };
    auto preferredBareMapHelperTarget = [&](std::string_view helperName) {
      const std::string canonical = "/std/collections/map/" + std::string(helperName);
      if (defMap_.find(canonical) != defMap_.end() || hasImportedDefinitionPath(canonical)) {
        return canonical;
      }
      const std::string alias = "/map/" + std::string(helperName);
      if (defMap_.find(alias) != defMap_.end()) {
        return alias;
      }
      return canonical;
    };
    auto tryRewriteBareMapHelperCall = [&](const Expr &candidate,
                                           std::string_view helperName,
                                           Expr &rewrittenOut) -> bool {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
        return false;
      }
      if (candidate.name.find('/') != std::string::npos || !candidate.namespacePrefix.empty()) {
        return false;
      }
      const size_t receiverIndex = mapHelperReceiverIndex(candidate);
      if (receiverIndex >= candidate.args.size()) {
        return false;
      }
      std::string keyType;
      std::string valueType;
      if (!resolveMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
        return false;
      }
      rewrittenOut = candidate;
      if (resolveExperimentalMapTarget(candidate.args[receiverIndex], keyType, valueType)) {
        rewrittenOut.name = preferredExperimentalMapHelperTarget(helperName);
      } else {
        rewrittenOut.name = preferredBareMapHelperTarget(helperName);
      }
      rewrittenOut.namespacePrefix.clear();
      return true;
    };
    const bool shouldInferBuiltinBareMapContainsCall = true;
    const bool shouldInferBuiltinBareMapTryAtCall = true;
    const bool shouldInferBuiltinBareMapAccessCall = true;
    auto isIndexedArgsPackMapReceiverTarget = [&](const Expr &receiverExpr) -> bool {
      std::string elemType;
      std::string keyType;
      std::string valueType;
      return ((resolveIndexedArgsPackElementType(receiverExpr, elemType) ||
               resolveWrappedIndexedArgsPackElementType(receiverExpr, elemType) ||
               resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType)) &&
              extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
    };
    BuiltinCollectionCountCapacityDispatchContext builtinCollectionCountCapacityDispatchContext;
    builtinCollectionCountCapacityDispatchContext.isCountLike =
        (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
         isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
        expr.args.size() == 1 && !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
        (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall) &&
        !prefersCanonicalVectorCountAliasDefinition;
    builtinCollectionCountCapacityDispatchContext.isCapacityLike =
        isVectorBuiltinName(expr, "capacity") && expr.args.size() == 1 &&
        (!isStdNamespacedVectorCapacityCall || shouldBuiltinValidateStdNamespacedVectorCapacityCall);
    builtinCollectionCountCapacityDispatchContext.isUnnamespacedMapCountFallbackCall =
        isUnnamespacedMapCountFallbackCall;
    builtinCollectionCountCapacityDispatchContext.shouldInferBuiltinBareMapCountCall =
        shouldInferBuiltinBareMapCountCall;
    builtinCollectionCountCapacityDispatchContext.resolveMethodCallPath = resolveMethodCallPath;
    builtinCollectionCountCapacityDispatchContext.preferVectorStdlibHelperPath = preferVectorStdlibHelperPath;
    builtinCollectionCountCapacityDispatchContext.hasDeclaredDefinitionPath = hasDeclaredDefinitionPath;
    builtinCollectionCountCapacityDispatchContext.resolveMapTarget = resolveMapTarget;
    builtinCollectionCountCapacityDispatchContext.dispatchResolvers = &builtinCollectionDispatchResolvers;
    auto defIt = hasResolvedPath ? defMap_.find(resolved) : defMap_.end();
    const bool hasResolvedDefinition = defIt != defMap_.end();
    std::string normalizedCallName = expr.name;
    if (!normalizedCallName.empty() && normalizedCallName.front() == '/') {
      normalizedCallName.erase(normalizedCallName.begin());
    }
    const bool isStdNamespacedMapAccessPath = normalizedCallName.rfind("std/collections/map/", 0) == 0;
    const bool shouldDeferNamespacedVectorAccessCall =
        isNamespacedVectorAccessCall && (!hasResolvedDefinition || isStdNamespacedVectorAccessSpelling);
    const bool shouldDeferNamespacedMapAccessCall =
        isNamespacedMapAccessCall && (!hasResolvedDefinition || isStdNamespacedMapAccessPath);
    const bool shouldUseResolvedStdNamespacedVectorCountDefinition =
        isStdNamespacedVectorCountCall && defMap_.find("/vector/count") == defMap_.end() &&
        defMap_.find("/std/collections/vector/count") != defMap_.end();
    bool shouldDeferResolvedNamespacedCollectionHelperReturn =
        isNamespacedVectorCountCall || isNamespacedMapCountCall || isResolvedMapCountCall ||
        isNamespacedVectorCapacityCall || shouldDeferNamespacedVectorAccessCall ||
        shouldDeferNamespacedMapAccessCall;
    if (prefersCanonicalVectorCountAliasDefinition) {
      shouldDeferResolvedNamespacedCollectionHelperReturn = false;
    }
    if (shouldUseResolvedStdNamespacedVectorCountDefinition) {
      shouldDeferResolvedNamespacedCollectionHelperReturn = false;
    }
    const bool isDirectBuiltinCountCapacityCountCall =
        !expr.isMethodCall &&
        (isVectorBuiltinName(expr, "count") || isStdNamespacedMapCountCall ||
         isNamespacedMapCountCall || isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall) &&
        !expr.args.empty() &&
        !isArrayNamespacedVectorCountCompatibilityCall(expr) &&
        (!isStdNamespacedVectorCountCall || shouldBuiltinValidateStdNamespacedVectorCountCall) &&
        !prefersCanonicalVectorCountAliasDefinition &&
        ((defMap_.find(resolved) == defMap_.end() && !isStdNamespacedMapCountCall) ||
         isNamespacedVectorCountCall || isStdNamespacedMapCountCall || isNamespacedMapCountCall ||
         isUnnamespacedMapCountFallbackCall || isResolvedMapCountCall);
    const bool isDirectBuiltinCountCapacityCapacityCall =
        !expr.isMethodCall && isVectorBuiltinName(expr, "capacity") &&
        (!isStdNamespacedVectorCapacityCall || shouldBuiltinValidateStdNamespacedVectorCapacityCall) &&
        !expr.args.empty() &&
        (defMap_.find(resolved) == defMap_.end() || isNamespacedVectorCapacityCall);
    BuiltinCollectionDirectCountCapacityContext builtinCollectionDirectCountCapacityContext;
    builtinCollectionDirectCountCapacityContext.isDirectCountCall = isDirectBuiltinCountCapacityCountCall;
    builtinCollectionDirectCountCapacityContext.isDirectCountSingleArg =
        isDirectBuiltinCountCapacityCountCall && expr.args.size() == 1;
    builtinCollectionDirectCountCapacityContext.isDirectCapacityCall = isDirectBuiltinCountCapacityCapacityCall;
    builtinCollectionDirectCountCapacityContext.isDirectCapacitySingleArg =
        isDirectBuiltinCountCapacityCapacityCall && expr.args.size() == 1;
    builtinCollectionDirectCountCapacityContext.shouldInferBuiltinBareMapCountCall =
        shouldInferBuiltinBareMapCountCall;
    builtinCollectionDirectCountCapacityContext.resolveMethodCallPath = resolveMethodCallPath;
    builtinCollectionDirectCountCapacityContext.preferVectorStdlibHelperPath =
        preferVectorStdlibHelperPath;
    builtinCollectionDirectCountCapacityContext.hasDeclaredDefinitionPath = hasDeclaredDefinitionPath;
    builtinCollectionDirectCountCapacityContext.inferResolvedPathReturnKind =
        inferResolvedPathReturnKind;
    builtinCollectionDirectCountCapacityContext.tryRewriteBareMapHelperCall =
        [&](const Expr &candidate, Expr &rewrittenOut) {
          return tryRewriteBareMapHelperCall(candidate, "count", rewrittenOut);
        };
    builtinCollectionDirectCountCapacityContext.inferRewrittenExprReturnKind =
        [&](const Expr &rewrittenExpr) { return inferExprReturnKind(rewrittenExpr, params, locals); };
    builtinCollectionDirectCountCapacityContext.resolveArgsPackCountTarget = resolveArgsPackCountTarget;
    builtinCollectionDirectCountCapacityContext.resolveVectorTarget = resolveVectorTarget;
    builtinCollectionDirectCountCapacityContext.resolveArrayTarget = resolveArrayTarget;
    builtinCollectionDirectCountCapacityContext.resolveStringTarget = resolveStringTarget;
    builtinCollectionDirectCountCapacityContext.resolveMapTarget = resolveMapTarget;
    builtinCollectionDirectCountCapacityContext.dispatchResolvers =
        &builtinCollectionDispatchResolvers;
    auto isTypeNamespaceMethodCall = [&](const Expr &callExpr, const std::string &resolvedPath) -> bool {
      if (!callExpr.isMethodCall || callExpr.args.empty() || resolvedPath.empty()) {
        return false;
      }
      const Expr &receiver = callExpr.args.front();
      if (receiver.kind != Expr::Kind::Name) {
        return false;
      }
      if (findParamBinding(params, receiver.name) != nullptr || locals.find(receiver.name) != locals.end()) {
        return false;
      }
      const size_t methodSlash = resolvedPath.find_last_of('/');
      if (methodSlash == std::string::npos || methodSlash == 0) {
        return false;
      }
      const std::string receiverPath = resolvedPath.substr(0, methodSlash);
      const size_t receiverSlash = receiverPath.find_last_of('/');
      const std::string receiverTypeName =
          receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
      return receiverTypeName == receiver.name;
    };
    if (defIt != defMap_.end() && !shouldDeferResolvedNamespacedCollectionHelperReturn) {
      if (expr.isMethodCall) {
        const auto &calleeParams = paramsByDef_[resolved];
        Expr trimmedTypeNamespaceCallExpr;
        const std::vector<Expr> *orderedCallArgs = &expr.args;
        const std::vector<std::optional<std::string>> *orderedCallArgNames = &expr.argNames;
        if (isTypeNamespaceMethodCall(expr, resolved)) {
          trimmedTypeNamespaceCallExpr = expr;
          trimmedTypeNamespaceCallExpr.args.erase(trimmedTypeNamespaceCallExpr.args.begin());
          if (!trimmedTypeNamespaceCallExpr.argNames.empty()) {
            trimmedTypeNamespaceCallExpr.argNames.erase(trimmedTypeNamespaceCallExpr.argNames.begin());
          }
          orderedCallArgs = &trimmedTypeNamespaceCallExpr.args;
          orderedCallArgNames = &trimmedTypeNamespaceCallExpr.argNames;
        }
        std::string orderedArgError;
        if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames, orderedArgError)) {
          error_ = orderedArgError.find("argument count mismatch") != std::string::npos
                       ? "argument count mismatch for " + resolved
                       : orderedArgError;
          return ReturnKind::Unknown;
        }
        std::vector<const Expr *> orderedArgs;
        if (!buildOrderedArguments(calleeParams,
                                   *orderedCallArgs,
                                   *orderedCallArgNames,
                                   orderedArgs,
                                   orderedArgError)) {
          error_ = orderedArgError.find("argument count mismatch") != std::string::npos
                       ? "argument count mismatch for " + resolved
                       : orderedArgError;
          return ReturnKind::Unknown;
        }
        for (size_t paramIndex = 0; paramIndex < orderedArgs.size() && paramIndex < calleeParams.size(); ++paramIndex) {
          const Expr *arg = orderedArgs[paramIndex];
          if (arg == nullptr) {
            continue;
          }
          const ParameterInfo &param = calleeParams[paramIndex];
          if (param.binding.typeName.empty() || param.binding.typeName == "auto") {
            continue;
          }
          std::string expectedBase;
          std::string expectedArgText;
          if (splitTemplateTypeName(param.binding.typeName, expectedBase, expectedArgText) ||
              splitTemplateTypeName(param.binding.typeTemplateArg, expectedBase, expectedArgText)) {
            continue;
          }
          const ReturnKind expectedKind = returnKindForTypeName(normalizeBindingTypeName(param.binding.typeName));
          if (expectedKind == ReturnKind::Unknown) {
            continue;
          }
          const ReturnKind actualKind = inferExprReturnKind(*arg, params, locals);
          if (actualKind == ReturnKind::Unknown) {
            continue;
          }
          if (actualKind != expectedKind) {
            error_ = "argument type mismatch for " + resolved + " parameter " + param.name;
            return ReturnKind::Unknown;
          }
        }
      }
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        ReturnKind builtinAccessKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
          return builtinAccessKind;
        }
        ReturnKind builtinCollectionKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionCountCapacityReturnKind(
                expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
          return builtinCollectionKind;
        }
        return ReturnKind::Unknown;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second;
      }
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
        return builtinAccessKind;
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(
              expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
        return builtinCollectionKind;
      }
      return ReturnKind::Unknown;
    }
    bool handledDirectBuiltinCountCapacity = false;
    ReturnKind directBuiltinCountCapacityKind =
        inferBuiltinCollectionDirectCountCapacityReturnKind(
            expr, resolved, builtinCollectionDirectCountCapacityContext, handledDirectBuiltinCountCapacity);
    if (handledDirectBuiltinCountCapacity) {
      return directBuiltinCountCapacityKind;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "to_soa") || isSimpleCallName(expr, "to_aos")) &&
        expr.args.size() == 1 && defMap_.find(resolved) == defMap_.end()) {
      std::string elemType;
      if (isSimpleCallName(expr, "to_soa")) {
        if (resolveVectorTarget(expr.args.front(), elemType)) {
          return ReturnKind::Array;
        }
      } else {
        if (resolveSoaVectorTarget(expr.args.front(), elemType)) {
          return ReturnKind::Array;
        }
      }
    }
    const bool isBuiltinGet = isSimpleCallName(expr, "get");
    const bool isBuiltinRef = isSimpleCallName(expr, "ref");
    if (!expr.isMethodCall &&
        ((isBuiltinAccess && !expr.args.empty()) || ((isBuiltinGet || isBuiltinRef) && expr.args.size() == 2)) &&
        (defMap_.find(resolved) == defMap_.end() || shouldDeferNamespacedVectorAccessCall ||
         shouldDeferNamespacedMapAccessCall)) {
      const std::string helperName = isBuiltinAccess ? builtinAccessName : (isBuiltinGet ? "get" : "ref");
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
      auto isCollectionAccessReceiverExpr = [&](const Expr &candidate) -> bool {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        return resolveVectorTarget(candidate, elemType) || resolveArrayTarget(candidate, elemType) ||
               resolveSoaVectorTarget(candidate, elemType) || resolveStringTarget(candidate) ||
               resolveMapTarget(candidate, keyType, valueType);
      };
      const bool probePositionalReorderedReceiver =
          !hasNamedArgs && expr.args.size() > 1 &&
          (expr.args.front().kind == Expr::Kind::Literal || expr.args.front().kind == Expr::Kind::BoolLiteral ||
           expr.args.front().kind == Expr::Kind::FloatLiteral || expr.args.front().kind == Expr::Kind::StringLiteral ||
           (expr.args.front().kind == Expr::Kind::Name &&
            !isCollectionAccessReceiverExpr(expr.args.front())));
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < expr.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
      const bool hasAlternativeCollectionReceiver = probePositionalReorderedReceiver &&
                                                    std::any_of(receiverIndices.begin(), receiverIndices.end(), [&](size_t index) {
                                                      if (index == 0 || index >= expr.args.size()) {
                                                        return false;
                                                      }
                                                      const Expr &candidate = expr.args[index];
                                                      std::string elemType;
                                                      std::string keyType;
                                                      std::string valueType;
                                                      return resolveVectorTarget(candidate, elemType) ||
                                                             resolveArrayTarget(candidate, elemType) ||
                                                             resolveSoaVectorTarget(candidate, elemType) ||
                                                             resolveStringTarget(candidate) ||
                                                             resolveMapTarget(candidate, keyType, valueType);
                                                    });
      for (size_t receiverIndex : receiverIndices) {
        if (receiverIndex >= expr.args.size()) {
          continue;
        }
        const Expr &receiverCandidate = expr.args[receiverIndex];
        std::string methodResolved;
        std::string elemType;
        std::string keyType;
        std::string valueType;
        if (resolveVectorTarget(receiverCandidate, elemType)) {
          methodResolved = "/vector/" + helperName;
        } else if (resolveArrayTarget(receiverCandidate, elemType)) {
          methodResolved = "/array/" + helperName;
        } else if ((helperName == "get" || helperName == "ref") &&
                   resolveSoaVectorTarget(receiverCandidate, elemType)) {
          methodResolved = "/soa_vector/" + helperName;
        } else if (resolveStringTarget(receiverCandidate)) {
          methodResolved = "/string/" + helperName;
        } else if (resolveMapTarget(receiverCandidate, keyType, valueType)) {
          methodResolved = "/map/" + helperName;
        } else {
          continue;
        }
        methodResolved = preferVectorStdlibHelperPath(methodResolved);
        ReturnKind builtinMethodKind = ReturnKind::Unknown;
        if (defMap_.find(methodResolved) == defMap_.end() &&
            resolveBuiltinCollectionMethodReturnKind(
                methodResolved, receiverCandidate, builtinCollectionDispatchResolvers, builtinMethodKind)) {
          if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
            continue;
          }
          return builtinMethodKind;
        }
        auto methodIt = defMap_.find(methodResolved);
        if (methodIt != defMap_.end()) {
          if (hasAlternativeCollectionReceiver && receiverIndex == 0) {
            continue;
          }
          if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
            return ReturnKind::Unknown;
          }
          auto kindIt = returnKinds_.find(methodResolved);
          if (kindIt != returnKinds_.end()) {
            return kindIt->second;
          }
          return ReturnKind::Unknown;
        }
      }
    }
    if (defIt != defMap_.end() && shouldDeferResolvedNamespacedCollectionHelperReturn) {
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        ReturnKind builtinAccessKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
          return builtinAccessKind;
        }
        ReturnKind builtinCollectionKind = ReturnKind::Unknown;
        if (resolveBuiltinCollectionCountCapacityReturnKind(
                expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
          return builtinCollectionKind;
        }
        return ReturnKind::Unknown;
      }
      ReturnKind builtinAccessKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionAccessCallReturnKind(expr, builtinCollectionDispatchResolvers, builtinAccessKind)) {
        return builtinAccessKind;
      }
      ReturnKind builtinCollectionKind = ReturnKind::Unknown;
      if (resolveBuiltinCollectionCountCapacityReturnKind(
              expr, builtinCollectionCountCapacityDispatchContext, builtinCollectionKind)) {
        return builtinCollectionKind;
      }
      auto kindIt = returnKinds_.find(resolved);
      if (kindIt != returnKinds_.end() && kindIt->second != ReturnKind::Unknown) {
        return kindIt->second;
      }
      return ReturnKind::Unknown;
    }
    std::string builtinName;
    if (defMap_.find(resolved) == defMap_.end() && getBuiltinArrayAccessName(expr, builtinName) &&
        (!isStdNamespacedVectorAccessSpelling || shouldAllowStdAccessCompatibilityFallback) &&
        (!isStdNamespacedMapAccessSpelling || hasStdNamespacedMapAccessDefinition) &&
        expr.args.size() == 2) {
      std::string elemType;
      if (resolveStringTarget(expr.args.front())) {
        return ReturnKind::Int;
      }
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      if (resolveArgsPackAccessTarget(expr.args.front(), elemType) ||
          resolveArrayTarget(expr.args.front(), elemType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(elemType));
        if (kind != ReturnKind::Unknown) {
          return kind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (defMap_.find(resolved) == defMap_.end() && !expr.isMethodCall &&
        isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
        shouldInferBuiltinBareMapContainsCall) {
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        return ReturnKind::Bool;
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && defMap_.find(resolved) == defMap_.end() && expr.args.size() == 2 &&
        (isSimpleCallName(expr, "contains") || isSimpleCallName(expr, "tryAt") ||
         getBuiltinArrayAccessName(expr, builtinAccessName))) {
      if ((isSimpleCallName(expr, "contains") && !shouldInferBuiltinBareMapContainsCall) ||
          (isSimpleCallName(expr, "tryAt") && !shouldInferBuiltinBareMapTryAtCall) ||
          ((expr.name == "at" || expr.name == "at_unsafe") && !shouldInferBuiltinBareMapAccessCall)) {
        Expr rewrittenMapHelperCall;
        if (tryRewriteBareMapHelperCall(expr,
                                        isSimpleCallName(expr, "contains")
                                            ? "contains"
                                            : (isSimpleCallName(expr, "tryAt") ? "tryAt" : builtinAccessName),
                                        rewrittenMapHelperCall)) {
          return inferExprReturnKind(rewrittenMapHelperCall, params, locals);
        }
      }
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        std::string methodResolved;
        if (resolveMethodCallPath(expr.name, methodResolved)) {
          if (methodResolved == "/std/collections/map/contains" && !shouldInferBuiltinBareMapContainsCall &&
              !isIndexedArgsPackMapReceiverTarget(expr.args.front()) &&
              !hasDeclaredDefinitionPath("/map/contains") &&
              !hasImportedDefinitionPath("/std/collections/map/contains") &&
              !hasDeclaredDefinitionPath("/std/collections/map/contains")) {
            error_ = "unknown call target: /std/collections/map/contains";
            return ReturnKind::Unknown;
          }
          if (methodResolved == "/std/collections/map/tryAt" && !shouldInferBuiltinBareMapTryAtCall &&
              !isIndexedArgsPackMapReceiverTarget(expr.args.front()) &&
              !hasDeclaredDefinitionPath("/map/tryAt") &&
              !hasImportedDefinitionPath("/std/collections/map/tryAt") &&
              !hasDeclaredDefinitionPath("/std/collections/map/tryAt")) {
            error_ = "unknown call target: /std/collections/map/tryAt";
            return ReturnKind::Unknown;
          }
          if ((methodResolved == "/map/at" || methodResolved == "/map/at_unsafe" ||
               methodResolved == "/std/collections/map/at" || methodResolved == "/std/collections/map/at_unsafe") &&
              !shouldInferBuiltinBareMapAccessCall &&
              !isIndexedArgsPackMapReceiverTarget(expr.args.front()) &&
              !hasDeclaredDefinitionPath("/map/" + builtinAccessName) &&
              !hasImportedDefinitionPath("/std/collections/map/" + builtinAccessName) &&
              !hasDeclaredDefinitionPath("/std/collections/map/" + builtinAccessName)) {
            error_ = "unknown call target: /std/collections/map/" + builtinAccessName;
            return ReturnKind::Unknown;
          }
          auto methodIt = defMap_.find(methodResolved);
          if (methodIt != defMap_.end()) {
            if (!ensureDefinitionReturnKindReady(*methodIt->second)) {
              return ReturnKind::Unknown;
            }
            auto kindIt = returnKinds_.find(methodResolved);
            if (kindIt != returnKinds_.end()) {
              return kindIt->second;
            }
          }
          ReturnKind builtinMethodKind = ReturnKind::Unknown;
          if (resolveBuiltinCollectionMethodReturnKind(
                  methodResolved, expr.args.front(), builtinCollectionDispatchResolvers, builtinMethodKind)) {
            return builtinMethodKind;
          }
        }
      }
    }
    if (getBuiltinGpuName(expr, builtinName)) {
      return ReturnKind::Int;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "try") && expr.args.size() == 1) {
      ResultTypeInfo argResult;
      if (resolveResultTypeForExpr(expr.args.front(), params, locals, argResult) &&
          argResult.isResult && argResult.hasValue) {
        return returnKindForTypeName(normalizeBindingTypeName(argResult.valueType));
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "buffer_load") && expr.args.size() == 2) {
      std::string elemType;
      if (resolveBufferTarget(expr.args.front(), elemType)) {
        ReturnKind kind = returnKindForTypeName(elemType);
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
      return ReturnKind::Unknown;
    }
    if (!expr.isMethodCall && (isSimpleCallName(expr, "buffer") || isSimpleCallName(expr, "upload") ||
                               isSimpleCallName(expr, "readback"))) {
      return ReturnKind::Array;
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "contains") && shouldInferBuiltinBareMapContainsCall &&
        expr.args.size() == 2) {
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        return ReturnKind::Bool;
      }
    }
    if (!expr.isMethodCall && isSimpleCallName(expr, "tryAt") && expr.args.size() == 2 &&
        (shouldInferBuiltinBareMapTryAtCall || isIndexedArgsPackMapReceiverTarget(expr.args.front()))) {
      ResultTypeInfo argResult;
      if (resolveResultTypeForExpr(expr, params, locals, argResult) && argResult.isResult) {
        return ReturnKind::Array;
      }
    }
    if (!expr.isMethodCall && getBuiltinArrayAccessName(expr, builtinAccessName) &&
        expr.args.size() == 2 &&
        (shouldInferBuiltinBareMapAccessCall || isIndexedArgsPackMapReceiverTarget(expr.args.front()))) {
      std::string keyType;
      std::string valueType;
      if (resolveMapTarget(expr.args.front(), keyType, valueType)) {
        ReturnKind kind = returnKindForTypeName(normalizeBindingTypeName(valueType));
        return kind == ReturnKind::Unknown ? ReturnKind::Unknown : kind;
      }
    }
    if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {
      if (builtinName == "dereference") {
        ReturnKind targetKind = pointerTargetKind(expr.args.front());
        if (targetKind != ReturnKind::Unknown) {
          return targetKind;
        }
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinMemoryName(expr, builtinName)) {
      if (builtinName == "free") {
        return ReturnKind::Void;
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinComparisonName(expr, builtinName)) {
      return ReturnKind::Bool;
    }
    if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {
      if (builtinName == "is_nan" || builtinName == "is_inf" || builtinName == "is_finite") {
        return ReturnKind::Bool;
      }
      if (builtinName == "lerp" && expr.args.size() == 3) {
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
        return result;
      }
      if (builtinName == "pow" && expr.args.size() == 2) {
        ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
        result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
        return result;
      }
      if (expr.args.empty()) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Float64) {
        return ReturnKind::Float64;
      }
      if (argKind == ReturnKind::Float32) {
        return ReturnKind::Float32;
      }
      return ReturnKind::Unknown;
    }
    if (getBuiltinOperatorName(expr, builtinName)) {
      if (builtinName == "negate") {
        if (expr.args.size() != 1) {
          return ReturnKind::Unknown;
        }
        ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
        if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
          return ReturnKind::Unknown;
        }
        return argKind;
      }
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      if ((builtinName == "plus" || builtinName == "minus" || builtinName == "multiply" ||
           builtinName == "divide") &&
          !inferStructReturnPath(expr, params, locals).empty()) {
        return ReturnKind::Array;
      }
      ReturnKind left = inferExprReturnKind(expr.args[0], params, locals);
      ReturnKind right = inferExprReturnKind(expr.args[1], params, locals);
      return combineNumeric(left, right);
    }
    if (getBuiltinClampName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 3) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
      result = combineNumeric(result, inferExprReturnKind(expr.args[2], params, locals));
      return result;
    }
    if (getBuiltinMinMaxName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      ReturnKind result = inferExprReturnKind(expr.args[0], params, locals);
      result = combineNumeric(result, inferExprReturnKind(expr.args[1], params, locals));
      return result;
    }
    if (getBuiltinAbsSignName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (getBuiltinSaturateName(expr, builtinName, allowMathBareName(expr.name))) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      ReturnKind argKind = inferExprReturnKind(expr.args.front(), params, locals);
      if (argKind == ReturnKind::Bool || argKind == ReturnKind::Void) {
        return ReturnKind::Unknown;
      }
      return argKind;
    }
    if (getBuiltinConvertName(expr, builtinName)) {
      if (expr.templateArgs.size() != 1) {
        return ReturnKind::Unknown;
      }
      return returnKindForTypeName(expr.templateArgs.front());
    }
    if (getBuiltinMutationName(expr, builtinName)) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args.front(), params, locals);
    }
    if (isSimpleCallName(expr, "move")) {
      if (expr.args.size() != 1) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args.front(), params, locals);
    }
    if (isAssignCall(expr)) {
      if (expr.args.size() != 2) {
        return ReturnKind::Unknown;
      }
      return inferExprReturnKind(expr.args[1], params, locals);
    }
    return ReturnKind::Unknown;
  }
  return ReturnKind::Unknown;
}

std::string SemanticsValidator::inferStructReturnPath(
    const Expr &expr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto normalizeCollectionPath = [&](std::string typeName, std::string typeTemplateArg) -> std::string {
    if ((typeName == "Reference" || typeName == "Pointer") && !typeTemplateArg.empty()) {
      typeName = normalizeBindingTypeName(typeTemplateArg);
      typeTemplateArg.clear();
    } else {
      typeName = normalizeBindingTypeName(typeName);
    }
    if (typeName == "string") {
      return "/string";
    }
    if ((typeName == "array" || typeName == "vector" || typeName == "soa_vector") && !typeTemplateArg.empty()) {
      return "/" + typeName;
    }
    if (isMapCollectionTypeName(typeName) && !typeTemplateArg.empty()) {
      return "/map";
    }
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(typeName, base, arg)) {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args)) {
        if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
          return "/" + base;
        }
        if (isMapCollectionTypeName(base) && args.size() == 2) {
          return "/map";
        }
      }
    }
    return "";
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
                                   const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return structNames_.count(typeName) > 0 ? typeName : "";
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string direct = current + "/" + typeName;
        if (structNames_.count(direct) > 0) {
          return direct;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames_.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structNames_.count(root) > 0) {
          return root;
        }
      }
      if (current.empty()) {
        break;
      }
      const size_t slash = current.find_last_of('/');
      if (slash == std::string::npos || slash == 0) {
        current.clear();
      } else {
        current.erase(slash);
      }
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return "";
  };
  auto isMatrixQuaternionTypePath = [](const std::string &typePath) {
    return typePath == "/std/math/Mat2" || typePath == "/std/math/Mat3" || typePath == "/std/math/Mat4" ||
           typePath == "/std/math/Quat";
  };
  auto matrixDimensionForTypePath = [](const std::string &typePath) -> size_t {
    if (typePath == "/std/math/Mat2") {
      return 2;
    }
    if (typePath == "/std/math/Mat3") {
      return 3;
    }
    if (typePath == "/std/math/Mat4") {
      return 4;
    }
    return 0;
  };
  auto isVectorTypePath = [](const std::string &typePath) {
    return typePath == "/std/math/Vec2" || typePath == "/std/math/Vec3" || typePath == "/std/math/Vec4";
  };
  auto vectorDimensionForTypePath = [](const std::string &typePath) -> size_t {
    if (typePath == "/std/math/Vec2") {
      return 2;
    }
    if (typePath == "/std/math/Vec3") {
      return 3;
    }
    if (typePath == "/std/math/Vec4") {
      return 4;
    }
    return 0;
  };
  auto isNumericScalarExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    const bool isSoftware =
        kind == ReturnKind::Integer || kind == ReturnKind::Decimal || kind == ReturnKind::Complex;
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64 ||
        kind == ReturnKind::Float32 || kind == ReturnKind::Float64 || isSoftware) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::StringLiteral || arg.kind == Expr::Kind::BoolLiteral) {
        return false;
      }
      if (!inferStructReturnPath(arg, params, locals).empty()) {
        return false;
      }
      return true;
    }
    return false;
  };
  auto normalizeCollectionTypePath = [&](const std::string &typePath) {
    std::string normalizedTypePath = normalizeBindingTypeName(typePath);
    if (!normalizedTypePath.empty() && normalizedTypePath.front() == '/') {
      normalizedTypePath.erase(normalizedTypePath.begin());
    }
    if (normalizedTypePath.rfind("std/collections/experimental_map/Map__", 0) == 0) {
      return std::string("/map");
    }
    if (typePath == "/array" || typePath == "array") {
      return std::string("/array");
    }
    if (typePath == "/vector" || typePath == "vector" || typePath == "/std/collections/vector" ||
        typePath == "std/collections/vector") {
      return std::string("/vector");
    }
    if (typePath == "/soa_vector" || typePath == "soa_vector") {
      return std::string("/soa_vector");
    }
    if (isMapCollectionTypeName(typePath) || typePath == "/map" || typePath == "/std/collections/map") {
      return std::string("/map");
    }
    if (typePath == "/string" || typePath == "string") {
      return std::string("/string");
    }
    return std::string();
  };
  std::function<std::string(const Expr &)> resolvePointerTargetTypeText;
  resolvePointerTargetTypeText = [&](const Expr &pointerExpr) -> std::string {
    auto resolveBindingTargetTypeText = [&](const Expr &target) -> std::string {
      if (target.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          return bindingTypeText(*paramBinding);
        }
        auto it = locals.find(target.name);
        if (it != locals.end()) {
          return bindingTypeText(it->second);
        }
        return {};
      }
      if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
        return {};
      }
      std::string receiverStruct = inferStructReturnPath(target.args.front(), params, locals);
      if (receiverStruct.empty() || structNames_.count(receiverStruct) == 0) {
        return {};
      }
      auto defIt = defMap_.find(receiverStruct);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return {};
      }
      for (const auto &stmt : defIt->second->statements) {
        if (!stmt.isBinding || stmt.name != target.name) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
          return {};
        }
        return bindingTypeText(fieldBinding);
      }
      return {};
    };
    if (pointerExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, pointerExpr.name)) {
        if ((paramBinding->typeName == "Reference" || paramBinding->typeName == "Pointer") &&
            !paramBinding->typeTemplateArg.empty()) {
          return paramBinding->typeTemplateArg;
        }
        return {};
      }
      auto it = locals.find(pointerExpr.name);
      if (it != locals.end() &&
          (it->second.typeName == "Reference" || it->second.typeName == "Pointer") &&
          !it->second.typeTemplateArg.empty()) {
        return it->second.typeTemplateArg;
      }
      return {};
    }
    if (pointerExpr.kind != Expr::Kind::Call) {
      return {};
    }
    std::string pointerBuiltin;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltin) && pointerExpr.args.size() == 1 &&
        pointerBuiltin == "location") {
      return resolveBindingTargetTypeText(pointerExpr.args.front());
    }
    auto defIt = defMap_.find(resolveCalleePath(pointerExpr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return {};
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      std::string base;
      std::string argText;
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      if (!splitTemplateTypeName(normalizedReturnType, base, argText) || argText.empty()) {
        return {};
      }
      base = normalizeBindingTypeName(base);
      if (base == "Reference" || base == "Pointer") {
        return argText;
      }
      return {};
    }
    return {};
  };
  if (expr.kind == Expr::Kind::Call) {
    std::string pointerBuiltin;
    if (getBuiltinPointerName(expr, pointerBuiltin) && pointerBuiltin == "dereference" && expr.args.size() == 1) {
      const std::string pointeeType = resolvePointerTargetTypeText(expr.args.front());
      if (!pointeeType.empty()) {
        const std::string collectionPath = normalizeCollectionTypePath(pointeeType);
        if (!collectionPath.empty()) {
          return collectionPath;
        }
        std::string unwrappedType = unwrapReferencePointerTypeText(pointeeType);
        std::string structPath = resolveStructTypePath(unwrappedType, expr.namespacePrefix);
        if (!structPath.empty()) {
          return structPath;
        }
      }
    }
  }
  auto resolveCallCollectionTypePath = [&](const Expr &target, std::string &typePathOut) -> bool {
    return SemanticsValidator::resolveCallCollectionTypePath(target, params, locals, typePathOut);
  };
  auto resolveCallCollectionTemplateArgs =
      [&](const Expr &target, const std::string &expectedBase, std::vector<std::string> &argsOut) -> bool {
    return SemanticsValidator::resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, argsOut);
  };
  const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers =
      makeBuiltinCollectionDispatchResolvers(params, locals);
  const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;
  const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;
  const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;
  const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;
  const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;
  auto isEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> bool {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    if (!candidate.args.empty() || !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return false;
    }
    if (!candidate.hasBodyArguments && candidate.bodyArguments.empty()) {
      return false;
    }
    return allowAnyName || isBuiltinBlockCall(candidate);
  };
  auto getEnvelopeValueExpr = [&](const Expr &candidate, bool allowAnyName) -> const Expr * {
    if (!isEnvelopeValueExpr(candidate, allowAnyName)) {
      return nullptr;
    }
    const Expr *valueExpr = nullptr;
    for (const auto &bodyExpr : candidate.bodyArguments) {
      if (bodyExpr.isBinding) {
        continue;
      }
      valueExpr = &bodyExpr;
    }
    return valueExpr;
  };
  auto collectionHelperPathCandidates = [&](const std::string &path) {
    std::vector<std::string> candidates;
    auto appendUnique = [&](const std::string &candidate) {
      for (const auto &existing : candidates) {
        if (existing == candidate) {
          return;
        }
      }
      candidates.push_back(candidate);
    };

    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("array/", 0) == 0 || normalizedPath.rfind("vector/", 0) == 0 ||
          normalizedPath.rfind("std/collections/vector/", 0) == 0 || normalizedPath.rfind("map/", 0) == 0 ||
          normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }

    appendUnique(path);
    appendUnique(normalizedPath);
    if (normalizedPath.rfind("/array/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/array/").size());
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/vector/" + suffix);
        appendUnique("/std/collections/vector/" + suffix);
      }
    } else if (normalizedPath.rfind("/vector/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/vector/").size());
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
          suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
          suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/std/collections/vector/" + suffix);
      }
      if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
        appendUnique("/array/" + suffix);
      }
      } else if (normalizedPath.rfind("/std/collections/vector/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
        appendUnique("/vector/" + suffix);
        if (suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" && suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" && suffix != "remove_at" && suffix != "remove_swap") {
          appendUnique("/array/" + suffix);
        }
      } else if (normalizedPath.rfind("/map/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/map/").size());
        if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
          appendUnique("/std/collections/map/" + suffix);
        }
      } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
        const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
        if (suffix != "map") {
          appendUnique("/map/" + suffix);
        }
      }
    return candidates;
  };
  auto pruneMapAccessStructReturnCompatibilityCandidates = [&](const std::string &path,
                                                               std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("map/", 0) == 0 || normalizedPath.rfind("std/collections/map/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    auto eraseCandidate = [&](const std::string &candidate) {
      for (auto it = candidates.begin(); it != candidates.end();) {
        if (*it == candidate) {
          it = candidates.erase(it);
        } else {
          ++it;
        }
      }
    };
    if (normalizedPath.rfind("/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/std/collections/map/" + suffix);
      }
    } else if (normalizedPath.rfind("/std/collections/map/", 0) == 0) {
      const std::string suffix = normalizedPath.substr(std::string("/std/collections/map/").size());
      if (suffix == "at" || suffix == "at_unsafe") {
        eraseCandidate("/map/" + suffix);
      }
    }
  };
  auto pruneBuiltinVectorAccessStructReturnCandidates = [&](const Expr &candidate,
                                                            const std::string &path,
                                                            std::vector<std::string> &candidates) {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath.front() != '/') {
      if (normalizedPath.rfind("vector/", 0) == 0 || normalizedPath.rfind("std/collections/vector/", 0) == 0) {
        normalizedPath.insert(normalizedPath.begin(), '/');
      }
    }
    if (normalizedPath.rfind("/std/collections/vector/", 0) != 0) {
      return;
    }
    const std::string suffix = normalizedPath.substr(std::string("/std/collections/vector/").size());
    if (suffix != "at" && suffix != "at_unsafe") {
      return;
    }
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.args.empty()) {
      return;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return;
    }
    std::string elemType;
    if (!resolveVectorTarget(candidate.args[receiverIndex], elemType) &&
        !resolveArrayTarget(candidate.args[receiverIndex], elemType) &&
        !resolveStringTarget(candidate.args[receiverIndex])) {
      return;
    }
    const std::string canonicalCandidate = "/std/collections/vector/" + suffix;
    for (auto it = candidates.begin(); it != candidates.end();) {
      if (*it == canonicalCandidate) {
        it = candidates.erase(it);
      } else {
        ++it;
      }
    }
  };
  auto isExplicitMapAccessStructReturnCompatibilityCall = [&](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    std::string helperName;
    if (normalized == "map/at") {
      helperName = "at";
    } else if (normalized == "map/at_unsafe") {
      helperName = "at_unsafe";
    } else {
      return false;
    }
    if (defMap_.find("/map/" + helperName) != defMap_.end()) {
      return false;
    }
    if (candidate.args.empty()) {
      return false;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndex = 0;
      }
    }
    if (receiverIndex >= candidate.args.size()) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    return resolveMapTarget(candidate.args[receiverIndex], keyType, valueType);
  };

  if (expr.isLambda) {
    return "";
  }

  if (expr.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
      if (std::string collectionPath =
              normalizeCollectionPath(paramBinding->typeName, paramBinding->typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (paramBinding->typeName != "Reference" && paramBinding->typeName != "Pointer") {
        return resolveStructTypePath(paramBinding->typeName, expr.namespacePrefix);
      }
      return "";
    }
    auto it = locals.find(expr.name);
    if (it != locals.end()) {
      if (std::string collectionPath = normalizeCollectionPath(it->second.typeName, it->second.typeTemplateArg);
          !collectionPath.empty()) {
        return collectionPath;
      }
      if (it->second.typeName != "Reference" && it->second.typeName != "Pointer") {
        return resolveStructTypePath(it->second.typeName, expr.namespacePrefix);
      }
    }
    return "";
  }

  if (expr.kind == Expr::Kind::Call) {
    std::string builtinName;
    if (getBuiltinOperatorName(expr, builtinName) && (builtinName == "plus" || builtinName == "minus") &&
        expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType) && leftType == rightType) {
        return leftType;
      }
    }
    if (getBuiltinOperatorName(expr, builtinName) && builtinName == "multiply" && expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType)) {
        if (leftType == "/std/math/Quat") {
          if (rightType == "/std/math/Quat") {
            return leftType;
          }
          if (rightType == "/std/math/Vec3") {
            return rightType;
          }
          if (isNumericScalarExpr(expr.args[1])) {
            return leftType;
          }
          return "";
        }
        if (isMatrixQuaternionTypePath(rightType) && leftType == rightType) {
          return leftType;
        }
        if (isVectorTypePath(rightType) &&
            matrixDimensionForTypePath(leftType) == vectorDimensionForTypePath(rightType)) {
          return rightType;
        }
        if (isNumericScalarExpr(expr.args[1])) {
          return leftType;
        }
        return "";
      }
      if (isMatrixQuaternionTypePath(rightType)) {
        if (isNumericScalarExpr(expr.args[0])) {
          return rightType;
        }
        return "";
      }
    }
    if (getBuiltinOperatorName(expr, builtinName) && builtinName == "divide" && expr.args.size() == 2) {
      const std::string leftType = inferStructReturnPath(expr.args[0], params, locals);
      const std::string rightType = inferStructReturnPath(expr.args[1], params, locals);
      if (isMatrixQuaternionTypePath(leftType) && rightType.empty() && isNumericScalarExpr(expr.args[1])) {
        return leftType;
      }
      if (isMatrixQuaternionTypePath(leftType) || isMatrixQuaternionTypePath(rightType)) {
        return "";
      }
    }
    if (expr.isFieldAccess && expr.args.size() == 1) {
      auto isStaticField = [](const Expr &stmt) -> bool {
        for (const auto &transform : stmt.transforms) {
          if (transform.name == "static") {
            return true;
          }
        }
        return false;
      };
      std::string receiverStruct;
      if (expr.args.front().kind == Expr::Kind::Name &&
          findParamBinding(params, expr.args.front().name) == nullptr &&
          locals.find(expr.args.front().name) == locals.end()) {
        receiverStruct = resolveStructTypePath(expr.args.front().name, expr.args.front().namespacePrefix);
      }
      if (receiverStruct.empty()) {
        receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      }
      if (receiverStruct.empty()) {
        return "";
      }
      auto defIt = defMap_.find(receiverStruct);
      if (defIt == defMap_.end() || !defIt->second) {
        return "";
      }
      const bool isTypeReceiver =
          expr.args.front().kind == Expr::Kind::Name &&
          findParamBinding(params, expr.args.front().name) == nullptr &&
          locals.find(expr.args.front().name) == locals.end();
      for (const auto &stmt : defIt->second->statements) {
        if (!stmt.isBinding || stmt.name != expr.name) {
          continue;
        }
        if (isTypeReceiver ? !isStaticField(stmt) : isStaticField(stmt)) {
          continue;
        }
        BindingInfo fieldBinding;
        if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
          return "";
        }
        std::string fieldType = fieldBinding.typeName;
        if ((fieldType == "Reference" || fieldType == "Pointer") && !fieldBinding.typeTemplateArg.empty()) {
          fieldType = fieldBinding.typeTemplateArg;
        }
        return resolveStructTypePath(fieldType, expr.namespacePrefix);
      }
      return "";
    }
    if (expr.isMethodCall) {
      if (expr.args.empty()) {
        return "";
      }
      std::string receiverStruct = inferStructReturnPath(expr.args.front(), params, locals);
      if (receiverStruct.empty()) {
        return "";
      }
      std::string methodName = expr.name;
      std::string rawMethodName = expr.name;
      if (!rawMethodName.empty() && rawMethodName.front() == '/') {
        rawMethodName.erase(rawMethodName.begin());
      }
      if (!methodName.empty() && methodName.front() == '/') {
        methodName.erase(methodName.begin());
      }
      std::string namespacedCollection;
      std::string namespacedHelper;
      if (getNamespacedCollectionHelperName(expr, namespacedCollection, namespacedHelper) &&
          !namespacedHelper.empty()) {
        methodName = namespacedHelper;
      }
      const bool blocksBuiltinVectorAccessStructReturnForwarding =
          methodName == "at" || methodName == "at_unsafe";
      const bool isExplicitRemovedMapAliasStructReturnMethod =
          rawMethodName == "map/at" || rawMethodName == "map/at_unsafe" ||
          rawMethodName == "std/collections/map/at" || rawMethodName == "std/collections/map/at_unsafe";
      std::vector<std::string> methodCandidates;
      if (receiverStruct == "/vector") {
        methodCandidates = {"/vector/" + methodName};
        if (!blocksBuiltinVectorAccessStructReturnForwarding) {
          methodCandidates.push_back("/std/collections/vector/" + methodName);
        }
        if (methodName != "count" && !blocksBuiltinVectorAccessStructReturnForwarding) {
          methodCandidates.push_back("/array/" + methodName);
        }
      } else if (receiverStruct == "/array") {
        methodCandidates = {"/array/" + methodName};
        if (methodName != "count") {
          methodCandidates.push_back("/vector/" + methodName);
          if (!blocksBuiltinVectorAccessStructReturnForwarding) {
            methodCandidates.push_back("/std/collections/vector/" + methodName);
          }
        }
      } else if (receiverStruct == "/map") {
        if (!isExplicitRemovedMapAliasStructReturnMethod) {
          methodCandidates = {"/std/collections/map/" + methodName, "/map/" + methodName};
        }
      } else {
        methodCandidates = {receiverStruct + "/" + methodName};
      }
      for (const auto &candidate : methodCandidates) {
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      for (const auto &candidate : methodCandidates) {
        auto defIt = defMap_.find(candidate);
        if (defIt == defMap_.end()) {
          continue;
        }
        if (!ensureDefinitionReturnKindReady(*defIt->second)) {
          return "";
        }
        auto structIt = returnStructs_.find(candidate);
        if (structIt != returnStructs_.end()) {
          return structIt->second;
        }
      }
      return "";
    }

    if (isMatchCall(expr)) {
      Expr expanded;
      std::string error;
      if (!lowerMatchToIf(expr, expanded, error)) {
        return "";
      }
      return inferStructReturnPath(expanded, params, locals);
    }
    if (isIfCall(expr) && expr.args.size() == 3) {
      const Expr &thenArg = expr.args[1];
      const Expr &elseArg = expr.args[2];
      const Expr *thenValue = getEnvelopeValueExpr(thenArg, true);
      const Expr *elseValue = getEnvelopeValueExpr(elseArg, true);
      const Expr &thenExpr = thenValue ? *thenValue : thenArg;
      const Expr &elseExpr = elseValue ? *elseValue : elseArg;
      std::string thenPath = inferStructReturnPath(thenExpr, params, locals);
      if (thenPath.empty()) {
        return "";
      }
      std::string elsePath = inferStructReturnPath(elseExpr, params, locals);
      return (thenPath == elsePath) ? thenPath : "";
    }

    if (const Expr *valueExpr = getEnvelopeValueExpr(expr, false)) {
      if (isReturnCall(*valueExpr) && !valueExpr->args.empty()) {
        return inferStructReturnPath(valueExpr->args.front(), params, locals);
      }
      return inferStructReturnPath(*valueExpr, params, locals);
    }

    if (expr.kind == Expr::Kind::Call) {
      std::string accessHelperName;
      if (getBuiltinArrayAccessName(expr, accessHelperName) && !expr.args.empty()) {
        size_t receiverIndex = expr.isMethodCall ? 0 : 0;
        if (!expr.isMethodCall && hasNamedArguments(expr.argNames)) {
          bool foundValues = false;
          for (size_t i = 0; i < expr.args.size(); ++i) {
            if (i < expr.argNames.size() && expr.argNames[i].has_value() &&
                *expr.argNames[i] == "values") {
              receiverIndex = i;
              foundValues = true;
              break;
            }
          }
          if (!foundValues) {
            receiverIndex = 0;
          }
        }
        if (receiverIndex < expr.args.size()) {
          std::string elemType;
          if (resolveArgsPackAccessTarget(expr.args[receiverIndex], elemType) ||
              resolveVectorTarget(expr.args[receiverIndex], elemType) ||
              resolveArrayTarget(expr.args[receiverIndex], elemType) ||
              resolveStringTarget(expr.args[receiverIndex])) {
            return "";
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(expr, collectionTypePath) && collectionTypePath == "/map") {
        const std::string resolvedCollectionPath = resolveCalleePath(expr);
        std::string normalizedCollectionPath = normalizeBindingTypeName(resolvedCollectionPath);
        if (!normalizedCollectionPath.empty() && normalizedCollectionPath.front() != '/') {
          normalizedCollectionPath.insert(normalizedCollectionPath.begin(), '/');
        }
        if (normalizedCollectionPath.rfind("/std/collections/experimental_map/Map__", 0) == 0 &&
            structNames_.count(resolvedCollectionPath) > 0) {
          return resolvedCollectionPath;
        }
        std::vector<std::string> collectionTemplateArgs;
        auto specializedExperimentalMapPath = [&](const std::vector<std::string> &templateArgs) {
          auto stripWhitespace = [](const std::string &text) {
            std::string result;
            result.reserve(text.size());
            for (unsigned char ch : text) {
              if (!std::isspace(ch)) {
                result.push_back(static_cast<char>(ch));
              }
            }
            return result;
          };
          auto fnv1a64 = [](const std::string &text) {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char ch : text) {
              hash ^= static_cast<uint64_t>(ch);
              hash *= 1099511628211ULL;
            }
            return hash;
          };
          std::ostringstream specializedPath;
          specializedPath << "/std/collections/experimental_map/Map__t"
                          << std::hex
                          << fnv1a64(stripWhitespace(joinTemplateArgs(templateArgs)));
          return specializedPath.str();
        };
        if (resolveCallCollectionTemplateArgs(expr, "map", collectionTemplateArgs) &&
            collectionTemplateArgs.size() == 2) {
          return specializedExperimentalMapPath(collectionTemplateArgs);
        }
        auto defIt = defMap_.find(resolvedCollectionPath);
        if (defIt != defMap_.end() && defIt->second != nullptr) {
          for (const auto &transform : defIt->second->transforms) {
            if (transform.name != "return" || transform.templateArgs.size() != 1) {
              continue;
            }
            std::string keyType;
            std::string valueType;
            if (extractMapKeyValueTypesFromTypeText(transform.templateArgs.front(), keyType, valueType)) {
              return specializedExperimentalMapPath({keyType, valueType});
            }
          }
        }
        return "/map";
      }
    }

    const std::string resolvedCallee = resolveCalleePath(expr);
    const bool isExplicitMapAccessCompatibilityCall = isExplicitMapAccessStructReturnCompatibilityCall(expr);
    const std::string structReturnProbePath = isExplicitMapAccessCompatibilityCall ? expr.name : resolvedCallee;
    auto resolvedCandidates = collectionHelperPathCandidates(structReturnProbePath);
    pruneMapAccessStructReturnCompatibilityCandidates(structReturnProbePath, resolvedCandidates);
    pruneBuiltinVectorAccessStructReturnCandidates(expr, structReturnProbePath, resolvedCandidates);
    for (const auto &candidate : resolvedCandidates) {
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
    }
    std::string resolved = resolvedCandidates.empty() ? std::string() : resolvedCandidates.front();
    bool hasDefinitionCandidate = false;
    for (const auto &candidate : resolvedCandidates) {
      auto defIt = defMap_.find(candidate);
      if (defIt == defMap_.end()) {
        continue;
      }
      hasDefinitionCandidate = true;
      if (!ensureDefinitionReturnKindReady(*defIt->second)) {
        return "";
      }
      auto structIt = returnStructs_.find(candidate);
      if (structIt != returnStructs_.end()) {
        return structIt->second;
      }
      if (resolved.empty()) {
        resolved = candidate;
      }
    }
    std::string collection;
    if (!hasDefinitionCandidate && getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector") && expr.templateArgs.size() == 1) {
        return "/" + collection;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        return "/map";
      }
    }
    if (!resolved.empty() && structNames_.count(resolved) > 0) {
      return resolved;
    }
  }

  return "";
}

bool SemanticsValidator::inferDefinitionReturnKind(const Definition &def) {
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
  if (kindIt->second != ReturnKind::Unknown) {
    return true;
  }
  if (!inferenceStack_.insert(def.fullPath).second) {
    error_ = "return type inference requires explicit annotation on " + def.fullPath;
    return false;
  }
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
      return false;
    }
    if (hasReturnTransform && hasReturnAuto && !sawExplicitReturnStmt &&
        stmtIndex == implicitReturnStmtIndex && !inferenceState.sawReturn) {
      if (!recordDefinitionInferredReturn(def, &stmt, defParams, locals, inferenceState)) {
        return false;
      }
    }
  }
  if (def.returnExpr.has_value()) {
    if (!recordDefinitionInferredReturn(def, &*def.returnExpr, defParams, locals, inferenceState)) {
      return false;
    }
  }
  if (!inferenceState.sawReturn) {
    if (hasReturnTransform && hasReturnAuto) {
      if (error_.empty()) {
        error_ = "unable to infer return type on " + def.fullPath;
      }
      return false;
    }
    kindIt->second = ReturnKind::Void;
  } else if (inferenceState.inferred == ReturnKind::Unknown) {
    if (error_.empty()) {
      error_ = "unable to infer return type on " + def.fullPath;
    }
    return false;
  } else {
    kindIt->second = inferenceState.inferred;
  }
  if (!inferenceState.inferredStructPath.empty()) {
    returnStructs_[def.fullPath] = inferenceState.inferredStructPath;
  }
  inferenceStack_.erase(def.fullPath);
  return true;
}

} // namespace primec::semantics
