#include "SemanticsValidator.h"

namespace primec::semantics {

bool SemanticsValidator::resolveStructFieldReceiverPath(const std::vector<ParameterInfo> &params,
                                                        const std::unordered_map<std::string, BindingInfo> &locals,
                                                        const Expr &receiverExpr,
                                                        std::string &structPathOut) {
  auto resolveStructPathFromType = [&](const std::string &typeName,
                                       const std::string &namespacePrefix,
                                       std::string &resolvedStructPathOut) -> bool {
      if (typeName.empty() || isPrimitiveBindingTypeName(typeName)) {
        return false;
      }
      if (!typeName.empty() && typeName[0] == '/') {
        if (structNames_.count(typeName) > 0) {
          resolvedStructPathOut = typeName;
          return true;
        }
        return false;
      }
      std::string current = namespacePrefix;
      while (true) {
        if (!current.empty()) {
          std::string scoped = current + "/" + typeName;
          if (structNames_.count(scoped) > 0) {
            resolvedStructPathOut = scoped;
            return true;
          }
          if (current.size() > typeName.size()) {
            const size_t start = current.size() - typeName.size();
            if (start > 0 && current[start - 1] == '/' &&
                current.compare(start, typeName.size(), typeName) == 0 &&
                structNames_.count(current) > 0) {
              resolvedStructPathOut = current;
              return true;
            }
          }
        } else {
          std::string root = "/" + typeName;
          if (structNames_.count(root) > 0) {
            resolvedStructPathOut = root;
            return true;
          }
        }
        if (current.empty()) {
          break;
        }
        const size_t slash = current.find_last_of('/');
        if (slash == std::string::npos || slash == 0) {
          current.clear();
        } else {
          current.erase(slash);
        }
      }
      auto importIt = importAliases_.find(typeName);
      if (importIt != importAliases_.end() && structNames_.count(importIt->second) > 0) {
        resolvedStructPathOut = importIt->second;
        return true;
      }
      return false;
  };

  structPathOut.clear();
  if (receiverExpr.kind == Expr::Kind::Name) {
    const BindingInfo *binding = findParamBinding(params, receiverExpr.name);
    if (!binding) {
      auto it = locals.find(receiverExpr.name);
      if (it != locals.end()) {
        binding = &it->second;
      }
    }
    if (binding) {
      std::string typeName = binding->typeName;
      if ((typeName == "Reference" || typeName == "Pointer") && !binding->typeTemplateArg.empty()) {
        typeName = binding->typeTemplateArg;
      }
      (void)resolveStructPathFromType(typeName, receiverExpr.namespacePrefix, structPathOut);
    } else {
      (void)resolveStructPathFromType(receiverExpr.name, receiverExpr.namespacePrefix, structPathOut);
    }
  } else if (receiverExpr.kind == Expr::Kind::Call && !receiverExpr.isBinding) {
    std::string accessName;
    if (getBuiltinArrayAccessName(receiverExpr, accessName) && receiverExpr.args.size() == 2) {
      std::string elemType;
      if (resolveArgsPackAccessTarget(receiverExpr.args.front(), elemType) ||
          resolveArrayTarget(receiverExpr.args.front(), elemType) ||
          resolveVectorTarget(receiverExpr.args.front(), elemType)) {
        const std::string unwrappedElemType = unwrapReferencePointerTypeText(elemType);
        (void)resolveStructPathFromType(unwrappedElemType, receiverExpr.namespacePrefix, structPathOut);
      }
    }
    std::string inferredStruct = inferStructReturnPath(receiverExpr, params, locals);
    if (!inferredStruct.empty() && structNames_.count(inferredStruct) > 0) {
      structPathOut = inferredStruct;
    } else {
      std::string resolvedType = resolveCalleePath(receiverExpr);
      if (structNames_.count(resolvedType) > 0) {
        structPathOut = resolvedType;
      }
    }
  }
  return !structPathOut.empty();
}

bool SemanticsValidator::isTypeNamespaceFieldReceiver(const std::vector<ParameterInfo> &params,
                                                      const std::unordered_map<std::string, BindingInfo> &locals,
                                                      const Expr &receiverExpr,
                                                      std::string &structPathOut) {
  structPathOut.clear();
  if (receiverExpr.kind != Expr::Kind::Name) {
    return false;
  }
  if (findParamBinding(params, receiverExpr.name) != nullptr || locals.find(receiverExpr.name) != locals.end()) {
    return false;
  }
  return resolveStructFieldReceiverPath(params, locals, receiverExpr, structPathOut);
}

bool SemanticsValidator::resolveStructFieldBinding(const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                                   const Expr &receiver,
                                                   const std::string &fieldName,
                                                   BindingInfo &bindingOut) {
  auto inferStructFieldBinding = [&](const Definition &def,
                                     const std::string &bindingFieldName,
                                     BindingInfo &fieldBindingOut,
                                     bool allowStatic,
                                     bool allowPrivate) -> bool {
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    auto isPrivateField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "private") {
          return true;
        }
      }
      return false;
    };
    for (const auto &stmt : def.statements) {
      if (!stmt.isBinding) {
        error_ = "struct definitions may only contain field bindings: " + def.fullPath;
        return false;
      }
      if (stmt.name != bindingFieldName) {
        continue;
      }
      if (isStaticField(stmt) && !allowStatic) {
        return false;
      }
      if (isPrivateField(stmt) && !allowPrivate) {
        error_ = "private field is not accessible: " + def.fullPath + "/" + bindingFieldName;
        return false;
      }
      BindingInfo fieldBinding;
      if (!SemanticsValidator::resolveStructFieldBinding(def, stmt, fieldBinding)) {
        return false;
      }
      fieldBindingOut = std::move(fieldBinding);
      return true;
    }
    return false;
  };

  std::string structPath;
  const bool isTypeReceiver = isTypeNamespaceFieldReceiver(params, locals, receiver, structPath);
  if (!isTypeReceiver && !resolveStructFieldReceiverPath(params, locals, receiver, structPath)) {
    return false;
  }
  auto defIt = defMap_.find(structPath);
  if (defIt == defMap_.end() || !defIt->second) {
    return false;
  }
  if (isTypeReceiver) {
    auto isStaticField = [](const Expr &stmt) {
      for (const auto &transform : stmt.transforms) {
        if (transform.name == "static") {
          return true;
        }
      }
      return false;
    };
    for (const auto &stmt : defIt->second->statements) {
      if (!stmt.isBinding || !isStaticField(stmt) || stmt.name != fieldName) {
        continue;
      }
      return SemanticsValidator::resolveStructFieldBinding(*defIt->second, stmt, bindingOut);
    }
    return false;
  }
  BindingInfo inferred;
  if (!inferStructFieldBinding(*defIt->second, fieldName, inferred, false, true)) {
    return false;
  }
  bindingOut = std::move(inferred);
  return true;
}

bool SemanticsValidator::isTypeNamespaceMethodCall(const std::vector<ParameterInfo> &params,
                                                   const std::unordered_map<std::string, BindingInfo> &locals,
                                                   const Expr &callExpr,
                                                   const std::string &resolvedPath) const {
  if (!callExpr.isMethodCall || callExpr.args.empty() || resolvedPath.empty()) {
    return false;
  }
  const Expr &receiver = callExpr.args.front();
  if (receiver.kind != Expr::Kind::Name) {
    return false;
  }
  if (findParamBinding(params, receiver.name) != nullptr || locals.find(receiver.name) != locals.end()) {
    return false;
  }
  const size_t methodSlash = resolvedPath.find_last_of('/');
  if (methodSlash == std::string::npos || methodSlash == 0) {
    return false;
  }
  const std::string receiverPath = resolvedPath.substr(0, methodSlash);
  const size_t receiverSlash = receiverPath.find_last_of('/');
  const std::string receiverTypeName =
      receiverSlash == std::string::npos ? receiverPath : receiverPath.substr(receiverSlash + 1);
  return receiverTypeName == receiver.name;
}

std::string SemanticsValidator::describeMethodReflectionTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &callExpr) const {
  auto isUnboundMetaReceiver = [&](const Expr &receiver) {
    if (receiver.kind != Expr::Kind::Name || receiver.name != "meta") {
      return false;
    }
    if (findParamBinding(params, receiver.name) != nullptr) {
      return false;
    }
    return locals.find(receiver.name) == locals.end();
  };

  if (!callExpr.isMethodCall || callExpr.args.empty()) {
    return "";
  }
  const Expr &receiver = callExpr.args.front();
  if (!isUnboundMetaReceiver(receiver)) {
    return "";
  }
  return "meta." + callExpr.name;
}

} // namespace primec::semantics
