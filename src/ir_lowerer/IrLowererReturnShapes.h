#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"

namespace primec::ir_lowerer {

// Whether a returned value is an integer that is not sum storage: a packed Result. `returnsStdlibResultSum` says
// whether a call's callee is declared to return the stdlib Result sum; `inferStructPath` and `inferKind` read the
// value's struct path and scalar kind.
bool isPackedResultReturnValue(
    const Expr &valueExpr,
    const LocalMap &locals,
    const std::function<const Definition *(const Expr &)> &resolveDefinitionCall,
    const std::function<bool(const Definition &)> &returnsStdlibResultSum,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructPath,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferKind);

// Whether `expr` is `wait(t)` of a task handle whose result is a builtin vector: the vector the task built, which the
// waiter owns, so a binding takes it over and a read of it as a temporary destroys it.
bool isTaskVectorWait(const Expr &expr, const LocalMap &locals);

// Whether the definition at `path` is declared `return<vector<T>>` of a builtin vector.
bool definitionReturnsBuiltinVector(
    const std::unordered_map<std::string, const Definition *> &defMap, const std::string &path);

// The return of a builtin vector from a real call (a spawned call's body): the vector is a pointer to a 5-slot record of
// the frame that made it, and that frame goes away, so the record of the returned local is copied into the caller's
// block (`resultBlockLocal` holds its address), and the local gives up the buffer (its drop flag is cleared) because
// the waiter owns it now. `returnStmt` is rewritten to return the block's address in `rewrittenStmt`, with the
// local it names added to `rewrittenLocals` (a copy of `locals`). False with `error` set when the value is not a
// local vector that owns its buffer.
bool emitRealCallVectorReturn(const Expr &returnStmt,
                              const LocalMap &locals,
                              int32_t resultBlockLocal,
                              const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
                              const std::function<int32_t()> &allocTempLocal,
                              std::vector<IrInstruction> &instructions,
                              Expr &rewrittenStmt,
                              LocalMap &rewrittenLocals,
                              std::string &error);

} // namespace primec::ir_lowerer
