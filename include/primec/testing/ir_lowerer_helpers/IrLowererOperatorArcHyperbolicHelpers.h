



enum class OperatorArcHyperbolicEmitResult { Handled, NotHandled, Error };

using EmitArcHyperbolicExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferArcHyperbolicExprKindWithLocalsFn =
    std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using AllocArcHyperbolicTempLocalFn = std::function<int32_t()>;

OperatorArcHyperbolicEmitResult emitArcHyperbolicOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitArcHyperbolicExprWithLocalsFn &emitExpr,
    const InferArcHyperbolicExprKindWithLocalsFn &inferExprKind,
    const AllocArcHyperbolicTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

