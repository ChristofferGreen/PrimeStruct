#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphDefinitionBindingSetup.h"
#include "TemplateMonomorphDefinitionExperimentalCollectionRewrites.h"
#include "TemplateMonomorphDefinitionReturnOrchestration.h"
#include "TemplateMonomorphDefinitionRewrites.h"
#include "TemplateMonomorphExecutionRewrites.h"
#include "TemplateMonomorphExperimentalCollectionArgumentRewrites.h"
#include "TemplateMonomorphExperimentalCollectionConstructorRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReceiverResolution.h"
#include "TemplateMonomorphExperimentalCollectionReturnRewrites.h"
#include "TemplateMonomorphExperimentalCollectionReturnSetup.h"
#include "TemplateMonomorphExperimentalCollectionTargetValueRewrites.h"
#include "TemplateMonomorphExperimentalCollectionValueRewrites.h"
#include "TemplateMonomorphExpressionRewrite.h"
#include "TemplateMonomorphFallbackTypeInference.h"
#include "TemplateMonomorphFinalOrchestration.h"
#include "TemplateMonomorphImplicitTemplateInference.h"
#include "TemplateMonomorphImplicitTemplateInferenceLambdas.h"
#include "TemplateMonomorphMethodTargets.h"
#include "TemplateMonomorphTemplateSpecialization.h"
#include "TemplateMonomorphTypeResolution.h"
#include "SemanticsHelpers.h"
#include "SemanticsValidatorInferCollectionCompatibilityInternal.h"
#include "StdlibCollectionSurfaceHelpers.h"
#include "TemplateMonomorphCoreUtilities.h"
#include "TemplateMonomorphSetupUtilities.h"
#include "TemplateMonomorphCollectionCompatibilityPaths.h"
#include "TemplateMonomorphExperimentalCollectionTypeHelpers.h"
#include "TemplateMonomorphSourceDefinitionSetup.h"
#include "TemplateMonomorphExperimentalCollectionConstructorPaths.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"

#include <sstream>

#include "primec/support/CompileArena.h"
#include "primec/support/CollectionHelperNames.h"

namespace primec {

using semantics::isPickCall;
using semantics::canonicalizeLegacySoaToAosHelperPath;
using semantics::isVectorCompatibilityHelperName;
using semantics::isExperimentalSoaGetLikeHelperPath;
using semantics::isExperimentalSoaRefLikeHelperPath;
using semantics::isPublishedVectorMutatorHelperName;

using semantics::isBindingAuxTransformName;
using semantics::isRootBuiltinName;
using semantics::isCanonicalVectorCompatibilityPath;
using semantics::getBuiltinArrayAccessName;
using semantics::isExperimentalSoaVectorTypePath;
using semantics::soaUnavailableMethodDiagnostic;
using semantics::trimLeadingSlash;

using semantics::canonicalizeLegacySoaRefHelperPath;
using semantics::isCompileTimeTypeBinding;
using semantics::isExperimentalSoaVectorHelperFamilyPath;
using semantics::isKeyValueCollectionTypeName;
using semantics::isLegacyExperimentalVectorCompatibilityPath;
using semantics::isLegacyExperimentalVectorCompatibilitySpecializedTypePath;
using semantics::legacyExperimentalVectorCompatibilityPrefix;
using semantics::preferredPublishedCollectionLoweringPath;
using semantics::resolveCanonicalVectorHelperNameFromResolvedPath;
using semantics::resolveVectorCompatibilityHelperNameFromResolvedPath;
using semantics::vectorHelperSurfaceMetadata;

using semantics::hasNamedArguments;
using semantics::getBuiltinPointerName;
using semantics::vectorConstructorSurfaceMetadata;
using semantics::canonicalVectorCompatibilityPrefixOrFallback;

using semantics::joinTemplateArgs;
using semantics::canonicalVectorTypeIdentityPrefix;
using semantics::legacyExperimentalVectorCompatibilityTypeText;
using semantics::returnKindForTypeName;
using semantics::resolveTypePath;
using semantics::mapCollectionAliasToken;
using semantics::stripUnrootedCanonicalVectorCompatibilityPrefix;
using semantics::publicSoaHelperTargetPath;
using semantics::isUnrootedCanonicalVectorCompatibilityPath;
using semantics::isSoftwareNumericTypeName;
using semantics::isLegacyOrCanonicalSoaHelperPath;
using semantics::isIfCall;
using semantics::isCanonicalSoaRefLikeHelperPath;
using semantics::compatibilitySoaHelperTargetPath;
using semantics::canonicalizeLegacySoaGetHelperPath;
using semantics::canonicalVectorCompatibilityHelperPathOrFallback;

using semantics::BindingInfo;
using semantics::ParameterInfo;
using semantics::ReturnKind;
using semantics::buildOrderedArguments;
using semantics::extractKeyValueCollectionTypesFromTypeText;
using semantics::getBuiltinCollectionName;
using semantics::isExperimentalSoaVectorSpecializedTypePath;
using semantics::isPrimitiveBindingTypeName;
using semantics::isReturnCall;
using semantics::isSimpleCallName;
using semantics::normalizeBindingTypeName;
using semantics::splitTemplateTypeName;
using semantics::splitTopLevelTemplateArgs;


bool extractBuiltinSoaElementTypeText(std::string typeText, std::string &elemTypeOut) {
  typeText = normalizeBindingTypeName(typeText);
  if (typeText.empty()) {
    return false;
  }
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
      return false;
    }
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (isTemplateMonomorphSoaReceiverType(
            normalizeCollectionReceiverTypeName(normalizedBase))) {
      elemTypeOut = normalizeBindingTypeName(argText);
      return !elemTypeOut.empty();
    }
    if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
        !argText.empty()) {
      std::vector<std::string> wrappedArgs;
      if (!splitTopLevelTemplateArgs(argText, wrappedArgs) ||
          wrappedArgs.size() != 1) {
        return false;
      }
      typeText = normalizeBindingTypeName(wrappedArgs.front());
      continue;
    }
    return false;
  }
}

bool extractBuiltinVectorElementTypeText(std::string typeText, std::string &elemTypeOut) {
  typeText = normalizeBindingTypeName(typeText);
  if (typeText.empty()) {
    return false;
  }
  while (true) {
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
      return false;
    }
    const std::string normalizedBase = normalizeBindingTypeName(base);
    if (normalizeCollectionReceiverTypeName(normalizedBase) == "vector") {
      elemTypeOut = normalizeBindingTypeName(argText);
      return !elemTypeOut.empty();
    }
    if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
        !argText.empty()) {
      std::vector<std::string> wrappedArgs;
      if (!splitTopLevelTemplateArgs(argText, wrappedArgs) ||
          wrappedArgs.size() != 1) {
        return false;
      }
      typeText = normalizeBindingTypeName(wrappedArgs.front());
      continue;
    }
    return false;
  }
}

bool assignBindingFromTypeText(const std::string &typeText, BindingInfo &bindingOut) {
  const std::string normalizedType = normalizeBindingTypeName(typeText);
  if (normalizedType.empty()) {
    return false;
  }
  std::string base;
  std::string argText;
  if (splitTemplateTypeName(normalizedType, base, argText) && !base.empty()) {
    bindingOut.typeName = base;
    bindingOut.typeTemplateArg = argText;
    return true;
  }
  bindingOut.typeName = normalizedType;
  bindingOut.typeTemplateArg.clear();
  return true;
}

std::string unsupportedBuiltinSoaPendingDiagnostic(const Expr &candidate, const LocalTypeMap &locals, const std::vector<ParameterInfo> &params, const std::string &namespacePrefix, Context &ctx, bool allowMathBare) {
  if (candidate.kind != Expr::Kind::Call || candidate.isBinding || candidate.name.empty()) {
    return {};
  }
  const auto normalizeCallName = [](std::string name) {
    if (!name.empty() && name.front() == '/') {
      name.erase(name.begin());
    }
    if (const size_t suffix = name.find("__t"); suffix != std::string::npos) {
      name.erase(suffix);
    }
    return name;
  };
  std::string resolvedPath;
  if (candidate.isMethodCall) {
    if (!resolveMethodCallTemplateTarget(candidate, locals, ctx, resolvedPath)) {
      resolvedPath.clear();
    }
  }
  if (candidate.args.empty()) {
    return {};
  }
  std::string normalizedPrefix = candidate.namespacePrefix;
  if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
    normalizedPrefix.erase(normalizedPrefix.begin());
  }
  const std::string normalizedName = normalizeCallName(candidate.name);
  if (normalizedName.empty()) {
    return {};
  }
  auto resolvesBuiltinSoaReceiver = [&](const Expr &receiverExpr) {
    auto matchesTypeText = [&](std::string typeText) {
      typeText = normalizeBindingTypeName(typeText);
      if (typeText.empty()) {
        return false;
      }
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
          return isTemplateMonomorphSoaReceiverType(
              normalizeCollectionReceiverTypeName(typeText));
        }
        const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
        if ((normalizedBase == "Reference" || normalizedBase == "Pointer") &&
            !argText.empty()) {
          std::vector<std::string> wrappedArgs;
          if (!splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
            return false;
          }
          typeText = normalizeBindingTypeName(wrappedArgs.front());
          continue;
        }
        return isTemplateMonomorphSoaReceiverType(normalizedBase);
      }
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        matchesTypeText(bindingTypeToString(receiverInfo))) {
      return true;
    }
    return matchesTypeText(
        inferExprTypeTextForTemplatedVectorFallback(
            receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  auto resolvesExperimentalSoaReceiver = [&](const Expr &receiverExpr) {
    auto matchesExperimentalTypeText = [](std::string typeText) {
      typeText = normalizeBindingTypeName(typeText);
      while (!typeText.empty()) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(typeText, base, argText) || base.empty()) {
          return isExperimentalSoaVectorTypePath(typeText);
        }
        const std::string normalizedBase = normalizeBindingTypeName(base);
        if (isExperimentalSoaVectorTypePath(normalizedBase)) {
          return true;
        }
        if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
          return false;
        }
        std::vector<std::string> wrappedArgs;
        if (!splitTopLevelTemplateArgs(argText, wrappedArgs) ||
            wrappedArgs.size() != 1) {
          return false;
        }
        typeText = normalizeBindingTypeName(wrappedArgs.front());
      }
      return false;
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(receiverExpr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     ctx,
                                     receiverInfo) &&
        matchesExperimentalTypeText(bindingTypeToString(receiverInfo))) {
      return true;
    }
    return matchesExperimentalTypeText(
        inferExprTypeTextForTemplatedVectorFallback(
            receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  if (!resolvesBuiltinSoaReceiver(candidate.args.front())) {
    return {};
  }
  const bool receiverIsExperimentalSoa =
      resolvesExperimentalSoaReceiver(candidate.args.front());
  auto hasVisibleSoaBorrowedHelper = [&](std::string_view helperName) {
    const std::string samePath =
        templateMonomorphSamePathSoaHelperPrefix() + std::string(helperName);
    const std::string canonicalPath =
        compatibilitySoaHelperTargetPath(helperName);
    return ctx.sourceDefs.count(samePath) > 0 ||
           ctx.helperOverloads.count(samePath) > 0 ||
           ctx.sourceDefs.count(canonicalPath) > 0 ||
           ctx.helperOverloads.count(canonicalPath) > 0;
  };
  const bool hasVisibleSoaRefHelper =
      hasVisibleSoaBorrowedHelper("ref");
  const bool hasVisibleSoaRefRefHelper =
      hasVisibleSoaBorrowedHelper(collection_helpers::kRefRef);
  const std::string resolvedSoaCanonical =
      canonicalizeLegacySoaRefHelperPath(resolvedPath);
  const std::string normalizedNameSoaCanonical =
      canonicalizeLegacySoaRefHelperPath("/" + normalizedName);
  const std::string normalizedNameSoaPath = "/" + normalizedName;
  const bool normalizedNameUsesCanonicalSoaNamespace =
      normalizedName.rfind(
          templateMonomorphCompatibilitySoaHelperPrefix(false), 0) == 0;
  const bool normalizedNameUsesLegacySoaNamespace =
      normalizedName.rfind(
          templateMonomorphSamePathSoaHelperPrefix(false), 0) == 0;
  const std::string normalizedPrefixedSoaPath =
      normalizedPrefix.empty()
          ? std::string{}
          : "/" + normalizedPrefix + "/" + normalizedName;
  const bool normalizedPrefixedUsesLegacySoaNamespace =
      normalizedPrefixedSoaPath.rfind(
          templateMonomorphSamePathSoaHelperPrefix(), 0) == 0;
  const bool normalizedPrefixedNameMatchesSoaRef =
      isLegacyOrCanonicalSoaHelperPath(normalizedPrefixedSoaPath, "ref");
  const bool normalizedPrefixedNameMatchesSoaRefRef =
      isLegacyOrCanonicalSoaHelperPath(
          normalizedPrefixedSoaPath, collection_helpers::kRefRef);
  const bool normalizedCanonicalNameMatchesSoaRef =
      isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaCanonical, "ref");
  const bool normalizedCanonicalNameMatchesSoaRefRef =
      isLegacyOrCanonicalSoaHelperPath(
          normalizedNameSoaCanonical, collection_helpers::kRefRef);
  const bool resolvedCanonicalNameMatchesSoaRef =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, "ref");
  const bool resolvedCanonicalNameMatchesSoaRefRef =
      isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, collection_helpers::kRefRef);
  const bool normalizedNameMatchesSoaRef =
      isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, "ref");
  const bool normalizedNameMatchesSoaRefRef =
      isLegacyOrCanonicalSoaHelperPath(normalizedNameSoaPath, collection_helpers::kRefRef);
  const bool canonicalNamespaceNameMatchesSoaRef =
      normalizedNameUsesCanonicalSoaNamespace &&
      normalizedCanonicalNameMatchesSoaRef;
  const bool canonicalNamespaceNameMatchesSoaRefRef =
      normalizedNameUsesCanonicalSoaNamespace &&
      normalizedCanonicalNameMatchesSoaRefRef;
  const bool isCanonicalBuiltinSoaRefRefCall =
      canonicalNamespaceNameMatchesSoaRefRef ||
      resolvedCanonicalNameMatchesSoaRefRef;
  const bool isOldSurfaceBuiltinSoaRefRefCall =
      normalizedNameUsesLegacySoaNamespace &&
      normalizedNameMatchesSoaRefRef;
  const bool isAnyCanonicalBuiltinSoaRefCall =
      canonicalNamespaceNameMatchesSoaRef ||
      canonicalNamespaceNameMatchesSoaRefRef ||
      resolvedCanonicalNameMatchesSoaRef ||
      resolvedCanonicalNameMatchesSoaRefRef;
  const bool isAnyOldSurfaceBuiltinSoaRefCall =
      normalizedNameUsesLegacySoaNamespace &&
      (normalizedNameMatchesSoaRef || normalizedNameMatchesSoaRefRef);
  const std::string normalizedMethodSoaPath =
      templateMonomorphSamePathSoaHelperPrefix() + normalizedName;
  const bool normalizedMethodNameMatchesSoaRef =
      isLegacyOrCanonicalSoaHelperPath(normalizedMethodSoaPath, "ref");
  const bool normalizedMethodNameMatchesSoaRefRef =
      isLegacyOrCanonicalSoaHelperPath(normalizedMethodSoaPath, collection_helpers::kRefRef);
  const bool isAnyNormalizedMethodNameSoaRefCall =
      normalizedMethodNameMatchesSoaRef || normalizedMethodNameMatchesSoaRefRef;
  const bool isAnyBuiltinSoaRefCall =
      isAnyNormalizedMethodNameSoaRefCall ||
      isAnyCanonicalBuiltinSoaRefCall ||
      isAnyOldSurfaceBuiltinSoaRefCall;
  if (isAnyBuiltinSoaRefCall) {
    const bool isAnyBuiltinSoaRefRefCall =
        normalizedMethodNameMatchesSoaRefRef ||
        isCanonicalBuiltinSoaRefRefCall ||
        isOldSurfaceBuiltinSoaRefRefCall;
    const std::string missingSoaRefHelperPath =
        compatibilitySoaHelperTargetPath(
            isAnyBuiltinSoaRefRefCall ? collection_helpers::kRefRef : "ref");
    if (isAnyBuiltinSoaRefRefCall ? hasVisibleSoaRefRefHelper
                                  : hasVisibleSoaRefHelper) {
      return {};
    }
    if (receiverIsExperimentalSoa) {
      return {};
    }
    if (isAnyCanonicalBuiltinSoaRefCall &&
        !candidate.args.empty() &&
        candidate.args.front().kind == Expr::Kind::Call) {
      return soaUnavailableMethodDiagnostic(missingSoaRefHelperPath);
    }
    const bool isAnyExplicitOrBuiltinSoaRefCall =
        ((!candidate.isMethodCall &&
          normalizedPrefixedUsesLegacySoaNamespace) &&
         (normalizedPrefixedNameMatchesSoaRef ||
          normalizedPrefixedNameMatchesSoaRefRef)) ||
        isAnyOldSurfaceBuiltinSoaRefCall ||
        (candidate.isMethodCall &&
         (isAnyNormalizedMethodNameSoaRefCall ||
          normalizedCanonicalNameMatchesSoaRef ||
          normalizedCanonicalNameMatchesSoaRefRef)) ||
        (!candidate.isMethodCall && isAnyNormalizedMethodNameSoaRefCall);
    if (isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical) ||
        isAnyExplicitOrBuiltinSoaRefCall) {
      return soaUnavailableMethodDiagnostic(missingSoaRefHelperPath);
    }
    return {};
  }
  const bool isKnownBuiltinSoaHelperName =
      normalizedName == "count" || normalizedName == "get" ||
      normalizedName == templateMonomorphSoaToSoaHelperName() ||
      normalizedName == templateMonomorphSoaToAosHelperName() ||
      normalizedName == templateMonomorphSoaToAosHelperName(true) ||
      normalizedName == "contains";
  if (isKnownBuiltinSoaHelperName) {
    return {};
  }
  const std::string ownedPath =
      templateMonomorphSamePathSoaHelperPrefix() + normalizedName;
  const std::string canonicalPath =
      templateMonomorphCompatibilitySoaHelperPrefix() + normalizedName;
  if (ctx.sourceDefs.count(ownedPath) > 0 ||
      ctx.helperOverloads.count(ownedPath) > 0 ||
      ctx.sourceDefs.count(canonicalPath) > 0 ||
      ctx.helperOverloads.count(canonicalPath) > 0) {
    return {};
  }
  return "field-view escapes via argument";
}

} // namespace primec
