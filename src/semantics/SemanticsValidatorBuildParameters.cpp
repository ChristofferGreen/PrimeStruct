#include "SemanticsValidator.h"

#include <array>
#include <functional>
#include <string_view>

#include "MapConstructorHelpers.h"

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
    ValidationState definitionValidationState;
    definitionValidationState.context.definitionPath = def.fullPath;
    for (const auto &transform : def.transforms) {
      if (transform.name == "compute") {
        definitionValidationState.context.definitionIsCompute = true;
      } else if (transform.name == "unsafe") {
        definitionValidationState.context.definitionIsUnsafe = true;
      } else if (transform.name == "return" && transform.templateArgs.size() == 1) {
        ResultTypeInfo resultInfo;
        if (resolveResultTypeFromTypeName(transform.templateArgs.front(), resultInfo)) {
          definitionValidationState.context.resultType = std::move(resultInfo);
        }
      }
    }
    definitionValidationState.context.activeEffects = resolveEffects(def.transforms, def.fullPath == entryPath_);
    ValidationStateScope validationContextScope(*this, std::move(definitionValidationState));
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
    auto typeTextCarriesExperimentalMapValue = [&](const std::string &typeText) {
      if (typeTextIsExperimentalMapValue(typeText)) {
        return true;
      }
      ResultTypeInfo resultInfo;
      return resolveResultTypeFromTypeName(typeText, resultInfo) &&
             resultInfo.hasValue &&
             typeTextIsExperimentalMapValue(resultInfo.valueType);
    };
    auto bindingCarriesExperimentalMapValue = [&](const BindingInfo &binding) {
      return typeTextCarriesExperimentalMapValue(bindingTypeText(binding));
    };
    auto isResolvedExperimentalMapConstructorPath = [](const std::string &resolvedPath) {
      std::string normalizedPath = resolvedPath;
      const size_t specializationSuffix = normalizedPath.find("__t");
      if (specializationSuffix != std::string::npos) {
        normalizedPath.erase(specializationSuffix);
      }
      const size_t overloadSuffix = normalizedPath.find("__ov");
      if (overloadSuffix != std::string::npos) {
        normalizedPath.erase(overloadSuffix);
      }
      return isResolvedMapConstructorPath(normalizedPath);
    };
    auto isBuiltinResultOkPayloadCall = [](const Expr &candidate) {
      if (candidate.kind != Expr::Kind::Call || !candidate.isMethodCall || candidate.name != "ok" ||
          candidate.args.size() != 2) {
        return false;
      }
      const Expr &receiver = candidate.args.front();
      return receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "Result";
    };
    std::function<bool(const Expr &)> isAllowedExperimentalMapDefaultExpr = [&](const Expr &candidate) {
      if (isDefaultExprAllowed(candidate, defaultResolvesToDefinition)) {
        return true;
      }
      if (candidate.isBinding || candidate.kind != Expr::Kind::Call) {
        return false;
      }
      if (hasNamedArguments(candidate.argNames) || candidate.hasBodyArguments || !candidate.bodyArguments.empty()) {
        return false;
      }
      if (isBuiltinResultOkPayloadCall(candidate)) {
        return isAllowedExperimentalMapDefaultExpr(candidate.args.back());
      }
      const std::string resolvedPath = resolveCalleePath(candidate);
      const bool isDirectExperimentalMapConstructor = isResolvedExperimentalMapConstructorPath(resolvedPath);
      if (!isDirectExperimentalMapConstructor) {
        for (const auto &arg : candidate.args) {
          if (!isAllowedExperimentalMapDefaultExpr(arg)) {
            return false;
          }
        }
      }
      BindingInfo inferredBinding;
      if (!inferBindingTypeFromInitializer(candidate, {}, {}, inferredBinding) ||
          !bindingCarriesExperimentalMapValue(inferredBinding)) {
        BindingInfo fallbackBinding;
        if (!tryInferBindingTypeFromInitializer(candidate, {}, {}, fallbackBinding, hasAnyMathImport()) ||
            !bindingCarriesExperimentalMapValue(fallbackBinding)) {
          return false;
        }
        inferredBinding = std::move(fallbackBinding);
      }
      if (defMap_.find(resolvedPath) != defMap_.end()) {
        return true;
      }
      if (!isDirectExperimentalMapConstructor) {
        return false;
      }
      return true;
    };
    for (const auto &param : def.parameters) {
      auto failParameterDiagnostic = [&](std::string message) -> bool {
        return failExprDiagnostic(param, std::move(message));
      };
      if (!param.isBinding) {
        return failParameterDiagnostic("parameters must use binding syntax: " +
                                       def.fullPath);
      }
      if (param.hasBodyArguments || !param.bodyArguments.empty()) {
        return failParameterDiagnostic(
            "parameter does not accept block arguments: " + param.name);
      }
      if (!seen.insert(param.name).second) {
        return failParameterDiagnostic("duplicate parameter: " + param.name);
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      if (!parseBindingInfo(param, def.namespacePrefix, structNames_, importAliases_, binding,
                            restrictType, error_, &sumNames_)) {
        return failExprDiagnostic(param, error_);
      }
      if (binding.typeName == "uninitialized") {
        return failParameterDiagnostic(
            "uninitialized storage is not allowed on parameters: " +
            param.name);
      }
      if (param.args.size() > 1) {
        return failParameterDiagnostic(
            "parameter defaults accept at most one argument: " + param.name);
      }
      if (param.args.size() == 1 &&
          !isDefaultExprAllowed(param.args.front(), defaultResolvesToDefinition) &&
          !isAllowedExperimentalMapDefaultExpr(param.args.front())) {
        if (param.args.front().kind == Expr::Kind::Call && hasNamedArguments(param.args.front().argNames)) {
          return failParameterDiagnostic(
              "parameter default does not accept named arguments: " +
              param.name);
        } else {
          return failParameterDiagnostic(
              "parameter default must be a literal or pure expression: " +
              param.name);
        }
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
        return failExprDiagnostic(param, error_);
      }
      std::string argsPackElementType;
      if (getArgsPackElementType(binding, argsPackElementType)) {
        std::string elementBase;
        std::string elementArg;
        if (splitTemplateTypeName(normalizeBindingTypeName(argsPackElementType), elementBase, elementArg)) {
          const std::string normalizedElementBase = normalizeBindingTypeName(elementBase);
          if ((normalizedElementBase == "Pointer" || normalizedElementBase == "Reference") &&
              normalizeBindingTypeName(elementArg) == "string") {
            return failParameterDiagnostic(
                "variadic args<T> does not support string pointers or references");
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
          return failParameterDiagnostic(
              "restrict type does not match binding type");
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
    auto failBuildParameterDefinitionDiagnostic = [&](std::string message) -> bool {
      return failDefinitionDiagnostic(def, std::move(message));
    };
    for (const auto &transform : def.transforms) {
      if (transform.name != "mut") {
        continue;
      }
      if (sawMut) {
        return failBuildParameterDefinitionDiagnostic(
            "duplicate mut transform on " + def.fullPath);
      }
      sawMut = true;
      if (!transform.templateArgs.empty()) {
        return failBuildParameterDefinitionDiagnostic(
            "mut transform does not accept template arguments on " +
            def.fullPath);
      }
      if (!transform.arguments.empty()) {
        return failBuildParameterDefinitionDiagnostic(
            "mut transform does not accept arguments on " + def.fullPath);
      }
    }
    if (sawMut && !isStructHelper) {
      return failBuildParameterDefinitionDiagnostic(
          "mut transform is only supported on struct helpers: " + def.fullPath);
    }
    if (sawMut && isStaticHelper) {
      return failBuildParameterDefinitionDiagnostic(
          "mut transform is not allowed on static helpers: " + def.fullPath);
    }

    if (isLifecycle) {
      if (isCopyHelperName(def.fullPath)) {
        if (params.size() != 1) {
          return failBuildParameterDefinitionDiagnostic(
              "Copy/Move helpers require exactly one parameter: " +
              def.fullPath);
        }
        const auto &copyParam = params.front();
        if (copyParam.binding.typeName != "Reference" || copyParam.binding.typeTemplateArg != parentPath) {
          return failBuildParameterDefinitionDiagnostic(
              "Copy/Move helpers require [Reference<Self>] parameter: " +
              def.fullPath);
        }
      }
    }
    if (isStructHelper && !isStaticHelper) {
      if (!seen.insert("this").second) {
        return failBuildParameterDefinitionDiagnostic(
            "duplicate parameter: this");
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
