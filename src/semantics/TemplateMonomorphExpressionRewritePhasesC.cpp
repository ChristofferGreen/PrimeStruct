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

PhaseStatus rewriteExprPhase6([[maybe_unused]] Expr &expr, [[maybe_unused]] const SubstMap &mapping, [[maybe_unused]] const std::unordered_set<std::string> &allowedParams, [[maybe_unused]] const std::string &namespacePrefix, [[maybe_unused]] Context &ctx, [[maybe_unused]] std::string &error, [[maybe_unused]] const LocalTypeMap &locals, [[maybe_unused]] const std::vector<ParameterInfo> &params, RewriteExprState &st) {
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
  if (!expr.isMethodCall && !expr.isBinding) {
    if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
      if (!isRootMapConstructorReceiverExpr(receiverExpr) &&
          !rewriteNestedExperimentalKeyValueConstructorValue(*receiverExpr)) {
        return st.done(false);
      }
      if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
        return st.done(false);
      }
      const auto callLeafIsPlausibleCollectionHelper = [](const Expr &callExpr) {
        std::string name = callExpr.name;
        const size_t lastSlash = name.find_last_of('/');
        const std::string leaf =
            lastSlash == std::string::npos ? name : name.substr(lastSlash + 1);
        const size_t genericSuffix = leaf.find("__t");
        const std::string leafBase =
            genericSuffix == std::string::npos ? leaf : leaf.substr(0, genericSuffix);
        // TODO-5235: built via systemHeapValue() so this magic static's
        // backing memory is never arena-allocated - see
        // docs/CompilerArenaAllocator.md.
        static const std::unordered_set<std::string> kCollectionHelperLeafNames =
            primec::systemHeapValue([] {
              return std::unordered_set<std::string>{
                  "at", "at_unsafe", "count", "capacity", "contains", "tryAt",
                  "insert", "push", "remove_at", "remove_swap", "get", "to_aos",
                  collection_helpers::kRefRef, "map", "vector"};
            });
        return kCollectionHelperLeafNames.count(leafBase) > 0;
      };
      // A plain scan (no rewriting) for a collection-constructor-shaped call
      // anywhere in the receiver's subtree - e.g. a user wrapper call whose
      // argument is a Result-ok-payload wrapping a `map(...)` constructor
      // needs the pre-rewrite even though neither the outer wrapper nor the
      // intermediate Result payload accessor is itself a collection helper
      // name; the `map(...)` a couple of levels down is what matters. This
      // is a read-only walk (no recursive rewrite call), so it's O(subtree
      // size) per node rather than the O(2^depth) this whole guard exists
      // to avoid.
      const std::function<bool(const Expr &)> subtreeContainsCollectionHelperCall =
          [&](const Expr &node) -> bool {
        if (node.kind == Expr::Kind::Call && callLeafIsPlausibleCollectionHelper(node)) {
          return true;
        }
        for (const Expr &arg : node.args) {
          if (subtreeContainsCollectionHelperCall(arg)) {
            return true;
          }
        }
        return false;
      };
      const bool outerCallIsPlausibleCollectionHelper =
          callLeafIsPlausibleCollectionHelper(expr) ||
          subtreeContainsCollectionHelperCall(*receiverExpr);
      if (outerCallIsPlausibleCollectionHelper) {
        if (!rewriteExpr(*receiverExpr,
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
      }
    }
    std::string resolvedPath =
        resolveCalleePath(expr, namespacePrefix, ctx, &locals, &params);
    if (!ctx.requirementOverloadSelectionError.empty()) {
      error = ctx.requirementOverloadSelectionError;
      ctx.requirementOverloadSelectionError.clear();
      return st.done(false);
    }
    const auto isKeyValueEntryConstructorArg = [](const Expr &argExpr) {
      if (argExpr.kind != Expr::Kind::Call || argExpr.isMethodCall ||
          argExpr.name.empty()) {
        return false;
      }
      std::string path = argExpr.name;
      if (path.front() != '/') {
        std::string prefix = argExpr.namespacePrefix;
        if (!prefix.empty() && prefix.front() != '/') {
          prefix.insert(prefix.begin(), '/');
        }
        path = prefix.empty() ? "/" + path : prefix + "/" + path;
      }
      return isTemplateMonomorphMapEntryConstructorPath(path);
    };
    const bool usesKeyValueEntryConstructorArgs =
        !expr.isMethodCall && !expr.args.empty() &&
        isTemplateMonomorphMapConstructorCallPath(resolvedPath) &&
        std::all_of(expr.args.begin(), expr.args.end(),
                    isKeyValueEntryConstructorArg);
    // Speculative early implicit-template-arg inference: the pair-to-entry
    // rewrite gate below only engages when `expr.templateArgs.size() == 2`
    // is already true, so an implicit-arg pair call (no explicit `<K, V>`)
    // never reaches the rewrite at all. Populate expr.templateArgs early
    // here, purely to unlock that gate; this is a no-op on failure and does
    // not surface any error from this call.
    if (!expr.isMethodCall && expr.templateArgs.empty() && !expr.args.empty() &&
        !usesKeyValueEntryConstructorArgs &&
        isTemplateMonomorphMapConstructorCallPath(resolvedPath)) {
      const auto speculativeDefIt = ctx.sourceDefs.find(resolvedPath);
      // Note: unlike the analogous call site below (which gates on
      // ctx.implicitTemplateDefs, a set of *auto-param-driven* templates
      // with no template-arg list at all), the map<K, V> entries
      // constructor has an explicit <K, V> template-arg list - it is the
      // *call site* that omits it, not the definition. Gate on it simply
      // being a template def instead.
      if (speculativeDefIt != ctx.sourceDefs.end() &&
          ctx.templateDefs.count(resolvedPath) > 0) {
        std::vector<std::string> speculativeInferredArgs;
        std::string speculativeError;
        if (inferImplicitTemplateArgs(speculativeDefIt->second,
                                      expr,
                                      locals,
                                      params,
                                      mapping,
                                      allowedParams,
                                      namespacePrefix,
                                      ctx,
                                      allowMathBare,
                                      speculativeInferredArgs,
                                      speculativeError)) {
          expr.templateArgs = std::move(speculativeInferredArgs);
        }
      }
    }
    if (!expr.isMethodCall &&
        expr.templateArgs.size() == 2 &&
        !expr.args.empty() &&
        !usesKeyValueEntryConstructorArgs &&
        isTemplateMonomorphMapConstructorCallPath(resolvedPath)) {
      if (expr.args.size() % 2 != 0) {
        error = "argument count mismatch for " + resolvedPath;
        return st.done(false);
      }
      const bool hasNamedPairArguments =
          std::any_of(expr.argNames.begin(), expr.argNames.end(),
                      [](const std::optional<std::string> &name) {
                        return name.has_value();
                      });
      if (hasNamedPairArguments) {
        static constexpr std::array<std::string_view, 8> PairOrdinals = {
            "first", "second", "third", "fourth",
            "fifth", "sixth", "seventh", "eighth"};
        auto pairSlotForName = [&](std::string_view name) -> size_t {
          for (size_t ordinal = 0; ordinal < PairOrdinals.size(); ++ordinal) {
            const std::string keyName = std::string(PairOrdinals[ordinal]) + "Key";
            const std::string valueName =
                std::string(PairOrdinals[ordinal]) + "Value";
            if (name == keyName) {
              return ordinal * 2;
            }
            if (name == valueName) {
              return ordinal * 2 + 1;
            }
          }
          return expr.args.size();
        };
        std::vector<const Expr *> slots(expr.args.size(), nullptr);
        std::vector<size_t> unnamedArgIndexes;
        for (size_t i = 0; i < expr.args.size(); ++i) {
          const bool hasName =
              i < expr.argNames.size() && expr.argNames[i].has_value();
          if (!hasName) {
            unnamedArgIndexes.push_back(i);
            continue;
          }
          const size_t slot = pairSlotForName(*expr.argNames[i]);
          if (slot >= slots.size()) {
            error = "unknown named argument: " + *expr.argNames[i];
            return st.done(false);
          }
          if (slots[slot] != nullptr) {
            error = "named argument duplicates parameter: " + *expr.argNames[i];
            return st.done(false);
          }
          slots[slot] = &expr.args[i];
        }
        size_t nextUnnamed = 0;
        for (size_t slot = 0; slot < slots.size(); ++slot) {
          if (slots[slot] != nullptr) {
            continue;
          }
          if (nextUnnamed >= unnamedArgIndexes.size()) {
            error = "argument count mismatch for " + resolvedPath;
            return st.done(false);
          }
          slots[slot] = &expr.args[unnamedArgIndexes[nextUnnamed++]];
        }
        std::vector<Expr> orderedArgs;
        orderedArgs.reserve(slots.size());
        for (const Expr *argExpr : slots) {
          orderedArgs.push_back(*argExpr);
        }
        expr.args = std::move(orderedArgs);
        expr.argNames.assign(expr.args.size(), std::nullopt);
      }
      const std::string diagnosticPath =
          keyValueConstructorSurfaceMetadataLocal() == nullptr
              ? resolvedPath
              : std::string(keyValueConstructorSurfaceMetadataLocal()->canonicalPath);
      auto mapConstructorParameterName = [](std::size_t argIndex) {
        static constexpr std::array<std::string_view, 8> Ordinals = {
            "first", "second", "third", "fourth",
            "fifth", "sixth", "seventh", "eighth"};
        const std::size_t pairIndex = argIndex / 2;
        const std::string ordinal =
            pairIndex < Ordinals.size()
                ? std::string(Ordinals[pairIndex])
                : "arg" + std::to_string(pairIndex + 1);
        return ordinal + (argIndex % 2 == 0 ? "Key" : "Value");
      };
      for (std::size_t argIndex = 0; argIndex < expr.args.size(); ++argIndex) {
        const std::optional<std::string> actualType =
            requirementOverloadArgumentTypeText(expr.args[argIndex], &locals, &params);
        if (!actualType.has_value()) {
          continue;
        }
        const std::string expectedType =
            normalizeBindingTypeName(expr.templateArgs[argIndex % 2]);
        const std::string normalizedActualType =
            normalizeBindingTypeName(*actualType);
        const ReturnKind expectedKind = returnKindForTypeName(expectedType);
        const ReturnKind actualKind = returnKindForTypeName(normalizedActualType);
        if (expectedKind == ReturnKind::Unknown ||
            actualKind == ReturnKind::Unknown ||
            expectedKind == actualKind) {
          continue;
        }
        error = "argument type mismatch for " + diagnosticPath +
                " parameter " + mapConstructorParameterName(argIndex) +
                ": expected " + expectedType + " got " + normalizedActualType;
        return st.done(false);
      }
      const StdlibSurfaceMetadata *keyValueMetadata =
          keyValueHelperSurfaceMetadataLocal();
      const auto hasVariadicEntriesOverload = [&]() {
        if (keyValueMetadata == nullptr || keyValueMetadata->canonicalPath.empty()) {
          return false;
        }
        const std::string constructorFamilyPath =
            std::string(keyValueMetadata->canonicalPath) + collection_helpers::kRootedMap;
        const auto familyIt = ctx.helperOverloads.find(constructorFamilyPath);
        if (familyIt == ctx.helperOverloads.end()) {
          return false;
        }
        return std::any_of(familyIt->second.begin(), familyIt->second.end(),
                           [](const auto &entry) {
                             return entry.isVariadic &&
                                    entry.parameterCount == 1;
                           });
      }();
      const bool hasEntryConstructorDefinition =
          keyValueMetadata != nullptr && !keyValueMetadata->canonicalPath.empty() &&
          (ctx.sourceDefs.count(std::string(keyValueMetadata->canonicalPath) +
                                "/entry") > 0 ||
           ctx.templateDefs.count(std::string(keyValueMetadata->canonicalPath) +
                                  "/entry") > 0);
      const bool hasMatchingPairArityOverload = [&]() {
        if (keyValueMetadata == nullptr || keyValueMetadata->canonicalPath.empty()) {
          return false;
        }
        const std::string constructorFamilyPath =
            std::string(keyValueMetadata->canonicalPath) + collection_helpers::kRootedMap;
        const auto familyIt = ctx.helperOverloads.find(constructorFamilyPath);
        if (familyIt == ctx.helperOverloads.end()) {
          return false;
        }
        return std::any_of(familyIt->second.begin(), familyIt->second.end(),
                           [&](const auto &entry) {
                             return !entry.isVariadic &&
                                    entry.parameterCount == expr.args.size();
                           });
      }();
      if (keyValueMetadata != nullptr && !keyValueMetadata->canonicalPath.empty() &&
          hasVariadicEntriesOverload && hasEntryConstructorDefinition &&
          !hasMatchingPairArityOverload) {
        const std::string entryConstructorPath =
            std::string(keyValueMetadata->canonicalPath) + "/entry";
        std::vector<Expr> entryPack;
        entryPack.reserve(expr.args.size() / 2);
        for (size_t argIndex = 0; argIndex + 1 < expr.args.size(); argIndex += 2) {
          Expr entryCall;
          entryCall.kind = Expr::Kind::Call;
          entryCall.name = entryConstructorPath;
          entryCall.templateArgs = expr.templateArgs;
          entryCall.templateArgDetails = expr.templateArgDetails;
          entryCall.sourceLine = expr.args[argIndex].sourceLine;
          entryCall.sourceColumn = expr.args[argIndex].sourceColumn;
          entryCall.args.push_back(std::move(expr.args[argIndex]));
          entryCall.args.push_back(std::move(expr.args[argIndex + 1]));
          entryCall.argNames.assign(2, std::nullopt);
          entryPack.push_back(std::move(entryCall));
        }
        expr.args = std::move(entryPack);
        expr.argNames.assign(expr.args.size(), std::nullopt);
        resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx, &locals, &params);
        if (!ctx.requirementOverloadSelectionError.empty()) {
          error = ctx.requirementOverloadSelectionError;
          ctx.requirementOverloadSelectionError.clear();
          return st.done(false);
        }
      }
    }
    if (!expr.isMethodCall) {
      unwrapDereferencedBorrowedVectorReceiver(expr, params, locals, allowMathBare, ctx);
    }
    if (rewriteBorrowedVectorBareHelperCall(expr, params, locals, allowMathBare, ctx)) {
      allConcrete = true;
      resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx, &locals, &params);
    }
    const std::string preferredCollectionHelperPath =
        preferCanonicalStdlibCollectionHelperPath(resolvedPath);
    if (!error.empty()) {
      return st.done(false);
    }
    if (preferredCollectionHelperPath != resolvedPath) {
      resolvedPath = preferredCollectionHelperPath;
      expr.name = preferredCollectionHelperPath;
      expr.namespacePrefix.clear();
    }
    if (!expr.templateArgs.empty() &&
        expr.name.find('/') == std::string::npos &&
        !expr.args.empty()) {
      BindingInfo receiverInfo;
      if (inferBindingTypeForMonomorph(expr.args.front(),
                                       params,
                                       locals,
                                       allowMathBare,
                                       ctx,
                                       receiverInfo)) {
        const std::string wrapperBase =
            normalizeCollectionReceiverTypeName(receiverInfo.typeName);
        if ((wrapperBase == "Reference" || wrapperBase == "Pointer") &&
            !receiverInfo.typeTemplateArg.empty()) {
          const std::string rootedWrapperMethodPath =
              "/" + wrapperBase + "/" + expr.name;
          if (ctx.templateDefs.count(rootedWrapperMethodPath) > 0 &&
              ctx.sourceDefs.count(rootedWrapperMethodPath) > 0) {
            resolvedPath = rootedWrapperMethodPath;
            expr.name = rootedWrapperMethodPath;
            expr.namespacePrefix.clear();
          }
        }
      }
    }
    if (!expr.isMethodCall && expr.templateArgs.empty() &&
        expr.name.find('/') == std::string::npos &&
        (resolvedPath == "/count" || resolvedPath == "/capacity")) {
      const std::string samePathVectorHelper =
          "/" + std::string("vector") + "/" + expr.name;
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
    if (!expr.isMethodCall &&
        expr.name.find('/') == std::string::npos &&
        isLegacyOrCanonicalSoaHelperPath(resolvedPath, "count") &&
        !isTemplateMonomorphSoaReceiverType(
            inferCollectionReceiverFamily(collectionHelperReceiverExpr(expr)))) {
      return st.done(true);
    }
    const std::string preferredBorrowedSoaPath =
        preferredBorrowedSoaWrapperPath(resolvedPath);
    if (!preferredBorrowedSoaPath.empty() &&
        resolvesBorrowedExperimentalSoaVectorReceiver(
            collectionHelperReceiverExpr(expr)) &&
        (ctx.sourceDefs.count(preferredBorrowedSoaPath) > 0 ||
         ctx.helperOverloads.count(preferredBorrowedSoaPath) > 0 ||
         ctx.templateDefs.count(preferredBorrowedSoaPath) > 0)) {
      resolvedPath = preferredBorrowedSoaPath;
      expr.name = preferredBorrowedSoaPath;
      expr.namespacePrefix.clear();
    }
    const bool resolvesBorrowedExperimentalKeyValueReceiver =
        resolvesExperimentalKeyValueBorrowedReceiver(
            collectionHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx);
    const std::string borrowedCanonicalKeyValueUnknownTarget =
        canonicalKeyValueHelperUnknownTargetPath(resolvedPath);
    if (!borrowedCanonicalKeyValueUnknownTarget.empty() &&
        resolvesBorrowedExperimentalKeyValueReceiver) {
      error = "unknown call target: " + borrowedCanonicalKeyValueUnknownTarget;
      return st.done(false);
    }
    auto stripGeneratedSuffix = [](std::string path) {
      const size_t suffix = path.find("__");
      if (suffix != std::string::npos) {
        path.erase(suffix);
      }
      return path;
    };
    const std::string removedKeyValueCompatibilityPath =
        stripGeneratedSuffix(resolvedPath);
    auto removedKeyValueCompatibilityHelperFromPath =
        [](std::string_view path) -> std::string {
      const primec::StdlibSurfaceMetadata *metadata =
          keyValueHelperSurfaceMetadataLocal();
      if (metadata == nullptr) {
        return {};
      }
      std::string helperName;
      for (const std::string_view alias : metadata->importAliasSpellings) {
        if (alias.find('/') != std::string_view::npos) {
          continue;
        }
        if (stripStdlibSurfaceRootedMemberName(path, alias, helperName)) {
          return helperName;
        }
      }
      return {};
    };
    const std::string removedKeyValueCompatibilityHelper =
        removedKeyValueCompatibilityHelperFromPath(
            removedKeyValueCompatibilityPath);
    const bool isRemovedKeyValueCompatibilityPath =
        !removedKeyValueCompatibilityHelper.empty();
    const std::string_view removedKeyValueCompatibilityHelperBase =
        keyValueCompatibilityHelperBase(removedKeyValueCompatibilityHelper);
    if (isRemovedKeyValueCompatibilityPath &&
        isRemovedKeyValueCompatibilityHelper(removedKeyValueCompatibilityHelperBase) &&
        (collection_helpers::isCountHelperName(removedKeyValueCompatibilityHelperBase) ||
         removedKeyValueCompatibilityHelperBase == "size") &&
        ctx.sourceDefs.count(removedKeyValueCompatibilityPath) == 0 &&
        ctx.templateDefs.count(removedKeyValueCompatibilityPath) == 0 &&
        ctx.helperOverloads.count(removedKeyValueCompatibilityPath) == 0) {
      const std::string helperName(removedKeyValueCompatibilityHelperBase);
      if (expr.hasBodyArguments || !expr.bodyArguments.empty()) {
        error = "block arguments require a definition target: " +
                removedKeyValueCompatibilityPath;
        return st.done(false);
      }
      const size_t expectedArgCount =
          (collection_helpers::isCountHelperName(helperName) ||
           helperName == "size")
              ? 1
              : ((collection_helpers::isAtHelperName(helperName) ||
                  collection_helpers::isAtUnsafeHelperName(helperName) ||
                  collection_helpers::isContainsHelperName(helperName) ||
                  collection_helpers::isTryAtHelperName(helperName))
                     ? 2
                     : 3);
      if (expr.args.size() != expectedArgCount) {
        error =
            "argument count mismatch for " + removedKeyValueCompatibilityPath;
        return st.done(false);
      }
      error = "unknown call target: " + removedKeyValueCompatibilityPath;
      return st.done(false);
    }
    if (isRemovedKeyValueCompatibilityPath &&
        removedKeyValueCompatibilityPath != resolvedPath &&
        ctx.templateDefs.count(removedKeyValueCompatibilityPath) > 0) {
      resolvedPath = removedKeyValueCompatibilityPath;
      expr.name = removedKeyValueCompatibilityPath;
      expr.namespacePrefix.clear();
    }
    const std::string experimentalKeyValuePath =
        experimentalKeyValueHelperPathForCanonicalHelper(resolvedPath);
    const Expr *experimentalKeyValueReceiverExpr = collectionHelperReceiverExpr(expr);
    const bool receiverIsPublishedKeyValueConstructor =
        isPublishedMapConstructorReceiverExpr(experimentalKeyValueReceiverExpr,
                                              namespacePrefix,
                                              ctx);
    const bool rejectsWrapperReturnedKeyValueAccess =
        experimentalKeyValueReceiverExpr != nullptr &&
        experimentalKeyValueReceiverExpr->kind == Expr::Kind::Call &&
        !experimentalKeyValueReceiverExpr->isFieldAccess &&
        !receiverIsPublishedKeyValueConstructor &&
        isTemplateMonomorphCanonicalKeyValueAccessPath(
            borrowedCanonicalKeyValueUnknownTarget);
    if (!experimentalKeyValuePath.empty() && ctx.sourceDefs.count(experimentalKeyValuePath) > 0 &&
        resolvesExperimentalKeyValueReceiver(
            experimentalKeyValueReceiverExpr, params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      if (rejectsWrapperReturnedKeyValueAccess) {
        error = "unknown call target: " + borrowedCanonicalKeyValueUnknownTarget;
        return st.done(false);
      }
      resolvedPath = experimentalKeyValuePath;
      expr.name = experimentalKeyValuePath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalKeyValueReceiverTemplateArgs(
                experimentalKeyValueReceiverExpr, params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalKeyValueConstructorValue(*receiverExpr)) {
          return st.done(false);
        }
      }
    }
    const std::string experimentalWrapperKeyValuePath =
        experimentalKeyValueHelperPathForWrapperHelper(resolvedPath);
    if (!experimentalWrapperKeyValuePath.empty() &&
        ctx.sourceDefs.count(experimentalWrapperKeyValuePath) > 0 &&
        resolvesExperimentalKeyValueReceiver(
            collectionHelperReceiverExpr(expr), params, locals, allowMathBare, mapping, allowedParams, namespacePrefix, ctx)) {
      resolvedPath = experimentalWrapperKeyValuePath;
      expr.name = experimentalWrapperKeyValuePath;
      expr.namespacePrefix.clear();
      if (expr.templateArgs.empty()) {
        std::vector<std::string> receiverTemplateArgs;
        if (resolveExperimentalKeyValueReceiverTemplateArgs(
                collectionHelperReceiverExpr(expr), params, locals, allowMathBare, namespacePrefix, ctx, receiverTemplateArgs)) {
          expr.templateArgs = std::move(receiverTemplateArgs);
        }
      }
      if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
        if (!rewriteNestedExperimentalKeyValueConstructorValue(*receiverExpr)) {
          return st.done(false);
        }
      }
    }
    const Expr *experimentalVectorReceiverExpr = collectionHelperReceiverExpr(expr);
    const bool canRewriteNamedExperimentalVectorTemporary =
        experimentalVectorReceiverExpr == nullptr ||
        experimentalVectorReceiverExpr->kind != Expr::Kind::Call ||
        !hasNamedArguments(experimentalVectorReceiverExpr->argNames);
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
          return st.done(false);
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
    bool inferredCanonicalKeyValueReceiverTemplateArgs = false;
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
        resolvedPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
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
    if (resolvedPath.rfind(legacyExperimentalVectorCompatibilityPrefix(), 0) == 0 &&
        resolvedPath.find("__t") != std::string::npos) {
      expr.templateArgs.clear();
    }
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
    const bool isKnownDef = ctx.sourceDefs.count(resolvedPath) > 0;
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
      return st.done(false);
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
    auto preferredConcreteSamePathSoaHelperPath = [&](const std::string &path) {
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
      if (path == publicSoaHelperTargetPath("push") ||
          path == publicSoaHelperTargetPath("reserve")) {
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
          canonicalVectorCompatibilityHelperPathOrFallback(helperName);
      if (ctx.sourceDefs.count(vectorPath) > 0 ||
          (isPublishedVectorMutatorHelperName(helperName) &&
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
          publicSoaHelperTargetPath(
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
            return st.done(false);
          }
        }
        }
        if (expr.templateArgs.empty()) {
          if (defIt != ctx.sourceDefs.end() &&
              definitionAllowsEmptyTypePackSpecialization(defIt->second)) {
            allConcrete = true;
          } else if (shouldDeferStdlibCollectionHelperTemplateRewrite(resolvedPath)) {
            return st.done(true);
          } else {
            error = "template arguments required for " + helperOverloadDisplayPath(resolvedPath, ctx);
            return st.done(false);
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
          return st.done(false);
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
          canonicalizeLegacySoaGetHelperPath(resolvedPath);
      const std::string resolvedRefPath =
          canonicalizeLegacySoaRefHelperPath(resolvedPath);
      std::string soaAccessHelper;
      if (isLegacyOrCanonicalSoaHelperPath(resolvedGetPath, "get")) {
        soaAccessHelper = "get";
      } else if (isLegacyOrCanonicalSoaHelperPath(resolvedGetPath, collection_helpers::kGetRef)) {
        soaAccessHelper = collection_helpers::kGetRef;
      } else if (isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, "ref")) {
        soaAccessHelper = "ref";
      } else if (isLegacyOrCanonicalSoaHelperPath(resolvedRefPath, collection_helpers::kRefRef)) {
        soaAccessHelper = collection_helpers::kRefRef;
      }
      if (!soaAccessHelper.empty()) {
        if (expr.args.size() != 2) {
          error = "argument count mismatch for builtin " + soaAccessHelper;
          return st.done(false);
        }
        const Expr &indexExpr = expr.args[1];
        if (indexExpr.kind == Expr::Kind::BoolLiteral ||
            indexExpr.kind == Expr::Kind::FloatLiteral ||
            indexExpr.kind == Expr::Kind::StringLiteral) {
          error = soaAccessHelper + " requires integer index";
          return st.done(false);
        }
      }
      if (soaAccessHelper.empty() &&
          (isExperimentalSoaGetLikeHelperPath(resolvedPath) ||
           isExperimentalSoaRefLikeHelperPath(resolvedPath))) {
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
            return st.done(false);
          }
          const Expr &indexExpr = expr.args[1];
          if (indexExpr.kind == Expr::Kind::BoolLiteral ||
              indexExpr.kind == Expr::Kind::FloatLiteral ||
              indexExpr.kind == Expr::Kind::StringLiteral) {
            error = soaAccessHelper + " requires integer index";
            return st.done(false);
          }
        }
      }
      error = "template arguments are only supported on templated definitions: " +
              helperOverloadDisplayPath(resolvedPath, ctx);
      return st.done(false);
      }
    }
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
        return st.done(false);
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
        return st.done(false);
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
  }
  return PhaseStatus::Continue;
}

} // namespace primec
