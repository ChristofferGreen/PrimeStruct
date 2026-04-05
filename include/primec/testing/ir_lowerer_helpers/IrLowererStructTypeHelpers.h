



struct Definition;
struct LayoutFieldBinding;
struct Program;
struct SemanticProductTargetAdapter;

using ResolveStructTypeNameFn = std::function<bool(const std::string &, const std::string &, std::string &)>;
using ValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using InferStructExprPathFn = std::function<std::string(const Expr &)>;
using InferStructExprWithLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using IsKnownStructPathFn = std::function<bool(const std::string &)>;
using InferDefinitionStructReturnPathFn = std::function<std::string(const std::string &)>;

struct StructArrayFieldInfo {
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

struct StructArrayTypeInfo {
  std::string structPath;
  LocalInfo::ValueKind elementKind = LocalInfo::ValueKind::Unknown;
  int32_t fieldCount = 0;
};

using ResolveStructArrayTypeInfoFn = std::function<bool(const std::string &, StructArrayTypeInfo &)>;
using ApplyStructArrayInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using ApplyStructValueInfoFn = std::function<void(const Expr &, LocalInfo &)>;
using CollectStructArrayFieldsFn = std::function<bool(const std::string &, std::vector<StructArrayFieldInfo> &)>;

struct StructTypeResolutionAdapters {
  ResolveStructTypeNameFn resolveStructTypeName{};
  ApplyStructValueInfoFn applyStructValueInfo{};
};
using SetupCombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
struct SetupTypeAndStructTypeAdapters {
  ValueKindFromTypeNameFn valueKindFromTypeName{};
  SetupCombineNumericKindsFn combineNumericKinds{};
  StructTypeResolutionAdapters structTypeResolutionAdapters{};
};

struct StructArrayInfoAdapters {
  ResolveStructArrayTypeInfoFn resolveStructArrayTypeInfoFromPath{};
  ApplyStructArrayInfoFn applyStructArrayInfo{};
};

struct StructSlotFieldInfo {
  std::string name;
  int32_t slotOffset = -1;
  int32_t slotCount = 0;
  LocalInfo::ValueKind valueKind = LocalInfo::ValueKind::Unknown;
  std::string structPath;
};

struct StructLayoutFieldInfo {
  std::string name;
  std::string typeName;
  std::string typeTemplateArg;
  bool isStatic = false;
};

struct StructSlotLayoutInfo {
  std::string structPath;
  int32_t totalSlots = 0;
  std::vector<StructSlotFieldInfo> fields;
};

using ResolveStructFieldSlotFn =
    std::function<bool(const std::string &, const std::string &, StructSlotFieldInfo &)>;
using ResolveStructSlotLayoutFn = std::function<bool(const std::string &, StructSlotLayoutInfo &)>;
using ResolveStructSlotFieldsFn =
    std::function<bool(const std::string &, std::vector<StructSlotFieldInfo> &)>;
struct StructSlotResolutionAdapters {
  ResolveStructSlotLayoutFn resolveStructSlotLayout{};
  ResolveStructFieldSlotFn resolveStructFieldSlot{};
};
struct StructLayoutResolutionAdapters {
  StructArrayInfoAdapters structArrayInfo{};
  StructSlotResolutionAdapters structSlotResolution{};
};
using CollectStructLayoutFieldsFn =
    std::function<bool(const std::string &, std::vector<StructLayoutFieldInfo> &)>;
using ResolveDefinitionNamespacePrefixByPathFn =
    std::function<bool(const std::string &, std::string &)>;
using StructSlotLayoutCache = std::unordered_map<std::string, StructSlotLayoutInfo>;
using StructLayoutFieldIndex = std::unordered_map<std::string, std::vector<StructLayoutFieldInfo>>;
using AppendStructLayoutFieldFn =
    std::function<void(const std::string &, const StructLayoutFieldInfo &)>;
using EnumerateStructLayoutFieldsFn =
    std::function<void(const AppendStructLayoutFieldFn &)>;

std::string joinTemplateArgsText(const std::vector<std::string> &args);

StructTypeResolutionAdapters makeStructTypeResolutionAdapters(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
SetupTypeAndStructTypeAdapters makeSetupTypeAndStructTypeAdapters(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
StructArrayInfoAdapters makeStructArrayInfoAdapters(
    const StructLayoutFieldIndex &fieldIndex,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName);
ResolveStructTypeNameFn makeResolveStructTypePathFromScope(
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
bool isWildcardImportPath(const std::string &path, std::string &prefixOut);
void buildDefinitionMapAndStructNames(
    const std::vector<Definition> &definitions,
    std::unordered_map<std::string, const Definition *> &defMapOut,
    std::unordered_set<std::string> &structNamesOut,
    const SemanticProductTargetAdapter *semanticProductTargets = nullptr);
void appendStructLayoutFieldsFromFieldBindings(
    const std::unordered_map<std::string, std::vector<LayoutFieldBinding>> &structFieldInfoByName,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const AppendStructLayoutFieldFn &appendStructLayoutField);
std::unordered_map<std::string, std::string> buildImportAliasesFromProgram(
    const std::vector<std::string> &imports,
    const std::vector<Definition> &definitions,
    const std::unordered_map<std::string, const Definition *> &defMap);
bool resolveStructTypePathFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases,
    std::string &resolvedOut);
std::string resolveStructTypePathCandidateFromScope(
    const std::string &typeName,
    const std::string &namespacePrefix,
    const std::unordered_set<std::string> &structNames,
    const std::unordered_map<std::string, std::string> &importAliases);
std::string resolveStructLayoutExprPathFromScope(
    const Expr &expr,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::unordered_map<std::string, std::string> &importAliases);
bool resolveDefinitionNamespacePrefixFromMap(
    const std::unordered_map<std::string, const Definition *> &defMap,
    const std::string &defPath,
    std::string &namespacePrefixOut);
StructLayoutFieldIndex buildStructLayoutFieldIndex(
    std::size_t structReserveHint,
    const EnumerateStructLayoutFieldsFn &enumerateStructLayoutFields);
bool collectStructLayoutFieldsFromIndex(const StructLayoutFieldIndex &fieldIndex,
                                        const std::string &structPath,
                                        std::vector<StructLayoutFieldInfo> &out);
bool collectStructArrayFieldsFromLayoutIndex(const StructLayoutFieldIndex &fieldIndex,
                                             const std::string &structPath,
                                             std::vector<StructArrayFieldInfo> &out);
bool resolveStructArrayTypeInfoFromLayoutFieldIndex(const std::string &structPath,
                                                    const StructLayoutFieldIndex &fieldIndex,
                                                    const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                                    StructArrayTypeInfo &out);
bool resolveStructArrayTypeInfoFromBindingWithLayoutFieldIndex(
    const Expr &expr,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructArrayTypeInfo &out);
ResolveStructArrayTypeInfoFn makeResolveStructArrayTypeInfoFromLayoutFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName);
bool resolveStructArrayTypeInfoFromPath(const std::string &structPath,
                                        const CollectStructArrayFieldsFn &collectStructArrayFields,
                                        const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                        StructArrayTypeInfo &out);
bool resolveStructArrayTypeInfoFromBinding(const Expr &expr,
                                           const ResolveStructTypeNameFn &resolveStructTypeName,
                                           const CollectStructArrayFieldsFn &collectStructArrayFields,
                                           const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                           StructArrayTypeInfo &out);
void applyStructArrayInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     const CollectStructArrayFieldsFn &collectStructArrayFields,
                                     const ValueKindFromTypeNameFn &valueKindFromTypeName,
                                     LocalInfo &info);
void applyStructArrayInfoFromBindingWithLayoutFieldIndex(
    const Expr &expr,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    LocalInfo &info);
ApplyStructArrayInfoFn makeApplyStructArrayInfoFromBindingWithLayoutFieldIndex(
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const StructLayoutFieldIndex &fieldIndex,
    const ValueKindFromTypeNameFn &valueKindFromTypeName);
bool resolveStructSlotFieldByName(const std::vector<StructSlotFieldInfo> &fields,
                                  const std::string &fieldName,
                                  StructSlotFieldInfo &out);
bool resolveStructFieldSlotFromLayout(const std::string &structPath,
                                      const std::string &fieldName,
                                      const ResolveStructSlotFieldsFn &resolveStructSlotFields,
                                      StructSlotFieldInfo &out);
bool resolveStructSlotLayoutFromDefinitionFields(
    const std::string &structPath,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error);
bool resolveStructSlotLayoutFromDefinitionFieldIndex(
    const std::string &structPath,
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotLayoutInfo &out,
    std::string &error);
bool resolveStructFieldSlotFromDefinitionFields(
    const std::string &structPath,
    const std::string &fieldName,
    const CollectStructLayoutFieldsFn &collectStructLayoutFields,
    const ResolveDefinitionNamespacePrefixByPathFn &resolveDefinitionNamespacePrefix,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotFieldInfo &out,
    std::string &error);
bool resolveStructFieldSlotFromDefinitionFieldIndex(
    const std::string &structPath,
    const std::string &fieldName,
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    StructSlotFieldInfo &out,
    std::string &error);
ResolveStructSlotLayoutFn makeResolveStructSlotLayoutFromDefinitionFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error);
ResolveStructFieldSlotFn makeResolveStructFieldSlotFromDefinitionFieldIndex(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error);
StructSlotResolutionAdapters makeStructSlotResolutionAdapters(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    StructSlotLayoutCache &layoutCache,
    std::unordered_set<std::string> &layoutStack,
    std::string &error);
StructSlotResolutionAdapters makeStructSlotResolutionAdaptersWithOwnedState(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    std::string &error);
StructLayoutResolutionAdapters makeStructLayoutResolutionAdaptersWithOwnedSlotState(
    const StructLayoutFieldIndex &fieldIndex,
    const std::unordered_map<std::string, const Definition *> &defMap,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const ValueKindFromTypeNameFn &valueKindFromTypeName,
    std::string &error);
ApplyStructValueInfoFn makeApplyStructValueInfoFromBinding(
    const ResolveStructTypeNameFn &resolveStructTypeName);
void applyStructValueInfoFromBinding(const Expr &expr,
                                     const ResolveStructTypeNameFn &resolveStructTypeName,
                                     LocalInfo &info);
std::string inferStructReturnPathFromDefinition(
    const Definition &def,
    const ResolveStructTypeNameFn &resolveStructTypeName,
    const InferStructExprPathFn &inferStructExprPath);
std::string inferStructPathFromCallTarget(
    const Expr &expr,
    const InferStructExprPathFn &resolveExprPath,
    const IsKnownStructPathFn &isKnownStructPath,
    const InferDefinitionStructReturnPathFn &inferDefinitionStructReturnPath);
std::string inferStructPathFromNameExpr(const Expr &expr, const LocalMap &localsIn);
std::string inferStructPathFromFieldAccessCall(
    const Expr &expr,
    const LocalMap &localsIn,
    const InferStructExprWithLocalsFn &inferStructExprPath,
    const ResolveStructFieldSlotFn &resolveStructFieldSlot);
