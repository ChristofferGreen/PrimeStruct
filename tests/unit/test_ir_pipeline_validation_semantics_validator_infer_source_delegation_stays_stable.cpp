#include "test_ir_pipeline_validation_helpers.h"

TEST_SUITE_BEGIN("primestruct.ir.pipeline.validation");

TEST_CASE("semantics validator infer source delegation stays stable") {
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

  const std::filesystem::path semanticsInferPath = repoRoot / "src" / "semantics" / "SemanticsValidatorInfer.cpp";
  const std::filesystem::path semanticsInferCollectionCountCapacityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionCountCapacity.cpp";
  const std::filesystem::path semanticsInferCollectionDirectCountCapacityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionDirectCountCapacity.cpp";
  const std::filesystem::path semanticsInferCollectionDispatchPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionDispatch.cpp";
  const std::filesystem::path semanticsInferCollectionDispatchSetupPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionDispatchSetup.cpp";
  const std::filesystem::path semanticsBuiltinPathHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsBuiltinPathHelpers.cpp";
  const std::filesystem::path semanticsInferLateFallbackBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferLateFallbackBuiltins.cpp";
  const std::filesystem::path semanticsInferPreDispatchCallsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferPreDispatchCalls.cpp";
  const std::filesystem::path semanticsInferScalarBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferScalarBuiltins.cpp";
  const std::filesystem::path semanticsInferResolvedCallsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferResolvedCalls.cpp";
  const std::filesystem::path semanticsInferCollectionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollections.cpp";
  const std::filesystem::path semanticsInferCollectionCompatibilityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionCompatibility.cpp";
  const std::filesystem::path semanticsEffectFreeCollectionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorEffectFreeCollections.cpp";
  const std::filesystem::path semanticsInferCollectionReturnInferencePath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionReturnInference.cpp";
  const std::filesystem::path semanticsInferCollectionCallResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionCallResolution.cpp";
  const std::filesystem::path semanticsInferCollectionBufferAndMapResolversPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp";
  const std::filesystem::path semanticsInferCollectionStringResolverPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferCollectionStringResolver.cpp";
  const std::filesystem::path semanticsInferControlFlowPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferControlFlow.cpp";
  const std::filesystem::path semanticsInferDefinitionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferDefinition.cpp";
  const std::filesystem::path semanticsInferGraphPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
  const std::filesystem::path semanticsDiagnosticsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorDiagnostics.cpp";
  const std::filesystem::path semanticsInferMethodResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  const std::filesystem::path semanticsInferStructReturnPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferStructReturn.cpp";
  const std::filesystem::path semanticsInferTargetResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferTargetResolution.cpp";
  const std::filesystem::path semanticsInferUtilityPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorInferUtility.cpp";
  const std::filesystem::path semanticsCollectionHelperRewritesPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorCollectionHelperRewrites.cpp";
  REQUIRE(std::filesystem::exists(semanticsInferPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionCountCapacityPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionDirectCountCapacityPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionDispatchPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionDispatchSetupPath));
  REQUIRE(std::filesystem::exists(semanticsBuiltinPathHelpersPath));
  REQUIRE(std::filesystem::exists(semanticsInferLateFallbackBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsInferPreDispatchCallsPath));
  REQUIRE(std::filesystem::exists(semanticsInferScalarBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsInferResolvedCallsPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionsPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionCompatibilityPath));
  REQUIRE(std::filesystem::exists(semanticsEffectFreeCollectionsPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionReturnInferencePath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionCallResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionBufferAndMapResolversPath));
  REQUIRE(std::filesystem::exists(semanticsInferCollectionStringResolverPath));
  REQUIRE(std::filesystem::exists(semanticsInferControlFlowPath));
  REQUIRE(std::filesystem::exists(semanticsInferDefinitionPath));
  REQUIRE(std::filesystem::exists(semanticsInferGraphPath));
  REQUIRE(std::filesystem::exists(semanticsDiagnosticsPath));
  REQUIRE(std::filesystem::exists(semanticsInferMethodResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsInferStructReturnPath));
  REQUIRE(std::filesystem::exists(semanticsInferTargetResolutionPath));
  REQUIRE(std::filesystem::exists(semanticsInferUtilityPath));
  REQUIRE(std::filesystem::exists(semanticsCollectionHelperRewritesPath));
  const std::string semanticsInferSource = readText(semanticsInferPath);
  const std::string semanticsDiagnosticsSource = readText(semanticsDiagnosticsPath);
  const std::string semanticsInferCombinedSource = readTexts({
      semanticsInferPath,
      semanticsInferCollectionCountCapacityPath,
      semanticsInferCollectionDirectCountCapacityPath,
      semanticsInferCollectionDispatchPath,
      semanticsInferCollectionDispatchSetupPath,
      semanticsInferLateFallbackBuiltinsPath,
      semanticsInferPreDispatchCallsPath,
      semanticsInferScalarBuiltinsPath,
      semanticsInferResolvedCallsPath,
      semanticsInferCollectionsPath,
      semanticsInferCollectionCompatibilityPath,
      semanticsInferCollectionReturnInferencePath,
      semanticsInferCollectionCallResolutionPath,
      semanticsInferCollectionBufferAndMapResolversPath,
      semanticsInferCollectionStringResolverPath,
      semanticsInferControlFlowPath,
      semanticsInferDefinitionPath,
      semanticsInferGraphPath,
      semanticsInferMethodResolutionPath,
      semanticsInferStructReturnPath,
      semanticsInferTargetResolutionPath,
      semanticsInferUtilityPath,
      semanticsCollectionHelperRewritesPath,
  });
  const std::string semanticsInferCollectionCountCapacitySource =
      readText(semanticsInferCollectionCountCapacityPath);
  const std::string semanticsInferCollectionDirectCountCapacitySource =
      readText(semanticsInferCollectionDirectCountCapacityPath);
  const std::string semanticsInferCollectionDispatchSource = readText(semanticsInferCollectionDispatchPath);
  const std::string semanticsInferCollectionDispatchSetupSource =
      readText(semanticsInferCollectionDispatchSetupPath);
  const std::string semanticsBuiltinPathHelpersSource =
      readText(semanticsBuiltinPathHelpersPath);
  const std::string semanticsInferLateFallbackBuiltinsSource =
      readText(semanticsInferLateFallbackBuiltinsPath);
  const std::string semanticsInferMethodResolutionSource =
      readText(semanticsInferMethodResolutionPath);
  const std::string semanticsInferPreDispatchCallsSource =
      readText(semanticsInferPreDispatchCallsPath);
  const std::string semanticsInferScalarBuiltinsSource =
      readText(semanticsInferScalarBuiltinsPath);
  const std::string semanticsInferResolvedCallsSource =
      readText(semanticsInferResolvedCallsPath);
  const std::string semanticsInferCollectionsSource = readTexts({
      semanticsInferCollectionsPath,
      semanticsInferCollectionCompatibilityPath,
      semanticsInferCollectionReturnInferencePath,
      semanticsInferCollectionCallResolutionPath,
      semanticsInferCollectionBufferAndMapResolversPath,
      semanticsInferCollectionStringResolverPath,
  });
  const std::string semanticsEffectFreeCollectionsSource =
      readText(semanticsEffectFreeCollectionsPath);
  const std::string semanticsInferCollectionCompatibilitySource =
      readText(semanticsInferCollectionCompatibilityPath);
  const std::string semanticsInferControlFlowSource = readText(semanticsInferControlFlowPath);
  const std::string semanticsInferDefinitionSource = readText(semanticsInferDefinitionPath);
  const std::string semanticsCollectionHelperRewritesSource = readText(semanticsCollectionHelperRewritesPath);
  CHECK(semanticsInferCombinedSource.find("ReturnKind SemanticsValidator::inferExprReturnKind") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferControlFlowExprReturnKind(expr, params, locals, handledControlFlow);") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const BuiltinCollectionDispatchResolvers builtinCollectionDispatchResolvers") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("InferCollectionDispatchSetup inferCollectionDispatchSetup;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("prepareInferCollectionDispatchSetup(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferLateFallbackReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferPreDispatchCallReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferScalarBuiltinReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferResolvedCallReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("setupOut.builtinCollectionCountCapacityDispatchContext.isCountLike =") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountCall =") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("makeBuiltinCollectionDispatchResolvers(params, locals, ") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveIndexedArgsPackElementType =") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("builtinCollectionDispatchResolvers.resolveIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveDereferencedIndexedArgsPackElementType =") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("resolveDereferencedIndexedArgsPackElementType =") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveWrappedIndexedArgsPackElementType =") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("builtinCollectionDispatchResolvers.resolveWrappedIndexedArgsPackElementType;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveArgsPackAccessTarget = builtinCollectionDispatchResolvers.resolveArgsPackAccessTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveArrayTarget = builtinCollectionDispatchResolvers.resolveArrayTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveVectorTarget = builtinCollectionDispatchResolvers.resolveVectorTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveSoaVectorTarget = builtinCollectionDispatchResolvers.resolveSoaVectorTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveBufferTarget = builtinCollectionDispatchResolvers.resolveBufferTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveStringTarget = builtinCollectionDispatchResolvers.resolveStringTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("const auto &resolveMapTarget = builtinCollectionDispatchResolvers.resolveMapTarget;") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::resolveCallCollectionTypePath(") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::resolveCallCollectionTemplateArgs(") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "const std::string resolvedSoaToAosCanonical =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "canonicalizeLegacySoaToAosHelperPath(resolveCalleePath(target))") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "const bool matchesSoaToAosTarget =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "const bool matchesBorrowedSoaToAosTarget =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "resolvedSoaToAosCanonical, \"to_aos\")") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "resolvedSoaToAosCanonical, \"to_aos_ref\")") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "isCanonicalSoaToAosHelperPath(resolvedSoaToAosCanonical)") ==
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "(matchesSoaToAosTarget || matchesBorrowedSoaToAosTarget)") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "resolveCalleePath(target) == \"/to_aos\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "resolveCalleePath(target) == \"/to_aos_ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "resolveCalleePath(target) == \"/soa_vector/to_aos_ref\"") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveDereferencedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveWrappedIndexedArgsPackElementType = [&](const Expr &target, std::string &elemTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveArgsPackAccessTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveArrayTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveVectorTarget = [&](const Expr &target, std::string &elemType)") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveSoaVectorTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveBufferTarget = [&](const Expr &target, std::string &elemType) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("std::function<bool(const Expr &)> resolveStringTarget =") == std::string::npos);
  CHECK(semanticsInferSource.find("std::function<bool(const Expr &, std::string &, std::string &)> resolveMapTarget =") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveBuiltinAccessReceiverExprInline = [&](const Expr &accessExpr) -> const Expr * {") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find("resolvedOut = \"/soa_vector/count\";") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "resolvedOut =\n"
            "            preferredSoaHelperTargetForCollectionType(\"count\", \"/vector\");") !=
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find("resolvedOut = \"/soa_vector/get\";") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "resolvedOut = preferredSoaHelperTargetForCollectionType(") !=
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find(
            "normalizedMethodPath == \"vector/at_unsafe\"") ==
        std::string::npos);
  CHECK(semanticsInferMethodResolutionSource.find("resolvedOut = \"/soa_vector/ref\";") ==
        std::string::npos);
  CHECK((semanticsInferMethodResolutionSource.find(
            "\"ref\",\n"
            "          collectionTypePath == \"/soa_vector\" ? \"/soa_vector\" : \"/vector\");") !=
         std::string::npos ||
         semanticsInferMethodResolutionSource.find(
             "normalizedMethodName,\n"
             "          collectionTypePath == \"/soa_vector\" ? \"/soa_vector\" : \"/vector\");") !=
             std::string::npos));
  CHECK(semanticsInferMethodResolutionSource.find("resolvedOut = \"/to_aos\";") ==
        std::string::npos);
  CHECK((semanticsInferMethodResolutionSource.find(
            "\"to_aos\",\n"
            "          collectionTypePath == \"/soa_vector\" ? \"/soa_vector\" : \"/vector\");") !=
         std::string::npos ||
         semanticsInferMethodResolutionSource.find(
             "normalizedMethodName,\n"
             "          collectionTypePath == \"/soa_vector\" ? \"/soa_vector\" : \"/vector\");") !=
             std::string::npos));
  CHECK(semanticsInferSource.find("auto extractWrappedPointeeType = [&](const std::string &typeText, std::string &pointeeTypeOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto extractCollectionElementType = [&](const std::string &typeText,") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto inferTargetTypeText = [&](const Expr &candidate, std::string &typeTextOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto matchesDirectMapConstructorPath = [&](std::string_view basePath)") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto extractAnyMapKeyValueTypes = [&](const BindingInfo &binding,") ==
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("resolveBuiltinCollectionMethodReturnKind(") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("resolveBuiltinCollectionAccessCallReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("builtinCollectionCountCapacityDispatchContext") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferBuiltinCollectionDirectCountCapacityReturnKind(") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("handledDirectBuiltinCountCapacity") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("DefinitionReturnInferenceState inferenceState;") != std::string::npos);
  CHECK(semanticsInferCombinedSource.find("inferDefinitionStatementReturns(def, defParams, stmt, locals, inferenceState)") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("recordDefinitionInferredReturn(def, &stmt, defParams, locals, inferenceState)") !=
        std::string::npos);
  CHECK(semanticsInferSource.find("resolveMethodCallPath(\"count\"") == std::string::npos);
  CHECK(semanticsInferSource.find("resolveMethodCallPath(\"capacity\"") == std::string::npos);
  CHECK(semanticsInferCombinedSource.find("methodRemovedCollectionCompatibilityPath(") !=
        std::string::npos);
  CHECK(semanticsInferCombinedSource.find("preferVectorStdlibHelperPath(resolveCalleePath(expr))") !=
        std::string::npos);
  CHECK(semanticsInferSource.find("auto preferVectorStdlibHelperPathForCall = [&](const std::string &path)") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto mapHelperReceiverIndex = [&](const Expr &candidate) -> size_t {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto bareMapHelperOperandIndices = [&](const Expr &candidate,") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto preferredBareMapHelperTarget = [&](std::string_view helperName)") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto tryRewriteBareMapHelperCall = [&](const Expr &candidate,") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto explicitRemovedCollectionMethodPath = [&](const Expr &candidate) -> std::string {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("std::function<bool(const Expr &, std::unordered_map<std::string, BindingInfo> &)> inferStatement;") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveBuiltinCollectionMethodReturnKind = [&]") == std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveBuiltinCollectionAccessCallReturnKind = [&]") == std::string::npos);
  CHECK(semanticsInferSource.find("auto resolveBuiltinCollectionCountCapacityReturnKind = [&]") == std::string::npos);
  CHECK(semanticsInferSource.find("std::string namespacedCollection;") == std::string::npos);
  CHECK(semanticsInferSource.find("const bool isStdNamespacedVectorCountCall =") == std::string::npos);
  CHECK(semanticsInferSource.find("const bool shouldDeferResolvedNamespacedCollectionHelperReturn =") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("BuiltinCollectionCountCapacityDispatchContext builtinCollectionCountCapacityDispatchContext;") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("BuiltinCollectionDirectCountCapacityContext builtinCollectionDirectCountCapacityContext;") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("const bool isBuiltinGet = isSimpleCallName(expr, \"get\");") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (!expr.isMethodCall && (isSimpleCallName(expr, \"to_soa\") || isSimpleCallName(expr, \"to_aos\")) &&") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (getBuiltinGpuName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (!expr.isMethodCall && isSimpleCallName(expr, \"buffer_load\") && expr.args.size() == 2) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("std::function<ReturnKind(const Expr &)> pointerTargetKind =") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("const std::string directRemovedMapCompatibilityPath =") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (getBuiltinOperatorName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (getBuiltinMutationName(expr, builtinName)) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (isAssignCall(expr)) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("auto isTypeNamespaceMethodCall = [&](const Expr &callExpr, const std::string &resolvedPath) -> bool {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (!validateNamedArgumentsAgainstParams(calleeParams, *orderedCallArgNames, orderedArgError)) {") ==
        std::string::npos);
  CHECK(semanticsInferSource.find("if (!ensureDefinitionReturnKindReady(*defIt->second)) {") ==
        std::string::npos);
  CHECK(semanticsInferCollectionCountCapacitySource.find("bool SemanticsValidator::resolveBuiltinCollectionCountCapacityReturnKind") !=
        std::string::npos);
  CHECK(semanticsInferCollectionCountCapacitySource.find(
            "auto failInferCountCapacityDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find("ReturnKind SemanticsValidator::inferBuiltinCollectionDirectCountCapacityReturnKind") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find(
            "auto failInferDirectCountCapacityDiagnostic =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find(
            "(void)failExprDiagnostic(expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find(
            "auto rejectsRootedVectorBuiltinAlias =") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find(
            "auto rejectsRootedVectorCountNonVectorAlias =") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find("const auto inferHelperReturnKind = [&](const std::string &helperName,") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find("if (context.isDirectCountCall)") != std::string::npos);
  CHECK(semanticsInferCollectionDirectCountCapacitySource.find("if (context.isDirectCapacitySingleArg && context.resolveVectorTarget != nullptr)") !=
        std::string::npos);
  CHECK(semanticsInferCollectionCountCapacitySource.find("context.isCountLike && methodResolved == \"/std/collections/map/count\"") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("bool SemanticsValidator::resolveBuiltinCollectionMethodReturnKind") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("bool SemanticsValidator::resolveBuiltinCollectionAccessCallReturnKind") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "const auto canonicalizeSoaMethodResolvedPath = [](std::string path)") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "canonicalizeLegacySoaGetHelperPath(resolvedPath)") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "std::string resolvedSoaCanonical =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("resolvedPath == \"/soa_vector/get\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("resolvedPath == \"/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("resolvedPath == \"/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("resolvedPath == \"/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "isCanonicalSoaRefLikeHelperPath(resolvedSoaCanonical)") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"get\")") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "isLegacyOrCanonicalSoaHelperPath(resolvedSoaCanonical, \"get_ref\")") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "isExperimentalSoaBorrowedHelperPath(resolvedSoaCanonical)") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/get\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/get_ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "resolvedSoaCanonical == \"/std/collections/soa_vector/ref_ref\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "resolvedSoaCanonical == \"/std/collections/experimental_soa_vector/soaVectorGetRef\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "resolvedSoaCanonical == \"/std/collections/experimental_soa_vector/soaVectorRefRef\"") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "auto tryResolveReceiverIndex = [&](size_t index) -> bool {") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find("std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "auto appendReceiverIndex = [&](size_t index)") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSource.find(
            "for (size_t receiverIndex : receiverIndices)") ==
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSetupSource.find("void SemanticsValidator::prepareInferCollectionDispatchSetup") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSetupSource.find("std::string namespacedCollection;") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSetupSource.find("const bool isStdNamespacedVectorCountCall =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSetupSource.find("setupOut.shouldDeferResolvedNamespacedCollectionHelperReturn =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSetupSource.find("setupOut.builtinCollectionCountCapacityDispatchContext.isCountLike =") !=
        std::string::npos);
  CHECK(semanticsInferCollectionDispatchSetupSource.find("setupOut.builtinCollectionDirectCountCapacityContext.isDirectCountCall =") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find("ReturnKind SemanticsValidator::inferLateFallbackReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "auto failInferLateFallbackDiagnostic = [&](std::string message) -> ReturnKind {") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "(void)failExprDiagnostic(expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find("const bool isBuiltinGet = isSimpleCallName(expr, \"get\");") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "const bool isBuiltinGetRef = isSimpleCallName(expr, \"get_ref\");") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "auto tryResolveReceiverIndex = [&](size_t index,") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find("std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "auto appendReceiverIndex = [&](size_t index)") ==
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "for (size_t receiverIndex : receiverIndices)") ==
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "(helperName == \"get\" || helperName == \"get_ref\" ||") !=
            std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "isSimpleCallName(expr, \"to_soa\") || isSimpleCallName(expr, \"to_aos\")") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find("if (getBuiltinGpuName(expr, builtinName)) {") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "if (!expr.isMethodCall && isSimpleCallName(expr, \"buffer_load\") &&") !=
        std::string::npos);
  CHECK(semanticsInferLateFallbackBuiltinsSource.find(
            "soaUnavailableMethodDiagnostic(methodResolved));") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find("ReturnKind SemanticsValidator::inferPreDispatchCallReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find(
            "auto failInferPreDispatchDiagnostic = [&](std::string message) -> ReturnKind {") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find(
            "(void)failExprDiagnostic(expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find("std::function<ReturnKind(const Expr &)> pointerTargetKind =") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find("const std::string directRemovedMapCompatibilityPath =") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find("if (getVectorStatementHelperName(expr, vectorHelper) && !expr.args.empty()) {") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find(
            "auto tryResolveReceiverIndex = [&](size_t index) -> std::optional<ReturnKind> {") !=
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find("std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find(
            "auto appendReceiverIndex = [&](size_t index)") ==
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find(
            "for (size_t receiverIndex : receiverIndices)") ==
        std::string::npos);
  CHECK(semanticsInferPreDispatchCallsSource.find(
            "soaUnavailableMethodDiagnostic(methodResolved));") !=
        std::string::npos);
  CHECK(semanticsBuiltinPathHelpersSource.find(
            "std::string soaFieldViewOrUnknownMethodDiagnostic(std::string_view resolvedPath)") ==
        std::string::npos);
  CHECK(semanticsInferScalarBuiltinsSource.find("ReturnKind SemanticsValidator::inferScalarBuiltinReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferScalarBuiltinsSource.find("if (getBuiltinPointerName(expr, builtinName) && expr.args.size() == 1) {") !=
        std::string::npos);
  CHECK(semanticsInferScalarBuiltinsSource.find("if (getBuiltinMathName(expr, builtinName, allowMathBareName(expr.name))) {") !=
        std::string::npos);
  CHECK(semanticsInferScalarBuiltinsSource.find("if (getBuiltinOperatorName(expr, builtinName)) {") !=
        std::string::npos);
  CHECK(semanticsInferScalarBuiltinsSource.find("if (getBuiltinMutationName(expr, builtinName)) {") !=
        std::string::npos);
  CHECK(semanticsInferScalarBuiltinsSource.find("if (isAssignCall(expr)) {") !=
        std::string::npos);
  CHECK(semanticsInferResolvedCallsSource.find("ReturnKind SemanticsValidator::inferResolvedCallReturnKind(") !=
        std::string::npos);
  CHECK(semanticsInferResolvedCallsSource.find(
            "auto failInferResolvedCallDiagnostic = [&](std::string message) -> ReturnKind {") !=
        std::string::npos);
  CHECK(semanticsInferResolvedCallsSource.find(
            "(void)failExprDiagnostic(expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsInferResolvedCallsSource.find(
            "auto isTypeNamespaceMethodCall = [&](const Expr &callExpr,") !=
        std::string::npos);
  CHECK(semanticsInferResolvedCallsSource.find("validateNamedArgumentsAgainstParams(") !=
        std::string::npos);
  CHECK(semanticsInferResolvedCallsSource.find("if (!ensureDefinitionReturnKindReady(*defIt->second)) {") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("std::string SemanticsValidator::normalizeCollectionTypePath") !=
        std::string::npos);
  CHECK(semanticsCollectionHelperRewritesSource.find("bool SemanticsValidator::tryRewriteBareMapHelperCall") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::hasImportedDefinitionPath") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::inferDefinitionReturnBinding") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("std::string SemanticsValidator::methodRemovedCollectionCompatibilityPath") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::getVectorStatementHelperName") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("std::string SemanticsValidator::getDirectVectorHelperCompatibilityPath") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::resolveCallCollectionTypePath") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("bool SemanticsValidator::resolveCallCollectionTemplateArgs") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "matchesCollectionCtorPath(\"/std/collections/vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "matchesCollectionCtorPath(\"/std/collections/experimental_vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsInferCollectionCompatibilitySource.find(
            "auto tryResolveReceiverIndex = [&](size_t index) -> bool {") !=
        std::string::npos);
  CHECK(semanticsInferCollectionCompatibilitySource.find(
            "std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsInferCollectionCompatibilitySource.find(
            "auto appendReceiverIndex = [&](size_t index)") ==
        std::string::npos);
  CHECK(semanticsInferCollectionCompatibilitySource.find(
            "for (size_t receiverIndex : receiverIndices)") ==
        std::string::npos);
  CHECK(semanticsEffectFreeCollectionsSource.find(
            "std::string SemanticsValidator::resolveEffectFreeBareMapCallPath(") !=
        std::string::npos);
  CHECK(semanticsEffectFreeCollectionsSource.find(
            "auto tryResolveReceiverIndex = [&](size_t index) -> bool {") !=
        std::string::npos);
  CHECK(semanticsEffectFreeCollectionsSource.find(
            "std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsEffectFreeCollectionsSource.find(
            "auto appendReceiverIndex = [&](size_t index)") ==
        std::string::npos);
  CHECK(semanticsEffectFreeCollectionsSource.find(
            "for (size_t receiverIndex : receiverIndices)") ==
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("SemanticsValidator::makeBuiltinCollectionDispatchResolvers(") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("auto resolveBindingTarget = [") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("auto inferCallBinding = [") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("auto isDirectMapConstructorCall = [") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find(
            "this->resolveSoaVectorOrExperimentalBorrowedReceiver(\n"
            "            target, params, locals, resolveDirectReceiver, elemType)") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("auto resolveInlineBorrowedSoaBinding =") ==
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("state->resolveSoaVectorTarget = [") !=
        std::string::npos);
  CHECK(semanticsInferCollectionsSource.find("state->resolveBufferTarget = [") !=
        std::string::npos);
  CHECK(semanticsInferControlFlowSource.find("ReturnKind combineNumericReturnKinds") != std::string::npos);
  CHECK(semanticsInferControlFlowSource.find("ReturnKind SemanticsValidator::inferControlFlowExprReturnKind") !=
        std::string::npos);
  CHECK(semanticsInferControlFlowSource.find("if (isMatchCall(expr))") != std::string::npos);
  CHECK(semanticsInferControlFlowSource.find("if (isIfCall(expr) && expr.args.size() == 3)") != std::string::npos);
  CHECK(semanticsInferControlFlowSource.find("if (isBlockCall(expr) && expr.hasBodyArguments)") !=
        std::string::npos);
  CHECK(semanticsInferDefinitionSource.find("bool SemanticsValidator::recordDefinitionInferredReturn") !=
        std::string::npos);
  CHECK(semanticsInferDefinitionSource.find(
            "auto failInferDefinitionDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsInferDefinitionSource.find("bool SemanticsValidator::inferDefinitionStatementReturns") !=
        std::string::npos);
  CHECK(semanticsInferDefinitionSource.find(
            "auto failInferDefinitionStatementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsInferDefinitionSource.find("if (isForCall(stmt) && stmt.args.size() == 4)") != std::string::npos);
  CHECK(semanticsDiagnosticsSource.find(
            "bool SemanticsValidator::definitionDiagnosticOrderLess(") !=
        std::string::npos);
  CHECK(semanticsDiagnosticsSource.find(
            "void SemanticsValidator::sortDefinitionsForDiagnosticOrder(") !=
        std::string::npos);
  CHECK(semanticsDiagnosticsSource.find(
            "bool SemanticsValidator::typeResolutionNodeDiagnosticOrderLess(") !=
        std::string::npos);
  CHECK(semanticsDiagnosticsSource.find(
            "void SemanticsValidator::sortTypeResolutionNodesForDiagnosticOrder(") !=
        std::string::npos);
  CHECK(semanticsInferGraphPath.string().find("SemanticsValidatorInferGraph.cpp") !=
        std::string::npos);
  const std::string semanticsInferGraphSource = readText(semanticsInferGraphPath);
  CHECK(semanticsInferGraphSource.find(
            "void sortDefinitionsForDeterministicDiagnostics(") ==
        std::string::npos);
  CHECK(semanticsInferGraphSource.find(
            "auto collectUnknownDefinitionNodes =") !=
        std::string::npos);
  CHECK(semanticsInferGraphSource.find(
            "sortTypeResolutionNodesForDiagnosticOrder(unresolvedNodes);") !=
        std::string::npos);
  CHECK(semanticsInferGraphSource.find(
            "auto failInferGraphDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsInferGraphSource.find(
            "auto failInferGraphCycleDiagnostic =") !=
        std::string::npos);
  CHECK(semanticsInferGraphSource.find(
            "rememberFirstCollectedDiagnosticMessage(message);") !=
        std::string::npos);
  CHECK(semanticsInferGraphSource.find(
            "return failUncontextualizedDiagnostic(std::move(message));") !=
        std::string::npos);
}

TEST_CASE("semantics validator passes source delegation stays stable") {
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

  const std::filesystem::path semanticsPassesPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPasses.cpp";
  const std::filesystem::path semanticsPassesDefinitionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPassesDefinitions.cpp";
  const std::filesystem::path semanticsBuildPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  const std::filesystem::path semanticsPassesEffectsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPassesEffects.cpp";
  const std::filesystem::path semanticsPassesOmittedInitializersPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPassesOmittedInitializers.cpp";
  const std::filesystem::path semanticsPassesStructLayoutsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPassesStructLayouts.cpp";
  const std::filesystem::path semanticsTraitsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorTraits.cpp";
  const std::filesystem::path semanticsPassesDiagnosticsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPassesDiagnostics.cpp";
  const std::filesystem::path semanticsExecutionDiagnosticsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorExecutionDiagnostics.cpp";
  const std::filesystem::path semanticsPassesExecutionsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorPassesExecutions.cpp";
  REQUIRE(std::filesystem::exists(semanticsPassesPath));
  REQUIRE(std::filesystem::exists(semanticsPassesDefinitionsPath));
  REQUIRE(std::filesystem::exists(semanticsBuildPath));
  REQUIRE(std::filesystem::exists(semanticsPassesEffectsPath));
  REQUIRE(std::filesystem::exists(semanticsPassesOmittedInitializersPath));
  REQUIRE(std::filesystem::exists(semanticsPassesStructLayoutsPath));
  REQUIRE(std::filesystem::exists(semanticsTraitsPath));
  REQUIRE(std::filesystem::exists(semanticsPassesDiagnosticsPath));
  REQUIRE(std::filesystem::exists(semanticsExecutionDiagnosticsPath));
  REQUIRE(std::filesystem::exists(semanticsPassesExecutionsPath));
  const std::string semanticsPassesSource = readText(semanticsPassesPath);
  const std::string semanticsBuildSource = readText(semanticsBuildPath);
  const std::string semanticsPassesCombinedSource = readTexts({
      semanticsPassesPath,
      semanticsPassesDefinitionsPath,
      semanticsPassesExecutionsPath,
      semanticsPassesDiagnosticsPath,
  });
  const std::string semanticsPassesEffectsSource = readText(semanticsPassesEffectsPath);
  const std::string semanticsPassesOmittedInitializersSource =
      readText(semanticsPassesOmittedInitializersPath);
  const std::string semanticsPassesDefinitionsSource = readText(semanticsPassesDefinitionsPath);
  const std::string semanticsPassesStructLayoutsSource = readText(semanticsPassesStructLayoutsPath);
  const std::string semanticsTraitsSource = readText(semanticsTraitsPath);
  const std::string semanticsPassesDiagnosticsSource = readText(semanticsPassesDiagnosticsPath);
  const std::string semanticsExecutionDiagnosticsSource = readText(semanticsExecutionDiagnosticsPath);

  CHECK(semanticsPassesCombinedSource.find("bool SemanticsValidator::validateDefinitions()") != std::string::npos);
  CHECK(semanticsPassesCombinedSource.find("bool SemanticsValidator::validateExecutions()") != std::string::npos);
  CHECK(semanticsPassesSource.find("auto failEntryDiagnostic = [&](std::string message,") !=
        std::string::npos);
  CHECK(semanticsPassesSource.find("return failEntryDiagnostic(\"missing entry definition ") !=
        std::string::npos);
  CHECK(semanticsPassesSource.find("bool SemanticsValidator::resolveExecutionEffects(") == std::string::npos);
  CHECK(semanticsPassesSource.find("bool SemanticsValidator::validateCapabilitiesSubset(") == std::string::npos);
  CHECK(semanticsPassesSource.find("std::unordered_set<std::string> SemanticsValidator::resolveEffects(") ==
        std::string::npos);
  CHECK(semanticsPassesSource.find("void SemanticsValidator::collectDefinitionIntraBodyCallDiagnostics(") ==
        std::string::npos);
  CHECK(semanticsPassesSource.find("void SemanticsValidator::collectExecutionIntraBodyCallDiagnostics(") ==
        std::string::npos);
  CHECK(semanticsPassesCombinedSource.find("collectDefinitionIntraBodyCallDiagnostics(def, intraDefinitionRecords);") !=
        std::string::npos);
  CHECK(semanticsPassesCombinedSource.find("collectExecutionIntraBodyCallDiagnostics(exec, intraExecutionRecords);") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("void expandEffectImplications(") != std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("std::unordered_set<std::string> SemanticsValidator::resolveEffects(") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("bool SemanticsValidator::validateCapabilitiesSubset(") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("auto failPassesEffectsDiagnostic = [&](std::string message)") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("bool SemanticsValidator::resolveExecutionEffects(") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("return failExprDiagnostic(expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find("return failExprDiagnostic(expr, error_);") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find(
            "return failExecutionDiagnostic(*currentExecutionContext_, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesEffectsSource.find(
            "return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "bool SemanticsValidator::publishPassesDefinitionsDiagnostic(") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "auto failPassesDefinitionsDiagnostic =") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "return failUncontextualizedDiagnostic(std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "return failExprDiagnostic(*expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "rememberFirstCollectedDiagnosticMessage(intraDefinitionRecords.front().message);") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "bool SemanticsValidator::validateDefinitionsFromStableIndexResolver(") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "const std::size_t stableIndex = resolveStableIndex(stableOrdinal);") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "return validateDefinitionsForStableRange(0, declarationCount);") !=
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find("auto buildAllStableIndices = [&]()") ==
        std::string::npos);
  CHECK(semanticsPassesDefinitionsSource.find(
            "stableIndices.reserve(boundedCount);") ==
        std::string::npos);
  CHECK(semanticsPassesOmittedInitializersSource.find(
            "auto failPassesOmittedInitializersDiagnostic =") !=
        std::string::npos);
  CHECK(semanticsPassesOmittedInitializersSource.find(
            "return failExprDiagnostic(*expr, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesOmittedInitializersSource.find(
            "return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsBuildSource.find(
            "auto failBuildDefinitionMapDiagnostic = [&](std::string message,") !=
        std::string::npos);
  CHECK(semanticsBuildSource.find(
            "return failDefinitionDiagnostic(*def, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesStructLayoutsSource.find(
            "auto failPassesStructLayoutsDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsPassesStructLayoutsSource.find(
            "return failDefinitionDiagnostic(*currentDefinitionContext_, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesStructLayoutsSource.find("return failPassesStructLayoutsDiagnostic(error_);") !=
        std::string::npos);
  CHECK(semanticsTraitsSource.find(
            "auto failTraitDiagnostic = [&](const Definition &defContext,") !=
        std::string::npos);
  CHECK(semanticsPassesExecutionsPath.string().find("SemanticsValidatorPassesExecutions.cpp") !=
        std::string::npos);
  const std::string semanticsPassesExecutionsSource = readText(semanticsPassesExecutionsPath);
  CHECK(semanticsPassesExecutionsSource.find(
            "auto failPassesExecutionsDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsPassesExecutionsSource.find("return failExecutionDiagnostic(exec, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsPassesExecutionsSource.find("return failExecutionDiagnostic(exec, error_);") !=
        std::string::npos);
  CHECK(semanticsPassesExecutionsSource.find(
            "rememberFirstCollectedDiagnosticMessage(intraExecutionRecords.front().message);") !=
        std::string::npos);
  CHECK(semanticsPassesDiagnosticsSource.find("void SemanticsValidator::collectDefinitionIntraBodyCallDiagnostics(") !=
        std::string::npos);
  CHECK(semanticsExecutionDiagnosticsSource.find("void SemanticsValidator::collectExecutionIntraBodyCallDiagnostics(") !=
        std::string::npos);
  CHECK(semanticsPassesDiagnosticsSource.find("isFlowEffectDiagnosticMessage(") != std::string::npos);
  CHECK(semanticsPassesDiagnosticsSource.find("collectResolvedCallArgumentDiagnostic") != std::string::npos);
}

TEST_CASE("semantics validator statement source delegation stays stable") {
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

  const std::filesystem::path semanticsStatementPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatement.cpp";
  const std::filesystem::path semanticsStatementBindingsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBindings.cpp";
  const std::filesystem::path semanticsStatementBuiltinsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBuiltins.cpp";
  const std::filesystem::path semanticsStatementBodyArgumentsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBodyArguments.cpp";
  const std::filesystem::path semanticsStatementControlFlowPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementControlFlow.cpp";
  const std::filesystem::path semanticsStatementVectorHelpersPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementVectorHelpers.cpp";
  const std::filesystem::path semanticsStatementVectorResolutionPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementVectorResolution.cpp";
  REQUIRE(std::filesystem::exists(semanticsStatementPath));
  REQUIRE(std::filesystem::exists(semanticsStatementBindingsPath));
  REQUIRE(std::filesystem::exists(semanticsStatementBuiltinsPath));
  REQUIRE(std::filesystem::exists(semanticsStatementBodyArgumentsPath));
  REQUIRE(std::filesystem::exists(semanticsStatementControlFlowPath));
  REQUIRE(std::filesystem::exists(semanticsStatementVectorHelpersPath));
  REQUIRE(std::filesystem::exists(semanticsStatementVectorResolutionPath));
  const std::string semanticsStatementSource = readText(semanticsStatementPath);
  const std::string semanticsStatementBindingsSource = readText(semanticsStatementBindingsPath);
  const std::string semanticsStatementBuiltinsSource = readText(semanticsStatementBuiltinsPath);
  const std::string semanticsStatementBodyArgumentsSource = readText(semanticsStatementBodyArgumentsPath);
  const std::string semanticsStatementControlFlowSource = readText(semanticsStatementControlFlowPath);
  const std::string semanticsStatementVectorHelpersSource = readText(semanticsStatementVectorHelpersPath);
  const std::string semanticsStatementVectorResolutionSource = readText(semanticsStatementVectorResolutionPath);
  CHECK(semanticsStatementSource.find("bool SemanticsValidator::validateStatement") != std::string::npos);
  CHECK(semanticsStatementSource.find("#include \"SemanticsValidatorStatementLoopCountStep.h\"") !=
        std::string::npos);
  CHECK(semanticsStatementSource.find("validateBindingStatement(") != std::string::npos);
  CHECK(semanticsStatementSource.find("validatePathSpaceComputeBuiltinStatement(") != std::string::npos);
  CHECK(semanticsStatementSource.find("validateControlFlowStatement(") != std::string::npos);
  CHECK(semanticsStatementSource.find("validateVectorStatementHelper(") != std::string::npos);
  CHECK(semanticsStatementSource.find("#include \"SemanticsValidatorStatementHelpers.h\"") == std::string::npos);
  CHECK(semanticsStatementSource.find("#include \"SemanticsValidatorStatementBindings.h\"") == std::string::npos);
  CHECK(semanticsStatementSource.find("#include \"SemanticsValidatorStatementBuiltins.h\"") == std::string::npos);
  CHECK(semanticsStatementSource.find("#include \"SemanticsValidatorStatementControlFlow.h\"") == std::string::npos);
  CHECK(semanticsStatementSource.find("auto validateLoopBody = [&](const Expr &body,") == std::string::npos);
  CHECK(semanticsStatementSource.find("auto canIterateMoreThanOnce = [&](const Expr &countExpr, bool allowBoolCount) -> bool {") ==
        std::string::npos);
  CHECK(semanticsStatementSource.find("auto isBoolLiteralFalse = [](const Expr &expr) -> bool { return expr.kind == Expr::Kind::BoolLiteral && !expr.boolValue; };") ==
        std::string::npos);
  CHECK(semanticsStatementSource.find("auto resolveVectorBinding = [&](const Expr &target, const BindingInfo *&bindingOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsStatementSource.find("auto getVectorStatementHelperName = [&](const Expr &candidate, std::string &nameOut) -> bool {") ==
        std::string::npos);
  CHECK(semanticsStatementSource.find("std::string vectorHelper;") == std::string::npos);
  CHECK(semanticsStatementSource.find("duplicate binding name: ") == std::string::npos);
  CHECK(semanticsStatementSource.find("std::function<bool(const Expr &, std::string &)> resolvePointerRoot;") ==
        std::string::npos);
  CHECK(semanticsStatementSource.find("entry argument strings require string bindings") == std::string::npos);
  CHECK(semanticsStatementSource.find("borrow conflict: ") == std::string::npos);
  CHECK(semanticsStatementSource.find("if (isSimpleCallName(stmt, \"dispatch\")) {") == std::string::npos);
  CHECK(semanticsStatementSource.find("if (isSimpleCallName(stmt, \"buffer_store\")) {") == std::string::npos);
  CHECK(semanticsStatementSource.find("PathSpaceBuiltin pathSpaceBuiltin;") == std::string::npos);
  CHECK(semanticsStatementSource.find("auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {") ==
        std::string::npos);

  const std::filesystem::path semanticsStatementHelpersHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementHelpers.h";
  CHECK_FALSE(std::filesystem::exists(semanticsStatementHelpersHeaderPath));
  const std::filesystem::path semanticsStatementBindingsHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBindings.h";
  CHECK_FALSE(std::filesystem::exists(semanticsStatementBindingsHeaderPath));
  const std::filesystem::path semanticsStatementBuiltinsHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementBuiltins.h";
  CHECK_FALSE(std::filesystem::exists(semanticsStatementBuiltinsHeaderPath));
  const std::filesystem::path semanticsStatementControlFlowHeaderPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementControlFlow.h";
  CHECK_FALSE(std::filesystem::exists(semanticsStatementControlFlowHeaderPath));

  const std::filesystem::path semanticsStatementReturnsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorStatementReturns.cpp";
  REQUIRE(std::filesystem::exists(semanticsStatementReturnsPath));
  const std::string semanticsStatementReturnsSource = readText(semanticsStatementReturnsPath);
  CHECK(semanticsStatementBindingsSource.find("bool SemanticsValidator::validateBindingStatement(") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("auto failBindingDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("duplicate binding name: ") != std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("binding initializer requires a value") != std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("entry argument strings require string bindings") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("std::function<bool(const Expr &, std::string &)> resolveStorageRootExpr;") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("std::function<bool(const Expr &, std::string &)> resolvePointerRoot;") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("isExperimentalSoaColumnSlotHelperPath(resolvedCallPath)") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("resolvedCallPath.rfind(\"/std/collections/experimental_soa_storage/soaColumnSlotUnsafe\", 0) == 0") ==
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("resolvedCallPath.rfind(\"/std/collections/experimental_vector/vectorSlotUnsafe\", 0) == 0") !=
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("isExplicitCanonicalVectorConstructor") ==
        std::string::npos);
  CHECK(semanticsStatementBindingsSource.find("borrow conflict: ") != std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("bool SemanticsValidator::validatePathSpaceComputeBuiltinStatement(") !=
        std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("if (isSimpleCallName(stmt, \"dispatch\")) {") !=
        std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("auto failStatementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("if (isSimpleCallName(stmt, \"buffer_store\")) {") !=
        std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("PathSpaceBuiltin pathSpaceBuiltin;") != std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("auto resolveBufferElemType = [&](const Expr &arg, std::string &elemType) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementBuiltinsSource.find("return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "bool SemanticsValidator::validateStatementBodyArguments(") !=
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "auto failStatementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "void appendUniqueReceiverIndex(std::vector<size_t> &receiverIndices, size_t index, size_t limit)") ==
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "appendUniqueReceiverIndex(receiverIndices, i, candidate.args.size());") ==
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "for (size_t receiverIndex : receiverIndices)") ==
        std::string::npos);
  CHECK(semanticsStatementBodyArgumentsSource.find(
            "if (isMapReceiverExpr(candidate.args[i])) {") !=
        std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("bool SemanticsValidator::validateControlFlowStatement(") !=
        std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find(
            "auto failStatementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find(
            "return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("if (isMatchCall(stmt)) {") != std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("if (isIfCall(stmt)) {") != std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("auto validateLoopBody = [&](const Expr &body,") !=
        std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("if (isLoopCall(stmt)) {") != std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("if (isWhileCall(stmt)) {") != std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("if (isForCall(stmt)) {") != std::string::npos);
  CHECK(semanticsStatementControlFlowSource.find("if (isRepeatCall(stmt)) {") != std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("bool SemanticsValidator::validateVectorStatementHelper(") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("std::string SemanticsValidator::preferVectorStdlibHelperPath(") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("auto failStatementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("auto publishExistingStatementDiagnostic = [&]() -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("std::string vectorHelper;") != std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("if (vectorHelperIsPush) {") != std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find("validateVectorRelocationHelperElementType(binding, \"push\"") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const std::string vectorHelperResolvedCanonical =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool hasNamedStatementArgs = hasNamedArguments(stmt.argNames);") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const size_t namedValuesStatementArgIndex = [&]() -> size_t {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool hasNamedValuesStatementArg = namedValuesStatementArgIndex < stmt.args.size();") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const std::string normalizedStatementNamespacePrefix =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const std::string normalizedStatementName =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool hasNamedArgs = hasNamedArguments(stmt.argNames);") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "bool hasValuesNamedReceiver = false;") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool vectorHelperNeedsStandaloneSoaBorrowCheck =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool hasHeapAllocEffect =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const std::string normalizedPrefix = std::string(trimLeadingSlash(stmt.namespacePrefix));") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const std::string normalizedName = std::string(trimLeadingSlash(stmt.name));") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool helperAllowsSoaVectorTarget = helperName == \"push\" || helperName == \"reserve\";") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "vectorHelperResolvedCanonical.rfind(\"/std/collections/soa_vector/\", 0) == 0") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "isLegacyOrCanonicalSoaHelperPath(vectorHelperResolvedCanonical, \"push\")") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "isLegacyOrCanonicalSoaHelperPath(vectorHelperResolvedCanonical, \"reserve\")") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto tryResolveVectorHelperReceiverIndex = [&](size_t receiverIndex) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "if (!tryResolveVectorHelperReceiverIndex(0) && probePositionalReorderedReceiver) {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto tryResolveCanonicalBuiltinCompatibilityReceiverIndex = [&](size_t receiverIndex) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool canonicalCompatibilityAllowsSoaVectorTarget =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool hasResolvedVectorHelperDefinition = defMap_.find(vectorHelperResolved) != defMap_.end();") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool isResolvedExperimentalVectorHelper =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto hasDeclaredOrImportedPath = [&](const std::string &path) {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto failIfStandaloneSoaGrowthBorrowed = [&](const BindingInfo &binding, const Expr &receiverExpr) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const size_t builtinReceiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto resolveBuiltinOperandIndex = [&](std::string_view namedArg) -> size_t {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const bool hasVisibleResolvedVectorHelper =") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "!hasDeclaredDefinitionPath(explicitCanonicalStdVectorMutatorCallPath) &&") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "helperCall.templateArgs.empty() &&\n"
            "        vectorHelperResolved.rfind(\"/std/collections/experimental_vector/\", 0) == 0") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "if (isSoaGrowthBinding(binding) &&\n"
            "        resolveStandaloneSoaGrowthRoot(stmt.args[receiverIndex], borrowRoot,\n"
            "                                       ignoreBorrowName) &&\n"
            "        hasActiveBorrowForRoot(borrowRoot, ignoreBorrowName)) {") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const size_t receiverIndex = shouldUseCanonicalBuiltinCompatibilityFallback\n"
            "                                     ? canonicalBuiltinCompatibilityReceiverIndex\n"
            "                                     : 0;") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "const size_t valueIndex = shouldUseCanonicalBuiltinCompatibilityFallback\n"
            "                                  ? findCanonicalBuiltinCompatibilityOperandIndex(\"value\")\n"
            "                                  : 1;") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "if (currentValidationState_.context.activeEffects.count(\"heap_alloc\") == 0) {") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "(defMap_.find(vectorHelperResolved) == defMap_.end() || isNamespacedVectorHelperCall) &&") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "    };\n"
            "    std::vector<size_t> receiverIndices;\n"
            "    auto appendReceiverIndex = [&](size_t index) {") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "if (canonicalBuiltinCompatibilityHelper && !stmt.args.empty()) {\n"
            "    std::vector<size_t> receiverIndices;\n"
            "    auto appendReceiverIndex = [&](size_t index) {") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto tryResolveSoaCanonicalMutatorReceiverIndex = [&](size_t receiverIndex) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "if (!shouldUseCanonicalBuiltinCompatibilityFallback &&\n"
            "      isStdNamespacedSoaCanonicalMutatorHelperCall &&\n"
            "      !stmt.args.empty()) {\n"
            "    std::vector<size_t> receiverIndices;\n"
            "    auto appendReceiverIndex = [&](size_t index) {") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto bareBuiltinVectorMutatorPreferredPath = [&]() -> std::string {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "auto tryResolveReceiverIndex = [&](size_t receiverIndex) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "if (tryResolveReceiverIndex(0)) {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "      return binding.typeName == \"vector\";\n"
            "    };\n"
            "    std::vector<size_t> receiverIndices;") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "vectorHelperResolved.rfind(\"/std/collections/soa_vector/push\", 0) == 0") ==
        std::string::npos);
  CHECK(semanticsStatementVectorHelpersSource.find(
            "vectorHelperResolved.rfind(\"/std/collections/soa_vector/reserve\", 0) == 0") ==
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find("bool SemanticsValidator::resolveVectorStatementBinding(") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find("bool SemanticsValidator::validateVectorStatementElementType(") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "auto failVectorElementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find("bool SemanticsValidator::validateVectorStatementHelperTarget(") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "auto failVectorTargetDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find("bool SemanticsValidator::resolveVectorStatementHelperTargetPath(") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "auto failVectorReceiverDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "matchesVectorCtorPath(\"/std/collections/vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "matchesVectorCtorPath(\"/std/collections/experimental_vector/vector\")") ==
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "return failExprDiagnostic(receiver, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "return failExprDiagnostic(arg, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementVectorResolutionSource.find(
            "return failExprDiagnostic(target, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementReturnsSource.find(
            "auto failReturnDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementSource.find("auto failStatementDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(semanticsStatementReturnsSource.find(
            "return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementSource.find("return failExprDiagnostic(stmt, std::move(message));") !=
        std::string::npos);
  CHECK(semanticsStatementReturnsSource.find("bool SemanticsValidator::statementAlwaysReturns") !=
        std::string::npos);
  CHECK(semanticsStatementReturnsSource.find("bool SemanticsValidator::blockAlwaysReturns") !=
        std::string::npos);
}

TEST_CASE("semantics validator build lifecycle publication stays stable") {
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

  const std::filesystem::path buildLifecyclePath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildLifecycle.cpp";
  REQUIRE(std::filesystem::exists(buildLifecyclePath));

  const std::string buildLifecycleSource = readText(buildLifecyclePath);
  CHECK(buildLifecycleSource.find(
            "bool SemanticsValidator::validateLifecycleHelperDefinitions()") !=
        std::string::npos);
  CHECK(buildLifecycleSource.find(
            "auto failLifecycleDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(buildLifecycleSource.find(
            "return failDefinitionDiagnostic(def, std::move(message));") !=
        std::string::npos);
  CHECK(buildLifecycleSource.find(
            "lifecycle helper must be nested inside a struct: ") !=
        std::string::npos);
  CHECK(buildLifecycleSource.find("return failLifecycleDiagnostic(") !=
        std::string::npos);
}

TEST_CASE("semantics validator build transform publication stays stable") {
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

  const std::filesystem::path buildTransformsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildTransforms.cpp";
  REQUIRE(std::filesystem::exists(buildTransformsPath));

  const std::string buildTransformsSource = readText(buildTransformsPath);
  CHECK(buildTransformsSource.find(
            "bool SemanticsValidator::validateDefinitionBuildTransforms(") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "auto addTransformDiagnostic = [&](const std::string &message) -> bool {") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "rememberFirstCollectedDiagnosticMessage(message);") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "pod definitions cannot be tagged as handle or gpu_lane: ") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "struct definitions cannot declare return types: ") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "fields cannot be tagged as handle and gpu_lane: ") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "if (addTransformDiagnostic(\"struct definitions may only contain field bindings: \" + def.fullPath)) {") !=
        std::string::npos);
  CHECK(buildTransformsSource.find(
            "if (addTransformDiagnostic(\"fields cannot be tagged as handle and gpu_lane: \" + def.fullPath)) {") !=
        std::string::npos);
}

TEST_CASE("semantics validator build return-kind publication stays stable") {
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

  const std::filesystem::path buildReturnKindsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildReturnKinds.cpp";
  REQUIRE(std::filesystem::exists(buildReturnKindsPath));

  const std::string buildReturnKindsSource = readText(buildReturnKindsPath);
  CHECK(buildReturnKindsSource.find(
            "bool SemanticsValidator::buildDefinitionReturnKinds(") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "auto failReturnKindDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "auto addReturnKindDiagnostic = [&](const std::string &message) -> bool {") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "rememberFirstCollectedDiagnosticMessage(message);") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "return failDefinitionDiagnostic(def, std::move(message));") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "soa_vector return type requires struct element type on ") !=
        std::string::npos);
  CHECK(buildReturnKindsSource.find(
            "if (!addReturnKindDiagnostic(returnKindError)) {") !=
        std::string::npos);
}

TEST_CASE("semantics validator build import publication stays stable") {
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

  const std::filesystem::path buildImportsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildImports.cpp";
  REQUIRE(std::filesystem::exists(buildImportsPath));

  const std::string buildImportsSource = readText(buildImportsPath);
  CHECK(buildImportsSource.find("bool SemanticsValidator::buildImportAliases()") !=
        std::string::npos);
  CHECK(buildImportsSource.find(
            "auto failImportDiagnostic = [&](std::string message, const Definition *relatedDef = nullptr) -> bool {") !=
        std::string::npos);
  CHECK(buildImportsSource.find(
            "auto addImportDiagnostic = [&](const std::string &message, const Definition *relatedDef = nullptr) {") !=
        std::string::npos);
  CHECK(buildImportsSource.find("return failImportDiagnostic(message, relatedDef);") !=
        std::string::npos);
  CHECK(buildImportsSource.find(
            "return failDefinitionDiagnostic(*relatedDef, std::move(message));") !=
        std::string::npos);
  CHECK(buildImportsSource.find(
            "return failUncontextualizedDiagnostic(std::move(message));") !=
        std::string::npos);
  CHECK(buildImportsSource.find("rememberFirstCollectedDiagnosticMessage(message);") !=
        std::string::npos);
  CHECK(buildImportsSource.find("if (!addImportDiagnostic(") !=
        std::string::npos);
  CHECK(buildImportsSource.find(
            "registerCanonicalSoaVectorWildcardAliases(prefix)") !=
        std::string::npos);
  CHECK(buildImportsSource.find(
            "registerExperimentalSoaVectorConversionWildcardAliases(") ==
        std::string::npos);
  CHECK(buildImportsSource.find(
            "/std/collections/experimental_soa_vector_conversions/soaVectorToAos") ==
        std::string::npos);
  CHECK(buildImportsSource.find(
            "/std/collections/experimental_soa_vector_conversions/soaVectorToAosRef") ==
        std::string::npos);
  CHECK(buildImportsSource.find(
            "const std::string scopedPrefix = prefix + \"/\";") !=
        std::string::npos);
}

TEST_CASE("semantics validator build struct-field publication stays stable") {
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

  const std::filesystem::path buildStructFieldsPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildStructFields.cpp";
  REQUIRE(std::filesystem::exists(buildStructFieldsPath));

  const std::string buildStructFieldsSource = readText(buildStructFieldsPath);
  CHECK(buildStructFieldsSource.find(
            "bool SemanticsValidator::resolveStructFieldBinding(") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find(
            "auto failStructFieldDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find(
            "auto failSoaFieldEnvelopeDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find("return failExprDiagnostic(fieldStmt, error_);") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find(
            "return failExprDiagnostic(fieldStmt, std::move(message));") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find(
            "omitted struct field envelope requires exactly one initializer: ") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find(
            "soa_vector field envelope is unsupported on ") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find("return failStructFieldDiagnostic(") !=
        std::string::npos);
  CHECK(buildStructFieldsSource.find("return failSoaFieldEnvelopeDiagnostic(") !=
        std::string::npos);
}

TEST_CASE("semantics validator build parameter publication stays stable") {
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

  const std::filesystem::path buildParametersPath =
      repoRoot / "src" / "semantics" / "SemanticsValidatorBuildParameters.cpp";
  REQUIRE(std::filesystem::exists(buildParametersPath));

  const std::string buildParametersSource = readText(buildParametersPath);
  CHECK(buildParametersSource.find("bool SemanticsValidator::buildParameters()") !=
        std::string::npos);
  CHECK(buildParametersSource.find(
            "auto failParameterDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(buildParametersSource.find(
            "auto failBuildParameterDefinitionDiagnostic = [&](std::string message) -> bool {") !=
        std::string::npos);
  CHECK(buildParametersSource.find("return failExprDiagnostic(param, error_);") !=
        std::string::npos);
  CHECK(buildParametersSource.find(
            "return failDefinitionDiagnostic(def, std::move(message));") !=
        std::string::npos);
  CHECK(buildParametersSource.find(
            "parameter default must be a literal or pure expression: ") !=
        std::string::npos);
  CHECK(buildParametersSource.find(
            "Copy/Move helpers require [Reference<Self>] parameter: ") !=
        std::string::npos);
  CHECK(buildParametersSource.find("return failParameterDiagnostic(") !=
        std::string::npos);
  CHECK(buildParametersSource.find(
            "return failBuildParameterDefinitionDiagnostic(") !=
        std::string::npos);
}

TEST_SUITE_END();
