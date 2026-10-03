#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/ir/Ir.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

enum class OperatorPowAbsSignEmitResult { Handled, NotHandled, Error };

using CombinePowAbsSignNumericKindsFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;

OperatorPowAbsSignEmitResult emitPowAbsSignOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        bool hasMathImport,
                                                        const ExprLocalsPredicateFn &emitExpr,
                                                        const ExprLocalsValueKindFn &inferExprKind,
                                                        const CombinePowAbsSignNumericKindsFn &combineNumericKinds,
                                                        const Int32ProviderFn &allocTempLocal,
                                                        const ActionFn &emitPowNegativeExponent,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

} // namespace primec::ir_lowerer
