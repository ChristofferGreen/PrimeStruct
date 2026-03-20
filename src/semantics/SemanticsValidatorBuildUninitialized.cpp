#include "SemanticsValidator.h"

namespace primec::semantics {

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
    if (getBuiltinPointerName(pointerExpr, pointerBuiltin) && pointerBuiltin == "location" &&
        pointerExpr.args.size() == 1) {
      BindingInfo pointeeBinding;
      bool pointeeResolved = false;
      if (!resolveUninitializedStorageBinding(params,
                                              locals,
                                              pointerExpr.args.front(),
                                              pointeeBinding,
                                              pointeeResolved)) {
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
      const std::string normalizedReturnType =
          normalizeBindingTypeName(transform.templateArgs.front());
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
    if (getBuiltinPointerName(storage, pointerBuiltin) && pointerBuiltin == "dereference" &&
        storage.args.size() == 1) {
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
      if ((receiverType == "Reference" || receiverType == "Pointer") &&
          !receiverBinding->typeTemplateArg.empty()) {
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

}  // namespace primec::semantics
