struct Definition;
struct SemanticProductTargetAdapter;

MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target,
                                               const LocalMap &localsIn,
                                               const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo);
MapAccessTargetInfo resolveMapAccessTargetInfo(const Expr &target, const LocalMap &localsIn);
bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error);
ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo);
ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target, const LocalMap &localsIn);
bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo, std::string &error);
MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
MapAccessLookupEmitResult tryEmitMapAccessLookup(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
StringTableAccessEmitResult tryEmitStringTableAccessLoad(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitArrayVectorIndexedAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallMapAccessTargetInfoFn &resolveCallMapAccessTargetInfo,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    size_t stringTableCount,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error);
bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error);
IrOpcode mapKeyCompareOpcode(LocalInfo::ValueKind mapKeyKind);
MapLookupStringKeyResult tryResolveMapLookupStringKey(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    int32_t &stringIndexOut,
    std::string &error);
MapLookupKeyLocalEmitResult tryEmitMapLookupStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error);
bool emitMapLookupNonStringKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error);
bool emitMapLookupKeyLocal(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &keyLocalOut,
    std::string &error);
bool emitMapLookupTargetPointerLocal(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &ptrLocalOut);
MapLookupLoopLocals emitMapLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitMapLookupAccessEpilogue(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
bool emitMapLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
bool emitMapLookupContains(
    LocalInfo::ValueKind mapKeyKind,
    const Expr &targetExpr,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
void emitStringAccessLoad(
    const std::string &accessName,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    size_t stringLength,
    int32_t stringIndex,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitArrayVectorAccessLoad(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    LocalInfo::ValueKind indexKind,
    bool isVectorTarget,
    uint64_t arrayHeaderSlots,
    int32_t elementSlotCount,
    bool loadElementValue,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
MapLookupLoopLocals emitMapLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
MapLookupLoopConditionAnchors emitMapLookupLoopCondition(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
MapLookupLoopMatchAnchors emitMapLookupLoopMatchCheck(
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind mapKeyKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
void emitMapLookupLoopAdvanceAndPatch(
    size_t jumpNotMatch,
    size_t jumpLoopEnd,
    size_t jumpFound,
    size_t loopStart,
    int32_t indexLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitMapLookupAtKeyNotFoundGuard(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitMapLookupValueLoad(
    int32_t ptrLocal,
    int32_t indexLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
bool validateMapLookupKeyKind(LocalInfo::ValueKind mapKeyKind,
                              LocalInfo::ValueKind lookupKeyKind,
                              std::string &error);
CountMethodFallbackResult tryEmitNonMethodCountFallback(
    const Expr &expr,
    const std::function<bool(const Expr &)> &isArrayCountCall,
    const std::function<bool(const Expr &)> &isStringCountCall,
    const std::function<const Definition *(const Expr &)> &resolveMethodCallDefinition,
    const std::function<bool(const Expr &, const Definition &)> &emitInlineDefinitionCall,
    std::string &error,
    std::function<bool(const Expr &)> isCollectionAccessReceiverExpr = {});

bool buildOrderedCallArguments(const Expr &callExpr,
                               const std::vector<Expr> &params,
                               std::vector<const Expr *> &ordered,
                               std::string &error);
bool definitionHasTransform(const Definition &def, const std::string &transformName);
bool isStructTransformName(const std::string &name);
bool isStructDefinition(const Definition &def);
bool isStructDefinition(const Definition &def, const SemanticProductTargetAdapter *semanticProductTargets);
bool isStructHelperDefinition(const Definition &def,
                              const std::unordered_set<std::string> &structNames,
                              std::string &parentStructPathOut);
Expr makeStructHelperThisParam(const std::string &structPath, bool isMutable);
bool isStaticFieldBinding(const Expr &expr);
bool collectInstanceStructFieldParams(const Definition &structDef,
                                      std::vector<Expr> &paramsOut,
                                      std::string &error);
bool buildInlineCallParameterList(const Definition &callee,
                                  const std::unordered_set<std::string> &structNames,
                                  std::vector<Expr> &paramsOut,
                                  std::string &error);
bool buildInlineCallOrderedArguments(const Expr &callExpr,
                                     const Definition &callee,
                                     const std::unordered_set<std::string> &structNames,
                                     const LocalMap &callerLocals,
                                     std::vector<Expr> &paramsOut,
                                     std::vector<const Expr *> &orderedArgsOut,
                                     std::vector<const Expr *> &packedArgsOut,
                                     size_t &packedParamIndexOut,
                                     std::string &error);
const Definition *resolveDefinitionByPath(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);
std::string resolveDefinitionNamespacePrefix(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &definitionPath);
