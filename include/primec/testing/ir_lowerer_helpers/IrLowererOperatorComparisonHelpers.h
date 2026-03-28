



enum class OperatorComparisonEmitResult { Handled, NotHandled, Error };

using EmitComparisonExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferComparisonExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ComparisonKindFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitComparisonToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;

OperatorComparisonEmitResult emitComparisonOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitComparisonExprWithLocalsFn &emitExpr,
                                                        const InferComparisonExprKindWithLocalsFn &inferExprKind,
                                                        const ComparisonKindFn &comparisonKind,
                                                        const EmitComparisonToZeroFn &emitCompareToZero,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

