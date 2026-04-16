#include "SemanticsValidator.h"

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

namespace primec::semantics {

SemanticsValidator::BuiltinCollectionDispatchResolvers SemanticsValidator::makeBuiltinCollectionDispatchResolvers(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const BuiltinCollectionDispatchResolverAdapters &adapters) {
  auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {
    pointeeTypeOut.clear();
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base != "Reference" && base != "Pointer") {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    pointeeTypeOut = args.front();
    return !pointeeTypeOut.empty();
  };
  auto extractCollectionElementType = [&](const std::string &typeText,
                                          const std::string &expectedBase,
                                          std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(normalizeBindingTypeName(typeText), base, argText)) {
      return false;
    }
    base = normalizeBindingTypeName(base);
    if (base != expectedBase) {
      return false;
    }
    std::vector<std::string> args;
    if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
      return false;
    }
    elemTypeOut = args.front();
    return true;
  };
  auto extractExperimentalMapFieldTypes = [this](const BindingInfo &binding,
                                                 std::string &keyTypeOut,
                                                 std::string &valueTypeOut) -> bool {
    auto extractFromTypeText = [&](std::string normalizedType) -> bool {
      while (true) {
        std::string base;
        std::string argText;
        if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
          base = normalizeBindingTypeName(base);
          if (base == "Reference" || base == "Pointer") {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
              return false;
            }
            normalizedType = normalizeBindingTypeName(args.front());
            continue;
          }
          std::string normalizedBase = base;
          if (!normalizedBase.empty() && normalizedBase.front() == '/') {
            normalizedBase.erase(normalizedBase.begin());
          }
          if ((normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map")) {
            std::vector<std::string> args;
            if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 2) {
              return false;
            }
            keyTypeOut = args[0];
            valueTypeOut = args[1];
            return true;
          }
        }

        std::string resolvedPath = normalizedType;
        if (!resolvedPath.empty() && resolvedPath.front() != '/') {
          resolvedPath.insert(resolvedPath.begin(), '/');
        }
        std::string normalizedResolvedPath = normalizeBindingTypeName(resolvedPath);
        if (!normalizedResolvedPath.empty() && normalizedResolvedPath.front() == '/') {
          normalizedResolvedPath.erase(normalizedResolvedPath.begin());
        }
        if (normalizedResolvedPath.rfind("std/collections/experimental_map/Map__", 0) != 0) {
          return false;
        }
        auto defIt = defMap_.find(resolvedPath);
        if (defIt == defMap_.end() || !defIt->second) {
          return false;
        }
        std::string keysElementType;
        std::string payloadsElementType;
        for (const auto &fieldExpr : defIt->second->statements) {
          if (!fieldExpr.isBinding) {
            continue;
          }
          BindingInfo fieldBinding;
          std::optional<std::string> restrictType;
          std::string parseError;
          if (!parseBindingInfo(fieldExpr,
                                defIt->second->namespacePrefix,
                                structNames_,
                                importAliases_,
                                fieldBinding,
                                restrictType,
                                parseError)) {
            continue;
          }
          std::string vectorElementType;
          if (normalizeBindingTypeName(fieldBinding.typeName) == "vector" &&
              !fieldBinding.typeTemplateArg.empty()) {
            std::vector<std::string> fieldArgs;
            if (!splitTopLevelTemplateArgs(fieldBinding.typeTemplateArg, fieldArgs) || fieldArgs.size() != 1) {
              continue;
            }
            vectorElementType = fieldArgs.front();
          } else if (!extractExperimentalVectorElementType(fieldBinding, vectorElementType)) {
            continue;
          }
          if (fieldExpr.name == "keys") {
            keysElementType = vectorElementType;
          } else if (fieldExpr.name == "payloads") {
            payloadsElementType = vectorElementType;
          }
        }
        if (keysElementType.empty() || payloadsElementType.empty()) {
          return false;
        }
        keyTypeOut = keysElementType;
        valueTypeOut = payloadsElementType;
        return true;
      }
    };

    keyTypeOut.clear();
    valueTypeOut.clear();
    if (binding.typeTemplateArg.empty()) {
      return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
    }
    return extractFromTypeText(normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
  };
  auto extractAnyMapKeyValueTypes = [extractExperimentalMapFieldTypes](const BindingInfo &binding,
                                                                       std::string &keyTypeOut,
                                                                       std::string &valueTypeOut) -> bool {
    return extractMapKeyValueTypes(binding, keyTypeOut, valueTypeOut) ||
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  auto resolveBindingTarget = [=, this](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        bindingOut = it->second;
        return true;
      }
    }
    return adapters.resolveBindingTarget != nullptr &&
           adapters.resolveBindingTarget(target, bindingOut);
  };
  auto resolveMethodOwnerPath = [=, this](const std::string &typeText,
                                          const std::string &typeNamespace) -> std::string {
    std::string normalizedType = normalizeBindingTypeName(unwrapReferencePointerTypeText(typeText));
    if (normalizedType.empty()) {
      return {};
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if (!normalizedBase.empty()) {
        normalizedType = normalizedBase;
      }
    }
    if (normalizedType.empty() || !normalizeCollectionTypePath(normalizedType).empty()) {
      return {};
    }
    if (normalizedType.front() == '/') {
      if (structNames_.count(normalizedType) > 0 || defMap_.count(normalizedType) > 0) {
        return normalizedType;
      }
    } else {
      const std::string rootPath = "/" + normalizedType;
      if (structNames_.count(rootPath) > 0 || defMap_.count(rootPath) > 0) {
        return rootPath;
      }
      auto importIt = importAliases_.find(normalizedType);
      if (importIt != importAliases_.end()) {
        return importIt->second;
      }
    }
    std::string resolvedType = resolveStructTypePath(normalizedType, typeNamespace, structNames_);
    if (resolvedType.empty()) {
      resolvedType = resolveTypePath(normalizedType, typeNamespace);
    }
    return resolvedType;
  };
  auto inferCallBinding = [=, this](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (adapters.inferCallBinding != nullptr && adapters.inferCallBinding(target, bindingOut)) {
      return true;
    }
    auto inferResolvedDefinitionBinding = [&](const std::string &resolvedPath) -> bool {
      auto defIt = defMap_.find(resolvedPath);
      return defIt != defMap_.end() && defIt->second != nullptr &&
             inferDefinitionReturnBinding(*defIt->second, bindingOut);
    };
    if (inferResolvedDefinitionBinding(resolveCalleePath(target))) {
      return true;
    }
    if (!target.isMethodCall || target.args.empty() || target.name.empty()) {
      return false;
    }
    BindingInfo receiverBinding;
    if (!resolveBindingTarget(target.args.front(), receiverBinding)) {
      return false;
    }
    std::string methodName = target.name;
    if (!methodName.empty() && methodName.front() == '/') {
      methodName.erase(methodName.begin());
    }
    if (methodName.empty()) {
      return false;
    }
    std::string resolvedMethodTarget;
    bool isBuiltinMethod = false;
    if (resolveMethodTarget(params,
                            locals,
                            target.namespacePrefix,
                            target.args.front(),
                            target.name,
                            resolvedMethodTarget,
                            isBuiltinMethod) &&
        inferResolvedDefinitionBinding(resolvedMethodTarget)) {
      return true;
    }
    const std::string ownerPath =
        resolveMethodOwnerPath(bindingTypeText(receiverBinding), target.args.front().namespacePrefix);
    return !ownerPath.empty() && inferResolvedDefinitionBinding(ownerPath + "/" + methodName);
  };
  auto resolveArrayLikeBinding = [](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    auto resolveReference = [&](const BindingInfo &candidate) -> bool {
      if (candidate.typeName != "Reference" || candidate.typeTemplateArg.empty()) {
        return false;
      }
      std::string base;
      std::string arg;
      if (!splitTemplateTypeName(candidate.typeTemplateArg, base, arg) ||
          normalizeBindingTypeName(base) != "array") {
        return false;
      }
      std::vector<std::string> args;
      if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
        return false;
      }
      elemTypeOut = args.front();
      return true;
    };
    if (resolveReference(binding)) {
      return true;
    }
    if ((binding.typeName != "array" && binding.typeName != "vector") ||
        binding.typeTemplateArg.empty()) {
      return false;
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveVectorBinding = [this](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (binding.typeName != "vector" || binding.typeTemplateArg.empty()) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalVectorElementType(binding, elemTypeOut);
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveSoaVectorBinding = [this](const BindingInfo &binding, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (binding.typeName != "soa_vector" || binding.typeTemplateArg.empty()) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalSoaVectorElementType(binding, elemTypeOut);
    }
    elemTypeOut = binding.typeTemplateArg;
    return true;
  };
  auto resolveMapBinding = [extractAnyMapKeyValueTypes](const BindingInfo &binding,
                                                        std::string &keyTypeOut,
                                                        std::string &valueTypeOut) -> bool {
    return extractAnyMapKeyValueTypes(binding, keyTypeOut, valueTypeOut);
  };
  auto isDirectMapConstructorCall = [this](const Expr &candidate) {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.isMethodCall) {
      return false;
    }
    const std::string resolvedCandidate = resolveCalleePath(candidate);
    auto matchesPath = [&](std::string_view basePath) {
      return resolvedCandidate == basePath || resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
    };
    return matchesPath("/std/collections/map/map") ||
           matchesPath("/std/collections/mapNew") ||
           matchesPath("/std/collections/mapSingle") ||
           matchesPath("/std/collections/mapDouble") ||
           matchesPath("/std/collections/mapPair") ||
           matchesPath("/std/collections/mapTriple") ||
           matchesPath("/std/collections/mapQuad") ||
           matchesPath("/std/collections/mapQuint") ||
           matchesPath("/std/collections/mapSext") ||
           matchesPath("/std/collections/mapSept") ||
           matchesPath("/std/collections/mapOct") ||
           matchesPath("/std/collections/experimental_map/mapNew") ||
           matchesPath("/std/collections/experimental_map/mapSingle") ||
           matchesPath("/std/collections/experimental_map/mapDouble") ||
           matchesPath("/std/collections/experimental_map/mapPair") ||
           matchesPath("/std/collections/experimental_map/mapTriple") ||
           matchesPath("/std/collections/experimental_map/mapQuad") ||
           matchesPath("/std/collections/experimental_map/mapQuint") ||
           matchesPath("/std/collections/experimental_map/mapSext") ||
           matchesPath("/std/collections/experimental_map/mapSept") ||
           matchesPath("/std/collections/experimental_map/mapOct");
  };

  auto state = std::make_shared<BuiltinCollectionDispatchResolvers>();
  const std::weak_ptr<BuiltinCollectionDispatchResolvers> weakState = state;

  state->resolveArgsPackAccessTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    elemType.clear();
    auto resolveBinding = [&](const BindingInfo &binding) { return getArgsPackElementType(binding, elemType); };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return resolveBinding(*paramBinding);
      }
      auto it = locals.find(target.name);
      if (it != locals.end()) {
        return resolveBinding(it->second);
      }
    }
    return false;
  };
  state->resolveIndexedArgsPackElementType = [=, this](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string accessName;
    if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) || target.args.size() != 2) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(target);
    if (accessReceiver == nullptr || accessReceiver->kind != Expr::Kind::Name) {
      return false;
    }
    auto resolveBinding = [&](const BindingInfo &binding) { return getArgsPackElementType(binding, elemTypeOut); };
    if (const BindingInfo *paramBinding = findParamBinding(params, accessReceiver->name)) {
      return resolveBinding(*paramBinding);
    }
    auto it = locals.find(accessReceiver->name);
    return it != locals.end() && resolveBinding(it->second);
  };
  state->resolveDereferencedIndexedArgsPackElementType = [=](const Expr &target, std::string &elemTypeOut) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    elemTypeOut.clear();
    if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
      return false;
    }
    std::string wrappedType;
    if (!lockedState->resolveIndexedArgsPackElementType(target.args.front(), wrappedType)) {
      return false;
    }
    return extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  state->resolveWrappedIndexedArgsPackElementType = [=](const Expr &target, std::string &elemTypeOut) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    elemTypeOut.clear();
    std::string wrappedType;
    if (!lockedState->resolveIndexedArgsPackElementType(target, wrappedType)) {
      return false;
    }
    return extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  state->resolveArrayTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveArrayLikeBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (lockedState->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      if (lockedState->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      if (lockedState->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
        std::vector<std::string> args;
        const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
        if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, args) && args.size() == 1) {
          elemType = args.front();
          return true;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(target, collectionName) &&
            (collectionName == "array" || collectionName == "vector") &&
            target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
        return false;
      }
    }
    return false;
  };
  state->resolveVectorTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return resolveVectorBinding(binding, elemType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (lockedState->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      if (lockedState->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      if (lockedState->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "vector", params, locals, args) && args.size() == 1) {
          elemType = args.front();
          return true;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(target, collectionName) &&
            collectionName == "vector" &&
            target.templateArgs.size() == 1) {
          elemType = target.templateArgs.front();
          return true;
        }
        return false;
      }
      const std::string resolvedSoaToAosCanonical =
          canonicalizeLegacySoaToAosHelperPath(resolveCalleePath(target));
      const bool matchesSoaToAosTarget =
          (!target.isMethodCall && isSimpleCallName(target, "to_aos")) ||
          isLegacyOrCanonicalSoaHelperPath(
              resolvedSoaToAosCanonical, "to_aos");
      const bool matchesBorrowedSoaToAosTarget =
          (!target.isMethodCall && isSimpleCallName(target, "to_aos_ref")) ||
          isLegacyOrCanonicalSoaHelperPath(
              resolvedSoaToAosCanonical, "to_aos_ref");
      if ((matchesSoaToAosTarget || matchesBorrowedSoaToAosTarget) &&
          target.args.size() == 1) {
        return lockedState->resolveSoaVectorTarget(target.args.front(), elemType);
      }
    }
    if (inferCallBinding(target, binding)) {
      return resolveVectorBinding(binding, elemType);
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return resolveVectorBinding(inferredBinding, elemType);
  };
  state->resolveExperimentalVectorTarget =
      [=, this](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractExperimentalVectorElementType(binding, elemTypeOut);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (inferCallBinding(target, binding) &&
        extractExperimentalVectorElementType(binding, elemTypeOut)) {
      return true;
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return extractExperimentalVectorElementType(inferredBinding, elemTypeOut);
  };
  state->resolveExperimentalVectorValueTarget =
      [=, this](const Expr &target, std::string &elemTypeOut) -> bool {
    auto extractValueBinding = [&](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      if (normalizedType == "Reference" || normalizedType == "Pointer") {
        return false;
      }
      return extractExperimentalVectorElementType(binding, elemTypeOut);
    };
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    elemTypeOut.clear();
    BindingInfo binding;
    if (resolveBindingTarget(target, binding)) {
      return extractValueBinding(binding);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    if (inferCallBinding(target, binding) &&
        extractValueBinding(binding)) {
      return true;
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return extractValueBinding(inferredBinding);
  };
  state->resolveSoaVectorTarget = [=, this](const Expr &target, std::string &elemType) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    auto extractBindingFromTypeText = [&](const std::string &typeText, BindingInfo &bindingOut) {
      const std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        bindingOut.typeName = normalizeBindingTypeName(base);
        bindingOut.typeTemplateArg = argText;
      } else {
        bindingOut.typeName = normalizedType;
        bindingOut.typeTemplateArg.clear();
      }
    };
    auto resolveDirectReceiver = [&](const Expr &candidate,
                                     std::string &elemTypeOut) -> bool {
      BindingInfo binding;
      return resolveBindingTarget(candidate, binding) &&
             resolveSoaVectorBinding(binding, elemTypeOut);
    };
    if (this->resolveSoaVectorOrExperimentalBorrowedReceiver(
            target, params, locals, resolveDirectReceiver, elemType)) {
      return true;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if (lockedState->resolveIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      if (lockedState->resolveWrappedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      if (lockedState->resolveDereferencedIndexedArgsPackElementType(target, indexedElemType) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/soa_vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "soa_vector", params, locals, args) && args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
      if (((!target.isMethodCall && isSimpleCallName(target, "to_soa")) ||
           resolveCalleePath(target) == "/to_soa") &&
          target.args.size() == 1) {
        return lockedState->resolveVectorTarget(target.args.front(), elemType);
      }
    }
    std::string inferredTypeText;
    if (!inferQueryExprTypeText(target, params, locals, inferredTypeText)) {
      return false;
    }
    BindingInfo inferredBinding;
    extractBindingFromTypeText(inferredTypeText, inferredBinding);
    return resolveSoaVectorBinding(inferredBinding, elemType);
  };
  populateBuiltinCollectionDispatchBufferAndMapResolvers(
      params,
      locals,
      state,
      resolveBindingTarget,
      inferCallBinding,
      resolveMapBinding,
      extractExperimentalMapFieldTypes,
      isDirectMapConstructorCall);
  const auto isDirectCanonicalVectorAccessCallOnBuiltinReceiver =
      [=](const Expr &candidate, size_t &receiverIndexOut) -> bool {
    const std::shared_ptr<BuiltinCollectionDispatchResolvers> lockedState = weakState.lock();
    if (!lockedState) {
      return false;
    }
    receiverIndexOut = 0;
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return false;
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    if (normalized != "std/collections/vector/at" && normalized != "std/collections/vector/at_unsafe") {
      return false;
    }
    if (candidate.args.empty()) {
      return false;
    }
    if (hasNamedArguments(candidate.argNames)) {
      bool foundValues = false;
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndexOut = i;
          foundValues = true;
          break;
        }
      }
      if (!foundValues) {
        receiverIndexOut = 0;
      }
    }
    if (receiverIndexOut >= candidate.args.size()) {
      return false;
    }
    std::string elemType;
    return lockedState->resolveArgsPackAccessTarget(candidate.args[receiverIndexOut], elemType) ||
           lockedState->resolveVectorTarget(candidate.args[receiverIndexOut], elemType) ||
           lockedState->resolveArrayTarget(candidate.args[receiverIndexOut], elemType) ||
           lockedState->resolveStringTarget(candidate.args[receiverIndexOut]);
  };
  populateBuiltinCollectionDispatchStringResolver(
      params,
      locals,
      state,
      resolveBindingTarget,
      isDirectCanonicalVectorAccessCallOnBuiltinReceiver);

  BuiltinCollectionDispatchResolvers resolvers{
      state->resolveIndexedArgsPackElementType,
      state->resolveDereferencedIndexedArgsPackElementType,
      state->resolveWrappedIndexedArgsPackElementType,
      state->resolveArgsPackAccessTarget,
      state->resolveArrayTarget,
      state->resolveVectorTarget,
      state->resolveExperimentalVectorTarget,
      state->resolveExperimentalVectorValueTarget,
      state->resolveSoaVectorTarget,
      state->resolveBufferTarget,
      state->resolveStringTarget,
      state->resolveMapTarget,
      state->resolveExperimentalMapTarget,
      state->resolveExperimentalMapValueTarget,
      nullptr};
  resolvers.resolverStateKeepAlive = state;
  return resolvers;
}

} // namespace primec::semantics
