#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "primec/Emitter.h"

namespace primec::emitter {

std::optional<std::string> runEmitterExprControlNameStep(
    const Expr &expr,
    const std::unordered_map<std::string, Emitter::BindingInfo> &localTypes,
    bool allowMathBare);

} // namespace primec::emitter
