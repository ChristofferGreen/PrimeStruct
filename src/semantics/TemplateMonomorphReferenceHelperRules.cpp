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
#include "TemplateMonomorphExpressionRewriteContext.h"
#include "SemanticsValidateLambdaClosures.h"
#include "TemplateMonomorphExpressionReferenceRules.h"

namespace primec {

// The rules of a plain call expression that settle which helper it calls: borrowed vector helpers, experimental
// vector temporaries, the canonical target and same-path helper templates; then the runner of all the rules.

std::string preferredConcreteSamePathSoaHelperPath(ExprReferenceContext &rc,
                                                   const std::string &path) {
  Expr &expr = rc.expr;
  Context &ctx = rc.ctx;

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
  std::string helperName = extractHelperName(templateMonomorphCompatibilitySoaHelperPrefix());
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
      helperName != templateMonomorphSoaToAosHelperName(true) && helperName != "push" &&
      helperName != "reserve") {
    return std::string{};
  }
  const std::string samePath = (helperName == templateMonomorphSoaToAosHelperName() ||
                                helperName == templateMonomorphSoaToAosHelperName(true))
                                   ? "/" + helperName
                                   : templateMonomorphSamePathSoaHelperPrefix() + helperName;
  if ((ctx.sourceDefs.count(samePath) == 0 && ctx.helperOverloads.count(samePath) == 0) ||
      ctx.templateDefs.count(samePath) > 0) {
    return std::string{};
  }
  const Expr *receiverExpr = collectionHelperReceiverExpr(rc.st, expr);
  if (!resolvesExperimentalSoaVectorReceiver(rc.st, receiverExpr) &&
      !resolvesBorrowedExperimentalSoaVectorReceiver(rc.st, receiverExpr)) {
    return std::string{};
  }
  return samePath;
}

PhaseStatus rewriteBorrowedVectorHelperCall(ExprReferenceContext &rc) {
  ExprRewriteContext &st = rc.st;
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &resolvedPath = rc.resolvedPath;
  auto &borrowedCanonicalKeyValueUnknownTarget = rc.borrowedCanonicalKeyValueUnknownTarget;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &allConcrete = rc.st.allConcrete;
  if (!expr.isMethodCall) {
    unwrapDereferencedBorrowedVectorReceiver(expr, params, locals, allowMathBare, ctx);
  }
  if (rewriteBorrowedVectorBareHelperCall(expr, params, locals, allowMathBare, ctx)) {
    allConcrete = true;
    resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx, &locals, &params);
  }
  const std::string preferredCollectionHelperPath =
      preferCanonicalStdlibCollectionHelperPath(rc.st, resolvedPath);
  if (!error.empty()) {
    return rc.done(st.done(false));
  }
  if (preferredCollectionHelperPath != resolvedPath) {
    resolvedPath = preferredCollectionHelperPath;
    expr.name = preferredCollectionHelperPath;
    expr.namespacePrefix.clear();
  }
  if (!expr.templateArgs.empty() && expr.name.find('/') == std::string::npos &&
      !expr.args.empty()) {
    semantics::BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(
            expr.args.front(), params, locals, allowMathBare, ctx, receiverInfo)) {
      const std::string wrapperBase = normalizeCollectionReceiverTypeName(receiverInfo.typeName);
      if ((wrapperBase == "Reference" || wrapperBase == "Pointer") &&
          !receiverInfo.typeTemplateArg.empty()) {
        const std::string rootedWrapperMethodPath = "/" + wrapperBase + "/" + expr.name;
        if (ctx.templateDefs.count(rootedWrapperMethodPath) > 0 &&
            ctx.sourceDefs.count(rootedWrapperMethodPath) > 0) {
          resolvedPath = rootedWrapperMethodPath;
          expr.name = rootedWrapperMethodPath;
          expr.namespacePrefix.clear();
        }
      }
    }
  }
  if (!expr.isMethodCall && expr.templateArgs.empty() && expr.name.find('/') == std::string::npos &&
      (resolvedPath == "/count" || resolvedPath == "/capacity")) {
    const std::string samePathVectorHelper = "/" + std::string("vector") + "/" + expr.name;
    auto samePathVectorHelperIt = ctx.sourceDefs.find(samePathVectorHelper);
    if (samePathVectorHelperIt != ctx.sourceDefs.end() &&
        ctx.templateDefs.count(samePathVectorHelper) > 0 &&
        ctx.helperOverloads.count(samePathVectorHelper) == 0) {
      std::vector<std::string> inferredArgs;
      std::string inferenceError;
      if (inferImplicitTemplateArgs(samePathVectorHelperIt->second,
                                    expr,
                                    locals,
                                    params,
                                    mapping,
                                    allowedParams,
                                    namespacePrefix,
                                    ctx,
                                    allowMathBare,
                                    inferredArgs,
                                    inferenceError)) {
        resolvedPath = samePathVectorHelper;
        expr.name = samePathVectorHelper;
        expr.namespacePrefix.clear();
        expr.templateArgs = std::move(inferredArgs);
        allConcrete = true;
      }
    }
  }
  if (!expr.isMethodCall && expr.name.find('/') == std::string::npos &&
      semantics::isLegacyOrCanonicalSoaHelperPath(resolvedPath, "count") &&
      !isTemplateMonomorphSoaReceiverType(
          inferCollectionReceiverFamily(rc.st, collectionHelperReceiverExpr(rc.st, expr)))) {
    return rc.done(st.done(true));
  }
  const std::string preferredBorrowedSoaPath = preferredBorrowedSoaWrapperPath(rc.st, resolvedPath);
  if (!preferredBorrowedSoaPath.empty() &&
      resolvesBorrowedExperimentalSoaVectorReceiver(rc.st,
                                                    collectionHelperReceiverExpr(rc.st, expr)) &&
      (ctx.sourceDefs.count(preferredBorrowedSoaPath) > 0 ||
       ctx.helperOverloads.count(preferredBorrowedSoaPath) > 0 ||
       ctx.templateDefs.count(preferredBorrowedSoaPath) > 0)) {
    resolvedPath = preferredBorrowedSoaPath;
    expr.name = preferredBorrowedSoaPath;
    expr.namespacePrefix.clear();
  }
  const bool resolvesBorrowedExperimentalKeyValueReceiver =
      resolvesExperimentalKeyValueBorrowedReceiver(collectionHelperReceiverExpr(rc.st, expr),
                                                   params,
                                                   locals,
                                                   allowMathBare,
                                                   mapping,
                                                   allowedParams,
                                                   namespacePrefix,
                                                   ctx);
  rc.borrowedCanonicalKeyValueUnknownTarget =
      canonicalKeyValueHelperUnknownTargetPath(resolvedPath);
  if (!borrowedCanonicalKeyValueUnknownTarget.empty() &&
      resolvesBorrowedExperimentalKeyValueReceiver) {
    error = "unknown call target: " + borrowedCanonicalKeyValueUnknownTarget;
    return rc.done(st.done(false));
  }
  auto stripGeneratedSuffix = [](std::string path) {
    const size_t suffix = path.find("__");
    if (suffix != std::string::npos) {
      path.erase(suffix);
    }
    return path;
  };
  rc.removedKeyValueCompatibilityPath = stripGeneratedSuffix(resolvedPath);
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExperimentalVectorTemporary(ExprReferenceContext &rc) {
  ExprRewriteContext &st = rc.st;
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &resolvedPath = rc.resolvedPath;
  auto &inferredCanonicalKeyValueReceiverTemplateArgs =
      rc.inferredCanonicalKeyValueReceiverTemplateArgs;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &allConcrete = rc.st.allConcrete;
  const Expr *experimentalVectorReceiverExpr = collectionHelperReceiverExpr(rc.st, expr);
  const bool canRewriteNamedExperimentalVectorTemporary =
      experimentalVectorReceiverExpr == nullptr ||
      experimentalVectorReceiverExpr->kind != Expr::Kind::Call ||
      !semantics::hasNamedArguments(experimentalVectorReceiverExpr->argNames);
  const std::string experimentalVectorPath =
      experimentalVectorHelperPathForCanonicalHelper(resolvedPath);
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
              collectionHelperReceiverExpr(rc.st, expr),
              params,
              locals,
              allowMathBare,
              namespacePrefix,
              ctx,
              receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(rc.st, expr)) {
      if (!rewriteNestedExperimentalVectorConstructorValue(rc.st, *receiverExpr)) {
        return rc.done(st.done(false));
      }
    }
  }
  const std::string experimentalSoaVectorPath =
      experimentalSoaVectorHelperPathForCanonicalHelper(resolvedPath);
  const bool shouldRewriteCanonicalSoaHelperToExperimental =
      ctx.sourceDefs.count(resolvedPath) == 0 && ctx.helperOverloads.count(resolvedPath) == 0 &&
      !isCanonicalSoaBorrowedWrapperHelper(rc.st, resolvedPath) &&
      hasVisibleStdCollectionsImportForPath(ctx, resolvedPath) &&
      resolvesConcreteExperimentalSoaVectorReceiver(rc.st,
                                                    collectionHelperReceiverExpr(rc.st, expr));
  if (shouldRewriteCanonicalSoaHelperToExperimental && !experimentalSoaVectorPath.empty() &&
      ctx.sourceDefs.count(experimentalSoaVectorPath) > 0) {
    resolvedPath = experimentalSoaVectorPath;
    expr.name = experimentalSoaVectorPath;
    expr.namespacePrefix.clear();
    if (expr.templateArgs.empty()) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalSoaVectorReceiverTemplateArgs(
              rc.st, collectionHelperReceiverExpr(rc.st, expr), receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
  }
  rc.inferredCanonicalKeyValueReceiverTemplateArgs = false;
  if (expr.templateArgs.empty() &&
      resolvedPath.rfind(experimentalCollectionConstructorRootLocal("map"), 0) == 0 &&
      resolvesExperimentalKeyValueReceiver(collectionHelperReceiverExpr(rc.st, expr),
                                           params,
                                           locals,
                                           allowMathBare,
                                           mapping,
                                           allowedParams,
                                           namespacePrefix,
                                           ctx)) {
    std::vector<std::string> receiverTemplateArgs;
    if (resolveExperimentalKeyValueReceiverTemplateArgs(collectionHelperReceiverExpr(rc.st, expr),
                                                        params,
                                                        locals,
                                                        allowMathBare,
                                                        namespacePrefix,
                                                        ctx,
                                                        receiverTemplateArgs)) {
      expr.templateArgs = std::move(receiverTemplateArgs);
    }
  }
  if (expr.templateArgs.empty() && isTemplateMonomorphCanonicalKeyValueHelperPath(resolvedPath) &&
      hasVisibleStdCollectionsImportForPath(ctx, resolvedPath)) {
    std::vector<std::string> receiverTemplateArgs;
    if (resolveExperimentalKeyValueReceiverTemplateArgs(collectionHelperReceiverExpr(rc.st, expr),
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
      resolvesCollectionVectorValueReceiver(collectionHelperReceiverExpr(rc.st, expr),
                                            params,
                                            locals,
                                            allowMathBare,
                                            namespacePrefix,
                                            ctx)) {
    std::vector<std::string> receiverTemplateArgs;
    if (resolveCollectionVectorValueReceiverTemplateArgs(collectionHelperReceiverExpr(rc.st, expr),
                                                         params,
                                                         locals,
                                                         allowMathBare,
                                                         namespacePrefix,
                                                         ctx,
                                                         receiverTemplateArgs)) {
      expr.templateArgs = std::move(receiverTemplateArgs);
    }
  }
  if (expr.templateArgs.empty() && isExperimentalSoaVectorPublicHelperPath(resolvedPath) &&
      resolvesExperimentalSoaVectorReceiver(rc.st, collectionHelperReceiverExpr(rc.st, expr))) {
    std::vector<std::string> receiverTemplateArgs;
    if (resolveExperimentalSoaVectorReceiverTemplateArgs(
            rc.st, collectionHelperReceiverExpr(rc.st, expr), receiverTemplateArgs)) {
      expr.templateArgs = std::move(receiverTemplateArgs);
    }
  }
  if (expr.templateArgs.empty() && isCanonicalSoaBorrowedWrapperHelper(rc.st, resolvedPath) &&
      resolvesBorrowedExperimentalSoaVectorReceiver(rc.st,
                                                    collectionHelperReceiverExpr(rc.st, expr))) {
    std::vector<std::string> receiverTemplateArgs;
    if (resolveExperimentalSoaVectorReceiverTemplateArgs(
            rc.st, collectionHelperReceiverExpr(rc.st, expr), receiverTemplateArgs)) {
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

PhaseStatus resolveCanonicalHelperTarget(ExprReferenceContext &rc) {
  ExprRewriteContext &st = rc.st;
  Expr &expr = rc.expr;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &resolvedPath = rc.resolvedPath;
  auto &inferredCanonicalKeyValueReceiverTemplateArgs =
      rc.inferredCanonicalKeyValueReceiverTemplateArgs;
  auto &isKnownDef = rc.isKnownDef;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &hadExplicitTemplateArgsOnEntry = rc.st.hadExplicitTemplateArgsOnEntry;
  if (isExperimentalSoaVectorPublicHelperPath(resolvedPath) &&
      resolvedPath.find("__t") != std::string::npos) {
    expr.templateArgs.clear();
  }
  if (isCanonicalSoaBorrowedWrapperHelper(rc.st, resolvedPath) &&
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
      expr.templateArgs.empty() &&
      isExplicitCollectionCompatibilityAliasPath(originalResolvedPath) &&
      preferredPath != originalResolvedPath && ctx.templateDefs.count(preferredPath) > 0;
  const bool resolvedWasTemplate = ctx.templateDefs.count(resolvedPath) > 0;
  const bool isBuiltinKeyValueCountPath =
      isTemplateMonomorphCanonicalKeyValueCountPath(resolvedPath);
  rc.isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
  if (!hadExplicitTemplateArgsOnEntry && !expr.templateArgs.empty() && !resolvedWasTemplate &&
      isKnownDef && isBuiltinKeyValueCountPath) {
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
  if (!expr.templateArgs.empty() && !resolvedWasTemplate && !isKnownDef &&
      isBuiltinKeyValueCountPath) {
    error = "count does not accept template arguments";
    return rc.done(st.done(false));
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
    const std::string implicitTemplatePreferredPath = preferVectorStdlibImplicitTemplatePath(
        expr, resolvedPath, locals, params, allowMathBare, ctx, namespacePrefix);
    if (implicitTemplatePreferredPath != resolvedPath) {
      resolvedPath = implicitTemplatePreferredPath;
      expr.name = implicitTemplatePreferredPath;
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteSamePathHelperTemplate(ExprReferenceContext &rc) {
  ExprRewriteContext &st = rc.st;
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &resolvedPath = rc.resolvedPath;
  auto &isKnownDef = rc.isKnownDef;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &allConcrete = rc.st.allConcrete;
  if (const std::string samePathHelper = preferredConcreteSamePathSoaHelperPath(rc, resolvedPath);
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
  const Expr *resolvedReceiverExpr = collectionHelperReceiverExpr(rc.st, expr);
  const std::string resolvedReceiverFamily =
      inferCollectionReceiverFamily(rc.st, resolvedReceiverExpr);
  const bool isSyntheticSamePathSoaHelperTemplateCarry =
      isSyntheticSamePathSoaHelperTemplateCarryPath(rc.st, resolvedPath) &&
      ctx.templateDefs.count(resolvedPath) == 0 && !expr.templateArgs.empty() &&
      resolvedReceiverExpr != nullptr &&
      ((resolvedReceiverExpr->kind == Expr::Kind::Call && !resolvedReceiverExpr->isBinding) ||
       resolvedReceiverFamily == "vector");
  if (isSyntheticSamePathSoaHelperTemplateCarry) {
    expr.templateArgs.clear();
  }
  const std::string latePreferredBorrowedSoaPath =
      preferredBorrowedSoaWrapperPath(rc.st, resolvedPath);
  if (expr.templateArgs.empty() && !latePreferredBorrowedSoaPath.empty() &&
      resolvesBorrowedExperimentalSoaVectorReceiver(rc.st, resolvedReceiverExpr) &&
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
  if (const std::string publicSoaMutatorPath = publicSoaMutatorBasePath(resolvedPath);
      !publicSoaMutatorPath.empty() && resolvedReceiverFamily == "vector") {
    const std::string helperName =
        publicSoaMutatorPath.substr(templateMonomorphPublicSoaHelperPrefix().size());
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
  if (const std::string publicSoaMutatorPath = publicSoaMutatorBasePath(resolvedPath);
      !publicSoaMutatorPath.empty() && resolvedReceiverFamily == "soa") {
    const std::string helperName =
        publicSoaMutatorPath.substr(templateMonomorphPublicSoaHelperPrefix().size());
    const std::string implementationPath = semantics::publicSoaHelperTargetPath(
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
        defIt != ctx.sourceDefs.end() && !expr.templateArgs.empty() &&
        (ctx.implicitTemplateDefs.count(resolvedPath) > 0 ||
         semantics::isLambdaClosureFactoryPath(resolvedPath)) &&
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
          return rc.done(st.done(false));
        }
      }
    }
    if (expr.templateArgs.empty()) {
      if (defIt != ctx.sourceDefs.end() &&
          definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
        allConcrete = true;
      } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(rc.st, resolvedPath)) {
        return rc.done(st.done(true));
      } else {
        error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
        return rc.done(st.done(false));
      }
    }
    if (allConcrete) {
      std::string specializedPath;
      if (!instantiateTemplate(
              resolvedPath,
              expr.templateArgs,
              matchingTemplateArgumentDetails(expr.templateArgs, expr.templateArgDetails),
              ctx,
              error,
              specializedPath)) {
        return rc.done(st.done(false));
      }
      expr.name = specializedPath;
      expr.templateArgs.clear();
      expr.templateArgDetails.clear();
    }
  } else if (isKnownDef && !expr.templateArgs.empty()) {
    const Expr *templateCarryReceiverExpr = collectionHelperReceiverExpr(rc.st, expr);
    const std::string templateCarryReceiverFamily =
        inferCollectionReceiverFamily(rc.st, templateCarryReceiverExpr);
    if (isSyntheticSamePathSoaHelperTemplateCarryPath(rc.st, resolvedPath) &&
        ctx.templateDefs.count(resolvedPath) == 0 && templateCarryReceiverExpr != nullptr &&
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
      } else if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedGetPath,
                                                             collection_helpers::kGetRef)) {
        soaAccessHelper = collection_helpers::kGetRef;
      } else if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, "ref")) {
        soaAccessHelper = "ref";
      } else if (semantics::isLegacyOrCanonicalSoaHelperPath(resolvedRefPath,
                                                             collection_helpers::kRefRef)) {
        soaAccessHelper = collection_helpers::kRefRef;
      }
      if (!soaAccessHelper.empty()) {
        if (expr.args.size() != 2) {
          error = "argument count mismatch for builtin " + soaAccessHelper;
          return rc.done(st.done(false));
        }
        const Expr &indexExpr = expr.args[1];
        if (indexExpr.kind == Expr::Kind::BoolLiteral ||
            indexExpr.kind == Expr::Kind::FloatLiteral ||
            indexExpr.kind == Expr::Kind::StringLiteral) {
          error = soaAccessHelper + " requires integer index";
          return rc.done(st.done(false));
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
            return rc.done(st.done(false));
          }
          const Expr &indexExpr = expr.args[1];
          if (indexExpr.kind == Expr::Kind::BoolLiteral ||
              indexExpr.kind == Expr::Kind::FloatLiteral ||
              indexExpr.kind == Expr::Kind::StringLiteral) {
            error = soaAccessHelper + " requires integer index";
            return rc.done(st.done(false));
          }
        }
      }
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return rc.done(st.done(false));
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprReference(Expr &expr,
                                 const SubstMap &mapping,
                                 const std::unordered_set<std::string> &allowedParams,
                                 const std::string &namespacePrefix,
                                 Context &ctx,
                                 std::string &error,
                                 const LocalTypeMap &locals,
                                 const std::vector<semantics::ParameterInfo> &params,
                                 ExprRewriteContext &st) {
  ExprReferenceContext rc{
      expr, mapping, allowedParams, namespacePrefix, ctx, error, locals, params, st};
  // Ordered rules: each either decides the expression (Done) or leaves it to the next.
  using Rule = PhaseStatus (*)(ExprReferenceContext &);
  static constexpr Rule Rules[] = {prerewriteReceiverConstructors,
                                   rewriteKeyValuePairConstructor,
                                   rewriteBorrowedVectorHelperCall,
                                   rewriteKeyValueEntryConstructor,
                                   rewriteExperimentalVectorTemporary,
                                   resolveCanonicalHelperTarget,
                                   rewriteSamePathHelperTemplate,
                                   rewriteConstructorArgsForTarget};
  for (const Rule rule : Rules) {
    if (rule(rc) == PhaseStatus::Done) {
      return rc.result;
    }
  }
  return PhaseStatus::Continue;
}

} // namespace primec
