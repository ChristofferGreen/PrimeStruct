



enum class OperatorPowAbsSignEmitResult { Handled, NotHandled, Error };

using EmitPowAbsSignExprWithLocalsFn = std::function<bool(const Expr &, const LocalMap &)>;
using InferPowAbsSignExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using CombinePowAbsSignNumericKindsFn = std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using AllocPowAbsSignTempLocalFn = std::function<int32_t()>;
using EmitPowNegativeExponentFn = std::function<void()>;

OperatorPowAbsSignEmitResult emitPowAbsSignOperatorExpr(const Expr &expr,
                                                        const LocalMap &localsIn,
                                                        bool hasMathImport,
                                                        const EmitPowAbsSignExprWithLocalsFn &emitExpr,
                                                        const InferPowAbsSignExprKindWithLocalsFn &inferExprKind,
                                                        const CombinePowAbsSignNumericKindsFn &combineNumericKinds,
                                                        const AllocPowAbsSignTempLocalFn &allocTempLocal,
                                                        const EmitPowNegativeExponentFn &emitPowNegativeExponent,
                                                        std::vector<IrInstruction> &instructions,
                                                        std::string &error);

