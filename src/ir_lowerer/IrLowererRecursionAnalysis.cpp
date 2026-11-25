#include "IrLowererRecursionAnalysis.h"
#include "IrLowererCollectionRecordShapes.h"
#include "IrLowererPackedErrorTypes.h"
#include "IrLowererRealCallBlockerText.h"
#include "IrLowererStringParameterUse.h"
#include "IrLowererScalarCallBlocker.h"

#include "primec/ir_lowerer/IrLowererBindingTransformHelpers.h"
#include "primec/ir_lowerer/IrLowererBindingTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererCallHelpers.h"
#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererStructTypeHelpers.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererTemplateTypeParseHelpers.h"
#include "primec/semantics/ImportedDefinitions.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace primec::ir_lowerer {

bool paramHasMutTransform(const Expr &paramExpr) {
  for (const auto &transform : paramExpr.transforms) {
    if (transform.name == "mut") {
      return true;
    }
  }
  return false;
}

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
        transform.name == "static" || transform.name == "ctor_only" || transform.name == "shared" ||
        transform.name == "placement" || transform.name == "align" || transform.name == "packed" ||
        transform.name == "reflection" || transform.name == "effects" ||
        transform.name == "capabilities" || transform.name == "args") {
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

std::unordered_set<std::string>
findReachableDefinitionPaths(const Program &program,
                             const std::string &entryPath,
                             const std::vector<std::string> *extraRoots) {
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
  if (extraRoots != nullptr) {
    for (const std::string &root : *extraRoots) {
      if (nodePaths.find(root) != nodePaths.end() && reachable.insert(root).second) {
        worklist.push_back(root);
      }
    }
  }
  // A lifecycle helper is expanded where a value is created, copied or destroyed, with no call to name it. The helpers
  // a library's interface carries call the library's functions (imported declarations), which must stay calls, so in a
  // program that links a library every lifecycle helper is a root.
  const bool linksLibrary =
      std::any_of(program.definitions.begin(),
                  program.definitions.end(),
                  [](const Definition &def) { return isImportedDefinition(def); });
  if (linksLibrary) {
    for (const Definition &def : program.definitions) {
      const size_t slash = def.fullPath.find_last_of('/');
      const std::string leaf =
          slash == std::string::npos ? def.fullPath : def.fullPath.substr(slash + 1);
      if (def.isNested &&
          (leaf == "Create" || leaf == "Destroy" || leaf == "Copy" || leaf == "Move") &&
          reachable.insert(def.fullPath).second) {
        worklist.push_back(def.fullPath);
      }
    }
  }
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

std::string builtinMapStoragePath(
    const std::vector<std::string> &templateArgs,
    const std::unordered_map<std::string, const Definition *> &definitionsByPath) {
  if (templateArgs.size() != 2) {
    return {};
  }
  // The storage of a builtin key/value collection is the struct the stdlib declares with the `key_value_type`
  // transform: its two `Vector` fields have `Copy` and `Destroy` helpers that own the buffers, so it crosses like any
  // struct. The specialization's name ends with the hash of its type arguments.
  const std::string suffix = mangleTemplateTypeArgsSuffix(templateArgs);
  std::string found;
  for (const auto &[path, def] : definitionsByPath) {
    if (def == nullptr || path.size() <= suffix.size() ||
        path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0 ||
        !definitionHasTransform(*def, "key_value_type")) {
      continue;
    }
    if (found.empty() || path < found) {
      found = path;
    }
  }
  return found;
}

std::string realCallStructReturnPath(
    const Definition &def,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, const Definition *> &definitionsByPath,
    bool includeErrorStructs) {
  for (const Transform &transform : def.transforms) {
    if (transform.name != "return") {
      continue;
    }
    if (transform.templateArgs.size() != 1) {
      return {};
    }
    const std::string returnType = trimTemplateTypeText(transform.templateArgs.front());
    std::string returnBase;
    std::string returnArgText;
    std::vector<std::string> returnArgs;
    if (splitTemplateTypeName(returnType, returnBase, returnArgText) &&
        trimTemplateTypeText(returnBase) == "map" && splitTemplateArgs(returnArgText, returnArgs)) {
      return builtinMapStoragePath(returnArgs, definitionsByPath);
    }
    const bool errorStruct = includeErrorStructs && isPackedErrorStructTypeName(returnType);
    if (returnType.empty() || returnType.find('<') != std::string::npos ||
        (!errorStruct && isSupportedScalarTypeName(returnType)) || returnType == "void") {
      return {};
    }
    // The callee moves (or copies) the result into a heap block and the call site moves it into its own frame, so a
    // struct with lifecycle helpers returns the same way as a plain one.
    const std::string path = resolveStructTypeName(returnType, def.namespacePrefix, structNames);
    if (path.empty() || (!includeErrorStructs && (isPackedErrorStructTypeName(returnType) ||
                                                  isPackedErrorStructTypeName(path)))) {
      return {};
    }
    const auto defIt = definitionsByPath.find(path);
    if (defIt == definitionsByPath.end() || defIt->second == nullptr ||
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
// keep element metadata in their locals. A spawned call (`allowCollections`) also takes the stdlib collection structs,
// whose slots cross like any struct's and whose `Copy` and `Destroy` helpers own the buffer.
bool isAddressAbiStruct(const std::string &path,
                        const std::unordered_set<std::string> &structNames,
                        bool allowCollections = false) {
  return !path.empty() && structNames.count(path) != 0 &&
         (allowCollections || path.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) != 0);
}

// Whether `path` names a sum definition (`[sum]`), whose owned parameters stay inline for a direct call: the call
// site's inline binding destroys a sum temporary itself, and the real-call body would destroy the payload again. A
// spawned call binds its arguments itself, in hidden locals, so it takes them.
bool isSumDefinitionPath(
    const std::string &path,
    const std::unordered_map<std::string, const Definition *> &definitionsByPath) {
  const auto defIt = definitionsByPath.find(path);
  return defIt != definitionsByPath.end() && defIt->second != nullptr &&
         definitionHasTransform(*defIt->second, "sum");
}

// A builtin `vector<T>` type (not the stdlib struct of the capitalized spelling) whose element the lowerer knows how to copy and
// destroy.
bool isBuiltinVectorType(const Transform &transform,
                         const std::string &namespacePrefix,
                         const std::unordered_set<std::string> &structNames) {
  if (transform.name != "vector" || transform.templateArgs.size() != 1 ||
      !transform.arguments.empty()) {
    return false;
  }
  std::string elementPath;
  return resolveBuiltinVectorElementStructPath(
      transform.templateArgs.front(),
      namespacePrefix,
      [&](const std::string &typeName, const std::string &prefix, std::string &pathOut) {
        pathOut = resolveStructTypeName(typeName, prefix, structNames);
        if (pathOut.empty() && typeName == "String" && structNames.count("/std/text/String") > 0) {
          pathOut =
              "/std/text/String"; // the stdlib text, declared `[String]` after `import /std/text/*`
        }
        return !pathOut.empty();
      },
      elementPath);
}

// `Reference<vector<T>>` of a builtin vector the lowerer can copy and destroy elements of: the argument is the
// vector's record address, as for a `mut` vector parameter.
bool isReferenceToBuiltinVector(const Transform &transform,
                                const std::string &namespacePrefix,
                                const std::unordered_set<std::string> &structNames) {
  if (transform.name != "Reference" || transform.templateArgs.size() != 1 ||
      !transform.arguments.empty()) {
    return false;
  }
  std::string base;
  std::string argText;
  if (!splitTemplateTypeName(trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
    return false;
  }
  Transform inner;
  inner.name = trimTemplateTypeText(base);
  inner.templateArgs.push_back(trimTemplateTypeText(argText));
  return isBuiltinVectorType(inner, namespacePrefix, structNames);
}

// A parameter type the address ABI passes as one slot: a scalar, a struct (its address), or a Pointer or Reference to
// a scalar or such a struct (the address). A `copy` or `move` parameter of a scalar or of a struct that is not a sum
// is taken too: the call site binds it (copying or moving the argument) in its own frame and passes the value or the
// copy's address, and the body owns and destroys it.
bool isAddressAbiParameter(
    const Definition &def,
    const Expr &param,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, const Definition *> &definitionsByPath,
    bool allowStringParams,
    bool allowCollections) {
  // A default (`[i32] limit{3i32}`) is fine: the call site binds the parameter, default included, as an inlined call
  // would, and the callee gets its value.
  if (isArgsPackBinding(param) || param.args.size() > 1) {
    return false;
  }
  const bool ownedParam =
      paramHasTransformNamed(param, "copy") || paramHasTransformNamed(param, "move");
  for (const Transform &transform : param.transforms) {
    if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
        transform.name == "static" || transform.name == "ctor_only" || transform.name == "shared" ||
        transform.name == "placement" || transform.name == "align" || transform.name == "packed" ||
        transform.name == "reflection" || transform.name == "effects" ||
        transform.name == "capabilities" || transform.name == "copy" || transform.name == "move") {
      continue;
    }
    if (isBuiltinVectorType(transform, def.namespacePrefix, structNames)) {
      // The record's address is the argument. A borrowed vector (`mut` or read-only) is the caller's own, with no
      // copy; a `copy` parameter gets a copy the call site makes in its frame and a `move` parameter the vector
      // handed over, and the real call's body owns that vector and frees it.
      return true;
    }
    if (!ownedParam && isReferenceToBuiltinVector(transform, def.namespacePrefix, structNames)) {
      return true;
    }
    if (transform.name == "map" && transform.templateArgs.size() == 2 &&
        transform.arguments.empty()) {
      // A builtin key/value collection is its storage struct (a spawned call's, `allowCollections`), borrowed, or owned
      // like a struct.
      return isAddressAbiStruct(builtinMapStoragePath(transform.templateArgs, definitionsByPath),
                                structNames,
                                allowCollections);
    }
    if (ownedParam) {
      if (!transform.arguments.empty() || !transform.templateArgs.empty()) {
        return false;
      }
      if (isSupportedScalarTypeName(transform.name)) {
        return true;
      }
      const std::string path =
          resolveStructTypeName(transform.name, def.namespacePrefix, structNames);
      // An owned sum is a spawned call's (`allowCollections`): the spawner binds it in a hidden local of its scope,
      // whose flag it clears, and the task's body owns the copy.
      return isAddressAbiStruct(path, structNames, allowCollections) &&
             (allowCollections || !isSumDefinitionPath(path, definitionsByPath));
    }
    if (!transform.arguments.empty()) {
      return false;
    }
    if (transform.name == "string" && transform.templateArgs.empty()) {
      return allowStringParams && !paramHasMutTransform(param) &&
             stringParameterUsedOnlyAsIndexReads(def, param.name);
    }
    if (normalizeCollectionBindingTypeName(transform.name) == "File" &&
        transform.templateArgs.size() == 1) {
      // A borrowed file handle is one slot, a scalar at run time; the body reads and writes through it and the
      // scope that opened the file closes it.
      return true;
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
    // `Self` names the struct whose helper this is (the `other` of a `Copy`).
    std::string selfPath;
    if (target == "Self" && isStructHelperDefinition(def, structNames, selfPath)) {
      return isAddressAbiStruct(selfPath, structNames, allowCollections);
    }
    return isSupportedScalarTypeName(target) ||
           isAddressAbiStruct(resolveStructTypeName(target, def.namespacePrefix, structNames),
                              structNames,
                              allowCollections);
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

// A `string` result is an index into the module's string table and a `Pointer<T>` result an address, each one slot
// like a scalar: the callee returns it and the caller reads it as it reads the result of an inlined call. A
// `Reference<T>` result stays refused, since it would name a place of the callee's frame.
bool hasSlotResultReturn(const Definition &def) {
  for (const Transform &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    const std::string returnType = trimTemplateTypeText(transform.templateArgs.front());
    if (returnType == "string" || isPackedErrorStructTypeName(returnType)) {
      return true;
    }
    std::string base;
    std::string argText;
    return splitTemplateTypeName(returnType, base, argText) &&
           (trimTemplateTypeText(base) == "Pointer" || trimTemplateTypeText(base) == "Result");
  }
  return false;
}

// The `return<vector<T>>` transform of `def` when it names a builtin vector the lowerer can copy, else null.
const Transform *builtinVectorReturnType(const Definition &def,
                                         const std::unordered_set<std::string> &structNames) {
  for (const Transform &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(
            trimTemplateTypeText(transform.templateArgs.front()), base, argText)) {
      return nullptr;
    }
    Transform inner;
    inner.name = trimTemplateTypeText(base);
    inner.templateArgs.push_back(trimTemplateTypeText(argText));
    return isBuiltinVectorType(inner, def.namespacePrefix, structNames) ? &transform : nullptr;
  }
  return nullptr;
}

// Whether the parameter names a sum, plain or through a `Reference` or `Pointer`.
bool isSumParameter(const Definition &def,
                    const Expr &param,
                    const std::unordered_set<std::string> &structNames,
                    const std::unordered_map<std::string, const Definition *> &definitionsByPath) {
  const Transform *type = taskParameterTypeTransform(param);
  if (type == nullptr) {
    return false;
  }
  std::string name = type->name;
  if ((name == "Reference" || name == "Pointer") && type->templateArgs.size() == 1) {
    name = trimTemplateTypeText(type->templateArgs.front());
  }
  return isSumDefinitionPath(resolveStructTypeName(name, def.namespacePrefix, structNames),
                             definitionsByPath);
}

AddressAbiBlocker addressAbiShapeBlocker(const Definition &def,
                                         const RealCallShapeContext &context) {
  const auto &byPath = *context.definitionsByPath;
  const auto &structNames = *context.structNames;
  if (definitionHasTransform(def, "host")) {
    return AddressAbiBlocker::HostDefinition;
  }
  if (definitionHasTransform(def, "struct")) {
    return AddressAbiBlocker::StructDefinition;
  }
  if (definitionHasTransform(def, "sum")) {
    return AddressAbiBlocker::SumDefinition;
  }
  if (definitionHasTransform(def, "compute")) {
    return AddressAbiBlocker::ComputeDefinition;
  }
  if (definitionHasTransform(def, "on_error")) {
    return AddressAbiBlocker::OnErrorHandler;
  }
  if (definitionHasTransform(def, "require")) {
    return AddressAbiBlocker::RequireContract;
  }
  if (def.fullPath.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0 &&
      (!context.narrowCollectionsRoot || takesOrReturnsCollectionRecord(def, structNames))) {
    return AddressAbiBlocker::CollectionsHelper;
  }
  std::string parentStructPath;
  if (isStructHelperDefinition(def, structNames, parentStructPath) &&
      !definitionHasTransform(def, "static")) {
    const size_t leafStart = def.fullPath.find_last_of('/');
    const std::string leaf =
        leafStart == std::string::npos ? def.fullPath : def.fullPath.substr(leafStart + 1);
    const bool lifecycleHelper =
        leaf == "Create" || leaf == "Destroy" || leaf == "Copy" || leaf == "Move";
    if (lifecycleHelper && !context.allowLifecycleHelpers) {
      return AddressAbiBlocker::LifecycleHelper;
    }
    if (!isAddressAbiStruct(parentStructPath, structNames, context.allowCollections)) {
      return AddressAbiBlocker::ReceiverStruct;
    }
    // A closure with a `[ref name]` capture runs its `call` inline, in the frame that holds the borrowed locals
    // (docs/spec/value-lifecycle.md, Borrowing).
    if (parentStructPath.find("__lambda") != std::string::npos) {
      const auto structIt = byPath.find(parentStructPath);
      if (structIt != byPath.end() && structIt->second != nullptr) {
        for (const Expr &field : structIt->second->statements) {
          for (const Transform &transform : field.transforms) {
            if (field.isBinding && (transform.name == "Reference" || transform.name == "Pointer")) {
              return AddressAbiBlocker::ClosureReferenceCapture;
            }
          }
        }
      }
    }
  }
  for (const Expr &param : def.parameters) {
    if (!isAddressAbiParameter(
            def, param, structNames, byPath, context.allowStringParams, context.allowCollections)) {
      return AddressAbiBlocker::Parameter;
    }
    if (context.refuseSumParameters && isSumParameter(def, param, structNames, byPath)) {
      return AddressAbiBlocker::SumParameter;
    }
  }
  // A `never` result (panic) is a void call: the callee ends the program with a trap.
  const std::string structResultPath =
      realCallStructReturnPath(def, structNames, byPath, context.allowSlotResults);
  if (!context.allowCollections &&
      structResultPath.rfind(collection_helpers::kCanonicalCollectionsRoot, 0) == 0 &&
      !isPackedErrorStructTypeName(structResultPath)) {
    return AddressAbiBlocker::CollectionResult;
  }
  if (context.allowVectorResults && structResultPath.empty() &&
      builtinVectorReturnType(def, structNames) != nullptr &&
      !vectorResultReturnsOwnedLocals(def)) {
    return AddressAbiBlocker::VectorResultNotOwned;
  }
  if (!hasScalarOrVoidReturn(def) && !hasTransformReturnType(def, "never") &&
      !(context.allowSlotResults && hasSlotResultReturn(def)) && structResultPath.empty() &&
      !(context.allowVectorResults && builtinVectorReturnType(def, structNames) != nullptr) &&
      !(context.allowCollections && builtinVectorReturnType(def, structNames) != nullptr)) {
    return AddressAbiBlocker::UnsupportedResult;
  }
  if (definitionUsesTry(def)) {
    return AddressAbiBlocker::UsesTry;
  }
  if (def.returnExpr.has_value() &&
      (def.statements.empty() || def.statements.back().kind != Expr::Kind::Call ||
       def.statements.back().name != "return")) {
    return AddressAbiBlocker::
        GeneratedTailValue; // the real-call body lowering only runs the statements
  }
  return AddressAbiBlocker::None;
}

bool isAddressAbiRealCallShape(const Definition &def, const RealCallShapeContext &context) {
  return addressAbiShapeBlocker(def, context) == AddressAbiBlocker::None;
}

} // namespace

const Transform *taskParameterTypeTransform(const Expr &param) {
  for (const Transform &transform : param.transforms) {
    if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
        transform.name == "static" || transform.name == "ctor_only" || transform.name == "shared" ||
        transform.name == "placement" || transform.name == "align" || transform.name == "packed" ||
        transform.name == "reflection" || transform.name == "effects" ||
        transform.name == "capabilities" || transform.name == "copy" || transform.name == "move") {
      continue;
    }
    return &transform;
  }
  return nullptr;
}

bool resolveTaskSpawnShape(const Definition &def,
                           const RealCallShapeContext &context,
                           TaskSpawnShape &shapeOut,
                           std::string &reasonOut) {
  shapeOut = {};
  reasonOut.clear();
  const auto &structNames = *context.structNames;
  for (const Expr &param : def.parameters) {
    const std::string subject = "parameter `" + param.name + "` ";
    if (isArgsPackBinding(param) || param.args.size() > 1) {
      reasonOut = subject + "is an argument pack";
      return false;
    }
    const bool ownedParam =
        paramHasTransformNamed(param, "copy") || paramHasTransformNamed(param, "move");
    if (!ownedParam && paramHasTransformNamed(param, "mut")) {
      reasonOut = subject + "is `mut`, and a task has no place of its spawner to write";
      return false;
    }
    const Transform *typeTransform = nullptr;
    for (const Transform &transform : param.transforms) {
      if (transform.name == "mut" || transform.name == "public" || transform.name == "private" ||
          transform.name == "static" || transform.name == "ctor_only" ||
          transform.name == "shared" || transform.name == "placement" ||
          transform.name == "align" || transform.name == "packed" ||
          transform.name == "reflection" || transform.name == "effects" ||
          transform.name == "capabilities" || transform.name == "copy" ||
          transform.name == "move") {
        continue;
      }
      typeTransform = &transform;
      break;
    }
    if (typeTransform == nullptr) {
      reasonOut = subject + "has no type a real call takes";
      return false;
    }
    if (typeTransform->name == "Pointer" || typeTransform->name == "Reference") {
      reasonOut = subject + "is a Pointer or Reference, which may name a local of its spawner";
      return false;
    }
    if (typeTransform->name == "string") {
      reasonOut = subject + "is a `string`, which no real call takes yet";
      return false;
    }
    if (isBuiltinVectorType(*typeTransform, def.namespacePrefix, structNames)) {
      TaskSpawnParameterShape parameter;
      parameter.kind = ownedParam ? TaskSpawnParameterShape::Kind::OwnedVector
                                  : TaskSpawnParameterShape::Kind::Vector;
      shapeOut.parameters.push_back(std::move(parameter));
      continue;
    }
    if (typeTransform->name == "map" && typeTransform->templateArgs.size() == 2 &&
        typeTransform->arguments.empty()) {
      TaskSpawnParameterShape parameter;
      parameter.structPath =
          builtinMapStoragePath(typeTransform->templateArgs, *context.definitionsByPath);
      if (parameter.structPath.empty()) {
        reasonOut = subject + "is a `map` whose storage struct the program does not hold";
        return false;
      }
      parameter.kind = ownedParam ? TaskSpawnParameterShape::Kind::OwnedStruct
                                  : TaskSpawnParameterShape::Kind::Struct;
      shapeOut.parameters.push_back(std::move(parameter));
      continue;
    }
    if (!typeTransform->templateArgs.empty() || !typeTransform->arguments.empty() ||
        typeTransform->name.find('<') != std::string::npos) {
      reasonOut = subject + "is a collection or another template type";
      return false;
    }
    TaskSpawnParameterShape parameter;
    if (isSupportedScalarTypeName(typeTransform->name)) {
      // An owned scalar is the task's own slot: the spawner evaluates (copies) it.
    } else {
      parameter.structPath =
          resolveStructTypeName(typeTransform->name, def.namespacePrefix, structNames);
      if (parameter.structPath.empty()) {
        reasonOut = subject + "has no type a real call takes";
        return false;
      }
      if (!isAddressAbiStruct(parameter.structPath, structNames, true)) {
        reasonOut = subject + "is a collection";
        return false;
      }
      parameter.kind = ownedParam ? TaskSpawnParameterShape::Kind::OwnedStruct
                                  : TaskSpawnParameterShape::Kind::Struct;
    }
    shapeOut.parameters.push_back(std::move(parameter));
  }
  const bool scalarOrVoidResult = hasScalarOrVoidReturn(def);
  if (!scalarOrVoidResult) {
    shapeOut.resultStructPath =
        realCallStructReturnPath(def, structNames, *context.definitionsByPath);
    if (shapeOut.resultStructPath.empty() && builtinVectorReturnType(def, structNames) != nullptr) {
      shapeOut.resultIsVector = true;
      if (!vectorResultReturnsOwnedLocals(def)) {
        reasonOut = "its vector result is not always a `mut` vector local the task built";
        return false;
      }
    } else if (shapeOut.resultStructPath.empty()) {
      reasonOut = "its result is not a scalar or a struct a real call returns";
      return false;
    }
  }
  if (definitionHasTransform(def, "on_error")) {
    reasonOut = "it has an `on_error` handler";
    return false;
  }
  if (definitionHasTransform(def, "require")) {
    reasonOut = "it has a `require` contract";
    return false;
  }
  if (definitionUsesTry(def)) {
    reasonOut = "it uses `try`";
    return false;
  }
  RealCallShapeContext addressContext = context;
  addressContext.allowStructs = true;
  addressContext.allowCollections = true;
  if (!isAddressAbiRealCallShape(def, addressContext)) {
    reasonOut = "it is not a definition a real call takes";
    return false;
  }
  return true;
}

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

bool isLifecycleHelperPath(const std::string &path) {
  const size_t slash = path.find_last_of('/');
  const std::string leaf = slash == std::string::npos ? path : path.substr(slash + 1);
  return leaf == "Create" || leaf == "Destroy" || leaf == "Copy" || leaf == "Move";
}

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

std::unordered_set<std::string>
computeRealCallEligibleDefinitionPaths(const Program &program,
                                       const std::string &entryPath,
                                       bool allowMutReceivers,
                                       bool allowNeverResult,
                                       bool allowStringParams,
                                       const std::vector<std::string> *libraryRoots) {
  const std::unordered_set<std::string> recursive = findRecursiveDefinitionPaths(program);
  const std::unordered_set<std::string> reachable =
      findReachableDefinitionPaths(program, entryPath, libraryRoots);
  const std::unordered_set<std::string> roots =
      libraryRoots != nullptr
          ? std::unordered_set<std::string>(libraryRoots->begin(), libraryRoots->end())
          : std::unordered_set<std::string>();
  const std::unordered_map<std::string, size_t> callSiteCounts =
      countCallSitesByDefinitionPath(program);

  std::unordered_map<std::string, const Definition *> byPath;
  byPath.reserve(program.definitions.size());
  std::unordered_set<std::string> structNames;
  // The names a function of a library resolves: the structs and the sums, which the lowering knows as struct names
  // (a sum is returned in a heap block like a struct, and a sum parameter is refused by its rule).
  std::unordered_set<std::string> libraryStructNames;
  for (const Definition &def : program.definitions) {
    byPath.emplace(def.fullPath, &def);
    if (isStructDefinition(def)) {
      structNames.insert(def.fullPath);
      libraryStructNames.insert(def.fullPath);
    } else if (definitionHasTransform(def, "sum")) {
      libraryStructNames.insert(def.fullPath);
    }
  }
  const RealCallShapeContext context{
      &byPath, &structNames, allowMutReceivers, allowNeverResult, allowStringParams};

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
    const auto defIt = byPath.find(path);
    // A root of a library is a function of the archive whatever its call sites, and a function a library defines
    // (an imported declaration) is always a call: it has no body to inline.
    const bool isImported = defIt != byPath.end() && isImportedDefinition(*defIt->second);
    if (!isRecursive && !isImported && roots.find(path) == roots.end()) {
      const auto countIt = callSiteCounts.find(path);
      const size_t siteCount = countIt != callSiteCounts.end() ? countIt->second : 0;
      if (siteCount < kMinCallSitesForRealCall) {
        continue;
      }
    }
    // A root of a library and a function a library defines are the ones that take a one-slot `string` or `Pointer<T>`
    // result.
    RealCallShapeContext definitionContext = context;
    definitionContext.allowSlotResults = isImported || roots.find(path) != roots.end();
    if (definitionContext.allowSlotResults) {
      definitionContext.structNames = &libraryStructNames;
      definitionContext.refuseSumParameters = true;
      definitionContext.allowVectorResults = true;
      definitionContext.narrowCollectionsRoot = true;
    }
    if (defIt == byPath.end() || !isRealCallShapeEligible(*defIt->second, definitionContext)) {
      continue;
    }
    eligible.insert(path);
  }
  return eligible;
}

std::string realCallBlockerReason(const Definition &def, const RealCallShapeContext &context) {
  return realCallBlocker(def, context).text;
}

RealCallBlocker realCallBlocker(const Definition &def, const RealCallShapeContext &context) {
  if (context.definitionsByPath == nullptr || context.structNames == nullptr) {
    return {};
  }
  if (!context.allowStructs) {
    std::string text = scalarOnlyParameterBlocker(def);
    return {text.empty() ? std::string() : "scalar-only-parameter", std::move(text)};
  }
  const auto &byPath = *context.definitionsByPath;
  const auto &structNames = *context.structNames;
  for (const Expr &param : def.parameters) {
    const std::string subject = "parameter `" + param.name + "` ";
    if (isAddressAbiParameter(
            def, param, structNames, byPath, context.allowStringParams, context.allowCollections)) {
      if (context.refuseSumParameters && isSumParameter(def, param, structNames, byPath)) {
        return {"sum-parameter",
                subject +
                    "is a sum, which a real call does not take: a function with a sum parameter "
                    "stays inline"};
      }
      continue;
    }
    if (isArgsPackBinding(param) || param.args.size() > 1) {
      return {"argument-pack", subject + "is an argument pack"};
    }
    const Transform *type = taskParameterTypeTransform(param);
    if (type == nullptr) {
      return {"untyped-parameter", subject + "has no type the address ABI passes"};
    }
    if (type->name == "vector" && !type->templateArgs.empty()) {
      return {"vector-parameter",
              subject + "is a `vector<" + trimTemplateTypeText(type->templateArgs.front()) +
                  ">` of an element the lowerer cannot copy or destroy"};
    }
    if ((type->name == "Reference" || type->name == "Pointer") && type->templateArgs.size() == 1 &&
        trimTemplateTypeText(type->templateArgs.front()).find('<') != std::string::npos) {
      return {"templated-reference-parameter",
              subject + "is a `" + type->name + "<" +
                  trimTemplateTypeText(type->templateArgs.front()) +
                  ">`: the address ABI passes a reference or pointer to a scalar or a struct, not "
                  "to a templated type" +
                  " such as a vector"};
    }
    if (type->name == "string") {
      return {"string-parameter",
              subject + "is a `string` used by more than `count()` or `__pstext_literal`"};
    }
    if (isSumDefinitionPath(resolveStructTypeName(type->name, def.namespacePrefix, structNames),
                            byPath)) {
      return {"owned-sum-parameter", subject + "is a sum, whose owned parameters stay inline"};
    }
    return {"parameter-type",
            subject + "has a type (`" + type->name +
                "`) the address ABI does not pass: a scalar, a struct outside the collections, or "
                "a reference or pointer " +
                "to one"};
  }
  if (const AddressAbiBlocker blocker = addressAbiShapeBlocker(def, context);
      blocker != AddressAbiBlocker::None) {
    return {addressAbiBlockerCode(blocker), describeAddressAbiBlocker(blocker, def)};
  }
  return {};
}

} // namespace primec::ir_lowerer
