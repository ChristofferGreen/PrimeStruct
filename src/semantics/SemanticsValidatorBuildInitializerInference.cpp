#include "SemanticsValidator.h"
#include "MapConstructorHelpers.h"

namespace primec::semantics {
namespace {

std::string bindingTypeText(const BindingInfo &binding) {
  if (binding.typeTemplateArg.empty()) {
    return binding.typeName;
  }
  return binding.typeName + "<" + binding.typeTemplateArg + ">";
}

} // namespace

bool SemanticsValidator::graphBindingIsUsable(const BindingInfo &binding) const {
  const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
  if (normalizedType.empty() || normalizedType == "auto") {
    return false;
  }
  if (normalizedType == "array" && binding.typeTemplateArg.empty()) {
    return false;
  }
  if (!binding.typeTemplateArg.empty()) {
    return true;
  }
  if (normalizedType == "Result" || normalizedType == "Pointer" || normalizedType == "Reference") {
    return true;
  }
  if (returnKindForTypeName(normalizedType) != ReturnKind::Unknown) {
    return true;
  }
  if (structNames_.count(binding.typeName) > 0) {
    return true;
  }
  std::string scopeNamespace;
  const auto scopeIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (scopeIt != defMap_.end() && scopeIt->second != nullptr) {
    scopeNamespace = scopeIt->second->namespacePrefix;
  }
  if (!resolveStructTypePath(normalizedType, scopeNamespace, structNames_).empty()) {
    return true;
  }
  auto importIt = importAliases_.find(normalizedType);
  return importIt != importAliases_.end() && structNames_.count(importIt->second) > 0;
}

bool SemanticsValidator::shouldBypassGraphBindingLookup(const Expr &candidate) const {
  if (candidate.args.size() != 1 || candidate.args.front().kind != Expr::Kind::Call) {
    return false;
  }
  const Expr &initializerCall = candidate.args.front();
  std::string collectionName;
  if (getBuiltinCollectionName(initializerCall, collectionName)) {
    return true;
  }
  std::string normalizedName = initializerCall.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = initializerCall.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  if (normalizedName == "vector/vector" ||
      normalizedName == "vector" ||
      (normalizedPrefix == "vector" && normalizedName == "vector") ||
      (normalizedPrefix == "std/collections/vector" && normalizedName == "vector")) {
    return true;
  }
  return false;
}

std::string SemanticsValidator::preferredCollectionHelperResolvedPath(
    const Expr &initializerCall) const {
  if (initializerCall.kind != Expr::Kind::Call || initializerCall.isMethodCall) {
    return {};
  }

  std::string normalizedName = initializerCall.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = initializerCall.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }

  auto explicitStdMapHelperName = [&]() -> std::string {
    if (normalizedPrefix == "std/collections/map" &&
        (normalizedName == "count" || normalizedName == "count_ref" ||
         normalizedName == "contains" || normalizedName == "contains_ref" ||
         normalizedName == "tryAt" || normalizedName == "tryAt_ref" ||
         normalizedName == "at" || normalizedName == "at_ref" ||
         normalizedName == "at_unsafe" || normalizedName == "at_unsafe_ref" ||
         normalizedName == "insert" || normalizedName == "insert_ref")) {
      return normalizedName;
    }
    if (normalizedName.rfind("std/collections/map/", 0) == 0) {
      const std::string helperName =
          normalizedName.substr(std::string("std/collections/map/").size());
      if (helperName == "count" || helperName == "count_ref" ||
          helperName == "contains" || helperName == "contains_ref" ||
          helperName == "tryAt" || helperName == "tryAt_ref" ||
          helperName == "at" || helperName == "at_ref" ||
          helperName == "at_unsafe" || helperName == "at_unsafe_ref" ||
          helperName == "insert" || helperName == "insert_ref") {
        return helperName;
      }
    }
    return {};
  };
  auto explicitStdVectorHelperName = [&]() -> std::string {
    if (normalizedPrefix == "std/collections/vector" &&
        (normalizedName == "at" || normalizedName == "at_unsafe" ||
         normalizedName == "count" || normalizedName == "capacity" ||
         normalizedName == "push" || normalizedName == "pop" ||
         normalizedName == "reserve" || normalizedName == "clear" ||
         normalizedName == "remove_at" || normalizedName == "remove_swap")) {
      return normalizedName;
    }
    if (normalizedName.rfind("std/collections/vector/", 0) == 0) {
      const std::string helperName =
          normalizedName.substr(std::string("std/collections/vector/").size());
      if (helperName == "at" || helperName == "at_unsafe" ||
          helperName == "count" || helperName == "capacity" ||
          helperName == "push" || helperName == "pop" ||
          helperName == "reserve" || helperName == "clear" ||
          helperName == "remove_at" || helperName == "remove_swap") {
        return helperName;
      }
    }
    return {};
  };

  if (const std::string helperName = explicitStdMapHelperName();
      !helperName.empty()) {
    const std::string canonical = "/std/collections/map/" + helperName;
    const bool prefersAlias = helperName == "at" || helperName == "at_ref" ||
                              helperName == "at_unsafe" ||
                              helperName == "at_unsafe_ref";
    const std::string alias = "/map/" + helperName;
    if (prefersAlias && defMap_.count(alias) > 0) {
      return alias;
    }
    if (defMap_.count(canonical) > 0) {
      return canonical;
    }
    if (defMap_.count(alias) > 0) {
      return alias;
    }
    return {};
  }

  if (const std::string helperName = explicitStdVectorHelperName();
      !helperName.empty()) {
    const std::string alias = "/vector/" + helperName;
    if (defMap_.count(alias) > 0) {
      return alias;
    }
    const std::string canonical = "/std/collections/vector/" + helperName;
    if (defMap_.count(canonical) > 0) {
      return canonical;
    }
    return {};
  }

  return {};
}

std::optional<std::string> SemanticsValidator::builtinSoaAccessHelperName(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
    return std::nullopt;
  }

  auto isDirectSoaVectorTarget = [&](const Expr &target) {
    auto isDirectSoaBinding = [&](const BindingInfo &binding) {
      if (normalizeBindingTypeName(binding.typeName) == "soa_vector") {
        return true;
      }
      std::string elemType;
      return extractExperimentalSoaVectorElementType(binding, elemType);
    };
    if (target.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, target.name)) {
        return isDirectSoaBinding(*paramBinding);
      }
      auto localIt = locals.find(target.name);
      return localIt != locals.end() &&
             isDirectSoaBinding(localIt->second);
    }
    std::string builtinCollection;
    return getBuiltinCollectionName(target, builtinCollection) &&
           builtinCollection == "soa_vector";
  };

  const std::string resolved = resolveCalleePath(candidate);
  const auto canonicalizeSoaResolvedPath = [](std::string_view path) -> std::string {
    std::string resolvedPath(path);
    const size_t templateSuffix = resolvedPath.find("__t");
    if (templateSuffix != std::string::npos) {
      resolvedPath.erase(templateSuffix);
    }
    if (resolvedPath == "/soa_vector/ref") {
      return "/std/collections/soa_vector/ref";
    }
    if (resolvedPath == "/soa_vector/ref_ref") {
      return "/std/collections/soa_vector/ref_ref";
    }
    if (resolvedPath == "/soa_vector/get") {
      return "/std/collections/soa_vector/get";
    }
    if (resolvedPath == "/soa_vector/get_ref") {
      return "/std/collections/soa_vector/get_ref";
    }
    return resolvedPath;
  };
  const std::string resolvedCanonical = canonicalizeSoaResolvedPath(resolved);
  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }

  const bool isExplicitSoaRefCall =
      (!candidate.isMethodCall && normalizedPrefix == "soa_vector" &&
       normalizedName == "ref") ||
      normalizedName == "soa_vector/ref";
  const bool isBuiltinSoaRefMethod =
      candidate.isMethodCall && normalizedName == "ref" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  if (resolvedCanonical == "/std/collections/soa_vector/ref" ||
      isExplicitSoaRefCall ||
      isBuiltinSoaRefMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, "ref"))) {
    return std::string("ref");
  }
  const bool isExplicitSoaRefRefCall =
      (!candidate.isMethodCall && normalizedPrefix == "soa_vector" &&
       normalizedName == "ref_ref") ||
      normalizedName == "soa_vector/ref_ref";
  const bool isBuiltinSoaRefRefMethod =
      candidate.isMethodCall && normalizedName == "ref_ref" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  if (resolvedCanonical == "/std/collections/soa_vector/ref_ref" ||
      isExplicitSoaRefRefCall ||
      isBuiltinSoaRefRefMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, "ref_ref"))) {
    return std::string("ref_ref");
  }

  const bool isExplicitSoaGetCall =
      (!candidate.isMethodCall && normalizedPrefix == "soa_vector" &&
       normalizedName == "get") ||
      normalizedName == "soa_vector/get";
  const bool isBuiltinSoaGetMethod =
      candidate.isMethodCall && normalizedName == "get" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  if (resolvedCanonical == "/std/collections/soa_vector/get" ||
      isExplicitSoaGetCall ||
      isBuiltinSoaGetMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, "get"))) {
    return std::string("get");
  }
  const bool isExplicitSoaGetRefCall =
      (!candidate.isMethodCall && normalizedPrefix == "soa_vector" &&
       normalizedName == "get_ref") ||
      normalizedName == "soa_vector/get_ref";
  const bool isBuiltinSoaGetRefMethod =
      candidate.isMethodCall && normalizedName == "get_ref" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  if (resolvedCanonical == "/std/collections/soa_vector/get_ref" ||
      isExplicitSoaGetRefCall ||
      isBuiltinSoaGetRefMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, "get_ref"))) {
    return std::string("get_ref");
  }

  return std::nullopt;
}

bool SemanticsValidator::isBuiltinSoaFieldViewExpr(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    std::string *fieldNameOut) const {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding) {
    return false;
  }

  auto assignBindingTypeFromText = [](const std::string &typeText, BindingInfo &bindingOut) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto inferSoaReceiverBinding = [&](const Expr &receiver, BindingInfo &bindingOut) -> bool {
    if (receiver.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, receiver.name)) {
        bindingOut = *paramBinding;
        return true;
      }
      auto localIt = locals.find(receiver.name);
      if (localIt != locals.end()) {
        bindingOut = localIt->second;
        return true;
      }
    }
    if (const_cast<SemanticsValidator *>(this)->inferBindingTypeFromInitializer(
            receiver, params, locals, bindingOut)) {
      return true;
    }
    std::string inferredTypeText;
    return const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(
               receiver, params, locals, inferredTypeText) &&
           assignBindingTypeFromText(inferredTypeText, bindingOut);
  };
  auto resolveDirectSoaReceiver = [&](const Expr &receiver,
                                      std::string &elemTypeOut) -> bool {
    BindingInfo receiverBinding;
    if (!inferSoaReceiverBinding(receiver, receiverBinding)) {
      return false;
    }
    if (normalizeBindingTypeName(receiverBinding.typeName) == "soa_vector" &&
        !receiverBinding.typeTemplateArg.empty()) {
      elemTypeOut = receiverBinding.typeTemplateArg;
      return true;
    }
    return extractExperimentalSoaVectorElementType(receiverBinding,
                                                   elemTypeOut) &&
           !elemTypeOut.empty();
  };

  const std::string resolved = resolveCalleePath(candidate);
  if (splitSoaFieldViewHelperPath(resolved, fieldNameOut)) {
    return true;
  }

  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  if (normalizedName.empty() || normalizedName == "count" || normalizedName == "get" ||
      normalizedName == "ref" || normalizedName == "to_soa" ||
      normalizedName == "to_aos" || normalizedName == "to_aos_ref") {
    return false;
  }

  if (hasVisibleSoaHelperTargetForCurrentImports(normalizedName)) {
    return false;
  }

  const Expr *receiver = nullptr;
  if (candidate.isMethodCall) {
    if (candidate.args.empty()) {
      return false;
    }
    receiver = &candidate.args.front();
  } else {
    if (!normalizedPrefix.empty() || candidate.args.size() != 1) {
      return false;
    }
    receiver = &candidate.args.front();
  }

  std::string receiverElemType;
  if (!const_cast<SemanticsValidator *>(this)
           ->resolveSoaVectorOrExperimentalBorrowedReceiver(
               *receiver,
               params,
               locals,
               resolveDirectSoaReceiver,
               receiverElemType) ||
      receiverElemType.empty()) {
    return false;
  }

  std::string currentNamespace;
  if (!currentValidationState_.context.definitionPath.empty()) {
    const size_t slash = currentValidationState_.context.definitionPath.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
      currentNamespace = currentValidationState_.context.definitionPath.substr(0, slash);
    }
  }
  const std::string lookupNamespace =
      !receiver->namespacePrefix.empty() ? receiver->namespacePrefix : currentNamespace;
  const std::string elementStructPath =
      resolveStructTypePath(normalizeBindingTypeName(receiverElemType),
                            lookupNamespace,
                            structNames_);
  auto structIt = defMap_.find(elementStructPath);
  if (elementStructPath.empty() || structIt == defMap_.end() || structIt->second == nullptr) {
    return false;
  }

  for (const auto &stmt : structIt->second->statements) {
    if (stmt.isBinding && stmt.name == normalizedName) {
      if (fieldNameOut != nullptr) {
        *fieldNameOut = normalizedName;
      }
      return true;
    }
  }
  return false;
}

std::optional<std::string> SemanticsValidator::builtinSoaDirectPendingHelperPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  auto assignBindingTypeFromText = [](const std::string &typeText,
                                      BindingInfo &bindingOut) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto isExperimentalSoaLikeExpr = [&](const Expr &expr) {
    auto bindingIsExperimentalSoaLike = [&](const BindingInfo &binding) {
      std::string elemType;
      return extractExperimentalSoaVectorElementType(binding, elemType);
    };
    if (expr.kind == Expr::Kind::Name) {
      if (const BindingInfo *paramBinding = findParamBinding(params, expr.name)) {
        return bindingIsExperimentalSoaLike(*paramBinding);
      }
      auto localIt = locals.find(expr.name);
      return localIt != locals.end() &&
             bindingIsExperimentalSoaLike(localIt->second);
    }
    BindingInfo inferredBinding;
    if (const_cast<SemanticsValidator *>(this)->inferBindingTypeFromInitializer(
            expr, params, locals, inferredBinding)) {
      return bindingIsExperimentalSoaLike(inferredBinding);
    }
    std::string inferredTypeText;
    return const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(
               expr, params, locals, inferredTypeText) &&
           assignBindingTypeFromText(inferredTypeText, inferredBinding) &&
           bindingIsExperimentalSoaLike(inferredBinding);
  };

  std::string fieldName;
  if (isBuiltinSoaFieldViewExpr(candidate, params, locals, &fieldName)) {
    return soaFieldViewHelperPath(fieldName);
  }
  const auto soaAccessHelper =
      builtinSoaAccessHelperName(candidate, params, locals);
  if (soaAccessHelper.has_value() &&
      (*soaAccessHelper == "ref" || *soaAccessHelper == "ref_ref") &&
      !candidate.args.empty() &&
      !isExperimentalSoaLikeExpr(candidate.args.front()) &&
      !hasVisibleSoaHelperTargetForCurrentImports(*soaAccessHelper)) {
    return std::string("/std/collections/soa_vector/") + *soaAccessHelper;
  }
  return std::nullopt;
}

bool SemanticsValidator::hasVisibleDefinitionPathForCurrentImports(
    std::string_view path) const {
  const std::string ownedPath(path);
  return hasImportedDefinitionPath(ownedPath) ||
         hasDeclaredDefinitionPath(ownedPath);
}

std::string SemanticsValidator::preferredSoaHelperTargetForCurrentImports(
    std::string_view helperName) const {
  const std::string helper(helperName);
  const std::string samePath =
      helper == "to_aos" ? "/to_aos" : "/soa_vector/" + helper;
  if (hasVisibleDefinitionPathForCurrentImports(samePath)) {
    return samePath;
  }
  return "/std/collections/soa_vector/" + helper;
}

bool SemanticsValidator::usesSamePathSoaHelperTargetForCurrentImports(
    std::string_view helperName) const {
  const std::string helper(helperName);
  const std::string samePath =
      helper == "to_aos" ? "/to_aos" : "/soa_vector/" + helper;
  return preferredSoaHelperTargetForCurrentImports(helperName) == samePath;
}

bool SemanticsValidator::hasVisibleSoaHelperTargetForCurrentImports(
    std::string_view helperName) const {
  const std::string helper(helperName);
  const std::string samePath =
      helper == "to_aos" ? "/to_aos" : "/soa_vector/" + helper;
  const std::string canonicalPath = "/std/collections/soa_vector/" + helper;
  return hasVisibleDefinitionPathForCurrentImports(samePath) ||
         hasVisibleDefinitionPathForCurrentImports(canonicalPath);
}

std::string SemanticsValidator::preferredSoaHelperTargetForCollectionType(
    std::string_view helperName,
    std::string_view collectionTypePath) const {
  const std::string helper(helperName);
  const std::string samePath =
      helper == "to_aos" ? "/to_aos" : "/soa_vector/" + helper;
  const std::string preferredTarget =
      preferredSoaHelperTargetForCurrentImports(helperName);
  if (preferredTarget != samePath) {
    return preferredTarget;
  }
  auto paramsIt = paramsByDef_.find(samePath);
  if (paramsIt == paramsByDef_.end() || paramsIt->second.empty()) {
    return "/std/collections/soa_vector/" + helper;
  }
  const BindingInfo &receiverBinding = paramsIt->second.front().binding;
  const std::string receiverTypeText =
      receiverBinding.typeTemplateArg.empty()
          ? receiverBinding.typeName
          : receiverBinding.typeName + "<" + receiverBinding.typeTemplateArg + ">";
  std::string resolvedCollectionType =
      inferMethodCollectionTypePathFromTypeText(receiverTypeText);
  if (resolvedCollectionType.empty() && collectionTypePath == "/soa_vector") {
    std::string elemType;
    if (extractExperimentalSoaVectorElementType(receiverBinding, elemType)) {
      resolvedCollectionType = "/soa_vector";
    }
  }
  if (resolvedCollectionType == collectionTypePath) {
    return samePath;
  }
  return "/std/collections/soa_vector/" + helper;
}

bool SemanticsValidator::usesSamePathSoaHelperTargetForCollectionType(
    std::string_view helperName,
    std::string_view collectionTypePath) const {
  const std::string helper(helperName);
  const std::string samePath =
      helper == "to_aos" ? "/to_aos" : "/soa_vector/" + helper;
  return preferredSoaHelperTargetForCollectionType(helperName,
                                                   collectionTypePath) ==
         samePath;
}

bool SemanticsValidator::hasDirectExperimentalVectorImport() const {
  const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
  for (const auto &importPath : importPaths) {
    if (importPath == "/std/collections/experimental_vector/*" ||
        importPath == "/std/collections/experimental_vector/vector" ||
        importPath == "/std/collections/experimental_vector") {
      return true;
    }
  }
  return false;
}

bool SemanticsValidator::canonicalizeInferredCollectionBinding(
    const Expr *sourceExpr,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut) {
  auto canonicalizeResolvedPath = [](std::string path) {
    const size_t suffix = path.find("__t");
    if (suffix != std::string::npos) {
      path.erase(suffix);
    }
    return path;
  };
  auto preferResolvedCollectionBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call) {
      return false;
    }
    std::string builtinCollectionName;
    if (getBuiltinCollectionName(candidate, builtinCollectionName)) {
      return false;
    }
    const std::string resolvedCandidate = canonicalizeResolvedPath(resolveCalleePath(candidate));
    if (resolvedCandidate.empty()) {
      return false;
    }
    BindingInfo resolvedBinding;
    if (!inferResolvedDirectCallBindingType(resolvedCandidate, resolvedBinding)) {
      return false;
    }
    if (normalizeCollectionTypePath(bindingTypeText(resolvedBinding)).empty()) {
      return false;
    }
    bindingOut = std::move(resolvedBinding);
    return true;
  };
  auto inferDirectMapConstructorBinding = [&](const Expr &candidate) -> bool {
    if (candidate.kind != Expr::Kind::Call || !isResolvedMapConstructorPath(resolveCalleePath(candidate))) {
      return false;
    }
    if (candidate.templateArgs.size() == 2) {
      bindingOut.typeName = "Map";
      bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
      return true;
    }
    if (candidate.args.size() < 2 || candidate.args.size() % 2 != 0) {
      return false;
    }
    auto inferArgumentTypeText = [&](const Expr &argument, std::string &typeTextOut) -> bool {
      typeTextOut.clear();
      if (inferQueryExprTypeText(argument, params, locals, typeTextOut) && !typeTextOut.empty()) {
        typeTextOut = normalizeBindingTypeName(typeTextOut);
        return true;
      }
      const ReturnKind argumentKind = inferExprReturnKind(argument, params, locals);
      if (argumentKind == ReturnKind::Unknown || argumentKind == ReturnKind::Void) {
        return false;
      }
      typeTextOut = typeNameForReturnKind(argumentKind);
      return !typeTextOut.empty();
    };
    std::string keyTypeText;
    std::string valueTypeText;
    if (!inferArgumentTypeText(candidate.args[0], keyTypeText) ||
        !inferArgumentTypeText(candidate.args[1], valueTypeText)) {
      return false;
    }
    bindingOut.typeName = "Map";
    bindingOut.typeTemplateArg = keyTypeText + ", " + valueTypeText;
    return true;
  };
  auto applySourceExprCollectionTemplateArgs = [&](const Expr &candidate) -> bool {
    std::vector<std::string> collectionArgs;
    if (resolveCallCollectionTemplateArgs(candidate,
                                         bindingOut.typeName,
                                         params,
                                         locals,
                                         collectionArgs) &&
        !collectionArgs.empty()) {
      bindingOut.typeTemplateArg = joinTemplateArgs(collectionArgs);
      return true;
    }
    std::string collectionName;
    if (!getBuiltinCollectionName(candidate, collectionName)) {
      return false;
    }
    if ((collectionName == "array" || collectionName == "vector" || collectionName == "soa_vector") &&
        candidate.templateArgs.size() == 1) {
      bindingOut.typeName = collectionName;
      bindingOut.typeTemplateArg = candidate.templateArgs.front();
      return true;
    }
    if (collectionName == "map" && candidate.templateArgs.size() == 2) {
      bindingOut.typeName = "map";
      bindingOut.typeTemplateArg = joinTemplateArgs(candidate.templateArgs);
      return true;
    }
    return false;
  };
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };

  if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call &&
      inferDirectMapConstructorBinding(*sourceExpr)) {
    return true;
  }
  if (sourceExpr != nullptr && preferResolvedCollectionBinding(*sourceExpr)) {
    return true;
  }
  const std::string normalizedBindingType = normalizeBindingTypeName(bindingOut.typeName);
  if ((normalizedBindingType == "Vector" && !bindingOut.typeTemplateArg.empty()) ||
      normalizedBindingType.rfind("/std/collections/experimental_vector/Vector__", 0) == 0 ||
      normalizedBindingType.rfind("std/collections/experimental_vector/Vector__", 0) == 0) {
    return true;
  }
  if ((normalizedBindingType == "Map" && !bindingOut.typeTemplateArg.empty()) ||
      normalizedBindingType.rfind("/std/collections/experimental_map/Map__", 0) == 0 ||
      normalizedBindingType.rfind("std/collections/experimental_map/Map__", 0) == 0) {
    return true;
  }
  const std::string normalizedCollectionType = normalizeCollectionTypePath(bindingTypeText(bindingOut));
  if (normalizedCollectionType.empty()) {
    return false;
  }
  if (sourceExpr != nullptr) {
    std::string inferredTypeText;
    if (inferQueryExprTypeText(*sourceExpr, params, locals, inferredTypeText) &&
        assignBindingTypeFromText(inferredTypeText)) {
      (void)applySourceExprCollectionTemplateArgs(*sourceExpr);
      return true;
    }
  }
  bindingOut.typeName = normalizedCollectionType.substr(1);
  bindingOut.typeTemplateArg.clear();
  if (sourceExpr != nullptr && sourceExpr->kind == Expr::Kind::Call) {
    if (applySourceExprCollectionTemplateArgs(*sourceExpr)) {
      return true;
    }
  }
  return true;
}

bool SemanticsValidator::inferBindingTypeFromInitializer(
    const Expr &initializer,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals,
    BindingInfo &bindingOut,
    const Expr *bindingExpr) {
  auto assignBindingTypeFromText = [&](const std::string &typeText) -> bool {
    const std::string normalizedType = normalizeBindingTypeName(typeText);
    if (normalizedType.empty()) {
      return false;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedType, base, argText)) {
      bindingOut.typeName = normalizeBindingTypeName(base);
      bindingOut.typeTemplateArg = argText;
      return true;
    }
    bindingOut.typeName = normalizedType;
    bindingOut.typeTemplateArg.clear();
    return true;
  };
  auto assignBindingTypeFromResultInfo = [&](const ResultTypeInfo &resultInfo) -> bool {
    if (!resultInfo.isResult || resultInfo.errorType.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    if (!resultInfo.hasValue) {
      bindingOut.typeTemplateArg = resultInfo.errorType;
      return true;
    }
    if (resultInfo.valueType.empty()) {
      return false;
    }
    bindingOut.typeTemplateArg = resultInfo.valueType + ", " + resultInfo.errorType;
    return true;
  };
  if (initializer.isLambda) {
    bindingOut.typeName = "lambda";
    bindingOut.typeTemplateArg.clear();
    return true;
  }
  auto inferUninitializedTakeBorrowBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall ||
        initializer.args.size() != 1 ||
        (!isSimpleCallName(initializer, "take") &&
         !isSimpleCallName(initializer, "borrow"))) {
      return false;
    }
    BindingInfo storageBinding;
    bool resolvedStorage = false;
    if (!resolveUninitializedStorageBinding(params,
                                           locals,
                                           initializer.args.front(),
                                           storageBinding,
                                           resolvedStorage) ||
        !resolvedStorage || storageBinding.typeName != "uninitialized" ||
        storageBinding.typeTemplateArg.empty()) {
      return false;
    }
    return assignBindingTypeFromText(storageBinding.typeTemplateArg);
  };
  if (bindingExpr != nullptr && !shouldBypassGraphBindingLookup(*bindingExpr) &&
      lookupGraphLocalAutoBinding(*bindingExpr, bindingOut)) {
    if (graphBindingIsUsable(bindingOut)) {
      return true;
    }
    bindingOut = {};
  }
  if (initializer.kind == Expr::Kind::Call &&
      !initializer.isMethodCall &&
      initializer.templateArgs.size() == 1 &&
      resolveCalleePath(initializer) == "/vector") {
    if (!hasDirectExperimentalVectorImport()) {
      bindingOut.typeName = "vector";
      bindingOut.typeTemplateArg = initializer.templateArgs.front();
      return true;
    }
    bindingOut.typeName = "Vector";
    bindingOut.typeTemplateArg = initializer.templateArgs.front();
    return true;
  }
  auto inferTryInitializerBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || initializer.isMethodCall || !isSimpleCallName(initializer, "try") ||
        initializer.args.size() != 1 || !initializer.templateArgs.empty() || initializer.hasBodyArguments ||
        !initializer.bodyArguments.empty()) {
      return false;
    }
    ResultTypeInfo resultInfo;
    if (!resolveResultTypeForExpr(initializer.args.front(), params, locals, resultInfo) || !resultInfo.isResult ||
        !resultInfo.hasValue || resultInfo.valueType.empty()) {
      return false;
    }
    return assignBindingTypeFromText(resultInfo.valueType);
  };
  if (inferUninitializedTakeBorrowBinding()) {
    return true;
  }
  auto inferDirectResultOkBinding = [&]() -> bool {
    if (initializer.kind != Expr::Kind::Call || !initializer.isMethodCall || initializer.name != "ok" ||
        initializer.templateArgs.size() != 0 || initializer.hasBodyArguments || !initializer.bodyArguments.empty()) {
      return false;
    }
    if (initializer.args.empty()) {
      return false;
    }
    const Expr &receiver = initializer.args.front();
    if (receiver.kind != Expr::Kind::Name || normalizeBindingTypeName(receiver.name) != "Result") {
      return false;
    }
    auto inferCurrentErrorType = [&]() -> std::string {
      if (currentValidationState_.context.resultType.has_value() &&
          currentValidationState_.context.resultType->isResult &&
          !currentValidationState_.context.resultType->errorType.empty()) {
        return currentValidationState_.context.resultType->errorType;
      }
      if (currentValidationState_.context.onError.has_value() &&
          !currentValidationState_.context.onError->errorType.empty()) {
        return currentValidationState_.context.onError->errorType;
      }
      return "_";
    };
    if (initializer.args.size() == 1) {
      bindingOut.typeName = "Result";
      bindingOut.typeTemplateArg = inferCurrentErrorType();
      return true;
    }
    if (initializer.args.size() != 2) {
      return false;
    }
    BindingInfo payloadBinding;
    if (!inferBindingTypeFromInitializer(initializer.args.back(), params, locals, payloadBinding)) {
      return false;
    }
    const std::string payloadTypeText = bindingTypeText(payloadBinding);
    if (payloadTypeText.empty()) {
      return false;
    }
    bindingOut.typeName = "Result";
    bindingOut.typeTemplateArg = payloadTypeText + ", " + inferCurrentErrorType();
    return true;
  };
  if (initializer.kind == Expr::Kind::Call) {
    if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
      return true;
    }
    const BindingInfo previousBinding = bindingOut;
    bindingOut = {};
    if (inferCallInitializerBinding(initializer, params, locals, bindingOut, bindingExpr)) {
      (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
      if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
        return true;
      }
    }
    bindingOut = previousBinding;
  }
  if (tryInferBindingTypeFromInitializer(initializer, params, locals, bindingOut, hasAnyMathImport())) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    if (!(bindingOut.typeName == "array" && bindingOut.typeTemplateArg.empty())) {
      return true;
    }
    if (inferCallInitializerBinding(initializer, params, locals, bindingOut, bindingExpr)) {
      (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
      return true;
    }
    if (inferBuiltinCollectionValueBinding(initializer, params, locals, bindingOut)) {
      return true;
    }
    if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
      return true;
    }
    return true;
  }
  if (inferTryInitializerBinding()) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    return true;
  }
  if (inferDirectResultOkBinding()) {
    return true;
  }
  ResultTypeInfo resultInfo;
  if (resolveResultTypeForExpr(initializer, params, locals, resultInfo) &&
      assignBindingTypeFromResultInfo(resultInfo)) {
    return true;
  }
  if (inferCallInitializerBinding(initializer, params, locals, bindingOut, bindingExpr)) {
    (void)canonicalizeInferredCollectionBinding(&initializer, params, locals, bindingOut);
    return true;
  }
  if (inferBuiltinCollectionValueBinding(initializer, params, locals, bindingOut)) {
    return true;
  }
  if (inferBuiltinPointerBinding(initializer, params, locals, bindingOut)) {
    return true;
  }
  ReturnKind kind = inferExprReturnKind(initializer, params, locals);
  if (kind == ReturnKind::Unknown || kind == ReturnKind::Void) {
    return false;
  }
  std::string inferred = typeNameForReturnKind(kind);
  if (inferred.empty()) {
    return false;
  }
  bindingOut.typeName = inferred;
  bindingOut.typeTemplateArg.clear();
  return true;
}

}  // namespace primec::semantics
