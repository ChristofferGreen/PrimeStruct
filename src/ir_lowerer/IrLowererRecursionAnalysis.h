#pragma once

#include <string>
#include <unordered_set>

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
std::unordered_set<std::string> findReachableDefinitionPaths(const Program &program,
                                                              const std::string &entryPath);

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
std::unordered_set<std::string> computeRealCallEligibleDefinitionPaths(const Program &program,
                                                                       const std::string &entryPath);

// Exposed so real-call body lowering (which must build the exact same
// parameter/return LocalInfo::ValueKind this eligibility scan already
// validated) can reuse the identical extraction instead of a second,
// possibly-diverging implementation.
bool isSupportedScalarTypeName(const std::string &typeName);
std::string extractParameterTypeNameStatic(const Expr &paramExpr);

} // namespace primec::ir_lowerer
