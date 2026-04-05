




struct UninitializedTypeInfo {
  LocalInfo::Kind kind = LocalInfo::Kind::Value;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapKeyKind = LocalInfo::ValueKind::Unknown;
  LocalInfo::ValueKind mapValueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

struct UninitializedFieldBindingInfo {
  std::string name;
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

using UninitializedFieldBindingIndex =
    std::unordered_map<std::string, std::vector<UninitializedFieldBindingInfo>>;
struct StructAndUninitializedFieldIndexes {
  StructLayoutFieldIndex structLayoutFieldIndex{};
  UninitializedFieldBindingIndex uninitializedFieldBindingIndex{};
};
using AppendUninitializedFieldBindingFn =
    std::function<void(const std::string &structPath, const UninitializedFieldBindingInfo &fieldBinding)>;
using EnumerateUninitializedFieldBindingsFn =
    std::function<void(const AppendUninitializedFieldBindingFn &appendFieldBinding)>;

using FindUninitializedFieldTemplateArgFn =
    std::function<bool(const std::string &structPath, const std::string &fieldName, std::string &typeTemplateArgOut)>;
using CollectUninitializedFieldBindingsFn =
    std::function<bool(const std::string &structPath, std::vector<UninitializedFieldBindingInfo> &fieldsOut)>;
using ResolveDefinitionNamespacePrefixFn = std::function<std::string(const std::string &structPath)>;
using ResolveUninitializedFieldTypeInfoFn =
    std::function<bool(const std::string &typeText, const std::string &namespacePrefix, UninitializedTypeInfo &out)>;
using InferStructReturnExprWithVisitedFn =
    std::function<std::string(const Expr &, std::unordered_set<std::string> &)>;
using InferDefinitionStructReturnPathWithVisitedFn =
    std::function<std::string(const std::string &, std::unordered_set<std::string> &)>;

struct UninitializedFieldStorageTypeInfo {
  const LocalInfo *receiver = nullptr;
  std::string structPath;
  UninitializedTypeInfo typeInfo;
};

struct UninitializedFieldStorageAccessInfo {
  const LocalInfo *receiver = nullptr;
  StructSlotFieldInfo fieldSlot;
  UninitializedTypeInfo typeInfo;
};

struct UninitializedStorageAccessInfo {
  enum class Location { Local, Field, Indirect } location = Location::Local;
  const LocalInfo *local = nullptr;
  const LocalInfo *pointer = nullptr;
  const Expr *pointerExpr = nullptr;
  const LocalInfo *receiver = nullptr;
  StructSlotFieldInfo fieldSlot;
  UninitializedTypeInfo typeInfo;
};

struct UninitializedLocalStorageAccessInfo {
  const LocalInfo *local = nullptr;
  UninitializedTypeInfo typeInfo;
};

using ResolveUninitializedStorageAccessFromFieldIndexFn =
    std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>;

struct UninitializedResolutionAdapters {
  ResolveUninitializedFieldTypeInfoFn resolveUninitializedTypeInfo{};
  ResolveUninitializedStorageAccessFromFieldIndexFn resolveUninitializedStorage{};
  InferStructExprWithLocalsFn inferStructExprPath{};
};

struct StructAndUninitializedResolutionSetup {
  StructAndUninitializedFieldIndexes fieldIndexes{};
  StructLayoutResolutionAdapters structLayoutResolutionAdapters{};
  UninitializedResolutionAdapters uninitializedResolutionAdapters{};
};
struct SetupTypeStructAndUninitializedResolutionSetup {
  SetupTypeAndStructTypeAdapters setupTypeAndStructTypeAdapters{};
  StructAndUninitializedResolutionSetup structAndUninitializedResolutionSetup{};
};
struct SetupMathTypeStructAndUninitializedResolutionSetup {
  SetupMathAndBindingAdapters setupMathAndBindingAdapters{};
  SetupTypeStructAndUninitializedResolutionSetup setupTypeStructAndUninitializedResolutionSetup{};
};
struct EntrySetupMathTypeStructAndUninitializedResolutionSetup {
  EntryCountCallOnErrorSetup entryCountCallOnErrorSetup{};
  SetupMathTypeStructAndUninitializedResolutionSetup setupMathTypeStructAndUninitializedResolutionSetup{};
};
struct RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup {
  RuntimeErrorAndStringLiteralSetup runtimeErrorAndStringLiteralSetup{};
  EntrySetupMathTypeStructAndUninitializedResolutionSetup entrySetupMathTypeStructAndUninitializedResolutionSetup{};
};
struct EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup {
  EntryReturnConfig entryReturnConfig{};
  RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup
      runtimeEntrySetupMathTypeStructAndUninitializedResolutionSetup{};
};

bool buildProgramEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildEntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    const std::string &entryPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntryReturnRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildRuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    std::vector<std::string> &stringTable,
    IrFunction &function,
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    RuntimeEntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildEntrySetupMathTypeStructAndUninitializedResolutionSetup(
    const Program &program,
    const Definition &entryDef,
    bool definitionReturnsVoid,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases,
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    EntrySetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildSetupMathTypeStructAndUninitializedResolutionSetup(
    bool hasMathImport,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupMathTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildSetupTypeStructAndUninitializedResolutionSetup(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const InferStructExprPathFn &resolveExprPath,
    SetupTypeStructAndUninitializedResolutionSetup &out,
    std::string &error);
bool buildStructAndUninitializedResolutionSetup(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    const InferStructExprPathFn &resolveExprPath,
    StructAndUninitializedResolutionSetup &out,
    std::string &error);
UninitializedResolutionAdapters makeUninitializedResolutionAdapters(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error);
ResolveUninitializedFieldTypeInfoFn makeResolveUninitializedTypeInfo(
    const ResolveStructTypeNameFn &resolveStructTypePath,
    std::string &error);
bool resolveUninitializedTypeInfo(const std::string &typeText,
                                  const std::string &namespacePrefix,
                                  const ResolveStructTypeNameFn &resolveStructTypePath,
                                  UninitializedTypeInfo &out,
                                  std::string &error);
bool resolveUninitializedTypeInfoFromLocalStorage(const LocalInfo &local, UninitializedTypeInfo &out);
bool resolveUninitializedLocalStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const LocalInfo *&localOut,
                                               UninitializedTypeInfo &typeInfoOut,
                                               bool &resolvedOut);
bool resolveUninitializedLocalStorageAccess(const Expr &storage,
                                            const LocalMap &localsIn,
                                            UninitializedLocalStorageAccessInfo &out,
                                            bool &resolvedOut);
StructAndUninitializedFieldIndexes buildStructAndUninitializedFieldIndexes(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields);
UninitializedFieldBindingIndex buildUninitializedFieldBindingIndex(
    std::size_t structReserveHint,
    const EnumerateUninitializedFieldBindingsFn &enumerateFieldBindings);
UninitializedFieldBindingIndex buildUninitializedFieldBindingIndexFromStructLayoutFieldIndex(
    const StructLayoutFieldIndex &fieldIndex);
bool hasUninitializedFieldBindingsForStructPath(const UninitializedFieldBindingIndex &fieldIndex,
                                                const std::string &structPath);
std::string inferStructPathFromCallTargetWithFieldBindingIndex(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath);
std::string inferStructPathFromCallTargetWithFieldBindingIndexAndVisited(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const InferDefinitionStructReturnPathWithVisitedFn &inferDefinitionStructReturnPath,
    std::unordered_set<std::string> &visitedDefs);
std::string inferStructReturnPathFromDefinitionMapWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath,
    std::unordered_set<std::string> &visitedDefs);
std::string inferStructReturnPathFromDefinitionMap(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructReturnExprWithVisitedFn &inferStructReturnExprPath);
std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndexWithVisited(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    std::unordered_set<std::string> &visitedDefs);
std::string inferStructReturnPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::string &defPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex);
std::string inferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const Expr &expr,
    const LocalMap &localsIn,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);
InferStructExprWithLocalsFn makeInferStructExprPathFromDefinitionMapByCallTargetWithFieldIndex(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &resolveExprPath,
    const UninitializedFieldBindingIndex &fieldIndex,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);
bool collectUninitializedFieldBindingsFromIndex(const UninitializedFieldBindingIndex &fieldIndex,
                                                const std::string &structPath,
                                                std::vector<UninitializedFieldBindingInfo> &fieldsOut);
bool resolveUninitializedFieldTemplateArg(const std::string &structPath,
                                          const std::string &fieldName,
                                          const CollectUninitializedFieldBindingsFn &collectFieldBindings,
                                          std::string &typeTemplateArgOut);
bool findUninitializedFieldTemplateArg(const std::vector<UninitializedFieldBindingInfo> &fields,
                                       const std::string &fieldName,
                                       std::string &typeTemplateArgOut);
bool resolveUninitializedFieldStorageCandidate(const Expr &storage,
                                               const LocalMap &localsIn,
                                               const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                               const LocalInfo *&receiverOut,
                                               std::string &structPathOut,
                                               std::string &typeTemplateArgOut);
bool resolveUninitializedFieldStorageTypeInfo(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    UninitializedFieldStorageTypeInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedFieldStorageAccess(
    const Expr &storage,
    const LocalMap &localsIn,
    const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedFieldStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedStorageAccess(const Expr &storage,
                                       const LocalMap &localsIn,
                                       const FindUninitializedFieldTemplateArgFn &findFieldTemplateArg,
                                       const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
                                       const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
                                       const ResolveStructFieldSlotFn &resolveStructFieldSlot,
                                       UninitializedStorageAccessInfo &out,
                                       bool &resolvedOut,
                                       std::string &error);
bool resolveUninitializedStorageAccessWithFieldBindings(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const ResolveDefinitionNamespacePrefixFn &resolveDefinitionNamespacePrefix,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedStorageAccessFromDefinitions(
    const Expr &storage,
    const LocalMap &localsIn,
    const CollectUninitializedFieldBindingsFn &collectFieldBindings,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
bool resolveUninitializedStorageAccessFromDefinitionFieldIndex(
    const Expr &storage,
    const LocalMap &localsIn,
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    UninitializedStorageAccessInfo &out,
    bool &resolvedOut,
    std::string &error);
ResolveUninitializedStorageAccessFromFieldIndexFn
makeResolveUninitializedStorageAccessFromDefinitionFieldIndex(
    const UninitializedFieldBindingIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveUninitializedFieldTypeInfoFn &resolveUninitializedTypeInfo,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot,
    std::string &error);
