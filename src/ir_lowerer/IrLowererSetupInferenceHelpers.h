#pragma once

#include <functional>
#include <string>

#include "IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"
#include "primec/support/CallbackTypes.h"

namespace primec {
struct SemanticProgram;
}

namespace primec::ir_lowerer {

struct SemanticProductIndex;
struct SemanticProductTargetAdapter;

using GetSetupInferenceBuiltinOperatorNameFn = std::function<bool(const Expr &, std::string &)>;
using ResolveSetupInferenceDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using ResolveSetupInferenceArrayElementKindByPathFn =
    std::function<bool(const std::string &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceArrayReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceCallReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &, bool &)>;
using ResolveSetupInferenceCallCollectionAccessValueKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;
using SetupInferenceBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using SetupInferenceBindingValueKindFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using InferSetupInferenceStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using SetupInferenceCombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using LowerSetupInferenceMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using InferSetupInferenceBodyValueKindFn =
    std::function<LocalInfo::ValueKind(const std::vector<Expr> &, const LocalMap &)>;
using IsSetupInferenceKnownDefinitionPathFn = std::function<bool(const std::string &)>;

enum class CallExpressionReturnKindResolution {
  NotResolved,
  Resolved,
  MatchedButUnsupported,
};
enum class ArrayKeyValueAccessElementKindResolution {
  NotMatched,
  Resolved,
};
enum class MathBuiltinReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class NonMathScalarCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class ControlFlowCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class PointerBuiltinCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class ComparisonOperatorCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class GpuBufferCallReturnKindResolution {
  NotMatched,
  Resolved,
};
enum class CountCapacityCallReturnKindResolution {
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
    const ExprLocalsValueKindFn &inferArrayElementKind);
LocalInfo::ValueKind inferArrayElementValueKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsValueKindFn &inferBufferElementKind,
    const ExprStringFn &resolveExprPath,
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
ArrayKeyValueAccessElementKindResolution resolveArrayKeyValueAccessElementKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isEntryArgsName,
    LocalInfo::ValueKind &kindOut,
    const ResolveSetupInferenceCallCollectionAccessValueKindFn &resolveCallCollectionAccessValueKind =
        ResolveSetupInferenceCallCollectionAccessValueKindFn{},
    const ExprLocalsValueKindFn &inferExprKind = ExprLocalsValueKindFn{});
LocalInfo::ValueKind inferBodyValueKindWithLocalsScaffolding(
    const std::vector<Expr> &bodyExpressions,
    const LocalMap &localsBase,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprPredicateFn &isBindingMutable,
    const SetupInferenceBindingKindFn &bindingKind,
    const ExprPredicateFn &hasExplicitBindingTypeTransform,
    const SetupInferenceBindingValueKindFn &bindingValueKind,
    const ExprLocalInfoVisitorFn &applyStructArrayInfo,
    const ExprLocalInfoVisitorFn &applyStructValueInfo,
    const InferSetupInferenceStructExprPathFn &inferStructExprPath,
    const ResolveSetupInferenceDefinitionCallFn &resolveDefinitionCall = {},
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
LocalInfo::ValueKind inferBodyValueKindWithLocalsScaffolding(
    const std::vector<Expr> &bodyExpressions,
    const LocalMap &localsBase,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprPredicateFn &isBindingMutable,
    const SetupInferenceBindingKindFn &bindingKind,
    const ExprPredicateFn &hasExplicitBindingTypeTransform,
    const SetupInferenceBindingValueKindFn &bindingValueKind,
    const ExprLocalInfoVisitorFn &applyStructArrayInfo,
    const ExprLocalInfoVisitorFn &applyStructValueInfo,
    const InferSetupInferenceStructExprPathFn &inferStructExprPath,
    const ResolveSetupInferenceDefinitionCallFn &resolveDefinitionCall,
    const SemanticProductTargetAdapter *semanticProductTargets);
MathBuiltinReturnKindResolution inferMathBuiltinReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const ExprLocalsValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut);
NonMathScalarCallReturnKindResolution inferNonMathScalarCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsValueKindFn &inferExprKind,
    const ExprLocalsValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut);
ControlFlowCallReturnKindResolution inferControlFlowCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprStringFn &resolveExprPath,
    const LowerSetupInferenceMatchToIfFn &lowerMatchToIf,
    const ExprLocalsValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    const InferSetupInferenceBodyValueKindFn &inferBodyValueKind,
    const IsSetupInferenceKnownDefinitionPathFn &isKnownDefinitionPath,
    std::string &error,
    LocalInfo::ValueKind &kindOut);
PointerBuiltinCallReturnKindResolution inferPointerBuiltinCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut);
ComparisonOperatorCallReturnKindResolution inferComparisonOperatorCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut);
GpuBufferCallReturnKindResolution inferGpuBufferCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsValueKindFn &inferBufferElementKind,
    LocalInfo::ValueKind &kindOut);
CountCapacityCallReturnKindResolution inferCountCapacityCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ExprLocalsPredicateFn &isArrayCountCall,
    const ExprLocalsPredicateFn &isStringCountCall,
    const ExprLocalsPredicateFn &isVectorCapacityCall,
    LocalInfo::ValueKind &kindOut);

} // namespace primec::ir_lowerer
