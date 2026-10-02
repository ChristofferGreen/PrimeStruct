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
#include "TemplateMonomorphMethodTargetsHelpers.h"

namespace primec {
using namespace methodTargetHelpers;

bool resolveMethodCallTemplateTarget(const Expr &expr,
                                     const LocalTypeMap &locals,
                                     const Context &ctx,
                                     std::string &pathOut) {
  pathOut.clear();
  if (!expr.isMethodCall || expr.args.empty() || expr.name.empty()) {
    return false;
  }
  const std::string rawMethodName = expr.name;
  std::string methodName = rawMethodName;
  if (!methodName.empty() && methodName.front() == '/') {
    methodName.erase(methodName.begin());
  }
  auto normalizeCollectionMethodName = [](const std::string &receiverTypeName,
                                          std::string candidate) -> std::string {
    if (receiverTypeName == "array" || receiverTypeName == "vector" ||
        isTemplateMonomorphSoaReceiverType(receiverTypeName)) {
      const std::string vectorPrefix = std::string("vector") + "/";
      const std::string arrayPrefix = "array/";
      if (candidate.rfind(vectorPrefix, 0) == 0) {
        return candidate.substr(vectorPrefix.size());
      }
      if (candidate.rfind(arrayPrefix, 0) == 0) {
        return candidate.substr(arrayPrefix.size());
      }
      if (isUnrootedCanonicalVectorCompatibilityPath(candidate)) {
        return std::string(stripUnrootedCanonicalVectorCompatibilityPrefix(candidate));
      }
      std::string helperName;
      if (stripTemplateMonomorphSoaHelperPrefix(candidate, helperName, false)) {
        return helperName;
      }
    }
    if (receiverTypeName == "map") {
      return metadataBackedKeyValueHelperMethodName(candidate);
    }
    return candidate;
  };
  auto normalizeFileMethodName = [](std::string_view methodName) {
    if (methodName == "readByte") {
      return std::string("read_byte");
    }
    if (methodName == "writeLine") {
      return std::string("write_line");
    }
    if (methodName == "writeByte") {
      return std::string("write_byte");
    }
    if (methodName == "writeBytes") {
      return std::string("write_bytes");
    }
    return std::string(methodName);
  };
  auto normalizeFileErrorMethodName = [](std::string_view methodName) {
    if (methodName == "isEof") {
      return std::string("is_eof");
    }
    return std::string(methodName);
  };
  auto selectStaticHelperOverloadPath = [&](const std::string &resolvedPath) -> std::string {
    auto familyIt = ctx.helperOverloads.find(resolvedPath);
    if (familyIt == ctx.helperOverloads.end()) {
      return resolvedPath;
    }
    const size_t argumentCount = expr.args.empty() ? 0 : expr.args.size() - 1;
    for (const auto &entry : familyIt->second) {
      if (entry.parameterCount == argumentCount) {
        return entry.internalPath;
      }
    }
    return resolvedPath;
  };
  auto hasDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathString(path);
    if (ctx.sourceDefs.count(pathString) > 0 || ctx.helperOverloads.count(pathString) > 0) {
      return true;
    }
    return anySourceDefStartsWith(ctx, pathString + "<") ||
           anySourceDefStartsWith(ctx, pathString + "__t") ||
           anySourceDefStartsWith(ctx, pathString + "__ov");
  };
  auto hasTemplatedDefinitionFamilyPath = [&](std::string_view path) {
    const std::string pathString(path);
    if (ctx.templateDefs.count(pathString) > 0) {
      return true;
    }
    return anySourceDefStartsWith(ctx, pathString + "<");
  };
  auto receiverHelperFamilyLeaf = [](std::string_view resolvedType) -> std::string {
    if (resolvedType.empty()) {
      return {};
    }
    const size_t slash = resolvedType.find_last_of('/');
    const size_t nameStart = slash == std::string_view::npos ? 0 : slash + 1;
    size_t nameEnd = resolvedType.size();
    auto limitNameEnd = [&](size_t candidate) {
      if (candidate != std::string_view::npos && candidate < nameEnd) {
        nameEnd = candidate;
      }
    };
    limitNameEnd(resolvedType.find("__t", nameStart));
    limitNameEnd(resolvedType.find("__ov", nameStart));
    limitNameEnd(resolvedType.find('<', nameStart));
    if (nameEnd <= nameStart) {
      return {};
    }
    return std::string(resolvedType.substr(nameStart, nameEnd - nameStart));
  };
  auto soaCanonicalMethodPath = [](const std::string &helperNameString) {
    return compatibilitySoaHelperTargetPath(helperNameString);
  };
  auto preferredSamePathSoaMethodTarget =
      [&](std::string_view helperName, std::string_view samePathPrefix) {
    const std::string helperNameString(helperName);
    const std::string samePathPrefixString(samePathPrefix);
    const std::string samePath = samePathPrefixString + helperNameString;
    if (hasDefinitionFamilyPath(samePath)) {
      return samePath;
    }
    return soaCanonicalMethodPath(helperNameString);
  };
  auto preferredSamePathSoaToAosMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(helperName, "/");
  };
  auto preferredSamePathSoaCountMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(
        helperName, templateMonomorphSamePathSoaHelperPrefix());
  };
  auto preferredSamePathSoaPushReserveMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(
        helperName, templateMonomorphSamePathSoaHelperPrefix());
  };
  auto preferredSamePathSoaGetMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(
        helperName, templateMonomorphSamePathSoaHelperPrefix());
  };
  auto preferredSamePathSoaRefMethodTarget = [&](std::string_view helperName) {
    return preferredSamePathSoaMethodTarget(
        helperName, templateMonomorphSamePathSoaHelperPrefix());
  };
  auto borrowedSoaWrapperMethodName = [](std::string_view helperName) {
    if (helperName == "count") {
      return std::string(collection_helpers::kCountRef);
    }
    if (helperName == "get") {
      return std::string(collection_helpers::kGetRef);
    }
    if (helperName == "ref") {
      return std::string(collection_helpers::kRefRef);
    }
    if (helperName == templateMonomorphSoaToAosHelperName()) {
      return templateMonomorphSoaToAosHelperName(true);
    }
    return std::string(helperName);
  };
  const Expr &receiver = expr.args.front();
  auto resolveIndexedArgsPackMapMethodTarget = [&]() -> bool {
    if (receiver.kind != Expr::Kind::Call || receiver.isBinding ||
        receiver.isMethodCall || receiver.args.size() != 2 ||
        (!isSimpleCallName(receiver, "at") &&
         !isSimpleCallName(receiver, "at_unsafe"))) {
      return false;
    }
    const Expr &packReceiver = receiver.args.front();
    if (packReceiver.kind != Expr::Kind::Name) {
      return false;
    }
    auto bindingIt = locals.find(packReceiver.name);
    if (bindingIt == locals.end()) {
      return false;
    }
    std::string elemType;
    if (!getArgsPackElementType(bindingIt->second, elemType)) {
      return false;
    }
    std::string keyType;
    std::string valueType;
    if (!extractKeyValueCollectionTypesFromTypeText(elemType, keyType, valueType)) {
      return false;
    }
    std::string helperName = normalizeCollectionMethodName("map", methodName);
    std::string base;
    std::string argText;
    const bool receiverIsWrapped =
        splitTemplateTypeName(normalizeBindingTypeName(elemType), base,
                              argText) &&
        (normalizeBindingTypeName(base) == "Reference" ||
         normalizeBindingTypeName(base) == "Pointer");
    if (receiverIsWrapped) {
      if (helperName == "count") {
        helperName = collection_helpers::kCountRef;
      } else if (helperName == "contains") {
        helperName = collection_helpers::kContainsRef;
      } else if (helperName == "tryAt") {
        helperName = collection_helpers::kTryAtRef;
      } else if (helperName == "at") {
        helperName = collection_helpers::kAtRef;
      } else if (helperName == "at_unsafe") {
        helperName = collection_helpers::kAtUnsafeRef;
      } else if (helperName == "insert") {
        helperName = collection_helpers::kInsertRef;
      }
    }
    pathOut = selectHelperOverloadPath(
        expr, metadataBackedCanonicalKeyValueHelperPath(helperName), ctx);
    return true;
  };
  bool isBorrowedSoaReceiver = false;
  std::string wrappedReceiverTypeName;
  if (receiver.kind == Expr::Kind::Name && normalizeBindingTypeName(receiver.name) == "FileError") {
    if (methodName == "result") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/result");
      return true;
    }
    if (methodName == "status") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/status");
      return true;
    }
    if (methodName == "why") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/why");
      return true;
    }
    if (methodName == "is_eof") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/is_eof");
      return true;
    }
    if (methodName == "eof") {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/eof");
      return true;
    }
  }
  std::string typeName;
  if (receiver.kind == Expr::Kind::Name || receiver.kind == Expr::Kind::Literal ||
      receiver.kind == Expr::Kind::BoolLiteral || receiver.kind == Expr::Kind::FloatLiteral ||
      receiver.kind == Expr::Kind::StringLiteral) {
    // Step 1c (docs/ReceiverTargetResolutionConsolidation.md): migrated onto
    // resolveReceiverType for Name-kind and primitive-literal-kind receivers
    // (F3-N1/N2, F3-L/B/Fl/S). Call-kind is migrated too, just onto the
    // sibling producer below (see that branch's own comment for why).
    CanonicalReceiverType canonical;
    resolveReceiverType(receiver, locals, ctx, canonical);
    wrappedReceiverTypeName = canonical.wrappedBaseTypeName;
    isBorrowedSoaReceiver = canonical.isBorrowed;
    typeName = canonical.collectionBaseName;
  } else if (receiver.kind == Expr::Kind::Call) {
    // Step 1c (docs/ReceiverTargetResolutionConsolidation.md): migrated onto
    // resolveReceiverTypeFromCallExprForTemplateMonomorph for Call-kind
    // receivers (F3-C1/C2/C3b/c/d), replacing the old inline cascade. F3-C3a
    // (the struct-constructor-call short circuit) is a resolution question,
    // not an inference one, so it stays outside CanonicalReceiverType's
    // scope and is handled here via the producer's
    // structConstructorReceiverPathOut - the same "resolved" callee path
    // the old inline cascade itself already special-cased, now handed back
    // instead of re-resolved (re-resolving here would re-invoke the
    // side-effecting inference helpers a second time and double their
    // ...ForTesting counter effects).
    CanonicalReceiverType canonical;
    std::string structConstructorReceiverPath;
    resolveReceiverTypeFromCallExprForTemplateMonomorph(
        receiver, locals, const_cast<Context &>(ctx), canonical, structConstructorReceiverPath);
    if (!structConstructorReceiverPath.empty()) {
      // F3-C3a: bypass the rest of F3, F5, and the entire F6-F16 chain,
      // exactly as the old inline cascade did.
      pathOut = selectHelperOverloadPath(
          expr, structConstructorReceiverPath + "/" + methodName, ctx);
      return true;
    }
    wrappedReceiverTypeName = canonical.wrappedBaseTypeName;
    isBorrowedSoaReceiver = canonical.isBorrowed;
    typeName = canonical.collectionBaseName;
  }
  if (resolveIndexedArgsPackMapMethodTarget()) {
    return true;
  }
  if (typeName.empty()) {
    return false;
  }
  if (!expr.templateArgs.empty() && !wrappedReceiverTypeName.empty()) {
    std::string wrapperBase;
    std::string wrapperArgText;
    if (splitTemplateTypeName(normalizeBindingTypeName(wrappedReceiverTypeName),
                              wrapperBase,
                              wrapperArgText)) {
      wrapperBase = normalizeCollectionReceiverTypeName(wrapperBase);
      if ((wrapperBase == "Reference" || wrapperBase == "Pointer") &&
          !wrapperArgText.empty()) {
        std::string wrapperMethodName = methodName;
        const size_t slash = wrapperMethodName.find_last_of('/');
        if (slash != std::string::npos) {
          wrapperMethodName.erase(0, slash + 1);
        }
        const std::string rootedWrapperMethodPath =
            "/" + wrapperBase + "/" + wrapperMethodName;
        if (hasTemplatedDefinitionFamilyPath(rootedWrapperMethodPath) &&
            hasDefinitionFamilyPath(rootedWrapperMethodPath)) {
          pathOut = selectHelperOverloadPath(expr, rootedWrapperMethodPath, ctx);
          return true;
        }
      }
    }
  }
  typeName = normalizeCollectionReceiverTypeName(typeName);
  auto preferredFileMethodTarget = [&](std::string_view helperName) {
    const std::string normalizedHelperName = normalizeFileMethodName(helperName);
    const std::string builtinPath = "/file/" + normalizedHelperName;
    if (normalizedHelperName != "write" &&
        normalizedHelperName != "write_line" &&
        normalizedHelperName != "close") {
      return builtinPath;
    }
    if ((normalizedHelperName == "write" || normalizedHelperName == "write_line") &&
        expr.args.size() > 10) {
      return builtinPath;
    }
    if (receiver.kind == Expr::Kind::Name && receiver.name == "self") {
      return builtinPath;
    }
    const std::string stdlibPath = "/File/" + normalizedHelperName;
    if (hasDefinitionFamilyPath(stdlibPath)) {
      return stdlibPath;
    }
    return builtinPath;
  };
  const std::string normalizedMethodName = normalizeCollectionMethodName(typeName, methodName);
  std::string normalizedTypeName = typeName;
  if (!normalizedTypeName.empty() && normalizedTypeName.front() == '/') {
    normalizedTypeName.erase(normalizedTypeName.begin());
  }
  const auto normalizedReceiverLeafName = [&]() {
    const size_t slash = normalizedTypeName.find_last_of('/');
    return slash == std::string::npos ? normalizedTypeName
                                      : normalizedTypeName.substr(slash + 1);
  }();
  // TODO-5294 Step 2, monomorphization stage: F7's File-family slice (per
  // docs/ReceiverTargetResolutionConsolidation.md's Step 0 Row F table) now
  // delegates its family classification to the shared classifier instead of
  // the inline (typeName == "File" || normalizedReceiverLeafName == "File")
  // && isFileMethodName(normalizedMethodName) gate, per the Step 1b
  // diff-audit harness this call site carried (proven zero-divergence,
  // 2026-09-09 - the classifier's File family check already uses
  // isFileHandleMethodName, the identical 11-name set as this file's own
  // isFileMethodName lambda, verified name-for-name). Only the
  // *classification* moved here - normalizedReceiverLeafName and
  // normalizedMethodName still feed preferredFileMethodTarget exactly as
  // before, so the resulting path construction is byte-identical to what F7
  // always produced. By this point typeName has already gone through
  // normalizeCollectionReceiverTypeName above (same as the F9/F11/F13
  // slices' own note), so there is no template text left to parse - this
  // feeds the classifier's isTemplateShaped/templateShapedBaseName inputs
  // the already-known leaf name directly, the same "hand over the
  // pre-parsed base" approach F9/F11/F13 all used.
  {
    ReceiverElementFamilyJointInput jointInput;
    jointInput.unwrappedElementType = normalizedReceiverLeafName;
    jointInput.rawElementBaseType = normalizedReceiverLeafName;
    jointInput.isTemplateShaped = true;
    jointInput.templateShapedBaseName = normalizedReceiverLeafName;
    jointInput.normalizedMethodName = normalizedMethodName;
    // Soa/KeyValue predicates are unreachable here for a "File" leaf: the
    // classifier's template-shape block checks VectorLike, then Soa, then
    // Buffer, then KeyValue, then File in that fixed order - none of the
    // earlier checks can match the literal base "File", so a null
    // (never-matches) predicate cannot change this outcome.
    ReceiverElementFamilyPredicates predicates{};
    const ReceiverElementFamily family =
        classifyReceiverElementFamilyJoint(jointInput, predicates).family;
    if (family == ReceiverElementFamily::File) {
      pathOut = preferredFileMethodTarget(normalizedMethodName);
      return true;
    }
  }
  if (isExplicitRemovedCollectionMethodAlias(typeName, rawMethodName)) {
    return false;
  }
  // TODO-5294 Step 2, monomorphization stage: F9's primitive slice (per
  // docs/ReceiverTargetResolutionConsolidation.md's Step 0 Row F table) now
  // delegates its family classification to the shared classifier instead of
  // the inline isPrimitiveBindingTypeName(typeName) gate, per the Step 1b
  // diff-audit harness this call site carried (proven zero-divergence,
  // 2026-09-09, including the String+Primitive-verdict equivalence claim
  // verified against the corpus, not just asserted). Only the
  // *classification* moved here - normalizeBindingTypeName(typeName) below
  // still runs on the original typeName text (not on any classifier-derived
  // value), so the resulting "/<baseType>/<method>" path construction is
  // byte-identical to what F9 always produced.
  {
    ReceiverElementFamilyJointInput jointInput;
    // F9 has no wrapped/unwrapped asymmetry to reproduce (typeName has
    // already gone through normalizeCollectionReceiverTypeName above, and F9
    // itself never re-derives a separate raw/wrapped variant), so both
    // classifier inputs are the same text.
    jointInput.unwrappedElementType = typeName;
    jointInput.rawElementBaseType = typeName;
    jointInput.isTemplateShaped = false;
    jointInput.normalizedMethodName = normalizedMethodName;
    // Soa/KeyValue predicates are unreachable here: isTemplateShaped is
    // false, so the classifier's template-shape-gated block (the only place
    // either predicate is consulted) never runs.
    ReceiverElementFamilyPredicates predicates{};
    const ReceiverElementFamily family =
        classifyReceiverElementFamilyJoint(jointInput, predicates).family;
    // Per the harness's proven finding: both Primitive and String verdicts
    // dispatch as primitive here, matching production's
    // isPrimitiveBindingTypeName folding "string" into the same bucket as
    // i32/bool/etc (both go through the identical "/<typeName>/<method>"
    // path formula).
    if (family == ReceiverElementFamily::Primitive ||
        family == ReceiverElementFamily::String) {
      pathOut = selectHelperOverloadPath(expr, "/" + normalizeBindingTypeName(typeName) + "/" + normalizedMethodName, ctx);
      return true;
    }
  }
  if (normalizedReceiverLeafName == "args") {
    const std::string argsPackMethodName =
        normalizeCollectionMethodName("array", methodName);
    if (argsPackMethodName == "count" || argsPackMethodName == "at" ||
        argsPackMethodName == "at_unsafe") {
      pathOut = collection_helpers::kRootedArrayPrefix + argsPackMethodName;
      return true;
    }
    return false;
  }
  const std::string fileErrorMethodName =
      normalizeFileErrorMethodName(normalizedMethodName);
  // TODO-5294 Step 2, monomorphization stage: F11's FileError sub-case (per
  // docs/ReceiverTargetResolutionConsolidation.md's Step 0 Row F table) now
  // delegates its family/method-name-gate decision to the shared classifier
  // instead of its own inline 4-name check, per the Step 1b diff-audit
  // harness this call site carried (proven zero-divergence, 2026-09-09).
  // Only the *classification* moved here - the resolved-path construction,
  // isBuiltinOut default, and return-value behavior below are byte-identical
  // to what F11 always did. Scoped deliberately narrow, matching the
  // harness's own scope: NOT F1's separate literal-Name-spelled-"FileError"
  // receiver shape a few dozen lines above (a different guard entirely, not
  // a type classification at all), and NOT the ImageError/ContainerError/
  // GfxError sub-cases immediately below (same shape, but the classifier has
  // no family for those - left as inline checks, unmigrated).
  {
    ReceiverElementFamilyJointInput jointInput;
    // Production's guard compares the leaf-extracted type name (post
    // slash-split), not the raw typeName text, so that is what is fed to the
    // classifier here too - there is no separate wrapped/unwrapped text at
    // this point in this function (typeName has already gone through
    // normalizeCollectionReceiverTypeName above), so both classifier inputs
    // are the same leaf text.
    jointInput.unwrappedElementType = normalizedReceiverLeafName;
    jointInput.rawElementBaseType = normalizedReceiverLeafName;
    jointInput.isTemplateShaped = false;
    // Production compares fileErrorMethodName (already normalized via
    // normalizeFileErrorMethodName's isEof->is_eof mapping), so that
    // already-normalized value - not the raw normalizedMethodName - is what
    // the classifier's own method-name gate sees, mirroring how the
    // already-migrated semantics-stage call sites pass their own
    // already-normalized method name in.
    jointInput.normalizedMethodName = fileErrorMethodName;
    // Soa/KeyValue predicates are unreachable here: the classifier's
    // FileError check (R2) runs before either, so a null (never-matches)
    // predicate cannot change this outcome for a "FileError" leaf.
    ReceiverElementFamilyPredicates predicates{};
    const ReceiverElementFamily family =
        classifyReceiverElementFamilyJoint(jointInput, predicates).family;
    if (normalizedReceiverLeafName == "FileError" &&
        family == ReceiverElementFamily::FileError) {
      pathOut = selectStaticHelperOverloadPath("/std/file/FileError/" + fileErrorMethodName);
      return true;
    }
  }
  if (normalizedReceiverLeafName == "ImageError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    pathOut = selectStaticHelperOverloadPath("/std/image/ImageError/" + normalizedMethodName);
    return true;
  }
  if (normalizedReceiverLeafName == "ContainerError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    pathOut = selectStaticHelperOverloadPath(collection_helpers::kCanonicalContainerErrorTypePrefix + normalizedMethodName);
    return true;
  }
  if (normalizedReceiverLeafName == "GfxError" &&
      (normalizedMethodName == "why" || normalizedMethodName == "status" ||
       normalizedMethodName == "result")) {
    // GfxError exists in both /std/gfx and /std/gfx/experimental; prefer
    // whichever one is actually present in this compilation rather than
    // hardcoding the non-experimental path. Without this, a method-call-
    // syntax use of an experimental GfxError's templated result<T>/why/
    // status requests instantiation of the wrong (non-existent) base path,
    // silently producing no specialization - normally masked by whole-file
    // stdlib splicing, where some other explicit-absolute-path call
    // elsewhere in the same file happens to already have triggered the
    // needed specialization via the correct path.
    const std::string experimentalPath =
        "/std/gfx/experimental/GfxError/" + normalizedMethodName;
    if (hasDefinitionFamilyPath(experimentalPath)) {
      pathOut = selectStaticHelperOverloadPath(experimentalPath);
      return true;
    }
    pathOut = selectStaticHelperOverloadPath("/std/gfx/GfxError/" + normalizedMethodName);
    return true;
  }
  // TODO-5294 Step 2, monomorphization stage: F12's generic-SOA-receiver
  // method-name-paired dispatch (count/count_ref, toAos/toAosRef, get/get_ref,
  // push/reserve, ref/ref_ref) now delegates its family classification to the
  // shared classifier instead of the inline
  // isTemplateMonomorphSoaReceiverType(normalizedTypeName) gate, per the Step
  // 1b diff-audit harness this call site carried (proven zero-divergence,
  // 2026-09-09). Unlike F9/F11's families, Soa family membership itself
  // carries no method-name gating in the classifier (confirmed by direct
  // reading of classifyReceiverElementFamilyJoint: the Soa check inside the
  // template-shape block returns unconditionally once the predicate matches,
  // no isBufferAccessorMethodName/isFileHandleMethodName-equivalent gate the
  // way Buffer/File have) - so one classification call, made once before all
  // five branches (which share the identical family gate), covers all of F12
  // rather than needing a separate call per method-name pair. By this point
  // typeName has already gone through normalizeCollectionReceiverTypeName
  // above (same as the F9/F11/F13 slices' own note), so there is no template
  // text left to parse - this feeds the classifier's isTemplateShaped/
  // templateShapedBaseName inputs the already-known base name directly
  // (isTemplateShaped=true, templateShapedBaseName=normalizedTypeName), the
  // same "hand over the pre-parsed base" approach F13/F13b/F13c's slice used,
  // including its identical isInternalSoaCollectionTypeName predicate wrapper
  // (a bare `== templateMonomorphSoaReceiverTypeName()` string comparison via
  // isTemplateMonomorphSoaReceiverType, not a real struct-metadata lookup for
  // this stage). Only the *classification* moved here - the five branches'
  // downstream helper-name/path-construction/return behavior below is
  // byte-identical to what F12 always produced.
  const bool isGenericSoaReceiver = [&] {
    ReceiverElementFamilyJointInput jointInput;
    jointInput.unwrappedElementType = normalizedTypeName;
    jointInput.rawElementBaseType = normalizedTypeName;
    jointInput.isTemplateShaped = true;
    jointInput.templateShapedBaseName = normalizedTypeName;
    jointInput.normalizedMethodName = normalizedMethodName;
    ReceiverElementFamilyPredicates predicates{};
    predicates.isInternalSoaCollectionTypeName =
        [](std::string_view candidate) {
          return isTemplateMonomorphSoaReceiverType(std::string(candidate));
        };
    // KeyValue predicate is unreachable here: the classifier's template-
    // shape block checks VectorLike then Soa before KeyValue, and
    // isTemplateMonomorphSoaReceiverType is a fixed-string match that, when
    // true, already committed to Soa above KeyValue's own check - a null
    // (never-matches) KeyValue predicate cannot change this outcome.
    return classifyReceiverElementFamilyJoint(jointInput, predicates).family ==
           ReceiverElementFamily::Soa;
  }();
  if (isGenericSoaReceiver &&
      (collection_helpers::isCountHelperName(normalizedMethodName))) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaCountMethodTarget(helperName), ctx);
    return true;
  }
  if (isGenericSoaReceiver &&
      (normalizedMethodName == templateMonomorphSoaToAosHelperName() ||
       normalizedMethodName == templateMonomorphSoaToAosHelperName(true))) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaToAosMethodTarget(helperName), ctx);
    return true;
  }
  if (isGenericSoaReceiver &&
      (collection_helpers::isGetHelperName(normalizedMethodName))) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaGetMethodTarget(helperName), ctx);
    return true;
  }
  if (isGenericSoaReceiver &&
      (normalizedMethodName == "push" || normalizedMethodName == "reserve")) {
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaPushReserveMethodTarget(normalizedMethodName), ctx);
    return true;
  }
  if (isGenericSoaReceiver &&
      (collection_helpers::isRefHelperName(normalizedMethodName))) {
    const std::string helperName =
        isBorrowedSoaReceiver ? borrowedSoaWrapperMethodName(normalizedMethodName)
                              : normalizedMethodName;
    pathOut = selectHelperOverloadPath(
        expr, preferredSamePathSoaRefMethodTarget(helperName), ctx);
    return true;
  }
  std::string resolvedType = resolveTypePath(typeName, receiver.namespacePrefix);
  // TODO-5294 Step 2, monomorphization stage: F13/F13b/F13c's collection-
  // family membership test (per
  // docs/ReceiverTargetResolutionConsolidation.md's Step 0 Row F table) now
  // delegates its family classification to the shared classifier instead of
  // the inline literal-set check ("array"/"vector"/"map" OR'd with
  // isTemplateMonomorphSoaReceiverType(typeName)), per the Step 1b diff-audit
  // harness this call site carried (proven zero-divergence, 2026-09-09). By
  // this point in the cascade typeName has already gone through
  // normalizeCollectionReceiverTypeName above (same as the F9/F11 slices'
  // own note), so it is already reduced to a bare base name with no
  // generic-argument text left to parse; there is nothing for the
  // classifier's own splitTemplateTypeName-shaped isTemplateShaped/
  // templateShapedBaseName inputs to derive from, so this call feeds them
  // the same already-known base name directly (isTemplateShaped=true,
  // templateShapedBaseName=typeName), the same "we already have the parsed
  // base, so hand it over pre-parsed" approach the F9 slice used for its own
  // isTemplateShaped=false case. The classifier's KeyValue predicate is
  // supplied as a literal `== "map"` match, deliberately mirroring this
  // exact production guard's own literal check (not any real struct-
  // metadata-backed key-value surface predicate). Only the *classification*
  // moved here - the downstream dispatch/import-alias-substitution/string-
  // fallback/rejection behavior below is byte-identical to what F13/F13b/F13c
  // always did.
  const ReceiverElementFamily collectionFamilyVerdict = [&] {
    ReceiverElementFamilyJointInput jointInput;
    jointInput.unwrappedElementType = typeName;
    jointInput.rawElementBaseType = typeName;
    jointInput.isTemplateShaped = true;
    jointInput.templateShapedBaseName = typeName;
    jointInput.normalizedMethodName = normalizedMethodName;
    ReceiverElementFamilyPredicates predicates{};
    predicates.isInternalSoaCollectionTypeName =
        [](std::string_view candidate) {
          return isTemplateMonomorphSoaReceiverType(std::string(candidate));
        };
    predicates.isKeyValueSurfaceTypeName =
        [](std::string_view candidate) { return candidate == "map"; };
    return classifyReceiverElementFamilyJoint(jointInput, predicates).family;
  }();
  const bool isCollectionFamilyReceiver =
      collectionFamilyVerdict == ReceiverElementFamily::VectorLike ||
      collectionFamilyVerdict == ReceiverElementFamily::Soa ||
      collectionFamilyVerdict == ReceiverElementFamily::KeyValue;
  if (ctx.sourceDefs.count(resolvedType) == 0 && !isCollectionFamilyReceiver) {
    if (const std::string *importAlias =
            lookupScopedImportAliasForNamespace(normalizedTypeName, receiver.namespacePrefix, ctx);
        importAlias != nullptr) {
      resolvedType = *importAlias;
    }
  }
  if (typeName == "vector" && !wrappedReceiverTypeName.empty()) {
    // TODO-5375: a Reference<vector<T>> receiver routes the vector helper
    // spellings to the canonical borrowed-vector helpers.
    std::string wrapperBase;
    std::string wrapperArgText;
    if (splitTemplateTypeName(normalizeBindingTypeName(wrappedReceiverTypeName),
                              wrapperBase, wrapperArgText) &&
        normalizeCollectionReceiverTypeName(wrapperBase) == "Reference") {
      const std::string_view leaf =
          collection_helpers::borrowedVectorHelperLeaf(normalizedMethodName);
      const std::string borrowedPath =
          std::string(collection_helpers::kCanonicalVectorPrefix) + std::string(leaf);
      if (!leaf.empty() &&
          (ctx.sourceDefs.count(borrowedPath) > 0 ||
           ctx.helperOverloads.count(borrowedPath) > 0)) {
        pathOut = selectHelperOverloadPath(expr, borrowedPath, ctx);
        return true;
      }
    }
  }
  if (ctx.sourceDefs.count(resolvedType) == 0) {
    if (isCollectionFamilyReceiver) {
      pathOut = "/" + typeName + "/" + normalizedMethodName;
      pathOut = preferVectorStdlibHelperPath(pathOut, ctx.sourceDefs);
      pathOut = selectHelperOverloadPath(expr, pathOut, ctx);
      return true;
    }
    if (typeName == "string") {
      pathOut = selectHelperOverloadPath(expr, collection_helpers::kRootedStringPrefix + normalizedMethodName, ctx);
      return true;
    }
    return false;
  }
  // TODO-5294: F14 (the isConcreteExperimentalSoaReceiver dispatch that used
  // to live here - isTemplateMonomorphSoaReceiverType(normalizedTypeName) &&
  // isExperimentalSoaVectorSpecializedTypePath(resolvedType), gating the same
  // six method-name pairs as F12 above: count/count_ref, get/get_ref,
  // push/reserve, ref/ref_ref, toAos/toAosRef) was deleted as proven-
  // unreachable dead code (see
  // docs/ReceiverTargetResolutionConsolidation.md, Step 0's "F12/F14 dead
  // code" finding, re-confirmed against this file's current (post-F12-
  // migration) code before deletion). F12's isGenericSoaReceiver gate a few
  // dozen lines above is exactly
  // isTemplateMonomorphSoaReceiverType(normalizedTypeName) (confirmed by
  // direct classifier trace: that fixed internal SOA name matches neither
  // "string" nor "FileError" nor vector / array, so the classifier's
  // isInternalSoaCollectionTypeName predicate is the first and only thing
  // that can match it, unconditionally landing on Soa) - both normalizedType-
  // Name and normalizedMethodName are unchanged between the two call sites,
  // so whenever F14's first conjunct held, F12's gate already held too, and
  // for any of the six shared method-name pairs F12 had already returned
  // long before reaching here. F14's second conjunct
  // (isExperimentalSoaVectorSpecializedTypePath(resolvedType)) could
  // therefore never matter: it only narrows an already-unreachable branch.
  const std::string samePathMethodTarget = resolvedType + "/" + normalizedMethodName;
  const std::string receiverHelperLeaf = receiverHelperFamilyLeaf(resolvedType);
  if (!receiverHelperLeaf.empty()) {
    const std::string rootedHelperTarget = "/" + receiverHelperLeaf + "/" + normalizedMethodName;
    if (samePathMethodTarget != rootedHelperTarget &&
        !hasDefinitionFamilyPath(samePathMethodTarget) &&
        hasDefinitionFamilyPath(rootedHelperTarget)) {
      pathOut = selectHelperOverloadPath(expr, rootedHelperTarget, ctx);
      return true;
    }
  }
  pathOut = preferVectorStdlibHelperPath(resolvedType + "/" + normalizedMethodName, ctx.sourceDefs);
  pathOut = selectHelperOverloadPath(expr, pathOut, ctx);
  return true;
}

std::string resolveNameToPath(const std::string &name,
                              const std::string &namespacePrefix,
                              const std::unordered_map<std::string, std::string> &importAliases,
                              const std::unordered_map<std::string, Definition> &defs) {
  if (name.empty()) {
    return "";
  }
  if (!name.empty() && name[0] == '/') {
    return name;
  }
  if (name.find('/') != std::string::npos) {
    return "/" + name;
  }
  if (!namespacePrefix.empty()) {
    std::string scoped = namespacePrefix + "/" + name;
    if (defs.count(scoped) > 0) {
      return scoped;
    }
    auto aliasIt = importAliases.find(name);
    if (aliasIt != importAliases.end()) {
      return aliasIt->second;
    }
    return scoped;
  }
  std::string root = "/" + name;
  if (defs.count(root) > 0) {
    return root;
  }
  auto aliasIt = importAliases.find(name);
  if (aliasIt != importAliases.end()) {
    return aliasIt->second;
  }
  return root;
}

} // namespace primec
