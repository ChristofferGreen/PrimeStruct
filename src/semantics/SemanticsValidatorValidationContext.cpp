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

const SemanticsValidator::ValidationContext &
SemanticsValidator::buildDefinitionValidationContext(const Definition &def) const {
  auto it = definitionValidationContexts_.find(def.fullPath);
  if (it != definitionValidationContexts_.end()) {
    return it->second;
  }
  static const ValidationContext EmptyContext;
  return EmptyContext;
}

const SemanticsValidator::ValidationContext &
SemanticsValidator::buildExecutionValidationContext(const Execution &exec) const {
  auto it = executionValidationContexts_.find(exec.fullPath);
  if (it != executionValidationContexts_.end()) {
    return it->second;
  }
  static const ValidationContext EmptyContext;
  return EmptyContext;
}

} // namespace primec::semantics
