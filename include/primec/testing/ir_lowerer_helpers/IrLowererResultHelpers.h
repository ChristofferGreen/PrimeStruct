



struct ReturnInfo;
using ::primec::SemanticProgram;
struct SemanticProductIndex;
struct SemanticProductTargetAdapter;
struct StructSlotLayoutInfo;

struct ResultExprInfo {
  bool isResult = false;
  bool hasValue = false;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::Kind valueCollectionKind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueMapKeyKind = LocalInfo::ValueKind::Unknown;
  bool valueIsFileHandle = false;
  std::string valueStructType;
  std::string errorType;
};

struct LocalResultInfo {
  bool found = false;
  bool isResult = false;
  bool resultHasValue = false;
  LocalInfo::ValueKind resultValueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::Kind resultValueCollectionKind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind resultValueMapKeyKind = LocalInfo::ValueKind::Unknown;
  bool resultValueIsFileHandle = false;
  std::string resultValueStructType;
  std::string resultErrorType;
  bool isFileHandle = false;
};

using LookupLocalResultInfoFn = std::function<LocalResultInfo(const std::string &)>;
using ResolveCallDefinitionFn = std::function<const Definition *(const Expr &)>;
using LookupDefinitionResultInfoFn = std::function<bool(const std::string &, ResultExprInfo &)>;
using ResolveMethodCallWithLocalsFn = std::function<const Definition *(const Expr &, const LocalMap &)>;
using LookupReturnInfoFn = std::function<bool(const std::string &, ReturnInfo &)>;
using InferExprKindWithLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveResultExprInfoWithLocalsFn =
    std::function<bool(const Expr &, const LocalMap &, ResultExprInfo &)>;
struct ResultWhyCallOps;
struct ResultWhyExprOps {
  std::function<bool()> emitEmptyString;
  std::function<Expr(LocalMap &, LocalInfo::ValueKind)> makeErrorValueExpr;
  std::function<Expr(LocalMap &)> makeBoolErrorExpr;
};

bool resolveResultExprInfo(const Expr &expr,
                           const LookupLocalResultInfoFn &lookupLocal,
                           const ResolveCallDefinitionFn &resolveMethodCall,
                           const ResolveCallDefinitionFn &resolveDefinitionCall,
                           const LookupDefinitionResultInfoFn &lookupDefinitionResult,
                           ResultExprInfo &out);
bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     const InferExprKindWithLocalsFn &inferExprKind,
                                     ResultExprInfo &out,
                                     const SemanticProgram *semanticProgram = nullptr,
                                     const SemanticProductIndex *semanticIndex = nullptr,
                                     std::string *errorOut = nullptr);
bool resolveResultExprInfoFromLocals(const Expr &expr,
                                     const LocalMap &localsIn,
                                     const ResolveMethodCallWithLocalsFn &resolveMethodCall,
                                     const ResolveCallDefinitionFn &resolveDefinitionCall,
                                     const LookupReturnInfoFn &lookupReturnInfo,
                                     const InferExprKindWithLocalsFn &inferExprKind,
                                     ResultExprInfo &out,
                                     const SemanticProductTargetAdapter *semanticProductTargets,
                                     std::string *errorOut = nullptr);
ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo,
    const InferExprKindWithLocalsFn &inferExprKind,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr,
    std::string *errorOut = nullptr);
ResolveResultExprInfoWithLocalsFn makeResolveResultExprInfoFromLocals(
    const ResolveMethodCallWithLocalsFn &resolveMethodCall,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const LookupReturnInfoFn &lookupReturnInfo,
    const InferExprKindWithLocalsFn &inferExprKind,
    const SemanticProductTargetAdapter *semanticProductTargets,
    std::string *errorOut = nullptr);
bool validateSemanticProductResultMetadataCompleteness(const primec::SemanticProgram *semanticProgram,
                                                       std::string &error);
bool resolveResultWhyCallInfo(const Expr &expr,
                              const LocalMap &localsIn,
                              const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                              ResultExprInfo &resultInfo,
                              std::string &error);
bool resolveResultErrorCallInfo(const Expr &expr,
                                const LocalMap &localsIn,
                                const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
                                ResultExprInfo &resultInfo,
                                std::string &error);
enum class ResultErrorMethodCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class ResultWhyMethodCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class ResultWhyDispatchEmitResult {
  NotHandled,
  Emitted,
  Error,
};
enum class ResultOkMethodCallEmitResult {
  NotHandled,
  Emitted,
  Error,
};
struct PackedResultStructPayloadInfo {
  bool supported = false;
  bool isPackedSingleSlot = false;
  LocalInfo::ValueKind packedKind = LocalInfo::ValueKind::Unknown;
  int32_t slotCount = 0;
  int32_t fieldOffset = 0;
};
std::string unsupportedPackedResultValueKindError(const std::string &builtinName);
bool resolveSupportedPackedResultStructValueKind(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    LocalInfo::ValueKind &out);
ResultOkMethodCallEmitResult tryEmitResultOkCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &isFileHandleExpr,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
bool inferPackedResultStructType(
    const Expr &expr,
    const LocalMap &localsIn,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<std::string(const Expr &, const LocalMap &)> &inferStructExprPath,
    std::string &structTypeOut);
ResultErrorMethodCallEmitResult tryEmitResultErrorCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    std::string &error);
bool emitResultWhyLocalsFromValueExpr(
    const Expr &valueExpr,
    const LocalMap &localsIn,
    const ResultExprInfo &resultInfo,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    int32_t &errorLocalOut);
ResultWhyMethodCallEmitResult tryEmitResultWhyCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::vector<IrInstruction> *instructionsOut,
    std::string &error);
ResultWhyDispatchEmitResult tryEmitResultWhyDispatchCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    int32_t &onErrorTempCounter,
    const ResolveResultExprInfoWithLocalsFn &resolveResultExprInfo,
    const ResolveCallDefinitionFn &resolveDefinitionCall,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::vector<IrInstruction> *instructionsOut,
    std::string &error);
ResultWhyExprOps makeResultWhyExprOps(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t &onErrorTempCounter,
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
ResultWhyCallOps makeResultWhyCallOps(
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<Expr(LocalMap &, LocalInfo::ValueKind)> &makeErrorValueExpr,
    const std::function<Expr(LocalMap &)> &makeBoolErrorExpr,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    const std::function<bool()> &emitEmptyString);
bool emitResultWhyCallWithComposedOps(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyExprOps &exprOps,
    const std::function<bool(const std::string &, const std::string &, std::string &)> &resolveStructTypeName,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<LocalInfo::Kind(const Expr &)> &bindingKind,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    const std::function<LocalInfo::ValueKind(const std::string &)> &valueKindFromTypeName,
    const std::function<bool(const Expr &, const Definition &, const LocalMap &)> &emitInlineDefinitionCall,
    const std::function<bool(int32_t)> &emitFileErrorWhy,
    std::string &error);
bool usesInlineBufferResultErrorDiscriminator(const ResultExprInfo &resultInfo);
bool isSupportedPackedResultCollectionKind(LocalInfo::Kind kind);
bool resolveSupportedResultCollectionType(const std::string &typeText,
                                          LocalInfo::Kind &collectionKindOut,
                                          LocalInfo::ValueKind &valueKindOut,
                                          LocalInfo::ValueKind *mapKeyKindOut = nullptr);
bool isSupportedPackedResultValueKind(LocalInfo::ValueKind kind);
bool isSupportedPackedResultValueInfo(const ResultExprInfo &info,
                                     const std::function<bool(const std::string &, StructSlotLayoutInfo &)>
                                         &resolveStructSlotLayout);
bool resolvePackedResultStructPayloadInfo(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    PackedResultStructPayloadInfo &out);
bool resolveSupportedResultStructPayloadInfo(
    const std::string &structType,
    const std::function<bool(const std::string &, StructSlotLayoutInfo &)> &resolveStructSlotLayout,
    bool &isPackedSingleSlotOut,
    LocalInfo::ValueKind &packedKindOut,
    int32_t &slotCountOut);
bool isSupportedResultWhyErrorKind(LocalInfo::ValueKind kind);
std::string normalizeResultWhyErrorName(const std::string &errorType, LocalInfo::ValueKind errorKind);
void emitResultWhyErrorLocalFromResult(
    int32_t resultLocal,
    const ResultExprInfo &resultInfo,
    int32_t errorLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
void emitPackedResultPayloadLocalFromResult(
    int32_t resultLocal,
    const ResultExprInfo &resultInfo,
    int32_t errorLocal,
    int32_t payloadLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
bool emitResultWhyEmptyString(
    const std::function<int32_t(const std::string &)> &internString,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
Expr makeResultWhyErrorValueExpr(int32_t errorLocal,
                                 LocalInfo::ValueKind valueKind,
                                 const std::string &namespacePrefix,
                                 int32_t tempOrdinal,
                                 LocalMap &callLocals);
Expr makeResultWhyBoolErrorExpr(
    int32_t errorLocal,
    const std::string &namespacePrefix,
    int32_t tempOrdinal,
    LocalMap &callLocals,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
enum class ResultWhyCallEmitResult {
  Emitted,
  Error,
};
struct ResultWhyCallOps {
  std::function<bool(const std::string &, const std::string &, std::string &)> resolveStructTypeName;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;
  std::function<LocalInfo::Kind(const Expr &)> bindingKind;
  std::function<bool(const Expr &, std::string &, std::vector<std::string> &)> extractFirstBindingTypeTransform;
  std::function<bool(const std::string &, StructSlotLayoutInfo &)> resolveStructSlotLayout;
  std::function<LocalInfo::ValueKind(const std::string &)> valueKindFromTypeName;
  std::function<Expr(LocalMap &, LocalInfo::ValueKind)> makeErrorValueExpr;
  std::function<Expr(LocalMap &)> makeBoolErrorExpr;
  std::function<bool(const Expr &, const Definition &, const LocalMap &)> emitInlineDefinitionCall;
  std::function<bool(int32_t)> emitFileErrorWhy;
  std::function<bool()> emitEmptyString;
};
ResultWhyCallEmitResult emitResolvedResultWhyCall(
    const Expr &expr,
    const ResultExprInfo &resultInfo,
    const LocalMap &localsIn,
    int32_t errorLocal,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResultWhyCallOps &ops,
    std::string &error);
