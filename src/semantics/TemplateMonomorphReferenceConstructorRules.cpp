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
#include "SemanticsValidateLambdaClosures.h"
#include "TemplateMonomorphExpressionReferenceRules.h"

namespace primec {

// The rules of a plain call expression that rewrite collection and key-value constructors: the receiver
// pre-rewrite, key/value pair and entry constructors, and the arguments of constructors of known targets. They run
// in the order of rewriteExprReference (TemplateMonomorphReferenceHelperRules.cpp).

PhaseStatus prerewriteReceiverConstructors(ExprReferenceContext &rc) {
  RewriteExprState &st = rc.st;
  Expr &expr = rc.expr;
  const SubstMap &mapping = rc.mapping;
  const std::unordered_set<std::string> &allowedParams = rc.allowedParams;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &resolvedPath = rc.resolvedPath;
  auto &usesKeyValueEntryConstructorArgs = rc.usesKeyValueEntryConstructorArgs;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &mutableCollectionHelperReceiverExpr = rc.st.mutableCollectionHelperReceiverExpr;
  auto &rewriteNestedExperimentalKeyValueConstructorValue =
      rc.st.rewriteNestedExperimentalKeyValueConstructorValue;
  auto &rewriteNestedExperimentalVectorConstructorValue =
      rc.st.rewriteNestedExperimentalVectorConstructorValue;
  if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
    if (!isRootMapConstructorReceiverExpr(receiverExpr) &&
        !rewriteNestedExperimentalKeyValueConstructorValue(*receiverExpr)) {
      return rc.done(st.done(false));
    }
    if (!rewriteNestedExperimentalVectorConstructorValue(*receiverExpr)) {
      return rc.done(st.done(false));
    }
    const auto callLeafIsPlausibleCollectionHelper = [](const Expr &callExpr) {
      std::string name = callExpr.name;
      const size_t lastSlash = name.find_last_of('/');
      const std::string leaf = lastSlash == std::string::npos ? name : name.substr(lastSlash + 1);
      const size_t genericSuffix = leaf.find("__t");
      const std::string leafBase =
          genericSuffix == std::string::npos ? leaf : leaf.substr(0, genericSuffix);
      // TODO-5235: built via systemHeapValue() so this magic static's
      // backing memory is never arena-allocated - see
      // docs/CompilerArenaAllocator.md.
      static const std::unordered_set<std::string> kCollectionHelperLeafNames =
          primec::systemHeapValue([] {
            return std::unordered_set<std::string>{"at",
                                                   "at_unsafe",
                                                   "count",
                                                   "capacity",
                                                   "contains",
                                                   "tryAt",
                                                   "insert",
                                                   "push",
                                                   "remove_at",
                                                   "remove_swap",
                                                   "get",
                                                   "to_aos",
                                                   collection_helpers::kRefRef,
                                                   "map",
                                                   "vector"};
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
        return rc.done(st.done(false));
      }
    }
  }
  rc.resolvedPath = resolveCalleePath(expr, namespacePrefix, ctx, &locals, &params);
  if (!ctx.requirementOverloadSelectionError.empty()) {
    error = ctx.requirementOverloadSelectionError;
    ctx.requirementOverloadSelectionError.clear();
    return rc.done(st.done(false));
  }
  const auto isKeyValueEntryConstructorArg = [](const Expr &argExpr) {
    if (argExpr.kind != Expr::Kind::Call || argExpr.isMethodCall || argExpr.name.empty()) {
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
  rc.usesKeyValueEntryConstructorArgs =
      !expr.isMethodCall && !expr.args.empty() &&
      isTemplateMonomorphMapConstructorCallPath(resolvedPath) &&
      std::all_of(expr.args.begin(), expr.args.end(), isKeyValueEntryConstructorArg);
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
    if (speculativeDefIt != ctx.sourceDefs.end() && ctx.templateDefs.count(resolvedPath) > 0) {
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
  return PhaseStatus::Continue;
}

PhaseStatus rewriteKeyValuePairConstructor(ExprReferenceContext &rc) {
  RewriteExprState &st = rc.st;
  Expr &expr = rc.expr;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  std::string &error = rc.error;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &resolvedPath = rc.resolvedPath;
  auto &usesKeyValueEntryConstructorArgs = rc.usesKeyValueEntryConstructorArgs;
  if (!expr.isMethodCall && expr.templateArgs.size() == 2 && !expr.args.empty() &&
      !usesKeyValueEntryConstructorArgs &&
      isTemplateMonomorphMapConstructorCallPath(resolvedPath)) {
    if (expr.args.size() % 2 != 0) {
      error = "argument count mismatch for " + resolvedPath;
      return rc.done(st.done(false));
    }
    const bool hasNamedPairArguments =
        std::any_of(expr.argNames.begin(),
                    expr.argNames.end(),
                    [](const std::optional<std::string> &name) { return name.has_value(); });
    if (hasNamedPairArguments) {
      static constexpr std::array<std::string_view, 8> PairOrdinals = {
          "first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth"};
      auto pairSlotForName = [&](std::string_view name) -> size_t {
        for (size_t ordinal = 0; ordinal < PairOrdinals.size(); ++ordinal) {
          const std::string keyName = std::string(PairOrdinals[ordinal]) + "Key";
          const std::string valueName = std::string(PairOrdinals[ordinal]) + "Value";
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
        const bool hasName = i < expr.argNames.size() && expr.argNames[i].has_value();
        if (!hasName) {
          unnamedArgIndexes.push_back(i);
          continue;
        }
        const size_t slot = pairSlotForName(*expr.argNames[i]);
        if (slot >= slots.size()) {
          error = "unknown named argument: " + *expr.argNames[i];
          return rc.done(st.done(false));
        }
        if (slots[slot] != nullptr) {
          error = "named argument duplicates parameter: " + *expr.argNames[i];
          return rc.done(st.done(false));
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
          return rc.done(st.done(false));
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
          "first", "second", "third", "fourth", "fifth", "sixth", "seventh", "eighth"};
      const std::size_t pairIndex = argIndex / 2;
      const std::string ordinal = pairIndex < Ordinals.size()
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
          semantics::normalizeBindingTypeName(expr.templateArgs[argIndex % 2]);
      const std::string normalizedActualType = semantics::normalizeBindingTypeName(*actualType);
      const semantics::ReturnKind expectedKind = semantics::returnKindForTypeName(expectedType);
      const semantics::ReturnKind actualKind =
          semantics::returnKindForTypeName(normalizedActualType);
      if (expectedKind == semantics::ReturnKind::Unknown ||
          actualKind == semantics::ReturnKind::Unknown || expectedKind == actualKind) {
        continue;
      }
      error = "argument type mismatch for " + diagnosticPath + " parameter " +
              mapConstructorParameterName(argIndex) + ": expected " + expectedType + " got " +
              normalizedActualType;
      return rc.done(st.done(false));
    }
    const StdlibSurfaceMetadata *keyValueMetadata = keyValueHelperSurfaceMetadataLocal();
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
      return std::any_of(familyIt->second.begin(), familyIt->second.end(), [](const auto &entry) {
        return entry.isVariadic && entry.parameterCount == 1;
      });
    }();
    const bool hasEntryConstructorDefinition =
        keyValueMetadata != nullptr && !keyValueMetadata->canonicalPath.empty() &&
        (ctx.sourceDefs.count(std::string(keyValueMetadata->canonicalPath) + "/entry") > 0 ||
         ctx.templateDefs.count(std::string(keyValueMetadata->canonicalPath) + "/entry") > 0);
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
      return std::any_of(familyIt->second.begin(), familyIt->second.end(), [&](const auto &entry) {
        return !entry.isVariadic && entry.parameterCount == expr.args.size();
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
        return rc.done(st.done(false));
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteKeyValueEntryConstructor(ExprReferenceContext &rc) {
  RewriteExprState &st = rc.st;
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
  auto &removedKeyValueCompatibilityPath = rc.removedKeyValueCompatibilityPath;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &collectionHelperReceiverExpr = rc.st.collectionHelperReceiverExpr;
  auto &mutableCollectionHelperReceiverExpr = rc.st.mutableCollectionHelperReceiverExpr;
  auto &rewriteNestedExperimentalKeyValueConstructorValue =
      rc.st.rewriteNestedExperimentalKeyValueConstructorValue;
  auto removedKeyValueCompatibilityHelperFromPath = [](std::string_view path) -> std::string {
    const primec::StdlibSurfaceMetadata *metadata = keyValueHelperSurfaceMetadataLocal();
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
      removedKeyValueCompatibilityHelperFromPath(removedKeyValueCompatibilityPath);
  const bool isRemovedKeyValueCompatibilityPath = !removedKeyValueCompatibilityHelper.empty();
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
      error = "block arguments require a definition target: " + removedKeyValueCompatibilityPath;
      return rc.done(st.done(false));
    }
    const size_t expectedArgCount =
        (collection_helpers::isCountHelperName(helperName) || helperName == "size")
            ? 1
            : ((collection_helpers::isAtHelperName(helperName) ||
                collection_helpers::isAtUnsafeHelperName(helperName) ||
                collection_helpers::isContainsHelperName(helperName) ||
                collection_helpers::isTryAtHelperName(helperName))
                   ? 2
                   : 3);
    if (expr.args.size() != expectedArgCount) {
      error = "argument count mismatch for " + removedKeyValueCompatibilityPath;
      return rc.done(st.done(false));
    }
    error = "unknown call target: " + removedKeyValueCompatibilityPath;
    return rc.done(st.done(false));
  }
  if (isRemovedKeyValueCompatibilityPath && removedKeyValueCompatibilityPath != resolvedPath &&
      ctx.templateDefs.count(removedKeyValueCompatibilityPath) > 0) {
    resolvedPath = removedKeyValueCompatibilityPath;
    expr.name = removedKeyValueCompatibilityPath;
    expr.namespacePrefix.clear();
  }
  const std::string experimentalKeyValuePath =
      experimentalKeyValueHelperPathForCanonicalHelper(resolvedPath);
  const Expr *experimentalKeyValueReceiverExpr = collectionHelperReceiverExpr(expr);
  const bool receiverIsPublishedKeyValueConstructor =
      isPublishedMapConstructorReceiverExpr(experimentalKeyValueReceiverExpr, namespacePrefix, ctx);
  const bool rejectsWrapperReturnedKeyValueAccess =
      experimentalKeyValueReceiverExpr != nullptr &&
      experimentalKeyValueReceiverExpr->kind == Expr::Kind::Call &&
      !experimentalKeyValueReceiverExpr->isFieldAccess && !receiverIsPublishedKeyValueConstructor &&
      isTemplateMonomorphCanonicalKeyValueAccessPath(borrowedCanonicalKeyValueUnknownTarget);
  if (!experimentalKeyValuePath.empty() && ctx.sourceDefs.count(experimentalKeyValuePath) > 0 &&
      resolvesExperimentalKeyValueReceiver(experimentalKeyValueReceiverExpr,
                                           params,
                                           locals,
                                           allowMathBare,
                                           mapping,
                                           allowedParams,
                                           namespacePrefix,
                                           ctx)) {
    if (rejectsWrapperReturnedKeyValueAccess) {
      error = "unknown call target: " + borrowedCanonicalKeyValueUnknownTarget;
      return rc.done(st.done(false));
    }
    resolvedPath = experimentalKeyValuePath;
    expr.name = experimentalKeyValuePath;
    expr.namespacePrefix.clear();
    if (expr.templateArgs.empty()) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalKeyValueReceiverTemplateArgs(experimentalKeyValueReceiverExpr,
                                                          params,
                                                          locals,
                                                          allowMathBare,
                                                          namespacePrefix,
                                                          ctx,
                                                          receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
      if (!rewriteNestedExperimentalKeyValueConstructorValue(*receiverExpr)) {
        return rc.done(st.done(false));
      }
    }
  }
  const std::string experimentalWrapperKeyValuePath =
      experimentalKeyValueHelperPathForWrapperHelper(resolvedPath);
  if (!experimentalWrapperKeyValuePath.empty() &&
      ctx.sourceDefs.count(experimentalWrapperKeyValuePath) > 0 &&
      resolvesExperimentalKeyValueReceiver(collectionHelperReceiverExpr(expr),
                                           params,
                                           locals,
                                           allowMathBare,
                                           mapping,
                                           allowedParams,
                                           namespacePrefix,
                                           ctx)) {
    resolvedPath = experimentalWrapperKeyValuePath;
    expr.name = experimentalWrapperKeyValuePath;
    expr.namespacePrefix.clear();
    if (expr.templateArgs.empty()) {
      std::vector<std::string> receiverTemplateArgs;
      if (resolveExperimentalKeyValueReceiverTemplateArgs(collectionHelperReceiverExpr(expr),
                                                          params,
                                                          locals,
                                                          allowMathBare,
                                                          namespacePrefix,
                                                          ctx,
                                                          receiverTemplateArgs)) {
        expr.templateArgs = std::move(receiverTemplateArgs);
      }
    }
    if (Expr *receiverExpr = mutableCollectionHelperReceiverExpr(expr)) {
      if (!rewriteNestedExperimentalKeyValueConstructorValue(*receiverExpr)) {
        return rc.done(st.done(false));
      }
    }
  }
  return PhaseStatus::Continue;
}

PhaseStatus rewriteConstructorArgsForTarget(ExprReferenceContext &rc) {
  RewriteExprState &st = rc.st;
  Expr &expr = rc.expr;
  const std::string &namespacePrefix = rc.namespacePrefix;
  Context &ctx = rc.ctx;
  const LocalTypeMap &locals = rc.locals;
  const std::vector<semantics::ParameterInfo> &params = rc.params;
  auto &allowMathBare = rc.st.allowMathBare;
  auto &rewriteKeyValueTargetValueForResolvedType = rc.st.rewriteKeyValueTargetValueForResolvedType;
  auto &rewriteVectorTargetValueForResolvedType = rc.st.rewriteVectorTargetValueForResolvedType;
  const std::string finalDirectCallResolvedPath = resolveCalleePath(expr, namespacePrefix, ctx);
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
      return rc.done(st.done(false));
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
      return rc.done(st.done(false));
    }
  }
  rewriteExperimentalAssignTargetValue(expr,
                                       params,
                                       locals,
                                       allowMathBare,
                                       namespacePrefix,
                                       ctx,
                                       [&](const std::string &targetTypeText, Expr &valueExpr) {
                                         return rewriteVectorTargetValueForResolvedType(
                                             targetTypeText, valueExpr);
                                       });
  rewriteExperimentalInitTargetValue(expr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     namespacePrefix,
                                     ctx,
                                     [&](const std::string &targetTypeText, Expr &valueExpr) {
                                       return rewriteVectorTargetValueForResolvedType(
                                           targetTypeText, valueExpr);
                                     });
  rewriteExperimentalAssignTargetValue(expr,
                                       params,
                                       locals,
                                       allowMathBare,
                                       namespacePrefix,
                                       ctx,
                                       [&](const std::string &targetTypeText, Expr &valueExpr) {
                                         return rewriteKeyValueTargetValueForResolvedType(
                                             targetTypeText, valueExpr);
                                       });
  rewriteExperimentalInitTargetValue(expr,
                                     params,
                                     locals,
                                     allowMathBare,
                                     namespacePrefix,
                                     ctx,
                                     [&](const std::string &targetTypeText, Expr &valueExpr) {
                                       return rewriteKeyValueTargetValueForResolvedType(
                                           targetTypeText, valueExpr);
                                     });
  return PhaseStatus::Continue;
}

} // namespace primec
