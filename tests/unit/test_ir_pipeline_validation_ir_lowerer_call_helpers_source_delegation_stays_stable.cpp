#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("ir lowerer call helpers source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                             : std::filesystem::path("..");

  const std::filesystem::path callHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCallHelpers.cpp";
  const std::filesystem::path accessTargetResolutionPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererAccessTargetResolution.cpp";
  const std::filesystem::path accessLoadHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererAccessLoadHelpers.cpp";
  const std::filesystem::path indexedAccessEmitPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererIndexedAccessEmit.cpp";
  const std::filesystem::path callResolutionPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCallResolution.cpp";
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";
  const std::filesystem::path inlineParamHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineParamHelpers.cpp";
  const std::filesystem::path uninitializedStructInferencePath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererUninitializedStructInference.cpp";
  const std::filesystem::path setupTypeMethodTargetHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeMethodTargetHelpers.cpp";
  const std::filesystem::path countAccessClassifiersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCountAccessClassifiers.cpp";
  const std::filesystem::path nativeTailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp";
  const std::filesystem::path operatorCollectionMutationHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererOperatorCollectionMutationHelpers.cpp";
  const std::filesystem::path astCallPathHelpersPath =
      repoRoot / "include" / "primec" / "AstCallPathHelpers.h";
  REQUIRE(std::filesystem::exists(callHelpersPath));
  REQUIRE(std::filesystem::exists(accessTargetResolutionPath));
  REQUIRE(std::filesystem::exists(accessLoadHelpersPath));
  REQUIRE(std::filesystem::exists(indexedAccessEmitPath));
  REQUIRE(std::filesystem::exists(callResolutionPath));
  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  REQUIRE(std::filesystem::exists(inlineParamHelpersPath));
  REQUIRE(std::filesystem::exists(uninitializedStructInferencePath));
  REQUIRE(std::filesystem::exists(setupTypeMethodTargetHelpersPath));
  REQUIRE(std::filesystem::exists(countAccessClassifiersPath));
  REQUIRE(std::filesystem::exists(nativeTailDispatchPath));
  REQUIRE(std::filesystem::exists(operatorCollectionMutationHelpersPath));
  REQUIRE(std::filesystem::exists(astCallPathHelpersPath));
  const std::string callHelpersSource = readText(callHelpersPath);
  const std::string accessTargetResolutionSource = readText(accessTargetResolutionPath);
  const std::string accessLoadHelpersSource = readText(accessLoadHelpersPath);
  const std::string indexedAccessEmitSource = readText(indexedAccessEmitPath);
  const std::string callResolutionSource = readText(callResolutionPath);
  const std::string inlineDispatchSource = readText(inlineDispatchPath);
  const std::string inlineParamHelpersSource = readText(inlineParamHelpersPath);
  const std::string uninitializedStructInferenceSource =
      readText(uninitializedStructInferencePath);
  const std::string setupTypeMethodTargetHelpersSource =
      readText(setupTypeMethodTargetHelpersPath);
  const std::string countAccessClassifiersSource = readText(countAccessClassifiersPath);
  const std::string nativeTailDispatchSource = readText(nativeTailDispatchPath);
  const std::string operatorCollectionMutationHelpersSource =
      readText(operatorCollectionMutationHelpersPath);
  const std::string astCallPathHelpersSource = readText(astCallPathHelpersPath);

  CHECK(callHelpersSource.find("const Definition *resolveDefinitionCall(const Expr &callExpr,") ==
        std::string::npos);
  CHECK(callHelpersSource.find("ResolveDefinitionCallFn makeResolveDefinitionCall(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("CallResolutionAdapters makeCallResolutionAdapters(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("EntryCallResolutionSetup buildEntryCallResolutionSetup(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("ResolveExprPathFn makeResolveCallPathFromScope(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("IsTailCallCandidateFn makeIsTailCallCandidate(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("DefinitionExistsFn makeDefinitionExistsByPath(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("std::string resolveCallPathFromScope(") == std::string::npos);
  CHECK(callHelpersSource.find("bool isTailCallCandidate(const Expr &expr,") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool hasTailExecutionCandidate(const std::vector<Expr> &statements,") ==
        std::string::npos);
  CHECK(callHelpersSource.find("ResolvedInlineCallResult emitResolvedInlineDefinitionCall(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("InlineCallDispatchResult tryEmitInlineCallWithCountFallbacks(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("InlineCallDispatchResult tryEmitInlineCallDispatchWithLocals(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool isMapContainsHelperName(const Expr &expr) {") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool isMapTryAtHelperName(const Expr &expr) {") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool isVectorTarget(const Expr &expr, const LocalMap &localsIn) {") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn) {") ==
        std::string::npos);
  CHECK(callHelpersSource.find("UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("MapAccessTargetInfo resolveMapAccessTargetInfo(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,") ==
        std::string::npos);
  CHECK(callHelpersSource.find("NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool resolveValidatedAccessIndexKind(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("MapAccessLookupEmitResult tryEmitMapAccessLookup(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("MapAccessLookupEmitResult tryEmitMapContainsLookup(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("StringTableAccessEmitResult tryEmitStringTableAccessLoad(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo,") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitArrayVectorIndexedAccess(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitBuiltinArrayAccess(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("BufferBuiltinDispatchResult tryEmitBufferBuiltinDispatchWithLocals(") !=
        std::string::npos);
  CHECK(callHelpersSource.find("IrOpcode mapKeyCompareOpcode(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("MapLookupStringKeyResult tryResolveMapLookupStringKey(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("MapLookupKeyLocalEmitResult tryEmitMapLookupStringKeyLocal(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitMapLookupNonStringKeyLocal(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitMapLookupKeyLocal(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitMapLookupTargetPointerLocal(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("MapLookupLoopLocals emitMapLookupLoopSearchScaffold(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("void emitMapLookupAccessEpilogue(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("void emitMapLookupContainsResult(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitMapLookupAccess(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitMapLookupContains(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool emitMapLookupTryAt(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("void emitStringAccessLoad(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("void emitArrayVectorAccessLoad(") ==
        std::string::npos);
  CHECK(callHelpersSource.find("bool validateMapLookupKeyKind(") ==
        std::string::npos);

  CHECK(accessTargetResolutionSource.find("MapAccessTargetInfo resolveMapAccessTargetInfo(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("bool validateMapAccessTargetInfo(const MapAccessTargetInfo &targetInfo,") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("NonLiteralStringAccessTargetResult validateNonLiteralStringAccessTarget(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("bool resolveValidatedAccessIndexKind(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("ArrayVectorAccessTargetInfo resolveArrayVectorAccessTargetInfo(") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("ArrayVectorAccessTargetInfo inferred;") !=
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("MapAccessLookupEmitResult tryEmitMapAccessLookup(") ==
        std::string::npos);
  CHECK(accessTargetResolutionSource.find("bool emitBuiltinArrayAccess(") ==
        std::string::npos);

  CHECK(accessLoadHelpersSource.find("IrOpcode mapKeyCompareOpcode(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("MapLookupStringKeyResult tryResolveMapLookupStringKey(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("MapLookupKeyLocalEmitResult tryEmitMapLookupStringKeyLocal(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool emitMapLookupNonStringKeyLocal(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool emitMapLookupKeyLocal(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool emitMapLookupTargetPointerLocal(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("MapLookupLoopLocals emitMapLookupLoopSearchScaffold(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("void emitMapLookupAccessEpilogue(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("void emitMapLookupContainsResult(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool emitMapLookupAccess(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool emitMapLookupContains(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool emitMapLookupTryAt(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("void emitStringAccessLoad(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("void emitArrayVectorAccessLoad(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("bool validateMapLookupKeyKind(") !=
        std::string::npos);
  CHECK(accessLoadHelpersSource.find("CountMethodFallbackResult tryEmitNonMethodCountFallback(") ==
        std::string::npos);

  CHECK(indexedAccessEmitSource.find("MapAccessLookupEmitResult tryEmitMapAccessLookup(") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("MapAccessLookupEmitResult tryEmitMapContainsLookup(") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("StringTableAccessEmitResult tryEmitStringTableAccessLoad(") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("enum class DynamicStringAccessEmitResult") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("bool validateArrayVectorAccessTargetInfo(const ArrayVectorAccessTargetInfo &targetInfo,") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("bool emitArrayVectorIndexedAccess(") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("bool emitBuiltinArrayAccess(") !=
        std::string::npos);
  CHECK(indexedAccessEmitSource.find("bool emitMapLookupAccess(") ==
        std::string::npos);

  CHECK(callResolutionSource.find("const Definition *resolveDefinitionCall(const Expr &callExpr,") !=
        std::string::npos);
  CHECK(callResolutionSource.find("ResolveDefinitionCallFn makeResolveDefinitionCall(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("CallResolutionAdapters makeCallResolutionAdapters(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("bool validateSemanticProductMethodCallCoverage(const Program &program,") !=
        std::string::npos);
  CHECK(callResolutionSource.find("EntryCallResolutionSetup buildEntryCallResolutionSetup(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("ResolveExprPathFn makeResolveCallPathFromScope(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("IsTailCallCandidateFn makeIsTailCallCandidate(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("DefinitionExistsFn makeDefinitionExistsByPath(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("std::string resolveCallPathFromScope(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("bool isTailCallCandidate(const Expr &expr,") !=
        std::string::npos);
  CHECK(callResolutionSource.find("bool hasTailExecutionCandidate(const std::vector<Expr> &statements,") !=
        std::string::npos);
  CHECK(callResolutionSource.find("bool isMapBuiltinResolvedPath(const Expr &expr, const std::string &resolvedPath)") !=
        std::string::npos);
  CHECK(callResolutionSource.find("normalizeMapImportAliasPath(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("std::string resolveCallPathFromScopeWithoutImportAliases(") ==
        std::string::npos);

  CHECK(inlineDispatchSource.find("bool isMapBuiltinInlinePath(const Expr &expr, const Definition &callee)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isMapContainsHelperName(const Expr &expr)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isMapTryAtHelperName(const Expr &expr)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isVectorTarget(const Expr &expr, const LocalMap &localsIn)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isCanonicalSoaToAosHelperCall(const Expr &expr)") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find(
            "isCanonicalCollectionHelperCall(expr, \"std/collections/soa_vector\", \"to_aos\", 1)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/to_aos") == std::string::npos);
  CHECK(inlineDispatchSource.find("isSimpleCallName(expr, \"to_aos\")") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/get") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/ref") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/count") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/push") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/reserve") == std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "semantics::isExperimentalSoaVectorSpecializedTypePath(structPath)") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "structPath.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "semantics::isExperimentalSoaVectorSpecializedTypePath(normalizedReceiverStruct)") !=
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "normalizedReceiverStruct.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) != 0") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "semantics::isExperimentalSoaVectorTypePath(normalized)") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "normalized == \"SoaVector\" || normalized.rfind(\"SoaVector<\", 0) == 0") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "normalized == \"std/collections/experimental_soa_vector/SoaVector\"") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "normalized.rfind(\"std/collections/experimental_soa_vector/SoaVector<\", 0) == 0") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "normalized.rfind(\"SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "normalized.rfind(\"std/collections/experimental_soa_vector/SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "auto canonicalizeLegacySoaRefHelperPath = [](const std::string &path)") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "semantics::canonicalizeLegacySoaRefHelperPath(samePathCallee->fullPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "const std::string canonicalPath =") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "semantics::canonicalizeLegacySoaRefHelperPath(path)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "semantics::isExperimentalSoaRefLikeHelperPath(canonicalPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "semantics::isExperimentalSoaVectorSpecializedTypePath(structPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "structPath.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "canonicalPath.rfind(\"/std/collections/soa_vector/ref\", 0) == 0") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "canonicalPath.rfind(\"/std/collections/experimental_soa_vector/soaVectorRef\", 0) == 0") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "semantics::isCanonicalSoaRefLikeHelperPath(canonicalPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "path.rfind(\"/std/collections/soa_vector/ref\", 0) == 0") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "samePathCallee->fullPath.rfind(\"/soa_vector/ref\", 0) == 0") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "normalizedMethodName == \"std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "normalizedMethodName == \"soa_vector/ref\"") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find("normalizedMethodName != \"ref\"") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "const std::string methodPath = \"/\" + normalizedMethodName;") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "const std::string canonicalMethodPath =") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "semantics::isLegacyOrCanonicalSoaHelperPath(canonicalMethodPath, \"ref\")") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "methodPath.rfind(\"/std/collections/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("ResolvedInlineCallResult emitResolvedInlineDefinitionCall(") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("InlineCallDispatchResult tryEmitInlineCallWithCountFallbacks(") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("InlineCallDispatchResult tryEmitInlineCallDispatchWithLocals(") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("const auto firstCountFallbackResult = tryEmitNonMethodCountFallback(") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("auto emitCanonicalInlineDefinitionCall = [&](const Expr &callExpr, const Definition &callee) {") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(") ==
        std::string::npos);

  CHECK(countAccessClassifiersSource.find("bool isCanonicalSoaToAosHelperCall(const Expr &expr)") ==
        std::string::npos);
  CHECK(countAccessClassifiersSource.find(
            "isCanonicalCollectionHelperCall(target, \"std/collections/soa_vector\", \"to_aos\", 1)") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("std/collections/soa_vector/to_aos") == std::string::npos);

  CHECK(astCallPathHelpersSource.find("bool isCanonicalCollectionHelperCall(") != std::string::npos);
  CHECK(astCallPathHelpersSource.find("std/collections/soa_vector/to_aos") == std::string::npos);

  CHECK(nativeTailDispatchSource.find("bool getUnsupportedVectorHelperName(const Expr &expr, std::string &helperName)") ==
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("UnsupportedNativeCallResult emitUnsupportedNativeCallDiagnostic(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("NativeCallTailDispatchResult tryEmitNativeCallTailDispatch(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("NativeCallTailDispatchResult tryEmitNativeCallTailDispatchWithLocals(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("const auto countAccessResult = tryEmitCountAccessCall(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("const auto unsupportedCallResult = emitUnsupportedNativeCallDiagnostic(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("if (!emitBuiltinArrayAccess(accessName,") !=
        std::string::npos);
}

TEST_CASE("soa field-view backend cleanup stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                            : std::filesystem::path("..");

  const std::filesystem::path emitterCollectionInferencePath =
      repoRoot / "src" / "emitter" / "EmitterBuiltinCollectionInferenceHelpers.cpp";
  const std::filesystem::path inlineDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererInlineNativeCallDispatch.cpp";
  const std::filesystem::path countAccessClassifiersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCountAccessClassifiers.cpp";
  const std::filesystem::path nativeTailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp";
  REQUIRE(std::filesystem::exists(emitterCollectionInferencePath));
  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  REQUIRE(std::filesystem::exists(countAccessClassifiersPath));
  REQUIRE(std::filesystem::exists(nativeTailDispatchPath));

  const std::string emitterCollectionInferenceSource = readText(emitterCollectionInferencePath);
  const std::string inlineDispatchSource = readText(inlineDispatchPath);
  const std::string countAccessClassifiersSource = readText(countAccessClassifiersPath);
  const std::string nativeTailDispatchSource = readText(nativeTailDispatchPath);

  CHECK(emitterCollectionInferenceSource.find("field_view") == std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("unknown method: /std/collections/soa_vector/field_view/") ==
        std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("unknown method: /std/collections/soa_vector/ref") ==
        std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("soaVectorGet") == std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("soaVectorRef") == std::string::npos);

  CHECK(inlineDispatchSource.find("field_view") == std::string::npos);
  CHECK(inlineDispatchSource.find("unknown method: /std/collections/soa_vector/field_view/") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("unknown method: /std/collections/soa_vector/ref") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("soaVectorGet") == std::string::npos);
  CHECK(inlineDispatchSource.find("soaVectorRef") == std::string::npos);

  CHECK(countAccessClassifiersSource.find("field_view") == std::string::npos);
  CHECK(countAccessClassifiersSource.find("unknown method: /std/collections/soa_vector/field_view/") ==
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("unknown method: /std/collections/soa_vector/ref") ==
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("soaVectorGet") == std::string::npos);
  CHECK(countAccessClassifiersSource.find("soaVectorRef") == std::string::npos);

  CHECK(nativeTailDispatchSource.find("field_view") == std::string::npos);
  CHECK(nativeTailDispatchSource.find("unknown method: /std/collections/soa_vector/field_view/") ==
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("unknown method: /std/collections/soa_vector/ref") ==
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("soaVectorGet") == std::string::npos);
  CHECK(nativeTailDispatchSource.find("soaVectorRef") == std::string::npos);
}

TEST_CASE("vm heap helpers source delegation stays stable") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  };
  auto readTexts = [&](const std::vector<std::filesystem::path> &paths) {
    std::string combined;
    for (const auto &path : paths) {
      combined += readText(path);
      combined.push_back('\n');
    }
    return combined;
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                            : std::filesystem::path("..");

  const std::filesystem::path vmPath = repoRoot / "src" / "Vm.cpp";
  const std::filesystem::path vmExecutionPath = repoRoot / "src" / "VmExecution.cpp";
  const std::filesystem::path vmDebugSessionInstructionPath =
      repoRoot / "src" / "VmDebugSessionInstruction.cpp";
  const std::filesystem::path vmHeapHelpersPath = repoRoot / "src" / "VmHeapHelpers.cpp";
  const std::filesystem::path vmHeapHelpersHeaderPath = repoRoot / "src" / "VmHeapHelpers.h";
  REQUIRE(std::filesystem::exists(vmPath));
  REQUIRE(std::filesystem::exists(vmExecutionPath));
  REQUIRE(std::filesystem::exists(vmDebugSessionInstructionPath));
  REQUIRE(std::filesystem::exists(vmHeapHelpersPath));
  REQUIRE(std::filesystem::exists(vmHeapHelpersHeaderPath));
  const std::string vmSource = readTexts({vmPath, vmExecutionPath, vmDebugSessionInstructionPath});
  const std::string vmHeapHelpersSource = readText(vmHeapHelpersPath);
  const std::string vmHeapHelpersHeaderSource = readText(vmHeapHelpersHeaderPath);

  CHECK(vmSource.find("#include \"VmHeapHelpers.h\"") != std::string::npos);
  CHECK(vmSource.find("constexpr uint64_t kVmHeapAddressTag = 1ull << 63;") ==
        std::string::npos);
  CHECK(vmSource.find("bool isVmHeapAddress(uint64_t address) {") ==
        std::string::npos);
  CHECK(vmSource.find("bool resolveIndirectAddress(uint64_t address,") ==
        std::string::npos);
  CHECK(vmSource.find("bool allocateVmHeapSlots(uint64_t slotCount,") ==
        std::string::npos);
  CHECK(vmSource.find("bool freeVmHeapSlots(uint64_t address,") ==
        std::string::npos);
  CHECK(vmSource.find("bool reallocVmHeapSlots(uint64_t address,") ==
        std::string::npos);
  CHECK(vmSource.find("vm_detail::resolveIndirectAddress(") != std::string::npos);
  CHECK(vmSource.find("vm_detail::allocateVmHeapSlots(") != std::string::npos);
  CHECK(vmSource.find("vm_detail::freeVmHeapSlots(") != std::string::npos);
  CHECK(vmSource.find("vm_detail::reallocVmHeapSlots(") != std::string::npos);

  CHECK(vmHeapHelpersHeaderSource.find("namespace primec::vm_detail {") !=
        std::string::npos);
  CHECK(vmHeapHelpersHeaderSource.find("bool resolveIndirectAddress(") !=
        std::string::npos);
  CHECK(vmHeapHelpersHeaderSource.find("bool allocateVmHeapSlots(") !=
        std::string::npos);
  CHECK(vmHeapHelpersHeaderSource.find("bool freeVmHeapSlots(") !=
        std::string::npos);
  CHECK(vmHeapHelpersHeaderSource.find("bool reallocVmHeapSlots(") !=
        std::string::npos);

  CHECK(vmHeapHelpersSource.find("constexpr uint64_t kVmHeapAddressTag = 1ull << 63;") !=
        std::string::npos);
  CHECK(vmHeapHelpersSource.find("bool isVmHeapAddress(uint64_t address) {") !=
        std::string::npos);
  CHECK(vmHeapHelpersSource.find("bool resolveIndirectAddress(uint64_t address,") !=
        std::string::npos);
  CHECK(vmHeapHelpersSource.find("bool allocateVmHeapSlots(uint64_t slotCount,") !=
        std::string::npos);
  CHECK(vmHeapHelpersSource.find("bool freeVmHeapSlots(uint64_t address,") !=
        std::string::npos);
  CHECK(vmHeapHelpersSource.find("bool reallocVmHeapSlots(uint64_t address,") !=
        std::string::npos);
}

TEST_CASE("ir lowerer effects unit rejects duplicate entry capabilities transform") {
  primec::Definition entryDef;
  entryDef.fullPath = "/main";

  primec::Transform capabilitiesA;
  capabilitiesA.name = "capabilities";
  capabilitiesA.arguments = {"io_out"};
  entryDef.transforms.push_back(capabilitiesA);

  primec::Transform capabilitiesB;
  capabilitiesB.name = "capabilities";
  capabilitiesB.arguments = {"io_err"};
  entryDef.transforms.push_back(capabilitiesB);

  uint64_t entryEffectMask = 0;
  uint64_t entryCapabilityMask = 0;
  std::string error;
  CHECK_FALSE(primec::ir_lowerer::resolveEntryMetadataMasks(
      entryDef, "/main", {"io_out"}, {"io_out"}, entryEffectMask, entryCapabilityMask, error));
  CHECK(error == "duplicate capabilities transform on /main");
}

TEST_CASE("ir lowerer call helpers resolve direct definition calls only") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  auto resolver = [](const primec::Expr &) { return std::string("/callee"); };

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";
  CHECK(primec::ir_lowerer::resolveDefinitionCall(directCall, defMap, resolver) == &callee);

  primec::Expr methodCall = directCall;
  methodCall.isMethodCall = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(methodCall, defMap, resolver) == nullptr);

  primec::Expr bindingCall = directCall;
  bindingCall.isBinding = true;
  CHECK(primec::ir_lowerer::resolveDefinitionCall(bindingCall, defMap, resolver) == nullptr);
}

TEST_CASE("ir lowerer call helpers build definition call resolver") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  const auto resolveExprPath = [](const primec::Expr &) { return std::string("/callee"); };
  const auto resolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(defMap, resolveExprPath);

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";
  CHECK(resolveDefinitionCall(directCall) == &callee);

  primec::Expr methodCall = directCall;
  methodCall.isMethodCall = true;
  CHECK(resolveDefinitionCall(methodCall) == nullptr);

  primec::Expr bindingCall = directCall;
  bindingCall.isBinding = true;
  CHECK(resolveDefinitionCall(bindingCall) == nullptr);
}

TEST_CASE("ir lowerer call helpers resolve definition paths") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionByPath(defMap, "/callee") == &callee);
  CHECK(primec::ir_lowerer::resolveDefinitionByPath(defMap, "/missing") == nullptr);
  CHECK(primec::ir_lowerer::resolveDefinitionByPath(defMap, "/null") == nullptr);
}

TEST_CASE("ir lowerer call helpers resolve scoped call paths") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"foo", "/import/foo"},
      {"bar", "/import/bar"},
      {"map_count", "std/collections/map/count"},
      {"map_at", "map/at"},
  };

  primec::Expr absolute;
  absolute.name = "/absolute";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(absolute, defMap, importAliases) == "/absolute");

  primec::Expr namespacedScoped;
  namespacedScoped.name = "foo";
  namespacedScoped.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(namespacedScoped, defMap, importAliases) == "/pkg/foo");

  primec::Expr namespacedAlias;
  namespacedAlias.name = "bar";
  namespacedAlias.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(namespacedAlias, defMap, importAliases) == "/import/bar");

  primec::Expr namespacedFallback;
  namespacedFallback.name = "baz";
  namespacedFallback.namespacePrefix = "/pkg";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(namespacedFallback, defMap, importAliases) == "/pkg/baz");

  primec::Expr rootAlias;
  rootAlias.name = "foo";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(rootAlias, defMap, importAliases) == "/import/foo");

  primec::Expr rootFallback;
  rootFallback.name = "main";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(rootFallback, defMap, importAliases) == "/main");

  primec::Expr slashlessCanonicalMapAlias;
  slashlessCanonicalMapAlias.name = "map_count";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(slashlessCanonicalMapAlias, defMap, importAliases) ==
        "/std/collections/map/count");

  primec::Expr slashlessMapAlias;
  slashlessMapAlias.name = "map_at";
  CHECK(primec::ir_lowerer::resolveCallPathFromScope(slashlessMapAlias, defMap, importAliases) == "/map/at");
}

TEST_CASE("ir lowerer call helpers build scoped call path resolver") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"foo", "/import/foo"},
      {"bar", "/import/bar"},
      {"map_count", "std/collections/map/count"},
      {"map_at", "map/at"},
  };
  auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);

  primec::Expr namespacedScoped;
  namespacedScoped.name = "foo";
  namespacedScoped.namespacePrefix = "/pkg";
  CHECK(resolveExprPath(namespacedScoped) == "/pkg/foo");

  primec::Expr namespacedAlias;
  namespacedAlias.name = "bar";
  namespacedAlias.namespacePrefix = "/pkg";
  CHECK(resolveExprPath(namespacedAlias) == "/import/bar");

  primec::Expr slashlessCanonicalMapAlias;
  slashlessCanonicalMapAlias.name = "map_count";
  CHECK(resolveExprPath(slashlessCanonicalMapAlias) == "/std/collections/map/count");

  primec::Expr slashlessMapAlias;
  slashlessMapAlias.name = "map_at";
  CHECK(resolveExprPath(slashlessMapAlias) == "/map/at");
}

TEST_CASE("ir lowerer call helpers keep alias fallback only on raw path") {
  primec::Definition scopedDef;
  scopedDef.fullPath = "/pkg/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/pkg/foo", &scopedDef}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"bar", "/import/bar"},
      {"map_count", "std/collections/map/count"},
  };

  const auto rawResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);

  primec::SemanticProgram semanticProgram;
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);

  primec::Expr namespacedAlias;
  namespacedAlias.name = "bar";
  namespacedAlias.namespacePrefix = "/pkg";
  CHECK(rawResolveExprPath(namespacedAlias) == "/import/bar");
  CHECK(semanticResolveExprPath(namespacedAlias) == "/pkg/bar");

  primec::Expr slashlessCanonicalMapAlias;
  slashlessCanonicalMapAlias.name = "map_count";
  CHECK(rawResolveExprPath(slashlessCanonicalMapAlias) == "/std/collections/map/count");
  CHECK(semanticResolveExprPath(slashlessCanonicalMapAlias) == "/map_count");
}

TEST_CASE("ir lowerer call helpers avoid semantic-product scope/root fallback probes") {
  primec::Definition rootDef;
  rootDef.fullPath = "/foo";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/foo", &rootDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);

  primec::Expr namespacedExpr;
  namespacedExpr.kind = primec::Expr::Kind::Name;
  namespacedExpr.name = "foo";
  namespacedExpr.namespacePrefix = "/pkg";
  CHECK(semanticResolveExprPath(namespacedExpr) == "/pkg/foo");
}

TEST_CASE("ir lowerer call helpers require semantic-product direct-call targets") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"callee", "/callee"},
  };

  primec::SemanticProgram semanticProgram;
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 17;
  CHECK(semanticResolveExprPath(callExpr).empty());

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 17,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/callee"),
  });
  const auto populatedTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto populatedResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, populatedTargets);
  CHECK(populatedResolveExprPath(callExpr) == "/callee");
}

TEST_CASE("ir lowerer call helpers remap unresolved rooted semantic operator targets through imports") {
  primec::Definition importedMultiply;
  importedMultiply.fullPath = "/std/math/multiply";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/math/multiply", &importedMultiply},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"multiply", "/std/math/multiply"},
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "multiply";
  callExpr.semanticNodeId = 71;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "multiply",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 71,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/multiply"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);

  CHECK(resolveExprPath(callExpr) == "/std/math/multiply");
}

TEST_CASE("ir lowerer call helpers prefer rooted rewritten direct-call paths over stale semantic-product targets") {
  primec::Definition legacyRootedCall;
  legacyRootedCall.fullPath = "/legacy";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/legacy", &legacyRootedCall},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/legacy";
  callExpr.semanticNodeId = 19;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "/legacy",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 19,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/semantic/target"),
  });
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);

  CHECK(resolveExprPath(callExpr) == "/legacy");
}

TEST_CASE("ir lowerer call helpers keep rewritten rooted direct-call paths without semantic targets") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::Expr rewrittenExpr;
  rewrittenExpr.kind = primec::Expr::Kind::Call;
  rewrittenExpr.name = "/operator/add";

  primec::SemanticProgram semanticProgram;
  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);

  CHECK(resolveExprPath(rewrittenExpr) == "/operator/add");
}

TEST_CASE("ir lowerer call helpers require semantic-product method-call targets") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.name = "contains";
  methodExpr.semanticNodeId = 44;

  primec::SemanticProgram semanticProgram;
  auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(resolveExprPath(methodExpr).empty());

  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 44,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
  });
  semanticTargets = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(resolveExprPath(methodExpr) == "/std/collections/map/contains");
}

TEST_CASE("ir lowerer semantic-product adapter reuses method-call path ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 44,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 45,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.semanticProgram == &semanticProgram);
  REQUIRE(adapter.semanticIndex.methodCallTargetIdsByExpr.count(44) == 1);
  REQUIRE(adapter.semanticIndex.methodCallTargetIdsByExpr.count(45) == 1);
  CHECK(adapter.semanticIndex.methodCallTargetIdsByExpr.at(44) ==
        adapter.semanticIndex.methodCallTargetIdsByExpr.at(45));
  CHECK(adapter.semanticIndex.methodCallTargetIdsByExpr.at(44) ==
        semanticProgram.methodCallTargets[0].resolvedPathId);

  primec::Expr firstExpr;
  firstExpr.kind = primec::Expr::Kind::Call;
  firstExpr.isMethodCall = true;
  firstExpr.semanticNodeId = 44;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, firstExpr) ==
        "/std/collections/map/contains");

  primec::Expr secondExpr;
  secondExpr.kind = primec::Expr::Kind::Call;
  secondExpr.isMethodCall = true;
  secondExpr.semanticNodeId = 45;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, secondExpr) ==
        "/std/collections/map/contains");
}

TEST_CASE("ir lowerer semantic-product adapter ignores method-call targets missing resolved path ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 144,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId = primec::InvalidSymbolId,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "contains",
      .receiverTypeText = "",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 145,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.methodCallTargetIdsByExpr.count(144) == 0);
  CHECK(adapter.semanticIndex.methodCallTargetIdsByExpr.count(145) == 1);

  primec::Expr missingPathExpr;
  missingPathExpr.kind = primec::Expr::Kind::Call;
  missingPathExpr.isMethodCall = true;
  missingPathExpr.semanticNodeId = 144;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, missingPathExpr).empty());

  primec::Expr validExpr;
  validExpr.kind = primec::Expr::Kind::Call;
  validExpr.isMethodCall = true;
  validExpr.semanticNodeId = 145;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, validExpr) ==
        "/std/collections/map/contains");
}

TEST_CASE("ir lowerer semantic-product adapter indexes callable summaries by full-path id") {
  primec::SemanticProgram semanticProgram;
  const auto mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 201,
      .provenanceHandle = 0,
      .fullPathId = mainPathId,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 202,
      .provenanceHandle = 0,
      .fullPathId = primec::InvalidSymbolId,
  });
  semanticProgram.callableSummaries.push_back(primec::SemanticProgramCallableSummary{
      .isExecution = false,
      .returnKind = "void",
      .isCompute = false,
      .isUnsafe = false,
      .activeEffects = {},
      .activeCapabilities = {},
      .hasResultType = false,
      .resultTypeHasValue = false,
      .resultValueType = "",
      .resultErrorType = "",
      .hasOnError = false,
      .onErrorHandlerPath = "",
      .onErrorErrorType = "",
      .onErrorBoundArgCount = 0,
      .semanticNodeId = 203,
      .provenanceHandle = 0,
      .fullPathId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.callableSummariesByPathId.count(mainPathId) == 1);
  CHECK(adapter.callableSummariesByPathId.count(primec::InvalidSymbolId) == 0);
  CHECK(adapter.callableSummariesByPathId.count(
            static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u)) == 0);
  const auto *summary = primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/main");
  REQUIRE(summary != nullptr);
  CHECK(summary->semanticNodeId == 201);
  CHECK(primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/missing") == nullptr);
}

TEST_CASE("ir lowerer call helpers require semantic-product bridge-path choices") {
  const std::unordered_map<std::string, const primec::Definition *> defMap;
  const std::unordered_map<std::string, std::string> importAliases;

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count";
  callExpr.semanticNodeId = 18;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "count",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 18,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });
  auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(semanticResolveExprPath(callExpr) == "/vector/count");

  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 18,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });
  semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(semanticResolveExprPath(callExpr) == "/vector/count");
}

TEST_CASE("ir lowerer semantic-product adapter ignores bridge-path choices with missing or invalid helper name ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 118,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::InvalidSymbolId,
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 119,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "count"),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "vector",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 120,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
      .helperNameId =
          static_cast<primec::SymbolId>(semanticProgram.callTargetStringTable.size() + 1u),
      .chosenPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/vector/count"),
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.bridgePathChoiceIdsByExpr.count(118) == 0);
  CHECK(adapter.semanticIndex.bridgePathChoiceIdsByExpr.count(119) == 1);
  CHECK(adapter.semanticIndex.bridgePathChoiceIdsByExpr.count(120) == 0);

  primec::Expr missingHelperExpr;
  missingHelperExpr.kind = primec::Expr::Kind::Call;
  missingHelperExpr.semanticNodeId = 118;
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, missingHelperExpr).empty());

  primec::Expr validExpr;
  validExpr.kind = primec::Expr::Kind::Call;
  validExpr.semanticNodeId = 119;
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, validExpr) == "/vector/count");

  primec::Expr invalidHelperIdExpr;
  invalidHelperIdExpr.kind = primec::Expr::Kind::Call;
  invalidHelperIdExpr.semanticNodeId = 120;
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, invalidHelperIdExpr).empty());
}

TEST_CASE("ir lowerer semantic-product adapter joins facts by semantic id with return-path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/renamed_main";
  mainDef.semanticNodeId = 61;

  primec::Expr localAutoExpr;
  localAutoExpr.kind = primec::Expr::Kind::Call;
  localAutoExpr.name = "select";
  localAutoExpr.semanticNodeId = 62;

  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 63;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.semanticNodeId = 64;

  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 61,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/legacy_main"),
  });
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "selected",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "i32",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "return",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 10,
      .sourceColumn = 5,
      .semanticNodeId = 62,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 63,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 12,
      .sourceColumn = 9,
      .semanticNodeId = 64,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const auto *returnFact = primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, mainDef);
  REQUIRE(returnFact != nullptr);
  CHECK(primec::semanticProgramReturnFactDefinitionPath(semanticProgram, *returnFact) ==
        "/legacy_main");

  primec::Definition legacyFixtureDef;
  legacyFixtureDef.fullPath = "/legacy_main";
  const auto *legacyReturnFact =
      primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, legacyFixtureDef);
  REQUIRE(legacyReturnFact != nullptr);
  CHECK(legacyReturnFact == returnFact);
  CHECK(primec::semanticProgramReturnFactDefinitionPath(semanticProgram, *legacyReturnFact) ==
        "/legacy_main");

  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(semanticTargets, localAutoExpr);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->bindingName == "selected");

  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(semanticTargets, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(primec::semanticProgramQueryFactResolvedPath(semanticProgram, *queryFact) == "/lookup");

  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(semanticTargets, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->onErrorHandlerPath == "/handler");
}

TEST_CASE("ir lowerer semantic-product adapter indexes return facts by definition path id") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 0;

  primec::SemanticProgram semanticProgram;
  semanticProgram.returnFacts.push_back(primec::SemanticProgramReturnFact{
      .returnKind = "return",
      .structPath = "/i32",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 9,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
  });

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  CHECK(semanticTargets.semanticIndex.returnFactsByDefinitionId.empty());
  CHECK(semanticTargets.semanticIndex.returnFactsByDefinitionPathId.count(*mainPathId) == 1);
  const auto *returnFact = primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, mainDef);
  REQUIRE(returnFact != nullptr);
  CHECK(returnFact->definitionPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramReturnFactDefinitionPath(semanticProgram, *returnFact) == "/main");
}

TEST_CASE("ir lowerer semantic-product adapter indexes on_error facts by definition path id") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 0;

  primec::SemanticProgram semanticProgram;
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "value"),
      },
  });

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  CHECK(semanticTargets.semanticIndex.onErrorFactsByDefinitionId.empty());
  CHECK(semanticTargets.semanticIndex.onErrorFactsByDefinitionPathId.count(*mainPathId) == 1);
  const auto *onErrorFact = primec::ir_lowerer::findSemanticProductOnErrorFact(semanticTargets, mainDef);
  REQUIRE(onErrorFact != nullptr);
  CHECK(primec::semanticProgramOnErrorFactDefinitionPath(semanticProgram, *onErrorFact) == "/main");
  CHECK(primec::semanticProgramOnErrorFactHandlerPath(semanticProgram, *onErrorFact) == "/handler");
}

TEST_CASE("ir lowerer semantic-product adapter indexes local-auto facts by initializer path and binding name") {
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "id";
  initCall.semanticNodeId = 7101;

  primec::Expr localBinding;
  localBinding.kind = primec::Expr::Kind::Name;
  localBinding.isBinding = true;
  localBinding.name = "value";
  localBinding.semanticNodeId = 0;
  localBinding.args = {initCall};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 12,
      .sourceColumn = 5,
      .semanticNodeId = 7101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  const primec::SymbolId bindingNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "value");
  const primec::SymbolId initializerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/id");
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.localAutoFactsByExpr.empty());
  CHECK(adapter.semanticIndex.localAutoFactsByInitPathAndBindingNameId.count(
            (static_cast<uint64_t>(initializerPathId) << 32) | static_cast<uint64_t>(bindingNameId)) == 1);
  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(adapter, localBinding);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->bindingTypeText == "i32");
  CHECK(primec::semanticProgramLocalAutoFactInitializerResolvedPath(semanticProgram, *localAutoFact) == "/id");
}

TEST_CASE("ir lowerer semantic-product adapter prefers local-auto semantic-id matches over initializer-path index") {
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "id";
  initCall.semanticNodeId = 7201;

  primec::Expr localBinding;
  localBinding.kind = primec::Expr::Kind::Name;
  localBinding.isBinding = true;
  localBinding.name = "value";
  localBinding.semanticNodeId = 7202;
  localBinding.args = {initCall};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 16,
      .sourceColumn = 5,
      .semanticNodeId = 7201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });

  const primec::SymbolId bindingNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "value");
  const primec::SymbolId initializerPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/id");

  primec::SemanticProgramLocalAutoFact semanticIdFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "i32",
      .initializerBindingTypeText = "i32",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 16,
      .sourceColumn = 3,
      .semanticNodeId = 7202,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  };
  semanticProgram.localAutoFacts.push_back(std::move(semanticIdFact));

  primec::SemanticProgramLocalAutoFact fallbackFact{
      .scopePath = "/main",
      .bindingName = "value",
      .bindingTypeText = "f64",
      .initializerBindingTypeText = "f64",
      .initializerReceiverBindingTypeText = "",
      .initializerQueryTypeText = "",
      .initializerResultHasValue = false,
      .initializerResultValueType = "",
      .initializerResultErrorType = "",
      .initializerHasTry = false,
      .initializerTryOperandResolvedPath = "",
      .initializerTryOperandBindingTypeText = "",
      .initializerTryOperandReceiverBindingTypeText = "",
      .initializerTryOperandQueryTypeText = "",
      .initializerTryValueType = "",
      .initializerTryErrorType = "",
      .initializerTryContextReturnKind = "",
      .initializerTryOnErrorHandlerPath = "",
      .initializerTryOnErrorErrorType = "",
      .initializerTryOnErrorBoundArgCount = 0,
      .sourceLine = 16,
      .sourceColumn = 3,
      .semanticNodeId = 0,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  };
  semanticProgram.localAutoFacts.push_back(std::move(fallbackFact));

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(adapter, localBinding);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->semanticNodeId == 7202);
  CHECK(localAutoFact->bindingTypeText == "i32");
}

TEST_CASE("ir lowerer semantic-product adapter indexes query facts by resolved path and call name") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 8101;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 15,
      .sourceColumn = 5,
      .semanticNodeId = 8101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 15,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.queryFactsByExpr.empty());
  CHECK(adapter.semanticIndex.queryFactsByResolvedPathAndCallNameId.count(
            (static_cast<uint64_t>(resolvedPathId) << 32) | static_cast<uint64_t>(callNameId)) == 1);
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->resolvedPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramQueryFactResolvedPath(semanticProgram, *queryFact) == "/lookup");
}

TEST_CASE("ir lowerer semantic-product adapter prefers query semantic-id matches over resolved-path index") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.semanticNodeId = 8202;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 17,
      .sourceColumn = 5,
      .semanticNodeId = 8202,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });

  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");

  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 17,
      .sourceColumn = 5,
      .semanticNodeId = 8202,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<f64, FileError>",
      .bindingTypeText = "Result<f64, FileError>",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "f64",
      .resultErrorType = "FileError",
      .sourceLine = 17,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->semanticNodeId == 8202);
  CHECK(queryFact->queryTypeText == "Result<i32, FileError>");
}

TEST_CASE("ir lowerer semantic-product adapter resolves try facts by operand path and source fallback") {
  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 9101;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 0;
  tryExpr.sourceLine = 33;
  tryExpr.sourceColumn = 9;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 9101,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 0,
      .operandResolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(adapter, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->operandResolvedPathId != primec::InvalidSymbolId);
  CHECK(primec::semanticProgramTryFactOperandResolvedPath(semanticProgram, *tryFact) == "/lookup");
}

TEST_CASE("ir lowerer semantic-product adapter prefers try semantic-id matches over operand-path fallback") {
  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 9201;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 9202;
  tryExpr.sourceLine = 41;
  tryExpr.sourceColumn = 11;

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 41,
      .sourceColumn = 11,
      .semanticNodeId = 9201,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });

  const primec::SymbolId operandResolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");

  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<i32, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<i32, FileError>",
      .valueType = "i32",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 41,
      .sourceColumn = 11,
      .semanticNodeId = 9202,
      .operandResolvedPathId = operandResolvedPathId,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      .scopePath = "/main",
      .operandBindingTypeText = "Result<f64, FileError>",
      .operandReceiverBindingTypeText = "",
      .operandQueryTypeText = "Result<f64, FileError>",
      .valueType = "f64",
      .errorType = "FileError",
      .contextReturnKind = "return",
      .onErrorHandlerPath = "/handler",
      .onErrorErrorType = "FileError",
      .onErrorBoundArgCount = 1,
      .sourceLine = 41,
      .sourceColumn = 11,
      .semanticNodeId = 0,
      .operandResolvedPathId = operandResolvedPathId,
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(adapter, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->semanticNodeId == 9202);
  CHECK(tryFact->valueType == "i32");
}

TEST_CASE("ir lowerer call helpers resolve definition calls through slashless map import aliases") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/map/count", &canonicalMapCountDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"count_alias", "std/collections/map/count"},
  };
  const auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "count_alias";
  CHECK(primec::ir_lowerer::resolveDefinitionCall(callExpr, defMap, resolveExprPath) == &canonicalMapCountDef);
}

TEST_CASE("ir lowerer call helpers prefer rooted rewritten helper paths over stale semantic direct-call targets") {
  primec::Definition quatMultiplyDef;
  quatMultiplyDef.fullPath = "/std/math/quat_multiply_internal";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/math/quat_multiply_internal", &quatMultiplyDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "multiply",
      .sourceLine = 12,
      .sourceColumn = 3,
      .semanticNodeId = 42,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/multiply"),
  });

  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(
          defMap,
          importAliases,
          primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram));

  primec::Expr rewrittenExpr;
  rewrittenExpr.kind = primec::Expr::Kind::Call;
  rewrittenExpr.name = "/std/math/quat_multiply_internal";
  rewrittenExpr.semanticNodeId = 42;

  CHECK(resolveExprPath(rewrittenExpr) == "/std/math/quat_multiply_internal");
  CHECK(primec::ir_lowerer::resolveDefinitionCall(rewrittenExpr, defMap, resolveExprPath) == &quatMultiplyDef);
}

TEST_CASE("ir lowerer call helpers resolve definition namespace prefixes") {
  primec::Definition namespacedDef;
  namespacedDef.fullPath = "/pkg/foo";
  namespacedDef.namespacePrefix = "/pkg";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/pkg/foo", &namespacedDef},
      {"/pkg/null", nullptr},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/foo") == "/pkg");
  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/missing").empty());
  CHECK(primec::ir_lowerer::resolveDefinitionNamespacePrefix(defMap, "/pkg/null").empty());
}

TEST_CASE("ir lowerer call helpers classify tail call candidates") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};
  const std::unordered_map<std::string, std::string> importAliases = {};
  auto resolveExprPath = [&](const primec::Expr &expr) {
    return primec::ir_lowerer::resolveCallPathFromScope(expr, defMap, importAliases);
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(primec::ir_lowerer::isTailCallCandidate(callExpr, defMap, resolveExprPath));

  primec::Expr methodCall = callExpr;
  methodCall.isMethodCall = true;
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(methodCall, defMap, resolveExprPath));

  primec::Expr nameExpr;
  nameExpr.kind = primec::Expr::Kind::Name;
  nameExpr.name = "callee";
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(nameExpr, defMap, resolveExprPath));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(unknownCall, defMap, resolveExprPath));
}

TEST_CASE("ir lowerer call helpers build tail-call and definition-exists adapters") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  auto resolveExprPath = primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases);
  auto isTailCallCandidate = primec::ir_lowerer::makeIsTailCallCandidate(defMap, resolveExprPath);
  auto definitionExists = primec::ir_lowerer::makeDefinitionExistsByPath(defMap);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(isTailCallCandidate(callExpr));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(isTailCallCandidate(unknownCall));

  CHECK(definitionExists("/callee"));
  CHECK_FALSE(definitionExists("/missing"));
  CHECK_FALSE(definitionExists("/null"));
}

TEST_CASE("ir lowerer call helpers build bundled call-resolution adapters") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/callee", &callee},
      {"/null", nullptr},
  };
  const std::unordered_map<std::string, std::string> importAliases = {{"callee", "/callee"}};

  auto adapters = primec::ir_lowerer::makeCallResolutionAdapters(defMap, importAliases);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  CHECK(adapters.resolveExprPath(callExpr) == "/callee");
  CHECK(adapters.isTailCallCandidate(callExpr));

  primec::Expr unknownCall = callExpr;
  unknownCall.name = "missing";
  CHECK_FALSE(adapters.isTailCallCandidate(unknownCall));

  CHECK(adapters.definitionExists("/callee"));
  CHECK_FALSE(adapters.definitionExists("/missing"));
  CHECK_FALSE(adapters.definitionExists("/null"));
}

TEST_CASE("ir lowerer call helpers detect tail execution candidates from statements") {
  auto isTailCandidate = [](const primec::Expr &expr) {
    return expr.kind == primec::Expr::Kind::Call && expr.name == "callee";
  };

  std::vector<primec::Expr> statements;
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));

  primec::Expr directTail;
  directTail.kind = primec::Expr::Kind::Call;
  directTail.name = "callee";
  statements = {directTail};
  CHECK(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, false, isTailCandidate));

  primec::Expr returnCall;
  returnCall.kind = primec::Expr::Kind::Call;
  returnCall.name = "return";
  returnCall.args = {directTail};
  statements = {returnCall};
  CHECK(primec::ir_lowerer::hasTailExecutionCandidate(statements, false, isTailCandidate));

  primec::Expr nonTail;
  nonTail.kind = primec::Expr::Kind::Name;
  nonTail.name = "value";
  statements = {nonTail};
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(statements, true, isTailCandidate));
}

TEST_SUITE_END();
