#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/ast/Ast.h"

namespace primec::ir_lowerer {

// Returns the fullPaths of definitions in `program` that participate in a
// call cycle (self-recursion or mutual recursion), based on each
// definition's already-resolved `Expr::resolvedCallPath` edges (populated by
// template monomorphization, before ir_lowerer runs). A definition is in the
// result set iff it is reachable from itself by following one or more call
// edges. Edges to paths outside `program.definitions` (unresolved/external)
// are ignored.
//
// This is a static call-graph analysis only; it does not consult
// `resolvedCallPath` for anything ir_lowerer treats as a builtin/intrinsic
// dispatch (those never populate `resolvedCallPath` to a `program.definitions`
// entry in the first place, so they never appear as edges here).
std::unordered_set<std::string> findRecursiveDefinitionPaths(const Program &program);

// Returns the fullPaths of every definition transitively reachable from
// `entryPath` by following the same `resolvedCallPath` call edges, plus
// `entryPath` itself. Empty if `entryPath` doesn't name a definition in
// `program`.
// `extraRoots` (a library's public functions) are reached too, with what they call.
std::unordered_set<std::string>
findReachableDefinitionPaths(const Program &program,
                             const std::string &entryPath,
                             const std::vector<std::string> *extraRoots = nullptr);

// Returns the fullPaths of definitions that (a) are reachable from
// `entryPath` but are not `entryPath` itself (the entry has its own
// dedicated lowering path and is never one of the bodies the real-call
// body-lowering loop iterates - a self-recursive entry keeps today's
// rejection behavior), (b) are either self-/mutually-recursive OR called
// from 2+ distinct call sites in the program (a definition inlined at only
// one site gains nothing from a real Call - see kMinCallSitesForRealCall
// in the .cpp - so single-call-site definitions are left inlined exactly
// as before this analysis existed), and (c) pass a conservative, purely
// static shape check: every parameter and the return type (if any) must
// resolve, via a plain explicit type transform (no template args, no
// args-pack, no `mut`), to a scalar valueKindFromTypeName result
// (Int32/Int64/UInt64/Float32/Float64/Bool) or void; the definition must
// not be a struct/sum/compute definition, have an on_error handler, or
// itself use `try(...)`/the `?` postfix (both desugar to the same "try"
// call at parse time - see ParserExpr.cpp). `mut` scalar parameters are
// PrimeStruct's out-parameter mechanism (the inline path passes the
// caller's local by address; a real call can't replicate that with a
// separate locals array) and `try`/`?` reads a dynamically-scoped handler
// that only the inline path's call-site inheritance provides - a real-call
// callee only ever seeds its handler from its own on_error transform.
//
// This is a *candidate* set for real (non-inlined) Call/CallVoid emission,
// not a guarantee: it is deliberately conservative and can under-approximate
// (a genuinely eligible definition using a transform shape this static scan
// doesn't recognize is simply left out, which is safe - it keeps today's
// behavior for that definition). It must never over-approximate; callers
// that actually lower a body should still re-verify each parameter/return
// kind with the authoritative inference machinery and fail loudly on a
// mismatch rather than silently miscompile.
// `allowMutReceivers` selects the address ABI shapes (isRealCallShapeEligible with allowStructs): the VM and the
// optimizing C++ emitters, where a local address names the same slot in every frame.
// `allowNeverResult` lets the scalar ABI call a `never` definition (panic): the targets where a runtime error in a
// called function ends the program.
std::unordered_set<std::string> computeRealCallEligibleDefinitionPaths(
    const Program &program,
    const std::string &entryPath,
    bool allowMutReceivers = false,
    bool allowNeverResult = false,
    bool allowStringParams = false,
    // The public functions of a library (`--emit=lib`): reached and reserved as
    // real calls whatever their call sites, when their shape allows it.
    const std::vector<std::string> *libraryRoots = nullptr);

// What the real-call shape check needs to know about the program.
struct RealCallShapeContext {
  const std::unordered_map<std::string, const Definition *> *definitionsByPath = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  // Struct parameters, struct results and `mut` receivers (the VM and the optimizing C++ emitters).
  bool allowStructs = false;
  // A `never` result on the scalar ABI (the address ABI always takes it): the target traps on runtime errors.
  bool allowNeverResult = false;
  // A `string` parameter on the address ABI (the VM and optcpp, which read a module string by a computed index): the
  // call site passes the string's table index and the body reads it as a runtime string. Only for a body that uses
  // the parameter as `count()` or as the argument of `__pstext_literal`, since indexing a runtime string expands to
  // a branch per table string.
  bool allowStringParams = false;
  // A builtin `vector<T>` result, and a struct with collection fields: only a spawned call's shape takes them. A
  // builtin `vector<T>` parameter is taken by direct calls too, `copy` and `move` included.
  bool allowCollections = false;
  // A `Create`, `Destroy`, `Copy` or `Move` helper on the address ABI. Lifecycle helpers are expanded inline where a
  // value is made, copied or destroyed; one reached again from inside its own expansion (a `Function` whose closure
  // holds a `Function`) is lowered as a function instead, since an expansion into itself never ends.
  bool allowLifecycleHelpers = false;
};

// Whether `def` can be lowered once as a function and called (the parameter, receiver and result shapes above),
// leaving aside how often it is called. A call site whose callee passes this and is not a tiny leaf reserves it.
bool isRealCallShapeEligible(const Definition &def, const RealCallShapeContext &context);

// Why `def` is not eligible as a real call, as text that names the first parameter (or the result or body shape) the
// address ABI does not take; empty when it is eligible or the context is the scalar ABI. Used to explain a recursive
// definition that stays inline.
std::string realCallBlockerReason(const Definition &def, const RealCallShapeContext &context);

// How a spawned call's parameters and result cross to its task on the address ABI targets (IrLowererTaskSpawn.h): a
// scalar is passed as its value; a struct is copied (or moved) into a heap block by the spawner, since a local
// address does not cross to another call stack, and the task gets the block's address; a struct result is written to
// such a block too (`resultStructPath`, empty for a scalar or void result).
struct TaskSpawnParameterShape {
  // A `Vector` parameter is a builtin `vector<T>`: one slot holding the record's address, a copy of the spawner's
  // vector that it lends (or, `owned`, gives the task to destroy).
  enum class Kind { Scalar, Struct, OwnedStruct, Vector, OwnedVector } kind = Kind::Scalar;
  std::string structPath;
};
struct TaskSpawnShape {
  std::vector<TaskSpawnParameterShape> parameters;
  std::string resultStructPath;
  // The result is a builtin `vector<T>`: the task returns the record's address and the waiter owns the vector.
  bool resultIsVector = false;
};
// The parameter's type transform (past `mut`, `copy`, `move` and the like), or null.
const Transform *taskParameterTypeTransform(const Expr &param);
// False with the reason in `reasonOut` when `def` cannot run as a task: a `mut`, Pointer, Reference, `string` or
// collection parameter, or a definition the address ABI does not call.
bool resolveTaskSpawnShape(const Definition &def,
                           const RealCallShapeContext &context,
                           TaskSpawnShape &shapeOut,
                           std::string &reasonOut);

// A body of at most two statements and this many expression nodes is cheaper inlined than called: it stays inline at
// every call site unless it is recursive.
inline constexpr size_t TinyInlineLeafMaxNodes = 12;
bool isTinyInlineLeaf(const Definition &def);

// Whether the last path segment names a lifecycle helper (`Create`, `Destroy`, `Copy` or `Move`).
bool isLifecycleHelperPath(const std::string &path);

// Exposed so real-call body lowering (which must build the exact same
// parameter/return LocalInfo::ValueKind this eligibility scan already
// validated) can reuse the identical extraction instead of a second,
// possibly-diverging implementation.
bool isSupportedScalarTypeName(const std::string &typeName);
// Whether a parameter carries the `mut` transform (an out-parameter, passed by address when inlined).
bool paramHasMutTransform(const Expr &paramExpr);
std::string extractParameterTypeNameStatic(const Expr &paramExpr);
// The struct a definition returns by value as a real call: the declared `return<T>` names a struct (not a sum, not a
// collection record, not a packed error struct), or empty when the result is a scalar, void or not eligible. The
// callee moves or copies the result into a heap block and the call site moves it into a block of its frame.
// The storage struct (the stdlib struct declared with `key_value_type`, specialized for these type arguments) of a
// builtin key/value collection, or empty when the program holds no such specialization.
std::string
builtinMapStoragePath(const std::vector<std::string> &templateArgs,
                      const std::unordered_map<std::string, const Definition *> &definitionsByPath);
std::string realCallStructReturnPath(const Definition &def,
                                     const std::unordered_set<std::string> &structNames,
                                     const std::unordered_map<std::string, const Definition *> &definitionsByPath);

} // namespace primec::ir_lowerer
