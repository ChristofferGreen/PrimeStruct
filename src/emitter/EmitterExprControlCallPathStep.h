#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlCallPathStep(
    const Expr &expr,
    const std::string &resolvedPath,
    const std::unordered_map<std::string, std::string> &nameMap,
    const std::unordered_map<std::string, std::string> &importAliases);

} // namespace primec::emitter
