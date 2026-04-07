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
  const std::filesystem::path countAccessClassifiersPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererCountAccessClassifiers.cpp";
  const std::filesystem::path nativeTailDispatchPath =
      repoRoot / "src" / "ir_lowerer" / "IrLowererNativeTailDispatch.cpp";
  const std::filesystem::path astCallPathHelpersPath =
      repoRoot / "include" / "primec" / "AstCallPathHelpers.h";
  REQUIRE(std::filesystem::exists(callHelpersPath));
  REQUIRE(std::filesystem::exists(accessTargetResolutionPath));
  REQUIRE(std::filesystem::exists(accessLoadHelpersPath));
  REQUIRE(std::filesystem::exists(indexedAccessEmitPath));
  REQUIRE(std::filesystem::exists(callResolutionPath));
  REQUIRE(std::filesystem::exists(inlineDispatchPath));
  REQUIRE(std::filesystem::exists(countAccessClassifiersPath));
  REQUIRE(std::filesystem::exists(nativeTailDispatchPath));
  REQUIRE(std::filesystem::exists(astCallPathHelpersPath));
  const std::string callHelpersSource = readText(callHelpersPath);
  const std::string accessTargetResolutionSource = readText(accessTargetResolutionPath);
  const std::string accessLoadHelpersSource = readText(accessLoadHelpersPath);
  const std::string indexedAccessEmitSource = readText(indexedAccessEmitPath);
  const std::string callResolutionSource = readText(callResolutionPath);
  const std::string inlineDispatchSource = readText(inlineDispatchPath);
  const std::string countAccessClassifiersSource = readText(countAccessClassifiersPath);
  const std::string nativeTailDispatchSource = readText(nativeTailDispatchPath);
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
  CHECK(emitterCollectionInferenceSource.find("soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("soa_vector borrowed views are not implemented yet: ref") ==
        std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("soaVectorGet") == std::string::npos);
  CHECK(emitterCollectionInferenceSource.find("soaVectorRef") == std::string::npos);

  CHECK(inlineDispatchSource.find("field_view") == std::string::npos);
  CHECK(inlineDispatchSource.find("soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("soa_vector borrowed views are not implemented yet: ref") ==
        std::string::npos);
  CHECK(inlineDispatchSource.find("soaVectorGet") == std::string::npos);
  CHECK(inlineDispatchSource.find("soaVectorRef") == std::string::npos);

  CHECK(countAccessClassifiersSource.find("field_view") == std::string::npos);
  CHECK(countAccessClassifiersSource.find("soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("soa_vector borrowed views are not implemented yet: ref") ==
        std::string::npos);
  CHECK(countAccessClassifiersSource.find("soaVectorGet") == std::string::npos);
  CHECK(countAccessClassifiersSource.find("soaVectorRef") == std::string::npos);

  CHECK(nativeTailDispatchSource.find("field_view") == std::string::npos);
  CHECK(nativeTailDispatchSource.find("soa_vector field views are not implemented yet: ") ==
        std::string::npos);
  CHECK(nativeTailDispatchSource.find("soa_vector borrowed views are not implemented yet: ref") ==
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
      "/main",
      "callee",
      "/callee",
      0,
      0,
      17,
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
      "/main",
      "multiply",
      "/multiply",
      0,
      0,
      71,
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
      "/main",
      "/legacy",
      "/semantic/target",
      0,
      0,
      19,
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
      "/main",
      "contains",
      "",
      "/std/collections/map/contains",
      0,
      0,
      44,
  });
  semanticTargets = primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  resolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(resolveExprPath(methodExpr) == "/std/collections/map/contains");
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
      "/main",
      "count",
      "/vector/count",
      0,
      0,
      18,
  });
  auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  auto semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(semanticResolveExprPath(callExpr).empty());

  semanticProgram.bridgePathChoices.push_back(primec::SemanticProgramBridgePathChoice{
      "/main",
      "vector",
      "count",
      "/vector/count",
      0,
      0,
      18,
  });
  semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);
  semanticResolveExprPath =
      primec::ir_lowerer::makeResolveCallPathFromScope(defMap, importAliases, semanticTargets);
  CHECK(semanticResolveExprPath(callExpr) == "/vector/count");
}

TEST_CASE("ir lowerer semantic-product adapter joins return and inference facts by semantic id only") {
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
      "/legacy_main",
      "return",
      "/i32",
      "i32",
      false,
      false,
      false,
      "",
      9,
      3,
      61,
  });
  semanticProgram.localAutoFacts.push_back(primec::SemanticProgramLocalAutoFact{
      "/main",
      "selected",
      "i32",
      "/id",
      "i32",
      "",
      "i32",
      false,
      "",
      "",
      false,
      "",
      "",
      "",
      "",
      "",
      "",
      "return",
      "",
      "",
      0,
      10,
      5,
      62,
      0,
      "",
      "",
  });
  semanticProgram.queryFacts.push_back(primec::SemanticProgramQueryFact{
      "/main",
      "lookup",
      "/lookup",
      "Result<i32, FileError>",
      "Result<i32, FileError>",
      "",
      true,
      true,
      "i32",
      "FileError",
      11,
      7,
      63,
  });
  semanticProgram.tryFacts.push_back(primec::SemanticProgramTryFact{
      "/main",
      "/lookup",
      "Result<i32, FileError>",
      "",
      "Result<i32, FileError>",
      "i32",
      "FileError",
      "return",
      "/handler",
      "FileError",
      1,
      12,
      9,
      64,
  });

  const auto semanticTargets =
      primec::ir_lowerer::buildSemanticProductTargetAdapter(&semanticProgram);

  const auto *returnFact = primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, mainDef);
  REQUIRE(returnFact != nullptr);
  CHECK(returnFact->definitionPath == "/legacy_main");

  primec::Definition legacyFixtureDef;
  legacyFixtureDef.fullPath = "/legacy_main";
  CHECK(primec::ir_lowerer::findSemanticProductReturnFact(semanticTargets, legacyFixtureDef) == nullptr);

  const auto *localAutoFact = primec::ir_lowerer::findSemanticProductLocalAutoFact(semanticTargets, localAutoExpr);
  REQUIRE(localAutoFact != nullptr);
  CHECK(localAutoFact->bindingName == "selected");

  const auto *queryFact = primec::ir_lowerer::findSemanticProductQueryFact(semanticTargets, queryExpr);
  REQUIRE(queryFact != nullptr);
  CHECK(queryFact->resolvedPath == "/lookup");

  const auto *tryFact = primec::ir_lowerer::findSemanticProductTryFact(semanticTargets, tryExpr);
  REQUIRE(tryFact != nullptr);
  CHECK(tryFact->onErrorHandlerPath == "/handler");
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
      "/main",
      "multiply",
      "/multiply",
      12,
      3,
      42,
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
