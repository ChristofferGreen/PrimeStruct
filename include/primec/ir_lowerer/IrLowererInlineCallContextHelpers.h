#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "primec/ir_lowerer/IrLowererFlowHelpers.h"
#include "primec/ir_lowerer/IrLowererOnErrorHelpers.h"
#include "primec/ir_lowerer/IrLowererSharedTypes.h"
#include "primec/ast/Ast.h"

namespace primec::ir_lowerer {

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
    const OnErrorByDefinition &onErrorByDef,
    InlineDefinitionCallContextSetup &out,
    std::string &error);
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

} // namespace primec::ir_lowerer
