




struct ReturnInfo;
struct SemanticProductTargetAdapter;

using ValueKindFromTypeNameFn = std::function<LocalInfo::ValueKind(const std::string &)>;
using CombineNumericKindsFn =
    std::function<LocalInfo::ValueKind(LocalInfo::ValueKind, LocalInfo::ValueKind)>;
using InferReceiverExprKindFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ResolveReceiverExprPathFn = std::function<std::string(const Expr &)>;
using IsMethodCallClassifierFn = std::function<bool(const Expr &, const LocalMap &)>;
using GetReturnInfoForPathFn = std::function<bool(const std::string &, ReturnInfo &)>;
using ResolveMethodCallDefinitionFn = std::function<const Definition *(const Expr &, const LocalMap &)>;
using ResolveDefinitionCallFn = std::function<const Definition *(const Expr &)>;

struct SetupTypeAdapters {
  ValueKindFromTypeNameFn valueKindFromTypeName;
  CombineNumericKindsFn combineNumericKinds;
};

SetupTypeAdapters makeSetupTypeAdapters();
ValueKindFromTypeNameFn makeValueKindFromTypeName();
CombineNumericKindsFn makeCombineNumericKinds();

LocalInfo::ValueKind valueKindFromTypeName(const std::string &name);
LocalInfo::ValueKind combineNumericKinds(LocalInfo::ValueKind left, LocalInfo::ValueKind right);
LocalInfo::ValueKind comparisonKind(LocalInfo::ValueKind left, LocalInfo::ValueKind right);
std::string typeNameForValueKind(LocalInfo::ValueKind kind);
std::string normalizeDeclaredCollectionTypeBase(const std::string &base);
bool isExplicitRemovedVectorMethodAliasPath(const std::string &methodName);
bool inferDeclaredReturnCollection(const Definition &definition,
                                   std::string &collectionNameOut,
                                   std::vector<std::string> &collectionArgsOut);
bool inferReceiverTypeFromDeclaredReturn(const Definition &definition, std::string &typeNameOut);
bool resolveMethodCallReceiverExpr(const Expr &callExpr,
                                   const LocalMap &localsIn,
                                   const IsMethodCallClassifierFn &isArrayCountCall,
                                   const IsMethodCallClassifierFn &isVectorCapacityCall,
                                   const IsMethodCallClassifierFn &isEntryArgsName,
                                   const Expr *&receiverOut,
                                   std::string &errorOut);
bool resolveMethodReceiverTypeFromLocalInfo(const LocalInfo &localInfo,
                                            std::string &typeNameOut,
                                            std::string &resolvedTypePathOut);
std::string resolveMethodReceiverTypeNameFromCallExpr(const Expr &receiverCallExpr,
                                                      LocalInfo::ValueKind inferredKind);
std::string resolveMethodReceiverStructTypePathFromCallExpr(
    const Expr &receiverCallExpr,
    const std::string &resolvedReceiverPath,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames);
const Definition *resolveMethodDefinitionFromReceiverTarget(
    const std::string &methodName,
    const std::string &typeName,
    const std::string &resolvedTypePath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
bool resolveReturnInfoKindForPath(const std::string &path,
                                  const GetReturnInfoForPathFn &getReturnInfo,
                                  bool requireArrayReturn,
                                  LocalInfo::ValueKind &kindOut);
bool resolveMethodCallReturnKind(const Expr &methodCallExpr,
                                 const LocalMap &localsIn,
                                 const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                 const ResolveDefinitionCallFn &resolveDefinitionCall,
                                 const GetReturnInfoForPathFn &getReturnInfo,
                                 bool requireArrayReturn,
                                 LocalInfo::ValueKind &kindOut,
                                 bool *methodResolvedOut = nullptr);
bool getNamespacedCollectionHelperName(const Expr &expr, std::string &collectionOut, std::string &helperOut);
bool resolveDefinitionCallReturnKind(const Expr &callExpr,
                                     const std::unordered_map<std::string, const Definition *> &defMap,
                                     const ResolveReceiverExprPathFn &resolveExprPath,
                                     const GetReturnInfoForPathFn &getReturnInfo,
                                     bool requireArrayReturn,
                                     LocalInfo::ValueKind &kindOut,
                                     bool *definitionMatchedOut = nullptr);
bool resolveCountMethodCallReturnKind(const Expr &callExpr,
                                      const LocalMap &localsIn,
                                      const IsMethodCallClassifierFn &isArrayCountCall,
                                      const IsMethodCallClassifierFn &isStringCountCall,
                                      const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                      const GetReturnInfoForPathFn &getReturnInfo,
                                      bool requireArrayReturn,
                                      LocalInfo::ValueKind &kindOut,
                                      bool *methodResolvedOut = nullptr,
                                      const InferReceiverExprKindFn &inferExprKind = {});
bool resolveCapacityMethodCallReturnKind(const Expr &callExpr,
                                         const LocalMap &localsIn,
                                         const ResolveMethodCallDefinitionFn &resolveMethodCallDefinition,
                                         const GetReturnInfoForPathFn &getReturnInfo,
                                         bool requireArrayReturn,
                                         LocalInfo::ValueKind &kindOut,
                                         bool *methodResolvedOut = nullptr);
const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const SemanticProgram *semanticProgram,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const GetReturnInfoForPathFn &getReturnInfo,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
const Definition *resolveMethodCallDefinitionFromExpr(
    const Expr &callExpr,
    const LocalMap &localsIn,
    const IsMethodCallClassifierFn &isArrayCountCall,
    const IsMethodCallClassifierFn &isVectorCapacityCall,
    const IsMethodCallClassifierFn &isEntryArgsName,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_set<std::string> &structNames,
    const InferReceiverExprKindFn &inferExprKind,
    const ResolveReceiverExprPathFn &resolveExprPath,
    const SemanticProgram *semanticProgram,
    const GetReturnInfoForPathFn &getReturnInfo,
    const std::unordered_map<std::string, const Definition *> &defMap,
    std::string &errorOut);
bool resolveMethodReceiverTypeFromNameExpr(const Expr &receiverNameExpr,
                                           const LocalMap &localsIn,
                                           const std::string &methodName,
                                           std::string &typeNameOut,
                                           std::string &resolvedTypePathOut,
                                           std::string &errorOut);
bool resolveMethodReceiverTarget(const Expr &receiverExpr,
                                 const LocalMap &localsIn,
                                 const std::string &methodName,
                                 const std::unordered_map<std::string, std::string> &importAliases,
                                 const std::unordered_set<std::string> &structNames,
                                 const InferReceiverExprKindFn &inferExprKind,
                                 const ResolveReceiverExprPathFn &resolveExprPath,
                                 std::string &typeNameOut,
                                 std::string &resolvedTypePathOut,
                                 std::string &errorOut);
