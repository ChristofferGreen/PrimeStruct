



namespace primec {
struct SemanticProgram;
}

struct SemanticProductIndex;
struct SemanticProductTargetAdapter;

using GetSetupInferenceBuiltinOperatorNameFn = std::function<bool(const Expr &, std::string &)>;
using InferSetupInferenceValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveSetupInferenceExprPathFn = std::function<std::string(const Expr &)>;
using ResolveSetupInferenceDefinitionCallFn = std::function<const Definition *(const Expr &)>;
using ResolveSetupInferenceArrayElementKindByPathFn =
    std::function<bool(const std::string &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceArrayReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;
using ResolveSetupInferenceCallReturnKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &, bool &)>;
using ResolveSetupInferenceCallCollectionAccessValueKindFn =
    std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>;
using IsSetupInferenceEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsSetupInferenceBindingMutableFn = std::function<bool(const Expr &)>;
using SetupInferenceBindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasSetupInferenceExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using SetupInferenceBindingValueKindFn =
    std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using ApplySetupInferenceStructInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using InferSetupInferenceStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using SetupInferenceCombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using LowerSetupInferenceMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using InferSetupInferenceBodyValueKindFn =
    std::function<LocalInfo::ValueKind(const std::vector<Expr> &, const LocalMap &)>;
using IsSetupInferenceKnownDefinitionPathFn = std::function<bool(const std::string &)>;
using IsSetupInferenceMethodCountLikeCallFn = std::function<bool(const Expr &, const LocalMap &)>;

enum class CallExpressionReturnKindResolution {
  NotResolved,
  Resolved,
  MatchedButUnsupported,
};
enum class ArrayMapAccessElementKindResolution {
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
    LocalInfo::ValueKind &kindOut,
    const ResolveSetupInferenceCallCollectionAccessValueKindFn &resolveCallCollectionAccessValueKind =
        ResolveSetupInferenceCallCollectionAccessValueKindFn{});
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
    const InferSetupInferenceStructExprPathFn &inferStructExprPath,
    const ResolveSetupInferenceDefinitionCallFn &resolveDefinitionCall = {},
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
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
    const InferSetupInferenceStructExprPathFn &inferStructExprPath,
    const ResolveSetupInferenceDefinitionCallFn &resolveDefinitionCall,
    const SemanticProductTargetAdapter *semanticProductTargets);
MathBuiltinReturnKindResolution inferMathBuiltinReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut);
NonMathScalarCallReturnKindResolution inferNonMathScalarCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const InferSetupInferenceValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut);
ControlFlowCallReturnKindResolution inferControlFlowCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveSetupInferenceExprPathFn &resolveExprPath,
    const LowerSetupInferenceMatchToIfFn &lowerMatchToIf,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    const InferSetupInferenceBodyValueKindFn &inferBodyValueKind,
    const IsSetupInferenceKnownDefinitionPathFn &isKnownDefinitionPath,
    std::string &error,
    LocalInfo::ValueKind &kindOut);
PointerBuiltinCallReturnKindResolution inferPointerBuiltinCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferPointerTargetKind,
    LocalInfo::ValueKind &kindOut);
ComparisonOperatorCallReturnKindResolution inferComparisonOperatorCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferExprKind,
    const SetupInferenceCombineNumericKindsFn &combineNumericKinds,
    LocalInfo::ValueKind &kindOut);
GpuBufferCallReturnKindResolution inferGpuBufferCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferSetupInferenceValueKindFn &inferBufferElementKind,
    LocalInfo::ValueKind &kindOut);
CountCapacityCallReturnKindResolution inferCountCapacityCallReturnKind(
    const Expr &expr,
    const LocalMap &localsIn,
    const IsSetupInferenceMethodCountLikeCallFn &isArrayCountCall,
    const IsSetupInferenceMethodCountLikeCallFn &isStringCountCall,
    const IsSetupInferenceMethodCountLikeCallFn &isVectorCapacityCall,
    LocalInfo::ValueKind &kindOut);
