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
  const std::filesystem::path countAccessHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCountAccessHelpers.cpp";
  const std::filesystem::path nativeTailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp";
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";
  const std::filesystem::path operatorCollectionMutationHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererOperatorCollectionMutationHelpers.cpp";
  const std::filesystem::path operatorMemoryPointerHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererOperatorMemoryPointerHelpers.cpp";
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
  REQUIRE(std::filesystem::exists(countAccessHelpersPath));
  REQUIRE(std::filesystem::exists(nativeTailDispatchPath));
  REQUIRE(std::filesystem::exists(tailDispatchPath));
  REQUIRE(std::filesystem::exists(operatorCollectionMutationHelpersPath));
  REQUIRE(std::filesystem::exists(operatorMemoryPointerHelpersPath));
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
  const std::string countAccessHelpersSource = readText(countAccessHelpersPath);
  const std::string nativeTailDispatchSource = readText(nativeTailDispatchPath);
  const std::string tailDispatchSource = readText(tailDispatchPath);
  const std::string operatorCollectionMutationHelpersSource =
      readText(operatorCollectionMutationHelpersPath);
  const std::string operatorMemoryPointerHelpersSource =
      readText(operatorMemoryPointerHelpersPath);
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
  CHECK(callResolutionSource.find("bool isMapBuiltinResolvedPath(const SemanticProgram *semanticProgram,") !=
        std::string::npos);
  CHECK(callResolutionSource.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("isBuiltinPublishedMapHelperName(expr, semanticHelperName)") !=
        std::string::npos);
  CHECK(callResolutionSource.find("return matchesResolvedPath(\"/std/collections/map/contains\") ||") !=
        std::string::npos);
  CHECK(callResolutionSource.find("return matchesResolvedPath(\"/std/collections/map/tryAt\") ||") !=
        std::string::npos);
  CHECK(callResolutionSource.find("bool isPublishedCollectionBridgeStdlibSurfaceId(") !=
        std::string::npos);
  CHECK(callResolutionSource.find("findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(callResolutionSource.find("findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(callResolutionSource.find("StdlibSurfaceId::CollectionsVectorHelpers") !=
        std::string::npos);
  CHECK(callResolutionSource.find("StdlibSurfaceId::CollectionsMapHelpers") !=
        std::string::npos);
  CHECK(callResolutionSource.find("StdlibSurfaceId::CollectionsSoaVectorHelpers") !=
        std::string::npos);
  CHECK(callResolutionSource.find("normalizeMapImportAliasPath(") !=
        std::string::npos);
  CHECK(callResolutionSource.find(
            "const std::string rawPath = resolveCallPathWithoutSemanticFallbackProbes(expr);") !=
        std::string::npos);
  CHECK(callResolutionSource.find("const bool hasSemanticRootedRewrite =") !=
        std::string::npos);
  CHECK(callResolutionSource.find("const size_t rawLeafStart = rawPath.find_last_of('/');") !=
        std::string::npos);
  CHECK(callResolutionSource.find("const bool hasGeneratedRootedRawPath =") !=
        std::string::npos);
  CHECK(callResolutionSource.find(
            "rawPath.find(\"__t\", rawLeafStart == std::string::npos ? 0 : rawLeafStart + 1) !=") !=
        std::string::npos);
  CHECK(callResolutionSource.find(
            "rawPath.find(\"__ov\", rawLeafStart == std::string::npos ? 0 : rawLeafStart + 1) !=") !=
        std::string::npos);
  CHECK(callResolutionSource.find("rawPath.front() == '/' &&") !=
        std::string::npos);
  CHECK(callResolutionSource.find("resolved.front() == '/' &&") !=
        std::string::npos);
  CHECK(callResolutionSource.find("(!hasSemanticRootedRewrite || hasGeneratedRootedRawPath)") !=
        std::string::npos);
  CHECK(callResolutionSource.find("rawPath.rfind(\"/map/\", 0) != 0 &&") !=
        std::string::npos);
  CHECK(callResolutionSource.find("return normalizeCollectionHelperPath(rawPath) ==") !=
        std::string::npos);
  CHECK(callResolutionSource.find("std::string resolveCallPathFromScopeWithoutImportAliases(") ==
        std::string::npos);
  CHECK(callResolutionSource.find("!isBridgeHelperCall(semanticProgram, expr, resolvedPath) &&") !=
        std::string::npos);
  CHECK(callResolutionSource.find("isResidualBridgeHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(callResolutionSource.find("isResidualBridgeHelperPath(fallbackResolvedPath)") ==
        std::string::npos);
  CHECK(callResolutionSource.find(
            "!isResidualBridgeHelperPath(resolvedPath)) {\n"
            "        return resolvedPath;\n"
            "      }\n"
            "      return resolveCallPathWithoutSemanticFallbackProbes(expr);") !=
        std::string::npos);
  CHECK(callResolutionSource.find("!isResolvedBridgeHelperPath(resolvedPath) &&") ==
        std::string::npos);
  CHECK(callResolutionSource.find("isResolvedBridgeHelperPath(fallbackResolvedPath) &&") ==
        std::string::npos);

  CHECK(inlineDispatchSource.find("bool isMapBuiltinInlinePath(const Expr &expr, const Definition &callee)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("#include \"primec/StdlibSurfaceRegistry.h\"") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("resolvePublishedInlineMapHelperName(callee.fullPath, resolvedHelperName)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("findStdlibSurfaceMetadataByResolvedPath(resolvedPath)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("metadata->id != StdlibSurfaceId::CollectionsMapHelpers") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("matchesInlineMapHelperFamily(aliasName, resolvedHelperName)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("isInlineMapBuiltinHelperName(resolvedHelperName)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find(
            "    const std::string scopedExprPath = resolveInlineCallPathWithoutFallbackProbes(expr);") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find(
            "    if (isExplicitMapContainsOrTryAtMethodPath(scopedExprPath) &&\n"
            "        normalizeCollectionHelperPath(scopedExprPath) ==") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("    std::string normalizedName = scopedExprPath;") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isMapContainsHelperName(const Expr &expr)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isMapTryAtHelperName(const Expr &expr)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("matchesHelper(\"/std/collections/mapContains\")") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("matchesHelper(\"/std/collections/experimental_map/mapContainsRef\")") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("matchesHelper(\"/std/collections/mapTryAt\")") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("matchesHelper(\"/std/collections/mapInsert\")") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isVectorTarget(const Expr &expr, const LocalMap &localsIn)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("return !hasNamedArguments(expr.argNames);") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isSoaVectorTarget(const Expr &expr, const LocalMap &localsIn)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("bool isCanonicalSoaToAosHelperCall(const Expr &expr)") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find(
            "isCanonicalCollectionHelperCall(expr, \"std/collections/soa_vector\", \"to_aos\", 1)") !=
        std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/to_aos") != std::string::npos);
  CHECK(inlineDispatchSource.find("isSimpleCallName(expr, \"to_aos\")") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/get") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/ref") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/count") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/push") == std::string::npos);
  CHECK(inlineDispatchSource.find("std/collections/soa_vector/reserve") == std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "soa_paths::isExperimentalSoaVectorSpecializedTypePath(structPath)") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "structPath.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) == 0") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/count\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/get\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/get_ref\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/ref\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/ref_ref\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/to_aos\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find(
            "matchesPath(\"/std/collections/soa_vector/to_aos_ref\")") !=
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/count\")") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/get\")") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/get_ref\")") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/ref\")") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/ref_ref\")") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/to_aos\")") ==
        std::string::npos);
  CHECK(inlineParamHelpersSource.find("matchesPath(\"/soa_vector/to_aos_ref\")") ==
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "soa_paths::isExperimentalSoaVectorSpecializedTypePath(normalizedReceiverStruct)") !=
        std::string::npos);
  CHECK(uninitializedStructInferenceSource.find(
            "normalizedReceiverStruct.rfind(\"/std/collections/experimental_soa_vector/SoaVector__\", 0) != 0") ==
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "auto isRawBuiltinSoaVectorReceiverTarget = [&](const std::string &candidate)") !=
        std::string::npos);
  CHECK(setupTypeMethodTargetHelpersSource.find(
            "semantics::isExperimentalSoaVectorTypePath(normalized)") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "auto canonicalizeLegacySoaRefHelperPath = [](const std::string &path)") ==
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "soa_paths::canonicalizeLegacySoaRefHelperPath(samePathCallee->fullPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "const std::string canonicalPath =") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "soa_paths::canonicalizeLegacySoaRefHelperPath(path)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "soa_paths::isExperimentalSoaRefLikeHelperPath(canonicalPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "soa_paths::isExperimentalSoaVectorSpecializedTypePath(structPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "std::string normalizedInternalSoaStorageLeaf(std::string structPath)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "areCompatibleInternalSoaStoragePaths(lhs, rhs)") !=
        std::string::npos);
  CHECK(operatorCollectionMutationHelpersSource.find(
            "structPath.rfind(\"/std/collections/internal_soa_storage/\", 0) == 0") ==
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
            "soa_paths::isCanonicalSoaRefLikeHelperPath(canonicalPath)") !=
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
            "soa_paths::isLegacyOrCanonicalSoaHelperPath(canonicalMethodPath, \"ref\")") !=
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
  CHECK(tailDispatchSource.find("auto stripGeneratedLeafSuffix = [](std::string helperPath)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("return findByPath(canonicalPath);") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("bool isExplicitPublishedVectorCountCall(const Expr &expr)") !=
        std::string::npos);
  CHECK(countAccessHelpersSource.find("isVectorCountTarget(expr.args.front(), localsIn)") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("bool isExplicitDirectVectorCountCall(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("!isExplicitDirectVectorCountCall(semanticProgram, expr) &&") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find(
            "!expr.isMethodCall && count_access_detail::isVectorBuiltinName(expr, \"count\")") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("!isNamedArgumentVectorTemporary(expr.args.front()))") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("bool isVectorBuiltinName(const Expr &expr, const char *name)") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("isExplicitRemovedCountLikeAliasCall(expr, name)") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("resolveVectorHelperAliasName(expr, aliasName)") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("aliasName == name") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("isExplicitPublishedVectorCountCall(expr)") ==
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("resolvePublishedNativeTailHelperName(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("const auto unsupportedCallResult = emitUnsupportedNativeCallDiagnostic(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("if (!emitBuiltinArrayAccess(accessName,") !=
        std::string::npos);
  CHECK(operatorMemoryPointerHelpersSource.find("getBuiltinArrayAccessName(candidate, accessName)") !=
        std::string::npos);
  CHECK(operatorMemoryPointerHelpersSource.find("candidate.name == \"at\" || candidate.name == \"at_unsafe\"") ==
        std::string::npos);
}

TEST_CASE("native tail and late collection helper metadata dispatch stays source locked") {
  auto readText = [](const std::filesystem::path &path) {
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
      return std::string{};
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  };
  const std::filesystem::path repoRoot =
      std::filesystem::exists(std::filesystem::path("src")) ? std::filesystem::path(".")
                                                            : std::filesystem::path("..");

  const std::filesystem::path setupHelpersHeaderPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.h";
  const std::filesystem::path setupHelpersSourcePath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererSetupTypeCollectionHelpers.cpp";
  const std::filesystem::path collectionHelpersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprCollectionHelpers.h";
  const std::filesystem::path tailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerEmitExprTailDispatch.h";
  const std::filesystem::path nativeTailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp";

  REQUIRE(std::filesystem::exists(setupHelpersHeaderPath));
  REQUIRE(std::filesystem::exists(setupHelpersSourcePath));
  REQUIRE(std::filesystem::exists(collectionHelpersPath));
  REQUIRE(std::filesystem::exists(tailDispatchPath));
  REQUIRE(std::filesystem::exists(nativeTailDispatchPath));

  const std::string setupHelpersHeader = readText(setupHelpersHeaderPath);
  const std::string setupHelpersSource = readText(setupHelpersSourcePath);
  const std::string collectionHelpersSource = readText(collectionHelpersPath);
  const std::string tailDispatchSource = readText(tailDispatchPath);
  const std::string nativeTailDispatchSource = readText(nativeTailDispatchPath);

  CHECK(setupHelpersHeader.find("#include \"primec/StdlibSurfaceRegistry.h\"") !=
        std::string::npos);
  CHECK(setupHelpersHeader.find("bool resolvePublishedStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersHeader.find("bool resolvePublishedStdlibSurfaceMemberToken(") !=
        std::string::npos);
  CHECK(setupHelpersHeader.find("bool resolvePublishedStdlibSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersHeader.find("bool resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersHeader.find("bool isPublishedStdlibSurfaceLoweringPath(") !=
        std::string::npos);
  CHECK(setupHelpersHeader.find("bool isCanonicalPublishedStdlibSurfaceHelperPath(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("findPublishedStdlibSurfaceMetadata(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("rebuildScopedCollectionHelperPath(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("matchesRegistrySpellingSet(metadata->loweringSpellings, path)") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("resolveStdlibSurfaceMemberName(*metadata, normalizedToken)") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("resolveStdlibSurfaceMemberName(*metadata, path)") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("resolvePublishedStdlibSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("findSemanticProductBridgePathChoiceStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("findSemanticProductMethodCallStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("normalizedPath.rfind(\"/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("\"/std/collections/soa_vector/\" +") !=
        std::string::npos);
  CHECK(setupHelpersSource.find(
            "normalizedPath.rfind(\"/std/collections/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(setupHelpersSource.find("\"/soa_vector/\" +") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("resolvePublishedStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("resolvePublishedStdlibSurfaceExprMemberName(") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("callResolutionAdapters.semanticProgram") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("primec::StdlibSurfaceId::CollectionsMapConstructors") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("primec::StdlibSurfaceId::CollectionsVectorConstructors") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("auto resolveMaterializedCollectionHelperName =") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("auto resolvePublishedLateCollectionMemberName =") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("primec::StdlibSurfaceId::CollectionsVectorHelpers") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("isPublishedStdlibSurfaceLoweringPath(") !=
        std::string::npos);
  CHECK(collectionHelpersSource.find("primec::StdlibSurfaceId::CollectionsMapHelpers") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("resolvePublishedStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, callExpr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("hasPublishedSemanticMapSurface(callExpr)") !=
        std::string::npos);
  CHECK(tailDispatchSource.find(
            "helperName == \"tryAt\" || helperName == \"at\" ||\n"
            "               helperName == \"at_unsafe\") &&\n"
            "              rawPath.rfind(\"/map/\", 0) == 0") != std::string::npos);
  CHECK(tailDispatchSource.find(
            "helperName == \"tryAt\" || helperName == \"at\" ||\n"
            "               helperName == \"at_unsafe\" || helperName == \"insert\" ||") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("isPublishedStdlibSurfaceLoweringPath(") !=
        std::string::npos);
  CHECK(tailDispatchSource.find("isCanonicalPublishedStdlibSurfaceHelperPath(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("resolvePublishedStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("resolvePublishedSemanticStdlibSurfaceMemberName(") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("findSemanticProductDirectCallStdlibSurfaceId(semanticProgram, expr)") !=
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("isCanonicalPublishedStdlibSurfaceHelperPath(") !=
        std::string::npos);
}

namespace {

primec::Definition *findLowererDefinitionByPathMutable(primec::Program &program, std::string_view fullPath) {
  const auto it =
      std::find_if(program.definitions.begin(),
                   program.definitions.end(),
                   [fullPath](const primec::Definition &definition) { return definition.fullPath == fullPath; });
  return it == program.definitions.end() ? nullptr : &*it;
}

template <typename Entry, typename Predicate>
const Entry *findLowererSemanticEntry(const std::vector<const Entry *> &entries, const Predicate &predicate) {
  const auto it = std::find_if(entries.begin(),
                               entries.end(),
                               [&](const Entry *entry) { return entry != nullptr && predicate(*entry); });
  return it == entries.end() ? nullptr : *it;
}

template <typename Predicate>
primec::Expr *findLowererExprRecursiveMutable(primec::Expr &expr, const Predicate &predicate) {
  if (predicate(expr)) {
    return &expr;
  }
  for (auto &arg : expr.args) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(arg, predicate)) {
      return found;
    }
  }
  for (auto &bodyExpr : expr.bodyArguments) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(bodyExpr, predicate)) {
      return found;
    }
  }
  return nullptr;
}

template <typename Predicate>
primec::Expr *findLowererExprInDefinitionMutable(primec::Definition &definition, const Predicate &predicate) {
  for (auto &parameter : definition.parameters) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(parameter, predicate)) {
      return found;
    }
  }
  for (auto &statement : definition.statements) {
    if (primec::Expr *found = findLowererExprRecursiveMutable(statement, predicate)) {
      return found;
    }
  }
  if (definition.returnExpr.has_value()) {
    return findLowererExprRecursiveMutable(*definition.returnExpr, predicate);
  }
  return nullptr;
}

} // namespace

TEST_CASE("ir lowerer call helpers consume pilot routing semantic-product facts") {
  const std::string source = R"(
import /std/collections/*

[return<i32>]
id_i32([i32] value) {
  return(value)
}

[effects(heap_alloc), return<i32>]
main() {
  [auto] selected{id_i32(1i32)}
  [auto] values{vector<i32>(1i32)}
  [i32] viaMethod{values.count()}
  [i32] viaBridge{count(values)}
  return(plus(selected, plus(viaMethod, viaBridge)))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto *bridgeEntry = findLowererSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(bridgeEntry != nullptr);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  primec::Definition *mainDef = findLowererDefinitionByPathMutable(program, "/main");
  REQUIRE(mainDef != nullptr);

  primec::Expr *directExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && !expr.isMethodCall && expr.name == "id_i32";
      });
  primec::Expr *methodExpr = findLowererExprInDefinitionMutable(
      *mainDef,
      [](const primec::Expr &expr) {
        return expr.kind == primec::Expr::Kind::Call && expr.isMethodCall && expr.name == "count";
      });
  REQUIRE(directExpr != nullptr);
  REQUIRE(methodExpr != nullptr);

  primec::Expr bridgeExpr;
  bridgeExpr.kind = primec::Expr::Kind::Call;
  bridgeExpr.semanticNodeId = bridgeEntry->semanticNodeId;

  CHECK(semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.count(directExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.count(methodExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.count(bridgeExpr.semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.count(
            directExpr->semanticNodeId) == 0);
  CHECK(semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.count(
            methodExpr->semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.count(
            bridgeExpr.semanticNodeId) == 1);
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, *directExpr) == "/id_i32");
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, *methodExpr) ==
        "/std/collections/vector/count");
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, bridgeExpr) ==
        "/std/collections/vector/count");
  CHECK_FALSE(primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, *directExpr)
                  .has_value());
  const auto methodSurfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, *methodExpr);
  REQUIRE(methodSurfaceId.has_value());
  CHECK(*methodSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);
  const auto bridgeSurfaceId =
      primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter, bridgeExpr);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);

  const auto *summary = primec::ir_lowerer::findSemanticProductCallableSummary(adapter, "/main");
  REQUIRE(summary != nullptr);
  CHECK(semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.count(summary->fullPathId) == 1);
  CHECK(summary->returnKind == "i32");
}

TEST_CASE("ir lowerer call helpers keep exact-import vector and map bridge parity") {
  const std::string source = R"(
import /std/collections/vector
import /std/collections/map

[effects(heap_alloc), return<i32>]
main() {
  [auto] values{vector<i32>(1i32, 2i32)}
  [auto] pairs{map<i32, i32>(1i32, 7i32, 2i32, 11i32)}
  [i32] viaVector{count(values)}
  [i32] viaMap{count(pairs)}
  return(plus(viaVector, viaMap))
}
)";

  primec::Program program;
  primec::SemanticProgram semanticProgram;
  std::string error;
  REQUIRE(parseAndValidate(source, program, semanticProgram, error, {"io_out", "io_err"}));
  CHECK(error.empty());

  const auto *vectorBridgeEntry = findLowererSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/vector/count";
      });
  REQUIRE(vectorBridgeEntry != nullptr);

  const auto *mapBridgeEntry = findLowererSemanticEntry(
      primec::semanticProgramBridgePathChoiceView(semanticProgram),
      [&semanticProgram](const primec::SemanticProgramBridgePathChoice &entry) {
        return entry.scopePath == "/main" &&
               primec::semanticProgramBridgePathChoiceHelperName(semanticProgram, entry) == "count" &&
               primec::semanticProgramResolveCallTargetString(semanticProgram, entry.chosenPathId) ==
                   "/std/collections/map/count";
      });
  REQUIRE(mapBridgeEntry != nullptr);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::Expr vectorBridgeExpr;
  vectorBridgeExpr.kind = primec::Expr::Kind::Call;
  vectorBridgeExpr.semanticNodeId = vectorBridgeEntry->semanticNodeId;
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.count(
            vectorBridgeExpr.semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.count(
            vectorBridgeExpr.semanticNodeId) == 1);
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, vectorBridgeExpr) ==
        "/std/collections/vector/count");
  const auto vectorBridgeSurfaceId =
      primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter,
                                                                              vectorBridgeExpr);
  REQUIRE(vectorBridgeSurfaceId.has_value());
  CHECK(*vectorBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);

  primec::Expr mapBridgeExpr;
  mapBridgeExpr.kind = primec::Expr::Kind::Call;
  mapBridgeExpr.semanticNodeId = mapBridgeEntry->semanticNodeId;
  CHECK(semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.count(
            mapBridgeExpr.semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.count(
            mapBridgeExpr.semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.count(
            mapBridgeExpr.semanticNodeId) == 1);
  CHECK(semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.count(
            mapBridgeExpr.semanticNodeId) == 1);
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, mapBridgeExpr) ==
        "/std/collections/map/count");
  CHECK(primec::ir_lowerer::findSemanticProductBridgePathChoice(adapter, mapBridgeExpr) ==
        "/std/collections/map/count");
  const auto mapDirectSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter,
                                                                        mapBridgeExpr);
  REQUIRE(mapDirectSurfaceId.has_value());
  CHECK(*mapDirectSurfaceId == primec::StdlibSurfaceId::CollectionsMapHelpers);
  const auto mapBridgeSurfaceId =
      primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter,
                                                                              mapBridgeExpr);
  REQUIRE(mapBridgeSurfaceId.has_value());
  CHECK(*mapBridgeSurfaceId == primec::StdlibSurfaceId::CollectionsMapHelpers);
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
  CHECK(countAccessClassifiersSource.find("IrLowererSetupTypeCollectionHelpers.h") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("primec::ir_lowerer::resolveVectorHelperAliasName(") !=
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("primec::ir_lowerer::resolveMapHelperAliasName(") !=
        std::string::npos);

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

TEST_CASE("vm numeric opcode helpers source delegation stays stable") {
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

  const std::filesystem::path vmExecutionNumericPath = repoRoot / "src" / "VmExecutionNumeric.cpp";
  const std::filesystem::path vmDebugNumericPath = repoRoot / "src" / "VmDebugSessionInstructionNumeric.cpp";
  const std::filesystem::path vmNumericSharedPath = repoRoot / "src" / "VmNumericOpcodeShared.cpp";
  const std::filesystem::path vmNumericSharedHeaderPath = repoRoot / "src" / "VmNumericOpcodeShared.h";
  REQUIRE(std::filesystem::exists(vmExecutionNumericPath));
  REQUIRE(std::filesystem::exists(vmDebugNumericPath));
  REQUIRE(std::filesystem::exists(vmNumericSharedPath));
  REQUIRE(std::filesystem::exists(vmNumericSharedHeaderPath));

  const std::string vmExecutionNumericSource = readText(vmExecutionNumericPath);
  const std::string vmDebugNumericSource = readText(vmDebugNumericPath);
  const std::string vmNumericSharedSource = readText(vmNumericSharedPath);
  const std::string vmNumericSharedHeaderSource = readText(vmNumericSharedHeaderPath);

  CHECK(vmExecutionNumericSource.find("#include \"VmNumericOpcodeShared.h\"") != std::string::npos);
  CHECK(vmDebugNumericSource.find("#include \"VmNumericOpcodeShared.h\"") != std::string::npos);
  CHECK(vmExecutionNumericSource.find("handleSharedVmNumericOpcode(inst, stack, error)") != std::string::npos);
  CHECK(vmDebugNumericSource.find("handleSharedVmNumericOpcode(inst, stack, error)") != std::string::npos);
  CHECK(vmExecutionNumericSource.find("case IrOpcode::AddI32:") == std::string::npos);
  CHECK(vmDebugNumericSource.find("case IrOpcode::AddI32:") == std::string::npos);
  CHECK(vmExecutionNumericSource.find("case IrOpcode::ConvertF64ToF32:") == std::string::npos);
  CHECK(vmDebugNumericSource.find("case IrOpcode::ConvertF64ToF32:") == std::string::npos);

  CHECK(vmNumericSharedHeaderSource.find("enum class VmNumericOpcodeResult") != std::string::npos);
  CHECK(vmNumericSharedHeaderSource.find("handleSharedVmNumericOpcode(") != std::string::npos);

  CHECK(vmNumericSharedSource.find("case IrOpcode::AddI32:") != std::string::npos);
  CHECK(vmNumericSharedSource.find("case IrOpcode::CmpEqF64:") != std::string::npos);
  CHECK(vmNumericSharedSource.find("case IrOpcode::ConvertF64ToF32:") != std::string::npos);
  CHECK(vmNumericSharedSource.find("division by zero in IR") != std::string::npos);
}

TEST_CASE("vm control flow opcode helpers source delegation stays stable") {
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

  const std::filesystem::path vmExecutionPath = repoRoot / "src" / "VmExecution.cpp";
  const std::filesystem::path vmDebugInstructionPath =
      repoRoot / "src" / "VmDebugSessionInstruction.cpp";
  const std::filesystem::path vmControlFlowSharedPath =
      repoRoot / "src" / "VmControlFlowOpcodeShared.cpp";
  const std::filesystem::path vmControlFlowSharedHeaderPath =
      repoRoot / "src" / "VmControlFlowOpcodeShared.h";
  REQUIRE(std::filesystem::exists(vmExecutionPath));
  REQUIRE(std::filesystem::exists(vmDebugInstructionPath));
  REQUIRE(std::filesystem::exists(vmControlFlowSharedPath));
  REQUIRE(std::filesystem::exists(vmControlFlowSharedHeaderPath));

  const std::string vmExecutionSource = readText(vmExecutionPath);
  const std::string vmDebugInstructionSource = readText(vmDebugInstructionPath);
  const std::string vmControlFlowSharedSource = readText(vmControlFlowSharedPath);
  const std::string vmControlFlowSharedHeaderSource = readText(vmControlFlowSharedHeaderPath);

  CHECK(vmExecutionSource.find("#include \"VmControlFlowOpcodeShared.h\"") != std::string::npos);
  CHECK(vmDebugInstructionSource.find("#include \"VmControlFlowOpcodeShared.h\"") !=
        std::string::npos);
  CHECK(vmExecutionSource.find("handleSharedVmControlFlowOpcode(") != std::string::npos);
  CHECK(vmDebugInstructionSource.find("handleSharedVmControlFlowOpcode(") != std::string::npos);
  CHECK(vmExecutionSource.find("case IrOpcode::JumpIfZero:") == std::string::npos);
  CHECK(vmDebugInstructionSource.find("case IrOpcode::JumpIfZero:") == std::string::npos);
  CHECK(vmExecutionSource.find("case IrOpcode::Call:") == std::string::npos);
  CHECK(vmDebugInstructionSource.find("case IrOpcode::Call:") == std::string::npos);
  CHECK(vmExecutionSource.find("case IrOpcode::ReturnI32:") == std::string::npos);
  CHECK(vmDebugInstructionSource.find("case IrOpcode::ReturnI32:") == std::string::npos);

  CHECK(vmControlFlowSharedHeaderSource.find("enum class VmControlFlowOpcodeResult") !=
        std::string::npos);
  CHECK(vmControlFlowSharedHeaderSource.find("struct VmControlFlowOpcodeOutcome") !=
        std::string::npos);
  CHECK(vmControlFlowSharedHeaderSource.find("handleSharedVmControlFlowOpcode(") !=
        std::string::npos);

  CHECK(vmControlFlowSharedSource.find("case IrOpcode::JumpIfZero:") != std::string::npos);
  CHECK(vmControlFlowSharedSource.find("case IrOpcode::CallVoid:") != std::string::npos);
  CHECK(vmControlFlowSharedSource.find("case IrOpcode::ReturnI32:") != std::string::npos);
  CHECK(vmControlFlowSharedSource.find("invalid jump target in IR") != std::string::npos);
  CHECK(vmControlFlowSharedSource.find("VM call stack overflow") != std::string::npos);
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

TEST_CASE("ir lowerer call helpers prefer explicit experimental vector helpers over structs") {
  primec::Definition vectorHelper;
  vectorHelper.fullPath = "/std/collections/experimental_vector/vector__t25a78a513414c3bf";
  primec::Definition vectorStruct;
  vectorStruct.fullPath = "/std/collections/experimental_vector/Vector__t25a78a513414c3bf";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {vectorHelper.fullPath, &vectorHelper},
      {vectorStruct.fullPath, &vectorStruct},
  };
  auto resolver = [](const primec::Expr &) {
    return std::string("/std/collections/experimental_vector/Vector__t25a78a513414c3bf");
  };

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "/std/collections/experimental_vector/vector";
  directCall.templateArgs = {"i32"};

  CHECK(primec::ir_lowerer::resolveDefinitionCall(directCall, defMap, resolver) ==
        &vectorHelper);
}

TEST_CASE("ir lowerer call helpers reject missing definition path resolver") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};

  primec::Expr directCall;
  directCall.kind = primec::Expr::Kind::Call;
  directCall.name = "callee";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            directCall, defMap, primec::ir_lowerer::ResolveExprPathFn{}) == nullptr);
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
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

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
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  primec::Expr namespacedExpr;
  namespacedExpr.kind = primec::Expr::Kind::Name;
  namespacedExpr.name = "foo";
  namespacedExpr.namespacePrefix = "/pkg";
  CHECK(semanticResolveExprPath(namespacedExpr) == "/pkg/foo");
}

TEST_CASE("ir lowerer call helpers fail closed when semantic-product direct-call targets are missing") {
  primec::Definition callee;
  callee.fullPath = "/imported/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/imported/callee", &callee},
  };
  const std::unordered_map<std::string, std::string> importAliases = {
      {"callee", "/imported/callee"},
  };

  primec::SemanticProgram semanticProgram;
  const auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  const auto semanticResolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(defMap, semanticResolveExprPath);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";
  callExpr.semanticNodeId = 17;
  CHECK(semanticResolveExprPath(callExpr) == "/callee");
  CHECK(semanticResolveDefinitionCall(callExpr) == nullptr);

  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "callee",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 17,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/imported/callee"),
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      17, semanticProgram.directCallTargets.back().resolvedPathId);
  const auto populatedResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(populatedResolveExprPath(callExpr) == "/imported/callee");
}

TEST_CASE("ir lowerer call helpers keep unresolved rooted semantic operator targets authoritative") {
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      71, semanticProgram.directCallTargets.back().resolvedPathId);
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  const auto resolveDefinitionCall =
      primec::ir_lowerer::makeResolveDefinitionCall(defMap, resolveExprPath);

  CHECK(resolveExprPath(callExpr) == "/multiply");
  CHECK(resolveDefinitionCall(callExpr) == nullptr);
}

TEST_CASE("ir lowerer call helpers keep semantic-product direct-call targets authoritative over rooted rewritten expr names") {
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      19, semanticProgram.directCallTargets.back().resolvedPathId);
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  CHECK(resolveExprPath(callExpr) == "/semantic/target");
}

TEST_CASE("ir lowerer call helpers keep rooted rewritten expr names when semantic-product direct-call targets are missing") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::Expr rewrittenExpr;
  rewrittenExpr.kind = primec::Expr::Kind::Call;
  rewrittenExpr.name = "/operator/add";

  primec::SemanticProgram semanticProgram;
  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);

  CHECK(resolveExprPath(rewrittenExpr) == "/operator/add");
}

TEST_CASE("ir lowerer call helpers keep source-shaped method paths when semantic-product targets are missing") {
  const std::unordered_map<std::string, const primec::Definition *> defMap = {};
  const std::unordered_map<std::string, std::string> importAliases = {
      {"contains", "/std/collections/map/contains"},
  };

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.namespacePrefix = "main";
  methodExpr.name = "contains";
  methodExpr.semanticNodeId = 44;

  primec::SemanticProgram semanticProgram;
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "map",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 44,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "map"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains"),
      .chosenPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram,
                                                        "/std/collections/map/contains"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
  CHECK(resolveExprPath(methodExpr) == "/main/contains");

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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      44, semanticProgram.methodCallTargets.back().resolvedPathId);
  resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
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
      .stdlibSurfaceId = std::nullopt,
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      44,
      semanticProgram.methodCallTargets[0].resolvedPathId);
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      45,
      semanticProgram.methodCallTargets[1].resolvedPathId);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.semanticProgram == &semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  REQUIRE(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(44) == 1);
  REQUIRE(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(45) == 1);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.at(44) ==
        adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.at(45));
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.at(44) ==
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

TEST_CASE("ir lowerer semantic-product adapter rejects call-target source lookup") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "status",
      .sourceLine = 7,
      .sourceColumn = 9,
      .semanticNodeId = 155,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/file/FileError/status"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "at",
      .receiverTypeText = "/std/collections/experimental_vector/Vector__t25a78a513414c3bf",
      .sourceLine = 11,
      .sourceColumn = 15,
      .semanticNodeId = 244,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "at"),
      .receiverTypeTextId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/experimental_vector/Vector__t25a78a513414c3bf"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/collections/vector/at"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsVectorHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      155, semanticProgram.directCallTargets[0].resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      155, primec::StdlibSurfaceId::FileErrorHelpers);
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      244, semanticProgram.methodCallTargets[0].resolvedPathId);
  semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(
      244, primec::StdlibSurfaceId::CollectionsVectorHelpers);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  primec::Expr directExpr;
  directExpr.kind = primec::Expr::Kind::Call;
  directExpr.name = "status";
  directExpr.sourceLine = 7;
  directExpr.sourceColumn = 9;
  directExpr.semanticNodeId = 0;

  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, directExpr) ==
        "");
  const auto directSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, directExpr);
  CHECK_FALSE(directSurfaceId.has_value());

  directExpr.semanticNodeId = 155;
  CHECK(primec::ir_lowerer::findSemanticProductDirectCallTarget(adapter, directExpr) ==
        "/std/file/FileError/status");
  const auto directSemanticSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, directExpr);
  REQUIRE(directSemanticSurfaceId.has_value());
  CHECK(*directSemanticSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.name = "at";
  methodExpr.sourceLine = 11;
  methodExpr.sourceColumn = 15;
  methodExpr.semanticNodeId = 0;

  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, methodExpr) ==
        "");
  const auto surfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, methodExpr);
  CHECK_FALSE(surfaceId.has_value());

  methodExpr.semanticNodeId = 244;
  CHECK(primec::ir_lowerer::findSemanticProductMethodCallTarget(adapter, methodExpr) ==
        "/std/collections/vector/at");
  const auto semanticSurfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, methodExpr);
  REQUIRE(semanticSurfaceId.has_value());
  CHECK(*semanticSurfaceId == primec::StdlibSurfaceId::CollectionsVectorHelpers);
}

TEST_CASE("ir lowerer semantic-product adapter exposes published stdlib surface ids") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "status",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 52,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/std/file/FileError/status"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.methodCallTargets.push_back(primec::SemanticProgramMethodCallTarget{
      .scopePath = "/main",
      .methodName = "status",
      .receiverTypeText = "FileError",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 53,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .methodNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "status"),
      .receiverTypeTextId = primec::semanticProgramInternCallTargetString(semanticProgram, "FileError"),
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/FileError/status"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::FileErrorHelpers,
  });
  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      .scopePath = "/main",
      .collectionFamily = "map",
      .sourceLine = 0,
      .sourceColumn = 0,
      .semanticNodeId = 54,
      .scopePathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
      .collectionFamilyId = primec::semanticProgramInternCallTargetString(semanticProgram, "map"),
      .helperNameId = primec::semanticProgramInternCallTargetString(semanticProgram, "contains_ref"),
      .chosenPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/experimental_map/mapContainsRef"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      52, primec::StdlibSurfaceId::FileErrorHelpers);
  semanticProgram.publishedRoutingLookups.methodCallStdlibSurfaceIdsByExpr.insert_or_assign(
      53, primec::StdlibSurfaceId::FileErrorHelpers);
  semanticProgram.publishedRoutingLookups.bridgePathChoiceStdlibSurfaceIdsByExpr.insert_or_assign(
      54, primec::StdlibSurfaceId::CollectionsMapHelpers);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->directCallStdlibSurfaceIdsByExpr.count(52) == 1);
  CHECK(adapter.publishedRoutingLookups->methodCallStdlibSurfaceIdsByExpr.count(53) == 1);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceStdlibSurfaceIdsByExpr.count(54) == 1);

  primec::Expr directExpr;
  directExpr.kind = primec::Expr::Kind::Call;
  directExpr.semanticNodeId = 52;
  const auto directSurfaceId =
      primec::ir_lowerer::findSemanticProductDirectCallStdlibSurfaceId(adapter, directExpr);
  REQUIRE(directSurfaceId.has_value());
  CHECK(*directSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  primec::Expr methodExpr;
  methodExpr.kind = primec::Expr::Kind::Call;
  methodExpr.isMethodCall = true;
  methodExpr.semanticNodeId = 53;
  const auto methodSurfaceId =
      primec::ir_lowerer::findSemanticProductMethodCallStdlibSurfaceId(adapter, methodExpr);
  REQUIRE(methodSurfaceId.has_value());
  CHECK(*methodSurfaceId == primec::StdlibSurfaceId::FileErrorHelpers);

  primec::Expr bridgeExpr;
  bridgeExpr.kind = primec::Expr::Kind::Call;
  bridgeExpr.semanticNodeId = 54;
  const auto bridgeSurfaceId =
      primec::ir_lowerer::findSemanticProductBridgePathChoiceStdlibSurfaceId(adapter, bridgeExpr);
  REQUIRE(bridgeSurfaceId.has_value());
  CHECK(*bridgeSurfaceId == primec::StdlibSurfaceId::CollectionsMapHelpers);
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
      .stdlibSurfaceId = std::nullopt,
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.methodCallTargetIdsByExpr.insert_or_assign(
      145,
      semanticProgram.methodCallTargets[1].resolvedPathId);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(144) == 0);
  CHECK(adapter.publishedRoutingLookups->methodCallTargetIdsByExpr.count(145) == 1);

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
  semanticProgram.publishedRoutingLookups.callableSummaryIndicesByPathId.insert_or_assign(mainPathId, 0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->callableSummaryIndicesByPathId.count(mainPathId) == 1);
  CHECK(adapter.publishedRoutingLookups->callableSummaryIndicesByPathId.count(primec::InvalidSymbolId) == 0);
  CHECK(adapter.publishedRoutingLookups->callableSummaryIndicesByPathId.count(
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      18, semanticProgram.directCallTargets.back().resolvedPathId);
  auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      18, semanticProgram.bridgePathChoices.back().chosenPathId);
  semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, &semanticProgram);
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
      .stdlibSurfaceId = std::nullopt,
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
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.publishedRoutingLookups.bridgePathChoiceIdsByExpr.insert_or_assign(
      119,
      semanticProgram.bridgePathChoices[1].chosenPathId);
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
      .stdlibSurfaceId = std::nullopt,
  });

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  REQUIRE(adapter.publishedRoutingLookups != nullptr);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceIdsByExpr.count(118) == 0);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceIdsByExpr.count(119) == 1);
  CHECK(adapter.publishedRoutingLookups->bridgePathChoiceIdsByExpr.count(120) == 0);

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

TEST_CASE("ir lowerer semantic-product adapter joins facts by semantic id without return-path fallback") {
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
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .initializerResolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
  });
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(62, 0);
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 63,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup"),
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(63, 0);
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
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(64, 0);

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
  CHECK(legacyReturnFact == nullptr);

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

TEST_CASE("ir lowerer semantic-product adapter does not expose return path fallback") {
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
  CHECK(semanticTargets.semanticIndex.returnFactsByDefinitionId.empty());
  const auto *returnFact = primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, mainDef);
  CHECK(returnFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index does not expose return path fallback") {
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

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.returnFactsByDefinitionId.empty());
  const auto *returnFact =
      primec::ir_lowerer::findSemanticProductReturnFact(
          &semanticProgram, semanticIndex, mainDef);
  CHECK(returnFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter ignores on_error definition-path fallback") {
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
  const auto mainPathId =
      primec::semanticProgramLookupCallTargetStringId(semanticProgram, "/main");
  REQUIRE(mainPathId.has_value());
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId.insert_or_assign(
      *mainPathId, 0);
  CHECK(primec::semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(
            semanticProgram, *mainPathId) == &semanticProgram.onErrorFacts[0]);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(semanticTargets.semanticIndex.onErrorFactsByDefinitionId.empty());
  const auto *onErrorFact = primec::ir_lowerer::findSemanticProductOnErrorFact(semanticTargets, mainDef);
  CHECK(onErrorFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter ignores raw on_error facts without maps") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5303;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 5303,
      .definitionPathId = mainPathId,
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

  CHECK(primec::semanticProgramLookupPublishedOnErrorFactByDefinitionSemanticId(
            semanticProgram, 5303) == nullptr);
  CHECK(primec::semanticProgramLookupPublishedOnErrorFactByDefinitionPathId(
            semanticProgram, mainPathId) == nullptr);

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.onErrorFactsByDefinitionId.empty());
  const auto *onErrorFact =
      primec::ir_lowerer::findSemanticProductOnErrorFact(
          &semanticProgram, semanticIndex, mainDef);
  CHECK(onErrorFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses on_error semantic-id matches without path fallback") {
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  mainDef.semanticNodeId = 5201;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "FileError",
      .boundArgCount = 1,
      .boundArgTexts = {"value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 5201,
      .definitionPathId = mainPathId,
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
  semanticProgram.onErrorFacts.push_back(primec::SemanticProgramOnErrorFact{
      .definitionPath = "/main",
      .returnKind = "return",
      .errorType = "OtherError",
      .boundArgCount = 2,
      .boundArgTexts = {"stale", "value"},
      .returnResultHasValue = false,
      .returnResultValueType = "",
      .returnResultErrorType = "",
      .semanticNodeId = 0,
      .definitionPathId = mainPathId,
      .returnKindId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "return"),
      .handlerPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/stale_handler"),
      .errorTypeId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "OtherError"),
      .boundArgTextIds = {
          primec::semanticProgramInternCallTargetString(semanticProgram, "stale"),
          primec::semanticProgramInternCallTargetString(semanticProgram, "value"),
      },
  });
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionId
      .insert_or_assign(5201, 0);
  semanticProgram.publishedRoutingLookups.onErrorFactIndicesByDefinitionPathId
      .insert_or_assign(mainPathId, 1);

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *onErrorFact = primec::ir_lowerer::findSemanticProductOnErrorFact(semanticTargets, mainDef);
  REQUIRE(onErrorFact != nullptr);
  CHECK(onErrorFact->semanticNodeId == 5201);
  CHECK(onErrorFact->errorType == "FileError");
  CHECK(onErrorFact->boundArgCount == 1);
}

TEST_CASE("ir lowerer semantic-product adapter ignores local-auto initializer-path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  });
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId
      .insert_or_assign((static_cast<uint64_t>(initializerPathId) << 32) |
                            static_cast<uint64_t>(bindingNameId),
                        0);
  CHECK(primec::semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
            semanticProgram, initializerPathId, bindingNameId) ==
        &semanticProgram.localAutoFacts[0]);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.localAutoFactsByExpr.empty());
  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(adapter, localBinding);
  CHECK(localAutoFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter ignores raw local-auto facts without maps") {
  primec::Expr initCall;
  initCall.kind = primec::Expr::Kind::Call;
  initCall.name = "id";
  initCall.semanticNodeId = 7301;

  primec::Expr localBinding;
  localBinding.kind = primec::Expr::Kind::Name;
  localBinding.isBinding = true;
  localBinding.name = "value";
  localBinding.semanticNodeId = 7302;
  localBinding.args = {initCall};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "id",
      .sourceLine = 12,
      .sourceColumn = 5,
      .semanticNodeId = 7301,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(semanticProgram, "/id"),
      .stdlibSurfaceId = std::nullopt,
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
      .semanticNodeId = 7302,
      .provenanceHandle = 0,
      .initializerDirectCallResolvedPath = "",
      .initializerDirectCallReturnKind = "",
      .initializerMethodCallResolvedPath = "",
      .initializerMethodCallReturnKind = "",
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  });

  CHECK(primec::semanticProgramLookupPublishedLocalAutoFactBySemanticId(
            semanticProgram, 7302) == nullptr);
  CHECK(primec::semanticProgramLookupPublishedLocalAutoFactByInitializerPathAndBindingNameId(
            semanticProgram, initializerPathId, bindingNameId) == nullptr);

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.localAutoFactsByExpr.empty());
  const auto *localAutoFact =
      primec::ir_lowerer::findSemanticProductLocalAutoFact(
          &semanticProgram, semanticIndex, localBinding);
  CHECK(localAutoFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses local-auto semantic-id matches without path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
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
      .initializerStdlibSurfaceId = std::nullopt,
      .initializerDirectCallStdlibSurfaceId = std::nullopt,
      .initializerMethodCallStdlibSurfaceId = std::nullopt,
      .bindingNameId = bindingNameId,
      .initializerResolvedPathId = initializerPathId,
  };
  semanticProgram.localAutoFacts.push_back(std::move(fallbackFact));
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByExpr.insert_or_assign(7202, 0);
  semanticProgram.publishedRoutingLookups.localAutoFactIndicesByInitPathAndBindingNameId
      .insert_or_assign((static_cast<uint64_t>(initializerPathId) << 32) |
                            static_cast<uint64_t>(bindingNameId),
                        1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(adapter, localBinding);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->semanticNodeId == 7202);
  CHECK(localAutoFact->bindingTypeText == "i32");
}

TEST_CASE("ir lowerer semantic-product adapter ignores raw collection specializations without maps") {
  primec::Expr collectionExpr;
  collectionExpr.kind = primec::Expr::Kind::Name;
  collectionExpr.name = "values";
  collectionExpr.semanticNodeId = 7701;

  primec::SemanticProgram semanticProgram;
  semanticProgram.collectionSpecializations.push_back(
      primec::SemanticProgramCollectionSpecialization{
          .scopePath = "/main",
          .siteKind = "local",
          .name = "values",
          .collectionFamily = "vector",
          .bindingTypeText = "vector<i32>",
          .elementTypeText = "i32",
          .keyTypeText = "",
          .valueTypeText = "",
          .isReference = false,
          .isPointer = false,
          .sourceLine = 15,
          .sourceColumn = 7,
          .semanticNodeId = 7701,
          .provenanceHandle = 0,
          .scopePathId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
          .siteKindId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "local"),
          .nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "values"),
          .collectionFamilyId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
          .bindingTypeTextId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
          .elementTypeTextId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
          .keyTypeTextId = primec::InvalidSymbolId,
          .valueTypeTextId = primec::InvalidSymbolId,
          .helperSurfaceId = std::nullopt,
          .constructorSurfaceId = std::nullopt,
      });

  CHECK(primec::semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
            semanticProgram, 7701) == nullptr);

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.collectionSpecializationsByExpr.empty());
  const auto *collectionFact =
      primec::ir_lowerer::findSemanticProductCollectionSpecialization(
          semanticIndex, collectionExpr);
  CHECK(collectionFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses collection specialization semantic-id maps") {
  primec::Expr collectionExpr;
  collectionExpr.kind = primec::Expr::Kind::Name;
  collectionExpr.name = "values";
  collectionExpr.semanticNodeId = 7702;

  primec::SemanticProgram semanticProgram;
  semanticProgram.collectionSpecializations.push_back(
      primec::SemanticProgramCollectionSpecialization{
          .scopePath = "/main",
          .siteKind = "local",
          .name = "values",
          .collectionFamily = "vector",
          .bindingTypeText = "vector<i32>",
          .elementTypeText = "i32",
          .keyTypeText = "",
          .valueTypeText = "",
          .isReference = false,
          .isPointer = false,
          .sourceLine = 16,
          .sourceColumn = 7,
          .semanticNodeId = 7702,
          .provenanceHandle = 0,
          .scopePathId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "/main"),
          .siteKindId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "local"),
          .nameId = primec::semanticProgramInternCallTargetString(semanticProgram, "values"),
          .collectionFamilyId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "vector"),
          .bindingTypeTextId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "vector<i32>"),
          .elementTypeTextId =
              primec::semanticProgramInternCallTargetString(semanticProgram, "i32"),
          .keyTypeTextId = primec::InvalidSymbolId,
          .valueTypeTextId = primec::InvalidSymbolId,
          .helperSurfaceId = std::nullopt,
          .constructorSurfaceId = std::nullopt,
      });
  semanticProgram.publishedRoutingLookups.collectionSpecializationIndicesByExpr
      .insert_or_assign(7702, 0);

  CHECK(primec::semanticProgramLookupPublishedCollectionSpecializationBySemanticId(
            semanticProgram, 7702) == &semanticProgram.collectionSpecializations[0]);

  const auto adapter =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *collectionFact =
      primec::ir_lowerer::findSemanticProductCollectionSpecialization(
          adapter, collectionExpr);
  REQUIRE(collectionFact != nullptr);
  CHECK(collectionFact->semanticNodeId == 7702);
  CHECK(collectionFact->collectionFamily == "vector");
  CHECK(collectionFact->bindingTypeText == "vector<i32>");
  CHECK(collectionFact->elementTypeText == "i32");
}

TEST_CASE("ir lowerer semantic-product index resolves binding facts by semantic id") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "i32",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 14,
      .sourceColumn = 7,
      .semanticNodeId = 7401,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/selected"),
  });

  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 7401;

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.bindingFactsByExpr.count(7401) == 1);
  const auto *bindingFact =
      primec::ir_lowerer::findSemanticProductBindingFact(semanticIndex, bindingExpr);
  REQUIRE(bindingFact != nullptr);
  CHECK(bindingFact->bindingTypeText == "i32");
}

TEST_CASE("ir lowerer statement binding helper consumes semantic-product index directly") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "selected",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 21,
      .sourceColumn = 5,
      .semanticNodeId = 7501,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/selected"),
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(
              semanticProgram, "map<i32, string>"),
  });

  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";
  bindingExpr.semanticNodeId = 7501;

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Name;
  initExpr.name = "selected";

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          bindingExpr,
          initExpr,
          {},
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          {},
          &semanticProgram,
          &semanticIndex);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
}

TEST_CASE("ir lowerer statement binding helper prefers semantic initializer binding facts") {
  primec::SemanticProgram semanticProgram;
  semanticProgram.bindingFacts.push_back(primec::SemanticProgramBindingFact{
      .scopePath = "/main",
      .siteKind = "local",
      .name = "source",
      .bindingTypeText = "",
      .isMutable = false,
      .isEntryArgString = false,
      .isUnsafeReference = false,
      .referenceRoot = "",
      .sourceLine = 22,
      .sourceColumn = 5,
      .semanticNodeId = 7601,
      .resolvedPathId =
          primec::semanticProgramInternCallTargetString(semanticProgram, "/source"),
      .bindingTypeTextId =
          primec::semanticProgramInternCallTargetString(
              semanticProgram, "map<i32, string>"),
  });

  primec::Expr bindingExpr;
  bindingExpr.kind = primec::Expr::Kind::Name;
  bindingExpr.isBinding = true;
  bindingExpr.name = "selected";

  primec::Expr initExpr;
  initExpr.kind = primec::Expr::Kind::Name;
  initExpr.name = "source";
  initExpr.semanticNodeId = 7601;

  primec::ir_lowerer::LocalInfo staleLocalInfo;
  staleLocalInfo.kind = primec::ir_lowerer::LocalInfo::Kind::Map;
  staleLocalInfo.mapKeyKind = primec::ir_lowerer::LocalInfo::ValueKind::Int64;
  staleLocalInfo.mapValueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  staleLocalInfo.valueKind = primec::ir_lowerer::LocalInfo::ValueKind::Bool;
  staleLocalInfo.structTypeName = "/stale/Map";
  const primec::ir_lowerer::LocalMap locals{{"source", staleLocalInfo}};

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  const primec::ir_lowerer::StatementBindingTypeInfo info =
      primec::ir_lowerer::inferStatementBindingTypeInfo(
          bindingExpr,
          initExpr,
          locals,
          [](const primec::Expr &) { return false; },
          [](const primec::Expr &) { return primec::ir_lowerer::LocalInfo::Kind::Value; },
          [](const primec::Expr &, primec::ir_lowerer::LocalInfo::Kind) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          [](const primec::Expr &, const primec::ir_lowerer::LocalMap &) {
            return primec::ir_lowerer::LocalInfo::ValueKind::Unknown;
          },
          {},
          &semanticProgram,
          &semanticIndex);

  CHECK(info.kind == primec::ir_lowerer::LocalInfo::Kind::Map);
  CHECK(info.mapKeyKind == primec::ir_lowerer::LocalInfo::ValueKind::Int32);
  CHECK(info.mapValueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(info.valueKind == primec::ir_lowerer::LocalInfo::ValueKind::String);
  CHECK(info.structTypeName != "/stale/Map");
}

TEST_CASE("ir lowerer semantic-product adapter ignores query resolved-path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
      .receiverBindingTypeText = "",
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
  semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
      .insert_or_assign((static_cast<uint64_t>(resolvedPathId) << 32) |
                            static_cast<uint64_t>(callNameId),
                        0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.queryFactsByExpr.empty());
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  CHECK(queryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses query semantic-id matches without path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
      .receiverBindingTypeText = "",
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
      .receiverBindingTypeText = "",
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
  semanticProgram.publishedRoutingLookups.queryFactIndicesByExpr.insert_or_assign(8202, 0);
  semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
      .insert_or_assign((static_cast<uint64_t>(resolvedPathId) << 32) |
                            static_cast<uint64_t>(callNameId),
                        1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->semanticNodeId == 8202);
  CHECK(queryFact->queryTypeText == "Result<i32, FileError>");
}

TEST_CASE("ir lowerer semantic-product adapter ignores raw query facts without maps") {
  primec::Expr queryExpr;
  queryExpr.kind = primec::Expr::Kind::Call;
  queryExpr.name = "lookup";
  queryExpr.sourceLine = 19;
  queryExpr.sourceColumn = 5;
  queryExpr.semanticNodeId = 8303;

  primec::SemanticProgram semanticProgram;
  const primec::SymbolId callNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "lookup");
  const primec::SymbolId resolvedPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/lookup");
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "lookup",
      .sourceLine = 19,
      .sourceColumn = 5,
      .semanticNodeId = 8303,
      .resolvedPathId = resolvedPathId,
      .stdlibSurfaceId = std::nullopt,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<i32, FileError>",
      .bindingTypeText = "Result<i32, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "i32",
      .resultErrorType = "FileError",
      .sourceLine = 19,
      .sourceColumn = 5,
      .semanticNodeId = 8303,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      .scopePath = "/main",
      .callName = "lookup",
      .queryTypeText = "Result<f64, FileError>",
      .bindingTypeText = "Result<f64, FileError>",
      .receiverBindingTypeText = "",
      .hasResultType = true,
      .resultTypeHasValue = true,
      .resultValueType = "f64",
      .resultErrorType = "FileError",
      .sourceLine = 19,
      .sourceColumn = 5,
      .semanticNodeId = 0,
      .callNameId = callNameId,
      .resolvedPathId = resolvedPathId,
  });
  semanticProgram.publishedRoutingLookups.queryFactIndicesByResolvedPathAndCallNameId
      .insert_or_assign((static_cast<uint64_t>(resolvedPathId) << 32) |
                            static_cast<uint64_t>(callNameId),
                        1);

  CHECK(primec::semanticProgramLookupPublishedQueryFactBySemanticId(
            semanticProgram, 8303) == nullptr);
  CHECK(primec::semanticProgramLookupPublishedQueryFactByResolvedPathAndCallNameId(
            semanticProgram, resolvedPathId, callNameId) ==
        &semanticProgram.queryFacts[1]);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.queryFactsByExpr.count(8303) == 0);
  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(adapter, queryExpr);
  CHECK(queryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product index does not expose query path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
      .receiverBindingTypeText = "",
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

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.queryFactsByExpr.empty());
  const auto *queryFact =
      primec::ir_lowerer::findSemanticProductQueryFact(&semanticProgram, semanticIndex, queryExpr);
  CHECK(queryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter ignores try operand-path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 0,
      .operandResolvedPathId = operandResolvedPathId,
  });
  const uint64_t tryKey =
      (static_cast<uint64_t>(operandResolvedPathId) << 32) ^
      (static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceLine)) * 1315423911ULL) ^
      static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceColumn));
  semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.insert_or_assign(
      tryKey, 0);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  CHECK(adapter.semanticIndex.tryFactsByExpr.empty());
  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(adapter, tryExpr);
  CHECK(tryFact == nullptr);
}

TEST_CASE("ir lowerer semantic-product adapter uses try semantic-id matches without path fallback") {
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
      .stdlibSurfaceId = std::nullopt,
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
  const uint64_t tryKey =
      (static_cast<uint64_t>(operandResolvedPathId) << 32) ^
      (static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceLine)) * 1315423911ULL) ^
      static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceColumn));
  semanticProgram.publishedRoutingLookups.tryFactIndicesByExpr.insert_or_assign(9202, 0);
  semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.insert_or_assign(
      tryKey, 1);

  const auto adapter = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(adapter, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->semanticNodeId == 9202);
  CHECK(tryFact->valueType == "i32");
}

TEST_CASE("ir lowerer semantic-product adapter ignores raw try facts without maps") {
  primec::Expr operandExpr;
  operandExpr.kind = primec::Expr::Kind::Call;
  operandExpr.name = "lookup";
  operandExpr.semanticNodeId = 9101;

  primec::Expr tryExpr;
  tryExpr.kind = primec::Expr::Kind::Call;
  tryExpr.name = "try";
  tryExpr.args = {operandExpr};
  tryExpr.semanticNodeId = 9303;
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
      .stdlibSurfaceId = std::nullopt,
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
      .sourceLine = 33,
      .sourceColumn = 9,
      .semanticNodeId = 9303,
      .operandResolvedPathId = operandResolvedPathId,
  });
  const uint64_t tryKey =
      (static_cast<uint64_t>(operandResolvedPathId) << 32) ^
      (static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceLine)) * 1315423911ULL) ^
      static_cast<uint64_t>(static_cast<uint32_t>(tryExpr.sourceColumn));
  semanticProgram.publishedRoutingLookups.tryFactIndicesByOperandPathAndSource.insert_or_assign(
      tryKey, 0);

  CHECK(primec::semanticProgramLookupPublishedTryFactBySemanticId(semanticProgram, 9303) ==
        nullptr);
  CHECK(primec::semanticProgramLookupPublishedTryFactByOperandPathAndSource(
            semanticProgram, operandResolvedPathId, 33, 9) == &semanticProgram.tryFacts[0]);

  const auto semanticIndex =
      primec::ir_lowerer::buildSemanticProductIndex(&semanticProgram);
  CHECK(semanticIndex.tryFactsByExpr.empty());
  const auto *tryFact =
      primec::ir_lowerer::findSemanticProductTryFact(&semanticProgram, semanticIndex, tryExpr);
  CHECK(tryFact == nullptr);
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

TEST_CASE("ir lowerer call helpers keep explicit canonical map contains and tryAt same-path defs") {
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "/std/collections/map/contains";
  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;
  containsCall.args = {valuesArg, keyArg};

  primec::Expr tryAtCall = containsCall;
  tryAtCall.name = "/std/collections/map/tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(containsCall, defMap, resolveExprPath) ==
        &canonicalMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(tryAtCall, defMap, resolveExprPath) ==
        &canonicalMapTryAtDef);
}

TEST_CASE("ir lowerer call helpers keep explicit map helper same-path defs") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalMapAtDef;
  canonicalMapAtDef.fullPath = "/std/collections/map/at";
  primec::Definition canonicalMapAtUnsafeDef;
  canonicalMapAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  primec::Definition aliasMapCountDef;
  aliasMapCountDef.fullPath = "/map/count";
  primec::Definition aliasMapContainsDef;
  aliasMapContainsDef.fullPath = "/map/contains";
  primec::Definition aliasMapTryAtDef;
  aliasMapTryAtDef.fullPath = "/map/tryAt";
  primec::Definition aliasMapAtDef;
  aliasMapAtDef.fullPath = "/map/at";
  primec::Definition aliasMapAtUnsafeDef;
  aliasMapAtUnsafeDef.fullPath = "/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapCountDef.fullPath, &canonicalMapCountDef},
      {canonicalMapAtDef.fullPath, &canonicalMapAtDef},
      {canonicalMapAtUnsafeDef.fullPath, &canonicalMapAtUnsafeDef},
      {aliasMapCountDef.fullPath, &aliasMapCountDef},
      {aliasMapContainsDef.fullPath, &aliasMapContainsDef},
      {aliasMapTryAtDef.fullPath, &aliasMapTryAtDef},
      {aliasMapAtDef.fullPath, &aliasMapAtDef},
      {aliasMapAtUnsafeDef.fullPath, &aliasMapAtUnsafeDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };
  const auto resolveExprPathWithCanonicalAliasAccessFallback =
      [](const primec::Expr &expr) {
        if (expr.name == "/map/at") {
          return std::string("/std/collections/map/at");
        }
        if (expr.name == "/map/at_unsafe") {
          return std::string("/std/collections/map/at_unsafe");
        }
        return expr.name;
      };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr canonicalCountCall;
  canonicalCountCall.kind = primec::Expr::Kind::Call;
  canonicalCountCall.name = "/std/collections/map/count";
  canonicalCountCall.args = {valuesArg};

  primec::Expr canonicalAtCall;
  canonicalAtCall.kind = primec::Expr::Kind::Call;
  canonicalAtCall.name = "/std/collections/map/at";
  canonicalAtCall.args = {valuesArg, keyArg};

  primec::Expr aliasCountCall = canonicalCountCall;
  aliasCountCall.name = "/map/count";

  primec::Expr aliasContainsCall = canonicalAtCall;
  aliasContainsCall.name = "/map/contains";

  primec::Expr aliasTryAtCall = canonicalAtCall;
  aliasTryAtCall.name = "/map/tryAt";

  primec::Expr aliasAtCall = canonicalAtCall;
  aliasAtCall.name = "/map/at";

  primec::Expr canonicalAtUnsafeCall = canonicalAtCall;
  canonicalAtUnsafeCall.name = "/std/collections/map/at_unsafe";

  primec::Expr aliasAtUnsafeCall = canonicalAtUnsafeCall;
  aliasAtUnsafeCall.name = "/map/at_unsafe";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(canonicalCountCall, defMap, resolveExprPath) ==
        &canonicalMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(canonicalAtCall, defMap, resolveExprPath) ==
        &canonicalMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(canonicalAtUnsafeCall, defMap, resolveExprPath) ==
        &canonicalMapAtUnsafeDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasCountCall, defMap, resolveExprPath) ==
        &aliasMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasContainsCall, defMap, resolveExprPath) ==
        &aliasMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasTryAtCall, defMap, resolveExprPath) ==
        &aliasMapTryAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasAtCall, defMap, resolveExprPath) ==
        &aliasMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasAtUnsafeCall, defMap, resolveExprPath) ==
        &aliasMapAtUnsafeDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            aliasAtCall, defMap, resolveExprPathWithCanonicalAliasAccessFallback) ==
        &aliasMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            aliasAtUnsafeCall, defMap, resolveExprPathWithCanonicalAliasAccessFallback) ==
        &aliasMapAtUnsafeDef);
}

TEST_CASE("ir lowerer call helpers keep bare semantic map sugar on canonical defs") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  primec::Definition canonicalMapAtDef;
  canonicalMapAtDef.fullPath = "/std/collections/map/at";
  primec::Definition canonicalMapAtUnsafeDef;
  canonicalMapAtUnsafeDef.fullPath = "/std/collections/map/at_unsafe";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapCountDef.fullPath, &canonicalMapCountDef},
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
      {canonicalMapAtDef.fullPath, &canonicalMapAtDef},
      {canonicalMapAtUnsafeDef.fullPath, &canonicalMapAtUnsafeDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "count") {
      return std::string("/std/collections/map/count");
    }
    if (expr.name == "contains") {
      return std::string("/std/collections/map/contains");
    }
    if (expr.name == "tryAt") {
      return std::string("/std/collections/map/tryAt");
    }
    if (expr.name == "at") {
      return std::string("/std/collections/map/at");
    }
    if (expr.name == "at_unsafe") {
      return std::string("/std/collections/map/at_unsafe");
    }
    return expr.name;
  };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "count";
  countCall.semanticNodeId = 91;
  countCall.args = {valuesArg};

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "contains";
  containsCall.semanticNodeId = 92;
  containsCall.args = {valuesArg, keyArg};

  primec::Expr tryAtCall = containsCall;
  tryAtCall.name = "tryAt";
  tryAtCall.semanticNodeId = 93;

  primec::Expr atCall;
  atCall.kind = primec::Expr::Kind::Call;
  atCall.name = "at";
  atCall.semanticNodeId = 94;
  atCall.args = {valuesArg, keyArg};

  primec::Expr atUnsafeCall = atCall;
  atUnsafeCall.name = "at_unsafe";
  atUnsafeCall.semanticNodeId = 95;

  CHECK(primec::ir_lowerer::resolveDefinitionCall(countCall, defMap, resolveExprPath) ==
        &canonicalMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(containsCall, defMap, resolveExprPath) ==
        &canonicalMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(tryAtCall, defMap, resolveExprPath) ==
        &canonicalMapTryAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(atCall, defMap, resolveExprPath) ==
        &canonicalMapAtDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(atUnsafeCall, defMap, resolveExprPath) ==
        &canonicalMapAtUnsafeDef);
}

TEST_CASE("ir lowerer call helpers keep bare non-semantic contains and tryAt on builtin dispatch") {
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "contains") {
      return std::string("/std/collections/map/contains");
    }
    if (expr.name == "tryAt") {
      return std::string("/std/collections/map/tryAt");
    }
    return expr.name;
  };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr containsCall;
  containsCall.kind = primec::Expr::Kind::Call;
  containsCall.name = "contains";
  containsCall.args = {valuesArg, keyArg};

  primec::Expr tryAtCall = containsCall;
  tryAtCall.name = "tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(containsCall, defMap, resolveExprPath) ==
        nullptr);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(tryAtCall, defMap, resolveExprPath) ==
        nullptr);
}

TEST_CASE("ir lowerer call helpers keep rooted map alias defs ahead of canonical non-semantic remaps") {
  primec::Definition canonicalMapCountDef;
  canonicalMapCountDef.fullPath = "/std/collections/map/count";
  primec::Definition canonicalMapContainsDef;
  canonicalMapContainsDef.fullPath = "/std/collections/map/contains";
  primec::Definition canonicalMapTryAtDef;
  canonicalMapTryAtDef.fullPath = "/std/collections/map/tryAt";
  primec::Definition aliasMapCountDef;
  aliasMapCountDef.fullPath = "/map/count";
  primec::Definition aliasMapContainsDef;
  aliasMapContainsDef.fullPath = "/map/contains";
  primec::Definition aliasMapTryAtDef;
  aliasMapTryAtDef.fullPath = "/map/tryAt";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {canonicalMapCountDef.fullPath, &canonicalMapCountDef},
      {canonicalMapContainsDef.fullPath, &canonicalMapContainsDef},
      {canonicalMapTryAtDef.fullPath, &canonicalMapTryAtDef},
      {aliasMapCountDef.fullPath, &aliasMapCountDef},
      {aliasMapContainsDef.fullPath, &aliasMapContainsDef},
      {aliasMapTryAtDef.fullPath, &aliasMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) {
    if (expr.name == "/map/count") {
      return std::string("/std/collections/map/count");
    }
    if (expr.name == "/map/contains") {
      return std::string("/std/collections/map/contains");
    }
    if (expr.name == "/map/tryAt") {
      return std::string("/std/collections/map/tryAt");
    }
    return expr.name;
  };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/map/count";
  aliasCountCall.args = {valuesArg};

  primec::Expr aliasContainsCall;
  aliasContainsCall.kind = primec::Expr::Kind::Call;
  aliasContainsCall.name = "/map/contains";
  aliasContainsCall.args = {valuesArg, keyArg};

  primec::Expr aliasTryAtCall = aliasContainsCall;
  aliasTryAtCall.name = "/map/tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasCountCall, defMap, resolveExprPath) ==
        &aliasMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasContainsCall, defMap, resolveExprPath) ==
        &aliasMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasTryAtCall, defMap, resolveExprPath) ==
        &aliasMapTryAtDef);
}

TEST_CASE("ir lowerer call helpers keep rooted map alias def families under same-path resolution") {
  primec::Definition aliasMapCountDef;
  aliasMapCountDef.fullPath = "/map/count__ov0";
  primec::Definition aliasMapContainsDef;
  aliasMapContainsDef.fullPath = "/map/contains__tmono";
  primec::Definition aliasMapTryAtDef;
  aliasMapTryAtDef.fullPath = "/map/tryAt__ov1";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {aliasMapCountDef.fullPath, &aliasMapCountDef},
      {aliasMapContainsDef.fullPath, &aliasMapContainsDef},
      {aliasMapTryAtDef.fullPath, &aliasMapTryAtDef},
  };
  const auto resolveExprPath = [](const primec::Expr &expr) { return expr.name; };

  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Literal;
  keyArg.intWidth = 32;
  keyArg.literalValue = 1;

  primec::Expr aliasCountCall;
  aliasCountCall.kind = primec::Expr::Kind::Call;
  aliasCountCall.name = "/map/count";
  aliasCountCall.args = {valuesArg};

  primec::Expr aliasContainsCall;
  aliasContainsCall.kind = primec::Expr::Kind::Call;
  aliasContainsCall.name = "/map/contains";
  aliasContainsCall.args = {valuesArg, keyArg};

  primec::Expr aliasTryAtCall = aliasContainsCall;
  aliasTryAtCall.name = "/map/tryAt";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasCountCall, defMap, resolveExprPath) ==
        &aliasMapCountDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasContainsCall, defMap, resolveExprPath) ==
        &aliasMapContainsDef);
  CHECK(primec::ir_lowerer::resolveDefinitionCall(aliasTryAtCall, defMap, resolveExprPath) ==
        &aliasMapTryAtDef);
}

TEST_CASE("ir lowerer call helpers keep lowered collection helper paths reachable via published surface ids") {
  primec::Definition loweredMapContainsDef;
  loweredMapContainsDef.fullPath = "/std/collections/mapContains";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {"/std/collections/mapContains", &loweredMapContainsDef},
  };
  const std::unordered_map<std::string, std::string> importAliases = {};

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 9,
      .sourceColumn = 4,
      .semanticNodeId = 81,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/mapContains"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      81, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      81, primec::StdlibSurfaceId::CollectionsMapHelpers);

  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(
          defMap,
          importAliases,
          &semanticProgram);

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/mapContains";
  callExpr.semanticNodeId = 81;

  CHECK(resolveExprPath(callExpr) == "/std/collections/mapContains");
  CHECK(primec::ir_lowerer::resolveDefinitionCall(callExpr, defMap, resolveExprPath) ==
        &loweredMapContainsDef);
}

TEST_CASE("ir lowerer call helpers classify lowered map helper overloads through semantic surface ids") {
  primec::Definition loweredMapContainsOverloadDef;
  loweredMapContainsOverloadDef.fullPath = "/std/collections/mapContains__ov1";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {loweredMapContainsOverloadDef.fullPath, &loweredMapContainsOverloadDef},
  };

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 10,
      .sourceColumn = 6,
      .semanticNodeId = 83,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, loweredMapContainsOverloadDef.fullPath),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      83, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      83, primec::StdlibSurfaceId::CollectionsMapHelpers);

  const auto resolveExprPath = [&](const primec::Expr &) {
    return loweredMapContainsOverloadDef.fullPath;
  };

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "contains";
  callExpr.semanticNodeId = 83;
  primec::Expr mapArg;
  mapArg.kind = primec::Expr::Kind::Name;
  mapArg.name = "values";
  primec::Expr keyArg;
  keyArg.kind = primec::Expr::Kind::Name;
  keyArg.name = "key";
  callExpr.args.push_back(mapArg);
  callExpr.args.push_back(keyArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            callExpr, defMap, resolveExprPath, &semanticProgram) == nullptr);
}

TEST_CASE("ir lowerer call helpers keep experimental map helper aliases off specialized method defs") {
  primec::Definition experimentalMapCountDef;
  experimentalMapCountDef.fullPath = "/std/collections/experimental_map/mapCount";
  primec::Definition specializedMapMethodDef;
  specializedMapMethodDef.fullPath = "/std/collections/experimental_map/Map__t1234/count";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {experimentalMapCountDef.fullPath, &experimentalMapCountDef},
      {specializedMapMethodDef.fullPath, &specializedMapMethodDef},
  };

  const auto resolveExprPath = [&](const primec::Expr &) {
    return specializedMapMethodDef.fullPath;
  };

  primec::Expr countCall;
  countCall.kind = primec::Expr::Kind::Call;
  countCall.name = "/std/collections/experimental_map/mapCount";
  primec::Expr valuesArg;
  valuesArg.kind = primec::Expr::Kind::Name;
  valuesArg.name = "values";
  countCall.args.push_back(valuesArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(countCall, defMap, resolveExprPath) ==
        &experimentalMapCountDef);

  primec::Expr bareCountCall = countCall;
  bareCountCall.name = "mapCount";

  CHECK(primec::ir_lowerer::resolveDefinitionCall(bareCountCall, defMap, resolveExprPath) == nullptr);

  primec::Definition specializedHelperDef;
  specializedHelperDef.fullPath = "/std/collections/experimental_map/mapCount__t1234";
  const std::unordered_map<std::string, const primec::Definition *> specializedDefMap = {
      {specializedHelperDef.fullPath, &specializedHelperDef},
  };
  const auto specializedResolveExprPath = [&](const primec::Expr &) {
    return specializedHelperDef.fullPath;
  };

  primec::Expr explicitSpecializedHelperCall;
  explicitSpecializedHelperCall.kind = primec::Expr::Kind::Call;
  explicitSpecializedHelperCall.name = specializedHelperDef.fullPath;
  explicitSpecializedHelperCall.args.push_back(valuesArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            explicitSpecializedHelperCall, specializedDefMap, specializedResolveExprPath) ==
        &specializedHelperDef);

  primec::Expr explicitUnspecializedHelperCall;
  explicitUnspecializedHelperCall.kind = primec::Expr::Kind::Call;
  explicitUnspecializedHelperCall.name = "/std/collections/experimental_map/mapCount";
  explicitUnspecializedHelperCall.args.push_back(valuesArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            explicitUnspecializedHelperCall, specializedDefMap, specializedResolveExprPath) ==
        &specializedHelperDef);

  primec::Definition competingRawHelperDef;
  competingRawHelperDef.fullPath = "/std/collections/experimental_map/mapCount";
  const std::unordered_map<std::string, const primec::Definition *> competingDefMap = {
      {competingRawHelperDef.fullPath, &competingRawHelperDef},
      {specializedHelperDef.fullPath, &specializedHelperDef},
      {specializedMapMethodDef.fullPath, &specializedMapMethodDef},
  };

  CHECK(primec::ir_lowerer::resolveDefinitionCall(
            explicitUnspecializedHelperCall, competingDefMap, resolveExprPath) ==
        &competingRawHelperDef);
}

TEST_CASE("ir lowerer call helpers prefer specialized rooted raw defs when semantic rooted rewrites miss") {
  primec::Definition specializedHelperDef;
  specializedHelperDef.fullPath =
      "/std/collections/internal_buffer_checked/bufferAlloc__t1234";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {
      {specializedHelperDef.fullPath, &specializedHelperDef},
  };

  const auto resolveExprPath = [&](const primec::Expr &) {
    return std::string("/std/collections/internal_buffer_checked/bufferAlloc");
  };

  primec::Expr allocCall;
  allocCall.kind = primec::Expr::Kind::Call;
  allocCall.name = specializedHelperDef.fullPath;
  allocCall.semanticNodeId = 41;
  primec::Expr countArg;
  countArg.kind = primec::Expr::Kind::Literal;
  countArg.literalValue = 1;
  allocCall.args.push_back(countArg);

  CHECK(primec::ir_lowerer::resolveDefinitionCall(allocCall, defMap, resolveExprPath) ==
        &specializedHelperDef);
}

TEST_CASE("ir lowerer bridge coverage uses published collection surface ids for lowered helper spellings") {
  primec::Program program;
  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "/std/collections/mapContains";
  callExpr.semanticNodeId = 82;
  mainDef.statements.push_back(callExpr);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.directCallTargets.push_back(primec::SemanticProgramDirectCallTarget{
      .scopePath = "/main",
      .callName = "contains",
      .sourceLine = 11,
      .sourceColumn = 7,
      .semanticNodeId = 82,
      .resolvedPathId = primec::semanticProgramInternCallTargetString(
          semanticProgram, "/std/collections/mapContains"),
      .stdlibSurfaceId = primec::StdlibSurfaceId::CollectionsMapHelpers,
  });
  semanticProgram.publishedRoutingLookups.directCallTargetIdsByExpr.insert_or_assign(
      82, semanticProgram.directCallTargets.front().resolvedPathId);
  semanticProgram.publishedRoutingLookups.directCallStdlibSurfaceIdsByExpr.insert_or_assign(
      82, primec::StdlibSurfaceId::CollectionsMapHelpers);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductBridgePathCoverage(
      program, &semanticProgram, error));
  CHECK(error.find("missing semantic-product bridge-path choice: /main -> /std/collections/mapContains") !=
        std::string::npos);
}

TEST_CASE("ir lowerer direct-call coverage uses published definition and import-alias lookups") {
  primec::Program program;
  program.imports.push_back("/pkg/*");

  primec::Definition helperDef;
  helperDef.fullPath = "/pkg/foo";
  helperDef.name = "foo";

  primec::Definition mainDef;
  mainDef.fullPath = "/main";
  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "foo";
  callExpr.semanticNodeId = 17;
  mainDef.statements.push_back(callExpr);

  program.definitions.push_back(helperDef);
  program.definitions.push_back(mainDef);

  primec::SemanticProgram semanticProgram;
  semanticProgram.imports = program.imports;
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "foo",
      .fullPath = "/pkg/foo",
      .namespacePrefix = "/pkg",
  });
  semanticProgram.definitions.push_back(primec::SemanticProgramDefinition{
      .name = "main",
      .fullPath = "/main",
      .namespacePrefix = "",
  });

  const auto helperPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/pkg/foo");
  const auto mainPathId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "/main");
  const auto aliasNameId =
      primec::semanticProgramInternCallTargetString(semanticProgram, "foo");
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      helperPathId, 0);
  semanticProgram.publishedRoutingLookups.definitionIndicesByPathId.insert_or_assign(
      mainPathId, 1);
  semanticProgram.publishedRoutingLookups.importAliasTargetPathIdsByNameId.insert_or_assign(
      aliasNameId, helperPathId);

  std::string error;
  CHECK_FALSE(primec::ir_lowerer::validateSemanticProductDirectCallCoverage(
      program, &semanticProgram, error));
  CHECK(error.find("missing semantic-product direct-call target: /main -> foo") !=
        std::string::npos);
}

TEST_CASE("ir lowerer call helpers keep semantic direct-call targets authoritative over rooted rewritten helper paths") {
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
      .stdlibSurfaceId = std::nullopt,
  });

  const auto resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(
          defMap,
          importAliases,
          &semanticProgram);

  primec::Expr rewrittenExpr;
  rewrittenExpr.kind = primec::Expr::Kind::Call;
  rewrittenExpr.name = "/std/math/quat_multiply_internal";
  rewrittenExpr.semanticNodeId = 42;

  CHECK(resolveExprPath(rewrittenExpr) == "/multiply");
  CHECK(primec::ir_lowerer::resolveDefinitionCall(rewrittenExpr, defMap, resolveExprPath) == nullptr);
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

TEST_CASE("ir lowerer call helpers reject missing tail-call resolver") {
  primec::Definition callee;
  callee.fullPath = "/callee";
  const std::unordered_map<std::string, const primec::Definition *> defMap = {{"/callee", &callee}};

  primec::Expr callExpr;
  callExpr.kind = primec::Expr::Kind::Call;
  callExpr.name = "callee";

  CHECK_FALSE(primec::ir_lowerer::isTailCallCandidate(
      callExpr, defMap, primec::ir_lowerer::ResolveExprPathFn{}));
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
  CHECK_FALSE(primec::ir_lowerer::hasTailExecutionCandidate(
      statements, true, primec::ir_lowerer::IsTailCallCandidateFn{}));

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

TEST_CASE("lowerer import aliases are delegated to frontend syntax helpers") {
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

  const std::filesystem::path frontendSyntaxHeaderPath =
      repoRoot / "include" / "primec" / "FrontendSyntax.h";
  const std::filesystem::path frontendSyntaxSourcePath = repoRoot / "src" / "FrontendSyntax.cpp";
  const std::filesystem::path lowerImportsStructsSetupSourcePath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererLowerImportsStructsSetup.cpp";
  const std::filesystem::path structTypeHelpersSourcePath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererStructTypeHelpers.cpp";
  const std::filesystem::path cmakePath = repoRoot / "CMakeLists.txt";

  REQUIRE(std::filesystem::exists(frontendSyntaxHeaderPath));
  REQUIRE(std::filesystem::exists(frontendSyntaxSourcePath));
  REQUIRE(std::filesystem::exists(lowerImportsStructsSetupSourcePath));
  REQUIRE(std::filesystem::exists(structTypeHelpersSourcePath));
  REQUIRE(std::filesystem::exists(cmakePath));

  const std::string frontendSyntaxHeader = readText(frontendSyntaxHeaderPath);
  const std::string frontendSyntaxSource = readText(frontendSyntaxSourcePath);
  const std::string lowerImportsStructsSetupSource = readText(lowerImportsStructsSetupSourcePath);
  const std::string structTypeHelpersSource = readText(structTypeHelpersSourcePath);
  const std::string cmake = readText(cmakePath);

  CHECK(frontendSyntaxHeader.find("buildSyntaxImportAliases(") != std::string::npos);
  CHECK(frontendSyntaxHeader.find("normalizeSyntaxImportAliasTargetPath(") != std::string::npos);
  CHECK(frontendSyntaxSource.find("std::unordered_map<std::string, std::string> buildSyntaxImportAliases(") !=
        std::string::npos);
  CHECK(frontendSyntaxSource.find("isSyntaxWildcardImportPath(importPath, wildcardPrefix)") !=
        std::string::npos);
  CHECK(cmake.find("src/FrontendSyntax.cpp") != std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("#include \"primec/FrontendSyntax.h\"") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find("primec::buildSyntaxImportAliases(") !=
        std::string::npos);
  CHECK(lowerImportsStructsSetupSource.find(
            "buildImportAliasesFromProgram(program.imports, program.definitions, defMapOut)") ==
        std::string::npos);
  CHECK(structTypeHelpersSource.find("return primec::buildSyntaxImportAliases(") !=
        std::string::npos);
  CHECK(structTypeHelpersSource.find("normalizeMapImportAliasPath(") == std::string::npos);
}

TEST_SUITE_END();
