



struct StructSlotLayoutInfo;

using ResolveInlineStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using InferInlineStructExprPathFn = std::function<std::string(const Expr &, const LocalMap &)>;
using InferInlineStructFieldLocalInfoFn = std::function<bool(const Expr &, const LocalMap &, LocalInfo &, std::string &)>;
using EmitInlineStructCopySlotsFn = std::function<bool(int32_t, int32_t, int32_t)>;
using EmitInlineStructInstructionFn = std::function<void(IrOpcode, uint64_t)>;

bool emitInlineStructDefinitionArguments(const std::string &calleePath,
                                         const std::vector<Expr> &params,
                                         const std::vector<const Expr *> &orderedArgs,
                                         const LocalMap &callerLocals,
                                         bool requireValue,
                                         int32_t &nextLocal,
                                         const ResolveInlineStructSlotLayoutFn &resolveStructSlotLayout,
                                         const ExprLocalsValueKindFn &inferExprKind,
                                         const InferInlineStructExprPathFn &inferStructExprPath,
                                         const ExprLocalsPredicateFn &emitExpr,
                                         const InferInlineStructFieldLocalInfoFn &inferFieldLocalInfo,
                                         const EmitInlineStructCopySlotsFn &emitStructCopySlots,
                                         const Int32ProviderFn &allocTempLocal,
                                         const EmitInlineStructInstructionFn &emitInstruction,
                                         std::string &error,
                                         std::optional<int32_t> destBaseLocal = std::nullopt);

bool emitInlineStructDefinitionArguments(const std::string &calleePath,
                                         const std::vector<const Expr *> &orderedArgs,
                                         const LocalMap &callerLocals,
                                         bool requireValue,
                                         int32_t &nextLocal,
                                         const ResolveInlineStructSlotLayoutFn &resolveStructSlotLayout,
                                         const ExprLocalsValueKindFn &inferExprKind,
                                         const InferInlineStructExprPathFn &inferStructExprPath,
                                         const ExprLocalsPredicateFn &emitExpr,
                                         const EmitInlineStructCopySlotsFn &emitStructCopySlots,
                                         const Int32ProviderFn &allocTempLocal,
                                         const EmitInlineStructInstructionFn &emitInstruction,
                                         std::string &error);
