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
  returnStructs_.clear();
  structNames_.clear();
  publicDefinitions_.clear();
  paramsByDef_.clear();
  definitionValidationContexts_.clear();
  executionValidationContexts_.clear();
  currentValidationContext_ = {};

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
  definitionValidationContexts_.reserve(program_.definitions.size());
  for (const auto &def : program_.definitions) {
    definitionValidationContexts_.try_emplace(def.fullPath, makeDefinitionValidationContext(def));
  }
  executionValidationContexts_.reserve(program_.executions.size());
  for (const auto &exec : program_.executions) {
    executionValidationContexts_.try_emplace(exec.fullPath, makeExecutionValidationContext(exec));
  }

  std::unordered_set<std::string> explicitStructs;
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };
  auto isStructDefinition = [&](const Definition &def, bool &isExplicitOut) {
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
      isExplicitOut = true;
      return true;
    }
    if (hasReturn || !def.parameters.empty() || def.hasReturnStatement || def.returnExpr.has_value()) {
      isExplicitOut = false;
      return false;
    }
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        isExplicitOut = false;
        return false;
      }
    }
    isExplicitOut = false;
    return true;
  };
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    bool isExplicit = false;
    if (isStructDefinition(def, isExplicit)) {
      structNames_.insert(def.fullPath);
      if (isExplicit) {
        explicitStructs.insert(def.fullPath);
      }
    }
  }

  auto isLifecycleHelperName = [](const std::string &fullPath) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
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
  };
  auto isStructHelperDefinition = [&](const Definition &def) -> bool {
    if (!def.isNested) {
      return false;
    }
    if (structNames_.count(def.fullPath) > 0) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string parent = def.fullPath.substr(0, slash);
    return structNames_.count(parent) > 0;
  };
  std::vector<SemanticDiagnosticRecord> transformDiagnosticRecords;
  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
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
    const bool isStructHelper = isStructHelperDefinition(def);
    const bool isLifecycleHelper = isLifecycleHelperName(def.fullPath);
    bool definitionTransformError = false;
    if (!validateDefinitionBuildTransforms(def,
                                           isStructHelper,
                                           isLifecycleHelper,
                                           explicitStructs,
                                           &transformDiagnosticRecords,
                                           definitionTransformError)) {
      return false;
    }
    if (definitionTransformError) {
      continue;
    }
    defMap_[def.fullPath] = &def;
  }
  if (collectDiagnostics_ && diagnosticInfo_ != nullptr && !transformDiagnosticRecords.empty()) {
    diagnosticSink_.setRecords(std::move(transformDiagnosticRecords));
    return false;
  }

  if (!buildImportAliases()) {
    return false;
  }

  if (!buildDefinitionReturnKinds(explicitStructs)) {
    return false;
  }

  if (!validateLifecycleHelperDefinitions()) {
    return false;
  }

  return buildParameters();
}

bool SemanticsValidator::buildParameters() {
  entryArgsName_.clear();
  struct HelperSuffixInfo {
    std::string_view suffix;
    std::string_view placement;
  };
  auto isLifecycleHelper =
      [](const std::string &fullPath, std::string &parentOut, std::string &placementOut) -> bool {
    static const std::array<HelperSuffixInfo, 10> suffixes = {{
        {"Create", ""},
        {"Destroy", ""},
        {"Copy", ""},
        {"Move", ""},
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
  auto isCopyHelperName = [](const std::string &fullPath) -> bool {
    static constexpr std::string_view copySuffix = "/Copy";
    static constexpr std::string_view moveSuffix = "/Move";
    if (fullPath.size() <= copySuffix.size()) {
      return false;
    }
    if (fullPath.compare(fullPath.size() - copySuffix.size(),
                         copySuffix.size(),
                         copySuffix.data(),
                         copySuffix.size()) == 0) {
      return true;
    }
    if (fullPath.size() <= moveSuffix.size()) {
      return false;
    }
    return fullPath.compare(fullPath.size() - moveSuffix.size(),
                            moveSuffix.size(),
                            moveSuffix.data(),
                            moveSuffix.size()) == 0;
  };
  auto isStructHelperDefinition = [&](const Definition &def, std::string &parentOut) -> bool {
    parentOut.clear();
    if (!def.isNested) {
      return false;
    }
    if (structNames_.count(def.fullPath) > 0) {
      return false;
    }
    const size_t slash = def.fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string parent = def.fullPath.substr(0, slash);
    if (structNames_.count(parent) == 0) {
      return false;
    }
    parentOut = parent;
    return true;
  };
  auto hasStaticTransform = [](const Definition &def) -> bool {
    for (const auto &transform : def.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };

  for (const auto &def : program_.definitions) {
    DefinitionContextScope definitionScope(*this, def);
    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> params;
    params.reserve(def.parameters.size());
    auto defaultResolvesToDefinition = [&](const Expr &candidate) -> bool {
      return candidate.kind == Expr::Kind::Call && defMap_.find(resolveCalleePath(candidate)) != defMap_.end();
    };
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
      if (binding.typeName == "uninitialized") {
        error_ = "uninitialized storage is not allowed on parameters: " + param.name;
        return false;
      }
      if (param.args.size() > 1) {
        error_ = "parameter defaults accept at most one argument: " + param.name;
        return false;
      }
      if (param.args.size() == 1 && !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition)) {
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
      if (!validateBuiltinMapKeyType(binding, &def.templateArgs, error_)) {
        return false;
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
    std::string helperParent;
    const bool isLifecycle = isLifecycleHelper(def.fullPath, parentPath, placement);
    if (!isLifecycle) {
      (void)isStructHelperDefinition(def, helperParent);
      if (!helperParent.empty()) {
        parentPath = helperParent;
      }
    }
    const bool isStructHelper = isLifecycle || !helperParent.empty();
    const bool isStaticHelper = isStructHelper && !isLifecycle && hasStaticTransform(def);
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
    if (sawMut && !isStructHelper) {
      error_ = "mut transform is only supported on struct helpers: " + def.fullPath;
      return false;
    }
    if (sawMut && isStaticHelper) {
      error_ = "mut transform is not allowed on static helpers: " + def.fullPath;
      return false;
    }

    if (isLifecycle) {
      if (isCopyHelperName(def.fullPath)) {
        if (params.size() != 1) {
          error_ = "Copy/Move helpers require exactly one parameter: " + def.fullPath;
          return false;
        }
        const auto &copyParam = params.front();
        if (copyParam.binding.typeName != "Reference" || copyParam.binding.typeTemplateArg != parentPath) {
          error_ = "Copy/Move helpers require [Reference<Self>] parameter: " + def.fullPath;
          return false;
        }
      }
    }
    if (isStructHelper && !isStaticHelper) {
      if (!seen.insert("this").second) {
        error_ = "duplicate parameter: this";
        return false;
      }
      ParameterInfo info;
      info.name = "this";
      info.binding.typeName = "Reference";
      info.binding.typeTemplateArg = parentPath;
      info.binding.isMutable = sawMut;
      params.insert(params.begin(), std::move(info));
    }
    if (def.fullPath == entryPath_) {
      if (params.size() == 1 && params.front().binding.typeName == "array" &&
          params.front().binding.typeTemplateArg == "string") {
        entryArgsName_ = params.front().name;
      }
    }
    paramsByDef_[def.fullPath] = std::move(params);
  }
  return true;
}

std::string SemanticsValidator::resolveCalleePath(const Expr &expr) const {
  auto rewriteCanonicalCollectionHelperPath = [&](const std::string &resolvedPath) -> std::string {
    auto canonicalMapHelperAliasPath = [&](std::string_view helperName) -> std::string {
      if (helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
          helperName == "at" || helperName == "at_unsafe") {
        return "/map/" + std::string(helperName);
      }
      return {};
    };
    auto rewriteCanonicalHelper = [&](std::string_view prefix,
                                      const auto &aliasPathBuilder,
                                      bool requirePositionalBuiltinAlias) -> std::string {
      if (resolvedPath.rfind(prefix, 0) != 0) {
        return resolvedPath;
      }
      const std::string helperName = resolvedPath.substr(prefix.size());
      const std::string aliasPath = aliasPathBuilder(helperName);
      if (aliasPath.empty()) {
        return resolvedPath;
      }
      if (defMap_.count(resolvedPath) == 0 && defMap_.count(aliasPath) > 0) {
        return aliasPath;
      }
      if (requirePositionalBuiltinAlias && !hasNamedArguments(expr.argNames) &&
          defMap_.count(resolvedPath) > 0) {
        return aliasPath;
      }
      return resolvedPath;
    };
    return rewriteCanonicalHelper("/std/collections/map/", canonicalMapHelperAliasPath, false);
  };
  auto rewriteCanonicalCollectionConstructorPath = [&](const std::string &resolvedPath) -> std::string {
    if (expr.isMethodCall) {
      return rewriteCanonicalCollectionHelperPath(resolvedPath);
    }
    auto vectorConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/vectorNew";
      case 1:
        return "/std/collections/vectorSingle";
      case 2:
        return "/std/collections/vectorPair";
      case 3:
        return "/std/collections/vectorTriple";
      case 4:
        return "/std/collections/vectorQuad";
      case 5:
        return "/std/collections/vectorQuint";
      case 6:
        return "/std/collections/vectorSext";
      case 7:
        return "/std/collections/vectorSept";
      case 8:
        return "/std/collections/vectorOct";
      default:
        return {};
      }
    };
    auto mapConstructorHelperPath = [&]() -> std::string {
      switch (expr.args.size()) {
      case 0:
        return "/std/collections/mapNew";
      case 2:
        return "/std/collections/mapSingle";
      case 4:
        return "/std/collections/mapPair";
      case 6:
        return "/std/collections/mapTriple";
      case 8:
        return "/std/collections/mapQuad";
      case 10:
        return "/std/collections/mapQuint";
      case 12:
        return "/std/collections/mapSext";
      case 14:
        return "/std/collections/mapSept";
      case 16:
        return "/std/collections/mapOct";
      default:
        return {};
      }
    };
    std::string helperPath;
    if (resolvedPath == "/std/collections/vector/vector") {
      helperPath = vectorConstructorHelperPath();
    } else if (resolvedPath == "/std/collections/map/map") {
      helperPath = mapConstructorHelperPath();
    } else if (resolvedPath == "/vector/vector" &&
               defMap_.count("/std/collections/vector/vector") > 0) {
      return "/std/collections/vector/vector";
    } else if (resolvedPath == "/map/map" &&
               defMap_.count("/std/collections/map/map") > 0) {
      return "/std/collections/map/map";
    }
    if (!helperPath.empty() && defMap_.count(helperPath) > 0) {
      return helperPath;
    }
    return rewriteCanonicalCollectionHelperPath(resolvedPath);
  };
  if (expr.name.empty()) {
    return "";
  }
  if (!expr.name.empty() && expr.name[0] == '/') {
    return rewriteCanonicalCollectionConstructorPath(expr.name);
  }
  if (expr.name.find('/') != std::string::npos) {
    return rewriteCanonicalCollectionConstructorPath("/" + expr.name);
  }
  std::string pointerBuiltinName;
  if (getBuiltinPointerName(expr, pointerBuiltinName)) {
    return "/" + pointerBuiltinName;
  }
  if (!expr.namespacePrefix.empty()) {
    std::string normalizedPrefix = expr.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() != '/') {
      normalizedPrefix.insert(normalizedPrefix.begin(), '/');
    }
    auto isRemovedVectorCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "capacity" || helperName == "at" ||
             helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
             helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
             helperName == "remove_swap";
    };
    auto isRemovedMapCompatibilityHelper = [](std::string_view helperName) {
      return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
    };
    const size_t lastSlash = normalizedPrefix.find_last_of('/');
    const std::string_view suffix = lastSlash == std::string::npos
                                        ? std::string_view(normalizedPrefix)
                                        : std::string_view(normalizedPrefix).substr(lastSlash + 1);
    if (suffix == expr.name && defMap_.count(normalizedPrefix) > 0) {
      return normalizedPrefix;
    }
    std::string prefix = normalizedPrefix;
    while (!prefix.empty()) {
      std::string candidate = prefix + "/" + expr.name;
      if (defMap_.count(candidate) > 0) {
        return candidate;
      }
      const size_t slash = prefix.find_last_of('/');
      if (slash == std::string::npos) {
        break;
      }
      prefix = prefix.substr(0, slash);
    }
    if (normalizedPrefix.rfind("/std/collections/vector", 0) == 0 &&
        isRemovedVectorCompatibilityHelper(expr.name)) {
      return normalizedPrefix + "/" + expr.name;
    }
    if (normalizedPrefix.rfind("/std/collections/map", 0) == 0 &&
        isRemovedMapCompatibilityHelper(expr.name)) {
      return normalizedPrefix + "/" + expr.name;
    }
    auto it = importAliases_.find(expr.name);
    if (it != importAliases_.end()) {
      return rewriteCanonicalCollectionConstructorPath(it->second);
    }
    return rewriteCanonicalCollectionConstructorPath(normalizedPrefix + "/" + expr.name);
  }
  std::string root = "/" + expr.name;
  if (defMap_.count(root) > 0) {
    return rewriteCanonicalCollectionConstructorPath(root);
  }
  auto it = importAliases_.find(expr.name);
  if (it != importAliases_.end()) {
    return rewriteCanonicalCollectionConstructorPath(it->second);
  }
  return rewriteCanonicalCollectionConstructorPath(root);
}

bool SemanticsValidator::resolveStructFieldBinding(const Definition &structDef,
                                                   const Expr &fieldStmt,
                                                   BindingInfo &bindingOut) {
  std::optional<std::string> restrictType;
  std::string parseError;
  if (!parseBindingInfo(fieldStmt, structDef.namespacePrefix, structNames_, importAliases_, bindingOut, restrictType,
                        parseError)) {
    error_ = parseError;
    return false;
  }
  if (hasExplicitBindingTypeTransform(fieldStmt)) {
    if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
      return false;
    }
    return true;
  }
  if (lookupGraphLocalAutoBinding(structDef.fullPath, fieldStmt, bindingOut)) {
    if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
      return false;
    }
    return true;
  }
  const std::string fieldPath = structDef.fullPath + "/" + fieldStmt.name;
  if (fieldStmt.args.size() != 1) {
    error_ = "omitted struct field envelope requires exactly one initializer: " + fieldPath;
    return false;
  }
  const std::vector<ParameterInfo> noParams;
  const std::unordered_map<std::string, BindingInfo> noLocals;
  BindingInfo inferred = bindingOut;
  if (inferBindingTypeFromInitializer(fieldStmt.args.front(), noParams, noLocals, inferred)) {
    if (!(inferred.typeName == "array" && inferred.typeTemplateArg.empty())) {
      bindingOut = std::move(inferred);
      if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return false;
      }
      return true;
    }
  }
  if (inferExprReturnKind(fieldStmt.args.front(), noParams, noLocals) == ReturnKind::Array) {
    std::string structPath = inferStructReturnPath(fieldStmt.args.front(), noParams, noLocals);
    if (!structPath.empty()) {
      bindingOut.typeName = std::move(structPath);
      bindingOut.typeTemplateArg.clear();
      if (!validateBuiltinMapKeyType(bindingOut, &structDef.templateArgs, error_)) {
        return false;
      }
      return true;
    }
  }
  error_ = "unresolved or ambiguous omitted struct field envelope: " + fieldPath;
  return false;
}

bool SemanticsValidator::validateSoaVectorElementFieldEnvelopes(const std::string &typeArg,
                                                                const std::string &namespacePrefix) {
  auto resolveStructPath = [&](const std::string &candidate, const std::string &scope) -> std::string {
    const std::string normalized = normalizeBindingTypeName(candidate);
    std::string resolved = resolveStructTypePath(normalized, scope, structNames_);
    if (!resolved.empty()) {
      return resolved;
    }
    auto importIt = importAliases_.find(normalized);
    if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
      return importIt->second;
    }
    return {};
  };
  std::string elementStructPath = resolveStructPath(typeArg, namespacePrefix);
  if (elementStructPath.empty()) {
    return true;
  }
  auto structIt = defMap_.find(elementStructPath);
  if (structIt == defMap_.end()) {
    return true;
  }
  auto hasStaticTransform = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  std::unordered_set<std::string> activeStructs;
  std::function<bool(const Definition &, const std::string &)> validateStructFields;
  validateStructFields = [&](const Definition &structDef, const std::string &pathPrefix) -> bool {
    if (!activeStructs.insert(structDef.fullPath).second) {
      return true;
    }
    for (const auto &fieldStmt : structDef.statements) {
      if (!fieldStmt.isBinding || hasStaticTransform(fieldStmt)) {
        continue;
      }
      BindingInfo fieldBinding;
      if (!resolveStructFieldBinding(structDef, fieldStmt, fieldBinding)) {
        activeStructs.erase(structDef.fullPath);
        return false;
      }
      const std::string fieldPath = pathPrefix + "/" + fieldStmt.name;
      const std::string normalizedFieldType = normalizeBindingTypeName(fieldBinding.typeName);
      if (normalizedFieldType == "string" || fieldBinding.typeName == "Pointer" ||
          fieldBinding.typeName == "Reference" || !fieldBinding.typeTemplateArg.empty()) {
        std::string fieldType = fieldBinding.typeName;
        if (!fieldBinding.typeTemplateArg.empty()) {
          fieldType += "<" + fieldBinding.typeTemplateArg + ">";
        }
        error_ = "soa_vector field envelope is unsupported on " + fieldPath + ": " + fieldType;
        activeStructs.erase(structDef.fullPath);
        return false;
      }
      if (fieldBinding.typeTemplateArg.empty() && !isPrimitiveBindingTypeName(normalizedFieldType)) {
        const std::string nestedStructPath = resolveStructPath(fieldBinding.typeName, structDef.namespacePrefix);
        if (!nestedStructPath.empty()) {
          auto nestedIt = defMap_.find(nestedStructPath);
          if (nestedIt != defMap_.end()) {
            if (!validateStructFields(*nestedIt->second, fieldPath)) {
              activeStructs.erase(structDef.fullPath);
              return false;
            }
          }
        }
      }
    }
    activeStructs.erase(structDef.fullPath);
    return true;
  };
  return validateStructFields(*structIt->second, structIt->second->fullPath);
}

bool SemanticsValidator::resolveUninitializedStorageBinding(const std::vector<ParameterInfo> &params,
                                                            const std::unordered_map<std::string, BindingInfo> &locals,
                                                            const Expr &storage,
                                                            BindingInfo &bindingOut,
                                                            bool &resolvedOut) {
  resolvedOut = false;
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
  };
  std::function<bool(const Expr &, BindingInfo &)> resolvePointerBinding;
  resolvePointerBinding = [&](const Expr &pointerExpr, BindingInfo &pointerBinding) -> bool {
    if (pointerExpr.kind == Expr::Kind::Name) {
      if (const BindingInfo *binding = findBinding(params, locals, pointerExpr.name)) {
        pointerBinding = *binding;
        return true;
      }
      return false;
    }
    if (pointerExpr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string accessBuiltin;
    if (getBuiltinArrayAccessName(pointerExpr, accessBuiltin) && pointerExpr.args.size() == 2 &&
        pointerExpr.args.front().kind == Expr::Kind::Name) {
      if (const BindingInfo *binding = findBinding(params, locals, pointerExpr.args.front().name)) {
        std::string elementType;
        if (getArgsPackElementType(*binding, elementType)) {
          BindingInfo elementBinding;
          std::string base;
          std::string argText;
          const std::string normalizedType = normalizeBindingTypeName(elementType);
          if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
            elementBinding.typeName = normalizeBindingTypeName(base);
            elementBinding.typeTemplateArg = argText;
          } else {
            elementBinding.typeName = normalizedType;
            elementBinding.typeTemplateArg.clear();
          }
          pointerBinding = elementBinding;
          return true;
        }
      }
      return false;
    }
    std::string pointerBuiltin;
    if (getBuiltinPointerName(pointerExpr, pointerBuiltin) && pointerBuiltin == "location" && pointerExpr.args.size() == 1) {
      BindingInfo pointeeBinding;
      bool pointeeResolved = false;
      if (!resolveUninitializedStorageBinding(params, locals, pointerExpr.args.front(), pointeeBinding, pointeeResolved)) {
        return false;
      }
      if (!pointeeResolved) {
        return false;
      }
      pointerBinding.typeName = "Reference";
      pointerBinding.typeTemplateArg = bindingTypeText(pointeeBinding);
      return true;
    }
    if (inferBindingTypeFromInitializer(pointerExpr, params, locals, pointerBinding)) {
      return true;
    }
    auto defIt = defMap_.find(resolveCalleePath(pointerExpr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText) || argText.empty()) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      if (base != "Reference" && base != "Pointer") {
        return false;
      }
      pointerBinding.typeName = base;
      pointerBinding.typeTemplateArg = argText;
      return true;
    }
    return false;
  };
  if (storage.kind == Expr::Kind::Name) {
    if (const BindingInfo *binding = findBinding(params, locals, storage.name)) {
      bindingOut = *binding;
      resolvedOut = true;
    }
    return true;
  }
  if (storage.kind == Expr::Kind::Call) {
    std::string pointerBuiltin;
    if (getBuiltinPointerName(storage, pointerBuiltin) && pointerBuiltin == "dereference" && storage.args.size() == 1) {
      const Expr &pointerExpr = storage.args.front();
      BindingInfo pointerBinding;
      if (resolvePointerBinding(pointerExpr, pointerBinding)) {
        const std::string normalizedPointerType = normalizeBindingTypeName(pointerBinding.typeName);
        if ((normalizedPointerType == "Reference" || normalizedPointerType == "Pointer") &&
            !pointerBinding.typeTemplateArg.empty()) {
          assignBindingTypeFromText(pointerBinding.typeTemplateArg);
          resolvedOut = true;
          return true;
        }
      }
      return true;
    }
  }
  if (!storage.isFieldAccess || storage.args.size() != 1) {
    return true;
  }
  auto resolveStructReceiverPath = [&](const Expr &receiver, std::string &structPathOut) -> bool {
    structPathOut.clear();
    auto assignStructPathFromType = [&](std::string receiverType, const std::string &namespacePrefix) {
      receiverType = normalizeBindingTypeName(receiverType);
      if (receiverType.empty()) {
        return false;
      }
      structPathOut = resolveStructTypePath(receiverType, namespacePrefix, structNames_);
      if (structPathOut.empty()) {
        auto importIt = importAliases_.find(receiverType);
        if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
          structPathOut = importIt->second;
        }
      }
      return !structPathOut.empty();
    };
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *receiverBinding = findBinding(params, locals, receiver.name);
      if (!receiverBinding) {
        return false;
      }
      std::string receiverType = receiverBinding->typeName;
      if ((receiverType == "Reference" || receiverType == "Pointer") && !receiverBinding->typeTemplateArg.empty()) {
        receiverType = receiverBinding->typeTemplateArg;
      }
      return assignStructPathFromType(receiverType, receiver.namespacePrefix);
    }
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding) {
      return false;
    }
    std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
    if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
      structPathOut = inferredStruct;
      return true;
    }
    const std::string resolvedType = resolveCalleePath(receiver);
    if (structNames_.count(resolvedType) > 0) {
      structPathOut = resolvedType;
      return true;
    }
    return false;
  };
  const Expr &receiver = storage.args.front();
  std::string structPath;
  if (!resolveStructReceiverPath(receiver, structPath)) {
    return true;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    return true;
  }
  auto isStaticField = [](const Expr &stmt) -> bool {
    for (const auto &transform : stmt.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  for (const auto &stmt : defIt->second->statements) {
    if (!stmt.isBinding || isStaticField(stmt)) {
      continue;
    }
    if (stmt.name != storage.name) {
      continue;
    }
    BindingInfo fieldBinding;
    if (!resolveStructFieldBinding(*defIt->second, stmt, fieldBinding)) {
      return false;
    }
    bindingOut = std::move(fieldBinding);
    resolvedOut = true;
    return true;
  }
  return true;
}

bool SemanticsValidator::isBuiltinBlockCall(const Expr &expr) const {
  if (!isBlockCall(expr)) {
    return false;
  }
  return defMap_.count(resolveCalleePath(expr)) == 0;
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
    case ReturnKind::String:
      return "string";
    case ReturnKind::Float32:
      return "f32";
    case ReturnKind::Float64:
      return "f64";
    case ReturnKind::Integer:
      return "integer";
    case ReturnKind::Decimal:
      return "decimal";
    case ReturnKind::Complex:
      return "complex";
    case ReturnKind::Array:
      return "array";
    default:
      return "";
  }
}

bool SemanticsValidator::lookupGraphLocalAutoBinding(const std::string &scopePath,
                                                     const Expr &bindingExpr,
                                                     BindingInfo &bindingOut) const {
  if (!graphTypeResolverEnabled_ || scopePath.empty()) {
    return false;
  }
  const auto [sourceLine, sourceColumn] = graphLocalAutoSourceLocation(bindingExpr);
  const auto it = graphLocalAutoBindings_.find(graphLocalAutoBindingKey(
      scopePath, sourceLine, sourceColumn));
  if (it == graphLocalAutoBindings_.end()) {
    return false;
  }
  bindingOut = it->second;
  return true;
}

bool SemanticsValidator::lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const {
  return lookupGraphLocalAutoBinding(currentValidationContext_.definitionPath, bindingExpr, bindingOut);
}

bool SemanticsValidator::inferResolvedDirectCallBindingType(const std::string &resolvedPath, BindingInfo &bindingOut) const {
  auto inferDeclaredCollectionBinding = [&](const Definition &definition) -> bool {
    auto isSupportedCollectionType = [&](const std::string &typeName) -> bool {
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(typeName), base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      return ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) ||
             (isMapCollectionTypeName(base) && args.size() == 2);
    };

    for (const auto &transform : definition.transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1 && isSupportedCollectionType(args.front())) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = args.front();
        return true;
      }
      return false;
    }
    return false;
  };

  const auto defIt = defMap_.find(resolvedPath);
  const auto directStructIt = returnStructs_.find(resolvedPath);
  if (directStructIt != returnStructs_.end() && !directStructIt->second.empty()) {
    if (directStructIt->second.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
      bindingOut.typeName = directStructIt->second;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (!normalizeCollectionTypePath(directStructIt->second).empty()) {
      if (defIt != defMap_.end() && defIt->second != nullptr && inferDeclaredCollectionBinding(*defIt->second)) {
        return true;
      }
      return false;
    }
    bindingOut.typeName = directStructIt->second;
    bindingOut.typeTemplateArg.clear();
    return true;
  }

  const auto kindIt = returnKinds_.find(resolvedPath);
  if (kindIt == returnKinds_.end()) {
    return false;
  }
  if (kindIt->second == ReturnKind::Unknown || kindIt->second == ReturnKind::Void) {
    return false;
  }
  if (kindIt->second == ReturnKind::Array) {
    if (defIt != defMap_.end() && defIt->second != nullptr && inferDeclaredCollectionBinding(*defIt->second)) {
      return true;
    }
    const auto structIt = returnStructs_.find(resolvedPath);
    if (structIt == returnStructs_.end() || structIt->second.empty()) {
      return false;
    }
    bindingOut.typeName = structIt->second;
    bindingOut.typeTemplateArg.clear();
    return true;
  }

  const std::string inferredType = typeNameForReturnKind(kindIt->second);
  if (inferredType.empty()) {
    return false;
  }
  bindingOut.typeName = inferredType;
  bindingOut.typeTemplateArg.clear();
  return true;
}

bool SemanticsValidator::inferBindingTypeFromInitializer(
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut,
    const Expr *bindingExpr) {
  auto bindingTypeText = [](const BindingInfo &binding) {
    if (binding.typeTemplateArg.empty()) {
      return binding.typeName;
    }
    return binding.typeName + "<" + binding.typeTemplateArg + ">";
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto assignBindingTypeFromResultInfo = [&](const ResultTypeInfo &resultInfo) -> bool {
    if (!resultInfo.isResult || resultInfo.errorType.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    if (!resultInfo.hasValue) {
      bindingOut.typeTemplateArg = resultInfo.errorType;
      return true;
    }
    if (resultInfo.valueType.empty()) {
      return false;
    }
    bindingOut.typeTemplateArg = resultInfo.valueType + ", " + resultInfo.errorType;
    return true;
  };
  auto canonicalizeInferredCollectionBinding = [&](const Expr *sourceExpr) -> bool {
    auto isResolvedMapConstructorPath = [](std::string resolvedPath) {
      const size_t specializationSuffix = resolvedPath.find("__t");
      if (specializationSuffix != std::string::npos) {
        resolvedPath.erase(specializationSuffix);
      }
      return resolvedPath == "/std/collections/map/map" ||
             resolvedPath == "/std/collections/mapNew" ||
             resolvedPath == "/std/collections/mapSingle" ||
             resolvedPath == "/std/collections/mapPair" ||
             resolvedPath == "/std/collections/mapDouble" ||
             resolvedPath == "/std/collections/mapTriple" ||
             resolvedPath == "/std/collections/mapQuad" ||
             resolvedPath == "/std/collections/mapQuint" ||
             resolvedPath == "/std/collections/mapSext" ||
             resolvedPath == "/std/collections/mapSept" ||
             resolvedPath == "/std/collections/mapOct" ||
             resolvedPath == "/std/collections/experimental_map/mapNew" ||
             resolvedPath == "/std/collections/experimental_map/mapSingle" ||
             resolvedPath == "/std/collections/experimental_map/mapPair" ||
             resolvedPath == "/std/collections/experimental_map/mapDouble" ||
             resolvedPath == "/std/collections/experimental_map/mapTriple" ||
             resolvedPath == "/std/collections/experimental_map/mapQuad" ||
             resolvedPath == "/std/collections/experimental_map/mapQuint" ||
             resolvedPath == "/std/collections/experimental_map/mapSext" ||
             resolvedPath == "/std/collections/experimental_map/mapSept" ||
             resolvedPath == "/std/collections/experimental_map/mapOct";
    };
    auto inferDirectMapConstructorBinding = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Call || !isResolvedMapConstructorPath(resolveCalleePath(candidate))) {
        return false;
      }
      if (candidate.templateArgs.size() == 2) {
        bindingOut.typeName = "Map";
        bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
        return true;
      }
      if (candidate.args.size() < 2 || candidate.args.size() % 2 != 0) {
        return false;
      }
      auto inferArgumentTypeText = [&](const Expr &argument, std::string &typeTextOut) -> bool {
        typeTextOut.clear();
        if (inferQueryExprTypeText(argument, params, locals, typeTextOut) && !typeTextOut.empty()) {
          typeTextOut = normalizeBindingTypeName(typeTextOut);
          return true;
        }
        const ReturnKind argumentKind = inferExprReturnKind(argument, params, locals);
        if (argumentKind == ReturnKind::Unknown || argumentKind == ReturnKind::Void) {
          return false;
        }
        typeTextOut = typeNameForReturnKind(argumentKind);
        return !typeTextOut.empty();
      };
      std::string keyTypeText;
      std::string valueTypeText;
      if (!inferArgumentTypeText(candidate.args[0], keyTypeText) ||
          !inferArgumentTypeText(candidate.args[1], valueTypeText)) {
        return false;
      }
      bindingOut.typeName = "Map";
      bindingOut.typeTemplateArg = keyTypeText + ", " + valueTypeText;
      return true;
    };
    if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call &&
        inferDirectMapConstructorBinding(*sourceExpr)) {
      return true;
    }
    const std::string normalizedBindingType = normalizeBindingTypeName(bindingOut.typeName);
    if ((normalizedBindingType == "Map" && !bindingOut.typeTemplateArg.empty()) ||
        normalizedBindingType.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
        normalizedBindingType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
      return true;
    }
    const std::string normalizedCollectionType = normalizeCollectionTypePath(bindingTypeText(bindingOut));
    if (normalizedCollectionType.empty()) {
      return false;
    }
    if (sourceExpr != nullptr) {
      std::string inferredTypeText;
      if (inferQueryExprTypeText(*sourceExpr, params, locals, inferredTypeText) &&
          assignBindingTypeFromText(inferredTypeText)) {
        return true;
      }
    }
    bindingOut.typeName = normalizedCollectionType.substr(1);
    bindingOut.typeTemplateArg.clear();
    if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call) {
      std::vector<std::string> collectionArgs;
      if (resolveCallCollectionTemplateArgs(*sourceExpr,
                                           bindingOut.typeName,
                                           params,
                                           locals,
                                           collectionArgs) &&
          !collectionArgs.empty()) {
        bindingOut.typeTemplateArg = joinTemplateArgs(collectionArgs);
        return true;
      }
      std::string collectionName;
      if (getBuiltinCollectionName(*sourceExpr, collectionName)) {
        if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
            sourceExpr->templateArgs.size() == 1) {
          bindingOut.typeName = collectionName;
          bindingOut.typeTemplateArg = sourceExpr->templateArgs.front();
          return true;
        }
        if (collectionName == "map" && sourceExpr->templateArgs.size() == 2) {
          bindingOut.typeName = "map";
          bindingOut.typeTemplateArg = joinTemplateArgs(sourceExpr->templateArgs);
          return true;
        }
      }
    }
    return true;
  };
  auto graphBindingIsUsable = [&](const BindingInfo &binding) {
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType.empty() || normalizedType == "auto") {
      return false;
    }
    if (normalizedType == "array" && binding.typeTemplateArg.empty()) {
      return false;
    }
    if (!binding.typeTemplateArg.empty()) {
      return true;
    }
    if (normalizedType == "Result" || normalizedType == "Pointer" || normalizedType == "Reference") {
      return true;
    }
    if (returnKindForTypeName(normalizedType) != ReturnKind::Unknown) {
      return true;
    }
    if (structNames_.count(binding.typeName) > 0) {
      return true;
    }
    std::string scopeNamespace;
    const auto scopeIt = defMap_.find(currentValidationContext_.definitionPath);
    if (scopeIt != defMap_.end() && scopeIt->second != nullptr) {
      scopeNamespace = scopeIt->second->namespacePrefix;
    }
    if (!resolveStructTypePath(normalizedType, scopeNamespace, structNames_).empty()) {
      return true;
    }
    auto importIt = importAliases_.find(normalizedType);
    return importIt != importAliases_.end() && structNames_.count(importIt->second) > 0;
  };
  auto shouldBypassGraphBindingLookup = [&](const Expr &candidate) {
    if (candidate.args.size() != 1 || candidate.args.front().kind != Expr::Kind::Call) {
      return false;
    }
    const Expr &initializerCall = candidate.args.front();
    std::string collectionName;
    if (getBuiltinCollectionName(initializerCall, collectionName)) {
      return true;
    }
    std::string normalizedName = initializerCall.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    std::string normalizedPrefix = initializerCall.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    if (normalizedName == "vector/vector" ||
        (normalizedPrefix == "vector" && normalizedName == "vector")) {
      return true;
    }
    if ((normalizedPrefix == "std/collections/map" &&
         (normalizedName == "count" || normalizedName == "contains" ||
          normalizedName == "tryAt" || normalizedName == "at" ||
          normalizedName == "at_unsafe")) ||
        normalizedName == "std/collections/map/count" ||
        normalizedName == "std/collections/map/contains" ||
        normalizedName == "std/collections/map/tryAt" ||
        normalizedName == "std/collections/map/at" ||
        normalizedName == "std/collections/map/at_unsafe") {
      return true;
    }
    if ((normalizedPrefix == "std/collections/vector" &&
         (normalizedName == "at" || normalizedName == "at_unsafe" ||
          normalizedName == "push" || normalizedName == "pop" ||
          normalizedName == "reserve" || normalizedName == "clear" ||
          normalizedName == "remove_at" || normalizedName == "remove_swap")) ||
        normalizedName == "std/collections/vector/at" ||
        normalizedName == "std/collections/vector/at_unsafe" ||
        normalizedName == "std/collections/vector/push" ||
        normalizedName == "std/collections/vector/pop" ||
        normalizedName == "std/collections/vector/reserve" ||
        normalizedName == "std/collections/vector/clear" ||
        normalizedName == "std/collections/vector/remove_at" ||
        normalizedName == "std/collections/vector/remove_swap") {
      return true;
    }
    return false;
  };
  if (bindingExpr != nullptr && !shouldBypassGraphBindingLookup(*bindingExpr) &&
      lookupGraphLocalAutoBinding(*bindingExpr, bindingOut)) {
    if (graphBindingIsUsable(bindingOut)) {
      return true;
    }
    bindingOut = {};
  }
  auto inferTryInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || !isSimpleCallName(initializer, "try") ||
        initializer.args.size() != 1 || !initializer.templateArgs.empty() || initializer.hasBodyArguments ||
        !initializer.bodyArguments.empty()) {
      return false;
    }
    ResultTypeInfo resultInfo;
    if (!resolveResultTypeForExpr(initializer.args.front(), params, locals, resultInfo) || !resultInfo.isResult ||
        !resultInfo.hasValue || resultInfo.valueType.empty()) {
      return false;
    }
    return assignBindingTypeFromText(resultInfo.valueType);
  };
  auto inferDirectResultOkBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || !initializer.isMethodCall || initializer.name != "ok" ||
        initializer.templateArgs.size() != 0 || initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
      return false;
    }
    if (initializer.args.empty()) {
      return false;
    }
    const Expr &receiver = initializer.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    auto inferCurrentErrorType = [&]() -> std::string {
      if (currentValidationContext_.resultType.has_value() && currentValidationContext_.resultType->isResult && !currentValidationContext_.resultType->errorType.empty()) {
        return currentValidationContext_.resultType->errorType;
      }
      if (currentValidationContext_.onError.has_value() && !currentValidationContext_.onError->errorType.empty()) {
        return currentValidationContext_.onError->errorType;
      }
      return "_";
    };
    if (initializer.args.size() == 1) {
      bindingOut.typeName = "Result";
      bindingOut.typeTemplateArg = inferCurrentErrorType();
      return true;
    }
    if (initializer.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(initializer.args.back(), params, locals, payloadBinding)) {
      return false;
    }
    const std::string payloadTypeText = bindingTypeText(payloadBinding);
    if (payloadTypeText.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    bindingOut.typeTemplateArg = payloadTypeText + ", " + inferCurrentErrorType();
    return true;
  };
  auto inferCollectionBindingFromExpr = [&](const Expr &expr) -> bool {
    auto copyNamedBinding = [&](const std::string &name) -> bool {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto it = locals.find(name);
      if (it == locals.end()) {
        return false;
      }
      bindingOut = it->second;
      return true;
    };
    if (expr.kind == Expr::Kind::Name) {
      return copyNamedBinding(expr.name);
    }
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string collection;
    if (getBuiltinCollectionName(expr, collection)) {
      if ((collection == "array" || collection == "vector" || collection == "soa_vector") && expr.templateArgs.size() == 1) {
        bindingOut.typeName = collection;
        bindingOut.typeTemplateArg = expr.templateArgs.front();
        return true;
      }
      if (collection == "map" && expr.templateArgs.size() == 2) {
        bindingOut.typeName = "map";
        bindingOut.typeTemplateArg = joinTemplateArgs(expr.templateArgs);
        return true;
      }
    }
    auto defIt = defMap_.find(resolveCalleePath(expr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizedReturnType, base, argText)) {
        return false;
      }
      base = normalizeBindingTypeName(base);
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args)) {
        return false;
      }
      if ((base == "array" || base == "vector" || base == "soa_vector") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if (isMapCollectionTypeName(base) && args.size() == 2) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = argText;
        return true;
      }
      if ((base == "Reference" || base == "Pointer") && args.size() == 1) {
        bindingOut.typeName = base;
        bindingOut.typeTemplateArg = args.front();
        return true;
      }
      return false;
    }
    return false;
  };
  auto inferBuiltinCollectionValueBinding = [&](const Expr &expr) -> bool {
    auto inferArrayElementType = [&](const BindingInfo &binding, std::string &elemTypeOut) {
      elemTypeOut.clear();
      if ((binding.typeName == "array" || binding.typeName == "vector" || binding.typeName == "soa_vector") &&
          !binding.typeTemplateArg.empty()) {
        elemTypeOut = binding.typeTemplateArg;
        return true;
      }
      if ((binding.typeName == "Reference" || binding.typeName == "Pointer") && !binding.typeTemplateArg.empty()) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(binding.typeTemplateArg, base, argText)) {
          return false;
        }
        base = normalizeBindingTypeName(base);
        if ((base == "array" || base == "vector" || base == "soa_vector") && !argText.empty()) {
          elemTypeOut = argText;
          return true;
        }
      }
      return false;
    };
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinAccessName;
    if (getBuiltinArrayAccessName(expr, builtinAccessName) && expr.args.size() == 2) {
      BindingInfo collectionBinding;
      if (!inferCollectionBindingFromExpr(expr.args.front())) {
        return false;
      }
      collectionBinding = bindingOut;
      std::string keyType;
      std::string valueType;
      if (extractMapKeyValueTypes(collectionBinding, keyType, valueType)) {
        bindingOut.typeName = normalizeBindingTypeName(valueType);
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      std::string elemType;
      if (inferArrayElementType(collectionBinding, elemType)) {
        bindingOut.typeName = normalizeBindingTypeName(elemType);
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      return false;
    }
    const bool isCountLike =
        (isSimpleCallName(expr, "count") ||
         (resolveCalleePath(expr) == "/std/collections/map/count" &&
          defMap_.find("/std/collections/map/count") != defMap_.end())) &&
        expr.args.size() == 1;
    const bool isMapContainsLike =
        !expr.isMethodCall && isSimpleCallName(expr, "contains") && expr.args.size() == 2 &&
        (currentValidationContext_.definitionPath == "/std/collections/mapContains" ||
         currentValidationContext_.definitionPath == "/std/collections/mapTryAt");
    if (isMapContainsLike) {
      BindingInfo collectionBinding;
      if (!inferCollectionBindingFromExpr(expr.args.front())) {
        return false;
      }
      collectionBinding = bindingOut;
      std::string keyType;
      std::string valueType;
      if (!extractMapKeyValueTypes(collectionBinding, keyType, valueType)) {
        return false;
      }
      bindingOut.typeName = "bool";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (!isCountLike) {
      return false;
    }
    BindingInfo collectionBinding;
    if (!inferCollectionBindingFromExpr(expr.args.front())) {
      return false;
    }
    collectionBinding = bindingOut;
    std::string keyType;
    std::string valueType;
    std::string elemType;
    if (extractMapKeyValueTypes(collectionBinding, keyType, valueType) ||
        inferArrayElementType(collectionBinding, elemType) ||
        collectionBinding.typeName == "string") {
      bindingOut.typeName = "i32";
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    return false;
  };
  auto inferBuiltinPointerBinding = [&](const Expr &expr) -> bool {
    std::function<bool(const Expr &, std::string &)> resolvePointerTargetType;
    resolvePointerTargetType = [&](const Expr &candidate, std::string &targetOut) -> bool {
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          if ((paramBinding->typeName == "Pointer" || paramBinding->typeName == "Reference") &&
              !paramBinding->typeTemplateArg.empty()) {
            targetOut = paramBinding->typeTemplateArg;
            return true;
          }
          return false;
        }
        auto it = locals.find(candidate.name);
        if (it == locals.end()) {
          return false;
        }
        if ((it->second.typeName == "Pointer" || it->second.typeName == "Reference") &&
            !it->second.typeTemplateArg.empty()) {
          targetOut = it->second.typeTemplateArg;
          return true;
        }
        return false;
      }
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      std::string builtinName;
      if (getBuiltinPointerName(candidate, builtinName) && builtinName == "location" && candidate.args.size() == 1) {
        const Expr &target = candidate.args.front();
        if (target.kind != Expr::Kind::Name) {
          return false;
        }
        if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
          if (paramBinding->typeName == "Reference" && !paramBinding->typeTemplateArg.empty()) {
            targetOut = paramBinding->typeTemplateArg;
          } else {
            targetOut = paramBinding->typeName;
          }
          return true;
        }
        auto it = locals.find(target.name);
        if (it == locals.end()) {
          return false;
        }
        if (it->second.typeName == "Reference" && !it->second.typeTemplateArg.empty()) {
          targetOut = it->second.typeTemplateArg;
        } else {
          targetOut = it->second.typeName;
        }
        return true;
      }
      if (getBuiltinMemoryName(candidate, builtinName)) {
        if (builtinName == "alloc" && candidate.templateArgs.size() == 1 && candidate.args.size() == 1) {
          targetOut = candidate.templateArgs.front();
          return true;
        }
        if (builtinName == "realloc" && candidate.args.size() == 2) {
          return resolvePointerTargetType(candidate.args.front(), targetOut);
        }
        if (builtinName == "at" && candidate.templateArgs.empty() && candidate.args.size() == 3) {
          return resolvePointerTargetType(candidate.args.front(), targetOut);
        }
        if (builtinName == "at_unsafe" && candidate.templateArgs.empty() && candidate.args.size() == 2) {
          return resolvePointerTargetType(candidate.args.front(), targetOut);
        }
      }
      std::string opName;
      if (getBuiltinOperatorName(candidate, opName) && (opName == "plus" || opName == "minus") &&
          candidate.args.size() == 2) {
        if (isPointerLikeExpr(candidate.args[1], params, locals)) {
          return false;
        }
        return resolvePointerTargetType(candidate.args.front(), targetOut);
      }
      return false;
    };
    if (expr.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinName;
    if (!getBuiltinMemoryName(expr, builtinName)) {
      return false;
    }
    std::string targetType;
    if (builtinName == "alloc") {
      if (expr.templateArgs.size() != 1 || expr.args.size() != 1) {
        return false;
      }
      targetType = expr.templateArgs.front();
    } else if (builtinName == "realloc") {
      if (expr.args.size() != 2 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "at") {
      if (!expr.templateArgs.empty() || expr.args.size() != 3 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else if (builtinName == "at_unsafe") {
      if (!expr.templateArgs.empty() || expr.args.size() != 2 || !resolvePointerTargetType(expr.args.front(), targetType)) {
        return false;
      }
    } else {
      return false;
    }
    bindingOut.typeName = "Pointer";
    bindingOut.typeTemplateArg = targetType;
    return true;
  };
  auto inferCallInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call) {
      return false;
    }
    Expr initializerForInference = initializer;
    const Expr *initializerExprForInference = &initializer;
    std::string namespacedCollection;
    std::string namespacedHelper;
    const std::string resolvedInitializer = resolveCalleePath(initializer);
    auto isResolvedMapConstructorPath = [](std::string resolvedPath) {
      const size_t specializationSuffix = resolvedPath.find("__t");
      if (specializationSuffix != std::string::npos) {
        resolvedPath.erase(specializationSuffix);
      }
      return resolvedPath == "/std/collections/map/map" ||
             resolvedPath == "/std/collections/mapNew" ||
             resolvedPath == "/std/collections/mapSingle" ||
             resolvedPath == "/std/collections/mapPair" ||
             resolvedPath == "/std/collections/mapDouble" ||
             resolvedPath == "/std/collections/mapTriple" ||
             resolvedPath == "/std/collections/mapQuad" ||
             resolvedPath == "/std/collections/mapQuint" ||
             resolvedPath == "/std/collections/mapSext" ||
             resolvedPath == "/std/collections/mapSept" ||
             resolvedPath == "/std/collections/mapOct" ||
             resolvedPath == "/std/collections/experimental_map/mapNew" ||
             resolvedPath == "/std/collections/experimental_map/mapSingle" ||
             resolvedPath == "/std/collections/experimental_map/mapPair" ||
             resolvedPath == "/std/collections/experimental_map/mapDouble" ||
             resolvedPath == "/std/collections/experimental_map/mapTriple" ||
             resolvedPath == "/std/collections/experimental_map/mapQuad" ||
             resolvedPath == "/std/collections/experimental_map/mapQuint" ||
             resolvedPath == "/std/collections/experimental_map/mapSext" ||
             resolvedPath == "/std/collections/experimental_map/mapSept" ||
             resolvedPath == "/std/collections/experimental_map/mapOct";
    };
    auto inferDeclaredDirectCallBinding = [&](const std::string &resolvedPath) -> bool {
      const auto defIt = defMap_.find(resolvedPath);
      if (defIt == defMap_.end() || defIt->second == nullptr) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
        if (normalizedReturnType.empty() || normalizedReturnType == "auto") {
          return false;
        }
        if (normalizedReturnType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
          bindingOut.typeName = "/" + normalizedReturnType;
          bindingOut.typeTemplateArg.clear();
          return true;
        }
        const std::string resolvedStructPath =
            resolveStructTypePath(normalizedReturnType, defIt->second->namespacePrefix, structNames_);
        if (!resolvedStructPath.empty()) {
          bindingOut.typeName = resolvedStructPath;
          bindingOut.typeTemplateArg.clear();
          return true;
        }
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedReturnType, base, argText)) {
          bindingOut.typeName = normalizeBindingTypeName(base);
          bindingOut.typeTemplateArg = argText;
          return true;
        }
        bindingOut.typeName = normalizedReturnType;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      return false;
    };
    auto hasImportedInitializerDefinitionPath = [&](const std::string &path) {
      std::string canonicalPath = path;
      const size_t suffix = canonicalPath.find("__t");
      if (suffix != std::string::npos) {
        canonicalPath.erase(suffix);
      }
      for (const auto &importPath : program_.imports) {
        if (importPath == canonicalPath) {
          return true;
        }
        if (importPath.size() >= 2 && importPath.compare(importPath.size() - 2, 2, "/*") == 0) {
          const std::string prefix = importPath.substr(0, importPath.size() - 2);
          if (canonicalPath == prefix || canonicalPath.rfind(prefix + "/", 0) == 0) {
            return true;
          }
        }
      }
      return false;
    };
    const bool isUnresolvedStdNamespacedVectorCountCapacityCall =
        initializer.kind == Expr::Kind::Call &&
        !initializer.isMethodCall &&
        getNamespacedCollectionHelperName(initializer, namespacedCollection, namespacedHelper) &&
        namespacedCollection == "vector" &&
        (namespacedHelper == "count" || namespacedHelper == "capacity") &&
        resolvedInitializer == "/std/collections/vector/" + namespacedHelper &&
        defMap_.find(resolvedInitializer) == defMap_.end() &&
        !hasImportedInitializerDefinitionPath(resolvedInitializer);
    if (isUnresolvedStdNamespacedVectorCountCapacityCall) {
      initializerForInference.name = "/vector/" + namespacedHelper;
      initializerForInference.namespacePrefix.clear();
      initializerExprForInference = &initializerForInference;
    }
    auto isUnresolvedActiveInferenceCall = [&](const Expr &candidate) {
      if (candidate.kind != Expr::Kind::Call) {
        return false;
      }
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (resolvedPath.empty()) {
        return false;
      }
      BindingInfo resolvedBinding;
      if (inferResolvedDirectCallBindingType(resolvedPath, resolvedBinding)) {
        return false;
      }
      return returnBindingInferenceStack_.contains(resolvedPath) ||
             inferenceStack_.contains(resolvedPath) ||
             queryTypeInferenceDefinitionStack_.contains(resolvedPath);
    };
    auto preferredResolvedInitializerPath = [&]() -> std::string {
      auto normalizedName = initializer.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      auto normalizedPrefix = initializer.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }

      auto explicitStdMapHelperName = [&]() -> std::string {
        if (normalizedPrefix == "std/collections/map" &&
            (normalizedName == "count" || normalizedName == "contains" ||
             normalizedName == "tryAt" || normalizedName == "at" ||
             normalizedName == "at_unsafe")) {
          return normalizedName;
        }
        if (normalizedName.rfind("std/collections/map/", 0) == 0) {
          const std::string helperName =
              normalizedName.substr(std::string("std/collections/map/").size());
          if (helperName == "count" || helperName == "contains" || helperName == "tryAt" ||
              helperName == "at" || helperName == "at_unsafe") {
            return helperName;
          }
        }
        return {};
      };
      auto explicitStdVectorHelperName = [&]() -> std::string {
        if (normalizedPrefix == "std/collections/vector" &&
            (normalizedName == "at" || normalizedName == "at_unsafe" ||
             normalizedName == "push" || normalizedName == "pop" ||
             normalizedName == "reserve" || normalizedName == "clear" ||
             normalizedName == "remove_at" || normalizedName == "remove_swap")) {
          return normalizedName;
        }
        if (normalizedName.rfind("std/collections/vector/", 0) == 0) {
          const std::string helperName =
              normalizedName.substr(std::string("std/collections/vector/").size());
          if (helperName == "at" || helperName == "at_unsafe" ||
              helperName == "push" || helperName == "pop" ||
              helperName == "reserve" || helperName == "clear" ||
              helperName == "remove_at" || helperName == "remove_swap") {
            return helperName;
          }
        }
        return {};
      };

      if (const std::string helperName = explicitStdMapHelperName(); !helperName.empty()) {
        const std::string canonical = "/std/collections/map/" + helperName;
        const bool prefersAlias = helperName == "at" || helperName == "at_unsafe";
        const std::string alias = "/map/" + helperName;
        if (prefersAlias && defMap_.count(alias) > 0) {
          return alias;
        }
        if (defMap_.count(canonical) > 0) {
          return canonical;
        }
        if (defMap_.count(alias) > 0) {
          return alias;
        }
      }

      if (const std::string helperName = explicitStdVectorHelperName(); !helperName.empty()) {
        const std::string alias = "/vector/" + helperName;
        if (defMap_.count(alias) > 0) {
          return alias;
        }
        const std::string canonical = "/std/collections/vector/" + helperName;
        if (defMap_.count(canonical) > 0) {
          return canonical;
        }
      }

      if ((normalizedPrefix == "vector" && normalizedName == "vector") ||
          normalizedName == "vector/vector") {
        const std::string canonical = "/std/collections/vector/vector";
        if (defMap_.count(canonical) > 0) {
          return canonical;
        }
      }

      return {};
    };
    const std::string preferredResolvedInitializer = preferredResolvedInitializerPath();
    if (!preferredResolvedInitializer.empty()) {
      if (inferResolvedDirectCallBindingType(preferredResolvedInitializer, bindingOut)) {
        return true;
      }
      if (inferDeclaredDirectCallBinding(preferredResolvedInitializer)) {
        return true;
      }
    }
    std::string inferredTypeText;
    if (initializerExprForInference != nullptr &&
        !isUnresolvedActiveInferenceCall(*initializerExprForInference) &&
        inferQueryExprTypeText(*initializerExprForInference, params, locals, inferredTypeText) &&
        assignBindingTypeFromText(inferredTypeText)) {
      const std::string resolvedInferencePath = resolveCalleePath(*initializerExprForInference);
      if (isResolvedMapConstructorPath(resolvedInferencePath)) {
        BindingInfo resolvedCallBinding;
        if (inferResolvedDirectCallBindingType(resolvedInferencePath, resolvedCallBinding)) {
          bindingOut = std::move(resolvedCallBinding);
          return true;
        }
        std::vector<std::string> mapArgs;
        if (resolveCallCollectionTemplateArgs(*initializerExprForInference, "map", params, locals, mapArgs) &&
            mapArgs.size() == 2) {
          bindingOut.typeName = "Map";
          bindingOut.typeTemplateArg = joinTemplateArgs(mapArgs);
          return true;
        }
      }
      const std::string normalizedBindingType = normalizeBindingTypeName(bindingOut.typeName);
      const bool shouldPreferResolvedDirectCallBinding =
          ((!bindingOut.typeTemplateArg.empty() && isMapCollectionTypeName(normalizedBindingType)) ||
           (bindingOut.typeTemplateArg.empty() &&
            !normalizedBindingType.empty() &&
            normalizedBindingType.front() != '/' &&
            returnKindForTypeName(normalizedBindingType) == ReturnKind::Unknown));
      if (shouldPreferResolvedDirectCallBinding) {
        BindingInfo resolvedCallBinding;
        if (inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), resolvedCallBinding)) {
          bindingOut = std::move(resolvedCallBinding);
        }
      }
      return true;
    }
    ReturnKind resolvedKind = inferExprReturnKind(*initializerExprForInference, params, locals);
    if (resolvedKind != ReturnKind::Unknown && resolvedKind != ReturnKind::Void) {
      if (resolvedKind == ReturnKind::Array) {
        if (inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), bindingOut)) {
          return true;
        }
        if (inferDeclaredDirectCallBinding(resolveCalleePath(*initializerExprForInference))) {
          return true;
        }
      }
      if (inferResolvedDirectCallBindingType(resolveCalleePath(initializer), bindingOut)) {
        return true;
      }
      if (inferDeclaredDirectCallBinding(resolveCalleePath(initializer))) {
        return true;
      }
      std::string inferredStruct = inferStructReturnPath(initializer, params, locals);
      if (!inferredStruct.empty()) {
        const std::string normalizedCollectionType = normalizeCollectionTypePath(inferredStruct);
        if (!normalizedCollectionType.empty()) {
          if (initializerExprForInference != nullptr &&
              !isUnresolvedActiveInferenceCall(*initializerExprForInference)) {
            std::string inferredTypeText;
            if (inferQueryExprTypeText(*initializerExprForInference, params, locals, inferredTypeText) &&
                assignBindingTypeFromText(inferredTypeText)) {
              return true;
            }
          }
          bindingOut.typeName = normalizedCollectionType.substr(1);
          bindingOut.typeTemplateArg.clear();
          if (initializerExprForInference != nullptr) {
            std::vector<std::string> collectionArgs;
            if (resolveCallCollectionTemplateArgs(*initializerExprForInference,
                                                 bindingOut.typeName,
                                                 params,
                                                 locals,
                                                 collectionArgs) &&
                !collectionArgs.empty()) {
              bindingOut.typeTemplateArg = joinTemplateArgs(collectionArgs);
              return true;
            }
          }
          std::string collectionName;
          if (initializerExprForInference != nullptr &&
              getBuiltinCollectionName(*initializerExprForInference, collectionName)) {
            if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
                initializerExprForInference->templateArgs.size() == 1) {
              bindingOut.typeName = collectionName;
              bindingOut.typeTemplateArg = initializerExprForInference->templateArgs.front();
            } else if (collectionName == "map" && initializerExprForInference->templateArgs.size() == 2) {
              bindingOut.typeName = "map";
              bindingOut.typeTemplateArg = joinTemplateArgs(initializerExprForInference->templateArgs);
            }
          }
          return true;
        }
        bindingOut.typeName = inferredStruct;
        bindingOut.typeTemplateArg.clear();
        return true;
      }
      if (resolvedKind == ReturnKind::Array) {
        return false;
      }
      std::string inferredType = typeNameForReturnKind(resolvedKind);
      if (inferredType.empty()) {
        return false;
      }
      bindingOut.typeName = inferredType;
      bindingOut.typeTemplateArg.clear();
      return true;
    }
    if (inferResolvedDirectCallBindingType(resolveCalleePath(*initializerExprForInference), bindingOut)) {
      return true;
    }
    if (inferDeclaredDirectCallBinding(resolveCalleePath(*initializerExprForInference))) {
      return true;
    }
    return false;
  };
  if (initializer.kind == Expr::Kind::Call) {
    const BindingInfo previousBinding = bindingOut;
    bindingOut = {};
    if (inferCallInitializerBinding()) {
      (void)canonicalizeInferredCollectionBinding(&initializer);
      if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
        return true;
      }
    }
    bindingOut = previousBinding;
  }
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, bindingOut, hasAnyMathImport())) {
    (void)canonicalizeInferredCollectionBinding(&initializer);
    if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
      return true;
    }
    if (inferCallInitializerBinding()) {
      (void)canonicalizeInferredCollectionBinding(&initializer);
      return true;
    }
    if (inferBuiltinCollectionValueBinding(initializer)) {
      return true;
    }
    if (inferBuiltinPointerBinding(initializer)) {
      return true;
    }
    return true;
  }
  if (inferTryInitializerBinding()) {
    return true;
  }
  if (inferDirectResultOkBinding()) {
    return true;
  }
  ResultTypeInfo resultInfo;
  if (resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
      assignBindingTypeFromResultInfo(resultInfo)) {
    return true;
  }
  if (inferCallInitializerBinding()) {
    (void)canonicalizeInferredCollectionBinding(&initializer);
    return true;
  }
  if (inferBuiltinCollectionValueBinding(initializer)) {
    return true;
  }
  if (inferBuiltinPointerBinding(initializer)) {
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
