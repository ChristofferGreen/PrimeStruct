#pragma once

#include <string>
#include <vector>

#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "IrLowererRecursionAnalysis.h"
#include "primec/ir_lowerer/IrLowererLowerSetupStage.h"
#include "primec/semantics/FunctionAddressDefinitions.h"

namespace primec::ir_lowerer {

// A call of a `[function_address("/path")]` definition (primec/semantics/FunctionAddressDefinitions.h): the definition
// it names becomes a real-call function (reserved here when no call site has) and the call is FunctionAddress for it,
// with the reservation as its immediate until the function table is final (`(1 << 32) | index`, as for a call). The
// opcode pushes the address; a call whose value nobody reads pops it. False with `error` set when the path names no
// definition or the definition cannot be a real call on this target.
bool lowerFunctionAddressCall(const Definition &getter,
                              LowerSetupStageState &setupStage,
                              std::vector<IrInstruction> &instructions,
                              bool requireValue,
                              std::string &error);

// The shape rules a call site checks a definition against to make it a real call (docs/spec/vm-design.md, Real calls).
// An indirect call also takes a one-slot `Pointer<T>` result, as a function of a library does.
RealCallShapeContext callSiteRealCallContext(const LowerSetupStageState &setupStage,
                                             const Definition &callee,
                                             bool lifecycleReentry);

} // namespace primec::ir_lowerer
