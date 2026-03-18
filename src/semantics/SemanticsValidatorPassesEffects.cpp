#include "SemanticsValidator.h"

namespace primec::semantics {

namespace {

void expandEffectImplications(std::unordered_set<std::string> &effects) {
  if (effects.count("file_write") != 0) {
    effects.insert("file_read");
  }
}

} // namespace

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
    if (activeEffects_.count(capability) == 0) {
      error_ = "capability requires matching effect on " + context + ": " + capability;
      return false;
    }
  }
  return true;
}

bool SemanticsValidator::resolveExecutionEffects(const Expr &expr, std::unordered_set<std::string> &effectsOut) {
  effectsOut = activeEffects_;
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
        error_ = "duplicate effects transform on " + context;
        return false;
      }
      sawEffects = true;
      if (!validateEffectsTransform(transform, context, error_)) {
        return false;
      }
      effectsOut.clear();
      for (const auto &arg : transform.arguments) {
        effectsOut.insert(arg);
      }
      expandEffectImplications(effectsOut);
    } else if (transform.name == "capabilities") {
      if (sawCapabilities) {
        error_ = "duplicate capabilities transform on " + context;
        return false;
      }
      sawCapabilities = true;
      if (!validateCapabilitiesTransform(transform, context, error_)) {
        return false;
      }
      capabilities.clear();
      for (const auto &arg : transform.arguments) {
        capabilities.insert(arg);
      }
    } else if (transform.name == "return") {
      error_ = "return transform not allowed on executions: " + context;
      return false;
    } else if (transform.name == "on_error") {
      error_ = "on_error transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "mut") {
      error_ = "mut transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "unsafe") {
      error_ = "unsafe transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "copy") {
      error_ = "copy transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "restrict") {
      error_ = "restrict transform is not allowed on executions: " + context;
      return false;
    } else if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
      error_ = "placement transforms are not supported: " + context;
      return false;
    } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
      error_ = "alignment transforms are not supported on executions: " + context;
      return false;
    } else if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
      error_ = "layout transforms are not supported on executions: " + context;
      return false;
    } else if (isBindingQualifierName(transform.name)) {
      error_ = "binding visibility/static transforms are only valid on bindings: " + context;
      return false;
    } else if (isReflectionTransformName(transform.name)) {
      error_ = "reflection transforms are only valid on struct definitions: " + context;
      return false;
    } else if (isStructTransformName(transform.name)) {
      error_ = "struct transforms are not allowed on executions: " + context;
      return false;
    }
  }
  if (sawEffects) {
    for (const auto &effect : effectsOut) {
      if (activeEffects_.count(effect) == 0) {
        error_ = "execution effects must be a subset of enclosing effects on " + context + ": " + effect;
        return false;
      }
    }
  }
  if (sawCapabilities) {
    for (const auto &capability : capabilities) {
      if (effectsOut.count(capability) == 0) {
        error_ = "capability requires matching effect on " + context + ": " + capability;
        return false;
      }
    }
  }
  return true;
}

} // namespace primec::semantics
