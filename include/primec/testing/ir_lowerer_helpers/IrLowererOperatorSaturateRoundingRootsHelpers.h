



enum class OperatorSaturateRoundingRootsEmitResult { Handled, NotHandled, Error };

using EmitSaturateExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferSaturateExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using AllocSaturateTempLocalFn = std::function<int32_t()>;

OperatorSaturateRoundingRootsEmitResult emitSaturateRoundingRootsOperatorExpr(
    const Expr &expr,
    const LocalMap &localsIn,
    bool hasMathImport,
    const EmitSaturateExprWithLocalsFn &emitExpr,
    const InferSaturateExprKindWithLocalsFn &inferExprKind,
    const AllocSaturateTempLocalFn &allocTempLocal,
    std::vector<IrInstruction> &instructions,
    std::string &error);

