#include "SemanticsValidator.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"

namespace primec::semantics {

void SemanticsValidator::populateBuiltinCollectionDispatchBufferAndMapResolvers(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> &state,
    const std::function<bool(const Expr &, BindingInfo &)> &resolveBindingTarget,
    const std::function<bool(const Expr &, BindingInfo &)> &inferCallBinding,
    const std::function<bool(const BindingInfo &, std::string &, std::string &)> &resolveMapBinding,
    const std::function<bool(const BindingInfo &, std::string &, std::string &)>
        &extractExperimentalMapFieldTypes,
    const std::function<bool(const Expr &)> &isDirectMapConstructorCall) {
  const std::weak_ptr<BuiltinCollectionDispatchResolvers> weakState = state;
  state->resolveBufferTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    auto resolveReferenceBufferType = [&](const std::string &typeName,
                                          const std::string &typeTemplateArg,
                                          std::string &elemTypeOut) -> bool {
      if ((typeName != "Reference" && typeName != "Pointer") || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) || normalizeBindingTypeName(base) != "Buffer" ||
          nestedArg.empty()) {
        return false;
      }
      elemTypeOut = nestedArg;
      return true;
    };
    auto resolveArgsPackReferenceBufferType = [&](const std::string &typeName,
                                                  const std::string &typeTemplateArg,
                                                  std::string &elemTypeOut) -> bool {
      if (typeName != "args" || typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string nestedArg;
      if (!splitTemplateTypeName(typeTemplateArg, base, nestedArg) ||
          (normalizeBindingTypeName(base) != "Reference" && normalizeBindingTypeName(base) != "Pointer") ||
          nestedArg.empty()) {
        return false;
      }
      return resolveReferenceBufferType(base, nestedArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackReferenceBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        if (resolveArgsPackReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemTypeOut)) {
          return true;
        }
      }
      auto it = locals.find(targetName);
      return it != locals.end() &&
             resolveArgsPackReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemTypeOut);
    };
    auto resolveIndexedArgsPackValueBuffer = [&](const Expr &targetExpr, std::string &elemTypeOut) -> bool {
      std::string accessName;
      if (targetExpr.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(targetExpr, accessName) ||
          targetExpr.args.size() != 2 || targetExpr.args.front().kind != Expr::Kind::Name) {
        return false;
      }
      const std::string &targetName = targetExpr.args.front().name;
      auto resolveBinding = [&](const BindingInfo &binding) {
        std::string packElemType;
        std::string base;
        return getArgsPackElementType(binding, packElemType) &&
               splitTemplateTypeName(normalizeBindingTypeName(packElemType), base, elemTypeOut) &&
               normalizeBindingTypeName(base) == "Buffer";
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, targetName)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(targetName);
      return it != locals.end() && resolveBinding(it->second);
    };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (normalizeBindingTypeName(paramBinding->typeName) == "Buffer" &&
            !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        if (normalizeBindingTypeName(it->second.typeName) == "Buffer" &&
            !it->second.typeTemplateArg.empty()) {
          elemType = it->second.typeTemplateArg;
          return true;
        }
      }
      return false;
    }
    if (target.kind == Expr::Kind::Call) {
      if (resolveIndexedArgsPackValueBuffer(target, elemType)) {
        return true;
      }
      if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
        const Expr &innerTarget = target.args.front();
        if (innerTarget.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, innerTarget.name)) {
            if (resolveReferenceBufferType(paramBinding->typeName, paramBinding->typeTemplateArg, elemType)) {
              return true;
            }
          }
          auto it = locals.find(innerTarget.name);
          if (it != locals.end() &&
              resolveReferenceBufferType(it->second.typeName, it->second.typeTemplateArg, elemType)) {
            return true;
          }
        }
        if (resolveIndexedArgsPackReferenceBuffer(innerTarget, elemType)) {
          return true;
        }
      }
      if (isSimpleCallName(target, "buffer") && target.templateArgs.size() == 1) {
        elemType = target.templateArgs.front();
        return true;
      }
      if (isSimpleCallName(target, "upload") && target.args.size() == 1) {
        return lockedState->resolveArrayTarget(target.args.front(), elemType);
      }
    }
    return false;
  };
  state->resolveMapTarget = [=, this](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    keyTypeOut.clear();
    valueTypeOut.clear();
    auto isRootMapAliasPath = [](const std::string &path) {
      return path == "/map" ||
             path.rfind("/map__", 0) == 0;
    };
    auto explicitCallPath = [](const Expr &candidate) -> std::string {
      if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
        return {};
      }
      if (candidate.name.front() == '/') {
        return candidate.name;
      }
      std::string prefix = candidate.namespacePrefix;
      if (!prefix.empty() && prefix.front() != '/') {
        prefix.insert(prefix.begin(), '/');
      }
      if (prefix.empty()) {
        return "/" + candidate.name;
      }
      return prefix + "/" + candidate.name;
    };
    auto hasRootMapDefinitionFamily = [&]() {
      if (defMap_.find("/map") != defMap_.end()) {
        return true;
      }
      for (const auto &[path, defPtr] : defMap_) {
        (void)defPtr;
        if (path.rfind("/map__", 0) == 0) {
          return true;
        }
      }
      return false;
    };
    if (target.kind == Expr::Kind::Call) {
      const std::string resolvedTarget = resolveCalleePath(target);
      const std::string explicitTarget = explicitCallPath(target);
      if (isRootMapAliasPath(resolvedTarget) ||
          isRootMapAliasPath(explicitTarget)) {
        return false;
      }
    }
    BindingInfo binding;
    if (resolveBindingTarget(target, binding) &&
        resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
      return true;
    }
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      const bool skipRootMapAliasInference =
          target.kind == Expr::Kind::Call &&
          ([&]() {
            const std::string resolvedPath = resolveCalleePath(target);
            return resolvedPath == "/map" ||
                   resolvedPath.rfind("/map__", 0) == 0;
          })();
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(inferredTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        binding.typeName = normalizeBindingTypeName(base);
        binding.typeTemplateArg = argText;
      } else {
        binding.typeName = normalizedType;
        binding.typeTemplateArg.clear();
      }
      if (!skipRootMapAliasInference &&
          resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
        return true;
      }
    }
    if (target.kind == Expr::Kind::Call) {
      const bool hasVisibleCanonicalMapConstructor =
          hasVisibleDefinitionPathForCurrentImports("/std/collections/map/map");
      const bool allowRootMapConstructorAlias =
          hasVisibleCanonicalMapConstructor && !hasRootMapDefinitionFamily();
      const std::string resolvedTarget = resolveCalleePath(target);
      if (isDirectMapConstructorPath(resolvedTarget) &&
          target.templateArgs.size() == 2 &&
          (!isRootMapAliasPath(resolvedTarget) || allowRootMapConstructorAlias)) {
        keyTypeOut = target.templateArgs[0];
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      std::string builtinCollectionName;
      if (getBuiltinCollectionName(target, builtinCollectionName) &&
          builtinCollectionName == "map" &&
          target.templateArgs.size() == 2 &&
          (!isRootMapAliasPath(resolvedTarget) || allowRootMapConstructorAlias)) {
        keyTypeOut = target.templateArgs[0];
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      std::string elemType;
      if (lockedState->resolveIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      if (lockedState->resolveWrappedIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      if (lockedState->resolveDereferencedIndexedArgsPackElementType(target, elemType) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyTypeOut, valueTypeOut)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
        const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target);
        if (accessReceiver != nullptr && accessReceiver->kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
            std::string packElemType;
            if (getArgsPackElementType(*paramBinding, packElemType) &&
                extractMapKeyValueTypesFromTypeText(packElemType, keyTypeOut, valueTypeOut)) {
              return true;
            }
          }
          auto it = locals.find(accessReceiver->name);
          if (it != locals.end()) {
            std::string packElemType;
            if (getArgsPackElementType(it->second, packElemType) &&
                extractMapKeyValueTypesFromTypeText(packElemType, keyTypeOut, valueTypeOut)) {
              return true;
            }
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/map") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
          keyTypeOut = args[0];
          valueTypeOut = args[1];
          return true;
        }
        return false;
      }
      if (inferCallBinding(target, binding) &&
          resolveMapBinding(binding, keyTypeOut, valueTypeOut)) {
        return true;
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        if (!returnsMapCollectionType(transform.templateArgs.front())) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(normalizeBindingTypeName(transform.templateArgs.front()), base, arg)) {
          return false;
        }
        while (base == "Reference" || base == "Pointer") {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
            return false;
          }
          if (!splitTemplateTypeName(normalizeBindingTypeName(args.front()), base, arg)) {
            return false;
          }
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 2) {
          return false;
        }
        keyTypeOut = args[0];
        valueTypeOut = args[1];
        return true;
      }
    }
    return false;
  };
  state->resolveExperimentalMapTarget =
      [=, this](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    auto assignBindingFromTypeText = [](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (isDirectMapConstructorCall(target)) {
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) && args.size() == 2) {
        keyTypeOut = args[0];
        valueTypeOut = args[1];
        return true;
      }
    }
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      assignBindingFromTypeText(inferredTypeText, binding);
      if (extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut)) {
        return true;
      }
    }
    return inferCallBinding(target, binding) &&
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  state->resolveExperimentalMapValueTarget =
      [=, this](const Expr &target, std::string &keyTypeOut, std::string &valueTypeOut) -> bool {
    auto extractValueBinding = [&](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
    };
    auto assignBindingFromTypeText = [](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    keyTypeOut.clear();
    valueTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractValueBinding(binding);
    }
    std::string inferredTypeText;
    if (inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      assignBindingFromTypeText(inferredTypeText, binding);
      if (extractValueBinding(binding)) {
        return true;
      }
    }
    return target.kind == Expr::Kind::Call &&
           inferCallBinding(target, binding) &&
           extractValueBinding(binding);
  };
}

} // namespace primec::semantics
