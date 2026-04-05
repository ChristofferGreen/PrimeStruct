



struct Definition;
struct OnErrorHandler;
struct ResultReturnInfo;
struct ReturnInfo;

struct InlineDefinitionCallContextSetup {
  ReturnInfo returnInfo;
  bool structDefinition = false;
  std::optional<OnErrorHandler> scopedOnError;
  std::optional<ResultReturnInfo> scopedResult;
};

bool prepareInlineDefinitionCallContext(
    const Definition &callee,
    bool requireValue,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Definition &)> &isStructDefinition,
    std::unordered_set<std::string> &inlineStack,
    std::unordered_set<std::string> &loweredCallTargets,
    const OnErrorByDefinition &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error);
