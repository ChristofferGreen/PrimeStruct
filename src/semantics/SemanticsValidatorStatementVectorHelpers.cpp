#include "SemanticsValidator.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace primec::semantics {
namespace {

std::string_view trimLeadingSlash(std::string_view text) {
  if (!text.empty() && text.front() == '/') {
    text.remove_prefix(1);
  }
  return text;
}

ReturnKind returnKindForStatementBinding(const BindingInfo &binding) {
  if (binding.typeName == "Reference") {
    std::string base;
    std::string arg;
    if (splitTemplateTypeName(binding.typeTemplateArg, base, arg) && base == "array") {
      std::vector<std::string> args;
      if (splitTopLevelTemplateArgs(arg, args) && args.size() == 1) {
        return ReturnKind::Array;
      }
    }
    return returnKindForTypeName(binding.typeTemplateArg);
  }
  return returnKindForTypeName(binding.typeName);
}

bool allowsArrayVectorCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

bool allowsVectorStdlibCompatibilitySuffix(const std::string &suffix) {
  return suffix != "count" && suffix != "capacity" && suffix != "at" && suffix != "at_unsafe" &&
         suffix != "push" && suffix != "pop" && suffix != "reserve" && suffix != "clear" &&
         suffix != "remove_at" && suffix != "remove_swap";
}

} // namespace

std::string SemanticsValidator::preferVectorStdlibHelperPath(const std::string &path) const {
  auto hasVisibleDefinitionPath = [&](const std::string &candidate) {
    if ((candidate.rfind("/vector/", 0) == 0 ||
         candidate.rfind("/map/", 0) == 0) &&
        defMap_.count(candidate) == 0 &&
        !hasDeclaredDefinitionPath(candidate)) {
      return false;
    }
    return hasImportedDefinitionPath(candidate) || defMap_.count(candidate) > 0;
  };
  std::string preferred = path;
  if (preferred.rfind("/array/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/array/").size());
    if (allowsArrayVectorCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (hasVisibleDefinitionPath(vectorAlias)) {
        return vectorAlias;
      }
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        return stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/vector/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string stdlibAlias = "/std/collections/vector/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        preferred = stdlibAlias;
      } else if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (hasVisibleDefinitionPath(arrayAlias)) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/std/collections/vector/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/std/collections/vector/").size());
    if (allowsVectorStdlibCompatibilitySuffix(suffix)) {
      const std::string vectorAlias = "/vector/" + suffix;
      if (hasVisibleDefinitionPath(vectorAlias)) {
        preferred = vectorAlias;
      } else if (allowsArrayVectorCompatibilitySuffix(suffix)) {
        const std::string arrayAlias = "/array/" + suffix;
        if (hasVisibleDefinitionPath(arrayAlias)) {
          preferred = arrayAlias;
        }
      }
    }
  }
  if (preferred.rfind("/map/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt") {
      const std::string stdlibAlias = "/std/collections/map/" + suffix;
      if (hasVisibleDefinitionPath(stdlibAlias)) {
        preferred = stdlibAlias;
      }
    }
  }
  if (preferred.rfind("/std/collections/map/", 0) == 0 && !hasVisibleDefinitionPath(preferred)) {
    const std::string suffix = preferred.substr(std::string("/std/collections/map/").size());
    if (suffix != "count" && suffix != "contains" && suffix != "tryAt" &&
        suffix != "at" && suffix != "at_unsafe") {
      const std::string mapAlias = "/map/" + suffix;
      if (hasVisibleDefinitionPath(mapAlias)) {
        preferred = mapAlias;
      }
    }
  }
  return preferred;
}

bool SemanticsValidator::validateVectorStatementHelper(const std::vector<ParameterInfo> &params,
                                                       const std::unordered_map<std::string, BindingInfo> &locals,
                                                       const Expr &stmt,
                                                       const std::string &namespacePrefix,
                                                       const std::vector<std::string> *definitionTemplateArgs,
                                                       const std::vector<Expr> *enclosingStatements,
                                                       size_t statementIndex,
                                                       bool &handled) {
  handled = false;
  std::optional<BindingInfo> resolvedVectorBindingStorage;
  auto isIntegerExpr = [&](const Expr &arg) -> bool {
    ReturnKind kind = inferExprReturnKind(arg, params, locals);
    if (kind == ReturnKind::Int || kind == ReturnKind::Int64 || kind == ReturnKind::UInt64) {
      return true;
    }
    if (kind == ReturnKind::Bool || kind == ReturnKind::Float32 || kind == ReturnKind::Float64 ||
        kind == ReturnKind::Void || kind == ReturnKind::Array) {
      return false;
    }
    if (kind == ReturnKind::Unknown) {
      if (arg.kind == Expr::Kind::FloatLiteral || arg.kind == Expr::Kind::StringLiteral) {
        return false;
      }
      if (isPointerExpr(arg, params, locals)) {
        return false;
      }
      if (arg.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, arg.name)) {
          ReturnKind paramKind = returnKindForStatementBinding(*paramBinding);
          return paramKind == ReturnKind::Int || paramKind == ReturnKind::Int64 || paramKind == ReturnKind::UInt64;
        }
        auto it = locals.find(arg.name);
        if (it != locals.end()) {
          ReturnKind localKind = returnKindForStatementBinding(it->second);
          return localKind == ReturnKind::Int || localKind == ReturnKind::Int64 || localKind == ReturnKind::UInt64;
        }
      }
      return true;
    }
    return false;
  };
  auto resolveVectorBinding = [&](const Expr &target, const BindingInfo *&bindingOut) -> bool {
    bindingOut = nullptr;
    resolvedVectorBindingStorage.reset();
    auto resolveNamedBinding = [&](const std::string &name) -> const BindingInfo * {
      if (const BindingInfo *paramBinding = findParamBinding(params, name)) {
        return paramBinding;
      }
      auto it = locals.find(name);
      if (it != locals.end()) {
        return &it->second;
      }
      return nullptr;
    };
    auto resolveVectorTypeText = [&](const std::string &typeText, bool isMutable) -> bool {
      if (typeText.empty()) {
        return false;
      }
      std::string normalizedType = normalizeBindingTypeName(typeText);
      std::string base;
      std::string arg;
      if (splitTemplateTypeName(normalizedType, base, arg)) {
        const std::string normalizedBase = normalizeBindingTypeName(base);
        if ((normalizedBase == "vector" || normalizedBase == "soa_vector") && !arg.empty()) {
          resolvedVectorBindingStorage.emplace();
          resolvedVectorBindingStorage->typeName = normalizedBase;
          resolvedVectorBindingStorage->typeTemplateArg = arg;
          resolvedVectorBindingStorage->isMutable = isMutable;
          bindingOut = &*resolvedVectorBindingStorage;
          return true;
        }
      } else if (normalizedType == "vector" || normalizedType == "soa_vector") {
        resolvedVectorBindingStorage.emplace();
        resolvedVectorBindingStorage->typeName = normalizedType;
        resolvedVectorBindingStorage->typeTemplateArg.clear();
        resolvedVectorBindingStorage->isMutable = isMutable;
        bindingOut = &*resolvedVectorBindingStorage;
        return true;
      }
      BindingInfo inferredBinding;
      inferredBinding.typeName = normalizedType;
      if (splitTemplateTypeName(normalizedType, base, arg)) {
        inferredBinding.typeName = normalizeBindingTypeName(base);
        inferredBinding.typeTemplateArg = arg;
      }
      std::string elemType;
      if (!extractExperimentalVectorElementType(inferredBinding, elemType)) {
        return false;
      }
      resolvedVectorBindingStorage.emplace();
      resolvedVectorBindingStorage->typeName = "Vector";
      resolvedVectorBindingStorage->typeTemplateArg = elemType;
      resolvedVectorBindingStorage->isMutable = isMutable;
      bindingOut = &*resolvedVectorBindingStorage;
      return true;
    };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *binding = resolveNamedBinding(target.name)) {
        if ((binding->typeName == "vector" || binding->typeName == "soa_vector") &&
            !binding->typeTemplateArg.empty()) {
          bindingOut = binding;
          return true;
        }
        std::string elemType;
        if (!extractExperimentalVectorElementType(*binding, elemType)) {
          return false;
        }
        resolvedVectorBindingStorage.emplace();
        resolvedVectorBindingStorage->typeName = "Vector";
        resolvedVectorBindingStorage->typeTemplateArg = elemType;
        resolvedVectorBindingStorage->isMutable = binding->isMutable;
        bindingOut = &*resolvedVectorBindingStorage;
        return true;
      }
      return false;
    }
    if (target.kind == Expr::Kind::Call) {
      std::string accessName;
      if (getBuiltinArrayAccessName(target, accessName) && target.args.size() == 2 &&
          target.args.front().kind == Expr::Kind::Name) {
        if (const BindingInfo *binding = resolveNamedBinding(target.args.front().name)) {
          std::string elementType;
          if (getArgsPackElementType(*binding, elementType)) {
            return resolveVectorTypeText(elementType, true);
          }
        }
        return false;
      }
      if (isSimpleCallName(target, "dereference") && target.args.size() == 1) {
        const Expr &derefTarget = target.args.front();
        if (derefTarget.kind == Expr::Kind::Name) {
          const BindingInfo *binding = resolveNamedBinding(derefTarget.name);
          if (binding == nullptr || binding->typeTemplateArg.empty()) {
            return false;
          }
          const std::string normalizedType = normalizeBindingTypeName(binding->typeName);
          if (normalizedType != "Reference" && normalizedType != "Pointer") {
            return false;
          }
          return resolveVectorTypeText(binding->typeTemplateArg, true);
        }
        if (getBuiltinArrayAccessName(derefTarget, accessName) && derefTarget.args.size() == 2 &&
            derefTarget.args.front().kind == Expr::Kind::Name) {
          if (const BindingInfo *binding = resolveNamedBinding(derefTarget.args.front().name)) {
            std::string elementType;
            if (getArgsPackElementType(*binding, elementType)) {
              std::string base;
              std::string arg;
              if (splitTemplateTypeName(normalizeBindingTypeName(elementType), base, arg)) {
                const std::string normalizedBase = normalizeBindingTypeName(base);
                if ((normalizedBase == "Reference" || normalizedBase == "Pointer") && !arg.empty()) {
                  return resolveVectorTypeText(arg, true);
                }
              }
            }
          }
        }
        return false;
      }
    }
    return false;
  };
  auto validateVectorElementType = [&](const Expr &arg, const std::string &typeName) -> bool {
    if (typeName.empty()) {
      error_ = "push requires vector element type";
      return false;
    }
    const std::string normalizedType = normalizeBindingTypeName(typeName);
    auto isStringValueExpr = [&](const Expr &candidate) {
      if (candidate.kind == Expr::Kind::StringLiteral) {
        return true;
      }
      if (candidate.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding = findParamBinding(params, candidate.name)) {
          return paramBinding->typeName == "string";
        }
        auto it = locals.find(candidate.name);
        return it != locals.end() && it->second.typeName == "string";
      }
      return inferExprReturnKind(candidate, params, locals) == ReturnKind::String;
    };
    if (normalizedType == "string") {
      if (!isStringValueExpr(arg)) {
        error_ = "push requires element type " + typeName;
        return false;
      }
      return true;
    }
    ReturnKind expectedKind = returnKindForTypeName(normalizedType);
    if (expectedKind == ReturnKind::Unknown) {
      return true;
    }
    if (isStringValueExpr(arg)) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    ReturnKind argKind = inferExprReturnKind(arg, params, locals);
    if (argKind != ReturnKind::Unknown && argKind != expectedKind) {
      error_ = "push requires element type " + typeName;
      return false;
    }
    return true;
  };
  auto validateVectorHelperTarget = [&](const Expr &target, const char *helperName, const BindingInfo *&bindingOut) -> bool {
    const std::string helper = helperName == nullptr ? "" : std::string(helperName);
    const bool allowSoaVectorTarget = helper == "push" || helper == "reserve";
    if (!resolveVectorBinding(target, bindingOut) || bindingOut == nullptr) {
      error_ = std::string(helperName) + " requires vector binding";
      return false;
    }
    std::string experimentalElemType;
    const bool isExperimentalVectorTarget = extractExperimentalVectorElementType(*bindingOut, experimentalElemType);
    if (
        (bindingOut->typeName != "vector" &&
         !(allowSoaVectorTarget && bindingOut->typeName == "soa_vector") &&
         !isExperimentalVectorTarget)) {
      error_ = std::string(helperName) + " requires vector binding";
      return false;
    }
    if (!bindingOut->isMutable) {
      error_ = std::string(helperName) + " requires mutable vector binding";
      return false;
    }
    return true;
  };
  auto isVectorBuiltinName = [&](const Expr &candidate, const char *helper) -> bool {
    if (isSimpleCallName(candidate, helper)) {
      return true;
    }
    std::string namespacedCollection;
    std::string namespacedHelper;
    if (!getNamespacedCollectionHelperName(candidate, namespacedCollection, namespacedHelper)) {
      const std::string resolvedPath = resolveCalleePath(candidate);
      if (resolvedPath.rfind("/std/", 0) != 0 || defMap_.count(resolvedPath) != 0) {
        return false;
      }
      const std::string helperName = helper == nullptr ? std::string() : std::string(helper);
      const size_t lastSlash = resolvedPath.find_last_of('/');
      if (lastSlash == std::string::npos || resolvedPath.substr(lastSlash + 1) != helperName) {
        return false;
      }
      return true;
    }
    return namespacedCollection == "vector" && namespacedHelper == helper;
  };
  auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    auto matchesHelper = [&](const char *helper) {
      return isVectorBuiltinName(candidate, helper);
    };
    if (matchesHelper("push")) {
      nameOut = "push";
      return true;
    }
    if (matchesHelper("pop")) {
      nameOut = "pop";
      return true;
    }
    if (matchesHelper("reserve")) {
      nameOut = "reserve";
      return true;
    }
    if (matchesHelper("clear")) {
      nameOut = "clear";
      return true;
    }
    if (matchesHelper("remove_at")) {
      nameOut = "remove_at";
      return true;
    }
    if (matchesHelper("remove_swap")) {
      nameOut = "remove_swap";
      return true;
    }
    return false;
  };
  auto resolveVectorHelperTargetPath = [&](const Expr &receiver,
                                           const std::string &helperName,
                                           std::string &resolvedOut) -> bool {
    resolvedOut.clear();
    auto resolveExperimentalVectorReceiver = [&](const Expr &candidate,
                                                 std::string &elemTypeOut) -> bool {
      BindingInfo inferredBinding;
      if (candidate.kind == Expr::Kind::Call) {
        const std::string resolvedCandidate = resolveCalleePath(candidate);
        auto matchesVectorCtorPath = [&](std::string_view basePath) {
          return resolvedCandidate == basePath ||
                 resolvedCandidate.rfind(std::string(basePath) + "__t", 0) == 0;
        };
        if (matchesVectorCtorPath("/std/collections/vector/vector") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vector") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorNew") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorSingle") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorPair") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorTriple") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorQuad") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorQuint") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorSext") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorSept") ||
            matchesVectorCtorPath("/std/collections/experimental_vector/vectorOct")) {
          std::vector<std::string> templateArgs;
          if (resolveCallCollectionTemplateArgs(candidate, "vector", params, locals, templateArgs) &&
              templateArgs.size() == 1) {
            elemTypeOut = templateArgs.front();
            return true;
          }
          if (candidate.templateArgs.size() == 1) {
            elemTypeOut = candidate.templateArgs.front();
            return true;
          }
        }
      }
      if (candidate.kind == Expr::Kind::Call &&
          inferBindingTypeFromInitializer(candidate, params, locals, inferredBinding) &&
          extractExperimentalVectorElementType(inferredBinding, elemTypeOut)) {
        return true;
      }
      std::string receiverTypeText;
      if (inferQueryExprTypeText(candidate, params, locals, receiverTypeText) &&
          [&]() {
            std::string base;
            std::string arg;
            const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
            if (splitTemplateTypeName(normalizedType, base, arg)) {
              inferredBinding.typeName = normalizeBindingTypeName(base);
              inferredBinding.typeTemplateArg = arg;
            } else {
              inferredBinding.typeName = normalizedType;
            }
            return extractExperimentalVectorElementType(inferredBinding, elemTypeOut);
          }()) {
        return true;
      }
      return false;
    };
    std::string experimentalElemType;
    if (resolveExperimentalVectorReceiver(receiver, experimentalElemType)) {
      resolvedOut = (helperName == "count" || helperName == "capacity" ||
                     helperName == "at" || helperName == "at_unsafe")
                        ? preferredBareVectorHelperTarget(helperName)
                        : specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
      return true;
    }
    if (receiver.kind == Expr::Kind::Call &&
        (helperName == "count" || helperName == "capacity" ||
         helperName == "at" || helperName == "at_unsafe")) {
      std::string collectionTypePath;
      if (resolveCallCollectionTypePath(receiver, params, locals, collectionTypePath) &&
          collectionTypePath == "/vector") {
        resolvedOut = preferredBareVectorHelperTarget(helperName);
        return true;
      }
    }
    const BindingInfo *vectorBinding = nullptr;
    if (resolveVectorBinding(receiver, vectorBinding) && vectorBinding != nullptr) {
      if (extractExperimentalVectorElementType(*vectorBinding, experimentalElemType)) {
        resolvedOut = specializedExperimentalVectorHelperTarget(helperName, experimentalElemType);
        return true;
      }
      if (vectorBinding->typeName == "vector" || vectorBinding->typeName == "soa_vector") {
        resolvedOut = "/" + vectorBinding->typeName + "/" + helperName;
        return true;
      }
    }
    auto resolveReceiverTypePath = [&](const std::string &typeName, const std::string &receiverNamespacePrefix) {
      if (typeName.empty()) {
        return std::string{};
      }
      if (isPrimitiveBindingTypeName(typeName)) {
        return "/" + normalizeBindingTypeName(typeName);
      }
      std::string resolvedType = resolveTypePath(typeName, receiverNamespacePrefix);
      if (structNames_.count(resolvedType) == 0 && defMap_.count(resolvedType) == 0) {
        auto importIt = importAliases_.find(typeName);
        if (importIt != importAliases_.end()) {
          resolvedType = importIt->second;
        }
      }
      return resolvedType;
    };
    if (receiver.kind == Expr::Kind::Name) {
      std::string typeName;
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        typeName = paramBinding->typeName;
      } else {
        auto it = locals.find(receiver.name);
        if (it != locals.end()) {
          typeName = it->second.typeName;
        }
      }
      if (typeName.empty() || typeName == "Pointer" || typeName == "Reference") {
        return false;
      }
      const std::string resolvedType = resolveReceiverTypePath(typeName, receiver.namespacePrefix);
      if (resolvedType.empty()) {
        return false;
      }
      resolvedOut = resolvedType + "/" + helperName;
      return true;
    }
    if (receiver.kind == Expr::Kind::Call && !receiver.isBinding && !receiver.isMethodCall) {
      std::string resolvedType = resolveCalleePath(receiver);
      if (resolvedType.empty() || structNames_.count(resolvedType) == 0) {
        resolvedType = inferStructReturnPath(receiver, params, locals);
      }
      if (resolvedType.empty()) {
        return false;
      }
      resolvedOut = resolvedType + "/" + helperName;
      return true;
    }
    return false;
  };
  auto isNonCollectionStructVectorHelperTarget = [&](const std::string &resolvedPath) -> bool {
    const size_t slash = resolvedPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
      return false;
    }
    const std::string receiverPath = resolvedPath.substr(0, slash);
    if (receiverPath == "/array" || receiverPath == "/vector" || receiverPath == "/map" || receiverPath == "/string") {
      return false;
    }
    return structNames_.count(receiverPath) > 0;
  };
  auto validateVectorHelperReceiver = [&](const Expr &receiver, const std::string &helperName) -> bool {
    if (!validateExpr(params, locals, receiver)) {
      return false;
    }
    std::string resolvedMethod;
    if (!resolveVectorHelperTargetPath(receiver, helperName, resolvedMethod)) {
      return true;
    }
    if (defMap_.find(resolvedMethod) == defMap_.end() && isNonCollectionStructVectorHelperTarget(resolvedMethod)) {
      error_ = "unknown method: " + resolvedMethod;
      return false;
    }
    return true;
  };

  std::string vectorHelper;
  if (!getVectorStatementHelperName(stmt, vectorHelper)) {
    return true;
  }
  handled = true;
  auto hasVisibleDefinitionPath = [&](const std::string &path) {
    if (hasImportedDefinitionPath(path)) {
      return true;
    }
    if (defMap_.count(path) == 0) {
      return false;
    }
    if (path.rfind("/std/collections/vector/", 0) == 0) {
      auto paramsIt = paramsByDef_.find(path);
      if (paramsIt != paramsByDef_.end() && !paramsIt->second.empty()) {
        std::string experimentalElemType;
        if (extractExperimentalVectorElementType(paramsIt->second.front().binding, experimentalElemType)) {
          return false;
        }
      }
    }
    return true;
  };

  std::string vectorHelperResolved = resolveCalleePath(stmt);
  std::string namespacedCollection;
  std::string namespacedHelper;
  const bool isNamespacedCollectionHelperCall =
      getNamespacedCollectionHelperName(stmt, namespacedCollection, namespacedHelper);
  const bool isNamespacedVectorHelperCall =
      isNamespacedCollectionHelperCall && namespacedCollection == "vector";
  const bool isStdNamespacedVectorCanonicalHelperCall =
      !stmt.isMethodCall && vectorHelperResolved.rfind("/std/collections/vector/", 0) == 0 &&
      (namespacedHelper == "count" || namespacedHelper == "capacity" || namespacedHelper == "at" ||
       namespacedHelper == "at_unsafe" || namespacedHelper == "push" || namespacedHelper == "pop" ||
       namespacedHelper == "reserve" || namespacedHelper == "clear" || namespacedHelper == "remove_at" ||
       namespacedHelper == "remove_swap");
  auto explicitCanonicalStdVectorMutatorMethodPath = [&]() -> std::string {
    if (!stmt.isMethodCall) {
      return "";
    }
    auto isCanonicalMutatorName = [](std::string_view helperName) {
      return helperName == "push" || helperName == "pop" || helperName == "reserve" ||
             helperName == "clear" || helperName == "remove_at" || helperName == "remove_swap";
    };
    const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));
    const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));
    if (normalizedPrefix == "std/collections/vector" && isCanonicalMutatorName(normalizedName)) {
      return "/std/collections/vector/" + normalizedName;
    }
    constexpr std::string_view kCanonicalPrefix = "std/collections/vector/";
    if (normalizedName.rfind(kCanonicalPrefix, 0) != 0) {
      return "";
    }
    const std::string_view helperName = std::string_view(normalizedName).substr(kCanonicalPrefix.size());
    if (!isCanonicalMutatorName(helperName)) {
      return "";
    }
    return "/std/collections/vector/" + std::string(helperName);
  }();
  const bool shouldAllowStdNamespacedVectorHelperCompatibilityFallback =
      isStdNamespacedVectorCanonicalHelperCall && !namespacedHelper.empty() &&
      hasVisibleDefinitionPath("/vector/" + namespacedHelper);
  const bool isUserMethodTarget =
      stmt.isMethodCall && defMap_.find(vectorHelperResolved) != defMap_.end() &&
      vectorHelperResolved.rfind("/vector/", 0) != 0 && vectorHelperResolved.rfind("/soa_vector/", 0) != 0;
  if (isUserMethodTarget) {
    return validateExpr(params, locals, stmt, enclosingStatements, statementIndex);
  }
  if (!explicitCanonicalStdVectorMutatorMethodPath.empty() &&
      !hasDeclaredDefinitionPath(explicitCanonicalStdVectorMutatorMethodPath) &&
      !hasImportedDefinitionPath(explicitCanonicalStdVectorMutatorMethodPath)) {
    error_ = "unknown method: " + explicitCanonicalStdVectorMutatorMethodPath;
    return false;
  }
  bool hasResolvedReceiverIndex = false;
  size_t resolvedReceiverIndex = 0;
  if (stmt.isMethodCall && !stmt.args.empty()) {
    hasResolvedReceiverIndex = true;
    resolvedReceiverIndex = 0;
  }
  const bool shouldProbeVectorHelperReceiver =
      !(isStdNamespacedVectorCanonicalHelperCall && defMap_.find(vectorHelperResolved) == defMap_.end() &&
        !shouldAllowStdNamespacedVectorHelperCompatibilityFallback) &&
      (defMap_.find(vectorHelperResolved) == defMap_.end() || isNamespacedVectorHelperCall) &&
      !(isStdNamespacedVectorCanonicalHelperCall && defMap_.find(vectorHelperResolved) != defMap_.end());
  if (shouldProbeVectorHelperReceiver && !stmt.args.empty()) {
    auto isVectorHelperReceiverName = [&](const Expr &candidate) -> bool {
      if (candidate.kind != Expr::Kind::Name) {
        return false;
      }
      const BindingInfo *binding = nullptr;
      if (!resolveVectorBinding(candidate, binding) || binding == nullptr) {
        return false;
      }
      if (binding->typeName == "soa_vector") {
        return true;
      }
      std::string experimentalElemType;
      return (binding->typeName == "vector" || extractExperimentalVectorElementType(*binding, experimentalElemType)) &&
             binding->isMutable;
    };
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= stmt.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    const bool hasNamedArgs = hasNamedArguments(stmt.argNames);
    if (hasNamedArgs) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
    }
    const bool probePositionalReorderedReceiver =
        !isStdNamespacedVectorCanonicalHelperCall &&
        !hasNamedArgs && stmt.args.size() > 1 &&
        (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
         stmt.args.front().kind == Expr::Kind::FloatLiteral || stmt.args.front().kind == Expr::Kind::StringLiteral ||
         (stmt.args.front().kind == Expr::Kind::Name && !isVectorHelperReceiverName(stmt.args.front())));
    if (probePositionalReorderedReceiver) {
      for (size_t i = 1; i < stmt.args.size(); ++i) {
        appendReceiverIndex(i);
      }
    }
    for (size_t receiverIndex : receiverIndices) {
      if (receiverIndex >= stmt.args.size()) {
        continue;
      }
      const Expr &receiverCandidate = stmt.args[receiverIndex];
      std::string methodTarget;
      if (resolveVectorHelperTargetPath(receiverCandidate, vectorHelper, methodTarget)) {
        methodTarget = preferVectorStdlibHelperPath(methodTarget);
      }
      if (hasVisibleDefinitionPath(methodTarget)) {
        vectorHelperResolved = methodTarget;
        hasResolvedReceiverIndex = true;
        resolvedReceiverIndex = receiverIndex;
        break;
      }
    }
  }
  const bool canonicalBuiltinCompatibilityHelper =
      isStdNamespacedVectorCanonicalHelperCall &&
      (hasDeclaredDefinitionPath(vectorHelperResolved) || hasImportedDefinitionPath(vectorHelperResolved));
  bool shouldUseCanonicalBuiltinCompatibilityFallback = false;
  size_t canonicalBuiltinCompatibilityReceiverIndex = 0;
  if (canonicalBuiltinCompatibilityHelper && !stmt.args.empty()) {
    std::vector<size_t> receiverIndices;
    auto appendReceiverIndex = [&](size_t index) {
      if (index >= stmt.args.size()) {
        return;
      }
      for (size_t existing : receiverIndices) {
        if (existing == index) {
          return;
        }
      }
      receiverIndices.push_back(index);
    };
    if (hasNamedArguments(stmt.argNames)) {
      bool hasValuesNamedReceiver = false;
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
          appendReceiverIndex(i);
          hasValuesNamedReceiver = true;
        }
      }
      if (!hasValuesNamedReceiver) {
        appendReceiverIndex(0);
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    } else {
      appendReceiverIndex(0);
      const bool probePositionalReorderedReceiver =
          !isStdNamespacedVectorCanonicalHelperCall &&
          stmt.args.size() > 1 &&
          (stmt.args.front().kind == Expr::Kind::Literal || stmt.args.front().kind == Expr::Kind::BoolLiteral ||
           stmt.args.front().kind == Expr::Kind::FloatLiteral ||
           stmt.args.front().kind == Expr::Kind::StringLiteral);
      if (probePositionalReorderedReceiver) {
        for (size_t i = 1; i < stmt.args.size(); ++i) {
          appendReceiverIndex(i);
        }
      }
    }
    for (size_t receiverIndex : receiverIndices) {
      const BindingInfo *receiverBinding = nullptr;
      if (!resolveVectorBinding(stmt.args[receiverIndex], receiverBinding) || receiverBinding == nullptr) {
        continue;
      }
      const bool allowSoaVectorTarget = vectorHelper == "push" || vectorHelper == "reserve";
      if (receiverBinding->typeName == "vector" ||
          (allowSoaVectorTarget && receiverBinding->typeName == "soa_vector")) {
        shouldUseCanonicalBuiltinCompatibilityFallback = true;
        canonicalBuiltinCompatibilityReceiverIndex = receiverIndex;
        break;
      }
    }
  }
  const bool isCanonicalStdVectorMutatorMethodCall =
      stmt.isMethodCall &&
      (vectorHelperResolved == "/std/collections/vector/push" ||
       vectorHelperResolved == "/std/collections/vector/pop" ||
       vectorHelperResolved == "/std/collections/vector/reserve" ||
       vectorHelperResolved == "/std/collections/vector/clear" ||
       vectorHelperResolved == "/std/collections/vector/remove_at" ||
       vectorHelperResolved == "/std/collections/vector/remove_swap");
  if (isCanonicalStdVectorMutatorMethodCall &&
      !hasDeclaredDefinitionPath(vectorHelperResolved) &&
      !hasImportedDefinitionPath(vectorHelperResolved)) {
    error_ = "unknown method: " + vectorHelperResolved;
    return false;
  }
  if (!shouldUseCanonicalBuiltinCompatibilityFallback &&
      (hasDeclaredDefinitionPath(vectorHelperResolved) || hasImportedDefinitionPath(vectorHelperResolved))) {
    Expr helperCall = stmt;
    helperCall.name = vectorHelperResolved;
    helperCall.isMethodCall = false;
    if (helperCall.templateArgs.empty() &&
        vectorHelperResolved.rfind("/std/collections/experimental_vector/", 0) == 0) {
      const size_t receiverIndex = hasResolvedReceiverIndex ? resolvedReceiverIndex : 0;
      if (receiverIndex < stmt.args.size()) {
        BindingInfo inferredBinding;
        bool hasInferredBinding = false;
        const Expr &receiverExpr = stmt.args[receiverIndex];
        if (receiverExpr.kind == Expr::Kind::Name) {
          if (const BindingInfo *paramBinding = findParamBinding(params, receiverExpr.name)) {
            inferredBinding = *paramBinding;
            hasInferredBinding = true;
          } else if (auto it = locals.find(receiverExpr.name); it != locals.end()) {
            inferredBinding = it->second;
            hasInferredBinding = true;
          }
        }
        if (!hasInferredBinding) {
          std::string receiverTypeText;
          if (inferQueryExprTypeText(receiverExpr, params, locals, receiverTypeText)) {
            std::string base;
            std::string argText;
            const std::string normalizedType = normalizeBindingTypeName(receiverTypeText);
            if (splitTemplateTypeName(normalizedType, base, argText)) {
              inferredBinding.typeName = normalizeBindingTypeName(base);
              inferredBinding.typeTemplateArg = argText;
            } else {
              inferredBinding.typeName = normalizedType;
            }
            hasInferredBinding = true;
          }
        }
        if (hasInferredBinding) {
          std::string experimentalElemType;
          if (extractExperimentalVectorElementType(inferredBinding, experimentalElemType)) {
            helperCall.templateArgs = {experimentalElemType};
          }
        }
      }
    }
    if (!isStdNamespacedVectorCanonicalHelperCall &&
        hasResolvedReceiverIndex && resolvedReceiverIndex > 0 && resolvedReceiverIndex < helperCall.args.size()) {
      std::swap(helperCall.args[0], helperCall.args[resolvedReceiverIndex]);
      if (helperCall.argNames.size() < helperCall.args.size()) {
        helperCall.argNames.resize(helperCall.args.size());
      }
      std::swap(helperCall.argNames[0], helperCall.argNames[resolvedReceiverIndex]);
    }
    return validateExpr(params, locals, helperCall, enclosingStatements, statementIndex);
  }
  if (isStdNamespacedVectorCanonicalHelperCall &&
      !shouldAllowStdNamespacedVectorHelperCompatibilityFallback &&
      !shouldUseCanonicalBuiltinCompatibilityFallback) {
    error_ = "unknown call target: " + vectorHelperResolved;
    return false;
  }
  auto validateBuiltinNamedReceiverShape = [&](const std::string &helperName) -> bool {
    if (!hasNamedArguments(stmt.argNames) || stmt.args.empty()) {
      return true;
    }
    size_t receiverIndex = 0;
    bool hasExplicitValuesReceiver = false;
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == "values") {
        receiverIndex = i;
        hasExplicitValuesReceiver = true;
        break;
      }
    }
    if (!hasExplicitValuesReceiver) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        const BindingInfo *candidateBinding = nullptr;
        if (!resolveVectorBinding(stmt.args[i], candidateBinding) || candidateBinding == nullptr) {
          continue;
        }
        const bool allowSoaVectorTarget = helperName == "push" || helperName == "reserve";
        if (candidateBinding->typeName == "vector" ||
            (allowSoaVectorTarget && candidateBinding->typeName == "soa_vector")) {
          receiverIndex = i;
          break;
        }
      }
    }
    if (receiverIndex >= stmt.args.size()) {
      receiverIndex = 0;
    }
    if (!validateVectorHelperReceiver(stmt.args[receiverIndex], helperName)) {
      return false;
    }
    const BindingInfo *binding = nullptr;
    return validateVectorHelperTarget(stmt.args[receiverIndex], helperName.c_str(), binding);
  };
  auto findCanonicalBuiltinCompatibilityOperandIndex = [&](std::string_view namedArg) -> size_t {
    if (!shouldUseCanonicalBuiltinCompatibilityFallback || stmt.args.empty()) {
      return 0;
    }
    if (hasNamedArguments(stmt.argNames)) {
      for (size_t i = 0; i < stmt.args.size(); ++i) {
        if (i == canonicalBuiltinCompatibilityReceiverIndex) {
          continue;
        }
        if (i < stmt.argNames.size() && stmt.argNames[i].has_value() && *stmt.argNames[i] == namedArg) {
          return i;
        }
      }
    }
    for (size_t i = 0; i < stmt.args.size(); ++i) {
      if (i != canonicalBuiltinCompatibilityReceiverIndex) {
        return i;
      }
    }
    return stmt.args.size();
  };
  if (hasNamedArguments(stmt.argNames)) {
    if (!validateBuiltinNamedReceiverShape(vectorHelper)) {
      return false;
    }
    if (shouldUseCanonicalBuiltinCompatibilityFallback) {
      // Imported canonical vector mutators still lower through builtin vector ops on builtin vector receivers.
    } else {
      error_ = "named arguments not supported for builtin calls";
      return false;
    }
  }
  if (!stmt.templateArgs.empty()) {
    error_ = vectorHelper + " does not accept template arguments";
    return false;
  }
  if (stmt.hasBodyArguments || !stmt.bodyArguments.empty()) {
    error_ = vectorHelper + " does not accept block arguments";
    return false;
  }
  if (vectorHelper == "push") {
    if (stmt.args.size() != 2) {
      error_ = "push requires exactly two arguments";
      return false;
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    const size_t valueIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                  ? findCanonicalBuiltinCompatibilityOperandIndex("value")
                                  : 1;
    if (receiverIndex >= stmt.args.size() || valueIndex >= stmt.args.size()) {
      error_ = "push requires exactly two arguments";
      return false;
    }
    if (!validateVectorHelperReceiver(stmt.args[receiverIndex], "push")) {
      return false;
    }
    const BindingInfo *binding = nullptr;
    if (!validateVectorHelperTarget(stmt.args[receiverIndex], "push", binding)) {
      return false;
    }
    if (currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = "push requires heap_alloc effect";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args[valueIndex])) {
      return false;
    }
    if (!validateVectorElementType(stmt.args[valueIndex], binding->typeTemplateArg)) {
      return false;
    }
    if (!validateVectorRelocationHelperElementType(*binding, "push", namespacePrefix, definitionTemplateArgs)) {
      return false;
    }
    return true;
  }
  if (vectorHelper == "reserve") {
    if (stmt.args.size() != 2) {
      error_ = "reserve requires exactly two arguments";
      return false;
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    const size_t capacityIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? findCanonicalBuiltinCompatibilityOperandIndex("capacity")
                                     : 1;
    if (receiverIndex >= stmt.args.size() || capacityIndex >= stmt.args.size()) {
      error_ = "reserve requires exactly two arguments";
      return false;
    }
    if (!validateVectorHelperReceiver(stmt.args[receiverIndex], "reserve")) {
      return false;
    }
    const BindingInfo *binding = nullptr;
    if (!validateVectorHelperTarget(stmt.args[receiverIndex], "reserve", binding)) {
      return false;
    }
    if (currentValidationContext_.activeEffects.count("heap_alloc") == 0) {
      error_ = "reserve requires heap_alloc effect";
      return false;
    }
    if (!validateExpr(params, locals, stmt.args[capacityIndex])) {
      return false;
    }
    if (!isIntegerExpr(stmt.args[capacityIndex])) {
      error_ = "reserve requires integer capacity";
      return false;
    }
    if (!validateVectorRelocationHelperElementType(*binding, "reserve", namespacePrefix, definitionTemplateArgs)) {
      return false;
    }
    return true;
  }
  if (vectorHelper == "remove_at" || vectorHelper == "remove_swap") {
    if (stmt.args.size() != 2) {
      error_ = vectorHelper + " requires exactly two arguments";
      return false;
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    const size_t indexArgIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? findCanonicalBuiltinCompatibilityOperandIndex("index")
                                     : 1;
    if (receiverIndex >= stmt.args.size() || indexArgIndex >= stmt.args.size()) {
      error_ = vectorHelper + " requires exactly two arguments";
      return false;
    }
    if (!validateVectorHelperReceiver(stmt.args[receiverIndex], vectorHelper)) {
      return false;
    }
    const BindingInfo *binding = nullptr;
    if (!validateVectorHelperTarget(stmt.args[receiverIndex], vectorHelper.c_str(), binding)) {
      return false;
    }
    if (!validateExpr(params, locals, stmt.args[indexArgIndex])) {
      return false;
    }
    if (!isIntegerExpr(stmt.args[indexArgIndex])) {
      error_ = vectorHelper + " requires integer index";
      return false;
    }
    if (!validateVectorIndexedRemovalHelperElementType(*binding, vectorHelper, namespacePrefix, definitionTemplateArgs)) {
      return false;
    }
    return true;
  }
  if (vectorHelper == "pop" || vectorHelper == "clear") {
    if (stmt.args.size() != 1) {
      error_ = vectorHelper + " requires exactly one argument";
      return false;
    }
    const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback
                                     ? canonicalBuiltinCompatibilityReceiverIndex
                                     : 0;
    if (receiverIndex >= stmt.args.size()) {
      error_ = vectorHelper + " requires exactly one argument";
      return false;
    }
    if (!validateVectorHelperReceiver(stmt.args[receiverIndex], vectorHelper)) {
      return false;
    }
    const BindingInfo *binding = nullptr;
    if (!validateVectorHelperTarget(stmt.args[receiverIndex], vectorHelper.c_str(), binding)) {
      return false;
    }
    if (!validateVectorDiscardHelperElementType(*binding, vectorHelper, namespacePrefix, definitionTemplateArgs)) {
      return false;
    }
    return true;
  }
  return true;
}

} // namespace primec::semantics
