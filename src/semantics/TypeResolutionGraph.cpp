#include "TypeResolutionGraph.h"

#include "CondensationDag.h"
#include "SemanticsHelpers.h"
#include "TypeResolutionGraphPreparation.h"
#include "primec/testing/SemanticsGraphHelpers.h"

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

namespace {

std::optional<uint64_t> readGraphMetricBudget(const char *envName) {
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
};

uint64_t &invalidationCountForFamily(InvalidationCounts &counts,
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

uint64_t invalidationCountForFamily(const InvalidationCounts &counts,
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

void recordInvalidationObservation(InvalidationCounts &counts,
                                   TypeResolutionGraphInvalidationEditFamily editFamily) {
  ++invalidationCountForFamily(counts, editFamily);
}

void setTypeResolutionGraphInvalidationCount(
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

void countInvalidationExpr(const Expr &expr, InvalidationCounts &counts) {
  if (isIfCall(expr) || isMatchCall(expr)) {
    recordInvalidationObservation(counts, TypeResolutionGraphInvalidationEditFamily::ControlFlow);
  }
  if (expr.isMethodCall && !expr.isFieldAccess) {
    recordInvalidationObservation(counts, TypeResolutionGraphInvalidationEditFamily::ReceiverType);
  }
  for (const auto &arg : expr.args) {
    countInvalidationExpr(arg, counts);
  }
  for (const auto &bodyExpr : expr.bodyArguments) {
    countInvalidationExpr(bodyExpr, counts);
  }
}

InvalidationCounts computeInvalidationCounts(const Program &program) {
  InvalidationCounts counts;
  for (size_t index = 0; index < program.definitions.size(); ++index) {
    recordInvalidationObservation(
        counts, TypeResolutionGraphInvalidationEditFamily::DefinitionSignature);
  }
  for (size_t index = 0; index < program.imports.size(); ++index) {
    recordInvalidationObservation(counts, TypeResolutionGraphInvalidationEditFamily::ImportAlias);
  }
  for (const auto &def : program.definitions) {
    for (const auto &param : def.parameters) {
      countInvalidationExpr(param, counts);
    }
    for (const auto &stmt : def.statements) {
      if (def.returnExpr.has_value() && isReturnCall(stmt)) {
        continue;
      }
      if (stmt.isBinding) {
        recordInvalidationObservation(counts, TypeResolutionGraphInvalidationEditFamily::LocalBinding);
        if (!stmt.args.empty() || !stmt.bodyArguments.empty()) {
          recordInvalidationObservation(
              counts, TypeResolutionGraphInvalidationEditFamily::InitializerShape);
        }
      }
      countInvalidationExpr(stmt, counts);
    }
    if (def.returnExpr.has_value()) {
      countInvalidationExpr(*def.returnExpr, counts);
    }
  }
  for (const auto &exec : program.executions) {
    for (const auto &arg : exec.arguments) {
      countInvalidationExpr(arg, counts);
    }
    for (const auto &bodyExpr : exec.bodyArguments) {
      countInvalidationExpr(bodyExpr, counts);
    }
  }
  return counts;
}

bool hasTransformNamed(const std::vector<Transform> &transforms, std::string_view name) {
  for (const auto &transform : transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> explicitBindingTypeName(const Expr &expr) {
  for (const auto &transform : expr.transforms) {
    if (isBindingAuxTransformName(transform.name)) {
      continue;
    }
    return transform.name;
  }
  return std::nullopt;
}

std::pair<int, int> graphLocalAutoSourceLocation(const Expr &expr) {
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

  bool isCallableDefinition(const Definition &def) const {
    return !isStructDefinition(def);
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

} // namespace

std::string_view typeResolutionNodeKindName(TypeResolutionNodeKind kind) {
  switch (kind) {
  case TypeResolutionNodeKind::DefinitionReturn:
    return "definition_return";
  case TypeResolutionNodeKind::CallConstraint:
    return "call_constraint";
  case TypeResolutionNodeKind::LocalAuto:
    return "local_auto";
  }
  return "definition_return";
}

std::string_view typeResolutionEdgeKindName(TypeResolutionEdgeKind kind) {
  switch (kind) {
  case TypeResolutionEdgeKind::Dependency:
    return "dependency";
  case TypeResolutionEdgeKind::Requirement:
    return "requirement";
  }
  return "dependency";
}

std::string_view typeResolutionGraphInvalidationEditFamilyName(
    TypeResolutionGraphInvalidationEditFamily editFamily) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    return "local_binding";
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    return "control_flow";
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    return "initializer_shape";
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    return "definition_signature";
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    return "import_alias";
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    return "receiver_type";
  }
  return "local_binding";
}

std::string_view typeResolutionGraphInvalidationPropagationName(
    TypeResolutionGraphInvalidationPropagation propagation) {
  switch (propagation) {
  case TypeResolutionGraphInvalidationPropagation::DefinitionLocal:
    return "definition_local";
  case TypeResolutionGraphInvalidationPropagation::CrossDefinition:
    return "cross_definition";
  }
  return "definition_local";
}

const std::vector<TypeResolutionGraphInvalidationContract> &
typeResolutionGraphInvalidationContracts() {
  static const std::vector<TypeResolutionGraphInvalidationContract> Contracts = {
      {TypeResolutionGraphInvalidationEditFamily::LocalBinding,
       "local_binding",
       TypeResolutionGraphInvalidationPropagation::DefinitionLocal,
       "local_auto, binding, query, try, on_error nodes in the edited definition",
       "binding/result consumers reached from the edited definition's dependency edges",
       "diagnostics attached to the edited binding and its dependent local facts"},
      {TypeResolutionGraphInvalidationEditFamily::ControlFlow,
       "control_flow",
       TypeResolutionGraphInvalidationPropagation::DefinitionLocal,
       "definition-return, branch-local binding, and control-dependent query nodes",
       "return/result consumers whose dependency edges touch the edited control-flow region",
       "branch reachability and return-consistency diagnostics in the edited definition"},
      {TypeResolutionGraphInvalidationEditFamily::InitializerShape,
       "initializer_shape",
       TypeResolutionGraphInvalidationPropagation::DefinitionLocal,
       "local_auto and call_constraint nodes owned by the edited initializer",
       "initializer binding/result queries and downstream local facts in dependency order",
       "initializer type, result-shape, and helper-resolution diagnostics for the edited site"},
      {TypeResolutionGraphInvalidationEditFamily::DefinitionSignature,
       "definition_signature",
       TypeResolutionGraphInvalidationPropagation::CrossDefinition,
       "definition_return nodes for the edited definition and directly dependent call sites",
       "dependent call_constraint, binding, result, and return nodes across import boundaries",
       "call-compatibility, return-contract, and template-argument diagnostics at dependent sites"},
      {TypeResolutionGraphInvalidationEditFamily::ImportAlias,
       "import_alias",
       TypeResolutionGraphInvalidationPropagation::CrossDefinition,
       "call_constraint nodes whose canonical path or helper-shadow choice used the alias",
       "dependent helper-routing, binding, and result queries in deterministic path order",
       "unresolved import, ambiguous helper, and alias-derived call diagnostics"},
      {TypeResolutionGraphInvalidationEditFamily::ReceiverType,
       "receiver_type",
       TypeResolutionGraphInvalidationPropagation::CrossDefinition,
       "method call_constraint nodes and receiver-derived helper-family selections",
       "dependent method targets, binding/result facts, and helper-routing queries",
       "method-target, receiver-binding, and helper-family diagnostics at dependent sites"},
  };
  return Contracts;
}

const TypeResolutionGraphInvalidationContract *
typeResolutionGraphInvalidationContract(
    TypeResolutionGraphInvalidationEditFamily editFamily) {
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    if (contract.editFamily == editFamily) {
      return &contract;
    }
  }
  return nullptr;
}

uint64_t typeResolutionGraphInvalidationCount(
    const TypeResolutionGraph &graph,
    TypeResolutionGraphInvalidationEditFamily editFamily) {
  switch (editFamily) {
  case TypeResolutionGraphInvalidationEditFamily::LocalBinding:
    return graph.invalidationLocalBindingCount;
  case TypeResolutionGraphInvalidationEditFamily::ControlFlow:
    return graph.invalidationControlFlowCount;
  case TypeResolutionGraphInvalidationEditFamily::InitializerShape:
    return graph.invalidationInitializerShapeCount;
  case TypeResolutionGraphInvalidationEditFamily::DefinitionSignature:
    return graph.invalidationDefinitionSignatureCount;
  case TypeResolutionGraphInvalidationEditFamily::ImportAlias:
    return graph.invalidationImportAliasCount;
  case TypeResolutionGraphInvalidationEditFamily::ReceiverType:
    return graph.invalidationReceiverTypeCount;
  }
  return 0;
}

TypeResolutionGraph buildTypeResolutionGraph(const Program &program) {
  const auto start = std::chrono::steady_clock::now();
  TypeResolutionGraphBuilder builder(program);
  TypeResolutionGraph graph = builder.build();
  const auto end = std::chrono::steady_clock::now();
  graph.buildMillis =
      static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  return graph;
}

bool buildTypeResolutionGraphForProgram(Program program,
                                        const std::string &entryPath,
                                        const std::vector<std::string> &semanticTransforms,
                                        std::string &error,
                                        TypeResolutionGraph &out) {
  error.clear();
  out = {};
  const auto prepStart = std::chrono::steady_clock::now();
  uint64_t explicitTemplateArgFactHitCount = 0;
  uint64_t implicitTemplateArgFactHitCount = 0;
  // Keep this baseline call-shape documented for architecture guard tests:
  // prepareProgramForTypeResolutionAnalysis(program, entryPath, semanticTransforms, error)
  if (!prepareProgramForTypeResolutionAnalysis(program,
                                               entryPath,
                                               semanticTransforms,
                                               error,
                                               &explicitTemplateArgFactHitCount,
                                               &implicitTemplateArgFactHitCount)) {
    return false;
  }
  const auto prepEnd = std::chrono::steady_clock::now();
  const InvalidationCounts invalidationCounts = computeInvalidationCounts(program);
  out = buildTypeResolutionGraph(program);
  out.prepareMillis =
      static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(prepEnd - prepStart).count());
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    setTypeResolutionGraphInvalidationCount(
        out,
        contract.editFamily,
        invalidationCountForFamily(invalidationCounts, contract.editFamily));
  }
  out.explicitTemplateArgInferenceFactHitCount = explicitTemplateArgFactHitCount;
  out.implicitTemplateArgInferenceFactHitCount = implicitTemplateArgFactHitCount;
  if (const auto maxPrepare = readGraphMetricBudget("PRIMESTRUCT_GRAPH_PREPARE_MS_MAX");
      maxPrepare.has_value()) {
    out.prepareMaxMillis = *maxPrepare;
    out.prepareOverBudget = out.prepareMillis > out.prepareMaxMillis;
  }
  if (const auto maxBuild = readGraphMetricBudget("PRIMESTRUCT_GRAPH_BUILD_MS_MAX");
      maxBuild.has_value()) {
    out.buildMaxMillis = *maxBuild;
    out.buildOverBudget = out.buildMillis > out.buildMaxMillis;
  }
  return true;
}

CondensationDag computeTypeResolutionDependencyDag(const TypeResolutionGraph &graph) {
  std::vector<DirectedGraphEdge> dependencyEdges;
  dependencyEdges.reserve(graph.edges.size());
  for (const TypeResolutionGraphEdge &edge : graph.edges) {
    if (edge.kind != TypeResolutionEdgeKind::Dependency) {
      continue;
    }
    dependencyEdges.push_back(DirectedGraphEdge{edge.sourceId, edge.targetId});
  }
  return computeCondensationDag(static_cast<uint32_t>(graph.nodes.size()), dependencyEdges);
}

std::string formatTypeResolutionGraph(const TypeResolutionGraph &graph) {
  std::ostringstream out;
  out << "type_graph {\n";
  size_t definitionReturnCount = 0;
  size_t callConstraintCount = 0;
  size_t localAutoCount = 0;
  for (const auto &node : graph.nodes) {
    switch (node.kind) {
      case TypeResolutionNodeKind::DefinitionReturn:
        ++definitionReturnCount;
        break;
      case TypeResolutionNodeKind::CallConstraint:
        ++callConstraintCount;
        break;
      case TypeResolutionNodeKind::LocalAuto:
        ++localAutoCount;
        break;
    }
  }
  size_t dependencyEdgeCount = 0;
  size_t requirementEdgeCount = 0;
  for (const auto &edge : graph.edges) {
    switch (edge.kind) {
      case TypeResolutionEdgeKind::Dependency:
        ++dependencyEdgeCount;
        break;
      case TypeResolutionEdgeKind::Requirement:
        ++requirementEdgeCount;
        break;
    }
  }
  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);
  size_t maxSccSize = 0;
  for (const auto &component : dag.nodes) {
    maxSccSize = std::max(maxSccSize, component.memberNodeIds.size());
  }
  out << "  metrics prepare_ms=" << graph.prepareMillis
      << " build_ms=" << graph.buildMillis
      << " prepare_ms_max=" << graph.prepareMaxMillis
      << " build_ms_max=" << graph.buildMaxMillis
      << " prepare_over=" << (graph.prepareOverBudget ? "true" : "false")
      << " build_over=" << (graph.buildOverBudget ? "true" : "false")
      << " nodes=" << graph.nodes.size()
      << " edges=" << graph.edges.size()
      << " nodes_definition_return=" << definitionReturnCount
      << " nodes_call_constraint=" << callConstraintCount
      << " nodes_local_auto=" << localAutoCount
      << " edges_dependency=" << dependencyEdgeCount
      << " edges_requirement=" << requirementEdgeCount
      << " scc_count=" << dag.nodes.size()
      << " scc_max_size=" << maxSccSize
      << " invalidation_local_binding=" << graph.invalidationLocalBindingCount
      << " invalidation_control_flow=" << graph.invalidationControlFlowCount
      << " invalidation_initializer_shape=" << graph.invalidationInitializerShapeCount
      << " invalidation_definition_signature=" << graph.invalidationDefinitionSignatureCount
      << " invalidation_import_alias=" << graph.invalidationImportAliasCount
      << " invalidation_receiver_type=" << graph.invalidationReceiverTypeCount << "\n";
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    out << "  invalidation_contract name=" << contract.name
        << " propagation=" << typeResolutionGraphInvalidationPropagationName(contract.propagation)
        << " observed=" << typeResolutionGraphInvalidationCount(graph, contract.editFamily)
        << " immediate=\"" << contract.immediateInvalidations << "\""
        << " lazy=\"" << contract.lazyRevisits << "\""
        << " diagnostics=\"" << contract.diagnosticDiscards << "\"\n";
  }
  for (const auto &node : graph.nodes) {
    out << "  node " << node.id << " kind=" << typeResolutionNodeKindName(node.kind)
        << " label=\"" << node.label << "\""
        << " scope=\"" << node.scopePath << "\""
        << " path=\"" << node.resolvedPath << "\""
        << " line=" << node.sourceLine
        << " column=" << node.sourceColumn << "\n";
  }
  for (size_t edgeIndex = 0; edgeIndex < graph.edges.size(); ++edgeIndex) {
    const auto &edge = graph.edges[edgeIndex];
    out << "  edge " << edgeIndex << " kind=" << typeResolutionEdgeKindName(edge.kind)
        << " source=" << edge.sourceId
        << " target=" << edge.targetId << "\n";
  }
  out << "}\n";
  return out.str();
}

bool buildTypeResolutionGraphForTesting(Program program,
                                        const std::string &entryPath,
                                        std::string &error,
                                        TypeResolutionGraphSnapshot &out,
                                        const std::vector<std::string> &semanticTransforms) {
  TypeResolutionGraph graph;
  if (!buildTypeResolutionGraphForProgram(std::move(program), entryPath, semanticTransforms, error, graph)) {
    out = {};
    return false;
  }
  out = {};
  out.nodeCount = graph.nodes.size();
  out.edgeCount = graph.edges.size();
  for (const auto &node : graph.nodes) {
    switch (node.kind) {
      case TypeResolutionNodeKind::DefinitionReturn:
        ++out.definitionReturnCount;
        break;
      case TypeResolutionNodeKind::CallConstraint:
        ++out.callConstraintCount;
        break;
      case TypeResolutionNodeKind::LocalAuto:
        ++out.localAutoCount;
        break;
    }
  }
  for (const auto &edge : graph.edges) {
    switch (edge.kind) {
      case TypeResolutionEdgeKind::Dependency:
        ++out.dependencyEdgeCount;
        break;
      case TypeResolutionEdgeKind::Requirement:
        ++out.requirementEdgeCount;
        break;
    }
  }
  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);
  size_t maxSccSize = 0;
  for (const auto &component : dag.nodes) {
    maxSccSize = std::max(maxSccSize, component.memberNodeIds.size());
  }
  out.sccCount = dag.nodes.size();
  out.sccMaxSize = maxSccSize;
  out.prepareMillis = graph.prepareMillis;
  out.buildMillis = graph.buildMillis;
  out.prepareMaxMillis = graph.prepareMaxMillis;
  out.buildMaxMillis = graph.buildMaxMillis;
  out.prepareOverBudget = graph.prepareOverBudget;
  out.buildOverBudget = graph.buildOverBudget;
  out.invalidationLocalBindingCount = graph.invalidationLocalBindingCount;
  out.invalidationControlFlowCount = graph.invalidationControlFlowCount;
  out.invalidationInitializerShapeCount = graph.invalidationInitializerShapeCount;
  out.invalidationDefinitionSignatureCount = graph.invalidationDefinitionSignatureCount;
  out.invalidationImportAliasCount = graph.invalidationImportAliasCount;
  out.invalidationReceiverTypeCount = graph.invalidationReceiverTypeCount;
  out.explicitTemplateArgInferenceFactHitCount = graph.explicitTemplateArgInferenceFactHitCount;
  out.implicitTemplateArgInferenceFactHitCount = graph.implicitTemplateArgInferenceFactHitCount;
  out.nodes.reserve(graph.nodes.size());
  for (const auto &node : graph.nodes) {
    out.nodes.push_back(TypeResolutionGraphSnapshotNode{
        node.id,
        std::string(typeResolutionNodeKindName(node.kind)),
        node.label,
        node.scopePath,
        node.resolvedPath,
        node.sourceLine,
        node.sourceColumn,
    });
  }
  out.edges.reserve(graph.edges.size());
  for (const auto &edge : graph.edges) {
    out.edges.push_back(TypeResolutionGraphSnapshotEdge{
        edge.sourceId,
        edge.targetId,
        std::string(typeResolutionEdgeKindName(edge.kind)),
    });
  }
  out.invalidationContracts.reserve(typeResolutionGraphInvalidationContracts().size());
  for (const auto &contract : typeResolutionGraphInvalidationContracts()) {
    out.invalidationContracts.push_back(TypeResolutionGraphInvalidationContractSnapshot{
        std::string(contract.name),
        std::string(typeResolutionGraphInvalidationPropagationName(contract.propagation)),
        std::string(contract.immediateInvalidations),
        std::string(contract.lazyRevisits),
        std::string(contract.diagnosticDiscards),
        typeResolutionGraphInvalidationCount(graph, contract.editFamily),
    });
  }
  return true;
}

bool computeTypeResolutionDependencyDagForTesting(Program program,
                                                  const std::string &entryPath,
                                                  std::string &error,
                                                  CondensationDagSnapshot &out,
                                                  const std::vector<std::string> &semanticTransforms) {
  TypeResolutionGraph graph;
  if (!buildTypeResolutionGraphForProgram(std::move(program), entryPath, semanticTransforms, error, graph)) {
    out = {};
    return false;
  }

  const CondensationDag dag = computeTypeResolutionDependencyDag(graph);
  out = {};
  out.componentIdByNodeId = dag.componentIdByNodeId;
  out.topologicalComponentIds = dag.topologicalComponentIds;
  out.nodes.reserve(dag.nodes.size());
  for (const CondensationDagNode &node : dag.nodes) {
    out.nodes.push_back(CondensationDagNodeSnapshot{
        node.componentId,
        node.memberNodeIds,
        node.incomingComponentIds,
        node.outgoingComponentIds,
    });
  }
  out.edges.reserve(dag.edges.size());
  for (const CondensationDagEdge &edge : dag.edges) {
    out.edges.push_back(CondensationDagEdgeSnapshot{
        edge.sourceComponentId,
        edge.targetComponentId,
    });
  }
  return true;
}

bool dumpTypeResolutionGraphForTesting(Program program,
                                       const std::string &entryPath,
                                       std::string &error,
                                       std::string &out,
                                       const std::vector<std::string> &semanticTransforms) {
  TypeResolutionGraph graph;
  if (!buildTypeResolutionGraphForProgram(std::move(program), entryPath, semanticTransforms, error, graph)) {
    out.clear();
    return false;
  }
  out = formatTypeResolutionGraph(graph);
  return true;
}

} // namespace primec::semantics
