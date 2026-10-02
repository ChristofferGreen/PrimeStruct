// soa-surface-audit: exempt
#include "SemanticsValidator.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/CollectionHelperNames.h"

#include <algorithm>
#include <limits>
#include "SemanticsValidatorBuildInitializerInferenceHelpers.h"

namespace primec::semantics {
using namespace buildInitializerHelpers;

std::optional<std::string> SemanticsValidator::builtinSoaDirectPendingHelperPath(
    const Expr &candidate,
    const std::vector<ParameterInfo> &params,
    const std::unordered_map<std::string, BindingInfo> &locals) const {
  std::string fieldName;
  if (isBuiltinSoaFieldViewExpr(candidate, params, locals, &fieldName)) {
    return soaFieldViewHelperPath(fieldName);
  }
  if (splitSoaFieldViewHelperPath(candidate.name, &fieldName)) {
    return soaFieldViewHelperPath(fieldName);
  }
  auto fieldViewPathFromExperimentalHelper = [&]() -> std::optional<std::string> {
    if (candidate.kind != Expr::Kind::Call || candidate.args.size() < 2 ||
        candidate.args[1].kind != Expr::Kind::Literal) {
      return std::nullopt;
    }
    std::string resolvedPath = preferredCollectionHelperResolvedPath(candidate);
    if (resolvedPath.empty()) {
      resolvedPath = resolveCalleePath(candidate);
    }
    std::string resolvedPathBase = resolvedPath;
    if (const size_t suffix = resolvedPathBase.find("__");
        suffix != std::string::npos) {
      resolvedPathBase.erase(suffix);
    }
    if (!isExperimentalSoaFieldViewHelperPath(resolvedPath)) {
      return std::nullopt;
    }
    if (candidate.args[1].literalValue >
        static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
      return std::nullopt;
    }
    auto inferStructTypeText = [&]() -> std::optional<std::string> {
      if (!candidate.templateArgs.empty()) {
        return candidate.templateArgs.front();
      }
      auto withPreservedError = [&](const std::function<bool()> &fn) {
        const std::string previousError =
            const_cast<SemanticsValidator *>(this)->error_;
        const_cast<SemanticsValidator *>(this)->error_.clear();
        const bool ok = fn();
        const_cast<SemanticsValidator *>(this)->error_.clear();
        const_cast<SemanticsValidator *>(this)->error_ = previousError;
        return ok;
      };
      const Expr &receiverExpr = candidate.args.front();
      auto extractReceiverStructType = [&](const BindingInfo &binding)
          -> std::optional<std::string> {
        if (normalizeBindingTypeName(binding.typeName) == "soa" &&
            !binding.typeTemplateArg.empty()) {
          return binding.typeTemplateArg;
        }
        std::string elemType;
        if (extractExperimentalSoaVectorElementType(binding, elemType)) {
          return elemType;
        }
        return std::nullopt;
      };
      if (receiverExpr.kind == Expr::Kind::Name) {
        if (const BindingInfo *paramBinding =
                findParamBinding(params, receiverExpr.name)) {
          return extractReceiverStructType(*paramBinding);
        }
        auto localIt = locals.find(receiverExpr.name);
        if (localIt != locals.end()) {
          return extractReceiverStructType(localIt->second);
        }
      }
      BindingInfo receiverBinding;
      if (withPreservedError([&]() {
            return const_cast<SemanticsValidator *>(this)
                ->inferBindingTypeFromInitializer(receiverExpr,
                                                  params,
                                                  locals,
                                                  receiverBinding);
          })) {
        if (const auto elemType = extractReceiverStructType(receiverBinding)) {
          return elemType;
        }
      }
      std::string inferredTypeText;
      if (withPreservedError([&]() {
            return const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(
                receiverExpr, params, locals, inferredTypeText);
          })) {
        BindingInfo inferredBinding;
        std::string base;
        std::string argText;
        const std::string normalizedType =
            normalizeBindingTypeName(inferredTypeText);
        if (splitTemplateTypeName(normalizedType, base, argText)) {
          inferredBinding.typeName = normalizeBindingTypeName(base);
          inferredBinding.typeTemplateArg = argText;
        } else {
          inferredBinding.typeName = normalizedType;
          inferredBinding.typeTemplateArg.clear();
        }
        return extractReceiverStructType(inferredBinding);
      }
      return std::nullopt;
    };
    const auto structTypeText = inferStructTypeText();
    if (!structTypeText.has_value()) {
      return std::nullopt;
    }
    std::string currentNamespace;
    if (!currentValidationState_.context.definitionPath.empty()) {
      const size_t slash =
          currentValidationState_.context.definitionPath.find_last_of('/');
      if (slash != std::string::npos && slash > 0) {
        currentNamespace =
            currentValidationState_.context.definitionPath.substr(0, slash);
      }
    }
    const std::string structPath = resolveStructTypePath(
        normalizeBindingTypeName(*structTypeText),
        currentNamespace,
        structNames_);
    auto defIt = defMap_.find(structPath);
    if (structPath.empty() || defIt == defMap_.end() ||
        defIt->second == nullptr) {
      return std::nullopt;
    }
    const size_t targetFieldIndex =
        static_cast<size_t>(candidate.args[1].literalValue);
    size_t currentFieldIndex = 0;
    for (const auto &fieldStmt : defIt->second->statements) {
      bool isStaticField = false;
      for (const auto &transform : fieldStmt.transforms) {
        if (transform.name == "static") {
          isStaticField = true;
          break;
        }
      }
      if (!fieldStmt.isBinding || isStaticField) {
        continue;
      }
      if (currentFieldIndex == targetFieldIndex) {
        return soaFieldViewHelperPath(fieldStmt.name);
      }
      ++currentFieldIndex;
    }
    return std::nullopt;
  };
  if (const auto pendingFieldViewPath = fieldViewPathFromExperimentalHelper()) {
    return *pendingFieldViewPath;
  }
  auto fallbackSoaFieldViewName = [&]() -> std::optional<std::string> {
    if (candidate.kind != Expr::Kind::Call || candidate.isBinding ||
        candidate.hasBodyArguments || !candidate.bodyArguments.empty() ||
        !candidate.templateArgs.empty() || hasNamedArguments(candidate.argNames)) {
      return std::nullopt;
    }
    std::string normalizedName = candidate.name;
    if (!normalizedName.empty() && normalizedName.front() == '/') {
      normalizedName.erase(normalizedName.begin());
    }
    if (normalizedName.empty() || normalizedName.find('/') != std::string::npos ||
        collection_helpers::isCountHelperName(normalizedName) ||
        collection_helpers::isGetHelperName(normalizedName) ||
        collection_helpers::isRefHelperName(normalizedName) ||
        normalizedName == "to_soa" || collection_helpers::isToAosHelperName(normalizedName) ||
        normalizedName == "location" || normalizedName == "dereference") {
      return std::nullopt;
    }
    if (hasVisibleSoaHelperTargetForCurrentImports(normalizedName)) {
      return std::nullopt;
    }
    const Expr *receiver = nullptr;
    if (candidate.isMethodCall) {
      if (candidate.args.size() != 1) {
        return std::nullopt;
      }
      receiver = &candidate.args.front();
    } else {
      if (!candidate.namespacePrefix.empty() || candidate.args.size() != 1) {
        return std::nullopt;
      }
      receiver = &candidate.args.front();
    }
    if (receiver == nullptr || receiver->kind != Expr::Kind::Name) {
      return std::nullopt;
    }
    const BindingInfo *binding = findParamBinding(params, receiver->name);
    if (binding == nullptr) {
      auto localIt = locals.find(receiver->name);
      if (localIt != locals.end()) {
        binding = &localIt->second;
      }
    }
    if (binding == nullptr ||
        normalizeBindingTypeName(binding->typeName) != "soa" ||
        binding->typeTemplateArg.empty()) {
      return std::nullopt;
    }
    return normalizedName;
  }();
  if (fallbackSoaFieldViewName.has_value()) {
    return soaFieldViewHelperPath(*fallbackSoaFieldViewName);
  }
  auto isExperimentalSoaLikeExpr = [&](const Expr &expr) {
    std::string typeText;
    if (!const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(
            expr, params, locals, typeText)) {
      BindingInfo inferredBinding;
      if (!const_cast<SemanticsValidator *>(this)->inferBindingTypeFromInitializer(
              expr, params, locals, inferredBinding)) {
        return false;
      }
      typeText = inferredBinding.typeTemplateArg.empty()
                     ? inferredBinding.typeName
                     : inferredBinding.typeName + "<" +
                           inferredBinding.typeTemplateArg + ">";
    }
    std::string normalizedTypeText = normalizeBindingTypeName(typeText);
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(normalizedTypeText, base, argText)) {
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
          !argText.empty()) {
        // A borrowed receiver (Reference<SoaVector<T>>/Pointer<SoaVector<T>>)
        // is just as SoA-like as an owned one for this check's purposes -
        // unwrap one level so borrowed receivers aren't misclassified as
        // "not experimental SoA-like" (TODO-5050 shape (a)).
        normalizedTypeText = normalizeBindingTypeName(argText);
      } else {
        normalizedTypeText = normalizedBase;
      }
    }
    std::string soaSurfaceTypeHelper;
    const bool isPublicSoaTypeText =
        normalizedTypeText == "soa" ||
        normalizedTypeText.rfind("soa<", 0) == 0 ||
        (splitSoaSurfaceHelperPath(normalizedTypeText,
                                   &soaSurfaceTypeHelper,
                                   nullptr) &&
         (soaSurfaceTypeHelper == "soa" ||
          soaSurfaceTypeHelper.rfind("soa<", 0) == 0));
    return isExperimentalSoaVectorTypePath(normalizedTypeText) ||
           isPublicSoaTypeText;
  };
  const auto soaAccessHelper =
      builtinSoaAccessHelperName(candidate, params, locals);
  if (soaAccessHelper.has_value() &&
      (*soaAccessHelper == "ref" || *soaAccessHelper == collection_helpers::kRefRef) &&
      !candidate.args.empty() &&
      !isExperimentalSoaLikeExpr(candidate.args.front()) &&
      !hasVisibleDefinitionPathForCurrentImports(
          samePathSoaHelperTargetPath(*soaAccessHelper))) {
    const bool usesExplicitPublicSoaHelperPath =
        semantics::usesExplicitPublicSoaHelperPath(candidate.namespacePrefix,
                                                   candidate.name);
    if (usesExplicitPublicSoaHelperPath) {
      return publicSoaHelperTargetPath(*soaAccessHelper);
    }
    return preferredSoaHelperTargetForCollectionType(*soaAccessHelper,
                                                     collection_helpers::kRootedSoa);
  }
  return std::nullopt;
}

bool SemanticsValidator::hasVisibleDefinitionPathForCurrentImports(
    std::string_view path) const {
  const std::string ownedPath(path);
  return hasDeclaredDefinitionPath(ownedPath) ||
         hasImportedDefinitionPath(ownedPath);
}

std::string SemanticsValidator::preferredSoaHelperTargetForCurrentImports(
    std::string_view helperName) const {
  const std::string helper(helperName);
  const std::string samePath = samePathSoaHelperTargetPath(helper);
  if (hasVisibleDefinitionPathForCurrentImports(samePath) ||
      hasDefinitionFamilyPath(samePath)) {
    return samePath;
  }
  const std::string publicPath = publicSoaHelperTargetPath(helper);
  // The public surface also ships reserve/push/to_aos/to_aos_ref wrappers,
  // so the whole supported helper family may canonicalize to it - but only
  // when the wrapper is actually visible; otherwise keep the historical
  // compatibility spelling for diagnostics.
  if (isSupportedCompatibilitySoaHelperName(helper) &&
      hasVisibleDefinitionPathForCurrentImports(publicPath)) {
    return publicPath;
  }
  return compatibilitySoaHelperTargetPath(helper);
}

bool SemanticsValidator::usesSamePathSoaHelperTargetForCurrentImports(
    std::string_view helperName) const {
  const std::string helper(helperName);
  const std::string samePath = samePathSoaHelperTargetPath(helper);
  return preferredSoaHelperTargetForCurrentImports(helperName) == samePath;
}

bool SemanticsValidator::hasVisibleSoaHelperTargetForCurrentImports(
    std::string_view helperName) const {
  const std::string helper(helperName);
  const std::string samePath = samePathSoaHelperTargetPath(helper);
  const std::string canonicalPath = compatibilitySoaHelperTargetPath(helper);
  const std::string publicPath = publicSoaHelperTargetPath(helper);
  return hasVisibleDefinitionPathForCurrentImports(samePath) ||
         hasDefinitionFamilyPath(samePath) ||
         hasVisibleDefinitionPathForCurrentImports(canonicalPath) ||
         (isSupportedCompatibilitySoaHelperName(helper) &&
          hasVisibleDefinitionPathForCurrentImports(publicPath));
}

std::string SemanticsValidator::preferredSoaHelperTargetForCollectionType(
    std::string_view helperName,
    std::string_view collectionTypePath) const {
  const std::string helper(helperName);
  const bool wantsInternalSoaCollection =
      isInternalSoaCollectionTypePath(collectionTypePath);
  const std::string samePath = samePathSoaHelperTargetPath(helper);
  const std::string canonicalPath = compatibilitySoaHelperTargetPath(helper);
  const std::string publicPath = publicSoaHelperTargetPath(helper);
  const std::string preferredTarget =
      preferredSoaHelperTargetForCurrentImports(helperName);
  if (preferredTarget != samePath) {
    return preferredTarget;
  }
  const bool hasVisibleSamePathHelper =
      hasVisibleDefinitionPathForCurrentImports(samePath) ||
      hasDefinitionFamilyPath(samePath);
  if (wantsInternalSoaCollection && hasVisibleSamePathHelper) {
    return samePath;
  }
  auto paramsIt = paramsByDef_.find(samePath);
  if (paramsIt == paramsByDef_.end() && hasDefinitionFamilyPath(samePath)) {
    const std::string templatedPrefix = samePath + "<";
    const std::string specializedPrefix = samePath + "__t";
    const std::string overloadPrefix = samePath + "__ov";
    paramsIt = std::find_if(paramsByDef_.begin(),
                            paramsByDef_.end(),
                            [&](const auto &entry) {
                              return entry.first == samePath ||
                                     entry.first.rfind(templatedPrefix, 0) == 0 ||
                                     entry.first.rfind(specializedPrefix, 0) == 0 ||
                                     entry.first.rfind(overloadPrefix, 0) == 0;
                            });
  }
  if (paramsIt == paramsByDef_.end() || paramsIt->second.empty()) {
    return canonicalPath;
  }
  const BindingInfo &receiverBinding = paramsIt->second.front().binding;
  const std::string receiverTypeText =
      receiverBinding.typeTemplateArg.empty()
          ? receiverBinding.typeName
          : receiverBinding.typeName + "<" + receiverBinding.typeTemplateArg + ">";
  std::string resolvedCollectionType =
      inferMethodCollectionTypePathFromTypeText(receiverTypeText);
  if (resolvedCollectionType.empty() && wantsInternalSoaCollection) {
    std::string elemType;
    if (extractExperimentalSoaVectorElementType(receiverBinding, elemType)) {
      resolvedCollectionType = internalSoaCollectionTypePath(true);
    }
  }
  if (resolvedCollectionType == collectionTypePath ||
      (wantsInternalSoaCollection &&
       isInternalSoaCollectionTypePath(resolvedCollectionType))) {
    if (!wantsInternalSoaCollection) {
      const std::string normalizedRcvr = normalizeBindingTypeName(receiverTypeText);
      if (normalizedRcvr == "soa" || normalizedRcvr.rfind("soa<", 0) == 0) {
        return publicPath;
      }
    }
    return samePath;
  }
  if (wantsInternalSoaCollection && isSoaReadRefHelperName(helper) &&
      hasVisibleDefinitionPathForCurrentImports(publicPath)) {
    return publicPath;
  }
  return canonicalPath;
}

bool SemanticsValidator::usesSamePathSoaHelperTargetForCollectionType(
    std::string_view helperName,
    std::string_view collectionTypePath) const {
  const std::string helper(helperName);
  const std::string samePath = samePathSoaHelperTargetPath(helper);
  return preferredSoaHelperTargetForCollectionType(helperName,
                                                   collectionTypePath) ==
         samePath;
}

bool SemanticsValidator::hasDirectExperimentalVectorImport() const {
  const auto &importPaths = program_.sourceImports.empty() ? program_.imports : program_.sourceImports;
  for (const auto &importPath : importPaths) {
    if (isDirectCollectionVectorImportPath(importPath)) {
      return true;
    }
  }
  return false;
}

}  // namespace primec::semantics
