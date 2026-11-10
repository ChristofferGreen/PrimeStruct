#include "SemanticsValidator.h"
#include "SemanticsValidateLambdaClosures.h"

#include <algorithm>
#include <cctype>

namespace primec::semantics {

namespace {

// `/keep__tead9077f04525e0f` -> `/keep`: the template a monomorphized definition was specialized from.
std::string stripSpecializationSuffix(const std::string &path) {
  const size_t lastSlash = path.find_last_of('/');
  const size_t nameStart = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  const size_t suffix = path.find("__t", nameStart);
  if (suffix == std::string::npos || suffix + 3 == path.size()) {
    return path;
  }
  for (size_t i = suffix + 3; i < path.size(); ++i) {
    if (std::isxdigit(static_cast<unsigned char>(path[i])) == 0) {
      return path;
    }
  }
  return path.substr(0, suffix);
}

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
    if (transform.name != "effects" || !transform.templateArgs.empty()) {
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
    if (currentExecutionContext_ != nullptr) {
      return failExecutionDiagnostic(*currentExecutionContext_, std::move(message));
    }
    if (currentDefinitionContext_ != nullptr) {
      return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));
    }
    return failUncontextualizedDiagnostic(std::move(message));
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
    if (currentValidationState_.context.activeEffects.count(capability) == 0) {
      return failPassesEffectsDiagnostic("capability requires matching effect on " + context + ": " + capability);
    }
  }
  return true;
}

bool SemanticsValidator::validateCalleeEffects(const Expr &expr, const Definition &callee) {
  // docs/spec/language-core.md, Callee effects: every effect the callee declares must be active in the caller. A
  // callee without `[effects]` places no requirement (its default set is not propagated, TODO-5688), and a lambda's
  // closure `call` was already checked where the lambda is written, so calling it needs no effects.
  if (callee.fullPath.find(LambdaClosureStructPrefix) != std::string::npos) {
    return true;
  }
  // A `Function<R, A...>` value's `call` (`/__FunctionN<...>/call`) only dispatches to the closures converted to it,
  // each checked where it was written, so calling it needs no effects either. Its generated `[effects]` are the union
  // of those closures' and would otherwise tie the caller (a stdlib Router) to whatever its handlers' creators hold.
  if (callee.fullPath.rfind("/__Function", 0) == 0 && callee.name == "call") {
    return true;
  }
  std::vector<std::string> required;
  for (const auto &transform : callee.transforms) {
    if (transform.name == "effects" && transform.templateArgs.empty()) {
      required.insert(required.end(), transform.arguments.begin(), transform.arguments.end());
    }
  }
  if (required.empty()) {
    return true;
  }
  std::sort(required.begin(), required.end());
  const auto &active = currentValidationState_.context.activeEffects;
  for (const auto &effect : required) {
    if (active.count(effect) != 0) {
      continue;
    }
    std::string caller = currentValidationState_.context.definitionPath;
    std::string callerKind;
    if (caller.empty()) {
      caller = currentExecutionContext_ != nullptr ? currentExecutionContext_->fullPath : std::string("<execution>");
      callerKind = "execution ";
    } else if (const size_t lambdaPos = caller.find(LambdaClosureStructPrefix); lambdaPos != std::string::npos) {
      caller = stripSpecializationSuffix(caller.substr(0, lambdaPos));
      callerKind = "a lambda in ";
    } else {
      caller = stripSpecializationSuffix(caller);
    }
    if (effect == "atomic") {
      // The operations of /std/sync/Atomic (docs/spec/stdlib-reference.md, std/sync).
      return failExprDiagnostic(
          expr,
          "atomic operation requires atomic effect: " + stripSpecializationSuffix(callee.fullPath) +
              " in " + callerKind + caller);
    }
    return failExprDiagnostic(expr, stripSpecializationSuffix(callee.fullPath) + " requires " + effect +
                                        " effect in " + callerKind + caller);
  }
  return true;
}

bool SemanticsValidator::resolveExecutionEffects(const Expr &expr, std::unordered_set<std::string> &effectsOut) {
  auto failPassesEffectsDiagnostic = [&](std::string message) -> bool {
    return failExprDiagnostic(expr, std::move(message));
  };
  effectsOut = currentValidationState_.context.activeEffects;
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
        return failExprDiagnostic(expr, error_);
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
        return failExprDiagnostic(expr, error_);
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
    } else if (transform.name == "unsafe" || transform.name == "unsafe_api") {
      return failPassesEffectsDiagnostic(transform.name +
                                         " transform is not allowed on executions: " + context);
    } else if (transform.name == "copy") {
      return failPassesEffectsDiagnostic("copy transform is not allowed on executions: " + context);
    } else if (transform.name == "move") {
      return failPassesEffectsDiagnostic("move transform is not allowed on executions: " + context);
    } else if (transform.name == "restrict") {
      return failPassesEffectsDiagnostic("restrict transform is not allowed on executions: " + context);
    } else if (transform.name == "stack" || transform.name == "heap" ||
               transform.name == "buffer") {
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
      if (currentValidationState_.context.activeEffects.count(effect) == 0) {
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
