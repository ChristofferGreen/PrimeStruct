#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using GetSetupInferenceBuiltinOperatorNameFn = std::function<bool(const Expr &, std::string &)>;

LocalInfo::ValueKind inferPointerTargetValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const GetSetupInferenceBuiltinOperatorNameFn &getBuiltinOperatorName);

} // namespace primec::ir_lowerer
