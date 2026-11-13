#include "TemplateMonomorphAssignmentTargetResolution.h"
#include "primec/frontend/StringLiteral.h"
#include "TemplateMonomorphBindingBlockInference.h"
#include "TemplateMonomorphBindingCallInference.h"
#include "TemplateMonomorphCallableRewrites.h"
#include "TemplateMonomorphTextViewConversions.h"
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
#include "TemplateMonomorphExpressionReferenceRules.h"
#include "TemplateMonomorphExpressionRewriteContext.h"

namespace primec {

// Which collection helper path a call resolves to and whether its template rewrite is deferred.

bool isCanonicalBuiltinKeyValueHelperPath([[maybe_unused]] ExprRewriteContext &rc,
                                          const std::string &path) {

  return isTemplateMonomorphCanonicalKeyValueHelperPath(path);
}

bool isCanonicalStdlibCollectionHelperPath(ExprRewriteContext &rc, const std::string &path) {

  if (isCanonicalBuiltinKeyValueHelperPath(rc, path)) {
    return true;
  }
  auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  auto isCanonicalSoaHelperPath = [](const std::string &candidate, std::string_view helperName) {
    return (candidate.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0 ||
            candidate.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) &&
           semantics::isLegacyOrCanonicalSoaHelperPath(candidate, helperName);
  };
  const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
  const std::string canonicalSoaGetPath = semantics::canonicalizeLegacySoaGetHelperPath(path);
  const std::string canonicalSoaToAosPath = semantics::canonicalizeLegacySoaToAosHelperPath(path);
  std::string vectorHelperName;
  return (semantics::resolveCanonicalVectorHelperNameFromResolvedPath(path, vectorHelperName) &&
          semantics::isVectorCompatibilityHelperName(vectorHelperName)) ||
         isCanonicalSoaHelperPath(canonicalSoaCountPath, "count") ||
         isCanonicalSoaHelperPath(canonicalSoaCountPath, collection_helpers::kCountRef) ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get") ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath,
                                                     collection_helpers::kGetRef) ||
         semantics::isCanonicalSoaRefLikeHelperPath(path) ||
         isCanonicalSoaHelperPath(canonicalSoaCountPath, "reserve") ||
         isCanonicalSoaHelperPath(canonicalSoaCountPath, "push") ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath,
                                                     templateMonomorphSoaToAosHelperName()) ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath,
                                                     templateMonomorphSoaToAosHelperName(true));
}

bool isTemplatedAutoCompatVectorHelperPath([[maybe_unused]] ExprRewriteContext &rc,
                                           std::string_view path) {

  const std::string compatibilityPrefix = "/std/collections/" + std::string("vector");
  return path == compatibilityPrefix + "At" || path == compatibilityPrefix + "AtUnsafe";
}

bool isSyntheticSamePathSoaHelperTemplateCarryPath([[maybe_unused]] ExprRewriteContext &rc,
                                                   const std::string &path) {

  auto isSyntheticSamePathSoaCarryNonRefHelperPath = [](const std::string &candidate) {
    if (semantics::isLegacyOrCanonicalSoaHelperPath(candidate, "count") ||
        semantics::isLegacyOrCanonicalSoaHelperPath(candidate, collection_helpers::kCountRef) ||
        semantics::isLegacyOrCanonicalSoaHelperPath(candidate, "push") ||
        semantics::isLegacyOrCanonicalSoaHelperPath(candidate, "reserve")) {
      return true;
    }
    const std::string getCanonicalPath = semantics::canonicalizeLegacySoaGetHelperPath(candidate);
    return semantics::isLegacyOrCanonicalSoaHelperPath(getCanonicalPath, "get") ||
           semantics::isLegacyOrCanonicalSoaHelperPath(getCanonicalPath,
                                                       collection_helpers::kGetRef);
  };
  const std::string canonicalPath = semantics::canonicalizeLegacySoaRefHelperPath(path);
  return isSyntheticSamePathSoaCarryNonRefHelperPath(path) ||
         semantics::isCanonicalSoaRefLikeHelperPath(canonicalPath) ||
         semantics::isExperimentalSoaGetLikeHelperPath(path) ||
         semantics::isExperimentalSoaRefLikeHelperPath(path);
}

bool isCanonicalSoaBorrowedWrapperHelper([[maybe_unused]] ExprRewriteContext &rc,
                                         const std::string &path) {

  auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
  const std::string canonicalSoaGetPath =
      canonicalizeSoaHelperPath(semantics::canonicalizeLegacySoaGetHelperPath(path));
  const std::string canonicalSoaRefPath =
      canonicalizeSoaHelperPath(semantics::canonicalizeLegacySoaRefHelperPath(path));
  const std::string canonicalSoaToAosPath = semantics::canonicalizeLegacySoaToAosHelperPath(path);
  return semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath,
                                                     collection_helpers::kCountRef) ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath,
                                                     collection_helpers::kGetRef) ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath,
                                                     collection_helpers::kRefRef) ||
         semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath,
                                                     templateMonomorphSoaToAosHelperName(true));
}

std::string preferredBorrowedSoaWrapperPath(ExprRewriteContext &rc, const std::string &path) {

  auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
  const std::string canonicalSoaGetPath =
      canonicalizeSoaHelperPath(semantics::canonicalizeLegacySoaGetHelperPath(path));
  const std::string canonicalSoaRefPath =
      canonicalizeSoaHelperPath(semantics::canonicalizeLegacySoaRefHelperPath(path));
  const std::string canonicalSoaToAosPath = semantics::canonicalizeLegacySoaToAosHelperPath(path);
  if (path == "/count" || path == "count") {
    return semantics::compatibilitySoaHelperTargetPath(collection_helpers::kCountRef);
  }
  if (path == "/get" || path == "get") {
    return semantics::compatibilitySoaHelperTargetPath(collection_helpers::kGetRef);
  }
  if (path == "/ref" || path == "ref") {
    return semantics::compatibilitySoaHelperTargetPath(collection_helpers::kRefRef);
  }
  const std::string samePathSoaToAos = "/" + templateMonomorphSoaToAosHelperName();
  if (path == samePathSoaToAos || path == templateMonomorphSoaToAosHelperName()) {
    return semantics::compatibilitySoaHelperTargetPath(templateMonomorphSoaToAosHelperName(true));
  }
  if (semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, "count")) {
    return preferredBorrowedPathForPublicOrCompatibility(
        rc, canonicalSoaCountPath, collection_helpers::kCountRef);
  }
  if (semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get")) {
    return preferredBorrowedPathForPublicOrCompatibility(
        rc, canonicalSoaGetPath, collection_helpers::kGetRef);
  }
  if (semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref")) {
    return preferredBorrowedPathForPublicOrCompatibility(
        rc, canonicalSoaRefPath, collection_helpers::kRefRef);
  }
  if (semantics::isLegacyOrCanonicalSoaHelperPath(canonicalSoaToAosPath,
                                                  templateMonomorphSoaToAosHelperName())) {
    return semantics::compatibilitySoaHelperTargetPath(templateMonomorphSoaToAosHelperName(true));
  }
  return std::string{};
}

std::string preferCanonicalStdlibCollectionHelperPath(ExprRewriteContext &rc,
                                                      const std::string &path) {
  Expr &expr = rc.expr;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  const Expr *receiverExpr = collectionHelperReceiverExpr(rc, expr);
  if (receiverExpr == nullptr) {
    return path;
  }
  auto hasDefinitionFamilyPath = [&](std::string_view candidatePath) {
    const std::string ownedPath(candidatePath);
    if (ctx.sourceDefs.count(ownedPath) > 0 || ctx.helperOverloads.count(ownedPath) > 0) {
      return true;
    }
    return anySourceDefStartsWith(ctx, ownedPath + "<") ||
           anySourceDefStartsWith(ctx, ownedPath + "__t");
  };
  // Shadow precedence: a real definition at the exact spelled path always
  // wins over any compat/alias rewrite this function would otherwise
  // apply below. Without this, an explicit call to a fully-qualified
  // canonical path (e.g. /std/collections/vector/count) that has no
  // definition of its own could still get silently redirected to an
  // unrelated same-named alias (e.g. /vector/count) defined elsewhere,
  // even though the two paths are not declared equivalent.
  if (hasDefinitionFamilyPath(path)) {
    return path;
  }
  auto hasVisibleRootBuiltinSoaConversionHelper = [&](std::string_view helperPath) {
    auto matchesBuiltinSoaHelper = [&](const std::string &helperPath) {
      auto defIt = ctx.sourceDefs.find(helperPath);
      if (defIt == ctx.sourceDefs.end() || defIt->second.parameters.empty()) {
        return false;
      }
      semantics::BindingInfo paramBinding;
      if (!extractExplicitBindingType(defIt->second.parameters.front(), paramBinding)) {
        return false;
      }
      return isTemplateMonomorphSoaReceiverType(
                 semantics::normalizeBindingTypeName(paramBinding.typeName)) &&
             !paramBinding.typeTemplateArg.empty();
    };
    const std::string ownedHelperPath(helperPath);
    if (matchesBuiltinSoaHelper(ownedHelperPath)) {
      return true;
    }
    auto familyIt = ctx.helperOverloads.find(ownedHelperPath);
    if (familyIt == ctx.helperOverloads.end()) {
      return false;
    }
    for (const auto &entry : familyIt->second) {
      if (matchesBuiltinSoaHelper(entry.internalPath)) {
        return true;
      }
    }
    return false;
  };
  auto resolvesBuiltinSoaToAosShadowReceiver = [&](const Expr *candidate) {
    if (candidate == nullptr) {
      return false;
    }
    if (isTemplateMonomorphSoaReceiverType(inferCollectionReceiverFamily(rc, candidate))) {
      return true;
    }
    if (candidate->kind != Expr::Kind::Call || candidate->isBinding) {
      return false;
    }
    std::string resolvedReceiverPath;
    if (candidate->isMethodCall) {
      if (!resolveMethodCallTemplateTarget(*candidate, locals, ctx, resolvedReceiverPath)) {
        resolvedReceiverPath.clear();
      }
    } else {
      resolvedReceiverPath = resolveCalleePath(*candidate, namespacePrefix, ctx);
    }
    auto defIt = ctx.sourceDefs.find(resolvedReceiverPath);
    if (defIt == ctx.sourceDefs.end()) {
      return false;
    }
    semantics::BindingInfo inferredReturn;
    if (!inferDefinitionReturnBindingForTemplatedFallback(
            defIt->second, hasMathImport(ctx), const_cast<Context &>(ctx), inferredReturn)) {
      return false;
    }
    return isTemplateMonomorphSoaReceiverType(
               semantics::normalizeBindingTypeName(inferredReturn.typeName)) &&
           !inferredReturn.typeTemplateArg.empty();
  };
  auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
    const size_t specializationSuffix = canonicalPath.find("__");
    if (specializationSuffix != std::string::npos) {
      canonicalPath.erase(specializationSuffix);
    }
    return canonicalPath;
  };
  std::string helperName;
  bool resolvesVectorFamilyPath = false;
  if (semantics::isCanonicalVectorCompatibilityPath(path)) {
    helperName = std::string(semantics::stripUnrootedCanonicalVectorCompatibilityPrefix(path));
    resolvesVectorFamilyPath = true;
  } else if (path.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0) {
    helperName = path.substr(templateMonomorphCompatibilitySoaHelperPrefix().size());
  } else if (!expr.isMethodCall && !path.empty() && path.front() == '/' &&
             path.find('/', 1) == std::string::npos && ctx.sourceDefs.count(path) == 0 &&
             ctx.helperOverloads.count(path) == 0) {
    helperName = path.substr(1);
  } else if (!expr.isMethodCall && !expr.name.empty() && expr.name.find('/') == std::string::npos &&
             ctx.sourceDefs.count(path) == 0 && ctx.helperOverloads.count(path) == 0) {
    helperName = expr.name;
  } else if (expr.isMethodCall && (expr.name == "remove_at" || expr.name == "remove_swap") &&
             ctx.sourceDefs.count(path) == 0 && ctx.helperOverloads.count(path) == 0) {
    // TODO-4753: every branch above this one is explicitly gated on
    // `!expr.isMethodCall` - none of them ever ran for method-call-sugar
    // (`values.remove_at(idx)`/`values.remove_swap(idx)`), so this whole
    // function always fell straight through to the unconditional
    // `return path;` below for these two method calls, leaving `path`
    // (e.g. "/vector/remove_at", the raw, not-yet-canonicalized shape
    // `resolveMethodCallTemplateTarget` builds for a collection-family
    // receiver) unresolved to its real definition path
    // ("/std/collections/vector/remove_at"). Narrowly scoped to these two
    // names specifically (not a general "handle any method call" branch):
    // every other recognized helper name here (count/push/pop/reserve/...)
    // already has its own established method-call resolution path
    // elsewhere in the pipeline that this function is deliberately not
    // supposed to interfere with - widening this to all method calls
    // regressed several diagnostic-pinning tests that require those paths
    // to keep rejecting specific visibility-gated shapes.
    helperName = expr.name;
  } else {
    return path;
  }
  helperName = canonicalizeSoaHelperPath(helperName);
  if (!collection_helpers::isCountHelperName(helperName) && helperName != "capacity" &&
      helperName != "push" && helperName != "pop" && helperName != "reserve" &&
      helperName != "clear" && helperName != "remove_at" && helperName != "remove_swap" &&
      !collection_helpers::isGetHelperName(helperName) &&
      !collection_helpers::isRefHelperName(helperName) &&
      helperName != templateMonomorphSoaToAosHelperName() &&
      helperName != templateMonomorphSoaToAosHelperName(true)) {
    return path;
  }
  const std::string receiverFamily = inferCollectionReceiverFamily(rc, receiverExpr);
  const bool receiverResolvesExperimentalSoaVector =
      resolvesExperimentalSoaVectorReceiver(rc, receiverExpr);
  const bool receiverResolvesBorrowedExperimentalSoaVector =
      resolvesBorrowedExperimentalSoaVectorReceiver(rc, receiverExpr);
  if (receiverResolvesBorrowedExperimentalSoaVector) {
    if (helperName == "count") {
      helperName = collection_helpers::kCountRef;
    } else if (helperName == "get") {
      helperName = collection_helpers::kGetRef;
    } else if (helperName == "ref") {
      helperName = collection_helpers::kRefRef;
    } else if (helperName == templateMonomorphSoaToAosHelperName()) {
      helperName = templateMonomorphSoaToAosHelperName(true);
    }
  }
  const bool isBarePublishedVectorMutatorSugar =
      !expr.isMethodCall && !resolvesVectorFamilyPath && expr.namespacePrefix.empty() &&
      expr.name == helperName && expr.name.find('/') == std::string::npos &&
      semantics::isPublishedVectorMutatorHelperName(helperName);
  const bool isMethodPublishedVectorMutatorSugar =
      expr.isMethodCall && semantics::isPublishedVectorMutatorHelperName(helperName);
  // A method on a local of the stdlib vector struct takes the stdlib helper of the same name when it is visible; the
  // bare call form stays rejected.
  const std::string canonicalMutatorHelperPath =
      semantics::canonicalVectorCompatibilityHelperPathOrFallback(helperName);
  const bool methodTakesVisibleStdlibMutator =
      isMethodPublishedVectorMutatorSugar && ctx.sourceDefs.count(canonicalMutatorHelperPath) > 0;
  if ((isBarePublishedVectorMutatorSugar || isMethodPublishedVectorMutatorSugar) &&
      !methodTakesVisibleStdlibMutator &&
      resolvesCollectionVectorValueReceiver(
          receiverExpr, params, locals, allowMathBare, namespacePrefix, ctx)) {
    error = "unknown call target: " +
            semantics::canonicalVectorCompatibilityHelperPathOrFallback(helperName);
    return path;
  }
  if (receiverFamily == "vector" &&
      (collection_helpers::isCountHelperName(helperName) || helperName == "capacity")) {
    const std::string samePathVectorHelper = "/" + std::string("vector") + "/" + helperName;
    if (hasDefinitionFamilyPath(samePathVectorHelper)) {
      return samePathVectorHelper;
    }
  }
  // Map twin of the vector same-path branch above: a rooted
  // `/map/count` / `/map/count_ref` user shadow wins for bare
  // `count(m)` / `count_ref(m)` on a map receiver, exactly like
  // `/vector/count` / `/vector/capacity` do for vector receivers.
  if (receiverFamily == "map" && (collection_helpers::isCountHelperName(helperName))) {
    const std::string samePathMapHelper = "/" + std::string("map") + "/" + helperName;
    if (hasDefinitionFamilyPath(samePathMapHelper)) {
      return samePathMapHelper;
    }
  }
  const auto receiverHasVisibleCanonicalCollectionHelper =
      [&](std::string_view candidateHelperName) {
        const std::string preferred =
            semantics::canonicalVectorCompatibilityHelperPathOrFallback(candidateHelperName);
        return receiverFamily == "vector" &&
               hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
               ctx.sourceDefs.count(preferred) > 0;
      };
  if (collection_helpers::isCountHelperName(helperName) || helperName == "push" ||
      helperName == "reserve") {
    const std::string samePathSoaNonRefHelper =
        templateMonomorphSamePathSoaHelperPrefix() + helperName;
    const bool receiverEligibleForSamePathSoaHelper =
        isTemplateMonomorphSoaReceiverType(receiverFamily) ||
        receiverResolvesBorrowedExperimentalSoaVector || receiverResolvesExperimentalSoaVector ||
        ((collection_helpers::isCountHelperName(helperName)) && receiverFamily == "vector" &&
         !receiverHasVisibleCanonicalCollectionHelper(helperName));
    if (receiverEligibleForSamePathSoaHelper && hasDefinitionFamilyPath(samePathSoaNonRefHelper)) {
      return samePathSoaNonRefHelper;
    }
  }
  // A public soa<T> receiver reports family "soa" rather than the
  // internal soa_vector name; it must still reach a same-path
  // /soa/<helper> shadow for the access helpers.
  const bool receiverIsPublicSoa = receiverFamily == "soa";
  if (collection_helpers::isGetHelperName(helperName)) {
    const std::string samePathGetHelper = templateMonomorphSamePathSoaHelperPrefix() + helperName;
    if (hasDefinitionFamilyPath(samePathGetHelper) &&
        (isTemplateMonomorphSoaReceiverType(receiverFamily) || receiverIsPublicSoa ||
         receiverResolvesBorrowedExperimentalSoaVector || receiverResolvesExperimentalSoaVector ||
         receiverFamily == "vector")) {
      return samePathGetHelper;
    }
  }
  if (collection_helpers::isRefHelperName(helperName)) {
    const std::string samePathRefHelper = templateMonomorphSamePathSoaHelperPrefix() + helperName;
    if (hasDefinitionFamilyPath(samePathRefHelper) &&
        (isTemplateMonomorphSoaReceiverType(receiverFamily) || receiverIsPublicSoa ||
         receiverResolvesBorrowedExperimentalSoaVector || receiverResolvesExperimentalSoaVector ||
         receiverFamily == "vector")) {
      return samePathRefHelper;
    }
  }
  if (helperName == templateMonomorphSoaToAosHelperName() ||
      helperName == templateMonomorphSoaToAosHelperName(true)) {
    const std::string samePathToAosHelper = "/" + helperName;
    if (hasVisibleRootBuiltinSoaConversionHelper(samePathToAosHelper) &&
        resolvesBuiltinSoaToAosShadowReceiver(receiverExpr)) {
      return samePathToAosHelper;
    }
  }
  if (isTemplateMonomorphSoaReceiverType(receiverFamily) ||
      receiverResolvesBorrowedExperimentalSoaVector) {
    const std::string preferred = templateMonomorphCompatibilitySoaHelperPrefix() + helperName;
    if (ctx.sourceDefs.count(preferred) > 0 &&
        (resolvesVectorFamilyPath || hasVisibleStdCollectionsImportForPath(ctx, preferred))) {
      return preferred;
    }
  }
  if (!resolvesVectorFamilyPath && receiverFamily == "map" &&
      (collection_helpers::isCountHelperName(helperName) ||
       collection_helpers::isAtHelperName(helperName) ||
       collection_helpers::isAtUnsafeHelperName(helperName))) {
    const std::string preferred = templateMonomorphCanonicalKeyValueHelperPath(helperName);
    if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
        ctx.sourceDefs.count(preferred) > 0) {
      return preferred;
    }
  }
  if (!resolvesVectorFamilyPath && (receiverFamily == "vector" || receiverFamily == "array")) {
    const std::string preferred =
        semantics::canonicalVectorCompatibilityHelperPathOrFallback(helperName);
    if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
        ctx.sourceDefs.count(preferred) > 0) {
      return preferred;
    }
    if (semantics::isPublishedVectorMutatorHelperName(helperName)) {
      return preferred;
    }
  }
  return path;
}

bool shouldDeferStdlibCollectionHelperTemplateRewrite(ExprRewriteContext &rc,
                                                      const std::string &path) {
  Expr &expr = rc.expr;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.allowMathBare;

  if (!expr.templateArgs.empty()) {
    return false;
  }
  auto forwardedEmptyConstructorPath = [] {
    const primec::StdlibSurfaceMetadata *metadata = keyValueConstructorSurfaceMetadataLocal();
    if (metadata == nullptr) {
      return std::string{};
    }
    const std::string_view memberName =
        primec::resolveStdlibSurfaceMemberName(*metadata, metadata->canonicalPath);
    if (memberName.empty()) {
      return std::string{};
    }
    return experimentalCollectionConstructorPathLocal("map", std::string(memberName) + "New");
  };
  const std::string emptyConstructorPath = forwardedEmptyConstructorPath();
  if (!emptyConstructorPath.empty() && path == emptyConstructorPath &&
      ctx.templateDefs.count(path) > 0) {
    return true;
  }
  if (isTemplatedAutoCompatVectorHelperPath(rc, path)) {
    return true;
  }
  if (!isCanonicalStdlibCollectionHelperPath(rc, path)) {
    return false;
  }
  const Expr *helperReceiverExpr = collectionHelperReceiverExpr(rc, expr);
  const bool borrowedExperimentalSoaReceiver =
      resolvesBorrowedExperimentalSoaVectorReceiver(rc, helperReceiverExpr);
  const bool isCanonicalNonBorrowedSoaHelperPath =
      path == semantics::compatibilitySoaHelperTargetPath("count") ||
      path == semantics::publicSoaHelperTargetPath("count") ||
      path == semantics::compatibilitySoaHelperTargetPath("get") ||
      path == semantics::publicSoaHelperTargetPath("get") ||
      path == semantics::compatibilitySoaHelperTargetPath("ref") ||
      path == semantics::publicSoaHelperTargetPath("ref") ||
      path == semantics::compatibilitySoaHelperTargetPath(templateMonomorphSoaToAosHelperName()) ||
      path == semantics::publicSoaHelperTargetPath(templateMonomorphSoaToAosHelperName());
  if (borrowedExperimentalSoaReceiver && isCanonicalNonBorrowedSoaHelperPath) {
    return true;
  }
  if ((path.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0 ||
       path.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) &&
      helperReceiverExpr != nullptr) {
    if (path == semantics::publicSoaHelperTargetPath(templateMonomorphSoaToAosHelperName()) ||
        path == semantics::publicSoaHelperTargetPath(templateMonomorphSoaToAosHelperName(true))) {
      return true;
    }
    const bool directArgsPackReceiver =
        inferCollectionReceiverFamilyForRewrite(rc, helperReceiverExpr) == "args";
    const bool directBuiltinSoaReceiver =
        isTemplateMonomorphSoaReceiverType(inferCollectionReceiverFamily(rc, helperReceiverExpr));
    const bool directExperimentalSoaReceiver =
        resolvesExperimentalSoaVectorReceiver(rc, helperReceiverExpr);
    if (directArgsPackReceiver || directBuiltinSoaReceiver || directExperimentalSoaReceiver ||
        borrowedExperimentalSoaReceiver) {
      return true;
    }
  }
  if (hasVisibleStdCollectionsImportForPath(ctx, path) && ctx.templateDefs.count(path) > 0) {
    if (semantics::isCanonicalVectorCompatibilityPath(path) &&
        !resolvesBuiltinVectorReceiver(rc, collectionHelperReceiverExpr(rc, expr)) &&
        !resolvesCollectionVectorValueReceiver(collectionHelperReceiverExpr(rc, expr),
                                               params,
                                               locals,
                                               allowMathBare,
                                               namespacePrefix,
                                               ctx)) {
      return true;
    }
    if (isTemplateMonomorphCanonicalKeyValueHelperPath(path)) {
      return true;
    }
    return false;
  }
  if (isCanonicalBuiltinKeyValueHelperPath(rc, path)) {
    return resolvesBuiltinKeyValueReceiver(rc, collectionHelperReceiverExpr(rc, expr)) &&
           ctx.templateDefs.count(path) == 0;
  }
  return true;
}

std::string preferredBorrowedPathForPublicOrCompatibility([[maybe_unused]] ExprRewriteContext &rc,
                                                          const std::string &canonicalPath,
                                                          std::string_view borrowedHelper) {

  if (canonicalPath.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) {
    return semantics::publicSoaHelperTargetPath(borrowedHelper);
  }
  return semantics::compatibilitySoaHelperTargetPath(borrowedHelper);
}

} // namespace primec
