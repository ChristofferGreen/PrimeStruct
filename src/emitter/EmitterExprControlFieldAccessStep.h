#pragma once

#include <functional>
#include <optional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

using EmitFieldAccessReceiverFn = std::function<std::string(const Expr &)>;
using ResolveFieldAccessStaticReceiverFn = std::function<std::optional<std::string>(const Expr &)>;

std::optional<std::string> runEmitterExprControlFieldAccessStep(const Expr &expr,
                                                                const EmitFieldAccessReceiverFn &emitReceiverExpr,
                                                                const ResolveFieldAccessStaticReceiverFn &resolveStaticReceiverExpr);

} // namespace primec::emitter
