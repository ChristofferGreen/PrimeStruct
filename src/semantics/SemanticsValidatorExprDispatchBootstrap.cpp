#include "SemanticsValidator.h"

namespace primec::semantics {

void SemanticsValidator::prepareExprDispatchBootstrap(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    ExprDispatchBootstrap &bootstrapOut) {
  bootstrapOut = {};
  const auto paramsCopy = params;
  const auto localsCopy = locals;

  auto resolveFieldBindingTarget = [this, paramsCopy, localsCopy](
                                       const Expr &target,
                                       BindingInfo &bindingOut) -> bool {
    if (!(target.kind == Expr::Kind::Call && target.isFieldAccess &&
          target.args.size() == 1)) {
      return false;
    }
    std::string structPath;
    const Expr &receiver = target.args.front();
    if (receiver.kind == Expr::Kind::Name) {
      const BindingInfo *receiverBinding =
          findParamBinding(paramsCopy, receiver.name);
      if (!receiverBinding) {
        auto it = localsCopy.find(receiver.name);
        if (it != localsCopy.end()) {
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
      std::string inferredStruct =
          inferStructReturnPath(receiver, paramsCopy, localsCopy);
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
      .resolveBindingTarget = resolveFieldBindingTarget,
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
  bootstrapOut.isDeclaredPointerLikeCall = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(candidate));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    BindingInfo inferredReturn;
    if (!inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      return false;
    }
    return inferredReturn.typeName == "Pointer" ||
           inferredReturn.typeName == "Reference";
  };
  bootstrapOut.resolveMapTarget = [&](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (bootstrapOut.dispatchResolvers.resolveMapTarget(target, keyType, valueType) ||
        bootstrapOut.dispatchResolvers.resolveExperimentalMapTarget(target, keyType,
                                                                    valueType)) {
      return true;
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(target));
    if ((defIt == defMap_.end() || defIt->second == nullptr) &&
        !target.name.empty() && target.name.find('/') == std::string::npos) {
      defIt = defMap_.find("/" + target.name);
    }
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    BindingInfo inferredReturn;
    if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
      const std::string inferredTypeText =
          inferredReturn.typeTemplateArg.empty()
              ? inferredReturn.typeName
              : inferredReturn.typeName + "<" + inferredReturn.typeTemplateArg + ">";
      if (returnsMapCollectionType(inferredTypeText)) {
        return true;
      }
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name == "return" && transform.templateArgs.size() == 1 &&
          returnsMapCollectionType(transform.templateArgs.front())) {
        return true;
      }
    }
    return false;
  };
}

} // namespace primec::semantics
