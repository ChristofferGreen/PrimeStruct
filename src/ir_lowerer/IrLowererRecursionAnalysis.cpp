#include "IrLowererRecursionAnalysis.h"

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

} // namespace

std::unordered_set<std::string> findRecursiveDefinitionPaths(const Program &program) {
  std::unordered_set<std::string> nodePaths;
  nodePaths.reserve(program.definitions.size());
  for (const Definition &def : program.definitions) {
    nodePaths.insert(def.fullPath);
  }

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

} // namespace primec::ir_lowerer
