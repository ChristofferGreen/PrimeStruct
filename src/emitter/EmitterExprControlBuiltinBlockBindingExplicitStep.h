#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "primec/ast/Ast.h"
#include "primec/backend/Emitter.h"
#include "primec/support/CallbackTypes.h"

namespace primec::emitter {


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
    const ExprStringFn &emitExpr);

} // namespace primec::emitter
