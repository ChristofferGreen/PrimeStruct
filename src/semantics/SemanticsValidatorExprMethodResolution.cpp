#include "SemanticsValidator.h"

#include <string>
#include <string_view>

namespace primec::semantics {
namespace {

bool isRemovedVectorCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "capacity" || helperName == "at" ||
         helperName == "at_unsafe" || helperName == "push" || helperName == "pop" ||
         helperName == "reserve" || helperName == "clear" || helperName == "remove_at" ||
         helperName == "remove_swap";
}

bool isRemovedMapCompatibilityHelper(std::string_view helperName) {
  return helperName == "count" || helperName == "at" || helperName == "at_unsafe";
}

bool isFileMethodName(std::string_view methodName) {
  return methodName == "write" || methodName == "write_line" || methodName == "write_byte" ||
         methodName == "read_byte" || methodName == "write_bytes" || methodName == "flush" ||
         methodName == "close";
}

} // namespace

bool SemanticsValidator::isStaticHelperDefinition(const Definition &def) const {
  for (const auto &transform : def.transforms) {
    if (transform.name == "static") {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::hasDeclaredDefinitionPath(const std::string &path) const {
  for (const auto &def : program_.definitions) {
    if (def.fullPath == path) {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::resolveMethodTarget(const std::vector<ParameterInfo> &params,
                                             const std::unordered_map<std::string, BindingInfo> &locals,
                                             const std::string &callNamespacePrefix,
                                             const Expr &receiver,
                                             const std::string &methodName,
                                             std::string &resolvedOut,
                                             bool &isBuiltinOut) {
  isBuiltinOut = false;
  auto explicitRemovedCollectionMethodPath = [&](const std::string &rawMethodName) -> std::string {
    std::string candidate = rawMethodName;
    if (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    std::string normalizedPrefix = callNamespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    std::string_view helperName;
    bool isStdNamespacedVectorHelper = false;
    std::string compatibilityCollection;
    if (normalizedPrefix == "array") {
      helperName = candidate;
      compatibilityCollection = "array";
    } else if (normalizedPrefix == "vector") {
      helperName = candidate;
      compatibilityCollection = "vector";
    } else if (normalizedPrefix == "std/collections/vector") {
      helperName = candidate;
      isStdNamespacedVectorHelper = true;
      compatibilityCollection = "vector";
    } else if (normalizedPrefix == "map") {
      helperName = candidate;
      compatibilityCollection = "map";
    } else if (normalizedPrefix == "std/collections/map") {
      helperName = candidate;
      compatibilityCollection = "map";
    } else if (candidate.rfind("array/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("array/").size());
      compatibilityCollection = "array";
    } else if (candidate.rfind("vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("vector/").size());
      compatibilityCollection = "vector";
    } else if (candidate.rfind("std/collections/vector/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/vector/").size());
      isStdNamespacedVectorHelper = true;
      compatibilityCollection = "vector";
    } else if (candidate.rfind("map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("map/").size());
      compatibilityCollection = "map";
    } else if (candidate.rfind("std/collections/map/", 0) == 0) {
      helperName = std::string_view(candidate).substr(std::string_view("std/collections/map/").size());
      compatibilityCollection = "map";
    }
    if (helperName.empty()) {
      return "";
    }
    if (compatibilityCollection == "map") {
      if (normalizedPrefix == "std/collections/map" ||
          candidate.rfind("std/collections/map/", 0) == 0) {
        return "";
      }
      if (!isRemovedMapCompatibilityHelper(helperName)) {
        return "";
      }
      return "/map/" + std::string(helperName);
    }
    if (!isRemovedVectorCompatibilityHelper(helperName)) {
      return "";
    }
    if (isStdNamespacedVectorHelper) {
      return "/std/collections/vector/" + std::string(helperName);
    }
    return "/" + candidate;
  };

  const std::string explicitRemovedMethodPath = explicitRemovedCollectionMethodPath(methodName);
  auto explicitVectorMethodPath = [&](const std::string &rawMethodName) -> std::string {
    std::string candidate = rawMethodName;
    if (!candidate.empty() && candidate.front() == '/') {
      candidate.erase(candidate.begin());
    }
    std::string normalizedPrefix = callNamespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    if (normalizedPrefix == "vector" || normalizedPrefix == "std/collections/vector") {
      return "/" + normalizedPrefix + "/" + candidate;
    }
    if (candidate.rfind("vector/", 0) == 0 || candidate.rfind("std/collections/vector/", 0) == 0) {
      return "/" + candidate;
    }
    return "";
  };
  const std::string explicitVectorHelperPath = explicitVectorMethodPath(methodName);
  std::string normalizedMethodName = methodName;
  if (!normalizedMethodName.empty() && normalizedMethodName.front() == '/') {
    normalizedMethodName.erase(normalizedMethodName.begin());
  }
  if (normalizedMethodName.rfind("vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("vector/").size());
  } else if (normalizedMethodName.rfind("array/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("array/").size());
  } else if (normalizedMethodName.rfind("std/collections/vector/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/vector/").size());
  } else if (normalizedMethodName.rfind("map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("map/").size());
  } else if (normalizedMethodName.rfind("std/collections/map/", 0) == 0) {
    normalizedMethodName = normalizedMethodName.substr(std::string("std/collections/map/").size());
  }

  auto isStaticBinding = [&](const Expr &bindingExpr) -> bool {
    for (const auto &transform : bindingExpr.transforms) {
      if (transform.name == "static") {
        return true;
      }
    }
    return false;
  };
  auto resolveStructTypePath = [&](const std::string &typeName,
                                   const std::string &namespacePrefix) -> std::string {
    if (typeName.empty()) {
      return "";
    }
    if (!typeName.empty() && typeName[0] == '/') {
      return typeName;
    }
    std::string current = namespacePrefix;
    while (true) {
      if (!current.empty()) {
        std::string scoped = current + "/" + typeName;
        if (structNames_.count(scoped) > 0) {
          return scoped;
        }
        if (current.size() > typeName.size()) {
          const size_t start = current.size() - typeName.size();
          if (start > 0 && current[start - 1] == '/' &&
              current.compare(start, typeName.size(), typeName) == 0 &&
              structNames_.count(current) > 0) {
            return current;
          }
        }
      } else {
        std::string root = "/" + typeName;
        if (structNames_.count(root) > 0) {
          return root;
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
    if (importIt != importAliases_.end()) {
      return importIt->second;
    }
    return "";
  };
  auto resolveExperimentalVectorValueTarget = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
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
    BindingInfo binding;
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return extractValueBinding(*paramBinding);
      }
      if (auto it = locals.find(target.name); it != locals.end()) {
        return extractValueBinding(it->second);
      }
    }
    if (target.kind == Expr::Kind::Call) {
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt != defMap_.end() && defIt->second != nullptr &&
          inferDefinitionReturnBinding(*defIt->second, binding) &&
          extractValueBinding(binding)) {
        return true;
      }
      std::string receiverTypeText;
      if (inferQueryExprTypeText(target, params, locals, receiverTypeText)) {
        extractBindingFromTypeText(receiverTypeText, binding);
        return extractValueBinding(binding);
      }
      const std::string inferredStructPath = inferStructReturnPath(target, params, locals);
      if (!inferredStructPath.empty()) {
        binding.typeName = inferredStructPath;
        binding.typeTemplateArg.clear();
        return extractValueBinding(binding);
      }
    }
    return false;
  };

  if (normalizedMethodName == "ok" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/ok";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "error" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/error";
    isBuiltinOut = true;
    return true;
  }
  if (normalizedMethodName == "why" && receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/why";
    isBuiltinOut = true;
    return true;
  }
  if ((normalizedMethodName == "map" || normalizedMethodName == "and_then" || normalizedMethodName == "map2") &&
      receiver.kind == Expr::Kind::Name && receiver.name == "Result") {
    resolvedOut = "/result/" + normalizedMethodName;
    isBuiltinOut = true;
    return true;
  }
  if (receiver.kind == Expr::Kind::Name &&
      findParamBinding(params, receiver.name) == nullptr &&
      locals.find(receiver.name) == locals.end()) {
    std::string resolvedReceiverPath;
    const std::string rootReceiverPath = "/" + receiver.name;
    if (defMap_.find(rootReceiverPath) != defMap_.end()) {
      resolvedReceiverPath = rootReceiverPath;
    } else {
      auto importIt = importAliases_.find(receiver.name);
      if (importIt != importAliases_.end()) {
        resolvedReceiverPath = importIt->second;
      }
    }
    if (!resolvedReceiverPath.empty() &&
        (structNames_.count(resolvedReceiverPath) > 0 ||
         defMap_.find(resolvedReceiverPath + "/" + normalizedMethodName) != defMap_.end())) {
      resolvedOut = resolvedReceiverPath + "/" + normalizedMethodName;
      return true;
    }
    const std::string resolvedType = resolveStructTypePath(receiver.name, receiver.namespacePrefix);
    if (!resolvedType.empty()) {
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
  }

  auto definitionPathContains = [&](std::string_view needle) {
    return currentValidationContext_.definitionPath.find(std::string(needle)) != std::string::npos;
  };
  auto preferredFileMethodTarget = [&](std::string_view helperName) {
    const std::string builtinPath = "/file/" + std::string(helperName);
    if (helperName != "write" && helperName != "write_line" && helperName != "close") {
      return builtinPath;
    }
    const std::string stdlibPath = "/File/" + std::string(helperName);
    if (currentValidationContext_.definitionPath.rfind(stdlibPath, 0) == 0) {
      return builtinPath;
    }
    if (defMap_.count(stdlibPath) > 0) {
      return stdlibPath;
    }
    return builtinPath;
  };
  auto preferredExperimentalMapHelperTarget = [&](std::string_view helperName) {
    if (helperName == "count") {
      return std::string("mapCount");
    }
    if (helperName == "contains") {
      return std::string("mapContains");
    }
    if (helperName == "tryAt") {
      return std::string("mapTryAt");
    }
    if (helperName == "at") {
      return std::string("mapAt");
    }
    if (helperName == "at_unsafe") {
      return std::string("mapAtUnsafe");
    }
    return std::string(helperName);
  };
  auto preferredCanonicalExperimentalMapHelperTarget = [&](std::string_view helperName) {
    return "/std/collections/experimental_map/" + preferredExperimentalMapHelperTarget(helperName);
  };
  auto shouldBuiltinValidateCurrentMapWrapperHelper = [&](std::string_view helperName) {
    if (helperName == "count") {
      return definitionPathContains("/mapCount");
    }
    if (helperName == "contains") {
      return definitionPathContains("/mapContains") ||
             definitionPathContains("/mapTryAt");
    }
    if (helperName == "tryAt") {
      return definitionPathContains("/mapTryAt");
    }
    if (helperName == "at") {
      return definitionPathContains("/mapAt");
    }
    if (helperName == "at_unsafe") {
      return definitionPathContains("/mapAtUnsafe");
    }
    return false;
  };
  auto resolveFieldBindingTarget = [&](const Expr &target, BindingInfo &bindingOut) -> bool {
    if (!(target.kind == Expr::Kind::Call && target.isFieldAccess && target.args.size() == 1)) {
      return false;
    }
    return resolveStructFieldBinding(params, locals, target.args.front(), target.name, bindingOut);
  };
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
  auto resolveArgsPackCountTarget = [&](const Expr &target, std::string &elemType) -> bool {
    elemType.clear();
    auto resolveBinding = [&](const BindingInfo &binding) {
      return getArgsPackElementType(binding, elemType);
    };
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
  auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType) -> bool {
    return resolveArgsPackElementTypeForExpr(target, params, locals, elemType);
  };
  auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string accessName;
    if (target.kind != Expr::Kind::Call || !getBuiltinArrayAccessName(target, accessName) ||
        target.args.size() != 2) {
      return false;
    }
    const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target);
    return accessReceiver != nullptr && resolveArgsPackAccessTarget(*accessReceiver, elemTypeOut);
  };
  auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    if (!isSimpleCallName(target, "dereference") || target.args.size() != 1) {
      return false;
    }
    std::string wrappedType;
    return resolveIndexedArgsPackElementType(target.args.front(), wrappedType) &&
           extractWrappedPointeeType(wrappedType, elemTypeOut);
  };
  auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {
    elemTypeOut.clear();
    std::string wrappedType;
    return resolveIndexedArgsPackElementType(target, wrappedType) &&
           extractWrappedPointeeType(wrappedType, elemTypeOut);
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
  auto resolveArrayTarget = [&](const Expr &target, std::string &elemType) -> bool {
    if (target.kind == Expr::Kind::Name) {
      auto resolveReference = [&](const BindingInfo &binding) -> bool {
        if (binding.typeName != "Reference" || binding.typeTemplateArg.empty()) {
          return false;
        }
        std::string base;
        std::string arg;
        if (!splitTemplateTypeName(binding.typeTemplateArg, base, arg) || base != "array") {
          return false;
        }
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(arg, args) || args.size() != 1) {
          return false;
        }
        elemType = args.front();
        return true;
      };
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (resolveReference(*paramBinding)) {
          return true;
        }
        if ((paramBinding->typeName == "array" || paramBinding->typeName == "vector") &&
            !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        return false;
      }
      auto it = locals.find(target.name);
      if (it == locals.end()) {
        return false;
      }
      if (resolveReference(it->second)) {
        return true;
      }
      if ((it->second.typeName == "array" || it->second.typeName == "vector") &&
          !it->second.typeTemplateArg.empty()) {
        elemType = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding)) {
      std::string base;
      std::string arg;
      if (fieldBinding.typeName == "Reference" && !fieldBinding.typeTemplateArg.empty() &&
          splitTemplateTypeName(fieldBinding.typeTemplateArg, base, arg) && base == "array") {
        std::vector<std::string> args;
        if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
          elemType = args.front();
          return true;
        }
      }
      if ((fieldBinding.typeName == "array" || fieldBinding.typeName == "vector") &&
          !fieldBinding.typeTemplateArg.empty()) {
        elemType = fieldBinding.typeTemplateArg;
        return true;
      }
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if ((resolveIndexedArgsPackElementType(target, indexedElemType) ||
           resolveWrappedIndexedArgsPackElementType(target, indexedElemType) ||
           resolveDereferencedIndexedArgsPackElementType(target, indexedElemType)) &&
          (extractCollectionElementType(indexedElemType, "array", elemType) ||
           extractCollectionElementType(indexedElemType, "vector", elemType))) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          (collectionTypePath == "/array" || collectionTypePath == "/vector")) {
        std::vector<std::string> args;
        const std::string expectedBase = collectionTypePath == "/vector" ? "vector" : "array";
        if (resolveCallCollectionTemplateArgs(target, expectedBase, params, locals, args) &&
            args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
    }
    return false;
  };
  std::function<bool(const Expr &, std::string &)> resolveSoaVectorTarget;
  std::function<bool(const Expr &, std::string &)> resolveVectorTarget =
      [&](const Expr &target, std::string &elemType) -> bool {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (paramBinding->typeName == "vector" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        return false;
      }
      auto it = locals.find(target.name);
      if (it != locals.end() && it->second.typeName == "vector" &&
          !it->second.typeTemplateArg.empty()) {
        elemType = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding) && fieldBinding.typeName == "vector" &&
        !fieldBinding.typeTemplateArg.empty()) {
      elemType = fieldBinding.typeTemplateArg;
      return true;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if ((resolveIndexedArgsPackElementType(target, indexedElemType) ||
           resolveWrappedIndexedArgsPackElementType(target, indexedElemType) ||
           resolveDereferencedIndexedArgsPackElementType(target, indexedElemType)) &&
          extractCollectionElementType(indexedElemType, "vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "vector", params, locals, args) &&
            args.size() == 1) {
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
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        BindingInfo inferredReturn;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
            normalizeBindingTypeName(inferredReturn.typeName) == "vector" &&
            !inferredReturn.typeTemplateArg.empty()) {
          elemType = inferredReturn.typeTemplateArg;
          return true;
        }
      }
      if (!target.isMethodCall && isSimpleCallName(target, "to_aos") && target.args.size() == 1) {
        std::string sourceElemType;
        const Expr &source = target.args.front();
        if (resolveSoaVectorTarget(source, sourceElemType)) {
          elemType = sourceElemType;
          return true;
        }
        if (source.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, source.name)) {
            if (paramBinding->typeName != "soa_vector" || paramBinding->typeTemplateArg.empty()) {
              return false;
            }
            sourceElemType = paramBinding->typeTemplateArg;
          } else {
            auto it = locals.find(source.name);
            if (it == locals.end() || it->second.typeName != "soa_vector" ||
                it->second.typeTemplateArg.empty()) {
              return false;
            }
            sourceElemType = it->second.typeTemplateArg;
          }
        } else if (source.kind == Expr::Kind::Call) {
          std::string sourceCollectionTypePath;
          if (defMap_.find(resolveCalleePath(source)) == defMap_.end()) {
            std::string collection;
            if (getBuiltinCollectionName(source, collection) && collection == "soa_vector" &&
                source.templateArgs.size() == 1) {
              sourceElemType = source.templateArgs.front();
            }
            if (sourceElemType.empty() && !source.isMethodCall && isSimpleCallName(source, "to_soa") &&
                source.args.size() == 1) {
              if (!resolveVectorTarget(source.args.front(), sourceElemType)) {
                return false;
              }
            }
          } else if (!resolveCallCollectionTypePath(source, params, locals, sourceCollectionTypePath) ||
                     sourceCollectionTypePath != "/soa_vector") {
            return false;
          } else {
            std::vector<std::string> sourceArgs;
            if (resolveCallCollectionTemplateArgs(source, "soa_vector", params, locals, sourceArgs) &&
                sourceArgs.size() == 1) {
              sourceElemType = sourceArgs.front();
            }
          }
        } else {
          return false;
        }
        if (!sourceElemType.empty()) {
          elemType = sourceElemType;
          return true;
        }
      }
    }
    return false;
  };
  resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        if (paramBinding->typeName == "soa_vector" && !paramBinding->typeTemplateArg.empty()) {
          elemType = paramBinding->typeTemplateArg;
          return true;
        }
        return false;
      }
      auto it = locals.find(target.name);
      if (it != locals.end() && it->second.typeName == "soa_vector" &&
          !it->second.typeTemplateArg.empty()) {
        elemType = it->second.typeTemplateArg;
        return true;
      }
      return false;
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding) && fieldBinding.typeName == "soa_vector" &&
        !fieldBinding.typeTemplateArg.empty()) {
      elemType = fieldBinding.typeTemplateArg;
      return true;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string indexedElemType;
      if ((resolveIndexedArgsPackElementType(target, indexedElemType) ||
           resolveWrappedIndexedArgsPackElementType(target, indexedElemType) ||
           resolveDereferencedIndexedArgsPackElementType(target, indexedElemType)) &&
          extractCollectionElementType(indexedElemType, "soa_vector", elemType)) {
        return true;
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/soa_vector") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "soa_vector", params, locals, args) &&
            args.size() == 1) {
          elemType = args.front();
        }
        return true;
      }
      if (!target.isMethodCall && isSimpleCallName(target, "to_soa") && target.args.size() == 1) {
        return resolveVectorTarget(target.args.front(), elemType);
      }
    }
    return false;
  };
  auto extractExperimentalMapFieldTypes = [&](const BindingInfo &binding,
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
          if (normalizedBase == "Map" || normalizedBase == "std/collections/experimental_map/Map") {
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
        return extractExperimentalMapFieldTypesFromStructPath(resolvedPath, keyTypeOut, valueTypeOut);
      }
    };

    keyTypeOut.clear();
    valueTypeOut.clear();
    if (binding.typeTemplateArg.empty()) {
      return extractFromTypeText(normalizeBindingTypeName(binding.typeName));
    }
    return extractFromTypeText(
        normalizeBindingTypeName(binding.typeName + "<" + binding.typeTemplateArg + ">"));
  };
  auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,
                                        std::string &keyTypeOut,
                                        std::string &valueTypeOut) -> bool {
    return extractMapKeyValueTypes(binding, keyTypeOut, valueTypeOut) ||
           extractExperimentalMapFieldTypes(binding, keyTypeOut, valueTypeOut);
  };
  auto resolveExperimentalMapTarget = [&](const Expr &target,
                                          std::string &keyTypeOut,
                                          std::string &valueTypeOut) -> bool {
    keyTypeOut.clear();
    valueTypeOut.clear();
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return extractExperimentalMapFieldTypes(*paramBinding, keyTypeOut, valueTypeOut);
      }
      auto it = locals.find(target.name);
      return it != locals.end() &&
             extractExperimentalMapFieldTypes(it->second, keyTypeOut, valueTypeOut);
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding)) {
      return extractExperimentalMapFieldTypes(fieldBinding, keyTypeOut, valueTypeOut);
    }
    if (target.kind != Expr::Kind::Call) {
      return false;
    }
    const std::string resolvedTarget = resolveCalleePath(target);
    auto matchesPath = [&](std::string_view basePath) {
      return resolvedTarget == basePath || resolvedTarget.rfind(std::string(basePath) + "__t", 0) == 0;
    };
    if (matchesPath("/std/collections/map/map") ||
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
        matchesPath("/std/collections/experimental_map/mapOct")) {
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) &&
          args.size() == 2) {
        keyTypeOut = args[0];
        valueTypeOut = args[1];
        return true;
      }
    }
    auto defIt = defMap_.find(resolvedTarget);
    if (defIt == defMap_.end() || !defIt->second) {
      return false;
    }
    BindingInfo inferredReturn;
    return inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
           extractExperimentalMapFieldTypes(inferredReturn, keyTypeOut, valueTypeOut);
  };
  auto resolveMapTarget = [&](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return extractAnyMapKeyValueTypes(*paramBinding, keyType, valueType);
      }
      auto it = locals.find(target.name);
      return it != locals.end() && extractAnyMapKeyValueTypes(it->second, keyType, valueType);
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding)) {
      return extractAnyMapKeyValueTypes(fieldBinding, keyType, valueType);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string elemType;
      if ((resolveIndexedArgsPackElementType(target, elemType) ||
           resolveDereferencedIndexedArgsPackElementType(target, elemType) ||
           resolveWrappedIndexedArgsPackElementType(target, elemType)) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType)) {
        return true;
      }
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2) {
        if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
          if (resolveArgsPackAccessTarget(*accessReceiver, elemType) &&
              extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType)) {
            return true;
          }
        }
      }
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/map") {
        std::vector<std::string> args;
        if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) &&
            args.size() == 2) {
          return true;
        }
        std::string collectionName;
        if (getBuiltinCollectionName(target, collectionName) &&
            collectionName == "map" &&
            target.templateArgs.size() == 2) {
          return true;
        }
        return true;
      }
      auto defIt = defMap_.find(resolveCalleePath(target));
      if (defIt == defMap_.end() || !defIt->second) {
        return false;
      }
      BindingInfo inferredReturn;
      if (inferDefinitionReturnBinding(*defIt->second, inferredReturn) &&
          extractAnyMapKeyValueTypes(inferredReturn, keyType, valueType)) {
        return true;
      }
      for (const auto &transform : defIt->second->transforms) {
        if (transform.name == "return" && transform.templateArgs.size() == 1) {
          return returnsMapCollectionType(transform.templateArgs.front());
        }
      }
    }
    return false;
  };
  auto resolveMapValueType = [&](const Expr &target, std::string &valueTypeOut) -> bool {
    valueTypeOut.clear();
    std::string keyType;
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return extractAnyMapKeyValueTypes(*paramBinding, keyType, valueTypeOut);
      }
      auto it = locals.find(target.name);
      return it != locals.end() && extractAnyMapKeyValueTypes(it->second, keyType, valueTypeOut);
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding)) {
      return extractAnyMapKeyValueTypes(fieldBinding, keyType, valueTypeOut);
    }
    if (target.kind == Expr::Kind::Call) {
      std::string elemType;
      if ((resolveIndexedArgsPackElementType(target, elemType) ||
           resolveWrappedIndexedArgsPackElementType(target, elemType) ||
           resolveDereferencedIndexedArgsPackElementType(target, elemType)) &&
          extractMapKeyValueTypesFromTypeText(elemType, keyType, valueTypeOut)) {
        return true;
      }
      std::string collectionTypePath;
      if (!resolveCallCollectionTypePath(target, params, locals, collectionTypePath) ||
          collectionTypePath != "/map") {
        return false;
      }
      std::vector<std::string> args;
      if (resolveCallCollectionTemplateArgs(target, "map", params, locals, args) &&
          args.size() == 2) {
        valueTypeOut = args[1];
        return true;
      }
      std::string collectionName;
      if (getBuiltinCollectionName(target, collectionName) &&
          collectionName == "map" &&
          target.templateArgs.size() == 2) {
        valueTypeOut = target.templateArgs[1];
        return true;
      }
      return false;
    }
    return false;
  };
  std::function<bool(const Expr &)> resolveStringTarget = [&](const Expr &target) -> bool {
    if (target.kind == Expr::Kind::StringLiteral) {
      return true;
    }
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return paramBinding->typeName == "string";
      }
      auto it = locals.find(target.name);
      return it != locals.end() && it->second.typeName == "string";
    }
    BindingInfo fieldBinding;
    if (resolveFieldBindingTarget(target, fieldBinding)) {
      return fieldBinding.typeName == "string";
    }
    if (target.kind == Expr::Kind::Call) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(target, params, locals, collectionTypePath) &&
          collectionTypePath == "/string") {
        return true;
      }
      if (target.isMethodCall && target.name == "why" && !target.args.empty()) {
        const Expr &receiverExpr = target.args.front();
        if (receiverExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
            if (normalizeBindingTypeName(paramBinding->typeName) == "FileError") {
              return true;
            }
          } else if (auto it = locals.find(receiverExpr.name); it != locals.end()) {
            if (normalizeBindingTypeName(it->second.typeName) == "FileError") {
              return true;
            }
          }
          if (receiverExpr.name == "Result") {
            return true;
          }
        }
        std::string elemType;
        if ((resolveIndexedArgsPackElementType(receiverExpr, elemType) ||
             resolveDereferencedIndexedArgsPackElementType(receiverExpr, elemType)) &&
            normalizeBindingTypeName(unwrapReferencePointerTypeText(elemType)) == "FileError") {
          return true;
        }
      }
      std::string builtinName;
      if (getBuiltinArrayAccessName(target, builtinName) && target.args.size() == 2) {
        if (const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(target)) {
          std::string elemType;
          std::string mapValueType;
          if (resolveArgsPackAccessTarget(*accessReceiver, elemType) ||
              resolveArrayTarget(*accessReceiver, elemType) ||
              resolveVectorTarget(*accessReceiver, elemType)) {
            return normalizeBindingTypeName(elemType) == "string";
          }
          if (resolveMapValueType(*accessReceiver, mapValueType)) {
            return normalizeBindingTypeName(mapValueType) == "string";
          }
          if (resolveStringTarget(*accessReceiver)) {
            return false;
          }
        }
      }
    }
    return inferExprReturnKind(target, params, locals) == ReturnKind::String;
  };
  auto getDirectMapHelperCompatibilityPath = [&](const Expr &candidate) -> std::string {
    if (candidate.kind != Expr::Kind::Call || candidate.isMethodCall || candidate.name.empty()) {
      return "";
    }
    std::string normalized = candidate.name;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    std::string normalizedPrefix = candidate.namespacePrefix;
    if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
      normalizedPrefix.erase(normalizedPrefix.begin());
    }
    std::string helperName;
    if (normalized == "map/count" || normalized == "map/contains" || normalized == "map/tryAt" ||
        normalized == "map/at" || normalized == "map/at_unsafe") {
      helperName = normalized.substr(std::string("map/").size());
    } else if (normalizedPrefix == "map" &&
               (normalized == "count" || normalized == "contains" || normalized == "tryAt" ||
                normalized == "at" || normalized == "at_unsafe")) {
      helperName = normalized;
    } else {
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (resolvedPath == "/map/count") {
        helperName = "count";
      } else if (resolvedPath == "/map/contains") {
        helperName = "contains";
      } else if (resolvedPath == "/map/tryAt") {
        helperName = "tryAt";
      } else if (resolvedPath == "/map/at") {
        helperName = "at";
      } else if (resolvedPath == "/map/at_unsafe") {
        helperName = "at_unsafe";
      } else {
        return "";
      }
    }
    const std::string removedPath = "/map/" + helperName;
    if (defMap_.find(removedPath) != defMap_.end() || candidate.args.empty()) {
      return "";
    }
    if (helperName == "at" || helperName == "at_unsafe") {
      return removedPath;
    }
    size_t receiverIndex = 0;
    if (hasNamedArguments(candidate.argNames)) {
      for (size_t i = 0; i < candidate.args.size(); ++i) {
        if (i < candidate.argNames.size() && candidate.argNames[i].has_value() &&
            *candidate.argNames[i] == "values") {
          receiverIndex = i;
          break;
        }
      }
    }
    return receiverIndex < candidate.args.size() && resolveMapTarget(candidate.args[receiverIndex])
               ? removedPath
               : "";
  };

  std::string elemType;
  auto setCollectionMethodTarget = [&](const std::string &path) {
    auto shouldPreserveBuiltinCompatibilityForExplicitRemovedMethod = [&]() {
      if (explicitRemovedMethodPath.empty()) {
        return false;
      }
      std::string ignoredElemType;
      const bool isCanonicalStdVectorPath =
          explicitRemovedMethodPath.rfind("/std/collections/vector/", 0) == 0;
      if (normalizedMethodName == "count") {
        if (isCanonicalStdVectorPath) {
          return resolveVectorTarget(receiver, ignoredElemType);
        }
        return resolveArgsPackCountTarget(receiver, ignoredElemType) ||
               resolveVectorTarget(receiver, ignoredElemType) ||
               resolveSoaVectorTarget(receiver, ignoredElemType) ||
               resolveArrayTarget(receiver, ignoredElemType) ||
               resolveStringTarget(receiver);
      }
      if (normalizedMethodName == "capacity") {
        if (isCanonicalStdVectorPath) {
          return resolveVectorTarget(receiver, ignoredElemType) ||
                 resolveSoaVectorTarget(receiver, ignoredElemType);
        }
        return resolveVectorTarget(receiver, ignoredElemType) ||
               resolveSoaVectorTarget(receiver, ignoredElemType);
      }
      if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
        if (isCanonicalStdVectorPath) {
          return resolveVectorTarget(receiver, ignoredElemType);
        }
        return resolveArgsPackAccessTarget(receiver, ignoredElemType) ||
               resolveVectorTarget(receiver, ignoredElemType) ||
               resolveArrayTarget(receiver, ignoredElemType) ||
               resolveStringTarget(receiver);
      }
      return false;
    };
    if (!explicitRemovedMethodPath.empty() && path.rfind("/string/", 0) != 0) {
      if (shouldPreserveBuiltinCompatibilityForExplicitRemovedMethod()) {
        resolvedOut = explicitRemovedMethodPath;
        isBuiltinOut = true;
        return true;
      }
      resolvedOut = explicitRemovedMethodPath;
      isBuiltinOut = false;
      return true;
    }
    resolvedOut = preferVectorStdlibHelperPath(path);
    if ((resolvedOut == "/std/collections/vector/count" ||
         resolvedOut == "/std/collections/vector/capacity" ||
         resolvedOut == "/std/collections/vector/at" ||
         resolvedOut == "/std/collections/vector/at_unsafe") &&
        hasImportedDefinitionPath(resolvedOut) &&
        defMap_.count(resolvedOut) == 0) {
      isBuiltinOut = true;
      return true;
    }
    if ((resolvedOut == "/std/collections/map/count" ||
         resolvedOut == "/std/collections/map/contains" ||
         resolvedOut == "/std/collections/map/tryAt" ||
         resolvedOut == "/std/collections/map/at" ||
         resolvedOut == "/std/collections/map/at_unsafe") &&
        (shouldBuiltinValidateCurrentMapWrapperHelper(
             resolvedOut.substr(std::string("/std/collections/map/").size())) ||
         hasImportedDefinitionPath(resolvedOut))) {
      isBuiltinOut = true;
      return true;
    }
    isBuiltinOut = defMap_.count(resolvedOut) == 0 &&
                   !hasImportedDefinitionPath(resolvedOut);
    return true;
  };
  auto preferredMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalMapTarget(receiverExpr, keyType, valueType)) {
      const std::string canonical = "/std/collections/map/" + helperName;
      if (hasDeclaredDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
        return canonical;
      }
      return preferredCanonicalExperimentalMapHelperTarget(helperName);
    }
    const std::string canonical = "/std/collections/map/" + helperName;
    const std::string alias = "/map/" + helperName;
    if (hasDeclaredDefinitionPath(canonical) || hasImportedDefinitionPath(canonical)) {
      return canonical;
    }
    if (hasDeclaredDefinitionPath(alias) || hasImportedDefinitionPath(alias)) {
      return alias;
    }
    return canonical;
  };
  auto setPreferredMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) {
    const std::string preferredMapHelper = preferredMapMethodTarget(receiverExpr, helperName);
    if (hasDeclaredDefinitionPath(preferredMapHelper) || defMap_.count(preferredMapHelper) > 0) {
      resolvedOut = preferredMapHelper;
      isBuiltinOut = false;
      return true;
    }
    return setCollectionMethodTarget(preferredMapHelper);
  };
  auto preferExplicitCanonicalVectorHelperForReceiver = [&](const Expr &receiverExpr) -> bool {
    if (explicitVectorHelperPath.empty() ||
        explicitVectorHelperPath.rfind("/std/collections/vector/", 0) != 0) {
      return false;
    }
    std::string elemType;
    return resolveExperimentalVectorValueTarget(receiverExpr, elemType);
  };
  auto classifyExplicitVectorHelperReceiver = [&](const Expr &receiverExpr) -> std::string {
    std::string elemType;
    if (resolveExperimentalVectorValueTarget(receiverExpr, elemType)) {
      return "experimental_vector";
    }
    if (resolveVectorTarget(receiverExpr, elemType)) {
      return "vector";
    }
    if (resolveSoaVectorTarget(receiverExpr, elemType)) {
      return "soa_vector";
    }
    if (resolveArrayTarget(receiverExpr, elemType)) {
      return "array";
    }
    if (resolveStringTarget(receiverExpr)) {
      return "string";
    }
    if (resolveMapTarget(receiverExpr)) {
      return "map";
    }
    return {};
  };
  auto classifyExplicitVectorHelperParam = [&](const BindingInfo &binding) -> std::string {
    std::string elemType;
    std::string keyType;
    std::string valueType;
    if (extractExperimentalVectorElementType(binding, elemType)) {
      return "experimental_vector";
    }
    if (extractMapKeyValueTypes(binding, keyType, valueType)) {
      return "map";
    }
    const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
    if (normalizedType == "vector" || normalizedType == "soa_vector" ||
        normalizedType == "array" || normalizedType == "string") {
      return normalizedType;
    }
    return {};
  };
  auto hasReceiverCompatibleExplicitVectorHelperPath = [&](const std::string &path,
                                                           const Expr &receiverExpr) {
    auto paramsIt = paramsByDef_.find(path);
    if (paramsIt == paramsByDef_.end() || paramsIt->second.empty()) {
      return false;
    }
    const std::string receiverFamily = classifyExplicitVectorHelperReceiver(receiverExpr);
    if (receiverFamily.empty()) {
      return false;
    }
    return classifyExplicitVectorHelperParam(paramsIt->second.front().binding) == receiverFamily;
  };
  auto resolveExplicitDirectCallReturnMethodTarget = [&](const Expr &receiverExpr) -> bool {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
      return false;
    }
    auto defIt = defMap_.find(resolveCalleePath(receiverExpr));
    if (defIt == defMap_.end() || defIt->second == nullptr) {
      return false;
    }
    for (const auto &transform : defIt->second->transforms) {
      if (transform.name != "return" || transform.templateArgs.size() != 1) {
        continue;
      }
      const std::string normalizedReturnType = normalizeBindingTypeName(transform.templateArgs.front());
      std::string normalizedReturnBaseType = normalizedReturnType;
      std::string normalizedReturnArgText;
      if (!normalizedReturnBaseType.empty() && normalizedReturnBaseType.front() == '/') {
        normalizedReturnBaseType.erase(normalizedReturnBaseType.begin());
      }
      std::string returnBase;
      if (splitTemplateTypeName(normalizedReturnBaseType, returnBase, normalizedReturnArgText) &&
          !returnBase.empty()) {
        normalizedReturnBaseType = normalizeBindingTypeName(returnBase);
      }
      if (normalizedReturnType.empty() || normalizedReturnBaseType == "auto" ||
          !normalizeCollectionTypePath(normalizedReturnType).empty()) {
        return false;
      }
      if (normalizedReturnBaseType == "Reference" ||
          normalizedReturnBaseType == "Pointer") {
        resolvedOut = "/" + normalizedReturnBaseType + "/" + normalizedMethodName;
        return true;
      }
      if (isPrimitiveBindingTypeName(normalizedReturnBaseType)) {
        resolvedOut = "/" + normalizedReturnBaseType + "/" + normalizedMethodName;
        return true;
      }
      std::string resolvedReturnType = resolveStructTypePath(normalizedReturnType, defIt->second->namespacePrefix);
      if (resolvedReturnType.empty()) {
        resolvedReturnType = resolveTypePath(normalizedReturnType, defIt->second->namespacePrefix);
      }
      if (!resolvedReturnType.empty()) {
        resolvedOut = resolvedReturnType + "/" + normalizedMethodName;
        return true;
      }
      return false;
    }
    return false;
  };
  auto resolveCollectionMethodFromTypePath = [&](const std::string &collectionTypePath) -> bool {
    if (normalizedMethodName == "count") {
      if (collectionTypePath == "/array") {
        return setCollectionMethodTarget("/array/count");
      }
      if (collectionTypePath == "/vector") {
        return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
      }
      if (collectionTypePath == "/soa_vector") {
        return setCollectionMethodTarget("/soa_vector/count");
      }
      if (collectionTypePath == "/string") {
        return setCollectionMethodTarget("/string/count");
      }
      if (collectionTypePath == "/map") {
        return setPreferredMapMethodTarget(receiver, "count");
      }
    }
    if (normalizedMethodName == "capacity" && collectionTypePath == "/vector") {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
    }
    if (normalizedMethodName == "contains" && collectionTypePath == "/map") {
      return setPreferredMapMethodTarget(receiver, "contains");
    }
    if (normalizedMethodName == "tryAt" && collectionTypePath == "/map") {
      return setPreferredMapMethodTarget(receiver, "tryAt");
    }
    if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
      if (collectionTypePath == "/array") {
        return setCollectionMethodTarget("/array/" + normalizedMethodName);
      }
      if (collectionTypePath == "/vector") {
        return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
      }
      if (collectionTypePath == "/string") {
        return setCollectionMethodTarget("/string/" + normalizedMethodName);
      }
      if (collectionTypePath == "/map") {
        return setPreferredMapMethodTarget(receiver, normalizedMethodName);
      }
    }
    if ((normalizedMethodName == "get" || normalizedMethodName == "ref") &&
        collectionTypePath == "/soa_vector") {
      return setCollectionMethodTarget("/soa_vector/" + normalizedMethodName);
    }
    return false;
  };
  auto resolveArgsPackElementMethodTarget = [&](const std::string &elementTypeText,
                                                const Expr &receiverExpr) -> bool {
    const std::string normalizedElemType = normalizeBindingTypeName(elementTypeText);
    std::string normalizedElemBaseType = normalizedElemType;
    if (!normalizedElemBaseType.empty() && normalizedElemBaseType.front() == '/') {
      normalizedElemBaseType.erase(normalizedElemBaseType.begin());
    }
    std::string collectionElemType = normalizedElemType;
    std::string wrappedPointeeType;
    if (extractWrappedPointeeType(normalizedElemType, wrappedPointeeType)) {
      collectionElemType = normalizeBindingTypeName(wrappedPointeeType);
    }
    if (collectionElemType == "string" || normalizedElemBaseType == "string") {
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    }
    if (collectionElemType == "FileError" && normalizedMethodName == "why") {
      resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
      isBuiltinOut = resolvedOut == "/file_error/why";
      return true;
    }
    std::string elemBase;
    std::string elemArgText;
    if (splitTemplateTypeName(collectionElemType, elemBase, elemArgText)) {
      elemBase = normalizeBindingTypeName(elemBase);
      if (elemBase == "vector" || elemBase == "array" || elemBase == "soa_vector") {
        return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
      }
      if (isMapCollectionTypeName(elemBase)) {
        return setPreferredMapMethodTarget(receiverExpr, normalizedMethodName);
      }
      if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
        resolvedOut = preferredFileMethodTarget(normalizedMethodName);
        isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
        return true;
      }
    }
    if (isPrimitiveBindingTypeName(normalizedElemBaseType)) {
      resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
      return true;
    }
    std::string resolvedElemType = resolveStructTypePath(collectionElemType, receiverExpr.namespacePrefix);
    if (resolvedElemType.empty()) {
      resolvedElemType = resolveTypePath(collectionElemType, receiverExpr.namespacePrefix);
    }
    if (!resolvedElemType.empty()) {
      resolvedOut = resolvedElemType + "/" + normalizedMethodName;
      return true;
    }
    return false;
  };
  auto setIndexedArgsPackMapMethodTarget = [&](const Expr &receiverExpr, const std::string &helperName) -> bool {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.args.size() != 2) {
      return false;
    }
    std::string indexedElemType;
    std::string keyType;
    std::string valueType;
    const bool resolvedIndexedMapType =
        ((resolveIndexedArgsPackElementType(receiverExpr, indexedElemType) ||
          resolveWrappedIndexedArgsPackElementType(receiverExpr, indexedElemType) ||
          resolveDereferencedIndexedArgsPackElementType(receiverExpr, indexedElemType)) &&
         extractMapKeyValueTypesFromTypeText(indexedElemType, keyType, valueType));
    const bool resolvedReceiverPackType = [&]() {
      std::string accessName;
      if (!getBuiltinArrayAccessName(receiverExpr, accessName)) {
        return false;
      }
      const Expr *accessReceiver = resolveBuiltinAccessReceiverExpr(receiverExpr);
      return accessReceiver != nullptr &&
             resolveArgsPackAccessTarget(*accessReceiver, indexedElemType) &&
             extractMapKeyValueTypesFromTypeText(indexedElemType, keyType, valueType);
    }();
    if (!resolvedIndexedMapType && !resolvedReceiverPackType) {
      return false;
    }
    return setPreferredMapMethodTarget(receiverExpr, helperName);
  };
  auto isDirectMapConstructorReceiverCall = [&](const Expr &receiverExpr) {
    if (receiverExpr.kind != Expr::Kind::Call || receiverExpr.isBinding || receiverExpr.isMethodCall) {
      return false;
    }
    const std::string resolvedReceiver = resolveCalleePath(receiverExpr);
    auto matchesPath = [&](std::string_view basePath) {
      return resolvedReceiver == basePath || resolvedReceiver.rfind(std::string(basePath) + "__t", 0) == 0;
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

  if ((normalizedMethodName == "count" || normalizedMethodName == "contains" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "at" ||
       normalizedMethodName == "at_unsafe") &&
      isDirectMapConstructorReceiverCall(receiver)) {
    std::string keyType;
    std::string valueType;
    if (resolveExperimentalMapTarget(receiver, keyType, valueType)) {
      error_ = "unknown call target: " +
               preferredCanonicalExperimentalMapHelperTarget(normalizedMethodName);
      return false;
    }
    return setPreferredMapMethodTarget(receiver, normalizedMethodName);
  }
  auto explicitVectorReceiverFamily = classifyExplicitVectorHelperReceiver(receiver);
  if (!explicitVectorHelperPath.empty() &&
      explicitVectorHelperPath.rfind("/std/collections/vector/", 0) == 0 &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity") &&
      (explicitVectorReceiverFamily == "string" || explicitVectorReceiverFamily == "array" ||
       explicitVectorReceiverFamily == "map")) {
    if (!hasReceiverCompatibleExplicitVectorHelperPath(explicitVectorHelperPath, receiver)) {
      error_ = "unknown method: " + explicitVectorHelperPath;
      return false;
    }
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (!explicitVectorHelperPath.empty() &&
      explicitVectorHelperPath.rfind("/vector/", 0) == 0 &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity") &&
      explicitVectorReceiverFamily == "map") {
    if (!hasReceiverCompatibleExplicitVectorHelperPath(explicitVectorHelperPath, receiver)) {
      error_ = "unknown method: " + explicitVectorHelperPath;
      return false;
    }
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (preferExplicitCanonicalVectorHelperForReceiver(receiver)) {
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (normalizedMethodName == "count") {
    if (resolveArgsPackCountTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/count");
    }
    if (resolveVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
    }
    if (resolveExperimentalVectorValueTarget(receiver, elemType)) {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
    }
    if (resolveSoaVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/soa_vector/count");
    }
    if (resolveArrayTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/count");
    }
    if (resolveStringTarget(receiver)) {
      return setCollectionMethodTarget("/string/count");
    }
    if (setIndexedArgsPackMapMethodTarget(receiver, "count")) {
      return true;
    }
    if (resolveMapTarget(receiver)) {
      return setPreferredMapMethodTarget(receiver, "count");
    }
  }
  if (normalizedMethodName == "contains" || normalizedMethodName == "tryAt") {
    if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (resolveMapTarget(receiver)) {
      return setPreferredMapMethodTarget(receiver, normalizedMethodName);
    }
  }
  if (normalizedMethodName == "capacity") {
    if (resolveVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
    }
    if (resolveExperimentalVectorValueTarget(receiver, elemType)) {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
    }
  }
  if (normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
    if (resolveArgsPackAccessTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/" + normalizedMethodName);
    }
    if (resolveVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
    }
    if (resolveExperimentalVectorValueTarget(receiver, elemType)) {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
    }
    if (resolveArrayTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/array/" + normalizedMethodName);
    }
    if (resolveStringTarget(receiver)) {
      return setCollectionMethodTarget("/string/" + normalizedMethodName);
    }
    if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
      return true;
    }
    if (resolveMapTarget(receiver)) {
      return setPreferredMapMethodTarget(receiver, normalizedMethodName);
    }
  }
  if (normalizedMethodName == "get" || normalizedMethodName == "ref") {
    if (resolveSoaVectorTarget(receiver, elemType)) {
      return setCollectionMethodTarget("/soa_vector/" + normalizedMethodName);
    }
  }
  if (resolveSoaVectorTarget(receiver, elemType)) {
    const std::string normalizedElemType = normalizeBindingTypeName(elemType);
    std::string currentNamespace;
    if (!currentValidationContext_.definitionPath.empty()) {
      const size_t slash = currentValidationContext_.definitionPath.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace = currentValidationContext_.definitionPath.substr(0, slash);
      }
    }
    const std::string lookupNamespace =
        !receiver.namespacePrefix.empty() ? receiver.namespacePrefix : currentNamespace;
    const std::string elementStructPath = resolveStructTypePath(normalizedElemType, lookupNamespace);
    if (!elementStructPath.empty()) {
      auto structIt = defMap_.find(elementStructPath);
      if (structIt != defMap_.end() && structIt->second != nullptr) {
        for (const auto &stmt : structIt->second->statements) {
          if (!stmt.isBinding || isStaticBinding(stmt) || stmt.name != normalizedMethodName) {
            continue;
          }
          const std::string helperPath = "/soa_vector/" + normalizedMethodName;
          if (defMap_.count(helperPath) > 0) {
            resolvedOut = helperPath;
            isBuiltinOut = false;
          } else {
            resolvedOut = "/soa_vector/field_view/" + normalizedMethodName;
            isBuiltinOut = true;
          }
          return true;
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string accessHelperName;
    if (getBuiltinArrayAccessName(receiver, accessHelperName) && !receiver.args.empty()) {
      const std::string removedMapCompatibilityPath = getDirectMapHelperCompatibilityPath(receiver);
      if (!removedMapCompatibilityPath.empty()) {
        error_ = "unknown call target: " + removedMapCompatibilityPath;
        return false;
      }
      size_t accessReceiverIndex = 0;
      if (!receiver.isMethodCall && hasNamedArguments(receiver.argNames)) {
        bool foundValues = false;
        for (size_t i = 0; i < receiver.args.size(); ++i) {
          if (i < receiver.argNames.size() && receiver.argNames[i].has_value() &&
              *receiver.argNames[i] == "values") {
            accessReceiverIndex = i;
            foundValues = true;
            break;
          }
        }
        if (!foundValues) {
          accessReceiverIndex = 0;
        }
      }
      if (accessReceiverIndex < receiver.args.size()) {
        const Expr &accessReceiver = receiver.args[accessReceiverIndex];
        std::string accessElemType;
        std::string accessValueType;
        if (resolveArgsPackAccessTarget(accessReceiver, accessElemType) ||
            resolveVectorTarget(accessReceiver, accessElemType) ||
            resolveArrayTarget(accessReceiver, accessElemType)) {
          const std::string normalizedElemType =
              normalizeBindingTypeName(unwrapReferencePointerTypeText(accessElemType));
          std::string normalizedElemBaseType = normalizedElemType;
          if (!normalizedElemBaseType.empty() && normalizedElemBaseType.front() == '/') {
            normalizedElemBaseType.erase(normalizedElemBaseType.begin());
          }
          if (normalizedElemType == "string" || normalizedElemBaseType == "string") {
            return setCollectionMethodTarget("/string/" + normalizedMethodName);
          }
          std::string elemBase;
          std::string elemArgText;
          if (splitTemplateTypeName(normalizedElemType, elemBase, elemArgText)) {
            elemBase = normalizeBindingTypeName(elemBase);
            if (elemBase == "vector" || elemBase == "array" || elemBase == "soa_vector") {
              return setCollectionMethodTarget("/" + elemBase + "/" + normalizedMethodName);
            }
            if (isMapCollectionTypeName(elemBase)) {
              if (setIndexedArgsPackMapMethodTarget(receiver, normalizedMethodName)) {
                return true;
              }
              return setPreferredMapMethodTarget(receiver, normalizedMethodName);
            }
            if (elemBase == "File" && isFileMethodName(normalizedMethodName)) {
              resolvedOut = preferredFileMethodTarget(normalizedMethodName);
              isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
              return true;
            }
          }
          if (normalizedElemType == "FileError" && normalizedMethodName == "why") {
            resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
            isBuiltinOut = resolvedOut == "/file_error/why";
            return true;
          }
          if (isPrimitiveBindingTypeName(normalizedElemBaseType)) {
            resolvedOut = "/" + normalizedElemBaseType + "/" + normalizedMethodName;
            return true;
          }
          std::string resolvedElemType = resolveStructTypePath(normalizedElemType, receiver.namespacePrefix);
          if (resolvedElemType.empty()) {
            resolvedElemType = resolveTypePath(normalizedElemType, receiver.namespacePrefix);
          }
          if (!resolvedElemType.empty()) {
            resolvedOut = resolvedElemType + "/" + normalizedMethodName;
            return true;
          }
        }
        if (resolveStringTarget(accessReceiver)) {
          resolvedOut = "/i32/" + normalizedMethodName;
          return true;
        }
        if (resolveMapValueType(accessReceiver, accessValueType) &&
            resolveCalleePath(receiver).rfind("/map/", 0) == 0) {
          const std::string normalizedValueType = normalizeBindingTypeName(accessValueType);
          std::string normalizedValueBaseType = normalizedValueType;
          if (!normalizedValueBaseType.empty() && normalizedValueBaseType.front() == '/') {
            normalizedValueBaseType.erase(normalizedValueBaseType.begin());
          }
          if (isPrimitiveBindingTypeName(normalizedValueBaseType)) {
            resolvedOut = "/" + normalizedValueBaseType + "/" + normalizedMethodName;
            return true;
          }
          std::string resolvedValueType = resolveStructTypePath(normalizedValueType, receiver.namespacePrefix);
          if (resolvedValueType.empty()) {
            resolvedValueType = resolveTypePath(normalizedValueType, receiver.namespacePrefix);
          }
          if (!resolvedValueType.empty()) {
            resolvedOut = resolvedValueType + "/" + normalizedMethodName;
            return true;
          }
        }
      }
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding) {
    std::string dereferencedElemType;
    if (resolveDereferencedIndexedArgsPackElementType(receiver, dereferencedElemType) &&
        resolveArgsPackElementMethodTarget(dereferencedElemType, receiver)) {
      return true;
    }
  }
  if ((normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
       normalizedMethodName == "at" || normalizedMethodName == "at_unsafe" ||
       normalizedMethodName == "push" || normalizedMethodName == "pop" ||
       normalizedMethodName == "reserve" || normalizedMethodName == "clear" ||
       normalizedMethodName == "remove_at" || normalizedMethodName == "remove_swap")) {
    std::string vectorMethodTarget;
    if (resolveVectorHelperMethodTarget(params, locals, receiver, normalizedMethodName,
                                        vectorMethodTarget)) {
      const bool isVectorFamilyTarget =
          vectorMethodTarget.rfind("/vector/", 0) == 0 ||
          vectorMethodTarget.rfind("/soa_vector/", 0) == 0 ||
          vectorMethodTarget.rfind("/std/collections/vector/", 0) == 0 ||
          vectorMethodTarget.rfind("/std/collections/experimental_vector/", 0) == 0;
      if (!isVectorFamilyTarget) {
        vectorMethodTarget.clear();
      }
    }
    if (!vectorMethodTarget.empty()) {
      if (vectorMethodTarget.rfind("/std/collections/experimental_vector/", 0) == 0 &&
          !hasDeclaredDefinitionPath(vectorMethodTarget) &&
          !hasImportedDefinitionPath(vectorMethodTarget)) {
        const std::string canonicalVectorMethodTarget =
            "/std/collections/vector/" + normalizedMethodName;
        if (hasDeclaredDefinitionPath(canonicalVectorMethodTarget) ||
            hasImportedDefinitionPath(canonicalVectorMethodTarget)) {
          vectorMethodTarget = canonicalVectorMethodTarget;
        } else {
          const std::string aliasVectorMethodTarget = "/vector/" + normalizedMethodName;
          if (hasDeclaredDefinitionPath(aliasVectorMethodTarget) ||
              hasImportedDefinitionPath(aliasVectorMethodTarget)) {
            vectorMethodTarget = aliasVectorMethodTarget;
          }
        }
      }
      return setCollectionMethodTarget(vectorMethodTarget);
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
    const std::string resolvedType = resolveCalleePath(receiver);
    if (!resolvedType.empty() && structNames_.count(resolvedType) > 0) {
      std::string experimentalElemType;
      BindingInfo receiverBinding;
      receiverBinding.typeName = resolvedType;
      if ((normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
           normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
          extractExperimentalVectorElementType(receiverBinding, experimentalElemType)) {
        if (normalizedMethodName == "count") {
          return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
        }
        if (normalizedMethodName == "capacity") {
          return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
        }
        return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
      }
      resolvedOut = resolvedType + "/" + normalizedMethodName;
      return true;
    }
    if (resolveExplicitDirectCallReturnMethodTarget(receiver)) {
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
       normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        receiverCollectionTypePath == "/vector") {
      if (normalizedMethodName == "count") {
        return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
      }
      if (normalizedMethodName == "capacity") {
        return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
      }
      return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
    }
  }
  if (receiver.kind == Expr::Kind::Call) {
    std::string receiverCollectionTypePath;
    if (!explicitVectorHelperPath.empty() &&
        resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        ((normalizedMethodName == "capacity" &&
          receiverCollectionTypePath != "/vector" && receiverCollectionTypePath != "/soa_vector") ||
         (normalizedMethodName == "count" &&
          receiverCollectionTypePath == "/map"))) {
      if (!hasReceiverCompatibleExplicitVectorHelperPath(explicitVectorHelperPath, receiver)) {
        error_ = "unknown method: " + explicitVectorHelperPath;
        return false;
      }
      resolvedOut = explicitVectorHelperPath;
      isBuiltinOut = false;
      return true;
    }
  }
  if (receiver.kind == Expr::Kind::Call) {
    std::string receiverCollectionTypePath;
    if (resolveCallCollectionTypePath(receiver, params, locals, receiverCollectionTypePath) &&
        resolveCollectionMethodFromTypePath(receiverCollectionTypePath)) {
      return true;
    }
  }

  std::string typeName;
  std::string typeTemplateArg;
  if (receiver.kind == Expr::Kind::Name) {
    if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
      typeName = paramBinding->typeName;
      typeTemplateArg = paramBinding->typeTemplateArg;
    } else {
      auto it = locals.find(receiver.name);
      if (it != locals.end()) {
        typeName = it->second.typeName;
        typeTemplateArg = it->second.typeTemplateArg;
      }
    }
  }
  if (typeName.empty()) {
    if (receiver.kind == Expr::Kind::Call) {
      auto defIt = defMap_.find(resolveCalleePath(receiver));
      if (defIt != defMap_.end() && defIt->second != nullptr) {
        BindingInfo inferredReturn;
        if (inferDefinitionReturnBinding(*defIt->second, inferredReturn)) {
          typeName = normalizeBindingTypeName(inferredReturn.typeName);
          typeTemplateArg = inferredReturn.typeTemplateArg;
        }
      }
    }
  }
  if (typeName.empty()) {
    std::string inferredStruct = inferStructReturnPath(receiver, params, locals);
    if (!inferredStruct.empty()) {
      std::string normalizedStruct = normalizeBindingTypeName(inferredStruct);
      if (!normalizedStruct.empty() && normalizedStruct.front() != '/') {
        normalizedStruct.insert(normalizedStruct.begin(), '/');
      }
      if (normalizedStruct == "/map" ||
          normalizedStruct.rfind("/std/collections/experimental_map/Map__", 0) == 0) {
        typeName = "/map";
      } else {
        typeName = inferredStruct;
      }
    }
  }
  if (typeName.empty()) {
    ReturnKind inferredKind = inferExprReturnKind(receiver, params, locals);
    std::string inferred;
    if (inferredKind == ReturnKind::Array) {
      inferred = inferStructReturnPath(receiver, params, locals);
      if (inferred.empty()) {
        inferred = typeNameForReturnKind(inferredKind);
      }
    } else {
      inferred = typeNameForReturnKind(inferredKind);
    }
    if (!inferred.empty()) {
      typeName = inferred;
    }
  }
  if (typeName == "File" && isFileMethodName(normalizedMethodName)) {
    resolvedOut = preferredFileMethodTarget(normalizedMethodName);
    isBuiltinOut = (resolvedOut.rfind("/file/", 0) == 0);
    return true;
  }
  const std::string normalizedTypeName = normalizeBindingTypeName(typeName);
  std::string normalizedBaseTypeName = normalizedTypeName;
  if (!normalizedBaseTypeName.empty() && normalizedBaseTypeName.front() == '/') {
    normalizedBaseTypeName.erase(normalizedBaseTypeName.begin());
  }
  if (normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
      normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") {
    BindingInfo receiverBinding;
    receiverBinding.typeName = typeName;
    receiverBinding.typeTemplateArg = typeTemplateArg;
    std::string experimentalElemType;
    if (extractExperimentalVectorElementType(receiverBinding, experimentalElemType)) {
      if (normalizedMethodName == "count") {
        return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
      }
      if (normalizedMethodName == "capacity") {
        return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
      }
      return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
    }
  }
  if (!explicitVectorHelperPath.empty() &&
      explicitVectorHelperPath.rfind("/std/collections/vector/", 0) == 0 &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity") &&
      (normalizedTypeName == "string" || normalizedTypeName == "array" ||
       isMapCollectionTypeName(normalizedTypeName))) {
    if (!hasReceiverCompatibleExplicitVectorHelperPath(explicitVectorHelperPath, receiver)) {
      error_ = "unknown method: " + explicitVectorHelperPath;
      return false;
    }
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (!explicitVectorHelperPath.empty() &&
      explicitVectorHelperPath.rfind("/vector/", 0) == 0 &&
      (normalizedMethodName == "count" || normalizedMethodName == "capacity") &&
      isMapCollectionTypeName(normalizedTypeName)) {
    if (!hasReceiverCompatibleExplicitVectorHelperPath(explicitVectorHelperPath, receiver)) {
      error_ = "unknown method: " + explicitVectorHelperPath;
      return false;
    }
    resolvedOut = explicitVectorHelperPath;
    isBuiltinOut = false;
    return true;
  }
  if (isMapCollectionTypeName(normalizeBindingTypeName(typeName)) &&
      (normalizedMethodName == "count" || normalizedMethodName == "contains" ||
       normalizedMethodName == "tryAt" || normalizedMethodName == "at" ||
       normalizedMethodName == "at_unsafe")) {
    const std::string canonicalMapHelper = "/std/collections/map/" + normalizedMethodName;
    if (hasDeclaredDefinitionPath(canonicalMapHelper) || hasImportedDefinitionPath(canonicalMapHelper)) {
      resolvedOut = canonicalMapHelper;
      isBuiltinOut = false;
      return true;
    }
    return setPreferredMapMethodTarget(receiver, normalizedMethodName);
  }
  if (typeName == "FileError" && normalizedMethodName == "why") {
    resolvedOut = defMap_.count("/FileError/why") > 0 ? "/FileError/why" : "/file_error/why";
    isBuiltinOut = resolvedOut == "/file_error/why";
    return true;
  }
  if (typeName == "string" &&
      (normalizedMethodName == "count" || normalizedMethodName == "at" || normalizedMethodName == "at_unsafe")) {
    return setCollectionMethodTarget("/string/" + normalizedMethodName);
  }
  if (typeName.empty()) {
    if (receiver.kind == Expr::Kind::Call && !validateExpr(params, locals, receiver)) {
      return false;
    }
    error_ = "unknown method target for " + normalizedMethodName;
    return false;
  }
  if (typeName == "Pointer" || typeName == "Reference") {
    error_ = "unknown method target for " + normalizedMethodName;
    return false;
  }
  if (isPrimitiveBindingTypeName(normalizedBaseTypeName)) {
    resolvedOut = "/" + normalizedBaseTypeName + "/" + normalizedMethodName;
    return true;
  }
  std::string resolvedType = resolveStructTypePath(typeName, receiver.namespacePrefix);
  if (resolvedType.empty()) {
    resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  }
  if ((normalizedMethodName == "count" || normalizedMethodName == "capacity" ||
       normalizedMethodName == "at" || normalizedMethodName == "at_unsafe") &&
      resolvedType.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
    if (normalizedMethodName == "count") {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("count"));
    }
    if (normalizedMethodName == "capacity") {
      return setCollectionMethodTarget(preferredBareVectorHelperTarget("capacity"));
    }
    return setCollectionMethodTarget(preferredBareVectorHelperTarget(normalizedMethodName));
  }
  resolvedOut = resolvedType + "/" + normalizedMethodName;
  return true;
}

bool SemanticsValidator::validateExprMethodCallTarget(
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    const Expr &expr,
    const ExprMethodResolutionContext &context,
    const BuiltinCollectionDispatchResolvers &dispatchResolvers,
    const BuiltinCollectionDispatchResolverAdapters &dispatchResolverAdapters,
    std::string &resolved,
    bool &resolvedMethod,
    bool &usedMethodTarget,
    bool &hasMethodReceiverIndex,
    size_t &methodReceiverIndex) {
  if (!expr.isMethodCall) {
    return true;
  }

  const auto &resolveVectorTarget = dispatchResolvers.resolveVectorTarget;
  const auto &resolveMapTargetWithTypes = dispatchResolvers.resolveMapTarget;
  const auto &resolveExperimentalMapTarget = dispatchResolvers.resolveExperimentalMapTarget;
  auto resolveMapTarget = [&](const Expr &target) -> bool {
    std::string keyType;
    std::string valueType;
    if (resolveMapTargetWithTypes(target, keyType, valueType) ||
        resolveExperimentalMapTarget(target, keyType, valueType)) {
      return true;
    }
    std::string inferredTypeText;
    return inferQueryExprTypeText(target, params, locals, inferredTypeText) &&
           returnsMapCollectionType(inferredTypeText);
  };

  auto isExplicitVectorCompatibilityMethodWithTemplateArgs = [&]() {
    if (expr.templateArgs.empty() || expr.args.empty()) {
      return false;
    }
    if (expr.name != "/vector/count" &&
        expr.name != "/std/collections/vector/count") {
      return false;
    }
    std::string elemType;
    return resolveVectorTarget(expr.args.front(), elemType);
  };
  if (isExplicitVectorCompatibilityMethodWithTemplateArgs()) {
    error_ = "unknown method: /vector/count";
    return false;
  }
  if (resolved == "/std/collections/vector/count" &&
      !expr.templateArgs.empty() &&
      !expr.args.empty()) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType)) {
      error_ = "unknown method: /vector/count";
      return false;
    }
  }
  if (resolved == "/std/collections/vector/count" &&
      !expr.hasBodyArguments &&
      expr.bodyArguments.empty() &&
      !expr.args.empty()) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType) &&
        expr.args.size() != 1) {
      error_ = "argument count mismatch for builtin count";
      return false;
    }
  }
  if (expr.namespacePrefix.empty() &&
      !expr.args.empty() &&
      (expr.name == "count" || expr.name == "capacity" ||
       expr.name == "at" || expr.name == "at_unsafe")) {
    std::string elemType;
    const bool isVectorReceiver = resolveVectorTarget(expr.args.front(), elemType);
    const bool hasImplicitSamePathHelper =
        hasDeclaredDefinitionPath("/vector/" + expr.name) ||
        hasImportedDefinitionPath("/vector/" + expr.name);
    const bool hasCanonicalOnlyVectorHelper =
        !hasImplicitSamePathHelper &&
        hasDeclaredDefinitionPath("/std/collections/vector/" + expr.name) &&
        !hasImportedDefinitionPath("/std/collections/vector/" + expr.name);
    if (isVectorReceiver && hasCanonicalOnlyVectorHelper) {
      if (expr.name == "count" && expr.args.size() != 1) {
        error_ = "argument count mismatch for builtin count";
        return false;
      }
      if (expr.name == "capacity" && expr.args.size() != 1) {
        error_ = "argument count mismatch for builtin capacity";
        return false;
      }
      if ((expr.name == "at" || expr.name == "at_unsafe") &&
          expr.args.size() == 2) {
        const ReturnKind indexKind = inferExprReturnKind(expr.args[1], params, locals);
        if (indexKind != ReturnKind::Int &&
            indexKind != ReturnKind::Int64 &&
            indexKind != ReturnKind::UInt64) {
          error_ = expr.name + " requires integer index";
          return false;
        }
      }
    }
  }
  auto rejectBuiltinStringCountShadowOnMapAccessReceiver = [&](const std::string &resolvedPath) -> bool {
    if (resolvedPath != "/string/count" || expr.args.empty()) {
      return false;
    }
    const Expr &receiverExpr = expr.args.front();
    if (receiverExpr.kind != Expr::Kind::Call || !receiverExpr.isMethodCall) {
      return false;
    }
    std::string accessHelperName;
    if (!getBuiltinArrayAccessName(receiverExpr, accessHelperName) ||
        (accessHelperName != "at" && accessHelperName != "at_unsafe")) {
      return false;
    }
    const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(receiverExpr);
    if (accessReceiver == nullptr) {
      return false;
    }
    std::string mapValueType;
    if (!this->resolveMapValueType(*accessReceiver, dispatchResolvers, mapValueType) ||
        normalizeBindingTypeName(mapValueType) == "string") {
      return false;
    }
    error_ = "argument type mismatch for /string/count parameter values: expected string";
    return true;
  };
  const std::string removedMapMethodPath =
      this->mapNamespacedMethodCompatibilityPath(expr, params, locals, dispatchResolverAdapters);
  if (!removedMapMethodPath.empty()) {
    error_ = "unknown method: " + removedMapMethodPath;
    return false;
  }
  if (context.hasVectorHelperCallResolution) {
    resolvedMethod = false;
    return true;
  }
  if (expr.args.empty()) {
    error_ = "method call missing receiver";
    return false;
  }
  usedMethodTarget = true;
  hasMethodReceiverIndex = true;
  methodReceiverIndex = 0;
  bool isBuiltinMethod = false;
  const bool hasBlockArgs = expr.hasBodyArguments || !expr.bodyArguments.empty();
  const std::string normalizedMethodNamespace = [&]() {
    std::string normalized = expr.namespacePrefix;
    if (!normalized.empty() && normalized.front() == '/') {
      normalized.erase(normalized.begin());
    }
    return normalized;
  }();
  const bool hasExplicitVectorCompatibilityNamespace =
      normalizedMethodNamespace == "vector" ||
      normalizedMethodNamespace == "std/collections/vector";
  std::string vectorMethodTarget;
  const bool isVectorCompatibilityMethodName =
      expr.name == "count" || expr.name == "capacity" || expr.name == "at" ||
      expr.name == "at_unsafe" || expr.name == "push" || expr.name == "pop" ||
      expr.name == "reserve" || expr.name == "clear" || expr.name == "remove_at" ||
      expr.name == "remove_swap";
  std::string receiverCollectionTypePath;
  std::string experimentalVectorElemType;
  const bool isExperimentalVectorCallReceiver =
      dispatchResolvers.resolveExperimentalVectorValueTarget != nullptr &&
      dispatchResolvers.resolveExperimentalVectorValueTarget(
          expr.args.front(), experimentalVectorElemType);
  const std::string preferredVisibleVectorMethodTarget =
      preferVectorStdlibHelperPath(preferredBareVectorHelperTarget(expr.name));
  const bool hasVisiblePreferredVectorMethodTarget =
      hasImportedDefinitionPath(preferredVisibleVectorMethodTarget) ||
      defMap_.count(preferredVisibleVectorMethodTarget) > 0;
  if (isVectorCompatibilityMethodName &&
      !hasExplicitVectorCompatibilityNamespace &&
      expr.args.front().kind == Expr::Kind::Call &&
      !expr.args.front().isBinding &&
      !expr.args.front().isMethodCall &&
      resolveCallCollectionTypePath(expr.args.front(), params, locals, receiverCollectionTypePath) &&
      receiverCollectionTypePath == "/vector" &&
      (isExperimentalVectorCallReceiver || hasVisiblePreferredVectorMethodTarget)) {
    resolved = preferredVisibleVectorMethodTarget;
    isBuiltinMethod = false;
  } else if (isVectorCompatibilityMethodName &&
             !hasExplicitVectorCompatibilityNamespace &&
             resolveVectorHelperMethodTarget(params, locals, expr.args.front(), expr.name,
                                             vectorMethodTarget)) {
    if (vectorMethodTarget.rfind("/std/collections/experimental_vector/", 0) == 0 &&
        !hasImportedDefinitionPath(vectorMethodTarget) &&
        defMap_.count(vectorMethodTarget) == 0) {
      const std::string canonicalVectorMethodTarget =
          "/std/collections/vector/" + expr.name;
      if (hasImportedDefinitionPath(canonicalVectorMethodTarget) ||
          defMap_.count(canonicalVectorMethodTarget) > 0) {
        vectorMethodTarget = canonicalVectorMethodTarget;
      } else {
        const std::string aliasVectorMethodTarget = "/vector/" + expr.name;
        if (hasImportedDefinitionPath(aliasVectorMethodTarget) ||
            defMap_.count(aliasVectorMethodTarget) > 0) {
          vectorMethodTarget = aliasVectorMethodTarget;
        }
      }
    }
    if (hasImportedDefinitionPath(vectorMethodTarget) ||
        defMap_.count(vectorMethodTarget) > 0) {
      resolved = vectorMethodTarget;
      isBuiltinMethod = false;
    } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(),
                                    expr.name, resolved, isBuiltinMethod)) {
      if (hasBlockArgs &&
          resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
        isBuiltinMethod = false;
      } else {
        return false;
      }
    }
  } else if (!resolveMethodTarget(params, locals, expr.namespacePrefix, expr.args.front(), expr.name, resolved,
                                  isBuiltinMethod)) {
    auto resolveInferredMapMethodFallback = [&]() -> bool {
      const std::string helperName = expr.name;
      if (!(helperName == "count" || helperName == "contains" ||
            helperName == "tryAt" || helperName == "at" ||
            helperName == "at_unsafe") ||
          !resolveMapTarget(expr.args.front())) {
        return false;
      }
      const std::string canonicalMapMethodTarget =
          "/std/collections/map/" + helperName;
      const std::string aliasMapMethodTarget =
          "/map/" + helperName;
      if (hasDeclaredDefinitionPath(canonicalMapMethodTarget) ||
          hasImportedDefinitionPath(canonicalMapMethodTarget)) {
        resolved = canonicalMapMethodTarget;
      } else if (hasDeclaredDefinitionPath(aliasMapMethodTarget) ||
                 hasImportedDefinitionPath(aliasMapMethodTarget)) {
        resolved = aliasMapMethodTarget;
      } else {
        resolved = canonicalMapMethodTarget;
      }
      if (resolved.rfind("/std/collections/map/", 0) == 0 &&
          (shouldBuiltinValidateCurrentMapWrapperHelper(helperName) ||
           hasImportedDefinitionPath(resolved))) {
        isBuiltinMethod = true;
      } else {
        isBuiltinMethod = defMap_.count(resolved) == 0 &&
                          !hasImportedDefinitionPath(resolved);
      }
      return true;
    };
    if (resolveInferredMapMethodFallback()) {
    } else if (hasBlockArgs &&
               resolvePointerLikeMethodTarget(params, locals, expr.args.front(), expr.name, resolved)) {
      isBuiltinMethod = false;
    } else {
      return false;
    }
  } else if (rejectBuiltinStringCountShadowOnMapAccessReceiver(resolved)) {
    return false;
  } else if (hasBlockArgs) {
    const std::string pointerLikeType =
        inferPointerLikeCallReturnType(expr.args.front(), params, locals);
    if (!pointerLikeType.empty()) {
      resolved = "/" + pointerLikeType + "/" + normalizeCollectionMethodName(expr.name);
      isBuiltinMethod = false;
    }
  }
  if (!isBuiltinMethod && isVectorCompatibilityMethodName &&
      resolved.rfind("/std/collections/experimental_vector/Vector__", 0) == 0) {
    const std::string canonicalVectorMethodTarget =
        "/std/collections/vector/" + expr.name;
    if (hasImportedDefinitionPath(canonicalVectorMethodTarget) ||
        defMap_.count(canonicalVectorMethodTarget) > 0) {
      resolved = canonicalVectorMethodTarget;
    } else {
      const std::string aliasVectorMethodTarget = "/vector/" + expr.name;
      if (hasImportedDefinitionPath(aliasVectorMethodTarget) ||
          defMap_.count(aliasVectorMethodTarget) > 0) {
        resolved = aliasVectorMethodTarget;
      }
    }
    resolved = preferVectorStdlibHelperPath(resolved);
  }
  bool keepBuiltinIndexedArgsPackMapMethod = false;
  keepBuiltinIndexedArgsPackMapMethod = resolveMapTarget(expr.args.front());
  if (expr.args.front().kind == Expr::Kind::Call) {
    std::string accessName;
    if (getBuiltinArrayAccessName(expr.args.front(), accessName) && expr.args.front().args.size() == 2) {
      if (const Expr *accessReceiver = this->resolveBuiltinAccessReceiverExpr(expr.args.front())) {
        std::string elemType;
        std::string keyType;
        std::string valueType;
        keepBuiltinIndexedArgsPackMapMethod =
            keepBuiltinIndexedArgsPackMapMethod ||
            (resolveArgsPackElementTypeForExpr(*accessReceiver, params, locals, elemType) &&
             extractMapKeyValueTypesFromTypeText(elemType, keyType, valueType));
      }
    }
  }
  if (((resolved == "/std/collections/map/count" &&
        !hasDeclaredDefinitionPath("/map/count") &&
        !hasImportedDefinitionPath("/std/collections/map/count")) ||
       (resolved == "/std/collections/map/contains" &&
        !hasDeclaredDefinitionPath("/map/contains") &&
        !hasImportedDefinitionPath("/std/collections/map/contains")) ||
       (resolved == "/std/collections/map/tryAt" &&
        !hasDeclaredDefinitionPath("/map/tryAt") &&
        !hasImportedDefinitionPath("/std/collections/map/tryAt")) ||
       (resolved == "/std/collections/map/at" &&
        !hasDeclaredDefinitionPath("/map/at") &&
        !hasImportedDefinitionPath("/std/collections/map/at")) ||
       (resolved == "/std/collections/map/at_unsafe" &&
        !hasDeclaredDefinitionPath("/map/at_unsafe") &&
        !hasImportedDefinitionPath("/std/collections/map/at_unsafe"))) &&
      !hasDeclaredDefinitionPath(resolved) &&
      !keepBuiltinIndexedArgsPackMapMethod) {
    isBuiltinMethod = false;
  }
  std::string builtinVectorReceiverCollection;
  if (isBuiltinMethod && resolved == "/vector/count" &&
      defMap_.find(resolved) == defMap_.end() &&
      !hasImportedDefinitionPath(resolved) &&
      !hasDeclaredDefinitionPath("/std/collections/vector/count") &&
      !hasImportedDefinitionPath("/std/collections/vector/count") &&
      !hasDeclaredDefinitionPath("/std/collections/vectorCount") &&
      !hasImportedDefinitionPath("/std/collections/vectorCount") &&
      !hasNamedArguments(expr.argNames) &&
      (expr.args.front().kind == Expr::Kind::Name ||
       (expr.args.front().kind == Expr::Kind::Call &&
        getBuiltinCollectionName(expr.args.front(), builtinVectorReceiverCollection) &&
        builtinVectorReceiverCollection == "vector"))) {
    std::string elemType;
    if (resolveVectorTarget(expr.args.front(), elemType)) {
      error_ = "unknown method: /vector/count";
      return false;
    }
  }
  if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
      isVectorBuiltinName(expr, "capacity")) {
    context.promoteCapacityToBuiltinValidation(expr.args.front(), resolved, isBuiltinMethod, true);
  }
  if (!isBuiltinMethod && defMap_.find(resolved) == defMap_.end() &&
      !hasImportedDefinitionPath(resolved) && !hasBlockArgs) {
    if (const std::string diagnostic = context.unavailableMethodDiagnostic(resolved);
        !diagnostic.empty()) {
      error_ = diagnostic;
    } else {
      error_ = "unknown method: " + resolved;
    }
    return false;
  }
  resolvedMethod = isBuiltinMethod;
  return true;
}

} // namespace primec::semantics
