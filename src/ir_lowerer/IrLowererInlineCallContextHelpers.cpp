#include "IrLowererInlineCallContextHelpers.h"

namespace primec::ir_lowerer {

bool prepareInlineDefinitionCallContext(
    const Definition &callee,
    bool requireValue,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Definition &)> &isStructDefinition,
    std::unordered_set<std::string> &inlineStack,
    std::unordered_set<std::string> &loweredCallTargets,
    const std::unordered_map<std::string, OnErrorHandler> &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error) {
  out = InlineDefinitionCallContextSetup{};
  if (!getReturnInfo(callee.fullPath, out.returnInfo)) {
    return false;
  }

  out.structDefinition = isStructDefinition(callee);
  if (out.returnInfo.returnsVoid && requireValue && !out.structDefinition) {
    error = "void call not allowed in expression context: " + callee.fullPath;
    return false;
  }

  if (!inlineStack.insert(callee.fullPath).second) {
    error = "native backend does not support recursive calls: " + callee.fullPath;
    return false;
  }
  loweredCallTargets.insert(callee.fullPath);

  auto onErrorIt = onErrorByDef.find(callee.fullPath);
  if (onErrorIt != onErrorByDef.end()) {
    out.scopedOnError = onErrorIt->second;
  }
  if (out.returnInfo.isResult) {
    out.scopedResult = ResultReturnInfo{true, out.returnInfo.resultHasValue};
  }
  return true;
}

} // namespace primec::ir_lowerer
