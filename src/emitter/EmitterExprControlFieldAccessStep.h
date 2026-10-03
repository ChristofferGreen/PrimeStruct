#pragma once

#include <functional>
#include <optional>
#include <string>

#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {

using ResolveFieldAccessStaticReceiverFn = std::function<std::optional<std::string>(const Expr &)>;

std::optional<std::string> runEmitterExprControlFieldAccessStep(const Expr &expr,
                                                                const ExprStringFn &emitReceiverExpr,
                                                                const ResolveFieldAccessStaticReceiverFn &resolveStaticReceiverExpr);

} // namespace primec::emitter
