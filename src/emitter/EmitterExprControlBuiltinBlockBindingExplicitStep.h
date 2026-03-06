#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "primec/Ast.h"
#include "primec/Emitter.h"

namespace primec::emitter {

using EmitterExprControlBuiltinBlockBindingExplicitEmitExprFn = std::function<std::string(const Expr &)>;

struct EmitterExprControlBuiltinBlockBindingExplicitStepResult {
  bool handled = false;
  std::string emittedStatement;
};

EmitterExprControlBuiltinBlockBindingExplicitStepResult runEmitterExprControlBuiltinBlockBindingExplicitStep(
    const Expr &stmt,
    const Emitter::BindingInfo &binding,
    bool hasExplicitType,
    bool needsConst,
    bool useRef,
    const std::string &namespacePrefix,
    const std::unordered_map<std::string, std::string> &importAliases,
    const std::unordered_map<std::string, std::string> &structTypeMap,
    const EmitterExprControlBuiltinBlockBindingExplicitEmitExprFn &emitExpr);

} // namespace primec::emitter
