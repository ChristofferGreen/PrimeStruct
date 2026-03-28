



enum class OperatorArithmeticEmitResult { Handled, NotHandled, Error };

using EmitExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using InferStructExprPathWithLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using CombineNumericKindsFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitInstructionFn = std::function<void(IrOpcode, uint64_t)>;

OperatorArithmeticEmitResult emitArithmeticOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const EmitExprWithLocalsFn &emitExpr,
                                                        const InferExprKindWithLocalsFn &inferExprKind,
                                                        const InferStructExprPathWithLocalsFn &inferStructExprPath,
                                                        const CombineNumericKindsFn &combineNumericKinds,
                                                        const EmitInstructionFn &emitInstruction,
                                                        std::string &error);

