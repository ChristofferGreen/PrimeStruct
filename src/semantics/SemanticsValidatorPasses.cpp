#include "SemanticsValidator.h"

#include <array>

namespace primec::semantics {

namespace {

struct HelperSuffixInfo {
  std::string_view suffix;
  std::string_view placement;
};

bool isLifecycleHelperName(const std::string &fullPath) {
  static const std::array<HelperSuffixInfo, 8> suffixes = {{
      {"Create", ""},
      {"Destroy", ""},
      {"CreateStack", "stack"},
      {"DestroyStack", "stack"},
      {"CreateHeap", "heap"},
      {"DestroyHeap", "heap"},
      {"CreateBuffer", "buffer"},
      {"DestroyBuffer", "buffer"},
  }};
  for (const auto &info : suffixes) {
    const std::string_view suffix = info.suffix;
    if (fullPath.size() < suffix.size() + 1) {
      continue;
    }
    const size_t suffixStart = fullPath.size() - suffix.size();
    if (fullPath[suffixStart - 1] != '/') {
      continue;
    }
    if (fullPath.compare(suffixStart, suffix.size(), suffix.data(), suffix.size()) != 0) {
      continue;
    }
    return true;
  }
  return false;
}

} // namespace

std::unordered_set<std::string> SemanticsValidator::resolveEffects(const std::vector<Transform> &transforms) const {
  bool sawEffects = false;
  std::unordered_set<std::string> effects;
  for (const auto &transform : transforms) {
    if (transform.name != "effects") {
      continue;
    }
    sawEffects = true;
    effects.clear();
    for (const auto &arg : transform.arguments) {
      effects.insert(arg);
    }
  }
  if (!sawEffects) {
    effects = defaultEffectSet_;
  }
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

bool SemanticsValidator::validateDefinitions() {
  for (const auto &def : program_.definitions) {
    activeEffects_ = resolveEffects(def.transforms);
    if (!validateCapabilitiesSubset(def.transforms, def.fullPath)) {
      return false;
    }
    std::unordered_map<std::string, BindingInfo> locals;
    const auto &defParams = paramsByDef_[def.fullPath];
    for (const auto &param : defParams) {
      locals.emplace(param.name, param.binding);
    }
    for (const auto &param : defParams) {
      if (param.defaultExpr == nullptr) {
        continue;
      }
      if (!validateExpr(defParams, locals, *param.defaultExpr)) {
        return false;
      }
    }
    ReturnKind kind = ReturnKind::Unknown;
    auto kindIt = returnKinds_.find(def.fullPath);
    if (kindIt != returnKinds_.end()) {
      kind = kindIt->second;
    }
    if (isLifecycleHelperName(def.fullPath) && kind != ReturnKind::Void) {
      error_ = "lifecycle helpers must return void: " + def.fullPath;
      return false;
    }
    bool sawReturn = false;
    for (const auto &stmt : def.statements) {
      if (!validateStatement(defParams, locals, stmt, kind, true, true, &sawReturn, def.namespacePrefix)) {
        return false;
      }
    }
    if (kind != ReturnKind::Void) {
      bool allPathsReturn = blockAlwaysReturns(def.statements);
      if (!allPathsReturn) {
        if (sawReturn) {
          error_ = "not all control paths return in " + def.fullPath;
        } else {
          error_ = "missing return statement in " + def.fullPath;
        }
        return false;
      }
    }
  }
  return true;
}

bool SemanticsValidator::validateExecutions() {
  for (const auto &exec : program_.executions) {
    activeEffects_ = resolveEffects(exec.transforms);
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : exec.transforms) {
      if (transform.name == "return") {
        error_ = "return transform not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "mut") {
        error_ = "mut transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
        error_ = "placement transforms are not supported: " + exec.fullPath;
        return false;
      }
      if (transform.name == "effects") {
        if (sawEffects) {
          error_ = "duplicate effects transform on " + exec.fullPath;
          return false;
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, exec.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          error_ = "duplicate capabilities transform on " + exec.fullPath;
          return false;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, exec.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        error_ = "alignment transforms are not supported on executions: " + exec.fullPath;
        return false;
      } else if (isStructTransformName(transform.name)) {
        error_ = "struct transforms are not allowed on executions: " + exec.fullPath;
        return false;
      }
    }
    if (!validateCapabilitiesSubset(exec.transforms, exec.fullPath)) {
      return false;
    }
    Expr execTarget;
    if (!exec.name.empty()) {
      execTarget.name = exec.name;
      execTarget.namespacePrefix = exec.namespacePrefix;
    } else {
      execTarget.name = exec.fullPath;
    }
    const std::string resolvedPath = resolveCalleePath(execTarget);
    auto it = defMap_.find(resolvedPath);
    if (it == defMap_.end()) {
      error_ = "unknown execution target: " + resolvedPath;
      return false;
    }
    if (!validateNamedArguments(exec.arguments, exec.argumentNames, resolvedPath, error_)) {
      return false;
    }
    const auto &execParams = paramsByDef_[resolvedPath];
    if (!validateNamedArgumentsAgainstParams(execParams, exec.argumentNames, error_)) {
      return false;
    }
    std::vector<const Expr *> orderedExecArgs;
    std::string orderError;
    if (!buildOrderedArguments(execParams, exec.arguments, exec.argumentNames, orderedExecArgs, orderError)) {
      if (orderError.find("argument count mismatch") != std::string::npos) {
        error_ = "argument count mismatch for " + resolvedPath;
      } else {
        error_ = orderError;
      }
      return false;
    }
    for (const auto *arg : orderedExecArgs) {
      if (!arg) {
        continue;
      }
      if (!validateExpr({}, std::unordered_map<std::string, BindingInfo>{}, *arg)) {
        return false;
      }
    }
    std::unordered_map<std::string, BindingInfo> execLocals;
    for (const auto &arg : exec.bodyArguments) {
      if (arg.kind != Expr::Kind::Call) {
        error_ = "execution body arguments must be calls";
        return false;
      }
      if (arg.isBinding) {
        error_ = "execution body arguments cannot be bindings";
        return false;
      }
      if (!validateStatement({}, execLocals, arg, ReturnKind::Unknown, false, false, nullptr, exec.namespacePrefix)) {
        return false;
      }
    }
  }
  return true;
}

bool SemanticsValidator::validateEntry() {
  auto entryIt = defMap_.find(entryPath_);
  if (entryIt == defMap_.end()) {
    error_ = "missing entry definition " + entryPath_;
    return false;
  }
  const auto &entryParams = paramsByDef_[entryPath_];
  if (!entryParams.empty()) {
    if (entryParams.size() != 1) {
      error_ = "entry definition must take a single array<string> parameter: " + entryPath_;
      return false;
    }
    const ParameterInfo &param = entryParams.front();
    if (param.binding.typeName != "array" || param.binding.typeTemplateArg != "string") {
      error_ = "entry definition must take a single array<string> parameter: " + entryPath_;
      return false;
    }
    if (param.defaultExpr != nullptr) {
      error_ = "entry parameter does not allow a default value: " + entryPath_;
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
