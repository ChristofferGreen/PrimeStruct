#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/Ast.h"

namespace primec::ir_lowerer {

using GetSetupInferenceBuiltinOperatorNameFn = std::function<bool(const Expr &, std::string &)>;
using InferSetupInferenceValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveSetupInferenceExprPathFn = std::function<std::string(const Expr &)>;
using ResolveSetupInferenceArrayElementKindByPathFn =
    std::function<bool(const std::string &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceArrayReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;

LocalInfo::ValueKind inferPointerTargetValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const GetSetupInferenceBuiltinOperatorNameFn &getBuiltinOperatorName);
LocalInfo::ValueKind inferBufferElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferArrayElementKind);
LocalInfo::ValueKind inferArrayElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferBufferElementKind,
    const ResolveSetupInferenceExprPathFn &resolveExprPath,
    const ResolveSetupInferenceArrayElementKindByPathFn &resolveStructArrayElementKindByPath,
    const ResolveSetupInferenceArrayReturnKindFn &resolveDirectCallArrayReturnKind,
    const ResolveSetupInferenceArrayReturnKindFn &resolveCountMethodArrayReturnKind,
    const ResolveSetupInferenceArrayReturnKindFn &resolveMethodCallArrayReturnKind);

} // namespace primec::ir_lowerer
