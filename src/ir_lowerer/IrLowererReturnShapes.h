#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ir_lowerer/IrLowererStatementBindingHelpers.h"

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

// The struct a definition's `Result<V, E>` return carries as its value (outside the collections), or empty. Such a
// result is the address of the payload block, or the error code in the high half.
std::string packedResultStructValuePath(const Definition &def,
                                        const std::unordered_set<std::string> &structNames);
// A real call returns a packed `Result` with a struct value: with the value on the stack, leaves the value to return
// in its place. The readers call a value an error when its low 32 bits are zero (the code is in the high half), and
// otherwise the address of a payload block, which is copied into the caller's block (`callerBlockLocal` holds its
// address) and replaced by the block's address. `valueLocal` is a temporary.
void emitPackedResultBlockReturnValue(
    std::vector<IrInstruction> &instructions,
    int32_t valueLocal,
    int32_t callerBlockLocal,
    int32_t slots,
    const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs);

// The adjustment of emitPackedResultBlockReturnValue for a `return` of a real call whose result takes `slots` payload
// slots; empty when `slots` is 0.
ActionFn makePackedResultBlockReturnAdjust(
    int32_t slots,
    std::vector<IrInstruction> &instructions,
    const Int32ProviderFn &allocTempLocal,
    int32_t callerBlockLocal,
    const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs);

// A packed `Result` whose struct value is one scalar field: with the field's value on the stack, builds the struct's
// block in `nextLocal` onward and leaves its address on the stack. Nothing happens for another payload.
void materializePackedSingleSlotPayload(
    std::vector<IrInstruction> &instructions,
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    int32_t &nextLocal,
    const Int32ProviderFn &allocTempLocal);

// The slots of the payload block a real call of `def` writes into the block of its caller when it returns a
// `Result<V, E>` whose value `V` is a struct of several slots (a struct of one packed slot is the scalar itself and
// needs no block), or 0. The call site and the callee both read it, so they agree on the extra parameter.
int32_t
realCallPackedResultSlots(const Definition &def,
                          const std::unordered_set<std::string> &structNames,
                          const std::function<bool(const std::string &, StructSlotLayoutInfo &)>
                              &resolveStructSlotLayout);

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

// The return of a sum built in this frame, as a pointer to its `totalSlots` slots at `ptrLocal`. An inlined call
// stores the pointer in the call's result local and jumps to its exit. A real call returns the sum in the caller's
// block (`realCallResultBlockLocal` holds its address, a negative value is no real call): the slots move there, the
// caller owns the payload from then on, and the block's address is returned. A function body ends in the shared exit
// when it has one, else in `cleanupAll` and the return; `sawReturn` is set when a return was emitted.
bool emitSumStorageReturn(int32_t ptrLocal,
                          int32_t totalSlots,
                          int32_t realCallResultBlockLocal,
                          const std::optional<ReturnStatementInlineContext> &inlineContext,
                          std::vector<IrInstruction> &instructions,
                          const std::function<bool(int32_t, int32_t, int32_t)> &copyFromPtrs,
                          const SharedReturnExitFn &emitSharedReturn,
                          const std::function<void()> &cleanupAll,
                          bool &sawReturn,
                          std::string &error);

} // namespace primec::ir_lowerer
