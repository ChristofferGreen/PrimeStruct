#include "SemanticsValidator.h"

#include <array>
#include <string_view>
#include <utility>

namespace primec::semantics {

bool SemanticsValidator::buildDefinitionMaps() {
  defaultEffectSet_.clear();
  entryDefaultEffectSet_.clear();
  defMap_.clear();
  returnKinds_.clear();
  structNames_.clear();
  paramsByDef_.clear();

  auto isMathBuiltinName = [&](const std::string &name) -> bool {
    Expr probe;
    probe.name = name;
    std::string builtinName;
    return getBuiltinMathName(probe, builtinName, true) || getBuiltinClampName(probe, builtinName, true) ||
           getBuiltinMinMaxName(probe, builtinName, true) || getBuiltinAbsSignName(probe, builtinName, true) ||
           getBuiltinSaturateName(probe, builtinName, true) || isBuiltinMathConstant(name, true);
  };

  for (const auto &effect : defaultEffects_) {
    if (!isEffectName(effect)) {
      error_ = "invalid default effect: " + effect;
      return false;
    }
    defaultEffectSet_.insert(effect);
  }
  for (const auto &effect : entryDefaultEffects_) {
    if (!isEffectName(effect)) {
      error_ = "invalid entry default effect: " + effect;
      return false;
    }
    entryDefaultEffectSet_.insert(effect);
  }

  for (const auto &def : program_.definitions) {
    if (defMap_.count(def.fullPath) > 0) {
      error_ = "duplicate definition: " + def.fullPath;
      return false;
    }
    if (def.fullPath.find('/', 1) == std::string::npos) {
      const std::string rootName = def.fullPath.substr(1);
      if ((mathImportAll_ || mathImports_.count(rootName) > 0) && isMathBuiltinName(rootName)) {
        error_ = "import creates name conflict: " + rootName;
        return false;
      }
    }
    bool sawEffects = false;
    bool sawCapabilities = false;
    for (const auto &transform : def.transforms) {
      if (isBindingQualifierName(transform.name)) {
        error_ = "binding visibility/static transforms are only valid on bindings: " + def.fullPath;
        return false;
      }
      if (transform.name == "copy") {
        error_ = "copy transform is only supported on bindings and parameters: " + def.fullPath;
        return false;
      }
      if (transform.name == "restrict") {
        error_ = "restrict transform is only supported on bindings and parameters: " + def.fullPath;
        return false;
      }
      if (transform.name == "return") {
        if (!transform.arguments.empty()) {
          error_ = "return transform does not accept arguments on " + def.fullPath;
          return false;
        }
      } else if (transform.name == "stack" || transform.name == "heap" || transform.name == "buffer") {
        error_ = "placement transforms are not supported: " + def.fullPath;
        return false;
      } else if (transform.name == "effects") {
        if (sawEffects) {
          error_ = "duplicate effects transform on " + def.fullPath;
          return false;
        }
        sawEffects = true;
        if (!validateEffectsTransform(transform, def.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "capabilities") {
        if (sawCapabilities) {
          error_ = "duplicate capabilities transform on " + def.fullPath;
          return false;
        }
        sawCapabilities = true;
        if (!validateCapabilitiesTransform(transform, def.fullPath, error_)) {
          return false;
        }
      } else if (transform.name == "align_bytes" || transform.name == "align_kbytes") {
        if (!validateAlignTransform(transform, def.fullPath, error_)) {
          return false;
        }
      } else if (isStructTransformName(transform.name)) {
        if (!transform.templateArgs.empty()) {
          error_ = "struct transform does not accept template arguments on " + def.fullPath;
          return false;
        }
        if (!transform.arguments.empty()) {
          error_ = "struct transform does not accept arguments on " + def.fullPath;
          return false;
        }
      }
    }
    bool isStruct = false;
    bool hasReturnTransform = false;
    bool hasPod = false;
    bool hasHandle = false;
    bool hasGpuLane = false;
    for (const auto &transform : def.transforms) {
      if (transform.name == "return") {
        hasReturnTransform = true;
      }
      if (transform.name == "pod") {
        hasPod = true;
      } else if (transform.name == "handle") {
        hasHandle = true;
      } else if (transform.name == "gpu_lane") {
        hasGpuLane = true;
      }
      if (isStructTransformName(transform.name)) {
        isStruct = true;
      }
    }
    if (hasPod && (hasHandle || hasGpuLane)) {
      error_ = "pod definitions cannot be tagged as handle or gpu_lane: " + def.fullPath;
      return false;
    }
    if (hasHandle && hasGpuLane) {
      error_ = "handle definitions cannot be tagged as gpu_lane: " + def.fullPath;
      return false;
    }
    bool isFieldOnlyStruct = false;
    if (!isStruct && !hasReturnTransform && def.parameters.empty() && !def.hasReturnStatement &&
        !def.returnExpr.has_value()) {
      isFieldOnlyStruct = true;
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          isFieldOnlyStruct = false;
          break;
        }
      }
    }
    if (isStruct || isFieldOnlyStruct) {
      structNames_.insert(def.fullPath);
    }
    if (isStruct) {
      if (hasReturnTransform) {
        error_ = "struct definitions cannot declare return types: " + def.fullPath;
        return false;
      }
      if (def.hasReturnStatement) {
        error_ = "struct definitions cannot contain return statements: " + def.fullPath;
        return false;
      }
      if (def.returnExpr.has_value()) {
        error_ = "struct definitions cannot return a value: " + def.fullPath;
        return false;
      }
      if (!def.parameters.empty()) {
        error_ = "struct definitions cannot declare parameters: " + def.fullPath;
        return false;
      }
      for (const auto &stmt : def.statements) {
        if (!stmt.isBinding) {
          error_ = "struct definitions may only contain field bindings: " + def.fullPath;
          return false;
        }
      }
    }
    if (isStruct || isFieldOnlyStruct) {
      for (const auto &stmt : def.statements) {
        if (stmt.isBinding && stmt.args.empty()) {
          error_ = "struct definitions require field initializers: " + def.fullPath;
          return false;
        }
        if (stmt.isBinding) {
          bool fieldHasHandle = false;
          bool fieldHasGpuLane = false;
          bool fieldHasPod = false;
          for (const auto &transform : stmt.transforms) {
            if (transform.name == "handle") {
              fieldHasHandle = true;
            } else if (transform.name == "pod") {
              fieldHasPod = true;
            } else if (transform.name == "gpu_lane") {
              fieldHasGpuLane = true;
            }
          }
          if (hasPod && (fieldHasHandle || fieldHasGpuLane)) {
            error_ = "pod definitions cannot contain handle or gpu_lane fields: " + def.fullPath;
            return false;
          }
          if (fieldHasPod && fieldHasHandle) {
            error_ = "fields cannot be tagged as pod and handle: " + def.fullPath;
            return false;
          }
          if (fieldHasPod && fieldHasGpuLane) {
            error_ = "fields cannot be tagged as pod and gpu_lane: " + def.fullPath;
            return false;
          }
          if (fieldHasHandle && fieldHasGpuLane) {
            error_ = "fields cannot be tagged as handle and gpu_lane: " + def.fullPath;
            return false;
          }
        }
      }
    }
    ReturnKind kind = ReturnKind::Void;
    if (!isStruct) {
      kind = getReturnKind(def, error_);
      if (!error_.empty()) {
        return false;
      }
    }
    returnKinds_[def.fullPath] = kind;
    defMap_[def.fullPath] = &def;
  }

  importAliases_.clear();
  for (const auto &importPath : program_.imports) {
    if (importPath == "/math") {
      error_ = "import /math is not supported; use import /math/* or /math/<name>";
      return false;
    }
    if (importPath.empty() || importPath[0] != '/') {
      error_ = "import path must be a slash path";
      return false;
    }
    bool isWildcard = false;
    std::string prefix;
    if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
      isWildcard = true;
      prefix = importPath.substr(0, importPath.size() - 2);
    } else if (importPath.find('/', 1) == std::string::npos) {
      isWildcard = true;
      prefix = importPath;
    }
    if (isWildcard) {
      const std::string scopedPrefix = prefix + "/";
      bool sawImmediateDefinition = false;
      for (const auto &def : program_.definitions) {
        if (def.fullPath.rfind(scopedPrefix, 0) != 0) {
          continue;
        }
        const std::string remainder = def.fullPath.substr(scopedPrefix.size());
        if (remainder.empty() || remainder.find('/') != std::string::npos) {
          continue;
        }
        sawImmediateDefinition = true;
        if (isRootBuiltinName(remainder)) {
          error_ = "import creates name conflict: " + remainder;
          return false;
        }
        if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
          error_ = "import creates name conflict: " + remainder;
          return false;
        }
        const std::string rootPath = "/" + remainder;
        if (defMap_.count(rootPath) > 0) {
          error_ = "import creates name conflict: " + remainder;
          return false;
        }
        auto [it, inserted] = importAliases_.emplace(remainder, def.fullPath);
        if (!inserted && it->second != def.fullPath) {
          error_ = "import creates name conflict: " + remainder;
          return false;
        }
      }
      if (!sawImmediateDefinition && prefix != "/math") {
        error_ = "unknown import path: " + importPath;
        return false;
      }
      continue;
    }
    bool isMathBuiltinImport = false;
    if (importPath.rfind("/math/", 0) == 0 && importPath.size() > 6) {
      std::string name = importPath.substr(6);
      if (name.find('/') == std::string::npos && name != "*" && isMathBuiltinName(name)) {
        isMathBuiltinImport = true;
      }
    }
    auto defIt = defMap_.find(importPath);
    if (defIt == defMap_.end()) {
      if (!isMathBuiltinImport) {
        error_ = "unknown import path: " + importPath;
        return false;
      }
      continue;
    }
    const std::string remainder = importPath.substr(importPath.find_last_of('/') + 1);
    if (remainder.empty()) {
      continue;
    }
    if (isRootBuiltinName(remainder)) {
      error_ = "import creates name conflict: " + remainder;
      return false;
    }
    if (allowMathBareName(remainder) && isMathBuiltinName(remainder)) {
      error_ = "import creates name conflict: " + remainder;
      return false;
    }
    const std::string rootPath = "/" + remainder;
    if (defMap_.count(rootPath) > 0) {
      error_ = "import creates name conflict: " + remainder;
      return false;
    }
    auto [it, inserted] = importAliases_.emplace(remainder, importPath);
    if (!inserted && it->second != importPath) {
      error_ = "import creates name conflict: " + remainder;
      return false;
    }
  }

  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };

  auto isLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, std::string &placementOut) -> bool {
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
      parentOut = fullPath.substr(0, suffixStart - 1);
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };
  auto isStructTaggedDefinition = [&](const std::string &path) -> bool {
    auto it = defMap_.find(path);
    if (it == defMap_.end()) {
      return false;
    }
    for (const auto &transform : it->second->transforms) {
      if (isStructTransformName(transform.name)) {
        return true;
      }
    }
    return false;
  };

  for (const auto &def : program_.definitions) {
    std::string parentPath;
    std::string placement;
    if (!isLifecycleHelper(def.fullPath, parentPath, placement)) {
      continue;
    }
    if (parentPath.empty() || !isStructTaggedDefinition(parentPath)) {
      error_ = "lifecycle helper must be nested inside a struct: " + def.fullPath;
      return false;
    }
    if (!def.parameters.empty()) {
      error_ = "lifecycle helpers do not accept parameters: " + def.fullPath;
      return false;
    }
  }

  return buildParameters();
}

bool SemanticsValidator::buildParameters() {
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };
  auto isLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, std::string &placementOut) -> bool {
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
      parentOut = fullPath.substr(0, suffixStart - 1);
      placementOut = std::string(info.placement);
      return true;
    }
    return false;
  };

  for (const auto &def : program_.definitions) {
    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> params;
    params.reserve(def.parameters.size());
    for (const auto &param : def.parameters) {
      if (!param.isBinding) {
        error_ = "parameters must use binding syntax: " + def.fullPath;
        return false;
      }
      if (param.hasBodyArguments || !param.bodyArguments.empty()) {
        error_ = "parameter does not accept block arguments: " + param.name;
        return false;
      }
      if (!seen.insert(param.name).second) {
        error_ = "duplicate parameter: " + param.name;
        return false;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(param, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, error_)) {
        return false;
      }
      if (param.args.size() > 1) {
        error_ = "parameter defaults accept at most one argument: " + param.name;
        return false;
      }
      if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front())) {
        if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
          error_ = "parameter default does not accept named arguments: " + param.name;
        } else {
          error_ = "parameter default must be a literal or pure expression: " + param.name;
        }
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
      }
      ParameterInfo info;
      info.name = param.name;
      info.binding = std::move(binding);
      if (param.args.size() == 1) {
        info.defaultExpr = &param.args.front();
      }
      if (restrictType.has_value()) {
        const bool hasTemplate = !info.binding.typeTemplateArg.empty();
        if (!restrictMatchesBinding(*restrictType,
                                    info.binding.typeName,
                                    info.binding.typeTemplateArg,
                                    hasTemplate,
                                    def.namespacePrefix)) {
          error_ = "restrict type does not match binding type";
          return false;
        }
      }
      params.push_back(std::move(info));
    }
    std::string parentPath;
    std::string placement;
    if (isLifecycleHelper(def.fullPath, parentPath, placement)) {
      bool sawMut = false;
      for (const auto &transform : def.transforms) {
        if (transform.name != "mut") {
          continue;
        }
        if (sawMut) {
          error_ = "duplicate mut transform on " + def.fullPath;
          return false;
        }
        sawMut = true;
        if (!transform.templateArgs.empty()) {
          error_ = "mut transform does not accept template arguments on " + def.fullPath;
          return false;
        }
        if (!transform.arguments.empty()) {
          error_ = "mut transform does not accept arguments on " + def.fullPath;
          return false;
        }
      }
      if (!seen.insert("this").second) {
        error_ = "duplicate parameter: this";
        return false;
      }
      ParameterInfo info;
      info.name = "this";
      info.binding.typeName = parentPath;
      info.binding.isMutable = sawMut;
      params.insert(params.begin(), std::move(info));
    } else {
      for (const auto &transform : def.transforms) {
        if (transform.name == "mut") {
          error_ = "mut transform is only supported on lifecycle helpers: " + def.fullPath;
          return false;
        }
      }
    }
    paramsByDef_[def.fullPath] = std::move(params);
  }
  return true;
}

std::string SemanticsValidator::resolveCalleePath(const Expr &expr) const {
  if (expr.name.empty()) {
    return "";
  }
  if (!expr.name.empty() && expr.name[0] == '/') {
    return expr.name;
  }
  if (expr.name.find('/') != std::string::npos) {
    return "/" + expr.name;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string scoped = expr.namespacePrefix + "/" + expr.name;
    if (defMap_.count(scoped) > 0) {
      return scoped;
    }
    auto it = importAliases_.find(expr.name);
    if (it != importAliases_.end()) {
      return it->second;
    }
    return scoped;
  }
  std::string root = "/" + expr.name;
  if (defMap_.count(root) > 0) {
    return root;
  }
  auto it = importAliases_.find(expr.name);
  if (it != importAliases_.end()) {
    return it->second;
  }
  return root;
}

bool SemanticsValidator::isParam(const std::vector<ParameterInfo> &params, const std::string &name) const {
  for (const auto &param : params) {
    if (param.name == name) {
      return true;
    }
  }
  return false;
}

const BindingInfo *SemanticsValidator::findParamBinding(const std::vector<ParameterInfo> &params,
                                                        const std::string &name) const {
  for (const auto &param : params) {
    if (param.name == name) {
      return &param.binding;
    }
  }
  return nullptr;
}

std::string SemanticsValidator::typeNameForReturnKind(ReturnKind kind) const {
  switch (kind) {
    case ReturnKind::Int:
      return "i32";
    case ReturnKind::Int64:
      return "i64";
    case ReturnKind::UInt64:
      return "u64";
    case ReturnKind::Bool:
      return "bool";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    default:
      return "";
  }
}

bool SemanticsValidator::inferBindingTypeFromInitializer(
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, bindingOut, hasAnyMathImport())) {
    return true;
  }
  ReturnKind kind = inferExprReturnKind(initializer, params, locals);
  if (kind == ReturnKind::Unknown || kind == ReturnKind::Void) {
    return false;
  }
  std::string inferred = typeNameForReturnKind(kind);
  if (inferred.empty()) {
    return false;
  }
  bindingOut.typeName = inferred;
  bindingOut.typeTemplateArg.clear();
  return true;
}

} // namespace primec::semantics
