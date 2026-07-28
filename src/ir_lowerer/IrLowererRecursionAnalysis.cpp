#include "IrLowererRecursionAnalysis.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererSharedTypes.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <unordered_map>
#include <vector>

namespace primec::ir_lowerer {

namespace {

void collectCallEdges(const Expr &expr, std::vector<std::string> &out) {
  if (expr.kind == Expr::Kind::Call && !expr.resolvedCallPath.empty()) {
    out.push_back(expr.resolvedCallPath);
  }
  for (const Expr &arg : expr.args) {
    collectCallEdges(arg, out);
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    collectCallEdges(bodyArg, out);
  }
}

enum class VisitState { Unvisited, OnStack, Done };

struct DfsFrame {
  size_t pathIndex = 0;
  size_t edgeIndex = 0;
};

std::unordered_map<std::string, std::vector<std::string>> buildCallEdgeMap(const Program &program) {
  std::unordered_map<std::string, std::vector<std::string>> edges;
  edges.reserve(program.definitions.size());
  for (const Definition &def : program.definitions) {
    std::vector<std::string> &calleeList = edges[def.fullPath];
    for (const Expr &stmt : def.statements) {
      collectCallEdges(stmt, calleeList);
    }
    if (def.returnExpr.has_value()) {
      collectCallEdges(*def.returnExpr, calleeList);
    }
  }
  return edges;
}

bool isSupportedScalarTypeName(const std::string &typeName) {
  switch (valueKindFromTypeName(typeName)) {
    case LocalInfo::ValueKind::Int32:
    case LocalInfo::ValueKind::Int64:
    case LocalInfo::ValueKind::UInt64:
    case LocalInfo::ValueKind::Float32:
    case LocalInfo::ValueKind::Float64:
    case LocalInfo::ValueKind::Bool:
      return true;
    default:
      return false;
  }
}

// Best-effort static extraction of an explicit, plain (non-templated,
// non-args-pack) type name from a parameter's transforms, mirroring the
// scan IrLowererLowerInlineCalls.h's extractCollectionParameterTypeName
// uses for the same purpose. Returns empty when no such transform is found
// (e.g. a struct type, a templated collection type, or an args-pack
// parameter) - callers treat that as "not statically known to be scalar".
std::string extractParameterTypeNameStatic(const Expr &paramExpr) {
  for (const auto &transform : paramExpr.transforms) {
    if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
        transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
        transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
        transform.name == "effects" || transform.name == "capabilities" || transform.name == "args") {
      continue;
    }
    if (!transform.arguments.empty() || !transform.templateArgs.empty()) {
      return {};
    }
    return transform.name;
  }
  return {};
}

bool hasOnlyScalarParameters(const Definition &def) {
  for (const Expr &param : def.parameters) {
    if (isArgsPackBinding(param)) {
      return false;
    }
    const std::string typeName = extractParameterTypeNameStatic(param);
    if (typeName.empty() || !isSupportedScalarTypeName(typeName)) {
      return false;
    }
  }
  return true;
}

bool hasScalarOrVoidReturn(const Definition &def) {
  for (const auto &transform : def.transforms) {
    if (transform.name != "return") {
      continue;
    }
    if (transform.templateArgs.size() != 1) {
      return false;
    }
    const std::string returnType = trimTemplateTypeText(transform.templateArgs.front());
    if (returnType == "void") {
      return true;
    }
    return isSupportedScalarTypeName(returnType);
  }
  // No explicit return<T> transform: this codebase's convention is that a
  // definition without one is void (see e.g. the "empty void main" fixture
  // pattern), so treat absence the same as an explicit return<void>.
  return true;
}

} // namespace

std::unordered_set<std::string> findRecursiveDefinitionPaths(const Program &program) {
  std::unordered_set<std::string> nodePaths;
  nodePaths.reserve(program.definitions.size());
  for (const Definition &def : program.definitions) {
    nodePaths.insert(def.fullPath);
  }

  const std::unordered_map<std::string, std::vector<std::string>> edges = buildCallEdgeMap(program);

  std::unordered_set<std::string> recursive;
  std::unordered_map<std::string, VisitState> visitState;
  std::unordered_map<std::string, size_t> stackPosition;
  visitState.reserve(nodePaths.size());

  for (const Definition &startDef : program.definitions) {
    if (visitState[startDef.fullPath] != VisitState::Unvisited) {
      continue;
    }

    std::vector<std::string> pathStack;
    std::vector<DfsFrame> dfsStack;
    visitState[startDef.fullPath] = VisitState::OnStack;
    stackPosition[startDef.fullPath] = pathStack.size();
    pathStack.push_back(startDef.fullPath);
    dfsStack.push_back({pathStack.size() - 1, 0});

    while (!dfsStack.empty()) {
      const size_t frameIndex = dfsStack.size() - 1;
      const std::string currentPath = pathStack[dfsStack[frameIndex].pathIndex];
      const auto edgeIt = edges.find(currentPath);
      const std::vector<std::string> *callees =
          edgeIt != edges.end() ? &edgeIt->second : nullptr;

      if (callees != nullptr && dfsStack[frameIndex].edgeIndex < callees->size()) {
        const std::string callee = (*callees)[dfsStack[frameIndex].edgeIndex];
        ++dfsStack[frameIndex].edgeIndex;
        if (nodePaths.find(callee) == nodePaths.end()) {
          continue;
        }
        const VisitState calleeState = visitState[callee];
        if (calleeState == VisitState::Unvisited) {
          visitState[callee] = VisitState::OnStack;
          stackPosition[callee] = pathStack.size();
          pathStack.push_back(callee);
          dfsStack.push_back({pathStack.size() - 1, 0});
        } else if (calleeState == VisitState::OnStack) {
          const size_t startPos = stackPosition[callee];
          for (size_t i = startPos; i < pathStack.size(); ++i) {
            recursive.insert(pathStack[i]);
          }
        }
      } else {
        visitState[currentPath] = VisitState::Done;
        pathStack.pop_back();
        dfsStack.pop_back();
      }
    }
  }

  return recursive;
}

std::unordered_set<std::string> findReachableDefinitionPaths(const Program &program,
                                                              const std::string &entryPath) {
  std::unordered_set<std::string> nodePaths;
  nodePaths.reserve(program.definitions.size());
  for (const Definition &def : program.definitions) {
    nodePaths.insert(def.fullPath);
  }
  if (nodePaths.find(entryPath) == nodePaths.end()) {
    return {};
  }

  const std::unordered_map<std::string, std::vector<std::string>> edges = buildCallEdgeMap(program);

  std::unordered_set<std::string> reachable;
  std::vector<std::string> worklist;
  reachable.insert(entryPath);
  worklist.push_back(entryPath);
  while (!worklist.empty()) {
    const std::string current = std::move(worklist.back());
    worklist.pop_back();
    const auto edgeIt = edges.find(current);
    if (edgeIt == edges.end()) {
      continue;
    }
    for (const std::string &callee : edgeIt->second) {
      if (nodePaths.find(callee) == nodePaths.end()) {
        continue;
      }
      if (reachable.insert(callee).second) {
        worklist.push_back(callee);
      }
    }
  }
  return reachable;
}

std::unordered_set<std::string> computeRealCallEligibleDefinitionPaths(const Program &program,
                                                                       const std::string &entryPath) {
  const std::unordered_set<std::string> recursive = findRecursiveDefinitionPaths(program);
  if (recursive.empty()) {
    return {};
  }
  const std::unordered_set<std::string> reachable = findReachableDefinitionPaths(program, entryPath);

  std::unordered_map<std::string, const Definition *> byPath;
  byPath.reserve(program.definitions.size());
  for (const Definition &def : program.definitions) {
    byPath.emplace(def.fullPath, &def);
  }

  std::unordered_set<std::string> eligible;
  for (const std::string &path : recursive) {
    if (reachable.find(path) == reachable.end()) {
      continue;
    }
    const auto defIt = byPath.find(path);
    if (defIt == byPath.end()) {
      continue;
    }
    const Definition &def = *defIt->second;
    if (definitionHasTransform(def, "struct") || definitionHasTransform(def, "sum") ||
        definitionHasTransform(def, "compute") || definitionHasTransform(def, "on_error")) {
      continue;
    }
    if (!hasOnlyScalarParameters(def) || !hasScalarOrVoidReturn(def)) {
      continue;
    }
    eligible.insert(path);
  }
  return eligible;
}

} // namespace primec::ir_lowerer
