



struct SemanticProductTargetAdapter;

enum class OperatorArithmeticEmitResult { Handled, NotHandled, Error };

using InferStructExprPathWithLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using CombineNumericKindsFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using EmitInstructionFn = std::function<void(IrOpcode, uint64_t)>;

OperatorArithmeticEmitResult emitArithmeticOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        const ExprLocalsPredicateFn &emitExpr,
                                                        const ExprLocalsValueKindFn &inferExprKind,
                                                        const InferStructExprPathWithLocalsFn &inferStructExprPath,
                                                        const CombineNumericKindsFn &combineNumericKinds,
                                                        const EmitInstructionFn &emitInstruction,
                                                        std::string &error,
                                                        const SemanticProductTargetAdapter *semanticProductTargets = nullptr);
