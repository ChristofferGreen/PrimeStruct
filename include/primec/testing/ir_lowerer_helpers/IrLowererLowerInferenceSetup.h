




struct Definition;
struct Program;
struct ReturnInfo;
struct SemanticProductTargetAdapter;

struct UninitializedStorageAccessInfo;

struct LowerInferenceSetupBootstrapState {
  std::unordered_map<std::string, ReturnInfo> returnInfoCache;
  std::unordered_set<std::string> returnInferenceStack;
  std::function<bool(const std::string &, ReturnInfo &)> getReturnInfo;

  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferExprKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferArrayElementKind;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferBufferElementKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferLiteralOrNameExprKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprBaseKind;
  std::function<CallExpressionReturnKindResolution(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>
      inferCallExprDirectReturnKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)>
      inferCallExprCountAccessGpuFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprOperatorFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, std::string &, LocalInfo::ValueKind &)>
      inferCallExprControlFlowFallbackKind;
  std::function<bool(const Expr &, const LocalMap &, LocalInfo::ValueKind &)> inferCallExprPointerFallbackKind;

  std::function<const Definition *(const Expr &, const LocalMap &)> resolveMethodCallDefinition;
  std::function<const Definition *(const Expr &)> resolveDefinitionCall;
  std::function<LocalInfo::ValueKind(const Expr &, const LocalMap &)> inferPointerTargetKind;
};

struct LowerInferenceSetupBootstrapInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  const SemanticProductTargetAdapter *semanticProductTargets = nullptr;

  IsArrayCountCallFn isArrayCountCall = {};
  IsVectorCapacityCallFn isVectorCapacityCall = {};
  IsEntryArgsNameFn isEntryArgsName = {};
  ResolveExprPathFn resolveExprPath = {};
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName = {};
};

struct LowerInferenceArrayKindSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath = {};
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath = {};
  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
};

struct LowerInferenceExprKindBaseSetupInput {
  GetSetupMathConstantNameFn getMathConstantName = {};
};

struct LowerInferenceExprKindCallBaseSetupInput {
  InferStructExprWithLocalsFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage = {};
};
struct LowerInferenceExprKindCallReturnSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath = {};
  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
};
struct LowerInferenceExprKindCallFallbackSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;

  ResolveExprPathFn resolveExprPath = {};
  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
  IsVectorCapacityCallFn isVectorCapacityCall = {};
  IsEntryArgsNameFn isEntryArgsName = {};
  InferSetupInferenceStructExprPathFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
};
struct LowerInferenceExprKindCallOperatorFallbackSetupInput {
  bool hasMathImport = false;
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
};
struct LowerInferenceExprKindCallControlFlowFallbackSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  ResolveSetupInferenceExprPathFn resolveExprPath = {};
  LowerSetupInferenceMatchToIfFn lowerMatchToIf = {};
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
  IsSetupInferenceBindingMutableFn isBindingMutable = {};
  SetupInferenceBindingKindFn bindingKind = {};
  HasSetupInferenceExplicitBindingTypeTransformFn hasExplicitBindingTypeTransform = {};
  SetupInferenceBindingValueKindFn bindingValueKind = {};
  ApplySetupInferenceStructInfoFn applyStructArrayInfo = {};
  ApplySetupInferenceStructInfoFn applyStructValueInfo = {};
  InferSetupInferenceStructExprPathFn inferStructExprPath = {};
};
struct LowerInferenceExprKindCallPointerFallbackSetupInput {};
struct LowerInferenceExprKindDispatchSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  ResolveExprPathFn resolveExprPath = {};
  std::string *error = nullptr;
};

struct LowerInferenceReturnInfoSetupInput {
  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ResolveStructArrayInfoForReturnFn resolveStructArrayInfoFromPath = {};
  IsBindingMutableForInferenceFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  HasExplicitBindingTypeTransformForInferenceFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  InferValueKindFromLocalsFn inferExprKind = {};
  IsFileErrorBindingForInferenceFn isFileErrorBinding = {};
  SetReferenceArrayInfoForInferenceFn setReferenceArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructValueInfo = {};
  InferStructExprPathFromLocalsFn inferStructExprPath = {};
  IsStringBindingForInferenceFn isStringBinding = {};
  InferValueKindFromLocalsFn inferArrayElementKind = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
};
struct LowerInferenceGetReturnInfoStepInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const LowerInferenceReturnInfoSetupInput *returnInfoSetupInput = nullptr;
};
struct LowerInferenceGetReturnInfoCallbackSetupInput {
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  const LowerInferenceReturnInfoSetupInput *returnInfoSetupInput = nullptr;
  std::string *error = nullptr;
};
struct LowerInferenceGetReturnInfoSetupInput {
  const Program *program = nullptr;
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  std::unordered_map<std::string, ReturnInfo> *returnInfoCache = nullptr;
  std::unordered_set<std::string> *returnInferenceStack = nullptr;
  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  ResolveStructArrayInfoForReturnFn resolveStructArrayInfoFromPath = {};
  IsBindingMutableForInferenceFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  HasExplicitBindingTypeTransformForInferenceFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  InferValueKindFromLocalsFn inferExprKind = {};
  IsFileErrorBindingForInferenceFn isFileErrorBinding = {};
  SetReferenceArrayInfoForInferenceFn setReferenceArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructValueInfo = {};
  InferStructExprPathFromLocalsFn inferStructExprPath = {};
  IsStringBindingForInferenceFn isStringBinding = {};
  InferValueKindFromLocalsFn inferArrayElementKind = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
  std::string *error = nullptr;
};
struct LowerInferenceSetupInput {
  const Program *program = nullptr;
  const std::unordered_map<std::string, const Definition *> *defMap = nullptr;
  const std::unordered_map<std::string, std::string> *importAliases = nullptr;
  const std::unordered_set<std::string> *structNames = nullptr;
  const SemanticProductTargetAdapter *semanticProductTargets = nullptr;

  IsArrayCountCallFn isArrayCountCall = {};
  IsStringCountCallFn isStringCountCall = {};
  IsVectorCapacityCallFn isVectorCapacityCall = {};
  IsEntryArgsNameFn isEntryArgsName = {};
  ResolveExprPathFn resolveExprPath = {};
  GetSetupInferenceBuiltinOperatorNameFn getBuiltinOperatorName = {};
  ResolveStructArrayTypeInfoFn resolveStructArrayInfoFromPath = {};
  InferStructExprWithLocalsFn inferStructExprPath = {};
  ResolveStructFieldSlotFn resolveStructFieldSlot = {};
  std::function<bool(const Expr &, const LocalMap &, UninitializedStorageAccessInfo &, bool &)>
      resolveUninitializedStorage = {};
  bool hasMathImport = false;
  SetupInferenceCombineNumericKindsFn combineNumericKinds = {};
  GetSetupMathConstantNameFn getMathConstantName = {};

  ResolveStructTypeNameForReturnFn resolveStructTypeName = {};
  IsBindingMutableForInferenceFn isBindingMutable = {};
  BindingKindForInferenceFn bindingKind = {};
  HasExplicitBindingTypeTransformForInferenceFn hasExplicitBindingTypeTransform = {};
  BindingValueKindForInferenceFn bindingValueKind = {};
  IsFileErrorBindingForInferenceFn isFileErrorBinding = {};
  SetReferenceArrayInfoForInferenceFn setReferenceArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructArrayInfo = {};
  ApplyStructInfoForInferenceFn applyStructValueInfo = {};
  IsStringBindingForInferenceFn isStringBinding = {};
  ExpandMatchToIfFn lowerMatchToIf = {};
};

bool runLowerInferenceSetupBootstrap(const LowerInferenceSetupBootstrapInput &input,
                                     LowerInferenceSetupBootstrapState &stateOut,
                                     std::string &errorOut);
bool runLowerInferenceSetup(const LowerInferenceSetupInput &input,
                            LowerInferenceSetupBootstrapState &stateOut,
                            std::string &errorOut);
bool runLowerInferenceArrayKindSetup(const LowerInferenceArrayKindSetupInput &input,
                                     LowerInferenceSetupBootstrapState &stateInOut,
                                     std::string &errorOut);
bool runLowerInferenceExprKindBaseSetup(const LowerInferenceExprKindBaseSetupInput &input,
                                        LowerInferenceSetupBootstrapState &stateInOut,
                                        std::string &errorOut);
bool runLowerInferenceExprKindCallBaseSetup(const LowerInferenceExprKindCallBaseSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut);
bool runLowerInferenceExprKindCallReturnSetup(const LowerInferenceExprKindCallReturnSetupInput &input,
                                              LowerInferenceSetupBootstrapState &stateInOut,
                                              std::string &errorOut);
bool runLowerInferenceExprKindCallFallbackSetup(const LowerInferenceExprKindCallFallbackSetupInput &input,
                                                LowerInferenceSetupBootstrapState &stateInOut,
                                                std::string &errorOut);
bool runLowerInferenceExprKindCallOperatorFallbackSetup(
    const LowerInferenceExprKindCallOperatorFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindCallControlFlowFallbackSetup(
    const LowerInferenceExprKindCallControlFlowFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindCallPointerFallbackSetup(
    const LowerInferenceExprKindCallPointerFallbackSetupInput &input,
    LowerInferenceSetupBootstrapState &stateInOut,
    std::string &errorOut);
bool runLowerInferenceExprKindDispatchSetup(const LowerInferenceExprKindDispatchSetupInput &input,
                                            LowerInferenceSetupBootstrapState &stateInOut,
                                            std::string &errorOut);
bool runLowerInferenceReturnInfoSetup(const LowerInferenceReturnInfoSetupInput &input,
                                      const Definition &definition,
                                      ReturnInfo &infoInOut,
                                      std::string &errorOut);
bool runLowerInferenceGetReturnInfoStep(const LowerInferenceGetReturnInfoStepInput &input,
                                        const std::string &path,
                                        ReturnInfo &outInfo,
                                        std::string &errorOut);
bool runLowerInferenceGetReturnInfoCallbackSetup(const LowerInferenceGetReturnInfoCallbackSetupInput &input,
                                                 std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                                 std::string &errorOut);
bool runLowerInferenceGetReturnInfoSetup(const LowerInferenceGetReturnInfoSetupInput &input,
                                         std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfoOut,
                                         std::string &errorOut);
