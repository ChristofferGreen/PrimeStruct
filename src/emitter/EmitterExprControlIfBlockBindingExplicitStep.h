#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlIfBlockBindingExplicitEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlIfBlockBindingExplicitStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlIfBlockBindingExplicitStepResult runEmitterExprControlIfBlockBindingExplicitStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const std::string &namespacePrefix,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlIfBlockBindingExplicitEmitExprFn &emitExpr);

} // namespace primec::emitter
