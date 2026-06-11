#pragma once

#include <functional>
#include <string>
#include <vector>

#include "IrLowererRuntimeErrorHelpers.h"
#include "primec/Ast.h"
#include "primec/Ir.h"

namespace primec::ir_lowerer {

// One runtime-checkable require(...) contract on a definition, rebuilt as the
// comparison expression the normal expression emitter can lower.
struct RequirementContractCheck {
  Expr predicate;
  std::string sourceText;
};

// Collects the runtime-checkable require(...) contracts on a definition.
// Compile-time predicates (already discharged by semantic validation) are
// skipped. Returns false when a contract names a runtime operand this
// lowering slice cannot emit, so contracts never silently lose their check.
bool collectRequirementContractChecks(const Definition &definition,
                                      std::vector<RequirementContractCheck> &checksOut,
                                      std::string &error);

// Emits the function-entry precondition checks for the runtime contracts on a
// definition. On contract failure the lowered code prints a deterministic
// message naming the definition and predicate, then fails with exit code 3.
bool emitRequirementContractChecks(
    const Definition &definition,
    IrFunction &function,
    const std::function<bool(const Expr &)> &emitPredicateExpr,
    const InternRuntimeErrorStringFn &internString,
    std::string &error);

} // namespace primec::ir_lowerer
