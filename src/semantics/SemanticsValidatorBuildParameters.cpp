#include "SemanticsValidator.h"

#include <array>
#include <string_view>

#include "semantics/MapConstructorHelpers.h"

namespace primec::semantics {

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
    ValidationContext definitionValidationContext;
    definitionValidationContext.definitionPath = def.fullPath;
    for (const auto &transform : def.transforms) {
      if (transform.name == "compute") {
        definitionValidationContext.definitionIsCompute = true;
      } else if (transform.name == "unsafe") {
        definitionValidationContext.definitionIsUnsafe = true;
      } else if (transform.name == "return" && transform.templateArgs.size() == 1) {
        ResultTypeInfo resultInfo;
        if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultInfo)) {
          definitionValidationContext.resultType = std::move(resultInfo);
        }
      }
    }
    definitionValidationContext.activeEffects = resolveEffects(def.transforms, def.fullPath == entryPath_);
    ValidationContextScope validationContextScope(*this, std::move(definitionValidationContext));
    std::unordered_set<std::string> seen;
    std::vector<ParameterInfo> params;
    params.reserve(def.parameters.size());
    auto bindingTypeText = [](const BindingInfo &binding) {
      if (binding.typeTemplateArg.empty()) {
        return binding.typeName;
      }
      return binding.typeName + "<" + binding.typeTemplateArg + ">";
    };
    auto defaultResolvesToDefinition = [&](const Expr &candidate) -> bool {
      return candidate.kind == Expr::Kind::Call && defMap_.find(resolveCalleePath(candidate)) != defMap_.end();
    };
    auto typeTextIsExperimentalMapValue = [](const std::string &typeText) {
      std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        normalizedType = normalizeBindingTypeName(base);
      }
      if (!normalizedType.empty() && normalizedType.front() == '/') {
        normalizedType.erase(normalizedType.begin());
      }
      return normalizedType == "Map" || normalizedType == "std/collections/experimental_map/Map" ||
             normalizedType.rfind("std/collections/experimental_map/Map__", 0) == 0;
    };
    auto bindingCarriesExperimentalMapValue = [&](const BindingInfo &binding) {
      return typeTextIsExperimentalMapValue(bindingTypeText(binding));
    };
    auto isResolvedExperimentalMapConstructorPath = [](const std::string &resolvedPath) {
      const size_t specializationSuffix = resolvedPath.find("__t");
      if (specializationSuffix != std::string::npos) {
        resolvedPath.erase(specializationSuffix);
      }
      const size_t overloadSuffix = resolvedPath.find("__ov");
      if (overloadSuffix != std::string::npos) {
        resolvedPath.erase(overloadSuffix);
      }
      return isResolvedMapConstructorPath(resolvedPath);
    };
    auto isAllowedExperimentalMapDefaultExpr = [&](const Expr &candidate) {
      if (isDefaultExprAllowed(candidate, defaultResolvesToDefinition)) {
        return true;
      }
      if (candidate.isBinding || candidate.kind != Expr::Kind::Call) {
        return false;
      }
      if (hasNamedArguments(candidate.argNames) || candidate.hasBodyArguments || !candidate.bodyArguments.empty()) {
        return false;
      }
      if (!isResolvedExperimentalMapConstructorPath(resolveCalleePath(candidate))) {
        return false;
      }
      BindingInfo inferredBinding;
      if (!inferBindingTypeFromInitializer(candidate, {}, {}, inferredBinding) ||
          !bindingCarriesExperimentalMapValue(inferredBinding)) {
        return false;
      }
      return true;
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
      if (param.args.size() == 1 &&
          !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition) &&
          !isAllowedExperimentalMapDefaultExpr(param.args.front())) {
        if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
          error_ = "parameter default does not accept named arguments: " + param.name;
        } else {
          error_ = "parameter default must be a literal or pure expression: " + param.name;
        }
        return false;
      }
      if (!hasExplicitBindingTypeTransform(param) && param.args.size() == 1) {
        BindingInfo inferredBinding;
        if (inferBindingTypeFromInitializer(param.args.front(), {}, {}, inferredBinding, &param)) {
          binding = std::move(inferredBinding);
        } else {
          (void)tryInferBindingTypeFromInitializer(param.args.front(), {}, {}, binding, hasAnyMathImport());
        }
      }
      if (!validateBuiltinMapKeyType(binding, &def.templateArgs, error_)) {
        return false;
      }
      std::string argsPackElementType;
      if (getArgsPackElementType(binding, argsPackElementType)) {
        std::string elementBase;
        std::string elementArg;
        if (splitTemplateTypeName(normalizeBindingTypeName(argsPackElementType), elementBase, elementArg)) {
          const std::string normalizedElementBase = normalizeBindingTypeName(elementBase);
          if ((normalizedElementBase == "Pointer" || normalizedElementBase == "Reference") &&
              normalizeBindingTypeName(elementArg) == "string") {
            error_ = "variadic args<T> does not support string pointers or references";
            return false;
          }
        }
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

}  // namespace primec::semantics
