#include "IrLowererRecursionAnalysis.h"

#include "IrLowererBindingTransformHelpers.h"
#include "IrLowererCallHelpers.h"
#include "IrLowererSetupTypeHelpers.h"
#include "IrLowererSharedTypes.h"
#include "IrLowererTemplateTypeParseHelpers.h"

#include <unordered_map>
#include <vector>

namespace primec::ir_lowerer {

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

bool paramHasMutTransform(const Expr &paramExpr) {
  for (const auto &transform : paramExpr.transforms) {
    if (transform.name == "mut") {
      return true;
    }
  }
  return false;
}

// `try(...)` calls (both the explicit form and the `expr?` postfix, which
// the parser desugars to `try(expr)` at parse time - see ParserExpr.cpp)
// read the dynamically-scoped currentOnError handler active at the call
// site: a definition with no on_error transform of its own can still
// legally use try/? today, inheriting whatever handler is active where it
// was inlined. A real-call callee is lowered standalone and only ever
// seeds currentOnError from its own on_error transform, never from the
// caller, so this only needs to look at `expr` itself and its own
// descendants - not through calls it makes, since any callee that itself
// needs try/? support is excluded on its own terms by this same check.
bool containsTryUsage(const Expr &expr) {
  if (expr.kind == Expr::Kind::Call && expr.name == "try") {
    return true;
  }
  for (const Expr &arg : expr.args) {
    if (containsTryUsage(arg)) {
      return true;
    }
  }
  for (const Expr &bodyArg : expr.bodyArguments) {
    if (containsTryUsage(bodyArg)) {
      return true;
    }
  }
  return false;
}

bool definitionUsesTry(const Definition &def) {
  for (const Expr &stmt : def.statements) {
    if (containsTryUsage(stmt)) {
      return true;
    }
  }
  if (def.returnExpr.has_value() && containsTryUsage(*def.returnExpr)) {
    return true;
  }
  return false;
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

// Total number of call *sites* targeting each definition, across the whole
// program - i.e. how many places would each get an inlined copy of the
// callee's body today. This is a flat count of edges (not distinct
// callers): three calls to the same helper from the same caller still
// count as three, since inlining duplicates the body at each one. Used to
// decide which non-recursive definitions are worth a real Call instead of
// inlining - see computeRealCallEligibleDefinitionPaths.
//
// Deliberately walks only `def.statements`, not `buildCallEdgeMap`'s
// combined statements+returnExpr traversal: the parser copies a top-level
// `return(...)` statement's inner expression into `def.returnExpr` *in
// addition to* leaving the full return statement in `def.statements`
// (ParserCoreBodyStatements.cpp) - walking both double-counts every call
// nested inside a return statement. `findRecursiveDefinitionPaths`/
// `findReachableDefinitionPaths` are set-membership checks so that
// duplication was harmless for them; it is not harmless for a count. Some
// synthetically-generated definitions set `returnExpr` without a matching
// statement, so this can under-count call sites for those specifically -
// safe, since under-counting only keeps a definition inlined rather than
// wrongly switching it to a real call.
std::unordered_map<std::string, size_t> countCallSitesByDefinitionPath(const Program &program) {
  std::unordered_map<std::string, size_t> counts;
  for (const Definition &def : program.definitions) {
    std::vector<std::string> calleeList;
    for (const Expr &stmt : def.statements) {
      collectCallEdges(stmt, calleeList);
    }
    for (const std::string &callee : calleeList) {
      ++counts[callee];
    }
  }
  return counts;
}

// A definition inlined at only one call site gains nothing from a real
// Call - one inlined copy is exactly as much code as one shared function
// plus a Call instruction, so there is no reason to take on the (small but
// nonzero) risk of changing its lowering path. Two or more call sites is
// where inlining starts duplicating the body, which is the actual
// motivating problem (see docs/todo.md's TODO-4747 entry).
constexpr size_t kMinCallSitesForRealCall = 2;

bool hasOnlyScalarParameters(const Definition &def) {
  for (const Expr &param : def.parameters) {
    if (isArgsPackBinding(param)) {
      return false;
    }
    if (paramHasMutTransform(param)) {
      // `[T mut]` is PrimeStruct's out-parameter mechanism, not value
      // semantics: the inline path passes the caller's local by address
      // (AddressOfLocal/StoreIndirect - see IrLowererInlineParamHelpers.cpp)
      // so mutations are visible to the caller after the call returns. A
      // real call copies the value onto the stack into the callee's own,
      // separate locals array, silently discarding that writeback.
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
  const std::unordered_set<std::string> reachable = findReachableDefinitionPaths(program, entryPath);
  const std::unordered_map<std::string, size_t> callSiteCounts = countCallSitesByDefinitionPath(program);

  std::unordered_map<std::string, const Definition *> byPath;
  byPath.reserve(program.definitions.size());
  for (const Definition &def : program.definitions) {
    byPath.emplace(def.fullPath, &def);
  }

  std::unordered_set<std::string> eligible;
  for (const std::string &path : reachable) {
    if (path == entryPath) {
      // The entry always has its own dedicated lowering path (argc/argv
      // binding, whole-program validation) and is never one of the bodies
      // the real-call body-lowering loop iterates - redirecting a
      // self-recursive entry here would lower it twice under the same
      // name. Keep today's rejection behavior for entry self-recursion.
      continue;
    }
    const bool isRecursive = recursive.find(path) != recursive.end();
    if (!isRecursive) {
      const auto countIt = callSiteCounts.find(path);
      const size_t siteCount = countIt != callSiteCounts.end() ? countIt->second : 0;
      if (siteCount < kMinCallSitesForRealCall) {
        continue;
      }
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
    if (definitionUsesTry(def)) {
      continue;
    }
    eligible.insert(path);
  }
  return eligible;
}

} // namespace primec::ir_lowerer
