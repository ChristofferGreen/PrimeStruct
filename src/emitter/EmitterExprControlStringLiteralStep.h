#pragma once

#include <optional>
#include <string>

#include "primec/ast/Ast.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlStringLiteralStep(const Expr &expr);

} // namespace primec::emitter
