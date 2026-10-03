#pragma once

#include "primec/ir_lowerer/IrLowererSetupTypeHelpers.h"
#include "primec/support/CallbackTypes.h"

namespace primec::ir_lowerer {

bool inferBuiltinAccessReceiverResultKind(const Expr &receiverCallExpr,
                                          const LocalMap &localsIn,
                                          const ExprLocalsValueKindFn &inferExprKind,
                                          const ExprStringFn &resolveExprPath,
                                          const GetReturnInfoForPathFn &getReturnInfo,
                                          const std::unordered_map<std::string, const Definition *> &defMap,
                                          LocalInfo::ValueKind &kindOut);
bool isSoaVectorReceiverExpr(const Expr &receiverExpr, const LocalMap &localsIn);

} // namespace primec::ir_lowerer
