#include "SemanticsValidator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <limits>

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

std::unordered_set<std::string> SemanticsValidator::resolveEffects(const std::vector<Transform> &transforms,
                                                                    bool isEntry) const {
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
    effects = isEntry ? entryDefaultEffectSet_ : defaultEffectSet_;
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

bool SemanticsValidator::validateDefinitions() {
  for (const auto &def : program_.definitions) {
    currentDefinitionPath_ = def.fullPath;
    activeEffects_ = resolveEffects(def.transforms, def.fullPath == entryPath_);
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
  currentDefinitionPath_.clear();
  return true;
}

bool SemanticsValidator::validateExecutions() {
  currentDefinitionPath_.clear();
  for (const auto &exec : program_.executions) {
    activeEffects_ = resolveEffects(exec.transforms, false);
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
      if (transform.name == "no_padding" || transform.name == "platform_independent_padding") {
        error_ = "layout transforms are not supported on executions: " + exec.fullPath;
        return false;
      }
      if (isBindingQualifierName(transform.name)) {
        error_ = "binding visibility/static transforms are only valid on bindings: " + exec.fullPath;
        return false;
      }
      if (transform.name == "copy") {
        error_ = "copy transform is not allowed on executions: " + exec.fullPath;
        return false;
      }
      if (transform.name == "restrict") {
        error_ = "restrict transform is not allowed on executions: " + exec.fullPath;
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
      if (!validateStatement({}, execLocals, arg, ReturnKind::Unknown, false, true, nullptr, exec.namespacePrefix)) {
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

bool SemanticsValidator::validateStructLayouts() {
  struct LayoutInfo {
    uint32_t sizeBytes = 0;
    uint32_t alignmentBytes = 1;
  };
  auto isStructDefinition = [&](const Definition &def) {
    bool hasStruct = false;
    bool hasReturn = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturn = true;
      }
      if (isStructTransformName(transform.name)) {
        hasStruct = true;
      }
    }
    if (hasStruct) {
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        return false;
      }
    }
    return true;
  };
  auto alignTo = [](uint32_t value, uint32_t alignment) -> uint32_t {
    if (alignment == 0) {
      return value;
    }
    uint32_t remainder = value % alignment;
    if (remainder == 0) {
      return value;
    }
    return value + (alignment - remainder);
  };
  auto extractAlignment = [&](const std::vector<Transform> &transforms,
                              const std::string &context,
                              uint32_t &alignmentOut,
                              bool &hasAlignment) -> bool {
    alignmentOut = 1;
    hasAlignment = false;
    for (const auto &transform : transforms) {
      if (transform.name != "align_bytes" && transform.name != "align_kbytes") {
        continue;
      }
      if (hasAlignment) {
        error_ = "duplicate " + transform.name + " transform on " + context;
        return false;
      }
      if (!validateAlignTransform(transform, context, error_)) {
        return false;
      }
      auto parsePositiveInt = [](const std::string &text, int &valueOut) -> bool {
        std::string digits = text;
        if (digits.size() > 3 && digits.compare(digits.size() - 3, 3, "i32") == 0) {
          digits.resize(digits.size() - 3);
        }
        if (digits.empty()) {
          return false;
        }
        int parsed = 0;
        for (char c : digits) {
          if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
          }
          int digit = c - '0';
          if (parsed > (std::numeric_limits<int>::max() - digit) / 10) {
            return false;
          }
          parsed = parsed * 10 + digit;
        }
        if (parsed <= 0) {
          return false;
        }
        valueOut = parsed;
        return true;
      };
      int value = 0;
      if (!parsePositiveInt(transform.arguments[0], value)) {
        error_ = transform.name + " requires a positive integer argument";
        return false;
      }
      uint64_t bytes = static_cast<uint64_t>(value);
      if (transform.name == "align_kbytes") {
        bytes *= 1024ull;
      }
      if (bytes > std::numeric_limits<uint32_t>::max()) {
        error_ = transform.name + " alignment too large on " + context;
        return false;
      }
      alignmentOut = static_cast<uint32_t>(bytes);
      hasAlignment = true;
    }
    return true;
  };
  auto resolveStructTypePath = [&](const std::string &typeName, const std::string &namespacePrefix) -> std::string {
    if (!typeName.empty() && typeName[0] == '/') {
      return typeName;
    }
    std::string resolved = resolveTypePath(typeName, namespacePrefix);
    if (structNames_.count(resolved) > 0) {
      return resolved;
    }
    auto importIt = importAliases_.find(typeName);
    if (importIt != importAliases_.end()) {
      return importIt->second;
    }
    return resolved;
  };

  std::unordered_map<std::string, LayoutInfo> layoutCache;
  std::unordered_set<std::string> layoutStack;

  std::function<bool(const Definition &, LayoutInfo &)> computeStructLayout;
  std::function<bool(const BindingInfo &, const std::string &, LayoutInfo &)> typeLayoutForBinding;

  computeStructLayout = [&](const Definition &def, LayoutInfo &out) -> bool {
    auto cached = layoutCache.find(def.fullPath);
    if (cached != layoutCache.end()) {
      out = cached->second;
      return true;
    }
    if (!layoutStack.insert(def.fullPath).second) {
      error_ = "recursive struct layout not supported: " + def.fullPath;
      return false;
    }
    bool requireNoPadding = false;
    bool requirePlatformPadding = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "no_padding") {
        requireNoPadding = true;
      } else if (transform.name == "platform_independent_padding") {
        requirePlatformPadding = true;
      }
    }
    uint32_t structAlign = 1;
    uint32_t explicitStructAlign = 1;
    bool hasStructAlign = false;
    if (!extractAlignment(def.transforms, "struct " + def.fullPath, explicitStructAlign, hasStructAlign)) {
      return false;
    }
    uint32_t offset = 0;
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        continue;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      LayoutInfo fieldLayout;
      if (!typeLayoutForBinding(binding, def.namespacePrefix, fieldLayout)) {
        return false;
      }
      uint32_t explicitFieldAlign = 1;
      bool hasFieldAlign = false;
      const std::string fieldContext = "field " + def.fullPath + "/" + stmt.name;
      if (!extractAlignment(stmt.transforms, fieldContext, explicitFieldAlign, hasFieldAlign)) {
        return false;
      }
      if (hasFieldAlign && explicitFieldAlign < fieldLayout.alignmentBytes) {
        error_ = "alignment requirement on " + fieldContext + " is smaller than required alignment of " +
                 std::to_string(fieldLayout.alignmentBytes);
        return false;
      }
      uint32_t fieldAlign = hasFieldAlign ? std::max(explicitFieldAlign, fieldLayout.alignmentBytes)
                                          : fieldLayout.alignmentBytes;
      uint32_t alignedOffset = alignTo(offset, fieldAlign);
      if (requireNoPadding && alignedOffset != offset) {
        error_ = "no_padding disallows alignment padding on " + fieldContext;
        return false;
      }
      if (requirePlatformPadding && alignedOffset != offset && !hasFieldAlign) {
        error_ = "platform_independent_padding requires explicit alignment on " + fieldContext;
        return false;
      }
      offset = alignedOffset + fieldLayout.sizeBytes;
      structAlign = std::max(structAlign, fieldAlign);
    }
    if (hasStructAlign && explicitStructAlign < structAlign) {
      error_ = "alignment requirement on struct " + def.fullPath + " is smaller than required alignment of " +
               std::to_string(structAlign);
      return false;
    }
    structAlign = hasStructAlign ? std::max(structAlign, explicitStructAlign) : structAlign;
    uint32_t totalSize = alignTo(offset, structAlign);
    if (requireNoPadding && totalSize != offset) {
      error_ = "no_padding disallows trailing padding on struct " + def.fullPath;
      return false;
    }
    if (requirePlatformPadding && totalSize != offset && !hasStructAlign) {
      error_ = "platform_independent_padding requires explicit struct alignment on " + def.fullPath;
      return false;
    }
    LayoutInfo layout{totalSize, structAlign};
    layoutCache.emplace(def.fullPath, layout);
    layoutStack.erase(def.fullPath);
    out = layout;
    return true;
  };

  typeLayoutForBinding = [&](const BindingInfo &binding,
                             const std::string &namespacePrefix,
                             LayoutInfo &layoutOut) -> bool {
    std::string normalized = normalizeBindingTypeName(binding.typeName);
    if (normalized == "i32" || normalized == "int" || normalized == "f32") {
      layoutOut = {4u, 4u};
      return true;
    }
    if (normalized == "i64" || normalized == "u64" || normalized == "f64") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (normalized == "bool") {
      layoutOut = {1u, 1u};
      return true;
    }
    if (normalized == "string") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "Pointer" || binding.typeName == "Reference") {
      layoutOut = {8u, 8u};
      return true;
    }
    if (binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "map") {
      layoutOut = {8u, 8u};
      return true;
    }
    std::string structPath = resolveStructTypePath(binding.typeName, namespacePrefix);
    auto defIt = defMap_.find(structPath);
    if (defIt == defMap_.end()) {
      error_ = "unknown struct type for layout: " + binding.typeName;
      return false;
    }
    LayoutInfo nested;
    if (!computeStructLayout(*defIt->second, nested)) {
      return false;
    }
    layoutOut = nested;
    return true;
  };

  for (const auto &def : program_.definitions) {
    if (!isStructDefinition(def)) {
      continue;
    }
    LayoutInfo layout;
    if (!computeStructLayout(def, layout)) {
      return false;
    }
  }
  return true;
}

} // namespace primec::semantics
