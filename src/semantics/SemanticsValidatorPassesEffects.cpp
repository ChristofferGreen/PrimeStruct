#include "SemanticsValidator.h"

namespace primec::semantics {

namespace {

void expandEffectImplications(std::unordered_set<std::string> &effects) {
  if (effects.count("file_write") != 0) {
    effects.insert("file_read");
  }
}

} // namespace

bool SemanticsValidator::publishPassesEffectsDiagnostic(const Expr *expr) {
  if (expr != nullptr) {
    captureExprContext(*expr);
  } else if (currentExecutionContext_ != nullptr) {
    captureExecutionContext(*currentExecutionContext_);
  } else if (currentDefinitionContext_ != nullptr) {
    captureDefinitionContext(*currentDefinitionContext_);
  }
  return publishCurrentStructuredDiagnosticNow();
}

std::unordered_set<std::string> SemanticsValidator::resolveEffects(const std::vector<Transform> &transforms,
                                                                   bool isEntry) const {
  bool sawEffects = false;
  std::unordered_set<std::string> effects;
  for (const auto &transform : transforms) {
    if (transform.name != "effects") {
      continue;
    }
    sawEffects = true;
    for (const auto &arg : transform.arguments) {
      effects.insert(arg);
    }
  }
  if (!sawEffects) {
    effects = isEntry ? entryDefaultEffectSet_ : defaultEffectSet_;
  }
  expandEffectImplications(effects);
  return effects;
}

bool SemanticsValidator::validateCapabilitiesSubset(const std::vector<Transform> &transforms,
                                                    const std::string &context) {
  auto failPassesEffectsDiagnostic = [&](std::string message) -> bool {
    error_ = std::move(message);
    return publishPassesEffectsDiagnostic();
  };
  bool sawCapabilities = false;
  std::unordered_set<std::string> capabilities;
  for (const auto &transform : transforms) {
    if (transform.name != "capabilities") {
      continue;
    }
    sawCapabilities = true;
    capabilities.clear();
    for (const auto &arg : transform.arguments) {
      capabilities.insert(arg);
    }
  }
  if (!sawCapabilities) {
    return true;
  }
  for (const auto &capability : capabilities) {
    if (currentValidationContext_.activeEffects.count(capability) == 0) {
      return failPassesEffectsDiagnostic("capability requires matching effect on " + context + ": " + capability);
    }
  }
  return true;
}

bool SemanticsValidator::resolveExecutionEffects(const Expr &expr, std::unordered_set<std::string> &effectsOut) {
  auto failPassesEffectsDiagnostic = [&](std::string message) -> bool {
    error_ = std::move(message);
    return publishPassesEffectsDiagnostic(&expr);
  };
  effectsOut = currentValidationContext_.activeEffects;
  bool sawEffects = false;
  bool sawCapabilities = false;
  std::unordered_set<std::string> capabilities;
  std::string context = resolveCalleePath(expr);
  if (context.empty()) {
    context = expr.name.empty() ? "<execution>" : expr.name;
  }
  for (const auto &transform : expr.transforms) {
    if (transform.name == "effects") {
      if (sawEffects) {
        return failPassesEffectsDiagnostic("duplicate effects transform on " + context);
      }
      sawEffects = true;
      if (!validateEffectsTransform(transform, context, error_)) {
        return publishPassesEffectsDiagnostic(&expr);
      }
      effectsOut.clear();
      for (const auto &arg : transform.arguments) {
        effectsOut.insert(arg);
      }
      expandEffectImplications(effectsOut);
    } else if (transform.name == "capabilities") {
      if (sawCapabilities) {
        return failPassesEffectsDiagnostic("duplicate capabilities transform on " + context);
      }
      sawCapabilities = true;
      if (!validateCapabilitiesTransform(transform, context, error_)) {
        return publishPassesEffectsDiagnostic(&expr);
      }
      capabilities.clear();
      for (const auto &arg : transform.arguments) {
        capabilities.insert(arg);
      }
    } else if (transform.name == "return") {
      return failPassesEffectsDiagnostic("return transform not allowed on executions: " + context);
    } else if (transform.name == "on_error") {
      return failPassesEffectsDiagnostic("on_error transform is not allowed on executions: " + context);
    } else if (transform.name == "mut") {
      return failPassesEffectsDiagnostic("mut transform is not allowed on executions: " + context);
    } else if (transform.name == "unsafe") {
      return failPassesEffectsDiagnostic("unsafe transform is not allowed on executions: " + context);
    } else if (transform.name == "copy") {
      return failPassesEffectsDiagnostic("copy transform is not allowed on executions: " + context);
    } else if (transform.name == "restrict") {
      return failPassesEffectsDiagnostic("restrict transform is not allowed on executions: " + context);
    } else if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
      return failPassesEffectsDiagnostic("placement transforms are not supported: " + context);
    } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
      return failPassesEffectsDiagnostic("alignment transforms are not supported on executions: " + context);
    } else if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
      return failPassesEffectsDiagnostic("layout transforms are not supported on executions: " + context);
    } else if (isBindingQualifierName(transform.name)) {
      return failPassesEffectsDiagnostic("binding visibility/static transforms are only valid on bindings: " + context);
    } else if (isReflectionTransformName(transform.name)) {
      return failPassesEffectsDiagnostic("reflection transforms are only valid on struct definitions: " + context);
    } else if (isStructTransformName(transform.name)) {
      return failPassesEffectsDiagnostic("struct transforms are not allowed on executions: " + context);
    }
  }
  if (sawEffects) {
    for (const auto &effect : effectsOut) {
      if (currentValidationContext_.activeEffects.count(effect) == 0) {
        return failPassesEffectsDiagnostic("execution effects must be a subset of enclosing effects on " + context +
                                           ": " + effect);
      }
    }
  }
  if (sawCapabilities) {
    for (const auto &capability : capabilities) {
      if (effectsOut.count(capability) == 0) {
        return failPassesEffectsDiagnostic("capability requires matching effect on " + context + ": " + capability);
      }
    }
  }
  return true;
}

} // namespace primec::semantics
