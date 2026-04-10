TEST_CASE("graph type resolver pilot is wired through options and semantics inference") {
  const std::filesystem::path cwd = std::filesystem::current_path();
  std::filesystem::path optionsHeaderPath = cwd / "include" / "primec" / "Options.h";
  std::filesystem::path optionsParserPath = cwd / "src" / "OptionsParser.cpp";
  std::filesystem::path semanticsHeaderPath = cwd / "include" / "primec" / "Semantics.h";
  std::filesystem::path semanticsValidatePath = cwd / "src" / "semantics" / "SemanticsValidate.cpp";
  std::filesystem::path validatorHeaderPath = cwd / "src" / "semantics" / "SemanticsValidator.h";
  std::filesystem::path validatorPrivateCoreHeaderPath =
      cwd / "src" / "semantics" / "SemanticsValidatorPrivateCore.h";
  std::filesystem::path validatorPrivateExprValidationHeaderPath =
      cwd / "src" / "semantics" / "SemanticsValidatorPrivateExprValidation.h";
  std::filesystem::path validatorPrivateExprInferenceHeaderPath =
      cwd / "src" / "semantics" / "SemanticsValidatorPrivateExprInference.h";
  std::filesystem::path validatorPrivateStatementsHeaderPath =
      cwd / "src" / "semantics" / "SemanticsValidatorPrivateStatements.h";
  std::filesystem::path validatorCorePath = cwd / "src" / "semantics" / "SemanticsValidator.cpp";
  std::filesystem::path validatorResultHelpersPath =
      cwd / "src" / "semantics" / "SemanticsValidatorResultHelpers.cpp";
  std::filesystem::path validatorBuildPath = cwd / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
  std::filesystem::path validatorBuildUtilityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildUtility.cpp";
  std::filesystem::path validatorBuildStructFieldsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildStructFields.cpp";
  std::filesystem::path validatorBuildDirectCallBindingPath =
      cwd / "src" / "semantics" / "SemanticsValidatorBuildDirectCallBinding.cpp";
  std::filesystem::path validatorCollectionsPath = cwd / "src" / "semantics" / "SemanticsValidatorInferCollections.cpp";
  std::filesystem::path validatorCollectionCompatibilityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionCompatibility.cpp";
  std::filesystem::path validatorCollectionCompatibilityInternalPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionCompatibilityInternal.h";
  std::filesystem::path validatorCollectionReturnInferencePath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionReturnInference.cpp";
  std::filesystem::path validatorCollectionCallResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionCallResolution.cpp";
  std::filesystem::path validatorCollectionBufferAndMapResolversPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp";
  std::filesystem::path validatorCollectionStringResolverPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionStringResolver.cpp";
  std::filesystem::path validatorExprPath = cwd / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
  std::filesystem::path validatorExprDispatchBootstrapPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprDispatchBootstrap.cpp";
  std::filesystem::path validatorExprPreDispatchDirectCallsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprPreDispatchDirectCalls.cpp";
  std::filesystem::path validatorExprMethodCompatibilitySetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprMethodCompatibilitySetup.cpp";
  std::filesystem::path validatorExprCollectionDispatchSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprCollectionDispatchSetup.cpp";
  std::filesystem::path validatorExprCollectionAccessSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessSetup.cpp";
  std::filesystem::path validatorExprDirectCollectionFallbacksPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprDirectCollectionFallbacks.cpp";
  std::filesystem::path validatorExprPostAccessPrechecksPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprPostAccessPrechecks.cpp";
  std::filesystem::path validatorExprBuiltinContextSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprBuiltinContextSetup.cpp";
  std::filesystem::path validatorExprResolvedCallSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprResolvedCallSetup.cpp";
  std::filesystem::path validatorExprMethodResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprMethodResolution.cpp";
  std::filesystem::path validatorExprMethodTargetResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprMethodTargetResolution.cpp";
  std::filesystem::path validatorExprLateUnknownTargetFallbacksPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprLateUnknownTargetFallbacks.cpp";
  std::filesystem::path validatorExprBodyArgumentsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorExprBodyArguments.cpp";
  std::filesystem::path validatorCollectionHelperRewritesPath =
      cwd / "src" / "semantics" / "SemanticsValidatorCollectionHelperRewrites.cpp";
  std::filesystem::path validatorInferPath = cwd / "src" / "semantics" / "SemanticsValidatorInfer.cpp";
  std::filesystem::path validatorInferCollectionCountCapacityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionCountCapacity.cpp";
  std::filesystem::path validatorInferCollectionDirectCountCapacityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionDirectCountCapacity.cpp";
  std::filesystem::path validatorInferCollectionDispatchPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionDispatch.cpp";
  std::filesystem::path validatorInferControlFlowPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferControlFlow.cpp";
  std::filesystem::path validatorInferDefinitionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferDefinition.cpp";
  std::filesystem::path validatorInferCollectionDispatchSetupPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferCollectionDispatchSetup.cpp";
  std::filesystem::path validatorInferLateFallbackBuiltinsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferLateFallbackBuiltins.cpp";
  std::filesystem::path validatorInferPreDispatchCallsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferPreDispatchCalls.cpp";
  std::filesystem::path validatorInferScalarBuiltinsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferScalarBuiltins.cpp";
  std::filesystem::path validatorInferResolvedCallsPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferResolvedCalls.cpp";
  std::filesystem::path validatorInferGraphPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
  std::filesystem::path validatorInferMethodResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
  std::filesystem::path validatorInferStructReturnPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferStructReturn.cpp";
  std::filesystem::path validatorInferTargetResolutionPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferTargetResolution.cpp";
  std::filesystem::path validatorInferUtilityPath =
      cwd / "src" / "semantics" / "SemanticsValidatorInferUtility.cpp";
  std::filesystem::path pipelinePath = cwd / "src" / "CompilePipeline.cpp";
  std::filesystem::path primecMainPath = cwd / "src" / "main.cpp";
  std::filesystem::path primevmMainPath = cwd / "src" / "primevm_main.cpp";
  if (!std::filesystem::exists(optionsHeaderPath)) {
    optionsHeaderPath = cwd.parent_path() / "include" / "primec" / "Options.h";
    optionsParserPath = cwd.parent_path() / "src" / "OptionsParser.cpp";
    semanticsHeaderPath = cwd.parent_path() / "include" / "primec" / "Semantics.h";
    semanticsValidatePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidate.cpp";
    validatorHeaderPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.h";
    validatorPrivateCoreHeaderPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorPrivateCore.h";
    validatorPrivateExprValidationHeaderPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorPrivateExprValidation.h";
    validatorPrivateExprInferenceHeaderPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorPrivateExprInference.h";
    validatorPrivateStatementsHeaderPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorPrivateStatements.h";
    validatorCorePath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidator.cpp";
    validatorResultHelpersPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorResultHelpers.cpp";
    validatorBuildPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuild.cpp";
    validatorBuildUtilityPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildUtility.cpp";
    validatorBuildStructFieldsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildStructFields.cpp";
    validatorBuildDirectCallBindingPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorBuildDirectCallBinding.cpp";
    validatorCollectionsPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollections.cpp";
    validatorCollectionCompatibilityPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionCompatibility.cpp";
    validatorCollectionCompatibilityInternalPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionCompatibilityInternal.h";
    validatorCollectionReturnInferencePath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionReturnInference.cpp";
    validatorCollectionCallResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionCallResolution.cpp";
    validatorCollectionBufferAndMapResolversPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionBufferAndMapResolvers.cpp";
    validatorCollectionStringResolverPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionStringResolver.cpp";
    validatorExprPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExpr.cpp";
    validatorExprDispatchBootstrapPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprDispatchBootstrap.cpp";
    validatorExprPreDispatchDirectCallsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprPreDispatchDirectCalls.cpp";
    validatorExprMethodCompatibilitySetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprMethodCompatibilitySetup.cpp";
    validatorExprCollectionDispatchSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprCollectionDispatchSetup.cpp";
    validatorExprCollectionAccessSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprCollectionAccessSetup.cpp";
    validatorExprDirectCollectionFallbacksPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprDirectCollectionFallbacks.cpp";
    validatorExprPostAccessPrechecksPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprPostAccessPrechecks.cpp";
    validatorExprBuiltinContextSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprBuiltinContextSetup.cpp";
    validatorExprResolvedCallSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprResolvedCallSetup.cpp";
    validatorExprMethodResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprMethodResolution.cpp";
    validatorExprMethodTargetResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprMethodTargetResolution.cpp";
    validatorExprLateUnknownTargetFallbacksPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprLateUnknownTargetFallbacks.cpp";
    validatorExprBodyArgumentsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorExprBodyArguments.cpp";
    validatorCollectionHelperRewritesPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorCollectionHelperRewrites.cpp";
    validatorInferPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInfer.cpp";
    validatorInferCollectionCountCapacityPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionCountCapacity.cpp";
    validatorInferCollectionDirectCountCapacityPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionDirectCountCapacity.cpp";
    validatorInferCollectionDispatchPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionDispatch.cpp";
    validatorInferControlFlowPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferControlFlow.cpp";
    validatorInferDefinitionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferDefinition.cpp";
    validatorInferCollectionDispatchSetupPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferCollectionDispatchSetup.cpp";
    validatorInferLateFallbackBuiltinsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferLateFallbackBuiltins.cpp";
    validatorInferPreDispatchCallsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferPreDispatchCalls.cpp";
    validatorInferScalarBuiltinsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferScalarBuiltins.cpp";
    validatorInferResolvedCallsPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferResolvedCalls.cpp";
    validatorInferGraphPath = cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferGraph.cpp";
    validatorInferMethodResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferMethodResolution.cpp";
    validatorInferStructReturnPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferStructReturn.cpp";
    validatorInferTargetResolutionPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferTargetResolution.cpp";
    validatorInferUtilityPath =
        cwd.parent_path() / "src" / "semantics" / "SemanticsValidatorInferUtility.cpp";
    pipelinePath = cwd.parent_path() / "src" / "CompilePipeline.cpp";
    primecMainPath = cwd.parent_path() / "src" / "main.cpp";
    primevmMainPath = cwd.parent_path() / "src" / "primevm_main.cpp";
  }
  REQUIRE(std::filesystem::exists(optionsHeaderPath));
  REQUIRE(std::filesystem::exists(optionsParserPath));
  REQUIRE(std::filesystem::exists(semanticsHeaderPath));
  REQUIRE(std::filesystem::exists(semanticsValidatePath));
  REQUIRE(std::filesystem::exists(validatorHeaderPath));
  REQUIRE(std::filesystem::exists(validatorPrivateCoreHeaderPath));
  REQUIRE(std::filesystem::exists(validatorPrivateExprValidationHeaderPath));
  REQUIRE(std::filesystem::exists(validatorPrivateExprInferenceHeaderPath));
  REQUIRE(std::filesystem::exists(validatorPrivateStatementsHeaderPath));
  REQUIRE(std::filesystem::exists(validatorCorePath));
  REQUIRE(std::filesystem::exists(validatorResultHelpersPath));
  REQUIRE(std::filesystem::exists(validatorBuildPath));
  REQUIRE(std::filesystem::exists(validatorBuildUtilityPath));
  REQUIRE(std::filesystem::exists(validatorBuildStructFieldsPath));
  REQUIRE(std::filesystem::exists(validatorBuildDirectCallBindingPath));
  REQUIRE(std::filesystem::exists(validatorCollectionsPath));
  REQUIRE(std::filesystem::exists(validatorCollectionCompatibilityPath));
  REQUIRE(std::filesystem::exists(validatorCollectionCompatibilityInternalPath));
  REQUIRE(std::filesystem::exists(validatorCollectionReturnInferencePath));
  REQUIRE(std::filesystem::exists(validatorCollectionCallResolutionPath));
  REQUIRE(std::filesystem::exists(validatorCollectionBufferAndMapResolversPath));
  REQUIRE(std::filesystem::exists(validatorCollectionStringResolverPath));
  REQUIRE(std::filesystem::exists(validatorExprPath));
  REQUIRE(std::filesystem::exists(validatorExprDispatchBootstrapPath));
  REQUIRE(std::filesystem::exists(validatorExprPreDispatchDirectCallsPath));
  REQUIRE(std::filesystem::exists(validatorExprMethodCompatibilitySetupPath));
  REQUIRE(std::filesystem::exists(validatorExprCollectionDispatchSetupPath));
  REQUIRE(std::filesystem::exists(validatorExprCollectionAccessSetupPath));
  REQUIRE(std::filesystem::exists(validatorExprDirectCollectionFallbacksPath));
  REQUIRE(std::filesystem::exists(validatorExprPostAccessPrechecksPath));
  REQUIRE(std::filesystem::exists(validatorExprBuiltinContextSetupPath));
  REQUIRE(std::filesystem::exists(validatorExprResolvedCallSetupPath));
  REQUIRE(std::filesystem::exists(validatorExprMethodResolutionPath));
  REQUIRE(std::filesystem::exists(validatorExprMethodTargetResolutionPath));
  REQUIRE(std::filesystem::exists(validatorExprLateUnknownTargetFallbacksPath));
  REQUIRE(std::filesystem::exists(validatorExprBodyArgumentsPath));
  REQUIRE(std::filesystem::exists(validatorCollectionHelperRewritesPath));
  REQUIRE(std::filesystem::exists(validatorInferPath));
  REQUIRE(std::filesystem::exists(validatorInferCollectionCountCapacityPath));
  REQUIRE(std::filesystem::exists(validatorInferCollectionDirectCountCapacityPath));
  REQUIRE(std::filesystem::exists(validatorInferCollectionDispatchPath));
  REQUIRE(std::filesystem::exists(validatorInferControlFlowPath));
  REQUIRE(std::filesystem::exists(validatorInferDefinitionPath));
  REQUIRE(std::filesystem::exists(validatorInferCollectionDispatchSetupPath));
  REQUIRE(std::filesystem::exists(validatorInferLateFallbackBuiltinsPath));
  REQUIRE(std::filesystem::exists(validatorInferPreDispatchCallsPath));
  REQUIRE(std::filesystem::exists(validatorInferScalarBuiltinsPath));
  REQUIRE(std::filesystem::exists(validatorInferResolvedCallsPath));
  REQUIRE(std::filesystem::exists(validatorInferGraphPath));
  REQUIRE(std::filesystem::exists(validatorInferMethodResolutionPath));
  REQUIRE(std::filesystem::exists(validatorInferStructReturnPath));
  REQUIRE(std::filesystem::exists(validatorInferTargetResolutionPath));
  REQUIRE(std::filesystem::exists(validatorInferUtilityPath));
  REQUIRE(std::filesystem::exists(pipelinePath));
  REQUIRE(std::filesystem::exists(primecMainPath));
  REQUIRE(std::filesystem::exists(primevmMainPath));

  const std::string optionsHeader = readTextFile(optionsHeaderPath);
  const std::string optionsParser = readTextFile(optionsParserPath);
  const std::string semanticsHeader = readTextFile(semanticsHeaderPath);
  const std::string semanticsValidate = readTextFile(semanticsValidatePath);
  const std::string validatorHeader = readTextFiles({
      validatorHeaderPath,
      validatorPrivateCoreHeaderPath,
      validatorPrivateExprValidationHeaderPath,
      validatorPrivateExprInferenceHeaderPath,
      validatorPrivateStatementsHeaderPath,
  });
  const std::string validatorCore = readTextFiles({
      validatorCorePath,
      validatorResultHelpersPath,
  });
  const std::string validatorBuild = readTextFiles({
      validatorBuildPath,
      validatorBuildUtilityPath,
      validatorBuildStructFieldsPath,
      validatorBuildDirectCallBindingPath,
  });
  const std::string validatorCollections = readTextFiles({
      validatorCollectionsPath,
      validatorCollectionCompatibilityPath,
      validatorCollectionCompatibilityInternalPath,
      validatorCollectionReturnInferencePath,
      validatorCollectionCallResolutionPath,
      validatorCollectionBufferAndMapResolversPath,
      validatorCollectionStringResolverPath,
  });
  const std::string validatorExprMain = readTextFile(validatorExprPath);
  const std::string validatorExprBodyArguments =
      readTextFile(validatorExprBodyArgumentsPath);
  const std::string validatorExpr = readTextFiles({
      validatorExprPath,
      validatorExprDispatchBootstrapPath,
      validatorExprPreDispatchDirectCallsPath,
      validatorExprMethodCompatibilitySetupPath,
      validatorExprCollectionDispatchSetupPath,
      validatorExprCollectionAccessSetupPath,
      validatorExprDirectCollectionFallbacksPath,
      validatorExprPostAccessPrechecksPath,
      validatorExprBuiltinContextSetupPath,
      validatorExprResolvedCallSetupPath,
      validatorExprMethodResolutionPath,
      validatorExprMethodTargetResolutionPath,
      validatorExprLateUnknownTargetFallbacksPath,
      validatorCollectionHelperRewritesPath,
  });
  const std::string validatorInferMain = readTextFile(validatorInferPath);
  const std::string validatorInfer = readTextFiles({
      validatorInferPath,
      validatorInferCollectionCountCapacityPath,
      validatorInferCollectionDirectCountCapacityPath,
      validatorInferCollectionDispatchPath,
      validatorCollectionsPath,
      validatorInferControlFlowPath,
      validatorInferCollectionDispatchSetupPath,
      validatorInferLateFallbackBuiltinsPath,
      validatorInferPreDispatchCallsPath,
      validatorInferScalarBuiltinsPath,
      validatorInferResolvedCallsPath,
      validatorInferDefinitionPath,
      validatorInferGraphPath,
      validatorInferMethodResolutionPath,
      validatorInferStructReturnPath,
      validatorInferTargetResolutionPath,
      validatorInferUtilityPath,
  });
  const std::string pipeline = readTextFile(pipelinePath);
  const std::string primecMain = readTextFile(primecMainPath);
  const std::string primevmMain = readTextFile(primevmMainPath);

  CHECK(optionsHeader.find("typeResolver") == std::string::npos);
  CHECK(optionsParser.find("--type-resolver") == std::string::npos);
  CHECK(optionsHeader.find("benchmarkSemanticDisableMethodTargetMemoization") != std::string::npos);
  CHECK(optionsHeader.find("benchmarkSemanticGraphLocalAutoLegacyKeyShadow") != std::string::npos);
  CHECK(optionsParser.find("--benchmark-semantic-disable-method-target-memoization") != std::string::npos);
  CHECK(optionsParser.find("--benchmark-semantic-graph-local-auto-legacy-key-shadow") != std::string::npos);
  CHECK(semanticsHeader.find("typeResolver") == std::string::npos);
  CHECK(semanticsHeader.find("benchmarkSemanticDisableMethodTargetMemoization") != std::string::npos);
  CHECK(semanticsHeader.find("benchmarkSemanticGraphLocalAutoLegacyKeyShadow") != std::string::npos);
  CHECK(semanticsValidate.find("collectDiagnostics") != std::string::npos);
  CHECK(semanticsValidate.find("benchmarkSemanticDisableMethodTargetMemoization") != std::string::npos);
  CHECK(semanticsValidate.find("benchmarkSemanticGraphLocalAutoLegacyKeyShadow") != std::string::npos);
  CHECK(pipeline.find("options.benchmarkSemanticDisableMethodTargetMemoization") != std::string::npos);
  CHECK(pipeline.find("options.benchmarkSemanticGraphLocalAutoLegacyKeyShadow") != std::string::npos);
  CHECK(primecMain.find("--benchmark-semantic-disable-method-target-memoization") != std::string::npos);
  CHECK(primecMain.find("--benchmark-semantic-graph-local-auto-legacy-key-shadow") != std::string::npos);
  CHECK(validatorHeader.find("bool inferUnknownReturnKindsGraph();") != std::string::npos);
  CHECK(validatorHeader.find("void collectGraphLocalAutoBindings(const TypeResolutionGraph &graph);") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool lookupGraphLocalAutoBinding(const std::string &scopePath,") !=
        std::string::npos);
  CHECK(validatorHeader.find("bool lookupGraphLocalAutoBinding(const Expr &bindingExpr, BindingInfo &bindingOut) const;") !=
        std::string::npos);
