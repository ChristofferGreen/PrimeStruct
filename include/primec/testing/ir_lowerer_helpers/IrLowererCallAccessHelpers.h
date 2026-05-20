struct SemanticProductIndex;
struct SemanticProductTargetAdapter;

CollectionPairTypeInfo resolveCollectionPairTypeInfo(const Expr &target,
                                               const LocalMap &localsIn,
                                               const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo);
CollectionPairTypeInfo resolveCollectionPairTypeInfo(const Expr &target, const LocalMap &localsIn);
bool inferForwardedCollectionPairTypeInfo(
    const Expr &target,
    const Definition &callee,
    const LocalMap &localsIn,
    const ResolveCallCollectionPairTypeInfoFn &resolveCallCollectionPairTypeInfo,
    CollectionPairTypeInfo &targetInfoOut);
bool validateCollectionPairTypeInfo(const CollectionPairTypeInfo &targetInfo,
                                 const std::string &accessName,
                                 std::string &error);
SemanticStringAccessTargetKind classifyAccessTargetSemanticStringKind(
    const Expr &targetExpr,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex);
ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target,
    const LocalMap &localsIn,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo);
ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(
    const Expr &target, const LocalMap &localsIn);
bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo, std::string &error);
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
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
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
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
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
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error,
    const SemanticProgram *semanticProgram = nullptr,
    const SemanticProductIndex *semanticIndex = nullptr);
bool emitBuiltinArrayAccess(
    const std::string &accessName,
    const Expr &targetExpr,
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const ResolveCallArrayVectorAccessTargetInfoFn &resolveCallArrayVectorAccessTargetInfo,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void()> &emitStringIndexOutOfBounds,
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
    const std::function<void()> &emitArrayIndexOutOfBounds,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm,
    std::string &error);
NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error);
NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &isEntryArgsName,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex);
bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error);
bool resolveValidatedAccessIndexKind(
    const Expr &indexExpr,
    const LocalMap &localsIn,
    const std::string &accessName,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    LocalInfo::ValueKind &indexKindOut,
    std::string &error,
    const SemanticProgram *semanticProgram,
    const SemanticProductIndex *semanticIndex);
IrOpcode keyValueKeyCompareOpcode(LocalInfo::ValueKind keyValueKeyKind);
KeyValueLookupStringKeyResult tryResolveKeyValueLookupStringKey(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    int32_t &stringIndexOut,
    std::string &error);
KeyValueLookupKeyLocalEmitResult tryEmitKeyValueLookupStringKeyLocal(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<bool(const Expr &, const LocalMap &, int32_t &, size_t &)> &resolveStringTableTarget,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<void(int32_t)> &emitPushI32,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error);
bool emitKeyValueLookupNonStringKeyLocal(
    LocalInfo::ValueKind keyValueKeyKind,
    const Expr &lookupKeyExpr,
    const LocalMap &localsIn,
    const std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> &inferExprKind,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t keyLocal,
    std::string &error);
bool emitKeyValueLookupKeyLocal(
    LocalInfo::ValueKind keyValueKeyKind,
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
bool emitKeyValueLookupTargetPointerLocal(
    const Expr &targetExpr,
    const LocalMap &localsIn,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<bool(const Expr &, const LocalMap &)> &emitExpr,
    const std::function<void(int32_t)> &emitStoreLocal,
    int32_t &ptrLocalOut);
KeyValueLookupLoopLocals emitKeyValueLookupLoopSearchScaffold(
    int32_t ptrLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitKeyValueLookupAccessEpilogue(
    const std::string &accessName,
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
bool emitKeyValueLookupAccess(
    const std::string &accessName,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::string &mapStructTypeName,
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
bool emitKeyValueLookupContains(
    LocalInfo::ValueKind keyValueKeyKind,
    const std::string &mapStructTypeName,
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
bool emitBuiltinCanonicalMapInsertOverwriteOrGrow(
    int32_t valuesLocal,
    int32_t valuesWrapperLocal,
    int32_t ptrLocal,
    int32_t keyLocal,
    int32_t valueLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
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
KeyValueLookupLoopLocals emitKeyValueLookupLoopLocals(
    int32_t ptrLocal,
    const std::function<int32_t()> &allocTempLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
KeyValueLookupLoopConditionAnchors emitKeyValueLookupLoopCondition(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
KeyValueLookupLoopMatchAnchors emitKeyValueLookupLoopMatchCheck(
    int32_t ptrLocal,
    int32_t indexLocal,
    int32_t keyLocal,
    LocalInfo::ValueKind keyValueKeyKind,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
void emitKeyValueLookupLoopAdvanceAndPatch(
    size_t jumpNotMatch,
    size_t jumpLoopEnd,
    size_t jumpFound,
    size_t loopStart,
    int32_t indexLocal,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitKeyValueLookupAtKeyNotFoundGuard(
    int32_t indexLocal,
    int32_t countLocal,
    const std::function<void()> &emitMapKeyNotFound,
    const std::function<size_t()> &instructionCount,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction,
    const std::function<void(size_t, uint64_t)> &patchInstructionImm);
void emitKeyValueLookupValueLoad(
    int32_t ptrLocal,
    int32_t indexLocal,
    const std::function<void(IrOpcode, uint64_t)> &emitInstruction);
bool validateKeyValueLookupKeyKind(LocalInfo::ValueKind keyValueKeyKind,
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
bool isStructDefinition(const Definition &def, const SemanticProgram *semanticProgram);
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
