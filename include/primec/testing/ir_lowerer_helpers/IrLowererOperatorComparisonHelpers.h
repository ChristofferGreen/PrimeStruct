



enum class OperatorComparisonEmitResult { Handled, NotHandled, Error };

using EmitComparisonExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferComparisonExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ComparisonKindFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitComparisonToZeroFn = std::function<bool(LocalInfo::ValueKind, bool)>;
using ComparisonAllocTempLocalFn = std::function<int32_t()>;

OperatorComparisonEmitResult emitComparisonOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitComparisonExprWithLocalsFn &emitExpr,
                                                        const InferComparisonExprKindWithLocalsFn &inferExprKind,
                                                        const ComparisonKindFn &comparisonKind,
                                                        const EmitComparisonToZeroFn &emitCompareToZero,
                                                        const ComparisonAllocTempLocalFn &allocTempLocal,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

