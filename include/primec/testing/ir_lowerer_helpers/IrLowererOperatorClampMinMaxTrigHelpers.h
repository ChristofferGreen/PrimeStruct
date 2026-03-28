



enum class OperatorClampMinMaxTrigEmitResult { Handled, NotHandled, Error };

using EmitClampMinMaxTrigExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferClampMinMaxTrigExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using CombineClampMinMaxTrigNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using AllocClampMinMaxTrigTempLocalFn = std::function<int32_t()>;

OperatorClampMinMaxTrigEmitResult emitClampMinMaxTrigOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitClampMinMaxTrigExprWithLocalsFn &emitExpr,
    const InferClampMinMaxTrigExprKindWithLocalsFn &inferExprKind,
    const CombineClampMinMaxTrigNumericKindsFn &combineNumericKinds,
    const AllocClampMinMaxTrigTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

