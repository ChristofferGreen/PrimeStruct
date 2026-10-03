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

bool SemanticsValidator::graphBindingIsUsable(const BindingInfo &binding) const {
  const std::string normalizedType = normalizeBindingTypeName(binding.typeName);
  if (normalizedType.empty() || normalizedType == "auto") {
    return false;
  }
  if (normalizedType == "array" && binding.typeTemplateArg.empty()) {
    return false;
  }
  std::string scopeNamespace;
  const auto scopeIt = defMap_.find(currentValidationState_.context.definitionPath);
  if (scopeIt != defMap_.end() && scopeIt->second != nullptr) {
    scopeNamespace = scopeIt->second->namespacePrefix;
  }
  if (!binding.typeTemplateArg.empty()) {
    return true;
  }
  std::string normalizedTypeBase = normalizedType;
  std::string normalizedTypeArgText;
  if (splitTemplateTypeName(normalizedType, normalizedTypeBase, normalizedTypeArgText) &&
      !normalizedTypeArgText.empty()) {
    if (normalizedTypeBase == "Result" || normalizedTypeBase == "Pointer" ||
        normalizedTypeBase == "Reference") {
      return true;
    }
    if (returnKindForTypeName(normalizedTypeBase) != ReturnKind::Unknown) {
      return true;
    }
    if (structNames_.count(normalizedTypeBase) > 0) {
      return true;
    }
    if (!resolveStructTypePath(normalizedTypeBase, scopeNamespace, structNames_).empty()) {
      return true;
    }
    if (resolveSumDefinitionForTypeText(normalizedTypeBase, scopeNamespace) != nullptr) {
      return true;
    }
    const auto importIt = importAliases_.find(normalizedTypeBase);
    return importIt != importAliases_.end() && structNames_.count(importIt->second) > 0;
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
  if (!resolveStructTypePath(normalizedType, scopeNamespace, structNames_).empty()) {
    return true;
  }
  if (resolveSumDefinitionForTypeText(normalizedType, scopeNamespace) != nullptr) {
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
  if (normalizedName == "vector" ||
      isCanonicalVectorConstructorCall(normalizedPrefix, normalizedName)) {
    return true;
  }
  return false;
}

std::string SemanticsValidator::preferredCollectionHelperResolvedPath(
    const Expr &initializerCall) const {
  // Steps 2b/2c (docs/CompatPathResolutionConsolidation.md): the shared
  // spelling classifier is the single answer; the legacy composed
  // resolver is retired. The one intended behavior change from the
  // legacy era is decision D5: explicit /std/collections/soa_vector/*
  // spellings no longer canonicalize to the dead family (the classifier
  // passes them through, and callers' resolveCalleePath fallback
  // preserves the spelled path for emergent unknown-target handling).
  if (initializerCall.kind != Expr::Kind::Call ||
      initializerCall.isMethodCall || initializerCall.name.empty()) {
    return {};
  }
  std::string rooted = initializerCall.name;
  if (rooted.front() != '/') {
    std::string prefix = initializerCall.namespacePrefix;
    if (!prefix.empty() && prefix.front() != '/') {
      prefix.insert(prefix.begin(), '/');
    }
    rooted = prefix.empty() ? "/" + rooted : prefix + "/" + rooted;
  }
  // A call already spelled as a user same-path `/soa/<helper>` shadow
  // (TODO-5295 rewrites builtin soa<T> access calls onto it) is typed by
  // that shadow's own declared return, not the canonical helper's
  // The classifier canonicalizes bare /soa/ spellings
  // shadow-blind, so short-circuit before consulting it.
  if (collection_helpers::isRootedSoaPath(rooted) && defMap_.count(rooted) > 0) {
    return {};
  }
  const CompatSpellingDecision decision = classifyCollectionHelperSpelling(
      rooted,
      CollectionCallShape::DirectCall,
      CollectionReceiverFamily::None,
      [this](std::string_view path) {
        // Family-aware: stdlib collection helpers are templates, so the
        // bare canonical key is often absent from defMap_ while the
        // definition family (path<...>, path__t..., path__ov...) exists.
        return defMap_.count(std::string(path)) > 0 ||
               hasDefinitionFamilyPath(path);
      });
  return decision.disposition == CompatSpellingDisposition::Canonicalize
             ? decision.canonicalPath
             : std::string{};
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
      if (normalizeBindingTypeName(binding.typeName) == "soa") {
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
           builtinCollection == "soa";
  };

  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  const bool isSoaConversionSurfaceSpelling =
      semantics::isSoaConversionSurfaceSpelling(normalizedPrefix, normalizedName);
  if (isSoaConversionSurfaceSpelling) {
    return std::nullopt;
  }

  std::string resolved = preferredCollectionHelperResolvedPath(candidate);
  if (resolved.empty()) {
    resolved = resolveCalleePath(candidate);
  }
  if (!resolved.empty()) {
    const std::string concreteResolved =
        resolveExprConcreteCallPath(params, locals, candidate, resolved);
    if (!concreteResolved.empty()) {
      resolved = concreteResolved;
    }
  }
  const std::string resolvedCanonical =
      canonicalizeLegacySoaGetHelperPath(resolved);
  auto explicitSoaHelperCall = [&](std::string_view expectedHelperName) {
    std::string helperName;
    if (splitSoaSurfaceHelperPath(normalizedName, &helperName, nullptr)) {
      return helperName == expectedHelperName;
    }
    return !candidate.isMethodCall &&
           isSoaCountOrAccessSurfaceSpelling(normalizedPrefix, normalizedName) &&
           normalizedName == expectedHelperName;
  };

  const bool isExplicitSoaRefCall =
      explicitSoaHelperCall("ref");
  const bool isBuiltinSoaRefMethod =
      candidate.isMethodCall && normalizedName == "ref" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  const bool resolvedCanonicalIsRefLike =
      isCanonicalSoaRefLikeHelperPath(resolvedCanonical);
  const bool resolvedCanonicalIsRef =
      resolvedCanonicalIsRefLike &&
      isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, "ref");
  const bool resolvedCanonicalIsRefRef =
      resolvedCanonicalIsRefLike &&
      isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, collection_helpers::kRefRef);
  if (resolvedCanonicalIsRef ||
      isExplicitSoaRefCall ||
      isBuiltinSoaRefMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, "ref"))) {
    return std::string("ref");
  }
  const bool isExplicitSoaRefRefCall =
      explicitSoaHelperCall(collection_helpers::kRefRef);
  const bool isBuiltinSoaRefRefMethod =
      candidate.isMethodCall && normalizedName == collection_helpers::kRefRef &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  if (resolvedCanonicalIsRefRef ||
      isExplicitSoaRefRefCall ||
      isBuiltinSoaRefRefMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, collection_helpers::kRefRef))) {
    return std::string(collection_helpers::kRefRef);
  }

  const bool isExplicitSoaGetCall =
      explicitSoaHelperCall("get");
  const bool isBuiltinSoaGetMethod =
      candidate.isMethodCall && normalizedName == "get" &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  const bool resolvedCanonicalIsGet =
      isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, "get");
  if (resolvedCanonicalIsGet ||
      isExplicitSoaGetCall ||
      isBuiltinSoaGetMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, "get"))) {
    return std::string("get");
  }
  const bool isExplicitSoaGetRefCall =
      explicitSoaHelperCall(collection_helpers::kGetRef);
  const bool isBuiltinSoaGetRefMethod =
      candidate.isMethodCall && normalizedName == collection_helpers::kGetRef &&
      !candidate.args.empty() && isDirectSoaVectorTarget(candidate.args.front());
  const bool resolvedCanonicalIsGetRef =
      isLegacyOrCanonicalSoaHelperPath(resolvedCanonical, collection_helpers::kGetRef);
  if (resolvedCanonicalIsGetRef ||
      isExplicitSoaGetRefCall ||
      isBuiltinSoaGetRefMethod ||
      (!candidate.isMethodCall && isSimpleCallName(candidate, collection_helpers::kGetRef))) {
    return std::string(collection_helpers::kGetRef);
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
  auto withPreservedError = [&](const std::function<bool()> &fn) {
    const std::string previousError = const_cast<SemanticsValidator *>(this)->error_;
    const_cast<SemanticsValidator *>(this)->error_.clear();
    const bool ok = fn();
    const_cast<SemanticsValidator *>(this)->error_.clear();
    const_cast<SemanticsValidator *>(this)->error_ = previousError;
    return ok;
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
    if (withPreservedError([&]() {
          return const_cast<SemanticsValidator *>(this)->inferBindingTypeFromInitializer(
              receiver, params, locals, bindingOut);
        })) {
      return true;
    }
    std::string inferredTypeText;
    return withPreservedError([&]() {
             return const_cast<SemanticsValidator *>(this)->inferQueryExprTypeText(
                 receiver, params, locals, inferredTypeText);
           }) &&
           assignBindingTypeFromText(inferredTypeText, bindingOut);
  };
  auto resolveDirectSoaReceiver = [&](const Expr &receiver,
                                      std::string &elemTypeOut) -> bool {
    BindingInfo receiverBinding;
    if (!inferSoaReceiverBinding(receiver, receiverBinding)) {
      return false;
    }
    if (normalizeBindingTypeName(receiverBinding.typeName) == "soa" &&
        !receiverBinding.typeTemplateArg.empty()) {
      elemTypeOut = receiverBinding.typeTemplateArg;
      return true;
    }
    return extractExperimentalSoaVectorElementType(receiverBinding,
                                                   elemTypeOut) &&
           !elemTypeOut.empty();
  };

  std::string normalizedName = candidate.name;
  if (!normalizedName.empty() && normalizedName.front() == '/') {
    normalizedName.erase(normalizedName.begin());
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  const bool isSoaConversionSurfaceSpelling =
      semantics::isSoaConversionSurfaceSpelling(normalizedPrefix, normalizedName);
  const bool isSoaCountOrAccessSurfaceSpelling =
      semantics::isSoaCountOrAccessSurfaceSpelling(normalizedPrefix, normalizedName);
  if (normalizedName.empty() || isSoaCountOrAccessSurfaceSpelling ||
      isSoaConversionSurfaceSpelling) {
      return false;
  }
  if (candidate.isMethodCall) {
    if (candidate.args.size() != 1) {
      return false;
    }
  } else {
    if ((!normalizedPrefix.empty() &&
         !isCompatibilitySoaSurfaceNamespace(normalizedPrefix)) ||
        candidate.args.size() != 1) {
      return false;
    }
  }

  if (normalizedName.find('/') == std::string::npos &&
      hasVisibleSoaHelperTargetForCurrentImports(normalizedName)) {
    return false;
  }

  std::string resolved = preferredCollectionHelperResolvedPath(candidate);
  if (resolved.empty()) {
    resolved = resolveCalleePath(candidate);
  }
  if (!resolved.empty()) {
    const std::string concreteResolved =
        resolveExprConcreteCallPath(params, locals, candidate, resolved);
    if (!concreteResolved.empty()) {
      resolved = concreteResolved;
    }
  }
  if (splitSoaFieldViewHelperPath(resolved, fieldNameOut)) {
    return true;
  }

  const Expr *receiver = nullptr;
  if (candidate.isMethodCall) {
    receiver = &candidate.args.front();
  } else {
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

}  // namespace primec::semantics
