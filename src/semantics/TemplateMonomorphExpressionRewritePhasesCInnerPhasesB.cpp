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
#include "TemplateMonomorphExpressionRewriteState.h"
#include "TemplateMonomorphExpressionRewriteInnerState.h"

namespace primec {

PhaseStatus rewriteExprReferencePhase5([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<semantics::ParameterInfo> &params, [[maybe_unused]] RewriteExprState &st, RewriteExprInnerState &st2) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &mutableCollectionHelperReceiverExpr = st.mutableCollectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  [[maybe_unused]] auto &preferredBorrowedSoaWrapperPath = st.preferredBorrowedSoaWrapperPath;
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
  [[maybe_unused]] auto &resolvedPath = st2.resolvedPath;
    const Expr *experimentalVectorReceiverExpr = collectionHelperReceiverExpr(expr);
    const bool canRewriteNamedExperimentalVectorTemporary =
        experimentalVectorReceiverExpr == nullptr ||
        experimentalVectorReceiverExpr->kind != Expr::Kind::Call ||
        !semantics::hasNamedArguments(experimentalVectorReceiverExpr->argNames);
    const std::string experimentalVectorPath = experimentalVectorHelperPathForCanonicalHelper(resolvedPath);
    if (!experimentalVectorPath.empty() && ctx.sourceDefs.count(experimentalVectorPath) > 0 &&
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        canRewriteNamedExperimentalVectorTemporary &&
        resolvesCollectionVectorValueReceiver(
            experimentalVectorReceiverExpr, params, locals, allowMathBare, namespacePrefix, ctx)) {
      resolvedPath = experimentalVectorPath;
      expr.name = experimentalVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveCollectionVectorValueReceiverTemplateArgs(
                collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
          return st2.done(st.done(false));
        }
      }
    }
    const std::string experimentalSoaVectorPath = experimentalSoaVectorHelperPathForCanonicalHelper(resolvedPath);
    const bool shouldRewriteCanonicalSoaHelperToExperimental =
        ctx.sourceDefs.count(resolvedPath) == 0 &&
        ctx.helperOverloads.count(resolvedPath) == 0 &&
        !isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&
        hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
        resolvesConcreteExperimentalSoaVectorReceiver(
            collectionHelperReceiverExpr(expr));
    if (shouldRewriteCanonicalSoaHelperToExperimental &&
        !experimentalSoaVectorPath.empty() &&
        ctx.sourceDefs.count(experimentalSoaVectorPath) > 0) {
      resolvedPath = experimentalSoaVectorPath;
      expr.name = experimentalSoaVectorPath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalSoaVectorReceiverTemplateArgs(collectionHelperReceiverExpr(expr), receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
    }
  st2.inferredCanonicalKeyValueReceiverTemplateArgs = false;
  [[maybe_unused]] auto &inferredCanonicalKeyValueReceiverTemplateArgs = st2.inferredCanonicalKeyValueReceiverTemplateArgs;
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
        resolvesExperimentalKeyValueReceiver(
            collectionHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalKeyValueReceiverTemplateArgs(
              collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        isTemplateMonomorphCanonicalKeyValueHelperPath(resolvedPath) &&
        hasVisibleStdCollectionsImportForPath(
            ctx,
            resolvedPath)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalKeyValueReceiverTemplateArgs(
              collectionHelperReceiverExpr(expr),
              params,
              locals,
              allowMathBare,
              namespacePrefix,
              ctx,
              receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
        allConcrete = true;
        inferredCanonicalKeyValueReceiverTemplateArgs = true;
      }
    }
    if (expr.templateArgs.empty() &&
        resolvedPath.rfind(semantics::legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
        resolvesCollectionVectorValueReceiver(
            collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveCollectionVectorValueReceiverTemplateArgs(
              collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        isExperimentalSoaVectorPublicHelperPath(resolvedPath) &&
        resolvesExperimentalSoaVectorReceiver(collectionHelperReceiverExpr(expr))) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalSoaVectorReceiverTemplateArgs(collectionHelperReceiverExpr(expr), receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (expr.templateArgs.empty() &&
        isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&
        resolvesBorrowedExperimentalSoaVectorReceiver(collectionHelperReceiverExpr(expr))) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalSoaVectorReceiverTemplateArgs(collectionHelperReceiverExpr(expr), receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
        allConcrete = true;
      }
    }
    if (resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (resolvedPath.rfind(semantics::legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprReferencePhase6([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<semantics::ParameterInfo> &params, [[maybe_unused]] RewriteExprState &st, RewriteExprInnerState &st2) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &mutableCollectionHelperReceiverExpr = st.mutableCollectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  [[maybe_unused]] auto &preferredBorrowedSoaWrapperPath = st.preferredBorrowedSoaWrapperPath;
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
  [[maybe_unused]] auto &resolvedPath = st2.resolvedPath;
  [[maybe_unused]] auto &inferredCanonicalKeyValueReceiverTemplateArgs = st2.inferredCanonicalKeyValueReceiverTemplateArgs;
    if (isExperimentalSoaVectorPublicHelperPath(resolvedPath) &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (isCanonicalSoaBorrowedWrapperHelper(resolvedPath) &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    if (inferredCanonicalKeyValueReceiverTemplateArgs &&
        isTemplateMonomorphCanonicalKeyValueHelperPath(resolvedPath) &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
    const std::string originalResolvedPath = resolvedPath;
    const std::string preferredPath = preferVectorStdlibHelperPath(resolvedPath, ctx.sourceDefs);
    if (preferredPath != resolvedPath && ctx.sourceDefs.count(preferredPath) > 0) {
      resolvedPath = preferredPath;
      expr.name = preferredPath;
    }
    const bool explicitCompatibilityAliasToCanonicalTemplate =
        expr.templateArgs.empty() && isExplicitCollectionCompatibilityAliasPath(originalResolvedPath) &&
        preferredPath != originalResolvedPath && ctx.templateDefs.count(preferredPath) > 0;
    const bool resolvedWasTemplate = ctx.templateDefs.count(resolvedPath) > 0;
    const bool isBuiltinKeyValueCountPath =
        isTemplateMonomorphCanonicalKeyValueCountPath(resolvedPath);
  st2.isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
  [[maybe_unused]] auto &isKnownDef = st2.isKnownDef;
    if (!hadExplicitTemplateArgsOnEntry && !expr.templateArgs.empty() &&
        !resolvedWasTemplate && isKnownDef && isBuiltinKeyValueCountPath) {
      // A same-path, non-templated user definition shadows the canonical
      // key-value count helper here. Implicit template-arg inference
      // pattern-matches on the canonical path text alone and doesn't know
      // about this local override, so it has attached template args that
      // don't apply to it (confirmed not user-written: `expr.templateArgs`
      // was empty when this call entered rewriting). Clear the inferred
      // args and let the call resolve as an ordinary, non-templated call
      // to the known definition instead of erroring. Explicit user-written
      // template args on such a definition (non-empty on entry) still fall
      // through to the diagnostic below, unchanged.
      expr.templateArgs.clear();
      expr.templateArgDetails.clear();
    }
    if (!expr.templateArgs.empty() && !resolvedWasTemplate && !isKnownDef && isBuiltinKeyValueCountPath) {
      error = "count does not accept template arguments";
      return st2.done(st.done(false));
    }
    if (!expr.templateArgs.empty() && !resolvedWasTemplate) {
      if (!shouldPreserveCompatibilityTemplatePath(resolvedPath, ctx)) {
        const std::string templatePreferredPath = preferVectorStdlibTemplatePath(resolvedPath, ctx);
        if (templatePreferredPath != resolvedPath) {
          resolvedPath = templatePreferredPath;
          expr.name = templatePreferredPath;
        }
      }
    }
    if (expr.templateArgs.empty() && !explicitCompatibilityAliasToCanonicalTemplate) {
      const std::string implicitTemplatePreferredPath =
          preferVectorStdlibImplicitTemplatePath(expr, resolvedPath, locals, params, allowMathBare, ctx, namespacePrefix);
      if (implicitTemplatePreferredPath != resolvedPath) {
        resolvedPath = implicitTemplatePreferredPath;
        expr.name = implicitTemplatePreferredPath;
      }
    }
  st2.preferredConcreteSamePathSoaHelperPath = [&](const std::string &path) {
      auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
        const size_t specializationSuffix = canonicalPath.find("__");
        if (specializationSuffix != std::string::npos) {
          canonicalPath.erase(specializationSuffix);
        }
        return canonicalPath;
      };
      const std::string canonicalPath = canonicalizeSoaHelperPath(path);
      auto extractHelperName = [&](std::string_view prefix) -> std::string {
        if (canonicalPath.rfind(std::string(prefix), 0) != 0) {
          return {};
        }
        return canonicalPath.substr(prefix.size());
      };
      std::string helperName =
          extractHelperName(templateMonomorphCompatibilitySoaHelperPrefix());
      if (helperName.empty()) {
        // TODO-5308: with `import /std/collections/soa/*` a bare helper call
        // arrives already resolved to the public /std/collections/soa/<helper>
        // spelling, which must reach the same-path /soa/<helper> shadow too.
        helperName = extractHelperName(templateMonomorphPublicSoaHelperPrefix());
      }
      if (helperName.empty()) {
        return std::string{};
      }
      if (!collection_helpers::isCountHelperName(helperName) &&
          !collection_helpers::isGetHelperName(helperName) &&
          !collection_helpers::isRefHelperName(helperName) &&
          helperName != templateMonomorphSoaToAosHelperName() &&
          helperName != templateMonomorphSoaToAosHelperName(true) &&
          helperName != "push" && helperName != "reserve") {
        return std::string{};
      }
      const std::string samePath =
          (helperName == templateMonomorphSoaToAosHelperName() ||
           helperName == templateMonomorphSoaToAosHelperName(true))
              ? "/" + helperName
              : templateMonomorphSamePathSoaHelperPrefix() + helperName;
      if ((ctx.sourceDefs.count(samePath) == 0 &&
           ctx.helperOverloads.count(samePath) == 0) ||
          ctx.templateDefs.count(samePath) > 0) {
        return std::string{};
      }
      const Expr *receiverExpr = collectionHelperReceiverExpr(expr);
      if (!resolvesExperimentalSoaVectorReceiver(receiverExpr) &&
          !resolvesBorrowedExperimentalSoaVectorReceiver(receiverExpr)) {
        return std::string{};
      }
      return samePath;
    };
  [[maybe_unused]] auto &preferredConcreteSamePathSoaHelperPath = st2.preferredConcreteSamePathSoaHelperPath;
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprReferencePhase7([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<semantics::ParameterInfo> &params, [[maybe_unused]] RewriteExprState &st, RewriteExprInnerState &st2) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &mutableCollectionHelperReceiverExpr = st.mutableCollectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  [[maybe_unused]] auto &preferredBorrowedSoaWrapperPath = st.preferredBorrowedSoaWrapperPath;
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
  [[maybe_unused]] auto &resolvedPath = st2.resolvedPath;
  [[maybe_unused]] auto &isKnownDef = st2.isKnownDef;
  [[maybe_unused]] auto &preferredConcreteSamePathSoaHelperPath = st2.preferredConcreteSamePathSoaHelperPath;
    if (const std::string samePathHelper =
            preferredConcreteSamePathSoaHelperPath(resolvedPath);
        !samePathHelper.empty()) {
      resolvedPath = samePathHelper;
      expr.name = samePathHelper;
      expr.namespacePrefix.clear();
      expr.templateArgs.clear();
    }
    if (ctx.helperOverloadInternalToPublic.count(resolvedPath) > 0) {
      expr.name = resolvedPath;
      expr.namespacePrefix.clear();
    }
    const Expr *resolvedReceiverExpr = collectionHelperReceiverExpr(expr);
    const std::string resolvedReceiverFamily =
        inferCollectionReceiverFamily(resolvedReceiverExpr);
    const bool isSyntheticSamePathSoaHelperTemplateCarry =
        isSyntheticSamePathSoaHelperTemplateCarryPath(resolvedPath) &&
        ctx.templateDefs.count(resolvedPath) == 0 &&
        !expr.templateArgs.empty() &&
        resolvedReceiverExpr != nullptr &&
        ((resolvedReceiverExpr->kind == Expr::Kind::Call &&
          !resolvedReceiverExpr->isBinding) ||
         resolvedReceiverFamily == "vector");
    if (isSyntheticSamePathSoaHelperTemplateCarry) {
      expr.templateArgs.clear();
    }
    const std::string latePreferredBorrowedSoaPath =
        preferredBorrowedSoaWrapperPath(resolvedPath);
    if (expr.templateArgs.empty() &&
        !latePreferredBorrowedSoaPath.empty() &&
        resolvesBorrowedExperimentalSoaVectorReceiver(resolvedReceiverExpr) &&
        (ctx.sourceDefs.count(latePreferredBorrowedSoaPath) > 0 ||
         ctx.helperOverloads.count(latePreferredBorrowedSoaPath) > 0 ||
         ctx.templateDefs.count(latePreferredBorrowedSoaPath) > 0)) {
      resolvedPath = latePreferredBorrowedSoaPath;
      expr.name = latePreferredBorrowedSoaPath;
      expr.namespacePrefix.clear();
    }
    auto publicSoaMutatorBasePath = [](std::string path) {
      if (const size_t specializationSuffix = path.find("__");
          specializationSuffix != std::string::npos) {
        path.erase(specializationSuffix);
      }
      if (path == semantics::publicSoaHelperTargetPath("push") ||
          path == semantics::publicSoaHelperTargetPath("reserve")) {
        return path;
      }
      return std::string{};
    };
    if (const std::string publicSoaMutatorPath =
            publicSoaMutatorBasePath(resolvedPath);
        !publicSoaMutatorPath.empty() &&
        resolvedReceiverFamily == "vector") {
      const std::string helperName = publicSoaMutatorPath.substr(
          templateMonomorphPublicSoaHelperPrefix().size());
      const std::string vectorPath =
          semantics::canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      if (ctx.sourceDefs.count(vectorPath) > 0 ||
          (semantics::isPublishedVectorMutatorHelperName(helperName) &&
           hasVisibleStdCollectionsImportForPath(ctx, publicSoaMutatorPath))) {
        resolvedPath = vectorPath;
        expr.name = vectorPath;
        expr.namespacePrefix.clear();
      }
    }
    if (const std::string publicSoaMutatorPath =
            publicSoaMutatorBasePath(resolvedPath);
        !publicSoaMutatorPath.empty() &&
        resolvedReceiverFamily == "soa") {
      const std::string helperName = publicSoaMutatorPath.substr(
          templateMonomorphPublicSoaHelperPrefix().size());
      const std::string implementationPath =
          semantics::publicSoaHelperTargetPath(
              helperName == "push" ? std::string("soa") + "VectorPush"
                                   : std::string("soa") + "VectorReserve");
      if (ctx.sourceDefs.count(implementationPath) > 0 ||
          ctx.templateDefs.count(implementationPath) > 0 ||
          hasVisibleStdCollectionsImportForPath(ctx, publicSoaMutatorPath)) {
        resolvedPath = implementationPath;
        expr.name = implementationPath;
        expr.namespacePrefix.clear();
      }
    }
    const bool isTemplateDef = ctx.templateDefs.count(resolvedPath) > 0;
    if (isTemplateDef) {
      auto defIt = ctx.sourceDefs.find(resolvedPath);
      const bool shouldInferImplicitTemplateTail =
          defIt != ctx.sourceDefs.end() &&
          !expr.templateArgs.empty() &&
          ctx.implicitTemplateDefs.count(resolvedPath) > 0 &&
          expr.templateArgs.size() < defIt->second.templateArgs.size();
      if (expr.templateArgs.empty() || shouldInferImplicitTemplateTail) {
        if (defIt != ctx.sourceDefs.end()) {
          std::vector<std::string> inferredArgs;
          if (inferImplicitTemplateArgs(defIt->second,
                                        expr,
                                        locals,
                                        params,
                                        mapping,
                                        allowedParams,
                                        namespacePrefix,
                                        ctx,
                                        allowMathBare,
                                        inferredArgs,
                                        error)) {
            expr.templateArgs = std::move(inferredArgs);
            allConcrete = true;
          } else if (!error.empty()) {
            return st2.done(st.done(false));
          }
        }
        }
        if (expr.templateArgs.empty()) {
          if (defIt != ctx.sourceDefs.end() &&
              definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
            allConcrete = true;
          } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(resolvedPath)) {
            return st2.done(st.done(true));
          } else {
            error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
            return st2.done(st.done(false));
          }
        }
      if (allConcrete) {
        std::string specializedPath;
        if (!instantiateTemplate(resolvedPath,
                                 expr.templateArgs,
                                 matchingTemplateArgumentDetails(expr.templateArgs,
                                                                 expr.templateArgDetails),
                                 ctx,
                                 error,
                                 specializedPath)) {
          return st2.done(st.done(false));
        }
        expr.name = specializedPath;
        expr.templateArgs.clear();
        expr.templateArgDetails.clear();
      }
    } else if (isKnownDef && !expr.templateArgs.empty()) {
      const Expr *templateCarryReceiverExpr = collectionHelperReceiverExpr(expr);
      const std::string templateCarryReceiverFamily =
          inferCollectionReceiverFamily(templateCarryReceiverExpr);
      if (isSyntheticSamePathSoaHelperTemplateCarryPath(resolvedPath) &&
          ctx.templateDefs.count(resolvedPath) == 0 &&
          templateCarryReceiverExpr != nullptr &&
          ((templateCarryReceiverExpr->kind == Expr::Kind::Call &&
            !templateCarryReceiverExpr->isBinding) ||
           templateCarryReceiverFamily == "vector")) {
        expr.templateArgs.clear();
      } else {
      const std::string resolvedGetPath =
          semantics::canonicalizeLegacySoaGetHelperPath(resolvedPath);
      const std::string resolvedRefPath =
          semantics::canonicalizeLegacySoaRefHelperPath(resolvedPath);
      std::string soaAccessHelper;
      if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedGetPath, "get")) {
        soaAccessHelper = "get";
      } else if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedGetPath, collection_helpers::kGetRef)) {
        soaAccessHelper = collection_helpers::kGetRef;
      } else if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, "ref")) {
        soaAccessHelper = "ref";
      } else if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, collection_helpers::kRefRef)) {
        soaAccessHelper = collection_helpers::kRefRef;
      }
      if (!soaAccessHelper.empty()) {
        if (expr.args.size() != 2) {
          error = "argument count mismatch for builtin " + soaAccessHelper;
          return st2.done(st.done(false));
        }
        const Expr &indexExpr = expr.args[1];
        if (indexExpr.kind == Expr::Kind::BoolLiteral ||
            indexExpr.kind == Expr::Kind::FloatLiteral ||
            indexExpr.kind == Expr::Kind::StringLiteral) {
          error = soaAccessHelper + " requires integer index";
          return st2.done(st.done(false));
        }
      }
      if (soaAccessHelper.empty() &&
          (semantics::isExperimentalSoaGetLikeHelperPath(resolvedPath) ||
           semantics::isExperimentalSoaRefLikeHelperPath(resolvedPath))) {
        if (resolvedPath.find(collection_helpers::kGetRef) != std::string::npos) {
          soaAccessHelper = collection_helpers::kGetRef;
        } else if (resolvedPath.find("get") != std::string::npos) {
          soaAccessHelper = "get";
        } else if (resolvedPath.find(collection_helpers::kRefRef) != std::string::npos) {
          soaAccessHelper = collection_helpers::kRefRef;
        } else if (resolvedPath.find("ref") != std::string::npos) {
          soaAccessHelper = "ref";
        }
        if (!soaAccessHelper.empty()) {
          if (expr.args.size() != 2) {
            error = "argument count mismatch for builtin " + soaAccessHelper;
            return st2.done(st.done(false));
          }
          const Expr &indexExpr = expr.args[1];
          if (indexExpr.kind == Expr::Kind::BoolLiteral ||
              indexExpr.kind == Expr::Kind::FloatLiteral ||
              indexExpr.kind == Expr::Kind::StringLiteral) {
            error = soaAccessHelper + " requires integer index";
            return st2.done(st.done(false));
          }
        }
      }
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return st2.done(st.done(false));
      }
    }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprReferencePhase8([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<semantics::ParameterInfo> &params, [[maybe_unused]] RewriteExprState &st, RewriteExprInnerState &st2) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &hadExplicitTemplateArgsOnEntry = st.hadExplicitTemplateArgsOnEntry;
  [[maybe_unused]] auto &isSyntheticSamePathSoaHelperTemplateCarryPath = st.isSyntheticSamePathSoaHelperTemplateCarryPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &mutableCollectionHelperReceiverExpr = st.mutableCollectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  [[maybe_unused]] auto &preferredBorrowedSoaWrapperPath = st.preferredBorrowedSoaWrapperPath;
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
    const std::string finalDirectCallResolvedPath =
        resolveCalleePath(expr, namespacePrefix, ctx);
    expr.resolvedCallPath = finalDirectCallResolvedPath;
    auto defIt = ctx.sourceDefs.find(finalDirectCallResolvedPath);
    if (defIt != ctx.sourceDefs.end()) {
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteVectorTargetValueForResolvedType(typeText, argExpr);
              })) {
        return st2.done(st.done(false));
      }
      if (!rewriteExperimentalConstructorArgsForTarget(
              expr,
              defIt->second,
              false,
              allowMathBare,
              ctx,
              [&](const std::string &typeText, Expr &argExpr) {
                return rewriteKeyValueTargetValueForResolvedType(typeText, argExpr);
              })) {
        return st2.done(st.done(false));
      }
    }
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteVectorTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteVectorTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalAssignTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteKeyValueTargetValueForResolvedType(targetTypeText, valueExpr);
        });
    rewriteExperimentalInitTargetValue(
        expr,
        params,
        locals,
        allowMathBare,
        namespacePrefix,
        ctx,
        [&](const std::string &targetTypeText, Expr &valueExpr) {
          return rewriteKeyValueTargetValueForResolvedType(targetTypeText, valueExpr);
        });
  return PhaseStatus::Continue;
}

} // namespace primec
