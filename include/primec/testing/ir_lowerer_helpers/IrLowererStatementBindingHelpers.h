



struct Definition;
struct ReturnInfo;
struct StructSlotLayoutInfo;
struct UninitializedStorageAccessInfo;
struct PrintBuiltin;

using HasExplicitBindingTypeTransformFn = std::function<bool(const Expr &)>;
using BindingKindFn = std::function<LocalInfo::Kind(const Expr &)>;
using BindingValueKindFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using InferBindingExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using IsBindingMutableFn = std::function<bool(const Expr &)>;
using IsFileErrorBindingFn = std::function<bool(const Expr &)>;
using SetReferenceArrayInfoForBindingFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyStructBindingInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using IsStringBindingFn = std::function<bool(const Expr &)>;
using EmitExprForBindingFn = std::function<bool(const Expr &, const LocalMap &)>;
using IsEntryArgsNameFn = std::function<bool(const Expr &, const LocalMap &)>;
using EmitStatementForBindingFn = std::function<bool(const Expr &, LocalMap &)>;
using EmitBlockForBindingFn = std::function<bool(const Expr &, LocalMap &)>;
using ResolveUninitializedStorageForStatementFn =
    std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>;
using ResolveStructSlotLayoutForStatementFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using EmitStructCopyFromPtrsForStatementFn = std::function<bool(int32_t, int32_t, int32_t)>;
using EmitPrintArgForStatementFn = std::function<bool(const Expr &, const LocalMap &, const PrintBuiltin &)>;
using ResolveDefinitionCallForStatementFn = std::function<const Definition *(const Expr &)>;

enum class UninitializedStorageInitDropEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class UninitializedStorageTakeEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class StatementPrintPathSpaceEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class ReturnStatementEmitResult {
  NotMatched,
  Emitted,
  Error,
};
enum class StatementMatchIfEmitResult {
  NotMatched,
  Emitted,
  Error,
};

struct ReturnStatementInlineContext {
  bool returnsVoid = false;
  bool returnsArray = false;
  LocalInfo::ValueKind returnKind = LocalInfo::ValueKind::Unknown;
  int32_t returnLocal = -1;
  std::vector<size_t> *returnJumps = nullptr;
};

struct StatementBindingTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
  std::string structTypeName;
};

StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind);
StatementBindingTypeInfo inferStatementBindingTypeInfo(const Expr &stmt,
                                                       const Expr &init,
                                                       const LocalMap &localsIn,
                                                       const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                                       const BindingKindFn &bindingKind,
                                                       const BindingValueKindFn &bindingValueKind,
                                                       const InferBindingExprKindFn &inferExprKind,
                                                       const ResolveDefinitionCallForStatementFn &resolveDefinitionCall);
bool isPointerMemoryIntrinsicCall(const Expr &expr);
bool inferPointerMemoryIntrinsicTargetsUninitializedStorage(const Expr &expr, const LocalMap &localsIn);

bool selectUninitializedStorageZeroInstruction(LocalInfo::Kind kind,
                                               LocalInfo::ValueKind valueKind,
                                               const std::string &bindingName,
                                               IrOpcode &zeroOp,
                                               uint64_t &zeroImm,
                                               std::string &error);
bool inferCallParameterLocalInfo(const Expr &param,
                                 const LocalMap &localsForKindInference,
                                 const IsBindingMutableFn &isBindingMutable,
                                 const HasExplicitBindingTypeTransformFn &hasExplicitBindingTypeTransform,
                                 const BindingKindFn &bindingKind,
                                 const BindingValueKindFn &bindingValueKind,
                                 const InferBindingExprKindFn &inferExprKind,
                                 const IsFileErrorBindingFn &isFileErrorBinding,
                                 const SetReferenceArrayInfoForBindingFn &setReferenceArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructArrayInfo,
                                 const ApplyStructBindingInfoFn &applyStructValueInfo,
                                 const IsStringBindingFn &isStringBinding,
                                 LocalInfo &infoOut,
                                 std::string &error,
                                 const std::function<const Definition *(const Expr &, const LocalMap &)>
                                     &resolveMethodCallDefinition = {},
                                 const std::function<const Definition *(const Expr &)> &resolveDefinitionCall = {},
                                 const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo = {});
bool emitStringStatementBindingInitializer(const Expr &stmt,
                                           const Expr &init,
                                           LocalMap &localsIn,
                                           int32_t &nextLocal,
                                           std::vector<IrInstruction> &instructions,
                                           const IsBindingMutableFn &isBindingMutable,
                                           const std::function<int32_t(const std::string &)> &internString,
                                           const EmitExprForBindingFn &emitExpr,
                                           const InferBindingExprKindFn &inferExprKind,
                                           const std::function<int32_t()> &allocTempLocal,
                                           const IsEntryArgsNameFn &isEntryArgsName,
                                           const std::function<void()> &emitArrayIndexOutOfBounds,
                                           std::string &error);
UninitializedStorageInitDropEmitResult tryEmitUninitializedStorageInitDropStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr,
    const ResolveStructSlotLayoutForStatementFn &resolveStructSlotLayout,
    const std::function<int32_t()> &allocTempLocal,
    const EmitStructCopyFromPtrsForStatementFn &emitStructCopyFromPtrs,
    std::string &error);
UninitializedStorageTakeEmitResult tryEmitUninitializedStorageTakeStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const ResolveUninitializedStorageForStatementFn &resolveUninitializedStorage,
    const EmitExprForBindingFn &emitExpr);
StatementPrintPathSpaceEmitResult tryEmitPrintPathSpaceStatementBuiltin(
    const Expr &stmt,
    const LocalMap &localsIn,
    const EmitPrintArgForStatementFn &emitPrintArg,
    const ResolveDefinitionCallForStatementFn &resolveDefinitionCall,
    const EmitExprForBindingFn &emitExpr,
    std::vector<IrInstruction> &instructions,
    std::string &error);
ReturnStatementEmitResult tryEmitReturnStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::optional<ReturnStatementInlineContext> &inlineContext,
    bool declaredReturnIsReferenceHandle,
    const std::optional<ResultReturnInfo> &resultReturnInfo,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferArrayElementKind,
    const std::function<void()> &emitFileScopeCleanupAll,
    std::string &error);
ReturnStatementEmitResult tryEmitReturnStatement(
    const Expr &stmt,
    const LocalMap &localsIn,
    std::vector<IrInstruction> &instructions,
    const std::optional<ReturnStatementInlineContext> &inlineContext,
    const std::optional<ResultReturnInfo> &resultReturnInfo,
    bool definitionReturnsVoid,
    bool &sawReturn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferArrayElementKind,
    const std::function<void()> &emitFileScopeCleanupAll,
    std::string &error);
StatementMatchIfEmitResult tryEmitMatchIfStatement(
    const Expr &stmt,
    LocalMap &localsIn,
    const EmitExprForBindingFn &emitExpr,
    const InferBindingExprKindFn &inferExprKind,
    const EmitBlockForBindingFn &emitBlock,
    const EmitStatementForBindingFn &emitStatement,
    std::vector<IrInstruction> &instructions,
    std::string &error);
