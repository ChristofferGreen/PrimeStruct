#include "SemanticsValidator.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprDispatchBootstrap(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    ExprDispatchBootstrap &bootstrapOut) {
  bootstrapOut = {};
  const auto *paramsPtr = &params;
  const auto *localsPtr = &locals;

  auto resolveFieldBindingTarget = [this, paramsPtr, localsPtr](const Expr &target,
                                                                BindingInfo &bindingOut) -> bool {
    if (!(target.kind == Expr::Kind::Call && target.isFieldAccess &&
          target.args.size() == 1)) {
      return false;
    }
    std::string structPath;
    const Expr &receiver = target.args.front();
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *receiverBinding = findParamBinding(*paramsPtr, receiver.name);
      if (!receiverBinding) {
        auto it = localsPtr->find(receiver.name);
        if (it != localsPtr->end()) {
          receiverBinding = &it->second;
        }
      }
      if (receiverBinding != nullptr) {
        std::string receiverType = receiverBinding->typeName;
        if ((receiverType == "Reference" || receiverType == "Pointer") &&
            !receiverBinding->typeTemplateArg.empty()) {
          receiverType = receiverBinding->typeTemplateArg;
        }
        structPath = resolveStructTypePath(receiverType, receiver.namespacePrefix,
                                           structNames_);
        if (structPath.empty()) {
          auto importIt = importAliases_.find(receiverType);
          if (importIt != importAliases_.end() &&
              structNames_.count(importIt->second) > 0) {
            structPath = importIt->second;
          }
        }
      }
    } else if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
      std::string inferredStruct = inferStructReturnPath(receiver, *paramsPtr, *localsPtr);
      if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
        structPath = inferredStruct;
      } else {
        const std::string resolvedType = resolveCalleePath(receiver);
        if (structNames_.count(resolvedType) > 0) {
          structPath = resolvedType;
        }
      }
    }
    if (structPath.empty()) {
      return false;
    }
    auto defIt = defMap_.find(structPath);
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &fieldStmt : defIt->second->statements) {
      bool isStaticField = false;
      for (const auto &transform : fieldStmt.transforms) {
        if (transform.name == "static") {
          isStaticField = true;
          break;
        }
      }
      if (!fieldStmt.isBinding || isStaticField || fieldStmt.name != target.name) {
        continue;
      }
      return resolveStructFieldBinding(*defIt->second, fieldStmt, bindingOut);
    }
    return false;
  };

  bootstrapOut.dispatchResolverAdapters = {
      .resolveBindingTarget =
          [resolveFieldBindingTarget](const Expr &target, BindingInfo &bindingOut) -> bool {
            return resolveFieldBindingTarget(target, bindingOut);
          },
      .inferCallBinding =
          [this](const Expr &target, BindingInfo &bindingOut) -> bool {
            if (target.kind != Expr::Kind::Call) {
              return false;
            }
            auto defIt = defMap_.find(resolveCalleePath(target));
            return defIt != defMap_.end() && defIt->second != nullptr &&
                   inferDefinitionReturnBinding(*defIt->second, bindingOut);
          }};
  bootstrapOut.dispatchResolvers = makeBuiltinCollectionDispatchResolvers(
      params, locals, bootstrapOut.dispatchResolverAdapters);
  // bootstrapOut.isDeclaredPointerLikeCall = [this](const Expr &candidate) -> bool {
  bootstrapOut.isDeclaredPointerLikeCall =
      [this, paramsPtr, localsPtr](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    auto isPointerLikeBinding = [](const BindingInfo &binding) {
      const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
      return normalizedType == "Pointer" || normalizedType == "Reference";
    };
    auto isImplicitSoaRefMethod = [&]() {
      const std::string normalizedName =
          !candidate.name.empty() && candidate.name.front() == '/'
              ? candidate.name.substr(1)
              : candidate.name;
      if (!candidate.isMethodCall || normalizedName != "ref" ||
          candidate.args.empty()) {
        return false;
      }
        std::string elemType;
        const Expr &receiver = candidate.args.front();
        if (receiver.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding =
                findParamBinding(*paramsPtr, receiver.name)) {
          return extractExperimentalSoaVectorElementType(*paramBinding, elemType);
        }
        auto localIt = localsPtr->find(receiver.name);
        return localIt != localsPtr->end() &&
               extractExperimentalSoaVectorElementType(localIt->second, elemType);
      }
      BindingInfo receiverBinding;
      std::string receiverTypeText;
      if (!inferQueryExprTypeText(receiver,
                                  *paramsPtr,
                                  *localsPtr,
                                  receiverTypeText)) {
        return false;
      }
      std::string base;
      std::string argText;
      const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
      if (splitTemplateTypeName(normalizedType, base, argText)) {
        receiverBinding.typeName = normalizeBindingTypeName(base);
        receiverBinding.typeTemplateArg = argText;
      } else {
        receiverBinding.typeName = normalizedType;
        receiverBinding.typeTemplateArg.clear();
      }
      return extractExperimentalSoaVectorElementType(receiverBinding, elemType);
    };
    if (isImplicitSoaRefMethod()) {
      return true;
    }
    std::string resolvedPath = resolveCalleePath(candidate);
    if (candidate.isMethodCall) {
      if (candidate.args.empty()) {
        return false;
      }
      bool isBuiltin = false;
      if (!resolveMethodTarget(*paramsPtr,
                               *localsPtr,
                               candidate.namespacePrefix,
                               candidate.args.front(),
                               candidate.name,
                               resolvedPath,
                               isBuiltin)) {
        return false;
      }
    }
    resolvedPath =
        resolveExprConcreteCallPath(*paramsPtr,
                                    *localsPtr,
                                    candidate,
                                    resolvedPath);
    BindingInfo inferredReturn;
    if (inferResolvedDirectCallBindingType(resolvedPath, inferredReturn) &&
        isPointerLikeBinding(inferredReturn)) {
      return true;
    }
    if (inferBindingTypeFromInitializer(candidate,
                                        *paramsPtr,
                                        *localsPtr,
                                        inferredReturn) &&
        isPointerLikeBinding(inferredReturn)) {
      return true;
    }
    auto defIt = defMap_.find(resolvedPath);
    return defIt != defMap_.end() && defIt->second != nullptr &&
           inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
           isPointerLikeBinding(inferredReturn);
  };
  auto isRootMapAliasPath = [](const std::string &path) {
    return path == "/map" || path.rfind("/map__", 0) == 0;
  };
  auto explicitCallPath = [&](const Expr &candidate) {
    if (candidate.name.empty() || candidate.name.front() == '/') {
      return candidate.name;
    }
    if (candidate.namespacePrefix.empty()) {
      return "/" + candidate.name;
    }
    return candidate.namespacePrefix + "/" + candidate.name;
  };
  bootstrapOut.resolveMapTarget = [this,
                                   paramsPtr,
                                   localsPtr,
                                   &bootstrapOut,
                                   &isRootMapAliasPath,
                                   &explicitCallPath](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (bootstrapOut.dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
        bootstrapOut.dispatchResolvers.resolveExperimentalMapTarget(target, keyType,
                                                                    valueType)) {
      return true;
    }
    if (target.kind == Expr::Kind::Call &&
        (isRootMapAliasPath(resolveCalleePath(target)) ||
         isRootMapAliasPath(explicitCallPath(target)))) {
      return false;
    }
    std::string inferredTypeText;
    return inferQueryExprTypeText(target, *paramsPtr, *localsPtr, inferredTypeText) &&
           returnsMapCollectionType(inferredTypeText);
  };
}

} // namespace primec::semantics
