#pragma once

#include <optional>
#include <string>

#include "primec/Ast.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlBoolLiteralStep(const Expr &expr);

} // namespace primec::emitter
