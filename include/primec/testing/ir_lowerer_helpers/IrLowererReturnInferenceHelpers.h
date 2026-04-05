



struct Definition;
struct ResultReturnInfo;
struct ReturnInfo;
struct SemanticProgram;

enum class MissingReturnBehavior { Error, Void };

struct ReturnInferenceOptions {
  MissingReturnBehavior missingReturnBehavior = MissingReturnBehavior::Error;
  bool includeDefinitionReturnExpr = false;
  bool deferUnknownReturnDependencyErrors = false;
};

struct EntryReturnConfig {
  bool hasReturnTransform = false;
  bool returnsVoid = false;
  bool hasResultInfo = false;
  ResultReturnInfo resultInfo{};
};

using InferBindingIntoLocalsFn = std::function<bool(const Expr &, bool, LocalMap &, std::string &)>;
using InferValueKindFromLocalsFn = std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)>;
using ExpandMatchToIfFn = std::function<bool(const Expr &, Expr &, std::string &)>;
using IsBindingMutableForInferenceFn = std::function<bool(const Expr &)>;
using BindingKindForInferenceFn = std::function<LocalInfo::Kind(const Expr &)>;
using HasExplicitBindingTypeTransformForInferenceFn = std::function<bool(const Expr &)>;
using BindingValueKindForInferenceFn = std::function<LocalInfo::ValueKind(const Expr &, LocalInfo::Kind)>;
using IsFileErrorBindingForInferenceFn = std::function<bool(const Expr &)>;
using ApplyStructInfoForInferenceFn = std::function<void(const Expr &, LocalInfo &)>;
using SetReferenceArrayInfoForInferenceFn = std::function<void(const Expr &, LocalInfo &)>;
using InferStructExprPathFromLocalsFn = std::function<std::string(const Expr &, const LocalMap &)>;
using IsStringBindingForInferenceFn = std::function<bool(const Expr &)>;
using ResolveStructTypeNameForReturnFn = std::function<bool(const std::string &, const std::string &, std::string &)>;
using ResolveStructArrayInfoForReturnFn = std::function<bool(const std::string &, StructArrayTypeInfo &)>;

bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error);
bool analyzeEntryReturnTransforms(const Definition &entryDef,
                                  const SemanticProgram *semanticProgram,
                                  const std::string &entryPath,
                                  EntryReturnConfig &out,
                                  std::string &error);
void analyzeDeclaredReturnTransforms(const Definition &def,
                                     const ResolveStructTypeNameForReturnFn &resolveStructTypeName,
                                     const ResolveStructArrayInfoForReturnFn &resolveStructArrayInfoFromPath,
                                     ReturnInfo &info,
                                     bool &hasReturnTransform,
                                     bool &hasReturnAuto);

bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error,
                               bool *sawUnresolvedDependencyOut = nullptr);
bool inferDefinitionReturnType(const Definition &def,
                               LocalMap localsForInference,
                               const InferBindingIntoLocalsFn &inferBindingIntoLocals,
                               const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                               const InferValueKindFromLocalsFn &inferArrayElementKindFromLocals,
                               const ExpandMatchToIfFn &expandMatchToIf,
                               const ReturnInferenceOptions &options,
                               ReturnInfo &outInfo,
                               std::string &error,
                               bool *sawUnresolvedDependencyOut = nullptr);
bool inferReturnInferenceBindingIntoLocals(const Expr &bindingExpr,
                                           bool isParameter,
                                           const std::string &definitionPath,
                                           LocalMap &activeLocals,
                                           const IsBindingMutableForInferenceFn &isBindingMutable,
                                           const BindingKindForInferenceFn &bindingKind,
                                           const HasExplicitBindingTypeTransformForInferenceFn
                                               &hasExplicitBindingTypeTransform,
                                           const BindingValueKindForInferenceFn &bindingValueKind,
                                           const InferValueKindFromLocalsFn &inferExprKindFromLocals,
                                           const IsFileErrorBindingForInferenceFn &isFileErrorBinding,
                                           const SetReferenceArrayInfoForInferenceFn &setReferenceArrayInfo,
                                           const ApplyStructInfoForInferenceFn &applyStructArrayInfo,
                                           const ApplyStructInfoForInferenceFn &applyStructValueInfo,
                                           const InferStructExprPathFromLocalsFn &inferStructExprPathFromLocals,
                                           const IsStringBindingForInferenceFn &isStringBinding,
                                           std::string &error);
