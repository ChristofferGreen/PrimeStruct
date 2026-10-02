#include "SemanticsValidator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionHelperNames.h"
#include "SemanticsValidatorInferCollectionReturnInferenceHelpers.h"

namespace primec::semantics {
using namespace collectionReturnInferenceHelpers;

bool SemanticsValidator::inferDefinitionReturnBinding(const Definition &def, BindingInfo &bindingOut) {
  auto findDefParamBinding = [&](const std::vector<ParameterInfo> &defParams,
                                 const std::string &name) -> const BindingInfo * {
    for (const auto &param : defParams) {
      if (param.name == name) {
        return &param.binding;
      }
    }
    return nullptr;
  };
  auto parseTypeText = [&](const std::string &typeText, BindingInfo &parsedOut) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalized, base, argText) && !base.empty()) {
      parsedOut.typeName = base;
      parsedOut.typeTemplateArg = argText;
      return true;
    }
    parsedOut.typeName = normalized;
    parsedOut.typeTemplateArg.clear();
    return true;
  };
  std::function<bool(const std::string &)> isConcreteReturnTypeText;
  std::function<bool(const std::string &)> returnTypeReferencesTemplateParam;
  isConcreteReturnTypeText = [&](const std::string &typeText) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    if (returnKindForTypeName(normalized) != ReturnKind::Unknown) {
      return true;
    }
    if (!normalizeCollectionTypePath(normalized).empty()) {
      return true;
    }
    if (!resolveStructTypePath(normalized, def.namespacePrefix, structNames_).empty()) {
      return true;
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalized, base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base.empty()) {
      return false;
    }
    if (base == "Pointer" || base == "Reference" || base == "Result" ||
        base == "Buffer" || base == "uninitialized" || base == "array" ||
        base == "vector" || base == "soa" || base == "Task" ||
        isKeyValueSurfaceTypeName(base) ||
        base == "Vector" ||
        isLegacyExperimentalVectorCompatibilityPath("/" + base) ||
        isUnspecializedExperimentalKeyValueBackingTypeName(base) ||
        !resolveStructTypePath(base, def.namespacePrefix, structNames_).empty()) {
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(argText, args) || args.empty()) {
        return false;
      }
      for (const auto &arg : args) {
        if (!isConcreteReturnTypeText(arg)) {
          return false;
        }
      }
      return true;
    }
    return false;
  };
  returnTypeReferencesTemplateParam = [&](const std::string &typeText) -> bool {
    const std::string normalized = normalizeBindingTypeName(typeText);
    if (normalized.empty()) {
      return false;
    }
    for (const auto &templateArg : def.templateArgs) {
      if (normalizeBindingTypeName(templateArg) == normalized) {
        return true;
      }
    }
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalized, base, argText)) {
      return false;
    }
    if (returnTypeReferencesTemplateParam(base)) {
      return true;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args)) {
      return false;
    }
    for (const auto &arg : args) {
      if (returnTypeReferencesTemplateParam(arg)) {
        return true;
      }
    }
    return false;
  };

  const bool isSpecializedDefinition = def.fullPath.find("__t") != std::string::npos;
  bool sawReturnTransform = false;
  bool sawUnusableReturnTransform = false;
  for (const auto &transform : def.transforms) {
    if (transform.name != "return" || transform.templateArgs.size() != 1) {
      continue;
    }
    sawReturnTransform = true;
    const std::string &returnType = transform.templateArgs.front();
    if (returnType == "auto") {
      sawUnusableReturnTransform = true;
      break;
    }
    if (isSpecializedDefinition) {
      const bool referencesTemplateParam = returnTypeReferencesTemplateParam(returnType);
      const bool isConcreteReturnType = isConcreteReturnTypeText(returnType);
      if (referencesTemplateParam || !isConcreteReturnType) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizeBindingTypeName(returnType), base, argText) &&
            !base.empty()) {
          return parseTypeText(returnType, bindingOut);
        }
        sawUnusableReturnTransform = true;
        break;
      }
    }
    return parseTypeText(returnType, bindingOut);
  }
  if (isSpecializedDefinition && (!sawReturnTransform || sawUnusableReturnTransform)) {
    const std::string &path = def.fullPath;
    const size_t suffix = path.find("__t");
    if (suffix != std::string::npos) {
      std::string basePath = path.substr(0, suffix);
      const size_t nextSlash = path.find('/', suffix);
      if (nextSlash != std::string::npos) {
        basePath += path.substr(nextSlash);
      }
      auto tryBase = [&](const std::string &candidate) -> bool {
        auto baseIt = defMap_.find(candidate);
        if (baseIt == defMap_.end() || baseIt->second == nullptr) {
          return false;
        }
        for (const auto &transform : baseIt->second->transforms) {
          if (transform.name != "return" || transform.templateArgs.size() != 1) {
            continue;
          }
          const std::string &returnType = transform.templateArgs.front();
          if (returnType == "auto") {
            break;
          }
          if (isConcreteReturnTypeText(returnType)) {
            return parseTypeText(returnType, bindingOut);
          }
          break;
        }
        return false;
      };
      if (tryBase(basePath)) {
        return true;
      }
      if (!basePath.empty() && basePath.front() == '/') {
        if (tryBase(basePath.substr(1))) {
          return true;
        }
      } else {
        if (tryBase("/" + basePath)) {
          return true;
        }
      }
    }
  }

  auto cachedBindingIt = returnBindings_.find(def.fullPath);
  if (cachedBindingIt != returnBindings_.end() && !cachedBindingIt->second.typeName.empty()) {
    bindingOut = cachedBindingIt->second;
    return true;
  }

  if (!returnBindingInferenceStack_.insert(def.fullPath).second) {
    return false;
  }
  struct ReturnBindingInferenceScopeGuard {
    std::unordered_set<std::string> &stack;
    const std::string &path;
    ~ReturnBindingInferenceScopeGuard() {
      stack.erase(path);
    }
  } returnBindingInferenceGuard{returnBindingInferenceStack_, def.fullPath};

  ValidationStateScope validationContextScope(*this, buildDefinitionValidationState(def));

  std::vector<ParameterInfo> defParams;
  defParams.reserve(def.parameters.size());
  for (const auto &paramExpr : def.parameters) {
    ParameterInfo paramInfo;
    paramInfo.name = paramExpr.name;
    std::optional<std::string> restrictType;
    std::string parseError;
    (void)parseBindingInfo(
        paramExpr,
        def.namespacePrefix,
        structNames_,
        importAliases_,
        paramInfo.binding,
        restrictType,
        parseError,
        &sumNames_);
    if (paramExpr.args.size() == 1) {
      paramInfo.defaultExpr = &paramExpr.args.front();
    }
    defParams.push_back(std::move(paramInfo));
  }

  std::unordered_map<std::string, BindingInfo> defLocals;
  const Expr *valueExpr = nullptr;
  bool sawReturn = false;
  for (const auto &stmt : def.statements) {
    if (stmt.isBinding) {
      if (isCompileTimeTypeBinding(stmt)) {
        continue;
      }
      BindingInfo binding;
      std::optional<std::string> restrictType;
      std::string parseError;
      if (parseBindingInfo(stmt, def.namespacePrefix, structNames_, importAliases_, binding, restrictType, parseError,
                           &sumNames_)) {
        const bool hasExplicitType = hasExplicitBindingTypeTransform(stmt);
        const bool explicitAutoType = hasExplicitType && normalizeBindingTypeName(binding.typeName) == "auto";
        if (stmt.args.size() == 1 && (!hasExplicitType || explicitAutoType)) {
          (void)inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding, &stmt);
        }
        defLocals[stmt.name] = binding;
      } else if (stmt.args.size() == 1 &&
                 inferBindingTypeFromInitializer(stmt.args.front(), defParams, defLocals, binding, &stmt)) {
        defLocals[stmt.name] = binding;
      }
      continue;
    }
    if (isReturnCall(stmt)) {
      if (stmt.args.size() != 1) {
        return false;
      }
      valueExpr = &stmt.args.front();
      sawReturn = true;
      continue;
    }
    if (!sawReturn) {
      valueExpr = &stmt;
    }
  }
  if (def.returnExpr.has_value()) {
    valueExpr = &*def.returnExpr;
  }
  if (valueExpr == nullptr) {
    return false;
  }
  if (valueExpr->kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findDefParamBinding(defParams, valueExpr->name)) {
      bindingOut = *paramBinding;
      return true;
    }
    auto localIt = defLocals.find(valueExpr->name);
    if (localIt != defLocals.end()) {
      bindingOut = localIt->second;
      return true;
    }
  }
  return inferBindingTypeFromInitializer(*valueExpr, defParams, defLocals, bindingOut);
}

} // namespace primec::semantics
