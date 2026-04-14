#include "IrLowererInlineCallContextHelpers.h"

namespace primec::ir_lowerer {

bool prepareInlineDefinitionCallContext(
    const Definition &callee,
    bool requireValue,
    const std::function<bool(const std::string &, ReturnInfo &)> &getReturnInfo,
    const std::function<bool(const Definition &)> &isStructDefinition,
    std::unordered_set<std::string> &inlineStack,
    std::unordered_set<std::string> &loweredCallTargets,
    const OnErrorByDefinition &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error) {
  out = InlineDefinitionCallContextSetup{};
  if (!getReturnInfo(callee.fullPath, out.returnInfo)) {
    return false;
  }

  out.structDefinition = isStructDefinition(callee);
  const bool isGeneratedMapInsertHelper =
      callee.fullPath == "/std/collections/mapInsert" ||
      callee.fullPath.rfind("/std/collections/mapInsert__", 0) == 0 ||
      callee.fullPath == "/std/collections/experimental_map/mapInsert" ||
      callee.fullPath.rfind("/std/collections/experimental_map/mapInsert__", 0) == 0 ||
      callee.fullPath == "/std/collections/experimental_map/mapInsertRef" ||
      callee.fullPath.rfind("/std/collections/experimental_map/mapInsertRef__", 0) == 0;
  if (out.returnInfo.returnsVoid && requireValue && !out.structDefinition &&
      !isGeneratedMapInsertHelper) {
    error = "void call not allowed in expression context: " + callee.fullPath;
    return false;
  }

  if (!inlineStack.insert(callee.fullPath).second) {
    error = "native backend does not support recursive calls: " + callee.fullPath;
    return false;
  }
  loweredCallTargets.insert(callee.fullPath);

  auto onErrorIt = onErrorByDef.find(callee.fullPath);
  if (onErrorIt != onErrorByDef.end() && onErrorIt->second.has_value()) {
    out.scopedOnError = onErrorIt->second;
  }
  if (out.returnInfo.isResult) {
    out.scopedResult = ResultReturnInfo{true, out.returnInfo.resultHasValue};
  }
  return true;
}

} // namespace primec::ir_lowerer
