// map-surface-audit: exempt
// vector-surface-audit: exempt
// soa-surface-audit: exempt
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

namespace primec {

PhaseStatus rewriteExprPhase4([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolveExperimentalSoaVectorReceiverTemplateArgs = st.resolveExperimentalSoaVectorReceiverTemplateArgs;
  st.resolvesExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
    std::vector<std::string> receiverTemplateArgs;
    return resolveExperimentalSoaVectorReceiverTemplateArgs(receiverExpr, receiverTemplateArgs);
  };
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  st.resolvesBorrowedExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    auto inferFromTypeText = [&](std::string receiverTypeText) {
      if (receiverTypeText.empty()) {
        return false;
      }
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(normalizeBindingTypeName(receiverTypeText), base, argText)) {
        return false;
      }
      const std::string normalizedBase = normalizeBindingTypeName(base);
      if ((normalizedBase != "Reference" && normalizedBase != "Pointer") ||
          argText.empty()) {
        return false;
      }
      std::vector<std::string> wrappedArgs;
      if (!splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
        return false;
      }
      std::vector<std::string> receiverTemplateArgs;
      return extractExperimentalSoaVectorValueReceiverTemplateArgsFromTypeText(
          wrappedArgs.front(), ctx, receiverTemplateArgs);
    };
    auto definitionReturnsBorrowedExperimentalSoaVector =
        [&](const Definition &definition) {
          BindingInfo inferredReturn;
          if (inferDefinitionReturnBindingForTemplatedFallback(
                  definition, allowMathBare, ctx, inferredReturn) &&
              inferFromTypeText(bindingTypeToString(inferredReturn))) {
            return true;
          }
          for (const auto &transform : definition.transforms) {
            if (transform.name == "return" &&
                transform.templateArgs.size() == 1 &&
                inferFromTypeText(transform.templateArgs.front())) {
              return true;
            }
          }
          return false;
        };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo) &&
        inferFromTypeText(bindingTypeToString(receiverInfo))) {
      return true;
    }
    if (receiverExpr->kind == Expr::Kind::Call && !receiverExpr->isBinding) {
      std::vector<std::string> receiverCandidatePaths;
      auto appendReceiverCandidatePath = [&](const std::string &candidatePath) {
        if (!candidatePath.empty() &&
            std::find(receiverCandidatePaths.begin(),
                      receiverCandidatePaths.end(),
                      candidatePath) == receiverCandidatePaths.end()) {
          receiverCandidatePaths.push_back(candidatePath);
        }
      };
      std::string resolvedReceiverPath;
      if (receiverExpr->isMethodCall) {
        if (resolveMethodCallTemplateTarget(*receiverExpr, locals, ctx, resolvedReceiverPath)) {
          appendReceiverCandidatePath(resolvedReceiverPath);
        }
      } else {
        appendReceiverCandidatePath(resolveCalleePath(*receiverExpr, namespacePrefix, ctx));
        if (!receiverExpr->name.empty() && receiverExpr->name.front() == '/') {
          appendReceiverCandidatePath(receiverExpr->name);
        } else {
          if (!receiverExpr->namespacePrefix.empty()) {
            appendReceiverCandidatePath(receiverExpr->namespacePrefix + "/" + receiverExpr->name);
          }
          appendReceiverCandidatePath("/" + receiverExpr->name);
          appendReceiverCandidatePath(receiverExpr->name);
        }
      }
      for (const std::string &receiverCandidatePath : receiverCandidatePaths) {
        auto defIt = ctx.sourceDefs.find(receiverCandidatePath);
        if (defIt == ctx.sourceDefs.end()) {
          continue;
        }
        if (definitionReturnsBorrowedExperimentalSoaVector(defIt->second)) {
          return true;
        }
      }
      if (!receiverExpr->name.empty()) {
        for (const auto &[defPath, definition] : ctx.sourceDefs) {
          const size_t slash = defPath.find_last_of('/');
          const std::string leaf =
              slash == std::string::npos ? defPath : defPath.substr(slash + 1);
          if (leaf == receiverExpr->name &&
              definitionReturnsBorrowedExperimentalSoaVector(definition)) {
            return true;
          }
        }
      }
    }
    return inferFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  st.resolvesConcreteExperimentalSoaVectorReceiver = [&](const Expr *receiverExpr) {
    if (receiverExpr == nullptr) {
      return false;
    }
    auto inferFromTypeText = [&](std::string receiverTypeText) {
      if (receiverTypeText.empty()) {
        return false;
      }
      receiverTypeText = normalizeBindingTypeName(receiverTypeText);
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
          break;
        }
        const std::string normalizedBase = normalizeBindingTypeName(base);
        if (normalizedBase == "Reference" || normalizedBase == "Pointer") {
          std::vector<std::string> wrappedArgs;
          if (!splitTopLevelTemplateArgs(argText, wrappedArgs) || wrappedArgs.size() != 1) {
            return false;
          }
          receiverTypeText = normalizeBindingTypeName(wrappedArgs.front());
          continue;
        }
        return isExperimentalSoaVectorSpecializedTypePath(normalizedBase);
      }
      if (!receiverTypeText.empty() && receiverTypeText.front() != '/') {
        receiverTypeText.insert(receiverTypeText.begin(), '/');
      }
      return isExperimentalSoaVectorSpecializedTypePath(
          normalizeBindingTypeName(receiverTypeText));
    };
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     ctx,
                                     receiverInfo) &&
        inferFromTypeText(bindingTypeToString(receiverInfo))) {
      return true;
    }
    return inferFromTypeText(inferExprTypeTextForTemplatedVectorFallback(
        *receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  [[maybe_unused]] auto &resolvesConcreteExperimentalSoaVectorReceiver = st.resolvesConcreteExperimentalSoaVectorReceiver;
  st.inferCollectionReceiverFamily = [&](const Expr *receiverExpr) -> std::string {
    auto inferFromTypeText = [&](std::string receiverTypeText) -> std::string {
      if (receiverTypeText.empty()) {
        return std::string{};
      }
      while (true) {
        std::string base;
        std::string argText;
        if (!splitTemplateTypeName(receiverTypeText, base, argText) || base.empty()) {
          return normalizeCollectionReceiverTypeName(receiverTypeText);
        }
        const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
        if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
          return normalizedBase;
        }
        std::vector<std::string> receiverArgs;
        if (!splitTopLevelTemplateArgs(argText, receiverArgs) || receiverArgs.size() != 1) {
          return {};
        }
        receiverTypeText = receiverArgs.front();
      }
    };
    if (receiverExpr == nullptr) {
      return {};
    }
    BindingInfo receiverInfo;
    if (inferBindingTypeForMonomorph(*receiverExpr, params, locals, allowMathBare, ctx, receiverInfo)) {
      const std::string family = inferFromTypeText(bindingTypeToString(receiverInfo));
      if (!family.empty()) {
        return family;
      }
    }
    return inferFromTypeText(
        inferExprTypeTextForTemplatedVectorFallback(*receiverExpr, locals, namespacePrefix, ctx, allowMathBare));
  };
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  auto preferredBorrowedPathForPublicOrCompatibility =
      [](const std::string &canonicalPath, std::string_view borrowedHelper) {
        if (canonicalPath.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) {
          return publicSoaHelperTargetPath(borrowedHelper);
        }
        return compatibilitySoaHelperTargetPath(borrowedHelper);
      };
  st.isCanonicalSoaBorrowedWrapperHelper = [&](const std::string &path) {
    auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
      const size_t specializationSuffix = canonicalPath.find("__");
      if (specializationSuffix != std::string::npos) {
        canonicalPath.erase(specializationSuffix);
      }
      return canonicalPath;
    };
    const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
    const std::string canonicalSoaGetPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaGetHelperPath(path));
    const std::string canonicalSoaRefPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaRefHelperPath(path));
    const std::string canonicalSoaToAosPath =
        canonicalizeLegacySoaToAosHelperPath(path);
    return isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, collection_helpers::kCountRef) ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, collection_helpers::kGetRef) ||
           isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, collection_helpers::kRefRef) ||
           isLegacyOrCanonicalSoaHelperPath(
               canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName(true));
  };
  [[maybe_unused]] auto &isCanonicalSoaBorrowedWrapperHelper = st.isCanonicalSoaBorrowedWrapperHelper;
  st.preferredBorrowedSoaWrapperPath = [&](const std::string &path) {
    auto canonicalizeSoaHelperPath = [](std::string canonicalPath) {
      const size_t specializationSuffix = canonicalPath.find("__");
      if (specializationSuffix != std::string::npos) {
        canonicalPath.erase(specializationSuffix);
      }
      return canonicalPath;
    };
    const std::string canonicalSoaCountPath = canonicalizeSoaHelperPath(path);
    const std::string canonicalSoaGetPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaGetHelperPath(path));
    const std::string canonicalSoaRefPath =
        canonicalizeSoaHelperPath(canonicalizeLegacySoaRefHelperPath(path));
    const std::string canonicalSoaToAosPath =
        canonicalizeLegacySoaToAosHelperPath(path);
    if (path == "/count" || path == "count") {
      return compatibilitySoaHelperTargetPath(collection_helpers::kCountRef);
    }
    if (path == "/get" || path == "get") {
      return compatibilitySoaHelperTargetPath(collection_helpers::kGetRef);
    }
    if (path == "/ref" || path == "ref") {
      return compatibilitySoaHelperTargetPath(collection_helpers::kRefRef);
    }
    const std::string samePathSoaToAos =
        "/" + templateMonomorphSoaToAosHelperName();
    if (path == samePathSoaToAos ||
        path == templateMonomorphSoaToAosHelperName()) {
      return compatibilitySoaHelperTargetPath(
          templateMonomorphSoaToAosHelperName(true));
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaCountPath, "count")) {
      return preferredBorrowedPathForPublicOrCompatibility(canonicalSoaCountPath,
                                                           collection_helpers::kCountRef);
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaGetPath, "get")) {
      return preferredBorrowedPathForPublicOrCompatibility(canonicalSoaGetPath,
                                                           collection_helpers::kGetRef);
    }
    if (isLegacyOrCanonicalSoaHelperPath(canonicalSoaRefPath, "ref")) {
      return preferredBorrowedPathForPublicOrCompatibility(canonicalSoaRefPath,
                                                           collection_helpers::kRefRef);
    }
    if (isLegacyOrCanonicalSoaHelperPath(
            canonicalSoaToAosPath, templateMonomorphSoaToAosHelperName())) {
      return compatibilitySoaHelperTargetPath(
          templateMonomorphSoaToAosHelperName(true));
    }
    return std::string{};
  };
  [[maybe_unused]] auto &preferredBorrowedSoaWrapperPath = st.preferredBorrowedSoaWrapperPath;
  st.preferCanonicalStdlibCollectionHelperPath = [&](const std::string &path) {
    const Expr *receiverExpr = collectionHelperReceiverExpr(expr);
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
        BindingInfo paramBinding;
        if (!extractExplicitBindingType(defIt->second.parameters.front(), paramBinding)) {
          return false;
        }
        return isTemplateMonomorphSoaReceiverType(
                   normalizeBindingTypeName(paramBinding.typeName)) &&
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
      if (isTemplateMonomorphSoaReceiverType(
              inferCollectionReceiverFamily(candidate))) {
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
      BindingInfo inferredReturn;
      if (!inferDefinitionReturnBindingForTemplatedFallback(
              defIt->second, hasMathImport(ctx), const_cast<Context &>(ctx), inferredReturn)) {
        return false;
      }
      return isTemplateMonomorphSoaReceiverType(
                 normalizeBindingTypeName(inferredReturn.typeName)) &&
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
    if (isCanonicalVectorCompatibilityPath(path)) {
      helperName =
          std::string(stripUnrootedCanonicalVectorCompatibilityPrefix(path));
      resolvesVectorFamilyPath = true;
    } else if (path.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0) {
      helperName = path.substr(templateMonomorphCompatibilitySoaHelperPrefix().size());
    } else if (!expr.isMethodCall &&
               !path.empty() &&
               path.front() == '/' &&
               path.find('/', 1) == std::string::npos &&
               ctx.sourceDefs.count(path) == 0 &&
               ctx.helperOverloads.count(path) == 0) {
      helperName = path.substr(1);
    } else if (!expr.isMethodCall &&
               !expr.name.empty() &&
               expr.name.find('/') == std::string::npos &&
               ctx.sourceDefs.count(path) == 0 &&
               ctx.helperOverloads.count(path) == 0) {
      helperName = expr.name;
    } else if (expr.isMethodCall &&
               (expr.name == "remove_at" || expr.name == "remove_swap") &&
               ctx.sourceDefs.count(path) == 0 &&
               ctx.helperOverloads.count(path) == 0) {
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
    if (!collection_helpers::isCountHelperName(helperName) &&
        helperName != "capacity" && helperName != "push" &&
        helperName != "pop" && helperName != "reserve" &&
        helperName != "clear" && helperName != "remove_at" &&
        helperName != "remove_swap" && !collection_helpers::isGetHelperName(helperName) && !collection_helpers::isRefHelperName(helperName) &&
        helperName != templateMonomorphSoaToAosHelperName() &&
        helperName != templateMonomorphSoaToAosHelperName(true)) {
      return path;
    }
    const std::string receiverFamily = inferCollectionReceiverFamily(receiverExpr);
    const bool receiverResolvesExperimentalSoaVector =
        resolvesExperimentalSoaVectorReceiver(receiverExpr);
    const bool receiverResolvesBorrowedExperimentalSoaVector =
        resolvesBorrowedExperimentalSoaVectorReceiver(receiverExpr);
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
        !expr.isMethodCall &&
        !resolvesVectorFamilyPath &&
        expr.namespacePrefix.empty() &&
        expr.name == helperName &&
        expr.name.find('/') == std::string::npos &&
        isPublishedVectorMutatorHelperName(helperName);
    const bool isMethodPublishedVectorMutatorSugar =
        expr.isMethodCall && isPublishedVectorMutatorHelperName(helperName);
    if ((isBarePublishedVectorMutatorSugar ||
         isMethodPublishedVectorMutatorSugar) &&
        resolvesCollectionVectorValueReceiver(
            receiverExpr, params, locals, allowMathBare, namespacePrefix, ctx)) {
      error = "unknown call target: " +
              canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      return path;
    }
    if (receiverFamily == "vector" &&
        (collection_helpers::isCountHelperName(helperName) ||
         helperName == "capacity")) {
      const std::string samePathVectorHelper =
          "/" + std::string("vector") + "/" + helperName;
      if (hasDefinitionFamilyPath(samePathVectorHelper)) {
        return samePathVectorHelper;
      }
    }
    // Map twin of the vector same-path branch above: a rooted
    // `/map/count` / `/map/count_ref` user shadow wins for bare
    // `count(m)` / `count_ref(m)` on a map receiver, exactly like
    // `/vector/count` / `/vector/capacity` do for vector receivers.
    if (receiverFamily == "map" &&
        (collection_helpers::isCountHelperName(helperName))) {
      const std::string samePathMapHelper =
          "/" + std::string("map") + "/" + helperName;
      if (hasDefinitionFamilyPath(samePathMapHelper)) {
        return samePathMapHelper;
      }
    }
    const auto receiverHasVisibleCanonicalCollectionHelper =
        [&](std::string_view candidateHelperName) {
          const std::string preferred =
              canonicalVectorCompatibilityHelperPathOrFallback(candidateHelperName);
          return receiverFamily == "vector" &&
                 hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
                 ctx.sourceDefs.count(preferred) > 0;
        };
    if (collection_helpers::isCountHelperName(helperName) ||
        helperName == "push" || helperName == "reserve") {
      const std::string samePathSoaNonRefHelper =
          templateMonomorphSamePathSoaHelperPrefix() + helperName;
      const bool receiverEligibleForSamePathSoaHelper =
          isTemplateMonomorphSoaReceiverType(receiverFamily) ||
          receiverResolvesBorrowedExperimentalSoaVector ||
           receiverResolvesExperimentalSoaVector ||
          ((collection_helpers::isCountHelperName(helperName)) &&
           receiverFamily == "vector" &&
           !receiverHasVisibleCanonicalCollectionHelper(helperName));
      if (receiverEligibleForSamePathSoaHelper &&
          hasDefinitionFamilyPath(samePathSoaNonRefHelper)) {
        return samePathSoaNonRefHelper;
      }
    }
    // A public soa<T> receiver reports family "soa" rather than the
    // internal soa_vector name; it must still reach a same-path
    // /soa/<helper> shadow for the access helpers.
    const bool receiverIsPublicSoa = receiverFamily == "soa";
    if (collection_helpers::isGetHelperName(helperName)) {
      const std::string samePathGetHelper =
          templateMonomorphSamePathSoaHelperPrefix() + helperName;
      if (hasDefinitionFamilyPath(samePathGetHelper) &&
          (isTemplateMonomorphSoaReceiverType(receiverFamily) ||
           receiverIsPublicSoa ||
           receiverResolvesBorrowedExperimentalSoaVector ||
           receiverResolvesExperimentalSoaVector ||
           receiverFamily == "vector")) {
        return samePathGetHelper;
      }
    }
    if (collection_helpers::isRefHelperName(helperName)) {
      const std::string samePathRefHelper =
          templateMonomorphSamePathSoaHelperPrefix() + helperName;
      if (hasDefinitionFamilyPath(samePathRefHelper) &&
          (isTemplateMonomorphSoaReceiverType(receiverFamily) ||
           receiverIsPublicSoa ||
           receiverResolvesBorrowedExperimentalSoaVector ||
           receiverResolvesExperimentalSoaVector ||
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
      const std::string preferred =
          templateMonomorphCompatibilitySoaHelperPrefix() + helperName;
      if (ctx.sourceDefs.count(preferred) > 0 &&
          (resolvesVectorFamilyPath ||
           hasVisibleStdCollectionsImportForPath(ctx, preferred))) {
        return preferred;
      }
    }
    if (!resolvesVectorFamilyPath && receiverFamily == "map" &&
        (collection_helpers::isCountHelperName(helperName) ||
         collection_helpers::isAtHelperName(helperName) ||
         collection_helpers::isAtUnsafeHelperName(helperName))) {
      const std::string preferred =
          templateMonomorphCanonicalKeyValueHelperPath(helperName);
      if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
          ctx.sourceDefs.count(preferred) > 0) {
        return preferred;
      }
    }
    if (!resolvesVectorFamilyPath &&
        (receiverFamily == "vector" || receiverFamily == "array")) {
      const std::string preferred =
          canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      if (hasVisibleStdCollectionsImportForPath(ctx, preferred) &&
          ctx.sourceDefs.count(preferred) > 0) {
        return preferred;
      }
      if (isPublishedVectorMutatorHelperName(helperName)) {
        return preferred;
      }
    }
    return path;
  };
  [[maybe_unused]] auto &preferCanonicalStdlibCollectionHelperPath = st.preferCanonicalStdlibCollectionHelperPath;
  return PhaseStatus::Continue;
}

PhaseStatus rewriteExprPhase5([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
  [[maybe_unused]] auto &allowMathBare = st.allowMathBare;
  [[maybe_unused]] auto &isCanonicalBuiltinKeyValueHelperPath = st.isCanonicalBuiltinKeyValueHelperPath;
  [[maybe_unused]] auto &isCanonicalStdlibCollectionHelperPath = st.isCanonicalStdlibCollectionHelperPath;
  [[maybe_unused]] auto &isTemplatedAutoCompatVectorHelperPath = st.isTemplatedAutoCompatVectorHelperPath;
  [[maybe_unused]] auto &collectionHelperReceiverExpr = st.collectionHelperReceiverExpr;
  [[maybe_unused]] auto &resolvesBuiltinKeyValueReceiver = st.resolvesBuiltinKeyValueReceiver;
  [[maybe_unused]] auto &resolvesBuiltinVectorReceiver = st.resolvesBuiltinVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamilyForRewrite = st.inferCollectionReceiverFamilyForRewrite;
  [[maybe_unused]] auto &resolvesSoaReceiverForRewrite = st.resolvesSoaReceiverForRewrite;
  [[maybe_unused]] auto &resolvesExperimentalSoaVectorReceiver = st.resolvesExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &resolvesBorrowedExperimentalSoaVectorReceiver = st.resolvesBorrowedExperimentalSoaVectorReceiver;
  [[maybe_unused]] auto &inferCollectionReceiverFamily = st.inferCollectionReceiverFamily;
  st.shouldDeferStdlibCollectionHelperTemplateRewrite = [&](const std::string &path) {
    if (!expr.templateArgs.empty()) {
      return false;
    }
    auto forwardedEmptyConstructorPath = [] {
      const primec::StdlibSurfaceMetadata *metadata =
          keyValueConstructorSurfaceMetadataLocal();
      if (metadata == nullptr) {
        return std::string{};
      }
      const std::string_view memberName =
          primec::resolveStdlibSurfaceMemberName(*metadata,
                                                 metadata->canonicalPath);
      if (memberName.empty()) {
        return std::string{};
      }
      return experimentalCollectionConstructorPathLocal(
          "map", std::string(memberName) + "New");
    };
    const std::string emptyConstructorPath = forwardedEmptyConstructorPath();
    if (!emptyConstructorPath.empty() && path == emptyConstructorPath &&
        ctx.templateDefs.count(path) > 0) {
      return true;
    }
    if (isTemplatedAutoCompatVectorHelperPath(path)) {
      return true;
    }
    if (!isCanonicalStdlibCollectionHelperPath(path)) {
      return false;
    }
    const Expr *helperReceiverExpr = collectionHelperReceiverExpr(expr);
    const bool borrowedExperimentalSoaReceiver =
        resolvesBorrowedExperimentalSoaVectorReceiver(helperReceiverExpr);
    const bool isCanonicalNonBorrowedSoaHelperPath =
        path == compatibilitySoaHelperTargetPath("count") ||
        path == publicSoaHelperTargetPath("count") ||
        path == compatibilitySoaHelperTargetPath("get") ||
        path == publicSoaHelperTargetPath("get") ||
        path == compatibilitySoaHelperTargetPath("ref") ||
        path == publicSoaHelperTargetPath("ref") ||
        path == compatibilitySoaHelperTargetPath(
                    templateMonomorphSoaToAosHelperName()) ||
        path == publicSoaHelperTargetPath(
                    templateMonomorphSoaToAosHelperName());
    if (borrowedExperimentalSoaReceiver &&
        isCanonicalNonBorrowedSoaHelperPath) {
      return true;
    }
    if ((path.rfind(templateMonomorphCompatibilitySoaHelperPrefix(), 0) == 0 ||
         path.rfind(templateMonomorphPublicSoaHelperPrefix(), 0) == 0) &&
        helperReceiverExpr != nullptr) {
      if (path == publicSoaHelperTargetPath(
                      templateMonomorphSoaToAosHelperName()) ||
          path == publicSoaHelperTargetPath(
                      templateMonomorphSoaToAosHelperName(true))) {
        return true;
      }
      const bool directArgsPackReceiver =
          inferCollectionReceiverFamilyForRewrite(helperReceiverExpr) == "args";
      const bool directBuiltinSoaReceiver =
          isTemplateMonomorphSoaReceiverType(
              inferCollectionReceiverFamily(helperReceiverExpr));
      const bool directExperimentalSoaReceiver =
          resolvesExperimentalSoaVectorReceiver(helperReceiverExpr);
      if (directArgsPackReceiver || directBuiltinSoaReceiver ||
          directExperimentalSoaReceiver ||
          borrowedExperimentalSoaReceiver) {
        return true;
      }
    }
    if (hasVisibleStdCollectionsImportForPath(ctx, path) && ctx.templateDefs.count(path) > 0) {
      if (isCanonicalVectorCompatibilityPath(path) &&
          !resolvesBuiltinVectorReceiver(collectionHelperReceiverExpr(expr)) &&
          !resolvesCollectionVectorValueReceiver(
              collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx)) {
        return true;
      }
      if (isTemplateMonomorphCanonicalKeyValueHelperPath(path)) {
        return true;
      }
      return false;
    }
    if (isCanonicalBuiltinKeyValueHelperPath(path)) {
      return resolvesBuiltinKeyValueReceiver(collectionHelperReceiverExpr(expr)) && ctx.templateDefs.count(path) == 0;
    }
    return true;
  };
  [[maybe_unused]] auto &shouldDeferStdlibCollectionHelperTemplateRewrite = st.shouldDeferStdlibCollectionHelperTemplateRewrite;
  [[maybe_unused]] auto &rewriteNestedExperimentalKeyValueConstructorValue = st.rewriteNestedExperimentalKeyValueConstructorValue;
  std::function<bool(Expr &)> rewriteNestedExperimentalKeyValueResultOkPayloadValue;
  [[maybe_unused]] auto &rewriteNestedExperimentalVectorConstructorValue = st.rewriteNestedExperimentalVectorConstructorValue;
  [[maybe_unused]] auto &rewriteKeyValueTargetValueForResolvedType = st.rewriteKeyValueTargetValueForResolvedType;
  [[maybe_unused]] auto &rewriteVectorTargetValueForResolvedType = st.rewriteVectorTargetValueForResolvedType;
  rewriteKeyValueTargetValueForResolvedType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return rewriteExperimentalKeyValueTargetValueForType(typeText,
                                                         valueExpr,
                                                         mapping,
                                                         allowedParams,
                                                         namespacePrefix,
                                                         ctx,
                                                         rewriteNestedExperimentalKeyValueConstructorValue,
                                                         rewriteNestedExperimentalKeyValueResultOkPayloadValue);
  };
  rewriteVectorTargetValueForResolvedType = [&](const std::string &typeText, Expr &valueExpr) -> bool {
    return rewriteExperimentalVectorTargetValueForType(typeText,
                                                       valueExpr,
                                                       rewriteNestedExperimentalVectorConstructorValue);
  };
  auto rewriteCanonicalKeyValueConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesExperimentalKeyValueTypeText(bindingTypeText,
                                                      mapping,
                                                      allowedParams,
                                                      namespacePrefix,
                                                      ctx);
        },
        "map",
        rewriteKeyValueTargetValueForResolvedType);
  };
  auto rewriteCanonicalExperimentalVectorConstructorBinding = [&](Expr &bindingExpr) -> bool {
    return rewriteExperimentalConstructorBinding(
        bindingExpr,
        params,
        locals,
        allowMathBare,
        ctx,
        [&](const std::string &bindingTypeText) {
          return resolvesCollectionVectorValueTypeText(bindingTypeText);
        },
        "vector",
        rewriteVectorTargetValueForResolvedType);
  };
  rewriteNestedExperimentalKeyValueConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return rewriteCanonicalExperimentalKeyValueConstructorExpr(current,
                                                                 locals,
                                                                 params,
                                                                 mapping,
                                                                 allowedParams,
                                                                 namespacePrefix,
                                                                 ctx,
                                                                 allowMathBare,
                                                                 error);
    });
  };
  rewriteNestedExperimentalKeyValueResultOkPayloadValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalKeyValueResultOkPayloadTree(candidate, rewriteNestedExperimentalKeyValueConstructorValue);
  };
  rewriteNestedExperimentalVectorConstructorValue = [&](Expr &candidate) -> bool {
    return rewriteExperimentalConstructorValueTree(candidate, [&](Expr &current) {
      return rewriteCanonicalExperimentalVectorConstructorExpr(current,
                                                               locals,
                                                               params,
                                                               mapping,
                                                               allowedParams,
                                                               namespacePrefix,
                                                               ctx,
                                                               allowMathBare,
                                                               error);
    });
  };
  if (expr.isBinding) {
    if (!rewriteCanonicalExperimentalVectorConstructorBinding(expr)) {
      return st.done(false);
    }
    if (!rewriteCanonicalKeyValueConstructorBinding(expr)) {
      return st.done(false);
    }
  }
  if (!expr.isBinding) {
    if (!expr.isMethodCall && expr.namespacePrefix.empty() &&
        expr.name.find('/') == std::string::npos && expr.args.size() == 1 &&
        !hasNamedCallArguments(expr) && expr.templateArgs.empty() &&
        !expr.hasBodyArguments && expr.bodyArguments.empty()) {
      BindingInfo receiverInfo;
      if (inferBindingTypeForMonomorph(expr.args.front(),
                                       params,
                                       locals,
                                       allowMathBare,
                                       ctx,
                                       receiverInfo) &&
          receiverInfo.typeTemplateArg.empty()) {
        const std::string receiverType =
            normalizeBindingTypeName(receiverInfo.typeName);
        if (receiverType == "i32" || receiverType == "i64" ||
            receiverType == "u64" || receiverType == "bool" ||
            receiverType == "f32" || receiverType == "f64" ||
            receiverType == "integer" || receiverType == "decimal" ||
            receiverType == "complex") {
          const std::string methodStylePath =
              "/" + receiverType + "/" + expr.name;
          if (ctx.sourceDefs.count(methodStylePath) > 0 ||
              ctx.helperOverloads.count(methodStylePath) > 0) {
            expr.name = methodStylePath;
          }
        }
      }
    }
    if (expr.isMethodCall &&
        (expr.name == "at" || expr.name == "at_unsafe") &&
        expr.namespacePrefix.empty() &&
        !expr.args.empty() &&
        inferCollectionReceiverFamilyForRewrite(&expr.args.front()) == "string") {
      expr.isMethodCall = false;
      expr.namespacePrefix.clear();
    }
    auto helperReturnSoaRefHelper = [&]() -> std::string {
      if (expr.args.empty() || expr.args.front().kind != Expr::Kind::Call) {
        return std::string{};
      }
      std::string normalizedName = expr.name;
      if (!normalizedName.empty() && normalizedName.front() == '/') {
        normalizedName.erase(normalizedName.begin());
      }
      std::string normalizedPrefix = expr.namespacePrefix;
      if (!normalizedPrefix.empty() && normalizedPrefix.front() == '/') {
        normalizedPrefix.erase(normalizedPrefix.begin());
      }
      const bool usesCanonicalSoaSurface =
          normalizedName.rfind(
              templateMonomorphCompatibilitySoaHelperPrefix(false), 0) == 0 ||
          normalizedName.rfind(
              templateMonomorphPublicSoaHelperPrefix(false), 0) == 0 ||
          normalizedPrefix ==
              trimLeadingSlash(compatibilitySoaHelperTargetPath("")) ||
          normalizedPrefix == trimLeadingSlash(publicSoaHelperTargetPath(""));
      if (!usesCanonicalSoaSurface) {
        return std::string{};
      }
      if (collection_helpers::isGetHelperName(normalizedName) ||
          collection_helpers::isRefHelperName(normalizedName)) {
        return normalizedName;
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "get" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "get" ||
          normalizedName == "soa/get" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "get") {
        return std::string("get");
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + collection_helpers::kGetRef ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + collection_helpers::kGetRef ||
          normalizedName == "soa/get_ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + collection_helpers::kGetRef) {
        return std::string(collection_helpers::kGetRef);
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + "ref" ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + "ref" ||
          normalizedName == "soa/ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + "ref") {
        return std::string("ref");
      }
      if (normalizedName == templateMonomorphSamePathSoaHelperPrefix(false) + collection_helpers::kRefRef ||
          normalizedName == templateMonomorphCompatibilitySoaHelperPrefix(false) + collection_helpers::kRefRef ||
          normalizedName == "soa/ref_ref" ||
          normalizedName == templateMonomorphPublicSoaHelperPrefix(false) + collection_helpers::kRefRef) {
        return std::string(collection_helpers::kRefRef);
      }
      if ((normalizedPrefix == templateMonomorphSoaReceiverTypeName() ||
           normalizedPrefix == trimLeadingSlash(compatibilitySoaHelperTargetPath("")) ||
           normalizedPrefix == "soa" ||
           normalizedPrefix == trimLeadingSlash(publicSoaHelperTargetPath(""))) &&
          (collection_helpers::isGetHelperName(normalizedName) ||
           collection_helpers::isRefHelperName(normalizedName))) {
        return normalizedName;
      }
      return std::string{};
    }();
    if (!helperReturnSoaRefHelper.empty() &&
        ctx.sourceDefs.count(templateMonomorphSamePathSoaHelperPrefix() +
                             helperReturnSoaRefHelper) == 0 &&
        ctx.helperOverloads.count(templateMonomorphSamePathSoaHelperPrefix() +
                                  helperReturnSoaRefHelper) == 0 &&
        !resolvesSoaReceiverForRewrite(expr.args.front())) {
      if (expr.args.size() != 2) {
        error = "argument count mismatch for builtin " + helperReturnSoaRefHelper;
        return st.done(false);
      }
      const Expr &indexExpr = expr.args[1];
      if (indexExpr.kind == Expr::Kind::BoolLiteral ||
          indexExpr.kind == Expr::Kind::FloatLiteral ||
          indexExpr.kind == Expr::Kind::StringLiteral) {
        error = helperReturnSoaRefHelper + " requires integer index";
        return st.done(false);
      }
      const std::string unavailablePath =
          (collection_helpers::isGetHelperName(helperReturnSoaRefHelper))
              ? compatibilitySoaHelperTargetPath(helperReturnSoaRefHelper)
              : templateMonomorphSamePathSoaHelperPrefix() +
                    helperReturnSoaRefHelper;
      error = soaUnavailableMethodDiagnostic(unavailablePath);
      return st.done(false);
    }
  }
  const bool isPackAtIntrinsic =
      !expr.isMethodCall && !expr.isBinding && expr.name == "pack_at";
  const bool isTypeofSymbolIntrinsic =
      !expr.isMethodCall && !expr.isBinding && expr.name == "typeof";
  st.allConcrete = true;
  [[maybe_unused]] auto &allConcrete = st.allConcrete;
  if (!isPackAtIntrinsic && !isTypeofSymbolIntrinsic &&
      !resolveTemplateArgumentList(expr.templateArgs,
                                   mapping,
                                   allowedParams,
                                   namespacePrefix,
                                   ctx,
                                   error,
                                   allConcrete)) {
    return st.done(false);
  }
  if (isPackAtIntrinsic) {
    if (expr.templateArgs.size() != 2) {
      error = "pack_at requires integer index and field-stem template arguments";
      return st.done(false);
    }
    if (expr.args.size() != 1 || hasNamedCallArguments(expr) ||
        expr.hasBodyArguments || !expr.bodyArguments.empty()) {
      error = "pack_at requires exactly one receiver argument";
      return st.done(false);
    }
    uint64_t packIndex = 0;
    if (!resolveTemplateIndexText(expr.templateArgs[0], mapping, packIndex)) {
      error = "pack_at index must resolve to an integer template argument: " +
              expr.templateArgs[0];
      return st.done(false);
    }
    auto isPackAtFieldStem = [](const std::string &text) {
      if (text.empty() ||
          !(std::isalpha(static_cast<unsigned char>(text.front())) ||
            text.front() == '_')) {
        return false;
      }
      for (const char ch : text) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
          return false;
        }
      }
      return true;
    };
    std::string fieldStem = trimWhitespace(expr.templateArgs[1]);
    if (!isPackAtFieldStem(fieldStem)) {
      error = "pack_at field stem must be an identifier: " + fieldStem;
      return st.done(false);
    }
    if (ctx.currentRewriteDefinition != nullptr &&
        ctx.currentRewriteDefinition->templatePackBindings.size() == 1 &&
        packIndex >= ctx.currentRewriteDefinition->templatePackBindings.front().arguments.size()) {
      error = "pack_at index out of range: " + fieldStem + "[" +
              expr.templateArgs[0] + "] has " +
              std::to_string(
                  ctx.currentRewriteDefinition->templatePackBindings.front().arguments.size()) +
              " elements";
      return st.done(false);
    }
    if (!rewriteExpr(expr.args.front(),
                     mapping,
                     allowedParams,
                     namespacePrefix,
                     ctx,
                     error,
                     locals,
                     params,
                     allowMathBare)) {
      return st.done(false);
    }
    expr.name = generatedPackFieldName(fieldStem, static_cast<size_t>(packIndex));
    expr.sourceName = expr.name;
    expr.templateArgs.clear();
    expr.templateArgDetails.clear();
    expr.isMethodCall = true;
    expr.sourceIsMethodCall = true;
    expr.isFieldAccess = true;
    return st.done(true);
  }
  return PhaseStatus::Continue;
}

} // namespace primec
