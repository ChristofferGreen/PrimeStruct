#include "IrLowererInlineCallContextHelpers.h"

#include <string_view>

namespace primec::ir_lowerer {

namespace {

bool isGeneratedPathWithPrefix(std::string_view path, std::string_view prefix) {
  return path.rfind(prefix, 0) == 0 && path.find("__", prefix.size()) != std::string_view::npos;
}

bool isGeneratedStdlibCollectionStructPath(std::string_view path) {
  return path.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
         path.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
         path.rfind("/std/collections/experimental_soa_vector/SoaVector__", 0) == 0 ||
         path.rfind("/std/collections/internal_soa_storage/SoaColumn__", 0) == 0 ||
         path.rfind("/std/collections/internal_soa_storage/SoaFieldView__", 0) == 0 ||
         isGeneratedPathWithPrefix(path, "/std/collections/internal_soa_storage/SoaColumns");
}

} // namespace

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

  const bool alreadyInInlineStack = inlineStack.count(callee.fullPath) != 0;
  if (alreadyInInlineStack &&
      !(out.structDefinition && isGeneratedStdlibCollectionStructPath(callee.fullPath))) {
    error = "native backend does not support recursive calls: " + callee.fullPath;
    return false;
  }
  if (!alreadyInInlineStack) {
    inlineStack.insert(callee.fullPath);
    out.insertedInlineStackEntry = true;
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
