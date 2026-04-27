



struct OnErrorHandler;
struct ResultReturnInfo;
struct ReturnInfo;

struct InlineDefinitionCallContextSetup {
  ReturnInfo returnInfo;
  bool structDefinition = false;
  bool insertedInlineStackEntry = false;
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
    const std::unordered_map<std::string, std::optional<OnErrorHandler>> &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error);
