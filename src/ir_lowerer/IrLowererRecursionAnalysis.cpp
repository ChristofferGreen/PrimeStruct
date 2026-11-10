#include "IrLowererRecursionAnalysis.h"

#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/support/CollectionHelperNames.h"

#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::ir_lowerer {

bool isSupportedScalarTypeName(const std::string &typeName) {
  switch (valueKindFromTypeName(typeName)) {
    case LocalInfo::ValueKind::Int32:
    case LocalInfo::ValueKind::Int64:
    case LocalInfo::ValueKind::UInt64:
    case LocalInfo::ValueKind::UInt8:
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

// FileError/ImageError/ContainerError/GfxError are single-i32-field structs
// that `valueKindFromTypeName` (and therefore `isSupportedScalarTypeName`)
// treats as Int32 so that Result<T,E> packing and binding representations
// can use a plain scalar - but a definition whose *declared return type* is
// bare one of these names still constructs and returns a real struct via
// the generic struct-constructor path (IrLowererInlineStructArgHelpers.cpp's
// emitInlineStructDefinitionArguments), which pushes an AddressOfLocal
// pointing into the callee's own per-call locals array. That address is
// only valid for as long as the callee's call frame lives, which is fine
// for inlined calls (the "struct" lives in the caller's own frame) but not
// for a real (non-inlined) Call/Return - the callee's frame is torn down
// before the caller ever reads the value, so the returned "scalar" is a
// stale, frame-relative small integer instead of a real payload. Excluding
// these four names here keeps such definitions on the safe inlining path.
bool isPackedErrorStructTypeName(const std::string &typeName) {
  return typeName == "FileError" || typeName == "/std/file/FileError" ||
         typeName == "ImageError" || typeName == "/std/image/ImageError" ||
         typeName == "ContainerError" || typeName == collection_helpers::kCanonicalContainerErrorType ||
         typeName == "GfxError" || typeName == "/std/gfx/GfxError" ||
         typeName == "/std/gfx/experimental/GfxError";
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
    if (isPackedErrorStructTypeName(returnType)) {
      return false;
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

namespace {

// A struct type name as written inside `namespacePrefix`, resolved to its full path, or empty.
std::string resolveStructTypeName(const std::string &typeName,
                                  const std::string &namespacePrefix,
                                  const std::unordered_set<std::string> &structNames) {
  if (typeName.empty()) {
    return {};
  }
  if (typeName.front() == '/') {
    return structNames.count(typeName) != 0 ? typeName : std::string{};
  }
  std::string prefix = namespacePrefix;
  while (true) {
    const std::string candidate = prefix + "/" + typeName;
    if (structNames.count(candidate) != 0) {
      return candidate;
    }
    if (prefix.empty()) {
      return {};
    }
    const size_t cut = prefix.find_last_of('/');
    prefix = cut == std::string::npos ? std::string() : prefix.substr(0, cut);
  }
}

} // namespace

std::string realCallStructReturnPath(const Definition &def,
                                     const std::unordered_set<std::string> &structNames,
                                     const std::unordered_map<std::string, const Definition *> &definitionsByPath) {
  for (const Transform &transform : def.transforms) {
    if (transform.name != "return") {
      continue;
    }
    if (transform.templateArgs.size() != 1) {
      return {};
    }
    const std::string returnType = trimTemplateTypeText(transform.templateArgs.front());
    if (returnType.empty() || returnType.find('<') != std::string::npos || isSupportedScalarTypeName(returnType) ||
        returnType == "void") {
      return {};
    }
    // The callee moves (or copies) the result into a heap block and the call site moves it into its own frame, so a
    // struct with lifecycle helpers returns the same way as a plain one.
    const std::string path = resolveStructTypeName(returnType, def.namespacePrefix, structNames);
    if (path.empty() || isPackedErrorStructTypeName(returnType) ||
        isPackedErrorStructTypeName(path) ||
        path.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0) {
      return {};
    }
    const auto defIt = definitionsByPath.find(path);
    if (defIt == definitionsByPath.end() || defIt->second == nullptr ||
        definitionHasTransform(*defIt->second, "sum") ||
        definitionHasTransform(*defIt->second, "compute")) {
      return {};
    }
    return path;
  }
  return {};
}

namespace {

bool paramHasTransformNamed(const Expr &param, const char *name) {
  for (const Transform &transform : param.transforms) {
    if (transform.name == name) {
      return true;
    }
  }
  return false;
}

// A struct (by its full path) whose values the address ABI passes: a struct definition outside the collections, which
// keep element metadata in their locals.
bool isAddressAbiStruct(const std::string &path,
                        const std::unordered_set<std::string> &structNames) {
  return !path.empty() && structNames.count(path) != 0 &&
         path.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) != 0;
}

// Whether `path` names the struct of a `Function` type (`/__FunctionN__t...`, docs/spec/value-lifecycle.md, Lambdas).
bool isFunctionStructPath(const std::string &path) {
  const size_t leafStart = path.find_last_of('/');
  const std::string_view leaf = leafStart == std::string::npos
                                    ? std::string_view(path)
                                    : std::string_view(path).substr(leafStart + 1);
  return leaf.rfind("__Function", 0) == 0;
}

// A parameter type the address ABI passes as one slot: a scalar, a struct (its address), or a Pointer or Reference to
// a scalar or such a struct (the address). A `copy` or `move` parameter is taken only for a `Function`: the call site
// binds it (copying or moving the argument) in its own frame, and the body owns and destroys it.
bool isAddressAbiParameter(const Definition &def,
                           const Expr &param,
                           const std::unordered_set<std::string> &structNames) {
  // A default (`[i32] limit{3i32}`) is fine: the call site binds the parameter, default included, as an inlined call
  // would, and the callee gets its value.
  if (isArgsPackBinding(param) || param.args.size() > 1) {
    return false;
  }
  const bool ownedParam =
      paramHasTransformNamed(param, "copy") || paramHasTransformNamed(param, "move");
  for (const Transform &transform : param.transforms) {
    if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
        transform.name == "static" || transform.name == "shared" || transform.name == "placement" ||
        transform.name == "align" || transform.name == "packed" || transform.name == "reflection" ||
        transform.name == "effects" || transform.name == "capabilities" ||
        transform.name == "copy" || transform.name == "move") {
      continue;
    }
    if (ownedParam) {
      if (!transform.arguments.empty() || !transform.templateArgs.empty()) {
        return false;
      }
      const std::string path =
          resolveStructTypeName(transform.name, def.namespacePrefix, structNames);
      return isFunctionStructPath(path) && isAddressAbiStruct(path, structNames);
    }
    if (!transform.arguments.empty()) {
      return false;
    }
    std::string target = transform.name;
    if (transform.name == "Pointer" || transform.name == "Reference") {
      if (transform.templateArgs.size() != 1) {
        return false;
      }
      target = trimTemplateTypeText(transform.templateArgs.front());
    } else if (!transform.templateArgs.empty()) {
      return false;
    }
    if (target.find('<') != std::string::npos) {
      return false;
    }
    return isSupportedScalarTypeName(target) ||
           isAddressAbiStruct(resolveStructTypeName(target, def.namespacePrefix, structNames),
                              structNames);
  }
  return false;
}

// The shape rules of the address ABI (the VM and the optimizing C++ emitters, where a local address names the same
// slot in every frame): the receiver and every struct, `mut` or Reference argument are passed as an address.
bool hasTransformReturnType(const Definition &def, const char *typeName) {
  for (const Transform &transform : def.transforms) {
    if (transform.name == "return" && transform.templateArgs.size() == 1 &&
        trimTemplateTypeText(transform.templateArgs.front()) == typeName) {
      return true;
    }
  }
  return false;
}

bool isAddressAbiRealCallShape(const Definition &def, const RealCallShapeContext &context) {
  const auto &byPath = *context.definitionsByPath;
  const auto &structNames = *context.structNames;
  if (definitionHasTransform(def, "host") || definitionHasTransform(def, "struct") ||
      definitionHasTransform(def, "sum") || definitionHasTransform(def, "compute") ||
      definitionHasTransform(def, "on_error") || definitionHasTransform(def, "require")) {
    return false;
  }
  if (def.fullPath.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0) {
    return false;
  }
  std::string parentStructPath;
  if (isStructHelperDefinition(def, structNames, parentStructPath) &&
      !definitionHasTransform(def, "static")) {
    const size_t leafStart = def.fullPath.find_last_of('/');
    const std::string leaf =
        leafStart == std::string::npos ? def.fullPath : def.fullPath.substr(leafStart + 1);
    if (leaf == "Create" || leaf == "Destroy" || leaf == "Copy" || leaf == "Move" ||
        !isAddressAbiStruct(parentStructPath, structNames)) {
      return false;
    }
    // A closure with a `[ref name]` capture runs its `call` inline, in the frame that holds the borrowed locals
    // (docs/spec/value-lifecycle.md, Borrowing).
    if (parentStructPath.find("__lambda") != std::string::npos) {
      const auto structIt = byPath.find(parentStructPath);
      if (structIt != byPath.end() && structIt->second != nullptr) {
        for (const Expr &field : structIt->second->statements) {
          for (const Transform &transform : field.transforms) {
            if (field.isBinding && (transform.name == "Reference" || transform.name == "Pointer")) {
              return false;
            }
          }
        }
      }
    }
  }
  for (const Expr &param : def.parameters) {
    if (!isAddressAbiParameter(def, param, structNames)) {
      return false;
    }
  }
  // A `never` result (panic) is a void call: the callee ends the program with a trap.
  if (!hasScalarOrVoidReturn(def) && !hasTransformReturnType(def, "never") &&
      realCallStructReturnPath(def, structNames, byPath).empty()) {
    return false;
  }
  if (definitionUsesTry(def)) {
    return false;
  }
  if (def.returnExpr.has_value() &&
      (def.statements.empty() || def.statements.back().kind != Expr::Kind::Call ||
       def.statements.back().name != "return")) {
    return false; // a generated tail value; the real-call body lowering only runs the statements
  }
  return true;
}

} // namespace

bool isRealCallShapeEligible(const Definition &def, const RealCallShapeContext &context) {
  if (context.allowStructs) {
    return isAddressAbiRealCallShape(def, context);
  }
  // The scalar ABI (the other targets): scalar parameters and result, and a read-only receiver passed as its slots.
  const auto &byPath = *context.definitionsByPath;
  const auto &structNames = *context.structNames;
  if (definitionHasTransform(def, "host")) {
    return false; // lowered to CallHost at each call site, never as a function
  }
  if (definitionHasTransform(def, "struct") || definitionHasTransform(def, "sum") ||
      definitionHasTransform(def, "compute") || definitionHasTransform(def, "on_error")) {
    return false;
  }
  // Struct helpers (nested methods and lifecycle helpers) take an implicit `this` (a Reference<Struct>) that is not in
  // `def.parameters` - see buildInlineCallParameterList; `static` helpers have none.
  std::string parentStructPath;
  if (isStructHelperDefinition(def, structNames, parentStructPath) &&
      !definitionHasTransform(def, "static")) {
    // A read-only method gets a copy of the receiver's slots, rebuilt in the callee's frame (a local address names
    // a slot of its own frame on these targets); `mut` methods and lifecycle helpers stay inline.
    const size_t leafStart = def.fullPath.find_last_of('/');
    const std::string leaf =
        leafStart == std::string::npos ? def.fullPath : def.fullPath.substr(leafStart + 1);
    if (leaf == "Create" || leaf == "Destroy" || leaf == "Copy" || leaf == "Move" ||
        definitionHasTransform(def, "mut")) {
      return false;
    }
    // The receivers whose locals carry metadata a copied receiver would lose (element and key/value kinds of the
    // collection records) keep their methods inline.
    if (parentStructPath.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0) {
      return false;
    }
    // A closure with a `[ref name]` capture holds the address of a local in the frame that made it: its `call` runs
    // inline, in that frame.
    if (parentStructPath.find("__lambda") != std::string::npos) {
      const auto structIt = byPath.find(parentStructPath);
      if (structIt != byPath.end() && structIt->second != nullptr) {
        for (const Expr &field : structIt->second->statements) {
          for (const Transform &transform : field.transforms) {
            if (field.isBinding && (transform.name == "Reference" || transform.name == "Pointer")) {
              return false;
            }
          }
        }
      }
    }
  }
  // A `never` result (panic) is a void call where a runtime error in the callee ends the program.
  const bool scalarOrNeverReturn =
      hasScalarOrVoidReturn(def) ||
      (context.allowNeverResult && hasTransformReturnType(def, "never"));
  if (!hasOnlyScalarParameters(def) || !scalarOrNeverReturn || definitionUsesTry(def)) {
    return false;
  }
  if (def.returnExpr.has_value() &&
      (def.statements.empty() || def.statements.back().kind != Expr::Kind::Call ||
       def.statements.back().name != "return")) {
    return false; // a generated tail value; the real-call body lowering only runs the statements
  }
  return true;
}

namespace {

size_t countExprNodes(const Expr &expr, size_t limit) {
  size_t count = 1;
  for (const Expr &arg : expr.args) {
    if (count > limit) {
      return count;
    }
    count += countExprNodes(arg, limit - count);
  }
  for (const Expr &arg : expr.bodyArguments) {
    if (count > limit) {
      return count;
    }
    count += countExprNodes(arg, limit - count);
  }
  return count;
}

} // namespace

bool isTinyInlineLeaf(const Definition &def) {
  if (def.statements.size() > 2) {
    return false;
  }
  size_t nodes = 0;
  for (const Expr &stmt : def.statements) {
    nodes += countExprNodes(stmt, TinyInlineLeafMaxNodes + 1);
    if (nodes > TinyInlineLeafMaxNodes) {
      return false;
    }
  }
  return true;
}

std::unordered_set<std::string> computeRealCallEligibleDefinitionPaths(const Program &program,
                                                                       const std::string &entryPath,
                                                                       bool allowMutReceivers,
                                                                       bool allowNeverResult) {
  const std::unordered_set<std::string> recursive = findRecursiveDefinitionPaths(program);
  const std::unordered_set<std::string> reachable =
      findReachableDefinitionPaths(program, entryPath);
  const std::unordered_map<std::string, size_t> callSiteCounts =
      countCallSitesByDefinitionPath(program);

  std::unordered_map<std::string, const Definition *> byPath;
  byPath.reserve(program.definitions.size());
  std::unordered_set<std::string> structNames;
  for (const Definition &def : program.definitions) {
    byPath.emplace(def.fullPath, &def);
    if (isStructDefinition(def)) {
      structNames.insert(def.fullPath);
    }
  }
  const RealCallShapeContext context{&byPath, &structNames, allowMutReceivers, allowNeverResult};

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
    if (defIt == byPath.end() || !isRealCallShapeEligible(*defIt->second, context)) {
      continue;
    }
    eligible.insert(path);
  }
  return eligible;
}

} // namespace primec::ir_lowerer
