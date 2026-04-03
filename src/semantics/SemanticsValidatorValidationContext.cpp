#include "SemanticsValidator.h"

#include <utility>

namespace primec::semantics {

bool SemanticsValidator::makeDefinitionValidationContext(const Definition &def, ValidationContext &out) {
  out = {};
  out.definitionPath = def.fullPath;
  for (const auto &transform : def.transforms) {
    if (transform.name == "compute") {
      out.definitionIsCompute = true;
    } else if (transform.name == "unsafe") {
      out.definitionIsUnsafe = true;
    } else if (transform.name == "return" && transform.templateArgs.size() == 1) {
      ResultTypeInfo resultInfo;
      if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultInfo)) {
        out.resultType = std::move(resultInfo);
      }
    }
  }
  out.activeEffects = resolveEffects(def.transforms, def.fullPath == entryPath_);
  std::optional<OnErrorHandler> onErrorHandler;
  if (!parseOnErrorTransform(def.transforms, def.namespacePrefix, def.fullPath, onErrorHandler)) {
    return false;
  }
  out.onError = std::move(onErrorHandler);
  return true;
}

SemanticsValidator::ValidationContext
SemanticsValidator::makeExecutionValidationContext(const Execution &exec) const {
  ValidationContext context;
  context.definitionPath.clear();
  context.definitionIsCompute = false;
  context.definitionIsUnsafe = false;
  context.activeEffects = resolveEffects(exec.transforms, false);
  return context;
}

SemanticsValidator::ValidationContext
SemanticsValidator::buildDefinitionValidationContext(const Definition &def) const {
  ValidationContext context;
  if (!const_cast<SemanticsValidator *>(this)->makeDefinitionValidationContext(def, context)) {
    return {};
  }
  return context;
}

SemanticsValidator::ValidationContext
SemanticsValidator::buildExecutionValidationContext(const Execution &exec) const {
  return makeExecutionValidationContext(exec);
}

bool SemanticsValidator::makeDefinitionValidationState(const Definition &def, ValidationState &out) {
  out = {};
  return makeDefinitionValidationContext(def, out.context);
}

SemanticsValidator::ValidationState
SemanticsValidator::makeExecutionValidationState(const Execution &exec) const {
  ValidationState state;
  state.context = makeExecutionValidationContext(exec);
  return state;
}

SemanticsValidator::ValidationState
SemanticsValidator::buildDefinitionValidationState(const Definition &def) const {
  ValidationState state;
  if (!const_cast<SemanticsValidator *>(this)->makeDefinitionValidationState(def, state)) {
    return {};
  }
  return state;
}

SemanticsValidator::ValidationState
SemanticsValidator::buildExecutionValidationState(const Execution &exec) const {
  return makeExecutionValidationState(exec);
}

} // namespace primec::semantics
