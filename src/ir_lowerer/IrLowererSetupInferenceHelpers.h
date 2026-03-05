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
using ResolveSetupInferenceCallReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &, bool &)>;
using IsSetupInferenceEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsSetupInferenceBindingMutableFn = std::function<bool(const Expr &)>;
using SetupInferenceBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasSetupInferenceExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using SetupInferenceBindingValueKindFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using ApplySetupInferenceStructInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using InferSetupInferenceStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;

enum class CallExpressionReturnKindResolution {
  NotResolved,
  Resolved,
  MatchedButUnsupported,
};
enum class ArrayMapAccessElementKindResolution {
  NotMatched,
  Resolved,
};

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
CallExpressionReturnKindResolution resolveCallExpressionReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveSetupInferenceCallReturnKindFn &resolveDefinitionCallReturnKind,
    const ResolveSetupInferenceCallReturnKindFn &resolveCountMethodCallReturnKind,
    const ResolveSetupInferenceCallReturnKindFn &resolveMethodCallReturnKind,
    LocalInfo::ValueKind &kindOut);
ArrayMapAccessElementKindResolution resolveArrayMapAccessElementKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const IsSetupInferenceEntryArgsNameFn &isEntryArgsName,
    LocalInfo::ValueKind &kindOut);
LocalInfo::ValueKind inferBodyValueKindWithLocalsScaffolding(
    const std::vector<Expr> &bodyExpressions,
    const LocalMap &localsBase,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const IsSetupInferenceBindingMutableFn &isBindingMutable,
    const SetupInferenceBindingKindFn &bindingKind,
    const HasSetupInferenceExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
    const SetupInferenceBindingValueKindFn &bindingValueKind,
    const ApplySetupInferenceStructInfoFn &applyStructArrayInfo,
    const ApplySetupInferenceStructInfoFn &applyStructValueInfo,
    const InferSetupInferenceStructExprPathFn &inferStructExprPath);

} // namespace primec::ir_lowerer
