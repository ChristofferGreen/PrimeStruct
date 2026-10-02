#pragma once

// Helpers shared by the TemplateMonomorphMethodTargets*.cpp units (split out of
// TemplateMonomorphMethodTargets.cpp without changes, TODO-5384).
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
#include "primec/support/CanonicalReceiverType.h"
#include "primec/support/CollectionSpellingClassifier.h"
#include "primec/support/ReceiverElementFamilyClassifier.h"
#include "primec/support/StdlibSurfaceRegistry.h"
#include <cassert>
#include <iostream>
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


namespace methodTargetHelpers {

// Step 1c (docs/ReceiverTargetResolutionConsolidation.md): resolveReceiverType
// for monomorphization's F3, the receiver-type-inference cascade that lives
// inline inside resolveMethodCallTemplateTarget below (see the Step 0 Rule
// Table's "F3 detail" sub-table, and this document's "Ready to implement"
// checklist, which named this stage/function as the second
// resolveReceiverType producer after ir_lowerer's RT2/RT3/G7).
//
// As of the "F3 Name/literal real migration" round, this is the SOLE
// production path for F3-N1/N2 (Name-kind receiver) and F3-L/B/Fl/S
// (primitive-literal-kind receivers) - resolveMethodCallTemplateTarget below
// calls this directly for those receiver kinds instead of running its own
// inline cascade. F3-C1/C2/C3 (Call-kind receivers) are deliberately NOT
// covered by this particular function - as of the "F3 Call-kind real
// migration" round they are fully migrated too, just onto the sibling
// producer `resolveReceiverTypeFromCallExprForTemplateMonomorph` below
// rather than merged into this one, for a stage-specific reason: they call
// inferBindingTypeForMonomorph (transitively
// inferImplicitTemplateArgs), inferExprTypeTextForTemplatedVectorFallback,
// and inferDefinitionReturnBindingForTemplatedFallback, all of which take a
// non-const Context& and mutate ctx-scoped, test-visible counters as a side
// effect of merely being called - specifically
// Context::implicitTemplateArgInferenceFactHitsForTesting and
// implicitTemplateArgFactsForTesting (TemplateMonomorphContext.h,
// incremented/appended inside TemplateMonomorphImplicitTemplateInference.cpp's
// inferImplicitTemplateArgs whenever a cached implicit-template-arg fact is
// hit, independent of the collectImplicitTemplateArgFactsForTesting gate),
// read back only by TemplateMonomorph.cpp for test-facing hit-count/fact
// reporting. RT2/RT3b/RT3c's own underlying inference (LocalInfo/Expr-kind
// based) is provably side-effect-free, which is what made migrating those
// kinds safe via the usual harness-then-migrate discipline; F3's Call-kind
// path is not side-effect-free in the same way, so a second, purely-
// observational invocation (the harness round's pre-migration step) would
// have corrupted those counters for any test that asserts on them had it
// been wired into production - which is exactly why that harness round
// snapshot/restored around its own audit-only second invocation rather than
// letting it run free. Now that the harness round proved zero-divergence
// and safe counter-restoration, Call-kind receivers are migrated for real
// via the single production invocation inside
// resolveReceiverTypeFromCallExprForTemplateMonomorph below (invoked
// exactly once per receiver, same as the old inline cascade it replaced -
// no second/audit invocation remains). They are still not folded into
// *this* function's own branches, since their underlying inference is
// fundamentally different in kind (delegated production helpers, not
// inline algorithm) from the independent reimplementations above.
// F3-C3a (the receiver-is-a-struct-constructor-call short-circuit) is
// separately and permanently out of scope regardless, per the earlier
// "Ready to implement" round's irreconcilable-case finding - it answers a
// resolution question ("what method-definition path"), not an inference one
// ("what type"), and was never returned by this cascade in the first place.
// TODO-4753: a bare collection-family base name ("vector"/"map") must never
// go through import-alias substitution when qualifying a receiver's OWN type
// text for method-target path construction. The module path (the stdlib
// collections vector module itself) and the constructor-family path (that
// same module path with one more "vector" leaf segment appended, since the
// vector<T>(...) constructor overload family's own leaf name happens to
// collide with its module's name) are both legitimately registered under
// the alias key "vector" - `stdlibSurfaceImportAliasPriority` deliberately
// ranks the constructor family above the helper/module one as the intended
// winner for that shared key in most contexts (confirmed load-bearing: an
// earlier attempt to change that priority broke 67 unrelated tests). But
// here the alias is being used to qualify a *receiver's own type name*, not
// to resolve a *call target* - picking the constructor's path corrupts
// every method-path this feeds into (e.g. a `remove_at` path built with the
// extra, colliding leaf segment doubled in instead of the correct
// single-leaf module path), which happens to stay harmless for
// builtin-dispatched methods (push/pop/count/...) but breaks any method
// that needs a real specialized function definition (remove_at/remove_swap)
// since no definition exists at the doubled path. Skip the alias lookup for
// exactly this bare-name case instead of touching the shared alias-priority
// machinery.
inline bool isCollectionModuleAliasCollisionName(std::string_view name) {
  return name == "vector" || name == "map";
}

inline bool resolveReceiverType(const Expr &receiver,
                         const LocalTypeMap &locals,
                         const Context &ctx,
                         CanonicalReceiverType &out) {
  out = CanonicalReceiverType{};
  if (receiver.kind == Expr::Kind::Name) {
    auto it = locals.find(receiver.name);
    if (it == locals.end()) {
      return false;
    }
    // Independent re-derivation of the same three lambdas
    // resolveMethodCallTemplateTarget defines locally below
    // (qualifyImportedCollectionTypeText/bindingTypeText/
    // isBorrowedSoaReceiverType/unwrapImportedCollectionReceiverType) -
    // deliberately not shared code, matching every prior resolveReceiverType
    // producer's own "independent reimplementation" precedent so this
    // diff-audit is a genuine cross-check, not a tautology.
    std::function<std::string(std::string)> qualifyImportedCollectionTypeText =
        [&](std::string typeText) -> std::string {
      typeText = normalizeBindingTypeName(typeText);
      if (typeText.empty()) {
        return typeText;
      }
      std::string base;
      std::string argText;
      if (splitTemplateTypeName(typeText, base, argText) && !base.empty()) {
        base = normalizeBindingTypeName(base);
        if ((base == "Reference" || base == "Pointer") && !argText.empty()) {
          std::vector<std::string> args;
          if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
            return typeText;
          }
          return base + "<" + qualifyImportedCollectionTypeText(args.front()) + ">";
        }
        if (!isCollectionModuleAliasCollisionName(base)) {
          if (const std::string *importAlias =
                  lookupScopedImportAliasForNamespace(base, receiver.namespacePrefix, ctx);
              importAlias != nullptr) {
            return *importAlias + "<" + argText + ">";
          }
        }
        return typeText;
      }
      if (!isCollectionModuleAliasCollisionName(typeText)) {
        if (const std::string *importAlias =
                lookupScopedImportAliasForNamespace(typeText, receiver.namespacePrefix, ctx);
            importAlias != nullptr) {
          return *importAlias;
        }
      }
      return typeText;
    };
    auto bindingTypeText = [](const BindingInfo &binding) {
      std::string typeText = binding.typeName;
      if (!binding.typeTemplateArg.empty()) {
        typeText += "<" + binding.typeTemplateArg + ">";
      }
      return typeText;
    };
    auto isBorrowedSoaReceiverType = [&](std::string typeText) {
      typeText = normalizeBindingTypeName(qualifyImportedCollectionTypeText(typeText));
      std::string base;
      std::string argText;
      if (!splitTemplateTypeName(typeText, base, argText) || argText.empty()) {
        return false;
      }
      const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
      if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
        return false;
      }
      return isTemplateMonomorphSoaReceiverType(
          normalizeCollectionReceiverTypeName(
              unwrapCollectionReceiverEnvelope(argText)));
    };
    auto unwrapImportedCollectionReceiverType = [&](const BindingInfo &binding) {
      return unwrapCollectionReceiverEnvelope(
          qualifyImportedCollectionTypeText(bindingTypeText(binding)));
    };

    const std::string wrappedReceiverTypeName =
        qualifyImportedCollectionTypeText(bindingTypeText(it->second));
    out.isBorrowed = isBorrowedSoaReceiverType(bindingTypeText(it->second));
    out.collectionBaseName = unwrapImportedCollectionReceiverType(it->second);
    out.wrappedBaseTypeName = wrappedReceiverTypeName;
    std::string wrapBase;
    std::string wrapArg;
    const std::string normalizedWrapped = normalizeBindingTypeName(wrappedReceiverTypeName);
    if (splitTemplateTypeName(normalizedWrapped, wrapBase, wrapArg) && !wrapArg.empty()) {
      const std::string normalizedWrapBase = normalizeCollectionReceiverTypeName(wrapBase);
      out.isWrapped = (normalizedWrapBase == "Reference" || normalizedWrapBase == "Pointer");
    }
    return !out.collectionBaseName.empty();
  }
  if (receiver.kind == Expr::Kind::Literal) {
    out.collectionBaseName = receiver.isUnsigned ? "u64" : (receiver.intWidth == 64 ? "i64" : "i32");
    return true;
  }
  if (receiver.kind == Expr::Kind::BoolLiteral) {
    out.collectionBaseName = "bool";
    return true;
  }
  if (receiver.kind == Expr::Kind::FloatLiteral) {
    out.collectionBaseName = receiver.floatWidth == 64 ? "f64" : "f32";
    return true;
  }
  if (receiver.kind == Expr::Kind::StringLiteral) {
    out.collectionBaseName = "string";
    return true;
  }
  // Call-kind (and any other kind) is out of scope this round - see the
  // long comment above.
  return false;
}

// Step 1c (docs/ReceiverTargetResolutionConsolidation.md): F3 Call-kind
// real migration. The SOLE production path for F3-C1/C2/C3b/c/d
// (Call-kind receivers, minus F3-C3a - see below). This is NOT an
// independent reimplementation the way resolveReceiverType's Name/literal
// branches above are: F3-C1/C2/C3b/c/d's underlying inference
// (inferBindingTypeForMonomorph/inferExprTypeTextForTemplatedVectorFallback/
// inferDefinitionReturnBindingForTemplatedFallback) is already fully
// delegated production logic, not inline algorithm this file could
// faithfully re-derive from scratch - so this function calls those same
// helpers directly, exactly as the old inline cascade it replaced did.
// Matches the structural precedent of ir_lowerer's
// resolveReceiverTypeFromCallExpr (RT3b's own Call-kind sub-cascade
// producer), which is itself a thin wrapper over shared production
// helpers rather than a from-scratch reimplementation, for the same
// reason.
//
// Takes `Context &ctx` (mutable, not const) because it genuinely invokes
// the three side-effecting helpers named above exactly once per call -
// this is the production computation itself, not a second/observational
// invocation, so the two non-idempotent `...ForTesting` counter fields
// (see the long comment above `resolveReceiverType`) end up mutated
// exactly as the old inline cascade left them: once, by this one call.
// The prior harness round's snapshot/restore machinery (which wrapped a
// deliberate *second*, audit-only invocation) has been removed entirely
// alongside the old inline cascade - there is only ever one invocation
// now, so nothing needs restoring.
//
// Returns true iff it produced a non-empty collectionBaseName, matching
// the outer cascade's own "typeName.empty() => failure" convention
// (resolveMethodCallTemplateTarget's `if (typeName.empty()) return false;`
// immediately after the receiver-kind dispatch). F3-C3a (receiver resolves
// to a struct-constructor call) is a resolution question ("what
// definition path"), not an inference one ("what type"), and stays
// permanently out of `CanonicalReceiverType`'s scope - this function
// returns false without filling `out` in that case, exactly as before, but
// additionally writes the already-resolved callee path into
// `structConstructorReceiverPathOut` so the caller can run F3-C3a's own
// separate short-circuit logic without re-resolving the callee path a
// second time (which would re-invoke the side-effecting helpers above via
// a nested `resolveMethodCallTemplateTarget` recursion and double their
// counter effects). `structConstructorReceiverPathOut` stays empty for
// every other outcome (success or ordinary failure).
inline bool resolveReceiverTypeFromCallExprForTemplateMonomorph(
    const Expr &receiver,
    const LocalTypeMap &locals,
    Context &ctx,
    CanonicalReceiverType &out,
    std::string &structConstructorReceiverPathOut) {
  out = CanonicalReceiverType{};
  structConstructorReceiverPathOut.clear();
  if (receiver.kind != Expr::Kind::Call) {
    return false;
  }
  std::function<std::string(std::string)> qualifyImportedCollectionTypeText =
      [&](std::string typeText) -> std::string {
    typeText = normalizeBindingTypeName(typeText);
    if (typeText.empty()) {
      return typeText;
    }
    std::string base;
    std::string argText;
    if (splitTemplateTypeName(typeText, base, argText) && !base.empty()) {
      base = normalizeBindingTypeName(base);
      if ((base == "Reference" || base == "Pointer") && !argText.empty()) {
        std::vector<std::string> args;
        if (!splitTopLevelTemplateArgs(argText, args) || args.size() != 1) {
          return typeText;
        }
        return base + "<" + qualifyImportedCollectionTypeText(args.front()) + ">";
      }
      if (!isCollectionModuleAliasCollisionName(base)) {
        if (const std::string *importAlias =
                lookupScopedImportAliasForNamespace(base, receiver.namespacePrefix, ctx);
            importAlias != nullptr) {
          return *importAlias + "<" + argText + ">";
        }
      }
      return typeText;
    }
    if (!isCollectionModuleAliasCollisionName(typeText)) {
      if (const std::string *importAlias =
              lookupScopedImportAliasForNamespace(typeText, receiver.namespacePrefix, ctx);
          importAlias != nullptr) {
        return *importAlias;
      }
    }
    return typeText;
  };
  auto bindingTypeText = [](const BindingInfo &binding) {
    std::string typeText = binding.typeName;
    if (!binding.typeTemplateArg.empty()) {
      typeText += "<" + binding.typeTemplateArg + ">";
    }
    return typeText;
  };
  auto isBorrowedSoaReceiverType = [&](std::string typeText) {
    typeText = normalizeBindingTypeName(qualifyImportedCollectionTypeText(typeText));
    std::string base;
    std::string argText;
    if (!splitTemplateTypeName(typeText, base, argText) || argText.empty()) {
      return false;
    }
    const std::string normalizedBase = normalizeCollectionReceiverTypeName(base);
    if (normalizedBase != "Reference" && normalizedBase != "Pointer") {
      return false;
    }
    return isTemplateMonomorphSoaReceiverType(
        normalizeCollectionReceiverTypeName(
            unwrapCollectionReceiverEnvelope(argText)));
  };
  auto unwrapImportedCollectionReceiverType = [&](const BindingInfo &binding) {
    return unwrapCollectionReceiverEnvelope(
        qualifyImportedCollectionTypeText(bindingTypeText(binding)));
  };

  std::string wrappedReceiverTypeName;
  bool isBorrowedSoaReceiver = false;
  std::string typeName;

  BindingInfo receiverInfo;
  if (inferBindingTypeForMonomorph(receiver, {}, locals, hasMathImport(ctx), ctx, receiverInfo)) {
    wrappedReceiverTypeName = qualifyImportedCollectionTypeText(bindingTypeText(receiverInfo));
    isBorrowedSoaReceiver = isBorrowedSoaReceiverType(bindingTypeText(receiverInfo));
    typeName = unwrapImportedCollectionReceiverType(receiverInfo);
  }
  if (typeName.empty()) {
    const std::string inferredTypeText = qualifyImportedCollectionTypeText(
        inferExprTypeTextForTemplatedVectorFallback(
            receiver, locals, receiver.namespacePrefix, ctx, hasMathImport(ctx)));
    isBorrowedSoaReceiver = isBorrowedSoaReceiverType(inferredTypeText);
    typeName = unwrapCollectionReceiverEnvelope(inferredTypeText);
  }
  if (!receiver.isBinding) {
    std::string resolved;
    if (receiver.isMethodCall) {
      if (!resolveMethodCallTemplateTarget(receiver, locals, ctx, resolved)) {
        resolved.clear();
      }
    } else {
      resolved = resolveCalleePath(receiver, receiver.namespacePrefix, ctx);
    }
    auto defIt = ctx.sourceDefs.find(resolved);
    if (defIt != ctx.sourceDefs.end()) {
      if (isStructDefinition(defIt->second)) {
        // F3-C3a: out of scope, see comment above. Hand the already-
        // resolved callee path back to the caller so it can run its own
        // separate short-circuit without re-resolving `resolved` (which
        // would re-invoke the side-effecting helpers above a second time).
        structConstructorReceiverPathOut = resolved;
        return false;
      }
      for (const auto &transform : defIt->second.transforms) {
        if (transform.name != "return" || transform.templateArgs.size() != 1) {
          continue;
        }
        const std::string &returnType = transform.templateArgs.front();
        if (returnType == "auto") {
          continue;
        }
        wrappedReceiverTypeName = qualifyImportedCollectionTypeText(returnType);
        isBorrowedSoaReceiver = isBorrowedSoaReceiverType(returnType);
        typeName = unwrapCollectionReceiverEnvelope(
            qualifyImportedCollectionTypeText(returnType));
        break;
      }
      if (typeName.empty()) {
        BindingInfo inferredReturn;
        if (inferDefinitionReturnBindingForTemplatedFallback(
                defIt->second, hasMathImport(ctx), ctx, inferredReturn)) {
          wrappedReceiverTypeName = qualifyImportedCollectionTypeText(bindingTypeText(inferredReturn));
          isBorrowedSoaReceiver =
              isBorrowedSoaReceiverType(bindingTypeText(inferredReturn));
          typeName = unwrapImportedCollectionReceiverType(inferredReturn);
        }
      }
    } else {
      std::string collection;
      if (getBuiltinCollectionName(receiver, collection)) {
        typeName = collection;
      }
    }
  }

  out.wrappedBaseTypeName = wrappedReceiverTypeName;
  out.isBorrowed = isBorrowedSoaReceiver;
  out.collectionBaseName = typeName;
  return !out.collectionBaseName.empty();
}

} // namespace methodTargetHelpers
} // namespace primec
