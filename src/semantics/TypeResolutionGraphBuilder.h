#pragma once

// Helpers shared by the TypeResolutionGraph*.cpp units (split out of
// TypeResolutionGraph.cpp without changes, TODO-5384).
#include "TypeResolutionGraph.h"
#include "CondensationDag.h"
#include "SemanticsHelpers.h"
#include "TypeResolutionGraphPreparation.h"
#include "primec/support/CompileArena.h"
#include "primec/ir/StdlibCollectionPaths.h"
#include "primec/testing/SemanticsGraphHelpers.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace primec::semantics {
namespace typeResolutionGraphBuilder {

inline std::optional<uint64_t> readGraphMetricBudget(const char *envName) {
  const char *value = std::getenv(envName);
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  errno = 0;
  char *end = nullptr;
  const unsigned long long parsed = std::strtoull(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0') {
    return std::nullopt;
  }
  return static_cast<uint64_t>(parsed);
}

struct InvalidationCounts {
  uint64_t localBinding = 0;
  uint64_t controlFlow = 0;
  uint64_t initializerShape = 0;
  uint64_t definitionSignature = 0;
  uint64_t importAlias = 0;
  uint64_t receiverType = 0;
  std::vector<std::string> controlFlowScopes;
  std::vector<std::string> receiverTypeScopes;
};

inline void appendUniqueString(std::vector<std::string> &values, std::string value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(std::move(value));
  }
}

inline uint64_t &invalidationCountForFamily(InvalidationCounts &counts,
                                     TypeResolutionGraphInvalidationEditFamily editFamily) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    return counts.localBinding;
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    return counts.controlFlow;
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    return counts.initializerShape;
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    return counts.definitionSignature;
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    return counts.importAlias;
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    return counts.receiverType;
  }
  return counts.localBinding;
}

inline uint64_t invalidationCountForFamily(const InvalidationCounts &counts,
                                    TypeResolutionGraphInvalidationEditFamily editFamily) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    return counts.localBinding;
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    return counts.controlFlow;
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    return counts.initializerShape;
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    return counts.definitionSignature;
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    return counts.importAlias;
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    return counts.receiverType;
  }
  return 0;
}

inline void recordInvalidationObservation(InvalidationCounts &counts,
                                   TypeResolutionGraphInvalidationEditFamily editFamily) {
  ++invalidationCountForFamily(counts, editFamily);
}

inline void setTypeResolutionGraphInvalidationCount(
    TypeResolutionGraph &graph,
    TypeResolutionGraphInvalidationEditFamily editFamily,
    uint64_t count) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    graph.invalidationLocalBindingCount = count;
    return;
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    graph.invalidationControlFlowCount = count;
    return;
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    graph.invalidationInitializerShapeCount = count;
    return;
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    graph.invalidationDefinitionSignatureCount = count;
    return;
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    graph.invalidationImportAliasCount = count;
    return;
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    graph.invalidationReceiverTypeCount = count;
    return;
  }
}

inline void countInvalidationExpr(const Expr &expr,
                           InvalidationCounts &counts,
                           const std::string &scopePath) {
  if (isIfCall(expr) || isMatchCall(expr)) {
    recordInvalidationObservation(
        counts, TypeResolutionGraphInvalidationEditFamily::ControlFlow);
    appendUniqueString(counts.controlFlowScopes, scopePath);
  }
  if (expr.isMethodCall && !expr.isFieldAccess) {
    recordInvalidationObservation(
        counts, TypeResolutionGraphInvalidationEditFamily::ReceiverType);
    appendUniqueString(counts.receiverTypeScopes, scopePath);
  }
  for (const auto &arg : expr.args) {
    countInvalidationExpr(arg, counts, scopePath);
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    countInvalidationExpr(bodyExpr, counts, scopePath);
  }
}

inline InvalidationCounts computeInvalidationCounts(const Program &program) {
  InvalidationCounts counts;
  for (size_t index = 0; index < program.definitions.size(); ++index) {
    recordInvalidationObservation(
        counts, TypeResolutionGraphInvalidationEditFamily::DefinitionSignature);
  }
  for (size_t index = 0; index < program.imports.size(); ++index) {
    recordInvalidationObservation(
        counts, TypeResolutionGraphInvalidationEditFamily::ImportAlias);
  }
  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      countInvalidationExpr(param, counts, def.fullPath);
    }
    for (const auto &stmt : def.statements) {
      if (def.returnExpr.has_value() && isReturnCall(stmt)) {
        continue;
      }
      if (stmt.isBinding) {
        recordInvalidationObservation(
            counts, TypeResolutionGraphInvalidationEditFamily::LocalBinding);
        if (!stmt.args.empty() || !stmt.bodyArguments.empty()) {
          recordInvalidationObservation(
              counts, TypeResolutionGraphInvalidationEditFamily::InitializerShape);
        }
      }
      countInvalidationExpr(stmt, counts, def.fullPath);
    }
    if (def.returnExpr.has_value()) {
      countInvalidationExpr(*def.returnExpr, counts, def.fullPath);
    }
  }
  for (const auto &exec : program.executions) {
    for (const auto &arg : exec.arguments) {
      countInvalidationExpr(arg, counts, exec.fullPath);
    }
    for (const auto &bodyExpr : exec.bodyArguments) {
      countInvalidationExpr(bodyExpr, counts, exec.fullPath);
    }
  }
  return counts;
}

inline bool hasScope(const std::vector<std::string> &scopes, const std::string &scopePath) {
  return std::find(scopes.begin(), scopes.end(), scopePath) != scopes.end();
}

inline std::vector<uint32_t> sortedUniqueNodeIds(std::vector<uint32_t> ids) {
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

inline std::vector<uint32_t> mergeNodeIds(std::vector<uint32_t> first,
                                   const std::vector<uint32_t> &second) {
  first.insert(first.end(), second.begin(), second.end());
  return sortedUniqueNodeIds(std::move(first));
}

inline std::vector<uint32_t> graphClosure(const TypeResolutionGraph &graph,
                                   const std::vector<uint32_t> &seeds,
                                   bool reverse) {
  std::vector<uint32_t> result;
  std::vector<uint32_t> worklist;
  std::vector<bool> visited(graph.nodes.size(), false);
  for (uint32_t seed : sortedUniqueNodeIds(seeds)) {
    if (seed >= visited.size() || visited[seed]) {
      continue;
    }
    visited[seed] = true;
    worklist.push_back(seed);
  }

  for (size_t index = 0; index < worklist.size(); ++index) {
    const uint32_t current = worklist[index];
    for (const auto &edge : graph.edges) {
      const bool matches =
          reverse ? edge.targetId == current : edge.sourceId == current;
      if (!matches) {
        continue;
      }
      const uint32_t next = reverse ? edge.sourceId : edge.targetId;
      if (next >= visited.size() || visited[next]) {
        continue;
      }
      visited[next] = true;
      result.push_back(next);
      worklist.push_back(next);
    }
  }
  return sortedUniqueNodeIds(std::move(result));
}

inline std::vector<uint32_t> directGraphTargets(const TypeResolutionGraph &graph,
                                         uint32_t sourceId) {
  std::vector<uint32_t> targets;
  for (const auto &edge : graph.edges) {
    if (edge.sourceId == sourceId) {
      targets.push_back(edge.targetId);
    }
  }
  return sortedUniqueNodeIds(std::move(targets));
}

inline bool fanoutContainsNodeKind(const TypeResolutionGraph &graph,
                            const std::vector<uint32_t> &nodeIds,
                            TypeResolutionNodeKind kind) {
  for (uint32_t nodeId : nodeIds) {
    if (nodeId < graph.nodes.size() && graph.nodes[nodeId].kind == kind) {
      return true;
    }
  }
  return false;
}

inline void appendInvalidationFanout(std::vector<TypeResolutionGraphInvalidationFanout> &fanouts,
                              TypeResolutionGraphInvalidationEditFamily editFamily,
                              const TypeResolutionGraphNode &trigger,
                              std::vector<uint32_t> immediateNodeIds,
                              std::vector<uint32_t> lazyRevisitNodeIds) {
  immediateNodeIds = sortedUniqueNodeIds(std::move(immediateNodeIds));
  lazyRevisitNodeIds = sortedUniqueNodeIds(std::move(lazyRevisitNodeIds));
  fanouts.push_back(TypeResolutionGraphInvalidationFanout{
      editFamily,
      trigger.id,
      trigger.label,
      immediateNodeIds,
      lazyRevisitNodeIds,
      mergeNodeIds(immediateNodeIds, lazyRevisitNodeIds),
  });
}

std::vector<TypeResolutionGraphInvalidationFanout>
buildInvalidationFanouts(const TypeResolutionGraph &graph, const InvalidationCounts &counts) {
  std::vector<TypeResolutionGraphInvalidationFanout> fanouts;
  for (const auto &node : graph.nodes) {
    if (node.kind == TypeResolutionNodeKind::LocalAuto && counts.localBinding > 0) {
      appendInvalidationFanout(
          fanouts,
          TypeResolutionGraphInvalidationEditFamily::LocalBinding,
          node,
          {node.id},
          graphClosure(graph, {node.id}, false));
    }

    if (node.kind == TypeResolutionNodeKind::LocalAuto && counts.initializerShape > 0) {
      std::vector<uint32_t> immediate{node.id};
      immediate = mergeNodeIds(std::move(immediate), directGraphTargets(graph, node.id));
      appendInvalidationFanout(
          fanouts,
          TypeResolutionGraphInvalidationEditFamily::InitializerShape,
          node,
          immediate,
          graphClosure(graph, immediate, false));
    }

    if (node.kind == TypeResolutionNodeKind::DefinitionReturn &&
        counts.definitionSignature > 0) {
      appendInvalidationFanout(
          fanouts,
          TypeResolutionGraphInvalidationEditFamily::DefinitionSignature,
          node,
          {node.id},
          graphClosure(graph, {node.id}, true));
    }

    if (node.kind == TypeResolutionNodeKind::DefinitionReturn &&
        counts.controlFlow > 0 &&
        hasScope(counts.controlFlowScopes, node.scopePath)) {
      const std::vector<uint32_t> lazyRevisits = graphClosure(graph, {node.id}, false);
      if (fanoutContainsNodeKind(
              graph, lazyRevisits, TypeResolutionNodeKind::CallConstraint)) {
        appendInvalidationFanout(
            fanouts,
            TypeResolutionGraphInvalidationEditFamily::ControlFlow,
            node,
            {node.id},
            lazyRevisits);
      }
    }

    if (node.kind == TypeResolutionNodeKind::CallConstraint && counts.importAlias > 0 &&
        !node.resolvedPath.empty()) {
      appendInvalidationFanout(
          fanouts,
          TypeResolutionGraphInvalidationEditFamily::ImportAlias,
          node,
          {node.id},
          mergeNodeIds(graphClosure(graph, {node.id}, true),
                       graphClosure(graph, {node.id}, false)));
    }

    if (node.kind == TypeResolutionNodeKind::CallConstraint && counts.receiverType > 0 &&
        hasScope(counts.receiverTypeScopes, node.scopePath)) {
      appendInvalidationFanout(
          fanouts,
          TypeResolutionGraphInvalidationEditFamily::ReceiverType,
          node,
          {node.id},
          mergeNodeIds(graphClosure(graph, {node.id}, true),
                       graphClosure(graph, {node.id}, false)));
    }
  }
  return fanouts;
}

inline std::string formatNodeIdList(const std::vector<uint32_t> &nodeIds) {
  std::ostringstream out;
  out << "[";
  for (size_t index = 0; index < nodeIds.size(); ++index) {
    if (index > 0) {
      out << ",";
    }
    out << nodeIds[index];
  }
  out << "]";
  return out.str();
}

inline bool hasTransformNamed(const std::vector<Transform> &transforms, std::string_view name) {
  for (const auto &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

inline std::optional<std::string> explicitBindingTypeName(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    return transform.name;
  }
  return std::nullopt;
}

inline std::pair<int, int> graphLocalAutoSourceLocation(const Expr &expr) {
  if (expr.sourceLine > 0 && expr.sourceColumn > 0) {
    return {expr.sourceLine, expr.sourceColumn};
  }
  if (!expr.args.empty() && expr.args.front().sourceLine > 0 && expr.args.front().sourceColumn > 0) {
    return {expr.args.front().sourceLine, expr.args.front().sourceColumn};
  }
  return {expr.sourceLine, expr.sourceColumn};
}

class TypeResolutionGraphBuilder {
public:
  explicit TypeResolutionGraphBuilder(const Program &program)
      : program_(program) {
    initializeMetadata();
  }

  TypeResolutionGraph build() {
    createDefinitionReturnNodes();
    for (const auto &def : program_.definitions) {
      traverseDefinition(def);
    }
    for (const auto &exec : program_.executions) {
      traverseExecution(exec);
    }
    return std::move(graph_);
  }

private:
  struct TraversalContext {
    std::string scopePath;
    uint32_t definitionReturnNodeId = 0;
    bool hasDefinitionReturnNode = false;
    bool definitionReturnIsInferred = false;
    size_t callOrdinal = 0;
    size_t localAutoOrdinal = 0;
  };

  const Program &program_;
  TypeResolutionGraph graph_;
  std::unordered_set<std::string> definedPaths_;
  std::unordered_set<std::string> callableDefinitionPaths_;
  std::unordered_set<std::string> publicDefinitions_;
  std::unordered_map<std::string, std::string> importAliases_;
  std::unordered_map<std::string, uint32_t> definitionReturnNodeIds_;

  void initializeMetadata() {
    definedPaths_.reserve(program_.definitions.size());
    callableDefinitionPaths_.reserve(program_.definitions.size());
    publicDefinitions_.reserve(program_.definitions.size());
    importAliases_.reserve(program_.definitions.size());

    for (const auto &def : program_.definitions) {
      definedPaths_.insert(def.fullPath);
      if (isCallableDefinition(def)) {
        callableDefinitionPaths_.insert(def.fullPath);
      }
      const bool sawPublic = hasTransformNamed(def.transforms, "public");
      const bool sawPrivate = hasTransformNamed(def.transforms, "private");
      if (sawPublic && !sawPrivate) {
        publicDefinitions_.insert(def.fullPath);
      }
    }

    auto importWildcardPrefix = [](const std::string &path, std::string &prefixOut) -> bool {
      if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/*") == 0) {
        prefixOut = path.substr(0, path.size() - 2);
        return true;
      }
      if (path.find('/', 1) == std::string::npos) {
        prefixOut = path;
        return true;
      }
      return false;
    };

    const auto &importPaths = program_.imports;
    for (const auto &importPath : importPaths) {
      std::string prefix;
      if (importWildcardPrefix(importPath, prefix)) {
        std::string scopedPrefix = prefix;
        if (!scopedPrefix.empty() && scopedPrefix.back() != '/') {
          scopedPrefix += "/";
        }
        if (prefix == collection_paths::moduleRoot(collection_paths::kVectorFolder)) {
          const std::string vectorPath = collection_paths::memberPath(
              collection_paths::kVectorFolder, collection_paths::kVectorTypeName);
          if (publicDefinitions_.count(std::string(vectorPath)) > 0) {
            importAliases_["Vector"] = vectorPath;
          }
        }
        for (const auto &def : program_.definitions) {
          if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
            continue;
          }
          const std::string remainder = def.fullPath.substr(scopedPrefix.size());
          if (remainder.empty() || remainder.find('/') != std::string::npos) {
            continue;
          }
          if (publicDefinitions_.count(def.fullPath) == 0) {
            continue;
          }
          importAliases_.emplace(remainder, def.fullPath);
        }
        continue;
      }

      const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
      if (remainder.empty() || publicDefinitions_.count(importPath) == 0) {
        continue;
      }
      importAliases_.emplace(remainder, importPath);
    }
  }

  bool isStructDefinition(const Definition &def) const {
    for (const auto &transform : def.transforms) {
      if (transform.name == "sum") {
        return false;
      }
      if (isStructTransformName(transform.name)) {
        return true;
      }
    }
    if (hasTransformNamed(def.transforms, "return")) {
      return false;
    }
    if (!def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
    }
    return !def.statements.empty();
  }

  bool isSumDefinition(const Definition &def) const {
    return hasTransformNamed(def.transforms, "sum");
  }

  bool isCallableDefinition(const Definition &def) const {
    return !isStructDefinition(def) && !isSumDefinition(def);
  }

  bool definitionReturnIsInferred(const Definition &def) const {
    for (const auto &transform : def.transforms) {
      if (transform.name != "return") {
        continue;
      }
      if (transform.templateArgs.size() != 1) {
        return true;
      }
      return transform.templateArgs.front() == "auto";
    }
    return true;
  }

  bool isLocalAutoConstraint(const Expr &expr) const {
    if (!expr.isBinding) {
      return false;
    }
    if (isCompileTimeTypeBinding(expr)) {
      return false;
    }
    const std::optional<std::string> typeName = explicitBindingTypeName(expr);
    return !typeName.has_value() || *typeName == "auto";
  }

  uint32_t addNode(TypeResolutionNodeKind kind,
                   std::string label,
                   std::string scopePath,
                   std::string resolvedPath,
                   int sourceLine,
                   int sourceColumn) {
    const uint32_t id = static_cast<uint32_t>(graph_.nodes.size());
    graph_.nodes.push_back(TypeResolutionGraphNode{
        id,
        kind,
        std::move(label),
        std::move(scopePath),
        std::move(resolvedPath),
        sourceLine,
        sourceColumn,
    });
    return id;
  }

  void addEdge(uint32_t sourceId, uint32_t targetId, TypeResolutionEdgeKind kind) {
    graph_.edges.push_back(TypeResolutionGraphEdge{sourceId, targetId, kind});
  }

  void createDefinitionReturnNodes() {
    definitionReturnNodeIds_.reserve(callableDefinitionPaths_.size());
    for (const auto &def : program_.definitions) {
      if (!isCallableDefinition(def)) {
        continue;
      }
      const uint32_t nodeId = addNode(
          TypeResolutionNodeKind::DefinitionReturn, def.fullPath, def.fullPath, def.fullPath, def.sourceLine, def.sourceColumn);
      definitionReturnNodeIds_.emplace(def.fullPath, nodeId);
    }
  }

  std::string resolveCallTargetPath(const Expr &expr) const {
    if (expr.name.empty()) {
      return {};
    }
    if (expr.name.front() == '/') {
      return expr.name;
    }
    if (expr.name.find('/') != std::string::npos) {
      return "/" + expr.name;
    }
    if (!expr.namespacePrefix.empty()) {
      std::string normalizedPrefix = expr.namespacePrefix;
      if (normalizedPrefix.front() != '/') {
        normalizedPrefix.insert(normalizedPrefix.begin(), '/');
      }
      const size_t lastSlash = normalizedPrefix.find_last_of('/');
      const std::string_view suffix =
          lastSlash == std::string::npos ? std::string_view(normalizedPrefix)
                                         : std::string_view(normalizedPrefix).substr(lastSlash + 1);
      if (suffix == expr.name && definedPaths_.count(normalizedPrefix) > 0) {
        return normalizedPrefix;
      }
      std::string prefix = normalizedPrefix;
      while (!prefix.empty()) {
        const std::string candidate = prefix + "/" + expr.name;
        if (definedPaths_.count(candidate) > 0) {
          return candidate;
        }
        const size_t slash = prefix.find_last_of('/');
        if (slash == std::string::npos) {
          break;
        }
        prefix = prefix.substr(0, slash);
      }
      auto importIt = importAliases_.find(expr.name);
      if (importIt != importAliases_.end()) {
        return importIt->second;
      }
      return normalizedPrefix + "/" + expr.name;
    }
    auto importIt = importAliases_.find(expr.name);
    if (importIt != importAliases_.end()) {
      return importIt->second;
    }
    if (expr.isMethodCall && !expr.isFieldAccess) {
      const std::string suffix = "/" + expr.name;
      std::string uniqueMethodTarget;
      for (const auto &def : program_.definitions) {
        if (callableDefinitionPaths_.count(def.fullPath) == 0 ||
            def.fullPath.size() < suffix.size() ||
            def.fullPath.compare(def.fullPath.size() - suffix.size(),
                                 suffix.size(),
                                 suffix) != 0) {
          continue;
        }
        if (!uniqueMethodTarget.empty()) {
          return {};
        }
        uniqueMethodTarget = def.fullPath;
      }
      if (!uniqueMethodTarget.empty()) {
        return uniqueMethodTarget;
      }
      return {};
    }
    return "/" + expr.name;
  }

  bool isGraphCallSite(const Expr &expr, std::string &resolvedPathOut) const {
    if (expr.kind != Expr::Kind::Call || expr.isBinding) {
      return false;
    }
    resolvedPathOut = resolveCallTargetPath(expr);
    return callableDefinitionPaths_.count(resolvedPathOut) > 0;
  }

  void appendCollectedCalls(std::vector<uint32_t> *callIdsOut, const std::vector<uint32_t> &callIds) {
    if (callIdsOut == nullptr) {
      return;
    }
    callIdsOut->insert(callIdsOut->end(), callIds.begin(), callIds.end());
  }

  void visitExpr(const Expr &expr, TraversalContext &context, std::vector<uint32_t> *callIdsOut) {
    if (expr.isBinding) {
      std::vector<uint32_t> initializerCallIds;
      for (const auto &arg : expr.args) {
        visitExpr(arg, context, &initializerCallIds);
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        visitExpr(bodyExpr, context, &initializerCallIds);
      }
      if (isLocalAutoConstraint(expr)) {
        const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(expr);
        const std::string label =
            context.scopePath + "::auto:" + expr.name + "#" + std::to_string(context.localAutoOrdinal++);
        const uint32_t localNodeId = addNode(
            TypeResolutionNodeKind::LocalAuto, label, context.scopePath, {}, sourceLine, sourceColumn);
        for (uint32_t callNodeId : initializerCallIds) {
          addEdge(localNodeId, callNodeId, TypeResolutionEdgeKind::Dependency);
        }
      }
      appendCollectedCalls(callIdsOut, initializerCallIds);
      return;
    }

    if (isReturnCall(expr)) {
      std::vector<uint32_t> returnCallIds;
      for (const auto &arg : expr.args) {
        visitExpr(arg, context, &returnCallIds);
      }
      for (const auto &bodyExpr : expr.bodyArguments) {
        visitExpr(bodyExpr, context, &returnCallIds);
      }
      if (context.hasDefinitionReturnNode) {
        const TypeResolutionEdgeKind edgeKind = context.definitionReturnIsInferred
                                                    ? TypeResolutionEdgeKind::Dependency
                                                    : TypeResolutionEdgeKind::Requirement;
        for (uint32_t callNodeId : returnCallIds) {
          addEdge(context.definitionReturnNodeId, callNodeId, edgeKind);
        }
      }
      appendCollectedCalls(callIdsOut, returnCallIds);
      return;
    }

    std::string resolvedPath;
    if (isGraphCallSite(expr, resolvedPath)) {
      const std::string label =
          context.scopePath + "::call#" + std::to_string(context.callOrdinal++);
      const uint32_t callNodeId = addNode(
          TypeResolutionNodeKind::CallConstraint,
          label,
          context.scopePath,
          resolvedPath,
          expr.sourceLine,
          expr.sourceColumn);
      auto definitionNodeIt = definitionReturnNodeIds_.find(resolvedPath);
      if (definitionNodeIt != definitionReturnNodeIds_.end()) {
        addEdge(callNodeId, definitionNodeIt->second, TypeResolutionEdgeKind::Dependency);
      }
      if (callIdsOut != nullptr) {
        callIdsOut->push_back(callNodeId);
      }
    }

    for (const auto &arg : expr.args) {
      visitExpr(arg, context, callIdsOut);
    }
    for (const auto &bodyExpr : expr.bodyArguments) {
      visitExpr(bodyExpr, context, callIdsOut);
    }
  }

  void traverseDefinition(const Definition &def) {
    TraversalContext context;
    context.scopePath = def.fullPath;
    auto definitionNodeIt = definitionReturnNodeIds_.find(def.fullPath);
    if (definitionNodeIt != definitionReturnNodeIds_.end()) {
      context.definitionReturnNodeId = definitionNodeIt->second;
      context.hasDefinitionReturnNode = true;
      context.definitionReturnIsInferred = definitionReturnIsInferred(def);
    }

    for (const auto &param : def.parameters) {
      for (const auto &arg : param.args) {
        visitExpr(arg, context, nullptr);
      }
      for (const auto &bodyExpr : param.bodyArguments) {
        visitExpr(bodyExpr, context, nullptr);
      }
    }

    for (const auto &stmt : def.statements) {
      if (def.returnExpr.has_value() && isReturnCall(stmt)) {
        continue;
      }
      visitExpr(stmt, context, nullptr);
    }

    if (def.returnExpr.has_value() && context.hasDefinitionReturnNode) {
      std::vector<uint32_t> returnExprCallIds;
      visitExpr(*def.returnExpr, context, &returnExprCallIds);
      const TypeResolutionEdgeKind edgeKind = context.definitionReturnIsInferred
                                                  ? TypeResolutionEdgeKind::Dependency
                                                  : TypeResolutionEdgeKind::Requirement;
      for (uint32_t callNodeId : returnExprCallIds) {
        addEdge(context.definitionReturnNodeId, callNodeId, edgeKind);
      }
    }
  }

  void traverseExecution(const Execution &exec) {
    TraversalContext context;
    context.scopePath = exec.fullPath;

    Expr executionCall;
    executionCall.kind = Expr::Kind::Call;
    executionCall.name = exec.name.empty() ? exec.fullPath : exec.name;
    executionCall.namespacePrefix = exec.namespacePrefix;
    executionCall.args = exec.arguments;
    executionCall.argNames = exec.argumentNames;
    executionCall.bodyArguments = exec.bodyArguments;
    executionCall.hasBodyArguments = exec.hasBodyArguments;
    executionCall.templateArgs = exec.templateArgs;
    executionCall.sourceLine = exec.sourceLine;
    executionCall.sourceColumn = exec.sourceColumn;
    visitExpr(executionCall, context, nullptr);
  }
};

} // namespace typeResolutionGraphBuilder
} // namespace primec::semantics
